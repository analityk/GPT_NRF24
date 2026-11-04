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

## Diagnoza bilansu przepustowości przy dwóch aktywnych UART (2026-11-04)

- Wykazano programowo, że obecny transport stop-and-wait, 22B/segment i co najmniej 2 wymiany RF na nowy segment przy opóźnionym ACK może ograniczać transfer użytkowy do ok. 6,7 kB/s/kierunek (przy 613 transakcjach/s), poniżej pełnego UART 115200 8N1 11,52 kB/s. Jeden kierunek może mieć krótsze ACK na radiu, więc czas transakcji nie musi być identyczny z ruchem dwukierunkowym.
- Dodano test obciążenia wejścia bez flow-control i stwierdzono przepełnienie 127B RX w obu kierunkach mimo idealnego RF. Zidentyfikowano lukę w dotychczasowym teście 4096B/kierunek: idealne wstrzymywanie producenta i zaledwie 3B/wymianę.
- Kolejny etap przed zmianą transportu: pomiar liczników `UART::Counters` na sprzęcie i binarne capture, próba z 1ms/znak RealTerm na obu nadajnikach.
- Docelowo rozbudować **opcjonalny transport** w okno co najmniej 2 segmentów per kierunek, z cumulative/selective ACK według potrzeb, uniezależnionym tempem przyjmowania UART ingress i deduplikacją. Nie podmieniać samego rdzenia 8+24 ani zachowania RF ACK Payload bez uzasadnienia.
