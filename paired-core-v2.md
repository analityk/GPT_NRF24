# Sparowana para P2P — rdzeń RF 8+24, profil diagnostyczny

**Wersja dokumentu:** 2026-11-03. **Status:** zaimplementowany i przetestowany na hoście oraz skompilowany krzyżowo AVR; **nieprzetestowany jeszcze fizycznie**. Oddzielny format od `protocol-v1.md`, który pozostaje formatem poprzedniego mostu UART.

## Cel i zakres

Dwa urządzenia z jednym radiem nRF24L01+ każde: A utrzymuje PTX, B utrzymuje PRX. Pełny dupleks LOGICZNY realizują DATA A→B i wcześniej przygotowany sprzętowy ACK Payload B→A. Przebiegi radiowe nie zachodzą jednocześnie. Rdzeń przesyła **BODY jako nieprzezroczysty ciąg bajtów**; nie interpretuje terminala, pliku, TCP-like ani ewentualnego szyfrogramu. W szczególności nie zapewnia aplikacyjnego niezawodnego dostarczenia, ponieważ auto-ACK radia nie oznacza przyjęcia przez usługę.

`FEATURE.EN_DPL` i `FEATURE.EN_ACK_PAY` aktywne, `DYNPD.DPL_P0` aktywny. DPL definiuje rzeczywistą szerokość odczytaną z radia przez R_RX_PL_WID. Payload ramki radiowej ma **8..32 B**; długość BODY = szerokość − 8, od 0 do 24 B. Dla IDLE długość BODY=0, czyli *8 B na radiu*; dla nasyconego DATA długość 32 B. Sprzętowe CRC16, adres 5B, kanał 76, 1Mb/s; dla benchmarku SPI8MHz.

## Formalna mapa nagłówka — dokładnie 8 bajtów

| Offset | Pole | Bity i znaczenie |
|---|---|---|
| 0 | `TX_SEQ` | 8-bitowy numer **nowej** ramki własnego urządzenia, modulo 256; DATA, IDLE i CONTROL; wewnętrzne retransmisje nRF nie zmieniają numeru |
| 1 | `RX_LAST` | Numer ostatniej **poprawnej ramki RF** partnera; przed pierwszym odbiorem `FF`; **nie jest** aplikacyjnym ACK ani potwierdzeniem odbioru przez UART |
| 2 | `LOSS` | b3..0: licznik luk **A→B**, właściciel B; b7..4: licznik luk **B→A**, właściciel A; druga wartość jest echem ostatnio znanej partnerowi |
| 3 | `QUALITY` | b3..0 `ARC_LAST`: ostatni odnotowany ARC_CNT strony PTX A; na PRX B pole jest echem ostatnio widzianej wartości A; b7..4 `RX_DROP`: lokalne odrzucenia poprawnych ramek z braku zasobów lub przez sink |
| 4 | `RX_CREDIT` | Liczba dodatkowych pełnych segmentów BODY o długości **24 B**, które lokalny odbiornik może przyjąć; 0 = wstrzymaj nowe DATA; nie oznacza gotowości radia do wysyłania sprzętowego ACK |
| 5 | `TYPE_FLAGS` | b1..0 TYPE, b2 URGENT, b3 PAUSE, b4 RESYNC, b5 WANT_PTX, b6 OVERFLOW, b7 REPORT |
| 6 | `SESSION_ID_LO` | Młodszy bajt 16-bitowego ID aktywnej sesji |
| 7 | `SESSION_ID_HI` | Starszy bajt ID sesji |
| 8..(7+LEN) | `BODY` | **0–24 dowolnych bajtów**, bez interpretacji na poziomie PHY/MAC |

`TYPE=0` DATA, `1` IDLE (BODY=0), `2` CONTROL (co najmniej 1 bajt BODY), `3` EXT (zarezerwowane). Zapis wielobajtowy little-endian, brak C++ packed, brak kopii nagłówka jako struktury ABI. Nie używamy jeszcze większości flags w działającej aplikacji diagnostycznej; to przestrzeń zarezerwowana na późniejszy sterownik MAC.

## Diagnostyka i reguły

- **Braki nie są BER**. Licznik LOSS jest obserwacją luk w numerach nowo wysłanych ramek. Jeśli radio wykona retransmisję i ostatecznie doręczy pakiet, ARC może wzrosnąć bez luki. Jeśli całe łącze zamilknie, brak nowych numerów nie daje sam z siebie liczby brakujących ramek.
- Dla 8-bitowej sekwencji różnice `1..127` są traktowane jako nowe numery, `0` jako duplikat, `128..255` jako stare/niejednoznaczne. Numeracja `255→0` jest prawidłowa. Liczniki nibbles narastają modulo 16 i **nie są pełną statystyką**; lokalne `PairedLink::Counters` utrzymują szersze podsumowania.
- Wcześniejszy/stary pakiet **nie może cofnąć `RX_CREDIT` ani telemetrii**: aktualizacja odbywa się wyłącznie dla nowego numeru w tej samej sesji. Wyższa warstwa, która chce lepszej rozdzielczości, może później użyć pełnego raportu CONTROL.
- W przypadku ACK Payload informacja `RX_LAST`, `RX_CREDIT` i `LOSS` jest **opóźniona o co najmniej jedną transakcję**, ponieważ PRX musi załadować ACK zanim nastąpi odbiór wyzwalającego DATA. Odbiornik musi odliczać kredyty również dla danych będących w locie; wysyłanie tylko na podstawie świeżo otrzymanego kredytu nie jest kompletne bez tej ostrożności.
- `RX_DROP` to lokalne odrzucenie poprawnego pakietu **przez wyższą warstwę**, nie liczba fizycznie uszkodzonych ramek. Sygnał `MAX_RT` nadajnika nie dowodzi, że odbiorca nie widział danych. `ARC_LAST` pochodzi tylko z rejestru `OBSERVE_TX.ARC_CNT` PTX. PRX nie ma niezależnego ARC dla ACK Payload.
- Kodek pozostawia bufor wyjściowy nietknięty przy błędzie, a `PairedLink` ma resetowalny, niezależny od MCU stan. `SESSION_ID` sprawdzany jest przed zmianą liczników i kredytów. 16 bitów **nie zapewnia bezpieczeństwa kryptograficznego ani bezkolizyjności po resetach**.

## Implementacja i przełączanie aplikacji

`GPT_NRF2401/application_select.h` definiuje `GPT_NRF24_APPLICATION`:

- `0` — wcześniejszy most UART, protokół `frame_v1.h`, osobna sesja v1 i programowanie F12 bez zmian;
- `1` — zmierzony sprzętowo benchmark **surowych 32+32 B**, wcześniej 540 wymian/s;
- `2` — NOWY test tego nagłówka: ramki IDLE 8 B i DATA 32 B (8+24), wielokrotnego użytku kodek `paired_frame.h`, stan `paired_link.h`, szybki `nrf24_ack_phy.h`.

W obecnej aplikacji `2` **obie strony używają testowego, sztywnego SESSION_ID=0x4E52**. To *fixture*, a nie sesja naprawdę uzgodniona. Po rzeczywistym parowaniu trzeba przekazać ID z niezależnego mechanizmu handshake i odświeżać je po resetach. Program nie wdraża jeszcze automatycznego parowania, wymiany kluczy, nowego transferu plikowego ani gwarancji dostarczenia po restarcie. Wybór 2 nie unieważnia fizycznego potwierdzenia 0/1, ale **nowy wariant wymaga odrębnego testu na płytkach**.

A0 do GND → A/PTX, A0 do własnego plusa → B/PRX; wspólny HEX. F7 buduj, F12 programuj ręcznie. RealTerm 115200 8N1 wyświetla **diagnostykę raz/s**, nie strumień przesyłany między terminalami. Nie wpisuj tekstu, aby go przesłać: dane użytkownika obecnie są deterministycznym, **binarnym wzorcem 24 B**, który aplikacja tylko tworzy/sprawdza, a sam kodek i PHY go nie interpretują. To fizyczny eksperyment rdzenia, nie docelowy transport UART.

Oczekiwany układ raportów:

```text
PAIR CORE A PTX 8+24B SID16 SPI8
A TX/s=... ACK/s=... ARC=... MAX=... BAD=... IO=... GAB=... GBA=... DROP=... FULL=...
PAIR CORE B PRX 8+24B SID16 SPI8
B RX/s=... Q/s=... ARC=... MAX=... BAD=... IO=... GAB=... GBA=... DROP=... FULL=...
```

`TX/s` liczy operacje potwierdzone przez radio, `ACK/s`/B `RX/s` liczy nowe, poprawne ramki pasującej sesji (włącznie z krótkim IDLE). `Q/s` B oznacza **umieszczenie** pakietu odpowiedzi w kolejce ACK, a nie dowód jego dostarczenia do A. `ARC` w raporcie A sumuje sprzętowe powtórzenia, na PRX B jest 0 (nie ma tam lokalnej obserwacji PTX). `GAB/GBA` i `DROP` są wartościami nibbles modulo16; `BAD` obejmuje błędną treść wzorca, ramkę lub SID. `FULL` liczy zatkanie kolejki ACK.

Zerowe błędy testu wzorca 24 B oznaczają integralność danych w obserwowanym eksperymencie, ale **nie dowodzą niezawodnego transportu użytkowego**. W stanie ciągłego DATA i przy tym samym tempie 540/s hipotetycznie: **540×24 = 12 960 B/s/kierunek**, niezmierzone na nowym kodzie. Aktualny limit UART 115200 8N1 = 11 520 B/s.

## Weryfikacja

Host C++11: `tests/paired_link_test.cpp` (codec, 8..32 DPL, IDLE, credit, nibbles, session, duplikaty, rollover), `tests/nrf24_paired_phy_test.cpp` (DPL 8/32, ACK empty/variable, MAX_RT, timeout, pełna kolejka), `tests/nrf24_mock_test.cpp` (nRF SPI W_TX_PAYLOAD o zmiennej długości). Skrypt `tests/run_host_tests.sh`, CI GitHub Actions. AVR-GCC: `tests/build_avr.sh` buduje bez flashowania **wszystkie tryby 0/1/2** (nie zmienia Microchip Studio). Wyniki sprzętowe dla nowej ramki: **jeszcze niepotwierdzone**.

Najbliższe kolejne etapy: automatyczne wynegocjowanie SESSION_ID16 i reset recovery w warstwie sesji, wiarygodne `RX_CREDIT` z realnym odbiornikiem (UART/sink), pełna telemetria `ARC/MAX_RT` i diagnostyka przy pogorszonym sygnale, oraz test 24 B+8 B z rzeczywistym throughput. Wymagania aplikacyjne (TCP-like, pliki, szyfrowanie) pozostają poza zakresem rdzenia.

**Źródło hardware:** Nordic nRF24L01+ Product Specification v1.0 (FEATURE/DYNPD, ACK payload, FIFOs, OBSERVE_TX, SPI), https://cdn.sparkfun.com/assets/3/d/8/5/1/nRF24L01P_Product_Specification_1_0.pdf
