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
