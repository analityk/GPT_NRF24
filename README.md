# GPT_NRF24 — dwuwęzłowy rdzeń 8+24 B z ACK Payload

Projekt C++11 dla dwóch identycznych **RF-Nano (ATmega328P 16 MHz + nRF24L01+)**, kompilowany w **Microchip Studio / Atmel Studio**. W bieżącym drzewie jest **jeden aktywny firmware** i jeden format ramki. To świadomy punkt wyjścia do dalszych prac nad protokołem, nie gotowy transfer plików ani UART↔UART.

## Aktualny stan sprzętowy

- **Działa fizycznie na dwóch RF-Nano:** 8 B metadanych + 24 B danych binarnych, dwa kierunki w jednym cyklu DATA A→B / sprzętowy ACK Payload B→A.
- **Pomiar z 2026-11-03:** stabilne około **625 potwierdzonych wymian na sekundę**, brak widocznych błędów w pokazanych oknach RealTerm. Pełne BODY testowe odpowiada **15 000 B/s na kierunek** (30 000 B/s łącznie). Średni odstęp wymian wynosi około 1,6 ms — **nie jest to osobny pomiar latencji pojedynczej wiadomości**.
- Rola A = **PTX**, B = **PRX**. Fizycznie radio jest półdupleksowe; logiczne dwa kierunki realizuje sprzętowy ACK Payload. B wysyła odpowiedź wcześniej przygotowaną w FIFO.
- **Jeszcze nie działa:** automatyczna negocjacja sesji i jej odzyskiwanie po restarcie, UART↔UART w tym nowym profilu, prawdziwy bufor odbiorcy dla `RX_CREDIT`, usługowy transport plików, szyfrowanie. `SESSION_ID=0x4E52` to wyłącznie wartość testowa.

## Budowanie i sprawdzenie na płytkach

1. Otwórz `GPT_NRF2401.atsln` w Microchip Studio.
2. Zbuduj **F7**, zaprogramuj ręcznie **F12**. **Nie włączać automatycznego programowania Post-Build**.
3. Wgraj **ten sam HEX** na obie płytki. A0 do GND → A/PTX; A0 do dodatniego zasilania własnej płytki → B/PRX.
4. RealTerm na dwóch portach USB/CH340, **115200, 8N1**. Port transmituje tutaj tylko diagnostykę; wpisywanie tekstu **nie wyśle** go do partnera.
5. W stabilnym stanie powinieneś zobaczyć raporty:
   - `A TX/s=625 ACK/s=625 ARC=0 MAX=0 BAD=0 IO=0 ...`
   - `B RX/s=625 Q/s=625 ARC=0 MAX=0 BAD=0 IO=0 ...`

Liczba 625 jest wcześniej **zmierzonym punktem odniesienia**, nie gwarancją dla każdych warunków. Dane wysyłane na radiu to obecnie deterministyczny, 24-bajtowy wzorzec testowy sprawdzany przez aplikację diagnostyczną; sam rdzeń traktuje BODY jako nieprzezroczysty ciąg bajtów.

## Format radia

Sprzętowe DPL zapewnia długość pakietu **8–32 B**; nie wysyłamy redundantnego pola LENGTH. Format `paired_frame.h`:

| Bajt | Pole |
|---|---|
| 0 | `TX_SEQ` — kolejna ramka lokalna (mod 256) |
| 1 | `RX_LAST` — ostatnia widziana ramka drugiej strony |
| 2 | `LOSS_AB / LOSS_BA` — dwa czterobitowe liczniki luk |
| 3 | `ARC_LAST / RX_DROP` — czterobitowe metryki |
| 4 | `RX_CREDIT` — liczba możliwych nowych segmentów 24 B |
| 5 | `TYPE_FLAGS` — DATA / IDLE / CONTROL + flagi |
| 6–7 | `SESSION_ID` — 16-bitowy, little-endian |
| 8–31 | `BODY` — **0–24 bajty, bez interpretacji w rdzeniu** |

Szczegółowe reguły (liczniki modulo, opóźnienie ACK Payload, znaczenie kredytów i zakres gwarancji) znajdują się w [paired-core-v2.md](paired-core-v2.md). **Sprzętowy ACK nie oznacza przyjęcia danych przez aplikację.**

## Struktura aktywnego kodu

| Warstwa | Pliki | Odpowiedzialność |
|---|---|---|
| Aplikacja diagnostyczna | `GPT_NRF2401/main.cpp` | Jeden program A/B, binarny wzorzec BODY, liczniki po UART |
| Kodek | `paired_frame.h` | Jawne bajtowe kodowanie/dekodowanie 8+24 |
| Stan łącza | `paired_link.h` | TX/RX, luki, kredyty, sesja, duplikaty |
| Radio/PHY | `nrf24.h`, `nrf24_ack_phy.h` | Rejestry, SPI, DPL, FIFO, DATA↔ACK Payload |
| Sprzęt platformy | `spi.*`, `gpio.*`, `adc0.*`, `timer1_clock.*`, `uart.*`, `byte_ring.h`, `avr_delay.h` | Peryferia i diagnostyka, bez zależności protokołu od AVR |

Brak `malloc/new`, kontenery statyczne, brak globalnego `-fpack-struct`.

## Testy

- `sh tests/run_host_tests.sh` — automatycznie znajduje i uruchamia aktywne testy C++11.
- `sh tests/build_avr.sh` — kompiluje **jeden aktualny firmware** kompilatorem `avr-g++`, bez programowania płytki.
- [GitHub Actions](https://github.com/analityk/GPT_NRF24/actions) uruchamia oba zestawy niezależnie.

## Dawne warianty i historia

Starszy most UART, odrębny benchmark 32+32 B, stary format v1 oraz związane z nimi źródła i testy zostały **wycofane tylko z bieżącej gałęzi**, aby nie utrzymywać kilku równoległych aplikacji. Zachowano je w gałęzi [`archive/pre-paired-core-cleanup-2026-11-03`](https://github.com/analityk/GPT_NRF24/tree/archive/pre-paired-core-cleanup-2026-11-03), wskazującej commit [`d91630509ba3`](https://github.com/analityk/GPT_NRF24/commit/d91630509ba32fb62ab0437ff2d2b5770feae31c). Pełne wyniki eksperymentów pozostają w `history.md`. Odtwarzanie starszych testów wymaga świadomego przejścia do archiwalnej wersji, a nie przełączania makra w bieżącym firmware.
