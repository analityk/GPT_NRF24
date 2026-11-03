# TODO — GPT_NRF24

**Etap: INITIAL**  
**Aktualny kamień milowy: M1 — dwukierunkowa komunikacja radiowa**

## Zadania

- [x] Spisać projekt formatu binarnego v1, parsera, telemetrii, retransmisji i wektorów testowych (`protocol-v1.md`); bez implementacji i testów sprzętowych.

- [ ] Ustalić architekturę komunikacji dwóch równorzędnych urządzeń.
- [x] Dodać nieblokujący sterownik USART0 z kolejkami RX/TX i programem echo; kompilacja AVR oraz test sprzętowy pozostają do potwierdzenia.
- [ ] Przetestować UART 115200 8N1 na obu RF-Nano: pojedyncze bajty, dane binarne, długie serie, przepełnienia i błędy USART.
- [x] Dodać i uruchomić na obu fizycznych RF-Nano obsługę SPI oraz diagnostykę rejestrów nRF24L01+; potwierdzenie COM10/COM11 w RealTerm, 2026-11-03.
- [x] Zweryfikować inicjalizację SPI/nRF na obu RF-Nano: oba raportują `NRF24 OK S=0E CFG=0C CH=4C RF=06 FIFO=11`, a procedura OK obejmuje readback rejestrów.
- [ ] Dodatkowa walidacja: build obu konfiguracji, obsługa odłączenia/awarii SPI i przypadki graniczne; dwukierunkowy RAW32 działa, ale nie sprawdzono jeszcze awarii/zakłóceń.
- [x] Przygotować firmware i sterownik PHY dla surowych 32-bajtowych ramek A→B i B→A, z auto-ACK, MAX_RT, ARC_CNT i RX_DR; testy modelowe przeszły.
- [x] Fizycznie zweryfikować RAW32 A→B→A: COM10 A i COM11 B, zgodne SEQ 000B–0019, `TX=ACK`, `REPLY=OK`, `RX=OK`, `ARC=00`, `RD=1` na zrzucie RealTerm (2026-11-03).
- [x] Zweryfikować przytrzymanie RESET B, pojawienie się `MAX_RT ARC=0F REPLY=MISS` na A oraz samoczynny powrót do `TX=ACK REPLY=OK` po zwolnieniu RESET B, bez restartu A. Sprawdzono również restart A i wznowienie wymiany.
- [ ] [ODROCZONE] Pełne odcięcie zasilania nRF24: obecny test wymaga odłączenia USB/CH340, co usuwa COM w RealTerm, a jego ponowne otwarcie resetuje ATmegę. Wykonać dopiero przy niezależnym zasilaniu lub aparaturze bez tego ograniczenia.
- [x] Wdrożyć koder/dekoder v1 (8 B nagłówek, 24 B BODY, stałe 32 B w radiu), testowe payloady 0/1/23/24 B, RX_LAST i niezależny TX_SEQ; testy hosta PASS.
- [x] Zweryfikować fizycznie payload v1 na dwóch RF-Nano: cykl długości 0/1/23/24 B, zgodne SEQ 45..53, A `REPLY=OK`, B `RX=OK`, obie strony `TX=ACK ARC=00` (zrzut RealTerm, 2026-11-03).
- [x] Potwierdzić samoczynne wznowienie testowego payloadu v1 po resetowaniu jednej lub obu ATmeg (informacja właściciela, 2026-11-03).
- [ ] Dodatkowo sprawdzić fizyczne odrzucanie błędnych ramek/niezerowego paddingu i warunki graniczne RX FIFO; dotąd negatywne testy parsera tylko na hoście.
- [x] Zaimplementować pierwszy dwukierunkowy UART↔RF z sesją HELLO/READY, DATA_ACK, stop-and-wait, deduplikacją, atomowym przyjęciem segmentu i EEPROM epoch; kod i hostowy model gotowe, fizyczny test otwarty.
- [x] Potwierdzić fizyczny dwukierunkowy most UART↔RF: COM10→COM11 `ala ma kota` oraz COM11→COM10 `kot ma ale`, identyczny firmware, zrzut RealTerm użytkownika (2026-11-03).
- [x] Sprawdzić równoczesne nadawanie plików w obu kierunkach: RealTerm COM10/COM11, wielosegmentowe strumienie, ciągły odbiór bez widocznego zakleszczenia; zrzuty użytkownika (2026-11-03), Delays=5/3.
- [ ] Potwierdzić integralność równoległych strumieni bajt po bajcie (capture, rozmiar, hash), retransmisje/duplikaty i zachowanie przy przeciążeniu; zrzuty terminali nie stanowią dowodu bezstratności.
- [ ] Zmierzyć rzeczywistą przepustowość w ramkach/s i B/s w obu kierunkach; wartość 18–20 ramek/s na kierunek to **oszacowanie z kodu**, nie pomiar; rozdzielić opóźnienia protokołu, SPI, radia, MCU i UART.
- [ ] Zastąpić sztuczne opóźnienia 20 ms oraz 2 ms przy aktywnym radiu obsługą zdarzeń i prawidłowych czasów ≥130 µs, z timeoutami; 25 ms B zastąpić bezpiecznym uzgodnieniem/oczekiwaniem uwzględniającym utracone ACK i ARC=15.
- [ ] Zweryfikować połączenie pinu IRQ nRF24 z MCU na konkretnej RF-Nano; jeśli NC, użyć nieblokującego odczytu STATUS/FIFO po SPI. Rozważyć ACK payload i przyspieszenie SPI do 8 MHz dopiero wraz z pomiarami i testami regresyjnymi.
- [ ] Zweryfikować bajty binarne (w tym 00/3F/FF), utratę ACK, reset podczas transmisji, przepełnienie RX UART i pacing w RealTerm; pełne pliki bez kontroli przepływu nie są gwarantowane.
- [ ] Zapewnić komunikację w obu kierunkach przy identycznym firmware.
- [ ] Utrzymywać naprzemienną wymianę 32-bajtowych ramek również bez danych UART.
- [ ] Wprowadzić wiarygodne, oddzielne statystyki jakości A→B i B→A, z rozróżnieniem strat i retransmisji.
- [ ] Współdzielić miejsce w ramce pomiędzy telemetrią i danymi użytkownika bez blokowania pomiarów.
- [ ] Zaimplementować automatyczne wykrywanie partnera.
- [ ] Obsłużyć jednoczesne próby nadawania bez trwałego zakleszczenia.
- [ ] Obsłużyć utratę i odzyskanie połączenia.
- [ ] Zweryfikować rezultat na dwóch fizycznych płytkach.

## Warunek zakończenia M1

Tekst wpisany do dowolnej z dwóch konsol pojawia się na drugiej. Obie strony mogą niezależnie inicjować transmisję, także podczas nakładających się prób nadawania. Urządzenia odnajdują się i odzyskują łączność po rozłączeniu.

**Status: M1 nieukończony.**

Szczegóły techniczne należą do kodu, testów i ewentualnej dokumentacji konkretnych warstw, a nie do tej listy.

## Koncepcja ACK Payload / STREAM / BURST

- [ ] Przed wdrożeniem profili MAC przetestować sprzętowy ACK Payload w obu kierunkach (z włączeniem DPL), testować FIFO, CRC, IRQ/STATUS, ponowienia i utracone ACK; nie mylić sprzętowego ACK z aplikacyjnym.
- [ ] Porównać zmierzone pakiety/s przy stałych rolach PTX/PRX z dotychczasowym ping-pongiem; zbudować konfigurowalną warstwę MAC korzystającą ze wspólnej warstwy PHY/formatu v1.
- [ ] Zaprojektować bezpieczny REQUEST/GRANT/COMMIT zmiany właściciela PTX, ograniczony lease BURST do pomiaru (propozycja 20 ms), SACK/kredyty i limity pamięci na 2 kB SRAM; zgodnie z `mac-profiles-proposal.md`.

## ACK Payload — konkretny eksperyment sprzętowy

- [x] Dodać odizolowaną aplikację 32 B DATA / 32 B ACK Payload z jednym wsadem na A i B, SPI 8 MHz, Timer1, polling TX_DS/MAX_RT i licznikami/s, z łatwym powrotem do mostu UART.
- [x] Dodać testy hostowe wzorca i szybkiego PHY (lokalne g++ C++11, PASS) oraz mock SPI z DPL/ACK Payload, podłączyć je do powtarzalnego skryptu testowego.
- [x] Uruchomiono F7/F12 i fizyczny ACK Payload 32B↔32B na obu RF-Nano: stabilnie **540 transakcji/s**, A TX/s=ACK/s=540, B RX/s=Q/s=540, EMPTY/MAX/ARC/BAD/IO/GAP/FULL=0 (zrzut RealTerm 2026-11-03).
- [ ] Po sprzętowym wyniku porównać osobno prędkość surowych 32 B, ACK Payload, liczbę pustych ACK, retransmisje i zajętość FIFO. Dopiero wtedy zdecydować o STREAM/BURST.

- [ ] Zmierzyć rzeczywistą latencję pojedynczej transakcji i rozkład czasów (min/średnia/p99), bez utożsamiania okresu 1,85 ms przy 540/s z latencją RF; następnie porównać potencjał z 18 B UART na segment i realnym end-to-end goodput.

## Kompaktowa ramka 4+28 dla już sparowanej pary

- [x] Wybrać format **8 B metadanych + do 24 B przezroczystego BODY**, zamiast 4+28; priorytet: diagnostyka i sterowanie, bez interpretacji danych transportu (decyzja 2026-11-03).
- [x] Uzgodniono i zaimplementowano układ 8 B (TX_SEQ, RX_LAST, LOSS_AB/BA, ARC_LAST/RX_DROP, RX_CREDIT, TYPE_FLAGS, SESSION_ID16) w oddzielnym formacie `paired_frame.h`; hostowe testy modulo i DPL PASS. Szczegóły `paired-core-v2.md`.
- [ ] Zweryfikować bezpieczne odseparowanie sesji po resecie bez SID w każdej ramce: negocjowany adres radiowy per epoch albo alternatywny identyfikator, z próbami resetów, FIFO flush i wykrywaniem starych ACK Payload.
- [x] Dodano wielokrotnego użytku DPL 8..32 B dla szybkiego PHY, testy hostowe oraz niezależny wariant 2; zachowany poprzedni benchmark 32+32 B w trybie 1.

## Bieżący etap: test nowego 8+24 na dwóch RF-Nano

- [x] Kod codec/link/PHY i demonstrator `paired_core_main.cpp`, wybór 0/1/2, stałe bufory i oddzielenie aplikacyjnego BODY od diagnostyki.
- [x] GitHub Actions: wszystkie testy hostowe i krzyżowa kompilacja AVR-GCC wariantów 0/1/2 zaliczone (2026-11-03).
- [ ] Zbudować F7 w Microchip Studio i wgrać F12 na **obie** płytki: tryb 2. Zebrać RealTerm `A TX/s ACK/s` i `B RX/s Q/s` (po kilkanaście sekund), porównać z osobnym pomiarem starego 540/s. Potwierdzić prawidłowy BODY 24B i IDLE 8B, brak FULL/BAD/IO.
- [ ] Wynegocjować prawdziwy SESSION_ID16 przy parowaniu i automatyczny powrót po resetach obu stron — stałe `0x4E52` jest teraz WYŁĄCZNIE testowym ID, nie gwarancją rozdzielenia sesji.
- [ ] Powiązać RX_CREDIT oraz RX_DROP z rzeczywistym sinkiem danych (UART lub inna usługa) i sprawdzić bezpieczeństwo pipeline ACK Payload, timeouty i wielokrotne przepełnienia.
- [ ] Dodać później wyższe protokoły (file, TCP-like, szyfrowanie) poza tym rdzeniem dopiero na osobne zamówienie; obecnie DATA BODY jest transparentne.
