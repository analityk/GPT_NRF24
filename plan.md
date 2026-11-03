# Plan techniczny M1 — GPT_NRF24

## Założenia
Dwa identyczne firmware na ATmega328P i nRF24L01+, terminale USB/UART po obu stronach. Fizycznie radio jest półdupleksowe; logiczny full duplex wymaga arbitrażu czasu RX/TX. Bez dynamicznej alokacji, bez zmiany Microchip Studio. Każdy etap wymaga osobnego testu i małego commita.

## 1. GPIO i projekt
Dodać gpio.h/gpio.cpp do projektu. API: set, clear, flip, release, i2c_set, i2c_clr, read, readOpenDrain, readPullUp. Obsługiwać PORT/DDR/PIN bez niejawnej arytmetyki adresów; konstruktor jawnych rejestrów pozwala testować klasę. Dla open-drain najpierw wyzerować PORT przed ustawieniem DDR na wyjście, aby nigdy aktywnie nie wystawić HIGH. Testy rejestrów, nieprawidłowych pinów, domyślnego konstruktora. Bramka: kompilacja Debug/Release na AVR i test sprzętowy.

## 2. UART
Skonfigurować USART 8N1, początkowo 115200 przy potwierdzonym F_CPU. Obliczyć błąd generatora baud. ISR RX i UDRE oraz dwa statyczne bufory pierścieniowe; na początek 128 B każdy, po audycie 2 KB SRAM. Zasada pojedynczego producenta i konsumenta; atomowe indeksy 8-bitowe, jawna obsługa pełnego bufora i błędów USART. Nie blokować w ISR. Bramka: niezależne odbieranie i nadawanie na terminalu, także seriami i po ponownym otwarciu portu.

## 3. SPI i nRF24L01+
Uruchomić sprzętowe SPI mode 0, bezpieczny zegar, GPIO dla CE/CSN. Udostępnić read/write register, read/write buffer i command. Potwierdzić odczyt/zapis rejestrów. Ustalić wspólny kanał, adres, CRC, 1 Mb/s, Enhanced ShockBurst i statyczny payload do 32 B. Obsłużyć CONFIG, STATUS, FIFO_STATUS, RX_DR, TX_DS, MAX_RT, flush FIFO, czasy CE i przejścia RX/TX. Jeśli IRQ nie jest podłączone, odpytywać status bez nieograniczonego czekania. Bramka: obie płytki raportują sprawne SPI i radio.

## 4. Test jednostronny
Na czas testu jedną płytkę ustawić w RX, drugą w TX (nie utrwalać ról). Przesłać znaną ramkę z licznikiem, zweryfikować payload na UART odbiorcy i TX_DS lub MAX_RT nadawcy. Powtórzyć w przeciwnym kierunku oraz bez odbiorcy. Bramka: rzeczywisty transfer A→B i B→A.

## 5. Protokół danych
Stała ramka z wersją, typem HELLO/DATA/ACK, identyfikatorem sesji, sekwencją, długością i danymi. Nie stosować dynamicznych payloadów na M1. Rozdzielić sprzętowy ACK od ACK aplikacyjnego: ten drugi oznacza przyjęcie danych do kolejki UART, nie ich fizyczne wyświetlenie. Stop-and-wait niezależnie dla każdego kierunku, retry bez zmiany numeru sekwencyjnego, deduplikacja, obsługa zawinięcia numerów i restartu sesji. Zaprojektować źródło rozróżnialnych identyfikatorów sesji, nie zakładać unikalnego ID w ATmega328P. Bramka: testy ramek, duplikatów, zgubionych ACK, restartów i pełnych buforów na hoście.

## 6. Odkrywanie partnera
Po starcie oba urządzenia w RX; okresowo krótkie HELLO, po TX szybki powrót do RX. Otrzymanie poprawnej ramki aktualizuje ostatni kontakt. Timeout przełącza w LINK_DOWN i ponawia discovery. Bez ról wyprowadzanych z COM, nazw plików i kolejności uruchomienia. Bramka: samodzielne połączenie i odtworzenie po restarcie jednej strony.

## 7. Arbitraż logicznego full duplex
Zaprojektować nieblokującą maszynę stanów: RX_IDLE → TX_PREPARE → TX_WAIT → RX_IDLE, z limitami czasów. Gdy obie strony nadają jednocześnie, obie mogą utracić ACK: obowiązkowe rozproszenie retry/backoff, przewaga czasu RX i ograniczenie serii TX. Identyczne timery na identycznym firmware mogą się synchronizować — sprawdzić źródło asymetrii/jitteru. Priorytety dla ACK/HELLO i sprawiedliwość kierunków. Symulować kolizje, opóźnienia, utratę ramek i jednoczesne restarty. Bramka: obie strony inicjują transfery niezależnie bez trwałego zakleszczenia.

## 8. Most UART ↔ radio
Zbierać bajty UART do pełnej ramki lub timeoutu krótkiego wpisu. Przekazywać odebrane dane do kolejki UART TX; nie blokować pętli głównej. Payload jest przezroczysty dla CR/LF, komunikaty diagnostyczne nie mogą mieszać się z danymi. Raportować przepełnienie, MAX_RT, brak partnera. Uwzględnić reset przy otwieraniu COM przez DTR.

## 9. Odbiór M1
Kompilacja Debug/Release, kontrola Flash/SRAM i rezerwy stosu; jeden HEX wgrany na dwie płytki. Testy A→B, B→A, prawie jednoczesne wysyłanie, wiadomości dłuższe niż ramka, przepełnienia, odłączenie i restart partnera, obciążenie obu kierunków. Zliczać utracone, zdublowane i przestawione bajty oraz czas odzyskania łącza. Zarchiwizować logi, commit, ustawienia i ograniczenia. Sukces flashowania nie dowodzi działania RF.

## Dokumentacja
Po każdym sprawdzonym etapie aktualizować todo.md, history.md i migracja.md. Szczegółowe parametry implementacyjne zapisywać przy kodzie, nie w TODO. Źródła: Nordic nRF24L01+ Product Specification; Microchip ATmega328P Datasheet.
