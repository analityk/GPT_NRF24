# Tymczasowy test PP: A generator 00..FF -> B echo -> A UART

**Tylko gałąź `experiment/pp-echo-ramp-2026-11-04`.** Bazowy `main`
pozostaje na commicie `6b94a43` i nie został zmieniony. Ten wariant
zastępuje zastosowanie UART↔UART diagnostyką, ale zachowuje istniejące
`NrfAckPayloadPhy`, `PairedFrame`, `PairedLink`, `PairedSession`, EEPROM
SID oraz `UartReliableStream` z oknem 4×24 B i Go-back-N. To nie jest BURST20.

## Połączenie i obsługa

1. W Microchip Studio otwórz rozwiązanie z tej gałęzi. **F7 kompiluje,
   F12 ręcznie programuje**; brak automatycznego Post-Build.
2. Wgraj **ten sam HEX** na obie RF-Nano. A0 na A do GND (PTX), A0 na B
   do dodatniego napięcia tej płytki (PRX). A1 nie ma znaczenia.
3. RealTerm podłącz do **A**, UART **115200 8N1**, wyłącz local echo.
   Ustaw wyświetlanie **Hex[space]** i włącz binarny Capture, jeżeli chcesz
   później sprawdzić strumień bajt po bajcie.
4. Wysyłaj pojedyncze znaki poleceniem **Send ASCII**: `s` lub `w`.
   Komendy nie wracają w strumieniu binarnym. Na B port COM nie jest używany
   do echa; B odbija DATA przez **następny hardware ACK Payload**.

## Generator

- Początkowo 1 bajt na **10 000 µs (10 ms)**.
- Każde otrzymane `s` zmniejsza odstęp o dokładnie **100 µs**.
- Każde otrzymane `w` zwiększa odstęp o dokładnie **100 µs**.
- Zakres: **0..10 000 µs**, bez zawijania lub przekraczania zakresu.
- Zero oznacza generowanie tak szybko, jak opróżnia się ograniczona kolejka;
  nie oznacza zerowego czasu pojedynczej transmisji radiowej.
- Kolejne wartości **00 01 02 ... FE FF 00 01 ...** pojawiają się tylko
  po udanym umieszczeniu bajtu w lokalnej kolejce do radia.
- Kolejka A ma 127 bajtów użytecznej pojemności, ale generator dopisuje
  najwyżej do **120 bajtów**. Przy takim poziomie przerywa dopisywanie,
  zachowuje następny numer i czeka na zwolnienie miejsca. Nie nadpisuje
  danych; nie jest to bezpośrednie zapełnianie sprzętowego TX FIFO nRF.

Harmonogram opiera się na zegarze Timer1 2 MHz (0,5 µs/takt), z czasem
monotonicznym i wyrównaniem do zadanych terminów. Radio PP nadal wykonuje
ciągłe wymiany również między generowanymi bajtami. Przy bardzo małym
odstępie rzeczywisty transfer ograniczą okno 4 i przepustowość radiowa,
nie wartość zadanego generatora. Żadne polecenie nie wyłącza ani nie zmienia
sprzętowych potwierdzeń nRF.

## Diagnostyka bez zanieczyszczania UART

Na A, w debuggerze (lub w późniejszym wydzielonym kanale diagnostycznym):

- `diagnosticPort.periodUs()` — aktualny odstęp,
- `diagnosticPort.generated()` — bajty dopisane przez generator,
- `diagnosticPort.pressureEvents()` — przypadki nasycenia kolejki źródła,
- `diagnosticPort.echoed()` — bajty zwrócone i przyjęte do UART TX,
- `diagnosticPort.echoGaps()` — nieciągłości sekwencji w zwróconych bajtach,
- `stream.retransmits()`, `stream.blocked()`, `stream.badFrames()` — stan
  istniejącego transportu PP.

Echo wypisywane na UART A jest surowe, w tym `00`, `0A`, `0D` oraz `FF`.
Wygląd tekstu w oknie RealTerm **nie** jest dowodem poprawności — używaj
Capture binarnego i porównania kolejności bajtów. Wzrost liczby retransmisji
nie musi oznaczać błędu echo, ponieważ Go-back-N może ukrywać straty RF.
Nasycenie kolejki źródła oznacza ograniczenie wydajności, nawet jeśli nie
ma jeszcze uszkodzonych bajtów. Po restarcie urządzenia lub zmianie SID
nie deklarujemy exactly-once przez utratę RAM.

## Weryfikacja i powrót do normalnego PP

- Test hostowy: `sh tests/run_host_tests.sh` (w tym `pp_echo_ramp_test`).
- AVR: `sh tests/build_avr.sh` lub F7 w Microchip Studio. Kompilacja
  i test hostowy **nie zastępują** próby na dwóch fizycznych RF-Nano.
- Przywrócenie aplikacji: przełącz źródła z powrotem na gałąź **`main`**,
  zbuduj F7 i **ręcznie wgraj F12 na obie płytki**. Samo przełączenie
  gałęzi nie zmienia już zaprogramowanych mikrokontrolerów.

Nie scalać tej gałęzi z `main` jako produkcyjnego UART bridge. Po pomiarze
można ją zachować jako powtarzalne narzędzie diagnostyczne albo usunąć.
