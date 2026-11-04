# TODO — aktywny transport RF-Nano

## Zaliczone sprzętowo

- [x] Pełny dwukierunkowy DATA↔ACK Payload (poprzedni test ~625 wymian/s).
- [x] Handshake SID16 i automatyczne wznowienie po pojedynczym resecie A/B oraz jednoczesnym obu (poprzedni firmware ~612–613 wymian/s).
- [x] Jeden projekt, jeden firmware, stałe bufory i brak Post-Build flashing.

## Gotowe w kodzie, do odebrania fizycznie

- [x] `UartReliableStream` z 22 B danych + 2 B app SEQ/ACK, transparentny binary UART na obu terminalach.
- [x] Aplikacyjne ACK, ponowienia, deduplikacja, atomowe wstawianie do UART TX, prawdziwe `RX_CREDIT` z `UART::freeSpace()`.
- [x] Zachowanie segmentu oczekującego na ACK przy rekey (gdy lokalny MCU nie zresetowany), rozdzielne sekwencje RF i stream.
- [x] CI host: testy opóźnienia sprzętowego ACK Payload, 4096 B w obie strony z symulowanymi stratami, napełnienia buforów i rollover numerów; AVR-GCC build PASS.
- [ ] **F7/F12, oba RF-Nano:** RealTerm A→B `ala ma kota`, B→A `12345`; bez logów i local echo. Potem równoczesne wiadomości i tekst dłuższy niż 22B.
- [ ] Przechwycić obustronne dane binarne do plików, porównać rozmiar i SHA-256, przetestować TX RX queue full, duplikaty i retry na fizycznych płytkach.
- [ ] Sprawdzić fizycznie ponowną negocjację SID podczas danych w locie, także reset jednej i obu ATmeg. **Uwaga:** niemożliwa gwarancja exactly-once przy resecie sinku bez trwałego journalu.
- [ ] Zmierzyć rzeczywisty goodput i latencję dla stop-and-wait 22B; historyczne 612–613 raw wymian/s nie jest wynikiem nowego UART.

## Kolejne warstwy, nie teraz

- [ ] Pełna regulacja tempa wejścia UART/CH340 (RTS/CTS lub inny uzgodniony mechanizm) dla niekontrolowanego flood; obecnie 127 B RX może się przepełnić.
- [ ] Transport z oknem większym niż 1 i ewentualny BURST/FLEX dla wyższej przepustowości.
- [ ] Opcjonalna trwała identyfikacja DEVICE_ID/PAIR_ID oddzielna od losowo/epokowo zmiennego SID.

## Status

**Implementacja nowego mostu kompletna, testy hostowe i build AVR zaliczone; fizyczny test tej wersji oczekuje na wynik użytkownika.**

## Diagnostyka zgłoszonych ubytków w obustronnym UART flood — 2026-11-04

- [x] Zweryfikowano w kodzie, że USART RX i TX to osobne asynchroniczne ISR i osobne kolejki, nie half-duplex.
- [x] Dodano `uart_unthrottled_ingress_test.cpp`: symulacja bez RTS/CTS dowodzi przepełnienia RX na obu węzłach przy 19B/wymianę, brak strat przy 3B/wymianę, nawet z idealnym RF; host PASS.
- [ ] Na obu RF-Nano powtórzyć transfer z `RealTerm Delays` **1 ms/char, 0 ms/line** (pierwsze pole), porównać pełne pliki w capture; ewentualnie stopniowo zmniejszać przerwę.
- [ ] Rozdzielić w diagnostyce przyczynę: `uart.counters().rx_buffer_full` kontra `rx_overrun` i `rx_framing`, transport `blocked` / `badFrames` oraz rzeczywiste wymiany RF. Nie mieszać logów z binarnym UART.
- [ ] Rozwinąć transport w wielokrotnego użytku pipeline/window >=2, aby stop-and-wait nie ograniczał nominalnego strumienia; zachować deduplikację, SID, transparentne BODY, kompatybilność z ograniczonym SRAM i testy loss/retry.
- [ ] Rozwiązać nadmiarową prędkość wejścia przez wspierany mechanizm kontroli przepływu lub zewnętrzne tempo nadawcy. **Nie przypisywać gwarancji bezstratności jedynie większemu ringowi.**
