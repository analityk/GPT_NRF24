# Protokół RF v1 — specyfikacja binarna (projekt)

> **Uwaga o wersjach (2026-11-03):** ten plik opisuje **starszy** format 8B CTRL/SRC/DST/SID/TX_SEQ/RX_LAST/EP/EP używany w mostku UART (wariant 0). Nowy, odrębny profil P2P 8+24, z LOSS/ARC/RX_DROP/CREDIT/TYPE_FLAGS/SID16 i sprzętowym ACK Payload, jest opisany w [`paired-core-v2.md`](paired-core-v2.md) oraz zaimplementowany w `paired_frame.h`. Nie wolno bez negocjacji mieszać obu formatów.

**Status:** projekt do implementacji, bez testów na sprzęcie. **Zakres M1:** dwa węzły różnych klas. Radio nRF24L01+, stały payload 32 B, CRC16 sprzętowe poza payloadem, 1 Mb/s. Adres sprzętowy radia jest odrębny od SRC/DST. Nie ma dodatkowego CRC w payloadzie.

## 1. Nagłówek: 8 bajtów, BODY: 24 bajty

| Offset | Nazwa | Znaczenie bitów |
|---|---|---|
| 0 | CTRL | b7..6: VERSION=01; b5: KIND (0 DATA, 1 CONTROL); b4..0: LEN 0..24 |
| 1 | SRC | Klasa logiczna nadawcy 0..253 |
| 2 | DST | Klasa logiczna odbiorcy 0..253; FF broadcast; FE zarezerwowany |
| 3 | SID | Znacznik sesji 1..255; 0 tylko podczas discovery |
| 4 | TX_SEQ | Numer nowej ramki radiowej w sesji modulo 256 |
| 5 | RX_LAST | Numer ostatniej poprawnej ramki partnera; FF przed pierwszym odbiorem |
| 6 | SRC_EP | Endpoint źródłowy, 0 = sterowanie |
| 7 | DST_EP | Endpoint docelowy, 0 = sterowanie |
| 8..31 | BODY | LEN znaczących bajtów, reszta wyzerowana |

CTRL = 0x40 | (KIND << 5) | LEN. Pozostałe wersje są nieobsługiwane. LEN>24 jest błędem. Niezerowy padding jest błędem. Liczby wielobajtowe w BODY są little-endian.

SRC oznacza **klasę funkcjonalną**, nie unikalny egzemplarz. Powtarzające się klasy są dozwolone w przyszłej sieci; v1 M1 **nie** obsługuje ich enumeracji ani jednoczesnych odpowiedzi. W M1: po starcie ustabilizować ADC0, uśrednić próbki, obliczyć adc8=adc10>>2, zatrzasnąć klasę: adc8<128 → 0; adc8>=128 → 1. Odniesienie ADC musi być jawne. Klasa nie zmienia się do restartu.

## 2. Typy i rozszerzenia

DATA KIND=0: dla LEN>0 wymagane SRC_EP,DST_EP !=0. DATA LEN=0, EP=0/0 jest ramką heartbeat, dozwoloną w każdej aktywnej sesji. Inne kombinacje EP=0 w DATA są błędne. Profil endpointu definiuje format BODY. Endpoint 1/1 = UART RELIABLE; pozostałe wymagają negocjacji, w M1 są nieznane.

CONTROL KIND=1: wymagane EP=0/0 i LEN>=1; BODY[0] = podtyp. Nieznany podtyp: odrzucić semantykę, zwiększyć licznik unknown_control, nie zmieniać transportu. EXT 0x7F: BODY[1] namespace, BODY[2] subtype, BODY[3..] dane. Nieznane rozszerzenia ignorować. W HELLO wymieniać maskę capabilities; nieznana funkcja nie zmienia znaczenia nagłówka.

Podtypy CONTROL (wartości hex; LEN obejmuje podtyp):
- 01 HELLO, LEN=7: nonce32 LE, caps16 LE.
- 02 HELLO_REPLY, LEN=11: local_nonce32, peer_nonce32, caps16.
- 03 READY, LEN=10: local_nonce32, peer_nonce32, SID.
- 04 READY_ACK, LEN=2: SID.
- 10 TRANSPORT_ACK, LEN=5: DATA_SEQ, STATUS, RX_FREE16 LE. STATUS 0 ACCEPTED, 1 BUSY, 2 REJECTED.
- 11 LINK_REPORT, LEN=10: BASE_SEQ, RX_BITMAP32 LE, HW_RETRY16 LE, MAX_RT16 LE (1 B podtypu + 1+4+2+2 B danych).
- 12 DATA_ACK, LEN=7..24: ACK_DATA_SEQ (1 B), ACK_STATUS (1 B), RX_FREE16 LE (2 B), DATA_SEQ (1 B), DATA (1..18 B). Ramka CONTROL z EP=0/0 łączy aplikacyjny ACK z nowym segmentem UART endpointu 1. Jest obsługiwana wyłącznie po negocjacji capability DATA_ACK; nieznający jej odbiornik ignoruje cały podtyp, a nie przyjmuje danych częściowo. Bit i bitmapy odpowiada (BASE_SEQ+i) mod 256; 1 = odebrano co najmniej raz.
- 7F EXT, LEN=3..24: namespace, subtype, dane.

Nieznany endpoint lub podtyp nigdy nie kieruje danych do UART. Nieznana wersja nie jest interpretowana jako v1.

## 3. Sesja i identyfikatory

HELLO/HELLO_REPLY: SID=0, TX_SEQ=0, RX_LAST=FF; HELLO może być broadcast. W M1 węzeł o niższej klasie inicjuje READY; wyższa klasa odpowiada READY_ACK. Oba zaczynają w RX i okresowo emitują HELLO z backoffem. Inicjator wysyła READY z oboma nonce i SID; responder weryfikuje nonce, odsyła READY_ACK i potrafi powtórzyć ACK dla duplikatu READY. Inicjator przechodzi do ACTIVE po READY_ACK; pierwsza aktywna ramka należy do inicjatora. Responder musi mieć timeout i resynchronizację, jeśli nie zobaczy aktywnej ramki.

SID = CRC-8/ATM (poly 07, init 00, bez odbicia, xorout 00) z bajtów: min(SRC), max(SRC), nonce_initiator LE32, nonce_responder LE32; wynik 00 mapować na 01. Wektor CRC-8/ATM: ASCII "123456789" → F4. Nonce MUSI być odświeżany po restarcie; źródło losowości AVR wymaga osobnego projektu i weryfikacji. SID 8-bit **nie zapewnia bezkolizyjnej identyfikacji sesji** ani ochrony przed atakami; po resecie transport nie ma gwarancji exactly-once.

Po wejściu ACTIVE obie strony zaczynają TX_SEQ=0, RX_LAST=FF. Przy zmianie nonce partnera, timeoutach lub utracie synchronizacji: zatrzymać transport, jawnie zgłosić niepotwierdzone dane i przeprowadzić discovery ponownie.

## 4. Telemetria i retransmisje

TX_SEQ zwiększać przy każdej **nowej ramce** ACTIVE, także heartbeat, CONTROL i retransmisji danych transportowych. Automatyczne ponowienia sprzętowe nRF24 nie zwiększają TX_SEQ. Ponowienie identycznej ramki przez MAC zachowuje numer; nowa próba transportu używa nowego TX_SEQ, ale starego DATA_SEQ.

RX_LAST jest **ostatnią zaobserwowaną** ramką, nie kumulatywnym ACK i nie potwierdzeniem przyjęcia danych. FF przed pierwszym odbiorem jest stanem inicjalnym; po zawinięciu 255 może być normalnym numerem. Luki w TX_SEQ są obserwacjami brakujących ramek, a nie pewnym dowodem wykonanej transmisji; późne ramki mogą je uzupełnić. W oknie 8-bitowym odległość >127 bez synchronizacji jest niejednoznaczna. Duplikat ramki nie może ponownie uruchamiać efektów aplikacyjnych.

Oddzielne liczniki: scheduled_slots, tx_attempts, tx_ds, max_rt, arc_retries, rx_valid, duplicate_rf, seq_gaps_observed, seq_gaps_resolved, transport_retries, uart_overflow. MAX_RT może oznaczać zgubione ACK, niekoniecznie zgubiony payload. Hardware ACK nie potwierdza przyjęcia danych do UART. Raport LINK_REPORT jest obserwacją odbiorcy; HW_RETRY i MAX_RT są obserwacją nadajnika.

M1: po synchronizacji naprzemienne sloty TX/RX, nawet bez danych; heartbeat DATA LEN=0 EP=0/0. Timeout slotu nie może zatrzymać zegara; recovery i backoff obowiązkowe. Broadcast i komunikacja grupowa bez auto-ACK, bez jednoczesnych odpowiedzi.

## 5. UART RELIABLE — stop-and-wait

DATA EP 1→1: LEN=1..24; BODY[0] DATA_SEQ (mod 256), BODY[1..LEN-1] 0..23 bajtów UART. Implementacja pierwszego mostu ogranicza aktualny segment do **18 B**, żeby zawsze móc łączyć retransmisję danych z ACK w ramce DATA_ACK i zapewnić postęp obu kierunkom. Jeden niepotwierdzony segment na kierunek i sesję. Nadawca przechowuje segment do TRANSPORT_ACK ACCEPTED. Timeout powoduje ponowienie **DATA_SEQ bez zmiany**, z nowym TX_SEQ. Limity timeout/retry są parametrami konfiguracji, nie polami ramki.

Odbiorca akceptuje tylko oczekiwany DATA_SEQ i wyłącznie jeśli **cały** segment mieści się w TX ring UART. Atomowo dopisuje bajty, przesuwa oczekiwany DATA_SEQ i nadaje ACK ACCEPTED. Brak miejsca → BUSY, bez częściowego zapisu. Duplikat ostatnio przyjętego segmentu → ponowne ACCEPTED bez ponownego dopisywania. Inny numer → REJECTED. Po wyczerpaniu retry → jawny DELIVERY_FAILED; restart sesji unieważnia oczekujące potwierdzenia. Potwierdzenie oznacza przyjęcie do kolejki UART, nie fizyczne wysłanie z UART.

## 6. Parser — kolejność

1. Wymagaj kompletnego 32 B payloadu zweryfikowanego przez radio sprzętowym CRC16.
2. Sprawdź VERSION=01, LEN<=24, zerowy padding.
3. Sprawdź SRC/DST (FF tylko w dozwolonych kontrolnych/broadcastowych typach), poprawność KIND, EP i długości podtypu.
4. Sprawdź stan sesji i SID; discovery akceptuj tylko w dozwolonych stanach.
5. Dopiero teraz aktualizuj statystyki odebranych poprawnych ramek i okno TX_SEQ; nieznane rozszerzenie może liczyć się jako poprawna ramka RF, ale bez efektów aplikacyjnych.
6. Przekaż BODY do właściwego endpointu. Nigdy nie wykonuj częściowego zapisu do UART ani odczytu poza LEN.

## 7. Wektory 32 B (hex, 16 bajtów na wiersz)

T1 HELLO, SRC 0, DST FF, nonce 12345678, caps 0001:
```text
67 00 FF 00 00 FF 00 00 01 78 56 34 12 01 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

T2 DATA testowe, EP 2/2 (nieznany w M1), SID 2A, SEQ 10, RX_LAST 05, "Hello!\r":
```text
47 00 01 2A 10 05 02 02 48 65 6C 6C 6F 21 0D 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

T3 HEARTBEAT SRC 1→0, SID 2A, SEQ 06, RX_LAST 10:
```text
40 01 00 2A 06 10 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

T4 RELIABLE SRC 0→1, SID 2A, TX_SEQ 11, RX_LAST 06, DATA_SEQ 00, "ABCDE":
```text
46 00 01 2A 11 06 01 01 00 41 42 43 44 45 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

T5 TRANSPORT_ACK SRC 1→0, SID 2A, SEQ 07, RX_LAST 11, DATA_SEQ 00, ACCEPTED, RX_FREE 120:
```text
65 01 00 2A 07 11 00 00 10 00 00 78 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

Testy negatywne: wersja 2, LEN 25..31, niezerowy padding, DATA SID=0, DATA EP=0 przy LEN>0, CONTROL EP!=0, nieznany subtype, nieznany endpoint, ACK o nieznanym STATUS, powtórzony DATA_SEQ, BUSY i pełny UART, restart przed ACK, wrap 255→0, utrata READY_ACK, kolizja slotów, brak partnera.

## 8. Warunki odbioru i ograniczenia

Koder/parser ma działać na jawnej tablicy 32 B bez dynamicznej alokacji i bez zależności od AVR; nie używać rzutowania packed struct. Testować round-trip i wszystkie wektory na hoście, następnie kompilację AVR, SRAM i dwa fizyczne radia. Parametry czasowe slotów, retry, nonce i okien telemetrii ustalić eksperymentalnie. Brak w v1: routing, enumeracja jednakowych klas, szyfrowanie, autoryzacja, gwarancja dostarczenia po wyczerpaniu retry oraz exactly-once przez restart.

## 9. Pierwsza implementacja UART↔RF (2026-11-03)

- Jeden firmware na obu RF-Nano. A0/GND daje klasę 0 (inicjator cyklu); A0/dodatnie zasilanie daje klasę 1 (odpowiadający). MAC w tym etapie steruje kolejnością A→B→A na dwóch węzłach, a nie arbitrażem wielowęzłowym.
- Sesja jest uzgadniana przez HELLO, HELLO_REPLY, READY i READY_ACK. Maski możliwości `0x0003` oznaczają wsparcie RELIABLE i DATA_ACK; węzeł nieznający obu bitów nie jest przyjmowany do tej sesji.
- `nonce32` używa licznika uruchomień z EEPROM (16-gniazdowy dziennik z markerem commit i CRC8; jeden zapis przy starcie, nie przy każdym radio retry). Przy nowym uzgadnianiu bez restartu inicjator zwiększa nonce w SRAM. Jest to rozróżnienie sesji, nie poufność ani uwierzytelnienie. **SID ma tylko 8 bitów**, więc wciąż może kolidować po restarcie.
- Osobna numeracja `TX_SEQ` (każda nowa ramka ACTIVE) i `DATA_SEQ` (segment UART stop-and-wait). Od strony odbioru atomowy zapis do kolejki TX UART lub BUSY. ACK aplikacyjny oznacza **przyjęcie do tej kolejki**, nie transmisję bajtu na przewód USB. Po utracie ACK ponawiany jest ten sam DATA_SEQ. Duplikat nie jest ponownie zapisywany do TX UART.
- W trybie jednoczesnego ruchu DATA_ACK umożliwia ACK i segment drugiego kierunku w jednej ramce. 18 B/segment jest świadomą decyzją implementacyjną; przyszła implementacja może negocjować większy rozmiar samodzielnego DATA, bez zmiany formatu bazowego.
- 64 próby transportowe bez ACCEPTED lub REJECTED wywołuje renegocjację sesji przy zachowaniu oczekującej porcji RAM; nie jest to obietnica dostarczenia po resecie. Pełny bufor UART nie powoduje przyjęcia części segmentu.
- **Ograniczenie gwarancji**: nie ma exactly-once przez reset/power loss, trwałego dziennika dostarczonych bajtów, sprzętowej kontroli przepływu UART ani gwarancji przy przesyłaniu danych do RX UART szybciej niż przepustowość RF. UART RX może się przepełnić; licznik overflow jest dostępny przez `UART::counters()`. Zdarzenia TX_DS/MAX_RT i podstawowe liczniki są w `LinkCounters` oraz `ReliableUartV1::stats()`, niewypisywane do przezroczystego strumienia danych.
- Hostowe testy warstw sesji i transportu, w tym duplikaty, BUSY i dwukierunkowy strumień binarny, są w `tests/`. Warstwa ACK ma oddzielne testy od kodera ramki. Konfiguracja fizyczna SPI/nRF pozostaje bez zmian.
