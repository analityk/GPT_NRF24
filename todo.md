# TODO — aktywny transport dwukierunkowy UART / ACK Payload

> **Eksperyment, nie plan produkcyjny:** na gałęzi
> `experiment/pp-echo-ramp-2026-11-04` znajduje się tymczasowy wariant
> echo/ramp A→B→A. Aktualny stan i instrukcja: `echo-ramp-test.md`.
> [ ] Fizycznie potwierdzić pełne echo, `echoGaps==0`, zmiany odstępu
> co 100 µs, przypadki nasycenia i prędkość przy obu rolach.
> [ ] Po próbie przywrócić `main` i ręcznie F12 wgrać bazowy PP na oba MCU.

## Potwierdzone wcześniej na dwóch RF-Nano

- [x] Sprzętowy ping-pong 8+24 B; około 625 wymian/s bez handshake'u i 612–613/s z handshake'em.
- [x] Odzyskiwanie sesji z nowym SID po resecie A, B i obu.
- [x] UART↔UART w jednym kierunku; użytkownik potwierdził przesyłanie plików.
- [x] Ujawniono ubytki podczas dwóch równoczesnych intensywnych nadawców w wersji stop-and-wait. To zjawisko realne, ale jego indywidualne przyczyny sprzętowe nie zostały jeszcze izolowane.

## Aktualna implementacja — test hostowy i AVR PASS, sprzęt do sprawdzenia

- [x] Usunięto sztuczne opóźnienie bezczynności 10 ms; PTX stale wyzwala DATA/ACK Payload, także przy pustym UART.
- [x] Zastąpiono stop-and-wait przez **okno 4×24 B w każdym kierunku**, kumulacyjne aplikacyjne ACK, Go-back-N po 8 wymianach bez ACK, przyjmowanie tylko kolejnych numerów.
- [x] Zachowano identyczny 8B nagłówek rdzenia, dynamiczne 8–32B ramki, negocjowany SID, CRC16, APP_ACK w RX_LAST nagłówka; BODY całkowicie przezroczyste i kredyt sinku.
- [x] Dopisano regresję multiple-in-flight po zmianie SID, brak pierwszej ramki i recovery, wrap sekwencji, pełny TX oraz 4096B/kierunek przy stratnych ACK.
- [x] Test 600 wymian × **19 B wejścia/wymianę w obu kierunkach** bez RTS/CTS i z idealnym RF: brak overflow RX. Test przy 30 B/wymianę: overflow obu odbiorników.
- [x] Kompilacja AVR-GCC C++11 PASS, statycznie 687B (.data+.bss) z 2048B SRAM.
- [ ] **Fizyczny odbiór nowego firmware:** F7 i ręcznie F12 na A i B, RealTerm 115200 8N1 bez local echo; oba pliki równocześnie przy `Delays=0,0` i przechwytywanie otrzymanych bajtów. Porównać z plikami wejścia (długość/hash) dla obu kierunków. 
- [ ] Zmierzyć faktyczną liczbę wymian/s i użytkowy goodput obu UART w nowej wersji oraz rozkład opóźnień, **nie przenosić 612/s ze starego firmware**.
- [ ] W przypadku dalszych ubytków sprawdzić osobno UART RX `rx_buffer_full`, `rx_overrun` i `rx_framing`, `UartReliableStream::blocked`, `badFrames`, `retransmits`, RF `MAX_RT` i `ARC_CNT`. Nie dopisywać diagnostyki do przejrzystego strumienia COM bez osobnego trybu.
- [ ] Realny limit 115200 bez RTS/CTS: w przypadku przeciążenia zastosować flow control/ograniczanie hosta lub kolejny protokół upstream. Samo okno 4 nie daje absolutnej gwarancji braku overflow.
- [ ] Zbadać zachowanie danych w locie przy resetach A/B w aktualnej wersji okienkowej, z tolerancją braku exactly-once przez utratę pamięci MCU.

## Opcje później

- [ ] Trwałe `DEVICE_ID`/`PAIR_ID` niezależnie od SID.
- [ ] Wyższa warstwa bulk/file/encryption wyłącznie na osobne zamówienie, bez zmiany semantyki MAC.

**Status:** pełne tempo ping-ponga i pipeline4 gotowe; testy host+AVR PASS; fizyczny test dwukierunkowego flood nowej wersji oczekuje na wynik.

- [x] Usunięto z BODY dwa bajty wtórnego nagłówka UART: **pełne 24 B danych**, DATA_SEQ=TX_SEQ, APP_ACK=RX_LAST, IDLE=8 B; powtórzony DATA przenosi nowszy ACK. Zastąpiono dawną API PHY-seq jednym profilem paired stream; host i cross AVR PASS.
- [ ] **Nowy test sprzętowy:** równoczesne pliki na obu RF-Nano przy `Delays=0,0`; capture binarny, porównanie długości i SHA-256 A→B/B→A. Wersja z 24B BODY nie była jeszcze wgrana ani zmierzona.

## PP checkpoint i implementacja BURST20 (od 2026-11-04)

- [x] Nazwać i zachować aktualny sprzętowo pokazany protokół **PP (Ping-Pong)** w `checkpoint/pp-2026-11-04` (SHA `784fd83`); brak pomiaru binarnej bezstratności, nie oznaczać jako verified-perfect.
- [x] Zweryfikować czas 32B on-air przy 1Mbps: 329µs, maks. 15 pełnych pakietów fizycznie w 5ms **bez** narzutów; 32 DATA+1 CONTROL niemożliwe. Opracować harmonogram z rzeczywistym deadline, a nie stałą liczbą 32 pakietów.
- [x] Dodać wielokrotnego użytku `Burst20Schedule`, podział 5/15,10/10,15/5, 1ms report i 0,3ms guard na każdą stronę, testy granic/rollover.
- [x] Dodać `ProfileSelect`: polityka strap A1 pull-up HIGH=PP, GND=BURST20 z dwukrotnym odczytem; test hostowy.
- [ ] Wdrażać osobny `NrfBurstPhy`, nie zmieniać ani nie zastępować działającego `NrfAckPayloadPhy` PP. DATA bez ACK (pierwszy wariant), ramka REPORT w zarezerwowanym oknie, zakończenie próby na deadline.
- [ ] Wdrożyć synchronizację okresu 20ms lider A↔B przez kontrolę on-air, fail-closed przy utracie fazy; testy utraty REPORT, opóźnienia IRQ, drift, resetu jednej/obu płytek.
- [x] Dodać `Burst20Report` codec do 24B BODY, liczby/CRC32/bitmapa odbioru poprzedniego bloku oraz testy długości/wariantu, serializacji i CRC32=0xCBF43926 (host PASS).
- [ ] Zintegrować `Burst20Report` z rzeczywistym radio driverem i synchronizacją cyklu, w tym obsługę utraconego raportu, rollover cyklu, symetryczne potwierdzenie poprzednich bloków.
- [ ] Wspólny firmwarowy boot dispatcher PP/BURST20 po A1 dopiero gdy BURST profile jest kompletny; obecny main **nie czyta jeszcze A1 i nadal uruchamia PP niezależnie od strapu**.
- [ ] Zmierzyć na fizycznych RF-Nano pakiety/5ms i czy ramka REPORT mieści się w przeznaczonym 1ms, przesunięcie roli 0,3ms, 10/10 i następnie 5/15; nie interpretować modelu jako pomiaru.
- [ ] Dla 5/15 przeprojektować świadomie UART ingress: 15ms przy 115200 8N1 to ~173B, więcej niż 127B RX ring; zatwierdzić flow control lub większy bufor/mniejszy baud na BURST bez zmieniania PP.
