# Plan prac — rdzeń 8+24 B z ACK Payload i dwukierunkowy UART

## Granice warstw

1. **PHY** — nRF24L01+, FIFO, zmienna długość 8..32 B, sprzętowe CRC/auto-ACK, SPI8 MHz.
2. **Ramka i link** — `PairedFrame` (8B + opaque BODY do24B), `PairedLink` (diagnostyka obu kierunków, SID, RX_CREDIT).
3. **Sesja** — `PairedSession` i `EpochJournal` z EEPROM; SID16 zmienia się po renegocjacji.
4. **Opcjonalna usługa UART** — `UartReliableStream<Port>` (do 24B danych w całym opaque BODY; DATA_SEQ/ACK_SEQ w polach TX_SEQ/RX_LAST 8B MAC), niezależny ACK w obu kierunkach, credit oparty na sinku. Nie zwiększa nagłówka rdzenia.
5. **Aplikacja** — jeden `main.cpp` łączy moduły; oba realne porty UART są **wyłącznie** strumieniem danych.

## Wyniki już potwierdzone

- Na dwóch RF-Nano: ~625/s DATA↔ACK Payload, ~612–613/s z handshake, restarty A, B i obu.
- W CI: hostowe testy UartReliableStream, w tym 4096 bajtów w każdą stronę z utratami ramek i ACK; kompilacja AVR dla aktywnego `main.cpp` PASS.
- Nie zmieniać pinoutu, C++11, statycznych buforów, F7/F12 ani Post-Build.

## Kolejność dalszego odbioru

1. **TERAZ:** fizyczny test nowego mostu UART, text A→B, B→A, równocześnie i wiadomości >24 B; porównanie binary capture dla dłuższych strumieni.
2. Ustalić i zmierzyć rzeczywisty UART goodput, latencję i zliczanie overflow w obu kierunkach. Użytkownik testuje RealTerm z kontrolowaną szybkością wysyłania.
3. Zweryfikować retry aplikacyjne, utracony ACK Payload, stop przy pustym `RX_CREDIT` i odtworzenie sesji po resecie, bez mylenia sprzętowego ACK z dostarczeniem UART.
4. Później: większe okno / selective repeat i BULK, jeśli wymagane przez pomiary. Obecna architektura pozwala rozbudować usługę bez modyfikacji `PairedFrame` i PHY.
5. Oddzielnie: trwałe `DEVICE_ID`/`PAIR_ID` niezależne od zmieniającego się SID, jeśli projekt parowania tego wymaga.

## Ograniczenia

W UART RX jest tylko 127 B i brak sprzętowego flow control. Przy przeciążeniu wejścia nie ma gwarancji bezstratności; kredyty chronią sink, nie zapobiegają utracie wejścia CH340→RX ISR. W trybie UART zrezygnowano z logów po tym samym COM. Pełne exactly-once przez reset własnego MCU wymaga trwałego transakcyjnego magazynu — obecny transport okienkowy zapewnia deduplikację **w obrębie sesji**, z zachowaniem oczekującego pakietu na żyjącym nadajniku.

## Diagnoza bilansu przepustowości przy dwóch aktywnych UART (2026-11-04)

- Wykazano programowo, że obecny transport stop-and-wait, 22B/segment i co najmniej 2 wymiany RF na nowy segment przy opóźnionym ACK może ograniczać transfer użytkowy do ok. 6,7 kB/s/kierunek (przy 613 transakcjach/s), poniżej pełnego UART 115200 8N1 11,52 kB/s. Jeden kierunek może mieć krótsze ACK na radiu, więc czas transakcji nie musi być identyczny z ruchem dwukierunkowym.
- Dodano test obciążenia wejścia bez flow-control i stwierdzono przepełnienie 127B RX w obu kierunkach mimo idealnego RF. Zidentyfikowano lukę w dotychczasowym teście 4096B/kierunek: idealne wstrzymywanie producenta i zaledwie 3B/wymianę.
- Kolejny etap przed zmianą transportu: pomiar liczników `UART::Counters` na sprzęcie i binarne capture, próba z 1ms/znak RealTerm na obu nadajnikach.
- Docelowo rozbudować **opcjonalny transport** w okno co najmniej 2 segmentów per kierunek, z cumulative/selective ACK według potrzeb, uniezależnionym tempem przyjmowania UART ingress i deduplikacją. Nie podmieniać samego rdzenia 8+24 ani zachowania RF ACK Payload bez uzasadnienia.

## Wdrożona odpowiedź na problem stop-and-wait — 2026-11-04

Wykonano: usunięcie 10ms idle scheduler z `main.cpp`, w `UartReliableStream` utworzenie statycznego bufora 4×22B per direction, cumulative ACK i Go-back-N z cyklem 8 wymian bez postępu; bez zmian `PairedFrame`, nRF PHY i handshake SID. Testy na hoście PASS: jednoczesne 19B w obu kierunkach per wymianę bez overflow przy idealnym RF, 24B per wymianę z overflow, strata pierwszego pakietu, 3 pakiety in-flight przy rekey (w tym stary baseSeq≠0), 4096B pod stratami i wrap. AVR-GCC PASS, 687B stałego SRAM w .data+.bss.

Teraz **konieczny odbiór fizyczny** nowej wersji przy 0ms delay w obu RealTerm i porównanie plików, bo model nie odtwarza realnego czasu RF, czasu obsługi ISR, wszystkich sprzętowych FIFO/CRC i faktycznego tempa USB CH340. Dopiero z pomiaru wynika dalsza decyzja o flow-control albo optymalizacji PHY/CPU. Nie obiecywać matematycznie 13,49 kB/s po stronie UART — UART 115200 ma maks. 11,52 kB/s per direction.

## Bieżąca semantyka 8+24 — obowiązuje od 2026-11-04

ACTIVE: ciągłe fizyczne wymiany PTX→PRX + sprzętowy ACK Payload w przeciwną stronę, bez idle scheduler. `PairedLink::makeStream/observeStream` koduje DATA_SEQ=TX_SEQ i cumulative acceptance ACK=RX_LAST w nagłówku; BODY wyłącznie 0..24 bajty użytkownika, ramki IDLE są 8B i nadal przenoszą ACK oraz RX_CREDIT. Utrata/próby sprzętowe nRF i widoczne luki **segmentów** są osobnymi metrykami; nie obiecywać 8bitowej numeracji każdego heartbeat przy równoczesnym nadpisaniu TX_SEQ na segment. WINDOW=4, Go-back-N i sesje SID zostają. Potrzebny fizyczny test pełnego obustronnego UART i binarne porównanie capture, bez przypisywania historycznych 612/s nowemu firmware.

## 2026-11-04 — druga osobna rodzina: PP i BURST20

**PP** zostaje obowiązującym, sprzętowo pokazanym profilem. Punkt przywrócenia:
`checkpoint/pp-2026-11-04` @ `784fd83`. Wybór trybu: A1/PC1 pull-up
(HIGH/floating=PP; GND=BURST20) po resecie, rola A/B nadal A0.
`ProfileSelect` jest zaimplementowany i testowany hostowo, ale
**nie został jeszcze podłączony do main**, bo BURST PHY nie istnieje.
Nie wolno uruchomić pustego profilowego stubu jako pozornej implementacji.

**BURST20** — cykl lidera A zawsze 20ms, A nadaje pierwsze 5,10,15ms,
B nadaje resztę; w każdym slocie 1ms REPORT (ostatnia 32B ramka)
i 0,3ms GUARD na zmianę ról. Twardy deadline i zero nowych
DATA po przejściu w REPORT. Burst20Schedule (2MHz timer tick,
O(1) cycle rollover, fail-closed when out of sync) plus host testy PASS.
Dokładna faza i radio role switch wymagają osobnego sterownika PHY,
control/beacon synchronizacji co20ms i prób na dwóch płytkach.

Dane: RF 1Mbps, 32B on-air=329µs, w 5ms max 15 ramek
w fizycznie niemożliwym scenariuszu bez narzutów; przy rezerwie
report/guard i rzeczywistym MCU spodziewamy się znacznie mniej,
ok. 6–10 łącznie, **bez pomiaru**. 32 DATA +1 CONTROL w 5ms
nie są wykonalne ani przy 1Mbps, ani przy 2Mbps. Odbiór
END_REPORT z CRC32 i bitmapą strat jest opóźniony o stronę/cykl,
nie znamy strat w momencie, gdy nadajnik raportuje swój blok.

Przed fizycznym BURST: nowy `NrfBurstPhy` (NO_ACK DATA,
bounded REPORT, FIFO 3, bez przestojów PWR_UP przy zmianie
kierunku); `BurstSession` synchronizująca zegary i role,
`BurstReport` z testami bitowego formatu i CRC; `BurstRunner`
z rzeczywistym UART. Przy 5/15 koniecznie bilans ingress:
15ms×11520B/s≈173B >127B obecnego RX ring. Start
z 10/10 i kontrolowanym źródłem, bez niekontrolowanego UART flood.
Szczegóły w `burst-20ms.md`.
