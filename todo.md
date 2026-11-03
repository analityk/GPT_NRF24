# TODO — aktywny rdzeń P2P nRF24

## Zaliczone

- [x] Sprzętowy DATA A→B / ACK Payload B→A z identycznym firmware na obu płytkach.
- [x] Format 8 B nagłówka + 0–24 B nieinterpretowanych danych, DPL 8–32 B, kodowanie niezależne od ABI.
- [x] Numeracja w obu kierunkach i diagnostyka: TX_SEQ, RX_LAST, LOSS_AB/LOSS_BA, ARC_LAST, RX_DROP, RX_CREDIT, FLAGS, SESSION_ID16.
- [x] Test sprzętowy: około **625 wymian/s**, wzorzec 24 B na kierunek, bez widocznych ARC/MAX/BAD/IO/GAP/DROP/FULL w zarejestrowanych oknach.
- [x] Uporządkowanie repozytorium do jednego aktywnego firmware i przegląd zależności; poprzednie etapy zachowane w gałęzi `archive/pre-paired-core-cleanup-2026-11-03`.
- [x] Testy hostowe i kompilacja AVR w CI (bez uruchamiania F12 automatycznie).

## Do zrobienia — bieżący protokół

- [ ] Prawdziwa negocjacja `SESSION_ID16` z rozdzieleniem epok i automatycznym odzyskaniem łączności po resecie jednej lub obu ATmeg. Obecne `0x4E52` to stała diagnostyczna.
- [ ] Zabezpieczyć stan po utraconym/zdublowanym ACK Payload oraz po długim braku partnera; przetestować zmianę sesji z pakietami w FIFO.
- [ ] Powiązać `RX_CREDIT` i `RX_DROP` z realnym odbiorcą danych (w tym obsługa kredytów in-flight), bez modyfikowania znaczenia BODY.
- [ ] Sprawdzić sprzętowo ramki IDLE (8 B) i krótsze DATA (9–31 B), przepełnienia FIFO oraz diagnostykę przy spadającej jakości zasięgu.
- [ ] Zmierzyć rozkład rzeczywistej latencji pojedynczej wymiany i goodput po dołożeniu rzeczywistego sinku; 625/s oznacza średnią repetycję, nie pomiar latency.
- [ ] Zarejestrować fizyczne testy resetu, timeoutu, `MAX_RT`, ARC, duplikatów i zawijania czterobitowych liczników strat.
- [ ] Po zakończeniu rdzenia zaprojektować opcjonalne usługi UART/pliki/wiadomości i ochronę danych *ponad* opaque BODY — poza zakresem aktualnego porządkowania.

## Stan

Rdzeń 8+24 fizycznie przetestowany w nominalnych warunkach. **Odporność na reset i pełna kontrola przepływu nie są jeszcze zaliczone.** Jeden projekt, jeden `main.cpp`, ręczne F7/F12.
