# TODO — aktywny transport dwukierunkowy UART / ACK Payload

## Potwierdzone wcześniej na dwóch RF-Nano

- [x] Sprzętowy ping-pong 8+24 B; około 625 wymian/s bez handshake'u i 612–613/s z handshake'em.
- [x] Odzyskiwanie sesji z nowym SID po resecie A, B i obu.
- [x] UART↔UART w jednym kierunku; użytkownik potwierdził przesyłanie plików.
- [x] Ujawniono ubytki podczas dwóch równoczesnych intensywnych nadawców w wersji stop-and-wait. To zjawisko realne, ale jego indywidualne przyczyny sprzętowe nie zostały jeszcze izolowane.

## Aktualna implementacja — test hostowy i AVR PASS, sprzęt do sprawdzenia

- [x] Usunięto sztuczne opóźnienie bezczynności 10 ms; PTX stale wyzwala DATA/ACK Payload, także przy pustym UART.
- [x] Zastąpiono stop-and-wait przez **okno 4×22 B w każdym kierunku**, kumulacyjne aplikacyjne ACK, Go-back-N po 8 wymianach bez ACK, przyjmowanie tylko kolejnych numerów.
- [x] Zachowano identyczny 8B nagłówek rdzenia, dynamiczne 8–32B ramki, negocjowany SID, CRC16, APP_ACK w opaque BODY i kredyt sinku.
- [x] Dopisano regresję multiple-in-flight po zmianie SID, brak pierwszej ramki i recovery, wrap sekwencji, pełny TX oraz 4096B/kierunek przy stratnych ACK.
- [x] Test 600 wymian × **19 B wejścia/wymianę w obu kierunkach** bez RTS/CTS i z idealnym RF: brak overflow RX. Test przy 24 B/wymianę: overflow obu odbiorników.
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
