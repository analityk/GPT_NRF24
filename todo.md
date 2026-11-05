# TODO — aktywny transport dwukierunkowy UART / ACK Payload

## Pierwsza zaakceptowana wersja PP i dalsze eksperymenty — 2026-11-05

- [ ] **Usunąć magiczne liczby z konfiguracji rejestrów nRF24**
  w `Nrf24::probeAndConfigure()` (`GPT_NRF2401/nrf24.h`): tablica
  `pairs[][2]` nadal zawiera nienazwane wartości `CONFIG`, `EN_AA`,
  `EN_RXADDR`, `SETUP_AW`, `SETUP_RETR`, `RF_CH`, `RX_PW_P0`, `DYNPD`
  i `FEATURE`. Sam komentarz nie spełnia zakazu z `AGENTS.md`.
  Ustalić nazwy i znaczenie każdego pola, wprowadzić opisowe
  `#define` w odpowiedzialnej warstwie, wykorzystać je w konfiguracji
  i testach; przy okazji skontrolować pozostałe modyfikowane fragmenty
  sterownika pod tym samym kątem. Zachować dokładnie dotychczasowe
  wartości bitowe, ustawienie 2 Mb/s w teście i zachowanie sprzętu.
  **Zadanie odłożone na polecenie właściciela — teraz nie zmieniać kodu.**
- [x] Wersja `pp-window-drain`: 23/23 testy hostowe PASS i pozytywne
  próby dwóch RF-Nano po uruchomieniu B→A, resecie B i resecie A.
  Właściciel zaakceptował ją jako bazę `main`.
- [x] **Właściciel wydał polecenie rozpoczęcia prac na `testy`:**
  wydzielono nieblokujący automat PHY z kolejką FIFO i rozdzielono
  obsługę IRQ/statusu od pracy generatora podczas `ON_AIR`. Okno4,
  SID, kredyty, retransmisje i replay bez zmian; 24 testy hostowe PASS.
- [x] GitHub Actions `testy` run #91: host i AVR-GCC **PASS**;
  statycznie 974 B SRAM (`96 B .data + 878 B .bss`), kod 17740 B.
- [x] **Przygotowano sondę czasu cyklu PP** w `testy`: Timer1 mierzy
  SPI zapisu, GUARD, CE, okres do odczytu statusu, SPI odczytu ACK,
  koszt złożenia pakietu, protokół i zajętość tła. Uśrednianie wyłącznie
  stabilnych, pełnych wymian bez retry; raport `A us` w konsoli A.
  Testowe radio 2 Mb/s jest wybrane jawnie i symetrycznie, bez zmiany
  domyślnej konfiguracji sterownika. Procedura: `pp-cycle-timing-test.md`.
  **25/25 host PASS; AVR PASS** (GitHub Actions `testy` run #93;
  `.text=20430`, `.data=96`, `.bss=939`, SRAM=1035/2048 B).
  Pomiar na dwóch płytkach nadal przed nami.
- [ ] **F7 i ręczne F12 na obu RF-Nano.** Sprawdzić w RealTerm:
  start B→A i A→B, reset samego B, reset samego A, reset obu i
  odtworzenie po osłabieniu RF. Porównać `cps`, `arc/max`, `ret/bad`,
  `link`, `sid`, `up` z punktem `main` (~14,7 kB/s przy dobrym RF).
  Zachować zrzuty A+B; brak ujemnych zmian w odtwarzaniu sesji jest
  ważniejszy od samej poprawy szybkości.
- [ ] **Wgrać i odczytać fizyczny pomiar czasu:** co najmniej pięć
  raportów `A us` przy dobrym RF oraz odpowiadające im `A cps/B cps`.
  Ustalić, czy dominuje `cpu`, `grd` czy `rf` (z narzutem pollingu).
  Osobno przetestować wpływ sondy na CPS poprzez jej wyłączenie.
  Hostowe testy ani AVR build nie mierzą rzeczywistych µs.
- [ ] **Dopiero po pomiarze** ocenić transakcyjne przygotowanie
  kolejnej ramki podczas bieżącego RF: bez cofania przyjętych danych,
  bez utraty cache przy rekey/NACK/kredycie=0 i bez spekulacyjnego
  wysyłania do odbiorcy po resecie. Nie tworzyć 4×32 B dodatkowych
  buforów bez policzenia SRAM i mierzalnego zysku.
- [ ] Potwierdzić na sprzęcie każdą przyszłą optymalizację przed
  rozważeniem kolejnego przeniesienia jej do `main`.

## Automatyczne wygaszanie lawiny Go-back-N — `experiment/pp-window-drain`

- [x] Test sprzętowy na `pp-rekey-replay`: po otwarciu B przy pracującym
  A utrzymywało się ~9 kB/s (`B ret≈378/s`, `A bad≈378/s`). Bez zmiany
  SID i bez resetu wymuszone na A `w`→~100 CPS→`s` przywracało stabilne
  ~14,8 kB/s, `ret=bad=0`.
- [x] W przenośnej warstwie Go-back-N dodano stan *drain window*:
  po NACK (`URGENT`), timeoutcie potwierdzenia lub rekey z zaległymi
  ramkami nie dopuszczamy **nowych** segmentów, dopóki wszystkie
  poprzednie nie zostaną potwierdzone. Retransmisje, IDLE i ACK nadal
  pracują; zapisane bajty i kolejność są zachowane. Po zakończeniu
  wraca natychmiast pełne okno4, bez stałego opóźnienia lub redukcji RF.
- [x] Test regresyjny `pp_recovery_drain_test`: **najpierw FAIL** na
  poprzedniej gałęzi (przedwczesny DATA3 zamiast IDLE), po poprawce PASS;
  potwierdza dostarczenie pełnych 120B i automatyczne wznowienie źródła.
- [x] `sh tests/run_host_tests.sh`: **23/23 PASS**.
- [x] **Rzeczywisty test dwóch RF-Nano po wgraniu `pp-window-drain`:**
  start B→A, samotny reset B i samotny reset A kończą się zgodnym
  SID na obu terminalach, stabilnym ~14,7 kB/s oraz `ret=bad=0`.
  Żaden z trzech przypadków nie wymaga ręcznego `w/s`; chwilowe stany
  `cold/uncertain` podczas negocjacji nie utrzymują się jako plateau.
- [x] **Obie płytki w rzeczywiście metalizowanych torebkach ESD:**
  `sid=53023`, `up=1`, ~14,4–14,7 kB/s; na A `arc≈9–43` (sprzętowe
  retry), `ret=bad=0`. Na B przejściowe `link=2` (SUSPECT) i powrót
  `link=1` (HEALTHY), `warm=1` bez zmiany SID. To częściowe osłabienie
  RF, a nie potwierdzony pełny zanik łącza.
- [ ] Udokumentować wynik F7/AVR-GCC (kompilator niedostępny lokalnie)
  oraz osobno przetestować re-open portu B przy stale otwartym A,
  w obu kolejnościach A→B i B→A, odczekując ≥15 s po nowym SID.
- [ ] Przebadać workiem metalizowanym ponowny pełny strumień po
  krótkim i długim osłabieniu **aż do rzeczywistej utraty odbioru**,
  osobno utrata zasilania i reset obu. Rejestrować te same liczniki,
  bez dokładania diagnostyki do PHY. Samo zamknięcie w torebkach
  nie przerwało transmisji, więc ten przypadek pozostaje otwarty.

## Korekta częściowego ACK podczas powtórek po rekey — `experiment/pp-rekey-replay`

- [x] Po wgraniu `experiment/pp-recovery-ack-fix` użytkownik sprzętowo
  potwierdził ~14,8 kB/s i `ret=bad=0` po resecie B i obu urządzeń.
  **Reset A nadal powodował stan ~9 kB/s, `B ret≈378/s`, `A bad≈378/s`.**
- [x] Znaleziono błąd niezależny od PHY: `acceptAck` kasował wszystkie
  niewysłane elementy planu replay już po *częściowym* ACK. Po restarcie
  A B mogło w ten sposób przeskoczyć z DATA0 na DATA4, pomijając 1–3.
- [x] Nowy test `pp_partial_ack_rekey_test.cpp` **najpierw padał** na
  starym kodzie w chwili `DATA4` zamiast `DATA1`; po poprawce kursora
  wszystkie **22** testy C++11 przechodzą. Rozszerzono też symulację
  pełnego obciążenia o reset *A* z ocalałym B.
- [ ] Zbudować AVR przez F7 i ręcznie wgrać F12 na **obie** płytki.
- [ ] Powtórzyć A→RESET, B→RESET, reset obu przy pełnym obciążeniu:
  obserwować 15 s po nowym SID; trwała wartość `ret` lub `bad` powinna
  być bliska zeru, docelowo 14–15 kB/s. W razie kolejnej degradacji
  zachować 20–30 linii z obu portów oraz kolejność resetów.
- [ ] Dopiero po fizycznej weryfikacji decydować o przeniesieniu
  mechanizmu do produkcyjnego PP; poprzednie branche zostają jako
  porównawcze, bez dopisywania statystyk do transmisji.

## Test warstwy transportowej po resecie B — `experiment/pp-recovery-ack-fix`

- [x] Zidentyfikowano dwa miejsca wzmacniające problem w przenośnym
  `UartReliableStream`: ponowienia DATA przy `RX_CREDIT=0` oraz brak
  szybkiej informacji o luce, pomimo stale powracającego `RX_LAST`.
- [x] Przy zerowym kredycie DATA/replay czeka w istniejącym cache;
  IDLE nadal przesyła ACK i kredyt zwrotny.
- [x] Wykorzystano istniejący bit `URGENT` w MAC jako prośbę o ponowienie
  od `RX_LAST+1` przy luce odbiorcy; ograniczenie do jednego wyzwolenia
  na konkretny brakujący segment, aby opóźnione ACK Payload nie tworzyły
  nowych burz retransmisji. Nie dodano bajtów do 8B nagłówka/24B BODY.
- [x] Lokalne testy C++11: luka 1. pakietu po rekey, utrata środkowego,
  powtórzone NACK, zerowy kredyt i wznowienie bez utraty bufora,
  B resetowany przy stałym nasyceniu A. `sh tests/run_host_tests.sh` PASS.
- [ ] Kompilacja AVR-GCC (lokalnego `avr-g++` brak); F7 i ręczne F12 na
  **obu** płytkach. Zestawić `A up=1`, `B up=1`, identyczny SID.
- [ ] **Sprzętowy odbiór poprawki**: bez ekranowania odczekać ≥10 s
  na ~14–15 kB/s, resetować TYLKO B z A0=3,3V, potem odczekać ≥15 s.
  `A ret` i `B bad` powinny spaść do wartości bliskich zera, `cps`
  powinno wrócić w okolice początkowego tempa. Osobno reset A
  (A0=GND), potem oba. Porównać obie strony, nowe `sid`, `up=1`.
- [ ] Jeśli 5,5 kB/s i `ret≈bad≈230/s` pozostaną, zebrać obydwa
  zrzuty po 10–15 s z `A ack/win` i `B lg/ls/rf/af`. Wtedy izolować
  sprzętową kolejkę ACK Payload nRF24; hostowy model nie odwzorowuje
  jej rzeczywistych wyścigów czasowych.

## Wznowienie PP v2 — gałąź `experiment/pp-recovery-v2`

- [x] Oddzielono milczenie radia od potwierdzonej zmiany tożsamości sesji:
  `PpRecoveryControl` (3 interwały → SUSPECT, 1 s → OFFLINE,
  5 s → nowa negocjacja), stan przenośny i symetryczny dla A/B.
- [x] Zatrzymano producenta A i DATA/replay na czas niepewnego łącza;
  nie kasować SID, okna ani bufora przy chwilowym zaniku.
- [x] Dwa lokalne bajty `TX_STATUS`/`RX_STATUS`, nie w ramce RF;
  przy zimnym wznowieniu raportować niejednoznaczność dostarczenia.
- [x] Hostowa regresja warm A/B, resetu jednej i obu stron, długiej ciszy,
  overflow kolejki, rollover zegara i decyzji hosta o retransmisji.
- [ ] **Kompilacja AVR-GCC F7** i wgranie **ręczne F12** na obie RF-Nano.
- [ ] **Fizyczny test:** zasięg traci B/A na 0,2–1 s (SID bez zmiany),
  potem >5 s (nowy SID), potem odłączone zasilanie A/B osobno i obu.
  Rejestrować `link`, `warm`, `cold`, `exp`, `uncertain`, `cps`, `ret`,
  `arc/max`, `sid`. Procedura w `pp-recovery-v2.md`.
- [ ] Przed wdrożeniem produkcyjnym podłączyć lokalne statusy do nowego
  hosta (atomowe przyjmowanie) i ustalić potwierdzanie `UNCERTAIN` oraz
  kontrakt *at-least-once* vs. jawne rozliczenie aplikacyjne.
- [ ] Rozstrzygnąć sprzętowo źródło histerezy 4,7 kB/s↔15 kB/s; zmiana
  recovery nie jest jej udowodnioną naprawą.

> **Eksperyment, nie plan produkcyjny:** na gałęzi
> `experiment/pp-echo-ramp-2026-11-04` znajduje się tymczasowy wariant
> echo/ramp A→B→A, obecnie rozszerzony do pomiaru RADIO_STRESS w pamięci
> z raportami ASCII co 1 s na obu UART. Instrukcja: `echo-ramp-test.md`.
> [x] Fizycznie potwierdzony wzrost odbioru echa do ~12 000 CPS
> (RealTerm COM10, 115200 8N1); to granica pomiaru UART, NIE radia.
> [x] Wariant saturation bez kopiowania payloadu na UART, miernik CPS,
> ARC/MAX_RT, retry aplikacyjne, duplikaty, błędy i renegocjacje; host PASS.
> [ ] Test fizyczny do nasycenia radia (A/B CPS), osłanianie workiem,
> przejście do utraty łączności i automatyczne odtworzenie.
> [ ] Po próbie przywrócić `main` i ręcznie F12 wgrać bazowy PP na oba MCU.

> [x] Przygotowano test rozróżniający zimny start ~4700 CPS i stan
> ~12 552 CPS po zwolnieniu/przyspieszeniu źródła: dodatkowe liczniki
> ACK / okna A, sekwencji MAC i próbek FIFO B, bez dodatkowego SPI.
> [ ] **Najbliższy pomiar sprzętowy:** nowy HEX na obie płytki, 5–10
> linii A+B po zimnym starcie i 5 po cyklu `w`→`s`, zgodnie z
> `echo-ramp-test.md`. Nie zmieniać algorytmu PP do zlokalizowania
> przyczyny; obecnie niska szybkość nie jest udowodnionym limitem PHY.

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
