# Paired Core — format 8+24 B i sprzętowe ACK Payload

**Status 2026-11-04:** główny, jedyny format aktywnego firmware. Format DATA+ACK Payload przetestowany na dwóch fizycznych RF-Nano (około 625 wymian/s w poprzednim firmware); nowy handshake i samoczynna rekonwergencja po resetach mają zaliczone testy hostowe i AVR build, lecz **wymagają osobnego testu fizycznego**. Pełny format jest częścią aktualnego `main`, a dawny `protocol-v1.md` dostępny jest tylko w gałęzi archiwalnej.

## Warstwa fizyczna i podział odpowiedzialności

A (A0→GND) jest stale PTX; B (A0→plus) stale PRX. A wysyła DATA, B w tej samej operacji Enhanced ShockBurst odsyła wcześniej przygotowaną wiadomość przez **sprzętowy ACK Payload**. Radio jest półdupleksowe. PTX wyzwala każdą transakcję; odpowiedź PRX jest opóźniona względem świeżo odebranej ramki. SPI 8 MHz, radio 1 Mb/s, adres 5 B, sprzętowe CRC16, retry `SETUP_RETR=0x23`. `NrfAckPayloadPhy` używa minimalnego odstępu sprzętowego, flag `TX_DS/MAX_RT`, FIFO i timeoutu, a nie sztucznego slotu 20–25 ms.

Sprzętowa DPL wyznacza rozmiar payloadu **8..32 B**. `BODY_LENGTH = DPL_WIDTH - 8`, czyli 0..24 B. Rozmiaru nie powtarzamy w nagłówku; 0-bajtowy BODY w ramce IDLE oznacza tylko 8 bajtów payloadu radiowego. Rdzeń nie interpretuje BODY; testowa aplikacja wypełnia je wzorcem, ale nie jest to format aplikacyjny.

## Układ bajtów

| Offset | Pole | Semantyka |
|---|---|---|
| 0 | `TX_SEQ` | 8-bitowy numer kolejnej **nowej** ramki; moduł 256, nie zmienia się podczas retransmisji wykonanej wewnętrznie przez radio |
| 1 | `RX_LAST` | Ostatni poprawny numer ramki partnera widziany przez rdzeń radiowy; `0xFF` przed pierwszym odbiorem. **Nie jest aplikacyjnym ACK** |
| 2 | `LOSS` | b3..0: braki A→B, licznik właściciela B, modulo16; b7..4: braki B→A, licznik właściciela A, modulo16. Druga strona echuje ostatnią widzianą wartość |
| 3 | `QUALITY` | b3..0 `ARC_LAST`: ostatni zmierzony sprzętowy `ARC_CNT` PTX; b7..4 `RX_DROP`: lokalne odrzucenia modulo16 |
| 4 | `RX_CREDIT` | Liczba dostępnych miejsc na kolejne pełne segmenty 24 B u odbiorcy, 0 oznacza wstrzymanie nowych danych, nie ramek kontrolnych |
| 5 | `TYPE_FLAGS` | b1..0: `TYPE`; b2 `URGENT`, b3 `PAUSE`, b4 `RESYNC`, b5 `WANT_PTX`, b6 `OVERFLOW`, b7 `REPORT` |
| 6–7 | `SESSION_ID` | 16 bitów little-endian identyfikatora aktywnej sesji |
| 8..31 | `BODY` | 0–24 bajty użytkownika lub informacji CONTROL, przezroczyste dla warstw PHY i `PairedFrame` |

Typy: `DATA=0`, `IDLE=1` (BODY=0), `CONTROL=2` (BODY ≥1), `EXT=3` (zarezerwowany). Każda ramka jest kodowana bajt po bajcie; żadnych struktur `packed` i żadnego odwołania do AVR ABI.

## Reguły interpretacji i bezpieczeństwo

- `RX_LAST` dotyczy poprawnie dekodowanej ramki radiowej; jego wartość nie dowodzi jeszcze przyjęcia do kolejki UART lub zapisania pliku.
- Braki nie są `BER`. `LOSS` pokazuje luki w kolejności nowych ramek; `ARC_LAST` pokazuje retransmisje sprzętowe nawet przy zerowych lukach. `MAX_RT` należy monitorować osobno. Brak nowych ramek sam w sobie nie podaje liczby błędnych bitów.
- `TX_SEQ` i `RX_LAST` zawijają się modulo256. W `PairedLink` różnica 0 to duplikat, 1..127 — nowa ramka, 128..255 — stara/niejednoznaczna. Po dużej przerwie lub resecie wymagana jest nowa sesja, nie interpretacja starej epoki jako ciąg dalszy.
- Czterobitowe LOSS i DROP zawijają się modulo16, więc **nie są pełnymi, długoterminowymi licznikami błędów**. Lokalny stan utrzymuje szersze liczniki diagnostyczne.
- Metadane ACK Payload są przygotowywane **przed** odebraniem następnej ramki. `RX_LAST` i `RX_CREDIT` raportowane przez B są więc opóźnione. Program wyższej warstwy musi uwzględniać już wysłane segmenty w locie. Duplikaty i stare ramki nie mogą cofnąć peer credits.
- `SESSION_ID` jest sprawdzany przed modyfikacją diagnostyki. **Obecnie jest negocjowany** przez kontrolne ramki z identyfikatorami obu uruchomień, zamiast stałej `0x4E52`. Wartość SID16 ma tylko 16 bitów: nie jest tajna, nie daje uwierzytelnienia ani matematycznie gwarantowanej unikalności.
- `RX_CREDIT=1` w obecnym generowanym wzorcu jest **symulacją miejsca w odbiorniku**. Nie używać tej wartości jako dowodu gotowości prawdziwego sinku.


## Handshake P2P — nowy etap (implementacja 2026-11-04)

Radio pozostaje w stałych rolach: **A=PTX inicjuje każdą transakcję**, **B=PRX odpowiada przygotowanym ACK Payload**. CONTROL jest przenoszony w tym samym formacie `PairedFrame` co DATA, ale ma `SESSION_ID=0`, `TYPE=CONTROL` i `FLAGS.RESYNC=1`. Dane BODY protokołu aplikacyjnego nadal są nieinterpretowane.

Każda strona przy starcie wywołuje `BootEpoch::advance()` i uzyskuje persistent **32-bitowy numer uruchomienia**. Numer próby (16 bitów) rośnie tylko przy kolejnych restartach negocjacji podczas *tego samego* uruchomienia. Gdy licznik próby zbliży się do zawinięcia, adapter odnawia epokę w EEPROM; nie wolno ponownie wykorzystać wcześniej użytego identyfikatora.

Tożsamość każdego peer: `[boot_epoch:32 LE, attempt:16 LE]`, łącznie 6 B. Opakowanie CONTROL:

| Op | Nadawca i stan | BODY | DPL |
|---|---|---|---:|
| 1 `HELLO` | A / DISCOVER | op (1) + ID_A (6) | 15 B |
| 2 `BEACON` | B / DISCOVER | op (1) + ID_B (6) | 15 B |
| 3 `WELCOME` | B / WAIT_READY | op (1) + ID_B (6) + echo ID_A (6) | 21 B |
| 4 `READY` | A / WAIT_READY_ACK | op (1) + ID_A (6) + ID_B (6) + SID16 (2) | 23 B |
| 5 `READY_ACK` | B / WAIT_CONFIRM | op (1) + ID_B (6) + ID_A (6) + SID16 (2) | 23 B |

SID16 jest **CRC-16/CCITT-FALSE** z 14 bajtów: `0x50,0x32,ID_A[6],ID_B[6]`, z mapowaniem wyniku 0 na 1. Pełne 12 bajtów tożsamości są porównywane podczas handshake, natomiast 16-bitowe SID służy wyłącznie jako tani filtr starych ramek DATA po jego ukończeniu.

**Wymuszona kolejność bez synchronizacji timerów MCU:** A wielokrotnie wyzwala HELLO, B odsyła wcześniej zaprogramowany BEACON albo WELCOME; A po WELCOME wysyła READY, B przechodzi do WAIT_CONFIRM i przygotowuje READY_ACK; A po odebranym READY_ACK zaczyna DATA/IDLE z nowym SID; **B uznaje sesję za ACTIVE dopiero po pierwszej poprawnej DATA/IDLE z tym SID.** Powtórzone HELLO/READY lub opóźniony stary ACK są ignorowane bądź obsługiwane idempotentnie.

Podczas zmiany kontrolnej odpowiedzi lub sesji B wykonuje przez `replacePrxReply` krótki guard **550 µs** w standby, czyści TX/RX FIFO oraz przygotowuje ACK Payload na nową epokę; po ponownym CE HIGH czeka **150 µs** na ustalenie PRX. To ochrona przed pozostawieniem starego payloadu w sprzętowej kolejce, nie stała przerwa między DATA. A przy rekonwergencji czyści TX/RX FIFO bez ponownego usypiania radia.

### Timeouty (obecne parametry eksperymentalne)

| Stan | Węzeł | Warunek inicjacji ponownego uzgodnienia |
|---|---|---|
| ACTIVE | A | Brak poprawnego pakietu sesji od B przez 250 ms |
| WAIT_READY_ACK | A | Brak postępu negocjacji przez 1000 ms |
| ACTIVE | B | Brak poprawnej ramki sesji A przez 750 ms |
| WAIT_READY / WAIT_CONFIRM | B | Brak postępu negocjacji przez 2000 ms |
| DISCOVER | Oba | Pozostają w discovery bez limitu; nie zużywają ciągle numerów prób podczas wielogodzinnego braku partnera |

Utrata zasięgu nie jest tożsama z utratą aplikacyjnych danych. Po ponownej synchronizacji strumień testowego BODY może ruszyć na nowej sesji, lecz **nie ma jeszcze gwarancji zachowania niewysłanego bufora użytkownika** — taka polityka należy do usług ponad rdzeniem.

`ST` w raporcie UART mapuje `PairedSession::Phase`: `0=DISCOVER`, `1=WAIT_READY` (B), `2=WAIT_READY_ACK` (A), `3=WAIT_CONFIRM` (B), `4=ACTIVE`. `SID` w logu jest dziesiętny, zero poza ACTIVE; `R` to licznik lokalnych restartów sesji, nie BER/PER. Dla zdrowej pary, po obu stronach, zobaczysz **`ST=4` i jednakowy niezerowy SID**. Na B przy pierwszym pakiecie po resecie mogą wystąpić `BAD/MAX`, które są przejściowymi informacjami, nie dowodem trwałej awarii.

### Trwałość i ograniczenia

`EpochJournal` przechowuje **16 rekordów po 6 bajtów EEPROM**: little-endian 32-bit boot epoch, CRC8 i `commit=0xA5` wpisywane na końcu. Zapis urwany przez reset nie nadpisuje ostatniego zatwierdzonego rekordu; każda próba restartu w RAM nie powoduje zapisu EEPROM. W obecnej płytce ma to ograniczyć zużycie EEPROM i ryzyko przypadkowego powtórzenia sesji.

SID16 nie zapewnia uwierzytelnienia ani kryptograficznej świeżości. Po dowolnie wielu różnych negocjacjach możliwa jest kolizja 16 bitów, a ponowny pełny reflash z czyszczeniem EEPROM resetuje licznik epoki. Mechanizm chroni przed **przypadkowymi starymi odpowiedziami podczas typowych restartów**, nie zastępuje zabezpieczeń przed atakami czy kontraktu exactly-once.

## Źródła i weryfikacja

- `paired_frame.h`: niezależny od MCU koder/dekoder 8 B + BODY.
- `paired_link.h`: stan kierunków, sekwencje, luki, RX_CREDIT, deduplikacja, sesja.
- `nrf24.h`, `nrf24_ack_phy.h`: rejestry DPL, FIFO, SPI i sprzętowy DATA/ACK Payload.
- `main.cpp`: pojedyncza diagnostyczna aplikacja fizyczna, A/PTX i B/PRX z tego samego HEX.
- `paired_session.h`: niezależna od AVR maszyna stanów sesji, kontrolne wiadomości i pochodny SID16.
- `epoch_journal.h` + `boot_epoch.cpp`: przenośny algorytm dziennika 16 rekordów i AVR EEPROM z CRC8 oraz znacznikiem commit.
- `tests/*_test.cpp`, `tests/run_host_tests.sh`, `tests/build_avr.sh`: testy hostowe i build AVR, **bez programowania płytki**.

**Zmierzony wynik:** A TX/s=ACK/s około 625, B RX/s=Q/s około 625 (czasami 626 w niezależnych oknach); zgłoszone ARC, MAX, BAD, IO, GAB, GBA, DROP, FULL = 0 w widocznym teście. 24 B × 625/s = **15 000 B/s wzorca testowego na kierunek**, 30 000 B/s łącznie; to nie pomiar transferu UART. Średnia repetycja 1,6 ms nie jest zmierzoną latencją pojedynczej ramki.

**Nowy handshake:** testy hostowe parowania, restartu A/B, opóźnionego BEACON, niepoprawnych nagłówków i urwanego zapisu EEPROM oraz kompilacja AVR-GCC przeszły. **Test fizyczny nowego handshake'u nie został wykonany.**

**Test sprzętowy:** ten sam firmware, A0 GND na A, A0 plus na B; F7 build i F12 manualne programowanie; RealTerm 115200 8N1 do odczytu diagnostyki. Docelowy kod transportu nie jest jeszcze częścią tego rdzenia.

Referencja radiowa: Nordic nRF24L01+ Product Specification v1.0 — [plik PDF produktu](https://cdn.sparkfun.com/assets/3/d/8/5/1/nRF24L01P_Product_Specification_1_0.pdf).

## Archiwalne eksperymenty

Poprzedni most UART (stary format v1), benchmark 32+32 B, koncepcje nagłówka 4 B i eksperymentalny wybór aplikacji `0/1/2` pozostają w historii Git i w [gałęzi archiwalnej](https://github.com/analityk/GPT_NRF24/tree/archive/pre-paired-core-cleanup-2026-11-03). Nie należą do bieżącego builda.

## Fizyczny odbiór negocjacji sesji i resetów — 2026-11-04

Zrzuty użytkownika potwierdzają działanie z handshake'em na obu RF-Nano i powrót do `ST=4` i zgodnego SID po **pojedynczym RESET B** (`SID=12715`) oraz **pojedynczym RESET A** (`SID=16598`). Po resecie B A raportuje `R=1`, nowo uruchomione B `R=0`; odwrotnie przy resecie A. Podczas nieobecności B A mierzy `MAX=110 ARC=330` lub `MAX=219 ARC=657` w 1-sekundowych oknach: wykorzystano wszystkie 3 sprzętowe retransmisje zgodnie z `SETUP_RETR=0x23`. Po renegocjacji tempo wynosi około **612–613 pełnych wymian/s** i wszystkie widoczne liczniki diagnostyczne w stanie ustalonym są zerowe.

W porównaniu z poprzednim testem 625/s bez handshake'u jest to około **2%** niższy wynik: 1,600 ms → 1,631–1,634 ms średniego odstępu. Nie zmieniono prędkości RF ani długości pełnych ramek; można podejrzewać dodatkową obsługę procesora, ale wymaga to osobnego profilowania, podobnie jak bezpośrednia latencja. Podczas resetu występują realne retransmisje i timeouty oraz czas negocjacji, więc nie sprowadzać spadku do samego MCU. **Jednoczesny reset obu płytek, długotrwałe zaniki zasięgu i kontrola przepływu nie są jeszcze fizycznie potwierdzone.**

## Oba urządzenia zrestartowane jednocześnie (2026-11-04)

Na kolejnych zrzutach użytkownik potwierdził **ponowny rozruch obu ATmeg** (dwa bannery), przejście z `SID=16598` na nowy wspólny `SID=13618`, `ST=4 R=0` na obu oraz stabilny ruch około 612–613/s. Tym samym testy sprzętowe obejmują reset A, reset B i reset obu. Zmienny identyfikator sesji jest zamierzony: 16-bitowy SID jest wyprowadzany z trwałych epok startowych i liczników prób obu urządzeń. Ewentualne stałe ID pary lub urządzenia przechowywane w EEPROM będzie **osobnym polem tożsamości** służącym późniejszemu parowaniu, nie zastąpi sesyjnego identyfikatora potrzebnego do odrzucania zaległych ACK Payload.
