# TODO — aktywny rdzeń P2P nRF24

## Zaliczone

- [x] Sprzętowy DATA A→B / ACK Payload B→A z identycznym firmware na obu płytkach.
- [x] Format 8 B nagłówka + 0–24 B nieinterpretowanych danych, DPL 8–32 B, kodowanie niezależne od ABI.
- [x] Numeracja w obu kierunkach i diagnostyka: TX_SEQ, RX_LAST, LOSS_AB/LOSS_BA, ARC_LAST, RX_DROP, RX_CREDIT, FLAGS, SESSION_ID16.
- [x] Test sprzętowy: około **625 wymian/s**, wzorzec 24 B na kierunek, bez widocznych ARC/MAX/BAD/IO/GAP/DROP/FULL w zarejestrowanych oknach.
- [x] Uporządkowanie repozytorium do jednego aktywnego firmware i przegląd zależności; poprzednie etapy zachowane w gałęzi `archive/pre-paired-core-cleanup-2026-11-03`.
- [x] Testy hostowe i kompilacja AVR w CI (bez uruchamiania F12 automatycznie).

## Do zrobienia — bieżący protokół

- [x] Kod negocjacji `SESSION_ID16` z epokami EEPROM i próbami, BEACON/HELLO/WELCOME/READY/READY_ACK, automatyczny rekey A/B po timeoutach, testy hostowe i kompilacja AVR.
- [x] **Fizycznie potwierdzony handshake i dwa pojedyncze resety:** A i B automatycznie wracają do zgodnego niezerowego SID i ST=4. Po resecie B wspólny SID=12715, po resecie A wspólny SID=16598; działający peer zwiększa R do 1, restartujący wraca z R=0. Stabilnie 612–613 wymian/s (2026-11-04).
- [ ] Osobno sprawdzić **jednoczesny reset obu RF-Nano** i automatyczny powrót do ST=4, zgodnego nowego SID; nie był częścią pokazanych testów.
- [x] Implementacja unieważniania starych ACK Payload na zmianie epoki, odrzucania opóźnionych BEACON i duplikatów, stabilnego DISCOVER podczas braku partnera; testy hostowe PASS.
- [x] Model pełnego potoku ACK Payload z opóźnioną odpowiedzią: sparowanie, 300 wymian, restart A/B/obu, utracone ramki i ACK oraz ponowne negocjacje; `tests/paired_session_pipeline_test.cpp`, host PASS.
- [ ] Zweryfikować fizycznie błędy `MAX_RT`, chwilowe rozłączenie RF, opóźnione ACK i zachowanie FIFO przy resetach. Potwierdzić, że nie występują trwałe dwie różne aktywne sesje.
- [ ] Powiązać `RX_CREDIT` i `RX_DROP` z realnym odbiorcą danych (w tym obsługa kredytów in-flight), bez modyfikowania znaczenia BODY.
- [ ] Sprawdzić sprzętowo ramki IDLE (8 B) i krótsze DATA (9–31 B), przepełnienia FIFO oraz diagnostykę przy spadającej jakości zasięgu.
- [ ] Zmierzyć rozkład rzeczywistej latencji pojedynczej wymiany i goodput po dołożeniu rzeczywistego sinku; 625/s oznacza średnią repetycję, nie pomiar latency.
- [x] Zarejestrowano fizyczny pojedynczy RESET A i B oraz sprzętowe niepowodzenia ACK po zniknięciu partnera: MAX=110 z ARC=330 i MAX=219 z ARC=657; po restarcie wznowiono transmisję.
- [ ] Zmierzyć i zweryfikować pozostałe przypadki: długotrwała utrata zasięgu, zniekształcenia, duplikaty, zawijanie 4-bitowych liczników oraz reset jednoczesny.
- [ ] Po zakończeniu rdzenia zaprojektować opcjonalne usługi UART/pliki/wiadomości i ochronę danych *ponad* opaque BODY — poza zakresem aktualnego porządkowania.

## Stan

Rdzeń 8+24 fizycznie przetestowany w nominalnych warunkach. **Powrót po pojedynczym resecie A/B jest zaliczony sprzętowo; reset jednoczesny i pełna kontrola przepływu pozostają otwarte.** Jeden projekt, jeden `main.cpp`, ręczne F7/F12.
