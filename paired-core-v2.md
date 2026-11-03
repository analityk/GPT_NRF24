# Paired Core — format 8+24 B i sprzętowe ACK Payload

**Status 2026-11-03:** główny, jedyny format aktywnego firmware. Przetestowany na dwóch fizycznych RF-Nano: około 625 wymian/s. Pełny format jest częścią aktualnego `main`, a dawny `protocol-v1.md` dostępny jest tylko w gałęzi archiwalnej.

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
- `SESSION_ID` jest sprawdzany przed modyfikacją diagnostyki. **W obecnym programie jest testową stałą `0x4E52`**, a nie wynikiem negocjacji; nie daje odporności na restarty, uwierzytelnienia ani gwarantowanej unikalności.
- `RX_CREDIT=1` w obecnym generowanym wzorcu jest **symulacją miejsca w odbiorniku**. Nie używać tej wartości jako dowodu gotowości prawdziwego sinku.

## Źródła i weryfikacja

- `paired_frame.h`: niezależny od MCU koder/dekoder 8 B + BODY.
- `paired_link.h`: stan kierunków, sekwencje, luki, RX_CREDIT, deduplikacja, sesja.
- `nrf24.h`, `nrf24_ack_phy.h`: rejestry DPL, FIFO, SPI i sprzętowy DATA/ACK Payload.
- `main.cpp`: pojedyncza diagnostyczna aplikacja fizyczna, A/PTX i B/PRX z tego samego HEX.
- `tests/*_test.cpp`, `tests/run_host_tests.sh`, `tests/build_avr.sh`: testy hostowe i build AVR, **bez programowania płytki**.

**Zmierzony wynik:** A TX/s=ACK/s około 625, B RX/s=Q/s około 625 (czasami 626 w niezależnych oknach); zgłoszone ARC, MAX, BAD, IO, GAB, GBA, DROP, FULL = 0 w widocznym teście. 24 B × 625/s = **15 000 B/s wzorca testowego na kierunek**, 30 000 B/s łącznie; to nie pomiar transferu UART. Średnia repetycja 1,6 ms nie jest zmierzoną latencją pojedynczej ramki.

**Test sprzętowy:** ten sam firmware, A0 GND na A, A0 plus na B; F7 build i F12 manualne programowanie; RealTerm 115200 8N1 do odczytu diagnostyki. Docelowy kod transportu nie jest jeszcze częścią tego rdzenia.

Referencja radiowa: Nordic nRF24L01+ Product Specification v1.0 — [plik PDF produktu](https://cdn.sparkfun.com/assets/3/d/8/5/1/nRF24L01P_Product_Specification_1_0.pdf).

## Archiwalne eksperymenty

Poprzedni most UART (stary format v1), benchmark 32+32 B, koncepcje nagłówka 4 B i eksperymentalny wybór aplikacji `0/1/2` pozostają w historii Git i w [gałęzi archiwalnej](https://github.com/analityk/GPT_NRF24/tree/archive/pre-paired-core-cleanup-2026-11-03). Nie należą do bieżącego builda.
