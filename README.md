> **Protokoły:** aktualny i zachowany sprzętowo **PP (Ping-Pong)**; drugi profil **BURST20** jest na etapie projektu harmonogramu i testów hostowych, **jeszcze nie jest wykonywany na radiu**. [Specyfikacja BURST20](burst-20ms.md). PP checkpoint: [checkpoint/pp-2026-11-04](https://github.com/analityk/GPT_NRF24/tree/checkpoint/pp-2026-11-04).

# GPT_NRF24 — UART ↔ nRF24L01+ ACK Payload ↔ UART

**Aktualny etap (2026-11-04):** jeden firmware na dwie RF-Nano (ATmega328P 16 MHz + nRF24L01+ + CH340), z **ciągłym ping-pongiem DATA/ACK Payload także na pustym łączu** i **oknem 4 segmentów UART w każdym kierunku**. Fizycznie potwierdzony transfer jednokierunkowy; poprzedni stop-and-wait gubił bajty przy równoczesnym intensywnym ruchu. Nowa wersja WINDOW=4: wszystkie hostowe testy, symulacja strat i cross-build AVR **PASS**, lecz **nowe firmware wymaga osobnego testu fizycznego obustronnego obciążenia**.

## Uruchomienie w Microchip Studio

1. Sklonuj / zaktualizuj `main` i otwórz `GPT_NRF2401.atsln`.
2. **F7** kompiluje, **F12** ręcznie programuje. **Brak automatycznego flashowania Post-Build**.
3. Wgraj ten sam HEX na obie RF-Nano. Na A ADC0/A0 do **GND** (PTX), na B ADC0/A0 do **dodatniego zasilania tej płytki** (PRX).
4. Otwórz oba RealTerm na odpowiednich COM, **115200 8N1**, wyłącz **local echo** i nadmiarowe dopisywanie CR/LF przez aplikację terminalową (chyba że właśnie chcesz te bajty wysłać).
5. Wyślij z A `ala ma kota`, sprawdź pojawienie się wyłącznie na B. Wyślij z B `12345`, sprawdź pojawienie się wyłącznie na A. Następnie wyślij dwie różne wiadomości równocześnie, również dłuższe niż 22 bajty.

**UART przesyła wyłącznie dane użytkownika** — także `00`, `FF`, CR/LF. Nie zobaczysz w nowym wsadzie tekstu `PAIR CORE`, `ST=4` ani liczników; diagnostyka nie może zanieczyszczać binarnego strumienia. `SID` nadal automatycznie negocjowany jest wewnątrz firmware, lecz nie drukowany na COM.

## Aktualny stos i format

- **PHY:** nRF24L01+, 1 Mb/s, CRC16, SPI 8 MHz; A stale PTX, B stale PRX, a B odsyła zwrotne dane w uprzednio przygotowanym **sprzętowym ACK Payload**.
- **Nagłówek rdzenia:** bez zmian: 8 bajtów (`TX_SEQ`, `RX_LAST`, `LOSS_AB/BA`, `ARC_LAST/RX_DROP`, `RX_CREDIT`, `TYPE_FLAGS`, `SESSION_ID16`).
- **BODY:** do 24 bajtów pozostaje nieinterpretowane w PHY/MAC. **Całe BODY to 0–24 bajty oryginalnego UART.** `DATA_SEQ` i kumulacyjny `APP_ACK` zajmują już istniejące bajty `TX_SEQ` i `RX_LAST` nagłówka MAC; nie istnieje wtórny nagłówek transportowy w BODY.
- **Niezawodność:** niezależne okna **4 segmentów × 24 B** na kierunek, kumulacyjne `ACK_SEQ`, retransmisja Go-back-N po 8 wymianach bez postępu potwierdzeń. Odbiornik przyjmuje wyłącznie kolejne segmenty przez atomowy `UART::writeAll`; brak ACK dla danych odrzuconych, duplikaty nie trafiają drugi raz do UART w tej samej sesji.
- **Kontrola przepływu:** `RX_CREDIT` pochodzi teraz z rzeczywistego wolnego miejsca w kolejce UART TX (jednostka: 24 bajty), a nie ze stałej `1`. Przy zapełnieniu odbiorcy nie przyjmuje się nowego segmentu, nie wysyła dla niego aplikacyjnego ACK i następuje ponowienie.
- **Sesja:** HELLO/BEACON/WELCOME/READY/READY_ACK, identyfikator `SID16` z epok EEPROM i liczników prób, auto-odnowienie po resecie. W razie zmiany sesji niepotwierdzony segment utrzymywany jest w RAM strony, która nie została zresetowana.
- **Bezczynność:** **brak odpytywania co 10 ms** — A wysyła następną ramkę bez względu na obecność danych, a B odpowiada sprzętowym ACK Payload; przy pustych UART oba kierunki niosą krótkie 8B ramki IDLE z bieżącym ACK w nagłówku. Minimalny odstęp obsługuje PHY, nie planista aplikacji.

## Zachowanie i ograniczenia

**Brak RTS/CTS.** USART RX ma pojemność 127 bajtów. Jeśli RealTerm bez ograniczeń poda plik szybciej, niż radio i odbiornik mogą odbierać, lub w czasie awarii łącza, ta kolejka może się przepełnić. Nie jest to sytuacja, którą radiowy ACK naprawi wstecznie. Pierwszy test: krótkie komunikaty; przy pliku w RealTerm włącz niewielkie opóźnienia i porównaj pełne pliki capture bajt po bajcie.

Okno czterech segmentów usuwa poprzednie ograniczenie do jednego DATA na kilka wymian. Przy 613 wymianach/s sufit arytmetyczny 24×613 ≈ **14,71 kB/s na kierunek** (24 B USER/frame) przekracza limit UART **11,52 kB/s/kierunek** przy 115200 8N1. To tylko *górna granica przy zachowaniu tempa RF* — nowe wykorzystanie CPU i zapełnione obie strony mogą ją obniżyć. **Wynik rzeczywistego bezstratnego UART end-to-end musi być zmierzony na obu płytkach.** Przy długim oferowanym obciążeniu ponad rzeczywistą przepustowość brak RTS/CTS nadal prowadzi do overflow RX.

**Reset i dokładnie-jeden-raz:** bieżący MCU zachowuje niepotwierdzoną porcję w RAM podczas renegocjacji sesji po resecie partnera, lecz reset własnego MCU usuwa dane niezapisane trwale. Jeśli partner zdążył wypisać bajty na UART, a jego potwierdzenie zaginęło przed resetem, możliwe jest ich ponowienie po nowej sesji. Pełnej gwarancji exactly-once między resetami bez trwałego dziennika transakcji nie deklarujemy.

## Pliki

| Warstwa | Źródła | Zastosowanie |
|---|---|---|
| Aplikacja AVR | `GPT_NRF2401/main.cpp` | jeden program A/B, przezroczysty most UART |
| USART / ring | `uart.*`, `byte_ring.h` | nieblokujący UART 115200, licznik overflow |
| Usługa transportowa | `uart_reliable_stream.h` | DATA_SEQ/ACK_SEQ, retry, deduplikacja, UART credit |
| Rama RF | `paired_frame.h`, `paired_link.h` | 8+24 B, stan jakości łącza |
| Sesja / EEPROM | `paired_session.h`, `epoch_journal.h`, `boot_epoch.*` | negocjacja SID, ochrona przed starym ACK |
| Radio i sprzęt | `nrf24.h`, `nrf24_ack_phy.h`, `spi.*`, `gpio.*`, `adc0.*`, `timer1_clock.*` | Enhanced ShockBurst i platforma |

## Weryfikacja i historia wyników

`sh tests/run_host_tests.sh` odkrywa aktualne testy hostowe. `sh tests/build_avr.sh` kompiluje jeden program dla AVR bez programowania płytki. [GitHub Actions](https://github.com/analityk/GPT_NRF24/actions) uruchamia oba zadania. Testy UART obejmują **okno 4 segmentów**, opóźnione ACK Payload, brak miejsca w UART TX, przerwy RF, renumerację sesji z trzema zaległymi segmentami, zawinięcie numeracji i **4096 bajtów w obu kierunkach** z symulowanymi stratami; dane odtwarzają się w symulacji. Test `uart_unthrottled_ingress_test.cpp` przy modelowych 600 wymianach i **19 B wejścia na wymianę na każdej płytce (11 400 B/stronę)** nie wykazuje overflow RX, a przy nieosiągalnych docelowo 24 B/wymianę odtwarza przeciążenie. To wynik modelu z idealnym RF, a nie pomiar hardware.

**Historyczne pomiary sprzętowe (inny firmware):** rdzeń 8+24 B bez handshake'u około 625 wymian/s; z handshake'em około 612–613 wymian/s, automatyczny powrót po resetach A, B i obu płyt. Nie są to pomiary nowej usługi UART. Wcześniejsze implementacje zachowane w gałęzi [archive/pre-paired-core-cleanup-2026-11-03](https://github.com/analityk/GPT_NRF24/tree/archive/pre-paired-core-cleanup-2026-11-03).

Szczegóły formatu i semantyki: [paired-core-v2.md](paired-core-v2.md). Historia fizycznych prób: [history.md](history.md). Bieżące zadania: [todo.md](todo.md).

## Uwaga po pierwszym sprzętowym teście dwukierunkowego UART (2026-11-04)

Wysłanie pliku **w jednym kierunku działa**. Równoczesny intensywny transfer w obu kierunkach ujawnił **zgłaszane ubytki bajtów**; NIE zaliczono bezstratnego dwukierunkowego flood. USART ATmega jest pełnodupleksowy i asynchroniczny — problemem może być przeciążenie wejściowych kolejek 127 B, gdy transport stop-and-wait przesyła najwyżej 22 B/segment i czeka na aplikacyjne ACK. Przy referencyjnych ~613 wymian/s przybliżony limit stop-and-wait to ~6,7 kB/s/kierunek, niższy niż pełne 115200 8N1 (~11,52 kB/s); aktualnej rzeczywistej prędkości nie zmierzono.

**Próba rozstrzygająca:** w RealTerm ustaw dla OBU nadawców w zakładce Send / Dump File / `Delays` **pierwsze pole = 1** (1 ms na znak), **drugie = 0** (brak dodatkowej przerwy na koniec linii). Wyślij oba pliki i porównaj binarny capture z oryginałami. Gdy to działa, zmniejszaj pierwszy delay, aby wyznaczyć granicę obecnego transportu. `Delays=0,10` z pokazanego eksperymentu to 0ms na znak i 10ms po linii: może nadal pozwalać długim liniom zapełnić 127B RX ring.

Nowy `tests/uart_unthrottled_ingress_test.cpp` jest powtarzalną regresją tego przypadku: symuluje UART bez RTS/CTS przy 3 i 19 B/iterację RF, z rzeczywistym `UartReliableStream` i opóźnionym ACK. Przy 19 B/iterację przepełnia oba wejścia nawet bez błędów radia, przy 3 B nie. Dotychczasowy stress test 4096B na kierunek **sztucznie zwalniał wejście**, więc nie udowadniał odporności na flood. Nie zmieniono jeszcze transportu produkcyjnego; do poprawy architektonicznej potrzebne większe okno lub flow-control, najlepiej oba.

### Zmiana architektury po teście równoczesnym (2026-11-04)

Użytkownik słusznie zwrócił uwagę, że sprzętowe ACK Payload to dwukierunkowa okazja do przeniesienia danych w **każdej** transakcji. Dawne stop-and-wait i sztuczne 10ms polling zostały **usunięte** z aktywnego kodu. Nie trzeba wysyłać nowej ramki radiowej tylko po to, by otrzymać aplikacyjne potwierdzenie — `ACK_SEQ` jest piggybackowane w każdej ramce. Przy `RealTerm Delays=1,0` host nadaje orientacyjnie do 1000 B/s na kanał (jeśli 1ms jest rzeczywistą przerwą po znaku), a nie testuje limitu 11,52 kB/s. Kolejny test powinien użyć `Delays=0,0` w obu kierunkach i **porównać oryginały z przechwyconymi plikami bajt po bajcie**.

**Uwaga:** ACK w radiu jest generowany dla poprawnej ramki (CRC i adres), lecz jego payload pochodzi z **wcześniej przygotowanego** FIFO; aplikacyjny ACK może być o jedną lub więcej wymian spóźniony. Okno 4 usuwa konieczność czekania. Prawdziwy RF nadal może retransmitować ramki lub zgłosić MAX_RT, zatem bezwzględnie stała latencja nie jest fizycznie gwarantowana.

### Korekta kontraktu nagłówka (2026-11-04)

Obowiązuje **8 B MAC + do 24 B nieinterpretowanych bajtów UART**, bez dwóch bajtów APP_SEQ/APP_ACK wewnątrz BODY. W stanie ACTIVE `TX_SEQ` numeruje kolejne segmenty 24B (retransmisja nie zwiększa go), `RX_LAST` potwierdza ostatnią **kolejną porcję przyjętą atomowo do UART TX**. Krótki `IDLE` ma dokładnie **8 B szerokości sprzętowej DPL**, nie przesuwa numeracji i przenosi bieżący ACK/kredyty. Jeśli DATA zostaje powtórzone, jego zaktualizowany ACK nadal jest analizowany. Fizyczne nRF ACK Payload pozostaje automatyczne, niezależnie od tego, czy transport ma nową porcję danych.

Przyjęcie do UART TX nie gwarantuje przyjęcia do aplikacji komputerowej. Powyższy wynik `24×613≈14,71 kB/s` jest jedynie limitem arytmetycznym przy wcześniejszej szybkości **innego wsadu**. Wysyłanie z RealTerm bez RTS/CTS szybciej niż faktycznie obsłuży CPU/RF nadal może zgubić wejściowe bajty. Nowego firmware nie zmierzono jeszcze na fizycznych RF-Nano.

**BURST20 — obecny zakres implementacji:** `burst20_schedule.h` (5/10/15ms, deadline, guard), `profile_select.h` (A1 logic boot latch), `burst20_report.h` (format 24B, CRC32 i bitmapa) oraz testy hostowe. Rzeczywisty profil BURST PHY i selektor w `main.cpp` **jeszcze nie istnieją**. Firmware uruchamia wyłącznie PP i pozostaje kompatybilny z obecnym sprzętowym testem.
