# Paired Core v2 — specyfikacja rdzenia i opcjonalnej usługi UART

**Status (2026-11-04):** nagłówek 8B, hardware ACK Payload, negocjowane SID16 i fizycznie potwierdzone restarty A/B/obu. Użytkownik potwierdził działanie starego stop-and-wait UART przy jednym nadawcy, lecz zgłosił utratę bajtów przy dwóch aktywnych nadawcach. **Zaimplementowano więc WINDOW=4 i ciągły ping-pong bez 10ms sleep**; host+AVR PASS, test fizyczny nowej wersji pozostaje otwarty.

## Jeden rdzeń radiowy 8+24 B

A/PTX stale inicjuje transmisje, B/PRX odpowiada uprzednio przygotowanym sprzętowym ACK Payload. nRF24 jest fizycznie półdupleksowy. Radio 1 Mb/s, CRC16, SPI 8 MHz, `SETUP_RETR=0x23` (750 µs i 3 retry). DPL w nRF ustala rzeczywistą szerokość 8..32 B: `BODY_LENGTH = RF_WIDTH - 8`.

| Offset | Pole | Znaczenie |
|---|---|---|
| 0 | `TX_SEQ` | DATA: numer segmentu użytkownika modulo256, bez zmiany przy retry; IDLE: bieżący numer bez nowego segmentu |
| 1 | `RX_LAST` | kumulacyjny ACK ostatniego kolejnego segmentu **zaakceptowanego do UART TX** partnera; `0xFF` przed pierwszym przyjęciem |
| 2 | `LOSS_AB/LOSS_BA` | 2 niezależne czterobitowe liczniki wykrytych luk |
| 3 | `ARC_LAST/RX_DROP` | 2 czterobitowe wskaźniki stanu |
| 4 | `RX_CREDIT` | wolne miejsce u odbiorcy w jednostkach pełnych 24B, obecnie `UART::freeSpace()/24` |
| 5 | `TYPE_FLAGS` | `DATA=0`, `IDLE=1`, `CONTROL=2`, `EXT=3`, flagi URGENT/PAUSE/RESYNC/WANT_PTX/OVERFLOW/REPORT |
| 6–7 | `SESSION_ID16` | little-endian, nowy po renegocjacji |
| 8..31 | `BODY` | 0–24 bajty **opaque** dla PHY, kodeka i linku |

Kredyt jest tylko wskazówką dla nadawcy; wiarygodnym testem przyjęcia jest `UART::writeAll` w odbiorniku, który atomowo sprawdza miejsce i dopiero potem rezerwuje bajty w TX. Hardware ACK może się pojawić, mimo że warstwa UART nie miała miejsca — nie należy interpretować `TX_DS` jako aplikacyjnego potwierdzenia.

## Usługa `UartReliableStream` (opcjonalna, obecnie wybrana w main)

W stanie ACTIVE pola sterujące znajdują się **wyłącznie w ośmiobajtowym nagłówku MAC**, nie w 24B BODY:

| Pole MAC | Rola w aktywnym strumieniu |
|---|---|
| `TX_SEQ` | Numer nowego segmentu danych (modulo256), powtórzony bez zmian przy retransmisji |
| `RX_LAST` | APP_ACK: ostatni numer segmentu partnera zaakceptowany w całości do UART TX |
| `RX_CREDIT` | Wolne miejsce na segmenty u odbiorcy; nie zastępuje fizycznej kontroli przyjęcia |
| `BODY[0..23]` | **0–24 bajty nieinterpretowanego UART**, bez DATA_SEQ ani ACK_SEQ |

Jeśli nie ma porcji użytkownika, `PairedLink::makeStream` tworzy ramkę `IDLE` **8 B DPL**. Nie nadaje wtedy nowego DATA_SEQ, ale wysyła aktualny `RX_LAST` i `RX_CREDIT` w nagłówku. Pusta ramka nadal wyzwala sprzętowy ACK Payload w drugim kierunku. Przy pełnym DATA wielkość RF payloadu wynosi 32 B (8+24).

Nadawca trzyma do **czterech niepotwierdzonych segmentów** (4×24B) w stałym buforze. Numer `DATA_SEQ` jest przydzielany każdej nowej porcji, a `ACK_SEQ` potwierdza **cały ciąg kolejno przyjętych segmentów** (cumulative ACK). W czasie nadejścia spóźnionego ACK inne ramki mogą przenosić nowe DATA. Przy braku postępu przez 8 własnych wymian nadajnik wykonuje **Go-back-N**, zaczynając od najstarszego niepotwierdzonego segmentu. Odbiornik przyjmuje tylko następny oczekiwany numer; przyszłe numery po luce są odrzucane i retransmitowane. Pełny UART TX nie powoduje aplikacyjnego ACK. Duplikaty segmentów nie wypisują bajtów drugi raz w obrębie danej sesji.

**Metadane B w ACK Payload są przygotowane przed odbiorem aktualnej ramki A**, zatem informacja w nich jest opóźniona co najmniej o jedną transakcję. Nie można na ich podstawie usuwać segmentu przed sprawdzeniem `RX_LAST` w aktualnej sesji.

`beginSession(sid)` zeruje numery, ale **nie wyrzuca niepotwierdzonego segmentu** pobranego z UART RX. Na MCU, który nie był resetowany, porcja może zostać wznowiona po nowym SID. Reset MCU lokalnego kasuje RAM, zaś potwierdzony wcześniej i wypisany do UART segment może zostać powtórzony po zmianie sesji, jeśli jego ACK zaginął. Dlatego dokładnie-raz **ponad resetem** wymaga trwałej wyższej warstwy — nie obiecujemy go teraz.

## Sesja

HELLO/BEACON/WELCOME/READY/READY_ACK to osobna maszyna `PairedSession` z trwałą `boot_epoch32` w EEPROM i `attempt16`. SID16 pochodzi z CRC16 uporządkowanych epok i prób obu urządzeń; nie jest zabezpieczeniem kryptograficznym. Utracona sesja powoduje odrzucenie starego ACK Payload oraz renegocjację z nowym SID. `DEVICE_ID`/`PAIR_ID` to przyszłe osobne identyfikatory trwałe — nie zmieniamy w tej fazie polityki SID.

## Czas, sterowanie i pamięć

Rola A wyzwala odpowiedź B **w każdej kolejnej iteracji, nawet bez danych**. Nie ma sztucznego oczekiwania 10 ms ani 20/25 ms; pozostaje wyłącznie ochrona czasowa PHY (~260 µs między zakończeniem a kolejnym CE), transmisja SPI i rzeczywisty czas RF/ACK/retry. W stanie IDLE ramka usługowa ma 8 B na radiu (wyłącznie MAC), w pełnym DATA 32 B. Timer1 2MHz służy do timeoutów sprzętowych, a nie synchronizacji obu MCU. Dwa kierunki współdzielą jedną wymianę Enhanced ShockBurst; **okno transportowe = 4** na kierunek, a nie dawne 1. Okno nie narzuca semantyki plikowej, szyfrowania ani BULK.

UART RX/TX po 127 bajtów użytecznych, 115200 8N1, brak RTT/CTS. Bez sprzętowego flow control bardzo szybki plik lub długa przerwa RF mogą przepełnić wejściowy ring. `RX_CREDIT` gwarantuje tylko, że nowy segment nie zostanie świadomie przyjęty przez przepełniony **wyjściowy** UART TX. Nie przywraca danych zgubionych przed odbiorem z CH340.

**UART w obu płytkach przenosi wyłącznie bajty aplikacyjne.** Do konsoli nie dopisujemy `NRF OK`, SID ani licznika. Statystyki UART/transport/PHY pozostają w RAM, do ewentualnej diagnostyki debugerem lub osobnym kanałem.

## Weryfikacja

- Host: `tests/uart_reliable_stream_test.cpp`, `tests/uart_reliable_stream_stress_test.cpp`, `tests/uart_pipeline_rekey_test.cpp`, `tests/uart_unthrottled_ingress_test.cpp`; niezależne 4096 bajtów na kierunek przy utratach ACK i RF, 4 segmenty w locie, zawijanie SEQ, trzy segmenty zachowane po zmianie SID, wymuszona luka, przepełnienie sinku i test 19 B/wymianę przy obu aktywnych UART. Wszystkie wymienione testy hostowe PASS.
- AVR-GCC CI: `sh tests/build_avr.sh` kompiluje jedyny firmware ATmega328P C++11 bez programowania F12.
- Sprzęt: wcześniej 625/s z formatem 8+24, ~612–613/s z handshake, resety A/B/obu. **Nowego UART↔UART należy przetestować oddzielnie** — jest nowym transportem i jego goodput pozostaje niezmierzony.

Dane sprzętowe: Nordic nRF24L01+ Product Specification v1.0, https://cdn.sparkfun.com/assets/3/d/8/5/1/nRF24L01P_Product_Specification_1_0.pdf

## Równoczesny UART flood — ograniczenie obowiązującej wersji

Użytkownik potwierdził transfer UART jednym kierunkiem, ale zgłosił ubytki, gdy oba RealTerm wysyłają pliki jednocześnie. UART ATmegi pracuje asynchronicznie pełnodupleksowo, z ISR RX/UDRE i dwoma ringami 127B. PHY zachowuje sprzętowy ACK Payload, lecz transport **stop-and-wait** czeka na aplikacyjny ACK, co ogranicza generowanie nowych 22-bajtowych segmentów; ACK z B jest dodatkowo opóźniony o wymianę. Z referencyjnym ~613 wymian/s wynika orientacyjnie `22×613/2 ≈ 6,74 kB/s/kierunek`, wobec `115200/10 = 11,52 kB/s/kierunek` na wejściu UART. Przy maksymalnym źródle i zerowej kontroli przepływu ring 127B może przepełnić się w ~27ms. Pełna dwukierunkowa wymiana 32B+32B radiowo trwa dłużej niż 32B+10B z małym ACK przy jednym kierunku.

Dotychczasowy test hostowy 4096B/kierunek był ograniczony do 3B/wymianę i pauzował na zapełnieniu wejścia (symulował flow control). Nowy `tests/uart_unthrottled_ingress_test.cpp` używa realnego kodu transportu, dwóch ByteRing<128> RX/TX na stronę, opóźnienia ACK, nieograniczonych producentów wejścia i sztucznie idealnej RF: 3B/wymianę bez overflow, **19B/wymianę powoduje utratę na obu UART RX**. Nie jest to pomiar rzeczywistych wartości sprzętowych; na urządzeniach nie odczytano jeszcze `rx_buffer_full` ani `rx_overrun`. Pierwszy jest overflow ring, drugi DOR0 hardware. `blocked` w transporcie oznacza nieprzyjęty segment przy pełnym UART TX — niekoniecznie utratę danych.

Przed modyfikacją rdzenia: sprawdzić RealTerm `Delays=1,0` (1ms na znak, bez drugiego delay) i pliki capture; następnie projektować mechanizm window>=2 oraz niezależne ograniczanie ingress, ponieważ większy bufor bez kontroli przepływu nie gwarantuje transferu bez strat. `SESSION_ID` i 8B nagłówka nie wymagają zmian. **W tej aktualizacji żadnej produkcyjnej warstwy RF/UART nie modyfikowano.**

## Aktualizacja protokołu transportowego: pełne tempo i Go-back-N (2026-11-04)

Zmiana obejmuje **wyłącznie implementację usługi UART w opaque BODY**, nie 8-bajtowy nagłówek, SID ani rolę nRF. `DATA_SEQ` oznacza kolejne nowo pobrane z UART porcje, `ACK_SEQ` oznacza ostatnią **kolejną poprawnie przyjętą do UART TX** porcję od partnera. Każda wymiana nRF może nieść nową porcję A→B i B→A niezależnie od opóźnienia potwierdzeń. Odbiornik stosuje prosty Go-back-N (bez bufora out-of-order): pierwsza luka blokuje przyjęcie dalszych numerów do czasu retransmisji. Każde `UART::writeAll` jest atomowe; odmowa powoduje ponowienie, a nie ACK. **Okno 4** ogranicza zapotrzebowanie SRAM do 88B cache węzła, a ogólny firmware po cross-buildzie AVR ma `.data=96B`, `.bss=591B` (687B stałego SRAM, bez stosu).

Model 600 transakcji/s i 19 B wejścia/transakcję na każdą stronę (11 400 B/stronę) uzyskuje 0 przepełnień RX na A i B przy bezbłędnym RF; przeciążenie 24 B/transakcję prowadzi do przepełnień. **To potwierdza architekturę potoku, lecz nie dowodzi wydajności na fizycznych dwóch RF-Nano.** Trzeba zmierzyć pełny dwukierunkowy UART z `Delays=0`, osobno liczniki RX overflow i DOR0 oraz rzeczywistą wydajność nowego MCU/PHY. Dokładnie-raz przez twardy reset obu uC nadal nie jest obiecane.

## Rozdział odpowiedzialności — korekta 2026-11-04

Kod `UartReliableStream` zachowuje WINDOW=4 i Go-back-N, ale nie zapisuje już żadnych danych kontrolnych w BODY. `PairedLink::makeStream/observeStream` dostarcza i odczytuje DATA_SEQ oraz APP_ACK w **TX_SEQ/RX_LAST**, a ramki IDLE pozostają pełnoprawnym wyzwalaczem sprzętowego ACK Payload. `observeStream` nie filtruje powtórzonego DATA przed analizą jego świeższego ACK; deduplikację oryginalnych bajtów UART wykonuje wyłącznie transport. `PairedLink` rozróżnia odbiór fizycznej ramki od obserwacji **luk w numeracji segmentów**. To świadomy kompromis: nie da się jednocześnie numerować w TX_SEQ wszystkich pustych wymian i danych/ponowień jako tych samych numerów; liczniki jakości fizycznej pozostają dostępne poprzez ARC/MAX_RT, statystyki lokalne i raporty kontrolne. Nie używać starego opisu `TX_SEQ` jako licznika każdego RF heartbeat.

F7/F12 pozostaje ręczne, brak Post-Build flash. Host i AVR-GCC PASS; **brak jeszcze fizycznego testu nowego 24B transportu**.
