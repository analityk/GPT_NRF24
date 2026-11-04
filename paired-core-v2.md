# Paired Core v2 — specyfikacja rdzenia i opcjonalnej usługi UART

**Status (2026-11-04):** nagłówek 8B, hardware ACK Payload, negocjowane SID16 i fizycznie potwierdzone restarty A/B/obu. **Nowa usługa UART↔UART jest w firmware, host/AVR PASS, lecz nie sprawdzona jeszcze na sprzęcie.**

## Jeden rdzeń radiowy 8+24 B

A/PTX stale inicjuje transmisje, B/PRX odpowiada uprzednio przygotowanym sprzętowym ACK Payload. nRF24 jest fizycznie półdupleksowy. Radio 1 Mb/s, CRC16, SPI 8 MHz, `SETUP_RETR=0x23` (750 µs i 3 retry). DPL w nRF ustala rzeczywistą szerokość 8..32 B: `BODY_LENGTH = RF_WIDTH - 8`.

| Offset | Pole | Znaczenie |
|---|---|---|
| 0 | `TX_SEQ` | numer nowej ramki RF modulo256 (nie aplikacyjny segment UART) |
| 1 | `RX_LAST` | numer ostatniej poprawnej ramki partnera, **nie gwarancja wpisania do UART** |
| 2 | `LOSS_AB/LOSS_BA` | 2 niezależne czterobitowe liczniki wykrytych luk |
| 3 | `ARC_LAST/RX_DROP` | 2 czterobitowe wskaźniki stanu |
| 4 | `RX_CREDIT` | wolne miejsce u odbiorcy w jednostkach pełnych 24B, obecnie `UART::freeSpace()/24` |
| 5 | `TYPE_FLAGS` | `DATA=0`, `IDLE=1`, `CONTROL=2`, `EXT=3`, flagi URGENT/PAUSE/RESYNC/WANT_PTX/OVERFLOW/REPORT |
| 6–7 | `SESSION_ID16` | little-endian, nowy po renegocjacji |
| 8..31 | `BODY` | 0–24 bajty **opaque** dla PHY, kodeka i linku |

Kredyt jest tylko wskazówką dla nadawcy; wiarygodnym testem przyjęcia jest `UART::writeAll` w odbiorniku, który atomowo sprawdza miejsce i dopiero potem rezerwuje bajty w TX. Hardware ACK może się pojawić, mimo że warstwa UART nie miała miejsca — nie należy interpretować `TX_DS` jako aplikacyjnego potwierdzenia.

## Usługa `UartReliableStream` (opcjonalna, obecnie wybrana w main)

W każdej ramce `DATA` w stanie ACTIVE:

| Offset BODY | Pole | Znaczenie |
|---|---|---|
| 0 | `DATA_SEQ` | numer bieżącego niezależnego segmentu danych, modulo256; nie zwiększa się przy retry |
| 1 | `ACK_SEQ` | ostatni kolejny segment partnera **przyjęty atomowo do UART TX** |
| 2..23 | `USER` | 0–22 bajty dowolnych danych UART, także NUL, CR/LF i 0xFF |

Ramka bez nowych danych nadal ma `TYPE=DATA`, 2-bajtowy BODY oraz radio DPL=10 B. Dzięki temu każdy następny trigger sprzętowego ACK niesie również app ACK. Nagłówek linku pozostaje **bez zmian** i nadal raportuje fizyczne TX_SEQ/RX_LAST; usługa posiada osobne, aplikacyjne DATA_SEQ/ACK_SEQ.

Nadawca trzyma jeden niepotwierdzony segment (do22B) we własnym stałym buforze i powtarza go do otrzymania aplikacyjnego ACK. W przypadku FULL po stronie UART TX odbiornik nie przyjmuje ani nie potwierdza segmentu; musi on zostać powtórzony. Duplikaty o zaakceptowanym numerze nie wypisują kolejnych bajtów. Stop-and-wait w obie strony jest symetryczny, choć role radiowe pozostają A/PTX B/PRX.

**Metadane B w ACK Payload są przygotowane przed odbiorem aktualnej ramki A**, zatem informacja w nich jest opóźniona co najmniej o jedną transakcję. Nie można na ich podstawie usuwać segmentu przed sprawdzeniem `ACK_SEQ` w aktualnej sesji.

`beginSession(sid)` zeruje numery, ale **nie wyrzuca niepotwierdzonego segmentu** pobranego z UART RX. Na MCU, który nie był resetowany, porcja może zostać wznowiona po nowym SID. Reset MCU lokalnego kasuje RAM, zaś potwierdzony wcześniej i wypisany do UART segment może zostać powtórzony po zmianie sesji, jeśli jego ACK zaginął. Dlatego dokładnie-raz **ponad resetem** wymaga trwałej wyższej warstwy — nie obiecujemy go teraz.

## Sesja

HELLO/BEACON/WELCOME/READY/READY_ACK to osobna maszyna `PairedSession` z trwałą `boot_epoch32` w EEPROM i `attempt16`. SID16 pochodzi z CRC16 uporządkowanych epok i prób obu urządzeń; nie jest zabezpieczeniem kryptograficznym. Utracona sesja powoduje odrzucenie starego ACK Payload oraz renegocjację z nowym SID. `DEVICE_ID`/`PAIR_ID` to przyszłe osobne identyfikatory trwałe — nie zmieniamy w tej fazie polityki SID.

## Czas, sterowanie i pamięć

Rola A wyzwala odpowiedź B nawet bez danych. Przy bezczynności ~1 zapytanie / 10 ms, po nadejściu danych szybki tor. Timer1 2MHz służy do timeoutów i odstępów fizycznych, nie synchronizacji dwóch MCU. Gdy oba kierunki przesyłają dużo danych, jednocześnie współdzielą wymiany Enhanced ShockBurst. W obecnej usłudze stop-and-wait rozmiar okna = 1; to **nie** nowy protokół BULK ani obietnica 15kB/s aplikacji.

UART RX/TX po 127 bajtów użytecznych, 115200 8N1, brak RTT/CTS. Bez sprzętowego flow control bardzo szybki plik lub długa przerwa RF mogą przepełnić wejściowy ring. `RX_CREDIT` gwarantuje tylko, że nowy segment nie zostanie świadomie przyjęty przez przepełniony **wyjściowy** UART TX. Nie przywraca danych zgubionych przed odbiorem z CH340.

**UART w obu płytkach przenosi wyłącznie bajty aplikacyjne.** Do konsoli nie dopisujemy `NRF OK`, SID ani licznika. Statystyki UART/transport/PHY pozostają w RAM, do ewentualnej diagnostyki debugerem lub osobnym kanałem.

## Weryfikacja

- Host: `tests/uart_reliable_stream_test.cpp` i `tests/uart_reliable_stream_stress_test.cpp`, 4096 bajtów na kierunek pod utratami pakietów/ACK i z opóźnionym sprzętowym ACK Payload, integralność bajtowa, deduplikacja, zapchany TX, zmiana SID i modulo256. Testy pozostałych warstw bez zmian.
- AVR-GCC CI: `sh tests/build_avr.sh` kompiluje jedyny firmware ATmega328P C++11 bez programowania F12.
- Sprzęt: wcześniej 625/s z formatem 8+24, ~612–613/s z handshake, resety A/B/obu. **Nowego UART↔UART należy przetestować oddzielnie** — jest nowym transportem i jego goodput pozostaje niezmierzony.

Dane sprzętowe: Nordic nRF24L01+ Product Specification v1.0, https://cdn.sparkfun.com/assets/3/d/8/5/1/nRF24L01P_Product_Specification_1_0.pdf

## Równoczesny UART flood — ograniczenie obowiązującej wersji

Użytkownik potwierdził transfer UART jednym kierunkiem, ale zgłosił ubytki, gdy oba RealTerm wysyłają pliki jednocześnie. UART ATmegi pracuje asynchronicznie pełnodupleksowo, z ISR RX/UDRE i dwoma ringami 127B. PHY zachowuje sprzętowy ACK Payload, lecz transport **stop-and-wait** czeka na aplikacyjny ACK, co ogranicza generowanie nowych 22-bajtowych segmentów; ACK z B jest dodatkowo opóźniony o wymianę. Z referencyjnym ~613 wymian/s wynika orientacyjnie `22×613/2 ≈ 6,74 kB/s/kierunek`, wobec `115200/10 = 11,52 kB/s/kierunek` na wejściu UART. Przy maksymalnym źródle i zerowej kontroli przepływu ring 127B może przepełnić się w ~27ms. Pełna dwukierunkowa wymiana 32B+32B radiowo trwa dłużej niż 32B+10B z małym ACK przy jednym kierunku.

Dotychczasowy test hostowy 4096B/kierunek był ograniczony do 3B/wymianę i pauzował na zapełnieniu wejścia (symulował flow control). Nowy `tests/uart_unthrottled_ingress_test.cpp` używa realnego kodu transportu, dwóch ByteRing<128> RX/TX na stronę, opóźnienia ACK, nieograniczonych producentów wejścia i sztucznie idealnej RF: 3B/wymianę bez overflow, **19B/wymianę powoduje utratę na obu UART RX**. Nie jest to pomiar rzeczywistych wartości sprzętowych; na urządzeniach nie odczytano jeszcze `rx_buffer_full` ani `rx_overrun`. Pierwszy jest overflow ring, drugi DOR0 hardware. `blocked` w transporcie oznacza nieprzyjęty segment przy pełnym UART TX — niekoniecznie utratę danych.

Przed modyfikacją rdzenia: sprawdzić RealTerm `Delays=1,0` (1ms na znak, bez drugiego delay) i pliki capture; następnie projektować mechanizm window>=2 oraz niezależne ograniczanie ingress, ponieważ większy bufor bez kontroli przepływu nie gwarantuje transferu bez strat. `SESSION_ID` i 8B nagłówka nie wymagają zmian. **W tej aktualizacji żadnej produkcyjnej warstwy RF/UART nie modyfikowano.**
