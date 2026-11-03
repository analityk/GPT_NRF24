# Plan techniczny M1 — GPT_NRF24

## Założenia
Dwa identyczne firmware na ATmega328P i nRF24L01+, terminale USB/UART po obu stronach. Fizycznie radio jest półdupleksowe; logiczny full duplex wymaga arbitrażu czasu RX/TX. Bez dynamicznej alokacji, bez zmiany Microchip Studio. Każdy etap wymaga osobnego testu i małego commita.

## Nowa decyzja M1: stały rytm ramek i pomiar jakości łącza

- **Każda ramka danych radiowych ma dokładnie 32 bajty** (stała długość payload nRF24L01+; preambuła, adres i CRC warstwy radiowej nie należą do tych 32 bajtów). Nie mylić jej z buforami UART, które mogą mieć po 128 bajtów.
- Każda strona regularnie nadaje ramkę nawet bez danych z terminala. Wymiana po zestawieniu łącza jest naprzemienna: A→B, następnie B→A, i ponownie A→B. Przydział pierwszego kierunku ma wynikać z automatycznej negocjacji, nie z różnic w firmware.
- Każda ramka przenosi obowiązkowe informacje sterujące i pomiarowe; pozostałe miejsce jest przeznaczone na bajty UART. Jeśli nie ma danych, wysyłana jest ramka pomiarowa/utrzymania łącza. Jeśli dane czekają, trafiają do tego samego slotu bez dodatkowej ramki tylko dla pomiaru.
- Ramka musi zawierać co najmniej wersję/typ, identyfikator sesji, numer kolejnej ramki pomiarowej, informację zwrotną o odbiorze ramek partnera, długość danych i opcjonalne bajty UART. Rozdzielić numerację ramek pomiarowych od identyfikatorów danych retransmitowanych, aby ponowienia nie fałszowały pomiarów.
- **Budżet ramki:** 32 B łącznie, wstępnie 8–12 B na sterowanie/telemetrię i 20–24 B na dane użytkownika; ostateczny format zatwierdzić po zaprojektowaniu niezawodnego ACK/deduplikacji. Nie obiecywać 32 B danych użytkownika.
- Liczyć jakość **oddzielnie dla A→B i B→A**: liczbę zaplanowanych/wysłanych ramek, odebranych ramek, luki w numeracji, duplikaty, ponowienia sprzętowe i zdarzenia MAX_RT; mierzyć okno kroczące oraz liczniki całkowite. Utrata ostatniej ramki nie jest od razu wykrywalna z samej luki sekwencji — uwzględnić timeout i zwrotną telemetrię.
- Odróżnić: brak poprawnego odbioru nowej ramki, nieudaną próbę TX po sprzętowych retransmisjach, zgubione potwierdzenie oraz utratę danych aplikacyjnych. Auto-ACK i automatyczne retransmisje nRF24 mogą maskować błędy pierwszej próby. Rejestry `OBSERVE_TX` / `ARC_CNT` / `PLOS_CNT` są pomocnicze, nie zastępują liczników protokołu; `PLOS_CNT` ma ograniczony zakres.
- Nie uzależniać przejścia do kolejnego slotu od otrzymania ramki: po stracie odpowiedzi wymagane są timeout, resynchronizacja i odzyskanie harmonogramu. Zaimplementować bezpieczny arbitraż startu dwóch identycznych urządzeń, gdy nie mają jeszcze wspólnego czasu i roli slotu.
- Okres slotu, czas RX/TX, timeout, retry i ewentualne adaptacyjne ograniczenie ruchu danych będą parametrami protokołu, dobranymi pomiarem. Najpierw stabilny rytm i wiarygodna telemetria, następnie optymalizacja przepustowości.
- Dane i telemetria nie mogą się wzajemnie blokować. W razie przeciążenia ograniczać ruch użytkowy, zachowując cykliczne ramki pomiarowe i jawnie raportując przepełnienie buforów.

### Przykład wykorzystania czasu radiowego (logika, nie skala)

```text
slot 0     A -> B : ramka 32 B [telemetria | dane A lub heartbeat]
slot 1     B -> A : ramka 32 B [telemetria | dane B lub heartbeat]
slot 2     A -> B : ramka 32 B [telemetria | dane A lub heartbeat]
slot 3     B -> A : ramka 32 B [telemetria | dane B lub heartbeat]
...        brak danych UART nie zatrzymuje wymiany
```

Po utracie ramki nie wolno zakładać, że obie strony nadal znają identyczną fazę harmonogramu. Recovery jest częścią M1, nie dodatkiem.

Specyfikacja projektowa formatu, parsera i retransmisji: [`protocol-v1.md`](protocol-v1.md). Nie jest jeszcze zaimplementowana.

## 1. GPIO i projekt
Dodać gpio.h/gpio.cpp do projektu. API: set, clear, flip, release, i2c_set, i2c_clr, read, readOpenDrain, readPullUp. Obsługiwać PORT/DDR/PIN bez niejawnej arytmetyki adresów; konstruktor jawnych rejestrów pozwala testować klasę. Dla open-drain najpierw wyzerować PORT przed ustawieniem DDR na wyjście, aby nigdy aktywnie nie wystawić HIGH. Testy rejestrów, nieprawidłowych pinów, domyślnego konstruktora. Bramka: kompilacja Debug/Release na AVR i test sprzętowy.

## 2. UART

Sterownik i program echo zostały dodane jako etap uruchomienia. Wymagana jeszcze kompilacja w Microchip Studio i sprzętowy test na obu płytkach przed uznaniem tego kroku za zweryfikowany.
Skonfigurować USART 8N1, początkowo 115200 przy potwierdzonym F_CPU. Obliczyć błąd generatora baud. ISR RX i UDRE oraz dwa statyczne bufory pierścieniowe; na początek 128 B każdy, po audycie 2 KB SRAM. Zasada pojedynczego producenta i konsumenta; atomowe indeksy 8-bitowe, jawna obsługa pełnego bufora i błędów USART. Nie blokować w ISR. Bramka: niezależne odbieranie i nadawanie na terminalu, także seriami i po ponownym otwarciu portu.

## 3. SPI i nRF24L01+

Warstwa SPI, rejestrowy sterownik nRF i UART diagnostyczny zostały dodane. **Etap komunikacji SPI i rejestrów nRF zaliczony na obu fizycznych RF-Nano**: zrzuty RealTerm COM10 i COM11 pokazują `NRF24 OK S=0E CFG=0C CH=4C RF=06 FIFO=11` (2026-11-03). Potwierdzenie nie obejmuje transmisji fal radiowych. Sterowanie TX/RX z czasami przejść należy do późniejszej maszyny stanów MAC.
Uruchomić sprzętowe SPI mode 0, bezpieczny zegar, GPIO dla CE/CSN. Udostępnić read/write register, read/write buffer i command. Potwierdzić odczyt/zapis rejestrów. Ustalić wspólny kanał, adres, CRC, 1 Mb/s, Enhanced ShockBurst i stały payload 32 B. Obsłużyć CONFIG, STATUS, FIFO_STATUS, RX_DR, TX_DS, MAX_RT, flush FIFO, czasy CE i przejścia RX/TX. Jeśli IRQ nie jest podłączone, odpytywać status bez nieograniczonego czekania. Bramka: obie płytki raportują sprawne SPI i radio.

## 4. Test jednostronny

Dodano diagnostyczny RAW32 ping-pong bez protokołu v1: identyczny firmware, inicjator według zatrzaśniętego ADC0 (GND vs 3,3 V), odbiorca odsyła sprawdzony surowy payload. Oddzielna warstwa PHY odpowiada za przełączanie CE/PRX/PTX, ACK i timeouty. Test modelowy (g++ C++11) wykonany; **weryfikacja fizyczna zaliczona 2026-11-03**: zrzut COM10/COM11, zgodne SEQ 000B–0019, ACK/REPLY/RX=OK i ARC=00. Awaryjne MAX_RT oraz odzyskiwanie po zaniku węzła pozostają do testów.
Na czas testu jedną płytkę ustawić w RX, drugą w TX (nie utrwalać ról). Przesłać znaną ramkę z licznikiem, zweryfikować payload na UART odbiorcy i TX_DS lub MAX_RT nadawcy. Powtórzyć w przeciwnym kierunku oraz bez odbiorcy. Bramka: rzeczywisty transfer A→B i B→A.

## 5. Protokół danych
Stała 32-bajtowa ramka łącząca telemetryczne heartbeat, potwierdzenia i dane UART; nagłówek ogranicza liczbę bajtów użytkownika. Osobne numery ramek pomiarowych i danych wymagających retransmisji, numer sesji, informacja zwrotna o odbiorze, długość i typ. Nie stosować dynamicznych payloadów na M1. Rozdzielić sprzętowy ACK od ACK aplikacyjnego: ten drugi oznacza przyjęcie danych do kolejki UART, nie ich fizyczne wyświetlenie. Stop-and-wait niezależnie dla każdego kierunku, retry bez zmiany numeru sekwencyjnego, deduplikacja, obsługa zawinięcia numerów i restartu sesji. Zaprojektować źródło rozróżnialnych identyfikatorów sesji, nie zakładać unikalnego ID w ATmega328P. Bramka: testy ramek, duplikatów, zgubionych ACK, restartów i pełnych buforów na hoście.

## 6. Odkrywanie partnera
Po starcie oba urządzenia w RX i negocjują fazę naprzemiennych slotów; HELLO nie może zakładać, że partner jest stale w RX. Po TX szybki powrót do RX. Otrzymanie poprawnej ramki aktualizuje ostatni kontakt. Timeout przełącza w LINK_DOWN i ponawia discovery. Bez ról wyprowadzanych z COM, nazw plików i kolejności uruchomienia. Bramka: samodzielne połączenie i odtworzenie po restarcie jednej strony.

## 7. Arbitraż logicznego full duplex
Zaprojektować nieblokującą maszynę stanów: DISCOVERY → SYNC → TX_SLOT/RX_SLOT → RECOVERY, z limitami czasów; lokalny sterownik radia używa TX_PREPARE → TX_WAIT → RX. Przy utracie slotu lub gdy obie strony nadają jednocześnie, obie mogą utracić ACK: obowiązkowe rozproszenie retry/backoff, przewaga czasu RX i ograniczenie serii TX. Identyczne timery na identycznym firmware mogą się synchronizować — sprawdzić źródło asymetrii/jitteru. Priorytety dla ACK/HELLO i sprawiedliwość kierunków. Symulować kolizje, opóźnienia, utratę ramek i jednoczesne restarty. Bramka: obie strony inicjują transfery niezależnie bez trwałego zakleszczenia.

## 8. Most UART ↔ radio
Zbierać bajty UART do pełnej ramki lub timeoutu krótkiego wpisu. Przekazywać odebrane dane do kolejki UART TX; nie blokować pętli głównej. Payload jest przezroczysty dla CR/LF, komunikaty diagnostyczne nie mogą mieszać się z danymi. Raportować przepełnienie, MAX_RT, brak partnera. Uwzględnić reset przy otwieraniu COM przez DTR.

## 9. Odbiór M1
Kompilacja Debug/Release, kontrola Flash/SRAM i rezerwy stosu; jeden HEX wgrany na dwie płytki. Testy A→B, B→A, prawie jednoczesne wysyłanie, wiadomości dłuższe niż ramka, przepełnienia, odłączenie i restart partnera, obciążenie obu kierunków. Zliczać utracone, zdublowane i przestawione bajty oraz czas odzyskania łącza. Osobno sprawdzić pomiar utraty ramek w obu kierunkach przy pustym UART, przy ruchu w jedną stronę i przy równoczesnym ruchu; porównać liczniki po obu stronach i sprawdzić zachowanie po utracie slotów. Zarchiwizować logi, commit, ustawienia i ograniczenia. Sukces flashowania nie dowodzi działania RF.

## Dokumentacja
Po każdym sprawdzonym etapie aktualizować todo.md, history.md i migracja.md. Szczegółowe parametry implementacyjne zapisywać przy kodzie, nie w TODO. Źródła: Nordic nRF24L01+ Product Specification; Microchip ATmega328P Datasheet.
