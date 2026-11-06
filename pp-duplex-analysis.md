# Analiza PP / UART full duplex — 2026-11-06

Badana baza: `testy` @ `35fd83bb7258ba49161aa48109411ec56fdacc10`.
Zrzut właściciela `obraz(20261106-011416).png`, SID 7556, RealTerm
115200 8N1, `Delays=0,15`. To analiza istniejącego przebiegu, nie pomiar
nowego firmware. Żaden test hostowy nie zastępuje dwóch RF-Nano.

## Co rzeczywiście pokazuje zrzut

| Wartość | A | B |
|---|---:|---:|
| UART przyjęte | 16638 | 14706 |
| UART wysłane | 14706 | 16638 |
| UART odrzucone przez pełny RX ring | 0 | 1932 |
| DOR / FE | 0 / 0 | 0 / 0 |
| PP potwierdzone bajty | 16638 | 14706 |
| PP przyjęte bajty | 14706 | 16638 |
| PP retransmisje | 0 | 231 |
| PP blocked / bad | 0 / 152 | 0 / 0 |
| Okno po zakończeniu / wolne RX | 0 / 255 | 0 / 255 |
| ARC / MAX_RT | 1163 / 272 | — |
| Pełny ACK FIFO / pełny RX FIFO (próbki) | — | 5 / 0 |

Łańcuch liczników jest zgodny w obu kierunkach:
A UART in = A APP_ACK = B PP rx = B UART out;
B UART in = B APP_ACK = A PP rx = A UART out.
B przyjęło 14706 + odrzuciło 1932 = 16638 oferowanych bajtów.
Około 11,6% wejścia B zginęło **przed przyjęciem do transportu**.
Radiowy retry nie odzyska tych bajtów. Zgodność liczników nie dowodzi
zgodności zawartości; do tego potrzebny jest Capture i porównanie bajtów.

Nie widać trwałego deadlocku: okna są opróżnione, bufor zwolniony,
SID zgodny. `bad` w tym kodzie oznacza przede wszystkim DATA poza
kolejnością, nie błąd CRC w eterze. `blk=0` nie wskazuje na odmowę
przyjęcia do UART TX; `dor=fe=0` nie wskazuje na utratę w rejestrze
USART lub błędny format znaków. `af=5` i `rf=0` nie są dowodem
nieobecności chwilowych wyścigów FIFO, ale nie pokazują stałego zatoru B.

## Co powoduje zapełnianie wejścia

Bezpośrednia przyczyna: ISR dopisuje do `RetainedByteRing`, a jego
`acknowledged` przesuwa się dopiero po APP_ACK. W czasie braku postępu
potwierdzeń bufor przechowuje zarówno dane jeszcze niewysłane, jak
niepotwierdzone. Jego 255 B to **nie** 255 B plus 96 B okna.
Przy 115200 8N1 dostajemy 11520 B/s na kierunek. Nawet pusty bufor
wystarcza na około 22,1 ms całkowitego braku zwalniania miejsca;
z 96 B zatrzymanymi w pełnym oknie pozostaje około 13,8 ms.
To rachunek granicy, nie zmierzony czas awarii.

Na A wystąpiły 272 zakończenia MAX_RT i 1163 sprzętowe powtórki
w oknie raportu, **nie tylko od startu urządzenia**. SETUP_RETR=0x23
znaczy ARD=750 µs, ARC=3. Same odstępy przed tymi powtórkami odpowiadają
około 872 ms, do czego dochodzi czas nadawania powtórek. Nie znamy
rozkładu tych zdarzeń w czasie; pojedyncze i skupione awarie mają
zupełnie inny wpływ na bufor. Brak współczynnika liczba błędów/liczba
wszystkich transakcji również ogranicza interpretację starego raportu.

A może poprawnie dostarczyć DATA do B, a mimo tego otrzymać MAX_RT,
gdy powrotny ACK nie dotarł. Kolejne nowe transakcje mogą wtedy
przeskoczyć niedostarczony segment B→A. Odbiorca A odrzuca późniejsze
DATA, wysyła URGENT, B odtwarza okno Go-back-N i wstrzymuje świeże
segmenty do jego opróżnienia. Taki łańcuch jest **zgodny** z A ret=0,
B ret=231, A bad=152 i overflow tylko na B. Sam zrzut nie dowodzi,
że właśnie tak powstał każdy brak. Nie rozstrzyga przyczyny MAX_RT:
RF, zasilanie, zgodność układu z Nordic, stan FIFO lub obsługa sprzętu.
Nie wolno zamieniać tej hipotezy w „udowodnione przesterowanie”.

## Dlaczego 904 µs i syntetyczne 21,7 kB/s nie rozstrzygają

`markRfCycle` kwalifikuje wyłącznie udane wymiany z zerowym ARC,
DATA w przynajmniej jednym kierunku i zdrową sesją. `PpShortProbeWindow`
zatrzymuje pierwsze 1024 próbki, nie prowadzi statystyki całej próby.
`cyc=904`, `lo=679`, `hi=1118` dotyczą tej wyselekcjonowanej próbki,
ze zmiennymi długościami 8–32 B. Nie zawierają długich wymian z MAX_RT.
24/904 µs ≈ 26,5 kB/s jest jedynie hipotetycznym sufitem pełnych BODY
przy takim okresie, a nie zmierzonym goodputem tej próby.

Generator RADIO_STRESS omija pełny strumień RX+TX USART i nie odtwarza
profilu przerw CH340/Windows. Test `uart_unthrottled_ingress_test`
podaje N bajtów **na wymianę**, bez fizycznego czasu transakcji.
Gdy radio zwalnia wskutek retransmisji, taki model również zwalnia
producenta. Nie reprezentuje stałego UART 115200 podczas awarii RF.
To istotna luka w dowodzie wydajności, mimo poprawnych regresji
kolejności, retained ring i odzyskiwania sesji.

## Warstwy i ustalenia z kodu / kompilacji

- PHY: A pozostaje PTX, B PRX. Dane zwrotne pochodzą z wcześniejszego
  ACK FIFO. Nie ma przełączania ról na każdą ramkę ani 20ms delay.
- ARD=750 µs nie jest zbyt krótki. Nordic podaje, że 500 µs wystarcza
  dla dowolnego ACK payload przy 1 i 2 Mb/s. Nie obniżamy ARD w ciemno.
- PP: cztery deskryptory, APP_ACK po udanym `writeAll`, retransmisje
  z tej samej pamięci. Brak znalezionego dowodu, że znika już przyjęty
  bajt w tym przebiegu. A opóźnia przetwarzanie ACK o jedną wymianę;
  B może mieć dodatkowe opóźnienie przez kolejkę ACK i `ackStaged`.
- Go-back-N liczy brak postępu w przygotowanych ramkach, nie w czasie
  ściennym. `make` oznacza przydzielenie numeru/retencję, a nie pewne
  wysłanie w eter. Powtórki są konserwatywne i mogą być zbędne.
  Ewentualna przebudowa musi rozdzielać prepare/commit/terminal outcome
  oraz zachować replay/rekey; nie wystarczy zmienić liczby 8 na większą.
- Recovery po 4,8 ms bez poprawnej ramki przechodzi w SUSPECT i hamuje
  DATA; poprawna ramka przywraca HEALTHY. Po 1 s OFFLINE, po 5 s
  renegocjacja. Kilka MAX_RT może więc ograniczać transport dodatkowo.
- UART: RX i TX działają w przerwaniach; `writeAll` kopiuje dane przy
  włączonych przerwaniach, atomowo przełącza jedynie UDRIE.
- Z dostarczonego MAP: `.text=0x4142=16706`, `.data=0xce=206`,
  `.bss=0x40b=1035`. Flash wraz z inicjalizatorami: 16912 B;
  statyczny SRAM: **1241 B**, pozostaje **807 B przed stosem**.
  To inne wartości niż starsze wpisy MD i inny toolchain niż CI.
- LSS pokazuje zwykłe wywołania `receiveInterrupt` / `transmitInterrupt`
  z wektorów USART oraz zachowywanie 15 rejestrów w każdym wrapperze.
  Jest narzut CPU, lecz brak podstaw, by uznać go za przyczynę
  utraty sprzętowego ACK. DOR=0 nie znaczy zerowego kosztu ISR.
- W MAP nadal zajmują SRAM skonstruowane, wyłączone sondy:
  `performance` 51 B, `cycleTiming` 83 B, `sourceProbe` 10 B,
  `pendingTimingDetail` 46 B. Łącznie 190 B możliwych do usunięcia
  przez kompilacyjne zarządzanie pamięcią opcji. Nie są kolejnym
  buforem danych i ich powiększanie nie naprawia MAX_RT.

Źródło fizyki ESB: Nordic, *nRF24L01+ Product Specification v1.0*,
§7.4.2 (ARD), §7.5 (maszyna stanów), §7.8.4–7.8.5 (ACK Payload),
§8.4 (FIFO):
https://docs-be.nordicsemi.com/bundle/nRF24L01P_PS_v1.0/raw/resource/enus/nRF24L01P_PS_v1.0.pdf

## Zmiana w tej iteracji

Przenośny monitor `PpUartQuietDiagnostic` rejestruje pierwsze
**zaobserwowane** MAX_RT, replay PP, bad/out-of-order i RX overflow
oraz stan okna/kredytu/wolnego wejścia i przyrosty liczników w tym
samym momencie. Dochodzą minimalne próbkowane wolne miejsce i
najdłuższy próbkowany okres bez postępu APP_ACK przy backlogu.
Nie ma zmian pakietu radiowego, ISR, retry policy ani dodatkowych
kopii danych UART. Formatowanie działa dopiero po ciszy.

To rozszerzenie obserwowalności, **nie naprawa osiągów**. Próbkowanie
co około 10 ms nie ustala kolejności wewnątrz jednej próbki i może
przeoczyć krótszą pauzę lub minimum bufora. Zegary A/B nie są
zsynchronizowane; porównujemy chronologię osobno na każdej płytce.
Nowe linie pozwolą sprawdzić, czy overflow pojawia się po błędach
radia/PP oraz czy wtedy pozostaje pełne okno lub zerowy kredyt.
Wyniki hostowe i AVR opisano w `history.md`.

## Następny pomiar i kryterium sukcesu

1. Ten sam nowy firmware na obu płytkach, F7 i ręczne F12. Powtórzyć
   `Delays=0,20`, `0,15`, następnie `0,0`; zapisać wszystkie linie
   raportów obu stron po każdym pliku. Nie zamykać portów między
   próbami. Zanotować czas przesyłania i długości źródeł.
2. Porównać osobno A→B, B→A i równoczesny transfer. Rosnące `mx` przy
   samym B→A skieruje uwagę na ACK path; problemy wyłącznie przy
   pełnym dupleksie wymagają korelacji z obciążeniem i kolejkami.
3. Dopiero kontrolowane A/B jednej zmiennej: większy odstęp modułów,
   mniejsza moc nadawcza obu stron, inny kanał, następnie wolniejsze
   SPI. Wynik to liczby `hw/mx/full`, nie wizualnie równy tekst.
   Nie zmieniać jednocześnie retry, mocy, kanału i buforów.
4. Docelowy odbiór: `PP_UART_DIAG_ENABLED=0`, dwa różne strumienie,
   stałe oferowane 11520 B/s na kierunek, byte-for-byte, osobny pomiar
   latencji małych wiadomości i odzyskiwania po resetach/zaniku RF.
   Niskie opóźnienie bez obciążenia i przepustowość nasyconego łącza
   są odrębnymi kryteriami. Obecne dane nie dowodzą osiągnięcia celu.
