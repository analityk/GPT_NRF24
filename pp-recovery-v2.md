# PP — symetryczne odzyskiwanie łącza, próba na dwóch RF-Nano

**Gałąź:** `experiment/pp-recovery-v2`. To eksperyment do wgrania na
urządzenia, NIE zamiennik sprawdzonego PP na `main` ani w checkpoint.

## Dlaczego poprzedni wariant się zatrzaskiwał

W starym `main.cpp` cisza na radiu powodowała automatycznie `restartIdentity()`
po 250 ms na A lub 750 ms na B. Obie strony mogły wtedy zresetować numerację
okna 4×24 B w różnych chwilach. A nie otrzymywało potwierdzeń danych, które
B mógł już przyjąć; wznowienie przez nowy SID nie rozstrzygało, co zostało
dostarczone. W szczególności samo `MAX_RT`, brak ACK Payload lub cisza **nie
oznaczają** fizycznego restartu partnera.

Obecna zmiana umieszcza decyzje w przenośnym `PpRecoveryControl`, buforowanie
i ponawianie w `UartReliableStream`, a nRF/PHY pozostawia bez zmiany. Nie
dodaje żadnych bajtów do ramek RF ani nie modyfikuje protokołu PP 8+24 B.

## Stany — identyczna logika na A i B

| Stan `link` | Kiedy | Działanie lokalnego procesu nadającego |
|---|---|---|
| `0` CONNECTING | początek, po rozpoznanym nowym peerze albo po przekroczonym limicie | bez nowych danych; negocjacja HELLO/BEACON/WELCOME/READY/READY_ACK |
| `1` HEALTHY | poprawny handshake oraz poprawne ramki z aktualnym SID | zwykły PP; okno 4×24B, aplikacyjny ACK |
| `2` SUSPECT | brak ważnego pakietu przez 3 spodziewane interwały (~4,8 ms przy 625 wymianach/s) | zatrzymaj nowe dane, zachowaj RAM, A wysyła IDLE |
| `3` OFFLINE | cisza 1 s od ostatniego dobrego pakietu | wstrzymaj producenta, A odpytuje IDLE co 20 ms; B czeka w PRX |

**Ciepłe wznowienie:** w ciągu 5 s od ostatniej ważnej ramki dochodzi
poprawny DATA/IDLE z tym samym SID. Oba urządzenia zachowują istniejące
numery segmentów, kolejkę niepotwierdzonych 24B porcji i ACK. `warm` rośnie.
Płynnie wznawiają stare okno, bez nowych nonce/SID.

**Zimne wznowienie (reset partnera):** druga płytka przesyła HELLO lub
BEACON z nową tożsamością `boot_epoch32+attempt16`. Następuje nowy handshake,
nowy SID i ponowienie lokalnie niepotwierdzonych segmentów. `cold` rośnie.
Jeśli były segmenty bez aplikacyjnego ACK, zostaje ustawiony status
`UNCERTAIN` — nawet po odzyskaniu linku.

**Zimne wznowienie (koniec limitu ciepłego):** po 5 s ciszy każda strona
samodzielnie porzuca próbę ciepłego wznowienia, zwiększa własne `attempt16`,
przechodzi do CONNECTING, uzgadnia nowy SID i odnotowuje `exp`. Decyzja jest
celowa, ale **nie dowodzi restartu partnera**. Niepotwierdzone dane pozostają
w pamięci; możliwe jest powtórzenie bajtów już wcześniej przyjętych.

Przejście od ACTIVE do SUSPECT/OFFLINE **nigdy nie kasuje pamięci**. Reset
fizycznego AVR kasuje jego pamięć bez możliwości jej odzyskania, czego radio
ani tymczasowy bufor w drugim AVR nie mogą naprawić.

## Dwa bajty dla lokalnego hosta, nie dla radia

`HostFlowStatus::snapshot(recovery,txFree,rxFree)` zwraca dwa bajty. Dotyczą
interfejsu **host ↔ MCU**. `txFree` to miejsce w kolejce oczekującej na
radio, `rxFree` to miejsce w kolejce lokalnego odbiorcy. Nie są przenoszone
przez radio. W obu bajtach bity 0..5 = `min(63, freeBytes/4)` (pełne czwórki
bajtów). Prawdziwa możliwość wpisania `n` bajtów wymaga atomowego
`HostFlowStatus::canCommit(recovery, exactTxFree, n)`.

- `TX_STATUS` bit 7 (`0x80`): STOP, nie przyjmuj teraz nowych danych;
  bit 6 (`0x40`): UNCERTAIN, co najmniej jedna starsza porcja może być
  dostarczona więcej niż raz albo jeszcze nie dotarła.
- `RX_STATUS` bit 7 (`0x80`): partner nie jest obecnie potwierdzony jako
  HEALTHY; bit 6 (`0x40`): poniżej 24B wolnego miejsca lokalnego sinku.

To rozdziela politykę obsługi producenta od konkretnego ATmega, UART czy
przyszłego szybszego MCU. Produkcyjne ustawienie klasy to `HOLD`: gdy nastąpi
zmiana sesji przy oczekujących danych, wstrzymuje ich powtarzanie aż host
wywoła `retryUncertain()` (świadoma zgoda na *at-least-once*). Dla **tego
testu nasycenia** `main.cpp` jawnie wybiera
`RETRY_AT_LEAST_ONCE`: po handshake wraca do pracy samodzielnie, ale
`UNCERTAIN` pozostaje do świadomego rozliczenia przez hosta. Eksperymentalna
konsola sama nie obsługuje potwierdzania tego bitu.

**UART bez RTS/CTS:** RealTerm nie respektuje tych statusów automatycznie.
Tutejszy syntetyczny generator A respektuje STOP. Zewnętrzny host na
niekontrolowanym UART nadal może przepełnić 127B wejściowej kolejki i zgubić
bajty; status nie jest gwarancją niezawodności RealTerm.

## Jak wykonać test fizyczny

1. Przełącz repo na gałąź: `git fetch origin` i
   `git switch --track origin/experiment/pp-recovery-v2` (gdy gałąź lokalna
   jeszcze nie istnieje). Nie mieszaj tego z `main`.
2. W Microchip Studio otwórz `GPT_NRF2401.atsln`, użyj **F7**. Po udanej
   kompilacji **ręcznie F12** na obu RF-Nano. Takie samo firmware na obu:
   A0 do GND = A/PTX, A0 do zasilania = B/PRX; A1 nie wybiera jeszcze BURST.
3. Otwórz RealTerm 115200 8N1 na obu portach. Ustal fizyczne mapowanie
   COM↔płytka przez pojedyncze odłączenie USB: nie zgaduj roli po położeniu
   okna terminala. W diagnostyce A raportuje `A cps=...`, B `B cps=...`.
4. Po ustaleniu transmisji obie linie powinny pokazywać `up=1`, ten sam
   `sid` oraz CPS (w zależności od obciążenia). A domyślnie generuje dane
   **maksymalnie szybko**; wysłanie `w` zwiększa odstęp o 100 µs, `s`
   zmniejsza. Bajty wzorca pozostają w pamięci, nie są wypisywane na UART.
5. Osłoń radio jednej płytki na **0,2–1 s**, odsłoń. Szukaj przejścia
   `link=2` (ew. `3`) → `link=1`, wzrostu `warm`, **tego samego SID**,
   bez trwałego zatrzasku. Dla B nawet w stanie OFFLINE host nie generuje
   nowych bajtów; nie utożsamiaj tego ze sprzętowym blokowaniem PRX.
6. Osłoń radio na **ponad 5 s** i odsłoń. Oczekiwane `link=3`, potem `0`,
   po powrocie `1`, inny `sid`, wzrost `exp`. Jeśli były dane w locie,
   spodziewany `uncertain=1`.
7. Resetuj **tylko B** (A0 przy zasilaniu), obserwuj nowy SID i `cold` na A;
   potem **tylko A** (A0 do GND), obserwuj `cold` na B. Powtórz próbę z
   odłączonym zasilaniem jednej płytki na kilka sekund, także w obu
   kierunkach. Na końcu zresetuj oba, sprawdź stabilne wspólne SID.
8. Jeżeli choć raz zawiśnie `sid=0`, `up=0` lub `cps=0` po przywróceniu
   łączności, zachowaj po 10–20 linii z obu terminali **przed, w trakcie
   i po zdarzeniu**, z `A arc/max/ret/win`, `B bad/lg/ls`, `link`, `sid`.

Raport `A/B link=N tx=... rx=... warm=... cold=... exp=... uncertain=...`
jest drukowany przy zmianie stanu. Zwykła linia `cps/ret/...` nadal pojawia
się co sekundę. Zbieranie statusu nie wykonuje dodatkowych odczytów SPI.

## Dowody i ograniczenia

- Host: `sh tests/run_host_tests.sh` sprawdza stany, granice 3 wymian,
  1 s, 5 s, Timer1 wrap, zapełnienie, zatrzymanie generatora, utrzymanie
  RAM i numeracji przy ciepłym wznowieniu, rekey po resecie A/B/obu oraz
  po długiej ciszy. To **symulacja**, a nie pomiar RF.
- AVR: `sh tests/build_avr.sh` wymaga `avr-g++`. W środowisku autora
  zmian kompilator AVR i dwie fizyczne RF-Nano nie były dostępne.
- Nie można udowodnić *exactly-once* przy utracie zasilania bez trwale
  potwierdzonej pamięci po obu stronach. Po zmianie SID bieżący test
  wybiera *at-least-once*, jawnie sygnalizując niepewność.
