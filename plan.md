# Plan prac — rdzeń 8+24 B z ACK Payload i dwukierunkowy UART

## Granice warstw

1. **PHY** — nRF24L01+, FIFO, zmienna długość 8..32 B, sprzętowe CRC/auto-ACK, SPI8 MHz.
2. **Ramka i link** — `PairedFrame` (8B + opaque BODY do24B), `PairedLink` (diagnostyka obu kierunków, SID, RX_CREDIT).
3. **Sesja** — `PairedSession` i `EpochJournal` z EEPROM; SID16 zmienia się po renegocjacji.
4. **Opcjonalna usługa UART** — `UartReliableStream<Port>` (2B transportowe + 22B danych), niezależne app ACK w obu kierunkach, credit oparty na sinku. Nie zmienia formatu rdzenia.
5. **Aplikacja** — jeden `main.cpp` łączy moduły; oba realne porty UART są **wyłącznie** strumieniem danych.

## Wyniki już potwierdzone

- Na dwóch RF-Nano: ~625/s DATA↔ACK Payload, ~612–613/s z handshake, restarty A, B i obu.
- W CI: hostowe testy UartReliableStream, w tym 4096 bajtów w każdą stronę z utratami ramek i ACK; kompilacja AVR dla aktywnego `main.cpp` PASS.
- Nie zmieniać pinoutu, C++11, statycznych buforów, F7/F12 ani Post-Build.

## Kolejność dalszego odbioru

1. **TERAZ:** fizyczny test nowego mostu UART, text A→B, B→A, równocześnie i wiadomości >22 B; porównanie binary capture dla dłuższych strumieni.
2. Ustalić i zmierzyć rzeczywisty UART goodput, latencję i zliczanie overflow w obu kierunkach. Użytkownik testuje RealTerm z kontrolowaną szybkością wysyłania.
3. Zweryfikować retry aplikacyjne, utracony ACK Payload, stop przy pustym `RX_CREDIT` i odtworzenie sesji po resecie, bez mylenia sprzętowego ACK z dostarczeniem UART.
4. Później: większe okno / selective repeat i BULK, jeśli wymagane przez pomiary. Obecna architektura pozwala rozbudować usługę bez modyfikacji `PairedFrame` i PHY.
5. Oddzielnie: trwałe `DEVICE_ID`/`PAIR_ID` niezależne od zmieniającego się SID, jeśli projekt parowania tego wymaga.

## Ograniczenia

W UART RX jest tylko 127 B i brak sprzętowego flow control. Przy przeciążeniu wejścia nie ma gwarancji bezstratności; kredyty chronią sink, nie zapobiegają utracie wejścia CH340→RX ISR. W trybie UART zrezygnowano z logów po tym samym COM. Pełne exactly-once przez reset własnego MCU wymaga trwałego transakcyjnego magazynu — obecny stop-and-wait zapewnia deduplikację **w obrębie sesji**, z zachowaniem oczekującego pakietu na żyjącym nadajniku.
