# TODO — aktywny transport dwukierunkowy UART / ACK Payload

## Priorytet 2026-11-06 — przyczyna MAX_RT i overflow B

- [x] Analiza SID 7556, kodu i LSS/MAP: `pp-duplex-analysis.md`.
- [x] Chronologia pierwszych obserwacji błędów i pauz APP_ACK,
  przenośna logika z regresjami, bez instrumentowania ISR.
- [x] CI #42144837039 PASS: 35 host, porównywarka i trzy AVR.
  SRAM: diagnostic 1253 B, clean 1001 B, synthetic 1439 B.
- [ ] F7/F12 obu, komplet raportów przy 0,20 i 0,15, później 0,0;
  ustalić porządek zdarzeń na każdej płytce, stan okna i kredytu
  przy overflow. Kolejność w jednej 10ms próbce nierozstrzygnięta.
- [ ] Rozdzielić przyczynę sprzętowych MAX_RT: testy jednego kierunku
  i obu, następnie pojedynczo zmieniany dystans/moc/kanał/SPI.
- [ ] Dodać model wejścia UART według czasu, niezależny od tempa RF,
  z rzeczywistą semantyką trzech pozycji ACK FIFO i powtórek ESB.
- [ ] Usunąć statyczną pamięć skonstruowanych, wyłączonych sond
  (190 B w dostarczonym MAP), zachowując osobne buildy diagnostyczne.
- [ ] Cel pozostaje otwarty: 11520 B/s w obu kierunkach byte-for-byte,
  latencja małych wiadomości oraz recovery na fizycznym sprzęcie.


## Test rzeczywistego pełnego dupleksu UART — 2026-11-05, `testy`

- [x] Przełączono domyślne źródło/sink testowy na przezroczysty
  `PpUartBridgePort` bez dodatkowych buforów i bez drukowania
  raportów do binarnego UART. A i B korzystają z identycznego
  adaptera; PP, ACK Payload, SID, WINDOW4, retransmisje i radio
  2 Mb/s pozostają bez zmian.
- [x] Zachowano tryb syntetycznego benchmarku jako parametr kompilacji
  `PP_TEST_TRAFFIC_MODE`; nie stworzono kolejnego brancha.
- [x] Przygotowano test hostowy: niezależne źródła danych na A i B,
  bajty `00`, `FF`, `s`, `w`, straty radiowe, opóźniony ACK,
  porównanie strumieni i atomowe `writeAll`.
- [x] Przygotowano generator i weryfikator plików:
  `tools/duplex_uart_vectors.py` oraz procedurę
  `pp-uart-full-duplex-test.md`.
- [ ] **Dwa rzeczywiste RF-Nano**: F7/F12, osobny COM dla A/B,
  oba Capture w trybie raw, równoczesny `Send File`,
  `Delays=0,0`, porównanie plików A→B i B→A.
- [ ] Po pełnym pozytywnym porównaniu: reset A/B, tłumienie RF,
  duże pliki i obciążenie z wejściem ~11520 B/s na stronę;
  monitorować RX full/DOR0 przez debugger lub osobne łącze.
  Bez RTS/CTS nie deklarować gwarancji przy dowolnym przeciążeniu.


- [x] CI [#42075780034](https://github.com/analityk/GPT_NRF24/actions/runs/42075780034):
  32 testy hostowe PASS, generator/porównanie dwóch plików i wykrycie
  celowo wprowadzonego uszkodzenia PASS; AVR UART BRIDGE i SYNTHETIC
  RADIO PASS; SRAM odpowiednio **961 B** i **1139 B**.

## Jednoogniskowa sonda i źródło danych — 2026-11-05, `testy`

- [x] Zidentyfikowano powtarzalny punkt z pomiarów na płytkach:
  18,7–18,9 kB/s, `cyc≈1255µs`, `spi≈86µs`, `ack≈197µs`,
  `src≈161µs`, `enc≈51µs`; bez trwałych `ret/bad`.
- [x] Wyłączono wielofazową sondę czasu w aktywnej konfiguracji;
  dodano izolowany pomiar `A hot` (2 odczyty 16-bitowego TCNT1)
  i bezpieczne uśrednianie poza gorącą sekcją.
- [x] Usunięto redundantną kopię BODY w źródle `UartReliableStream`:
  jeden przebieg zapisuje bajt w cache retransmisji i w ramce;
  zachowano sloty i semantykę Go-back-N.
- [ ] **Test dwóch urządzeń:** F7/F12 obu, A/B `cps` i `A hot`,
  samodzielne resety A i B, raporty `ret/bad`, osłabienie RF;
  dopiero po próbie rozważyć dalsze optymalizacje.

- [x] CI commita `0ec12ca`: 31/31 testów hostowych PASS, AVR-GCC PASS, `.text=19214`, `.data=96`, `.bss=1043`, SRAM **1139/2048 B** ([Actions #42058415426](https://github.com/analityk/GPT_NRF24/actions/runs/42058415426)).

## Szybka ścieżka SPI i odroczony ACK — 2026-11-05, `testy`

- [x] Ustalono granicę sprzętową: z `W_TX_PAYLOAD` nie odczytuje się
  jednocześnie RX FIFO. Komenda `R_RX_PAYLOAD` wymaga osobnej transakcji;
  odczyt w czasie zapisu to STATUS/odpowiedź tej komendy, nie poprzedni
  payload. Pozostawiono SPI polling i fizyczną kolejkę ACK Payload.
- [x] `SpiBlockTransfer<Bus>` ma specjalizowane poza pętlą kierunki
  TX/RX, 8-bitowy limit 255 odpytań na bajt i inlinowane rejestry AVR.
  Obsługa awarii pozostaje ograniczona; hostowy test obejmuje pełne
  bloki, zerowe bufory, brak master oraz timeout komendy/danych.
- [x] Na A dodano jednopozycyjną skrzynkę metadanych otrzymanej ramki:
  po pełnym TX można uruchomić **następną** wymianę, a poprzedni
  prawidłowy ACK DATA/IDLE tego samego SID przetwarzać podczas jej
  `ON_AIR`, zanim SPI mogłoby nadpisać bufor RX. CONTROL, zły SID i
  błędna ramka są rozliczane natychmiast przed nowym TX. Bez kolejnych
  buforów 4×32 B i bez zmiany transportowego WINDOW4.
- [x] Rozszerzono pomiar o `A det prf=`: część protokołu równoległa do
  `rf`; `rest` odlicza tylko protokół spoza `ON_AIR`. Testy obejmują
  zachowanie skrzynki, strumień dwukierunkowy z stratami, timeout SPI
  oraz niewliczanie równoległego czasu do wyłącznego CPU.
- [x] GitHub Actions run #42004061533 dla `7915019`: host i AVR
  **PASS**, `.text=24434`, `.data=96`, `.bss=1034`, statyczny
  SRAM **1130/2048 B** (o 6 B więcej od poprzedniego CI).
- [ ] **Pełny test sprzętowy**:
  F7/ręcznie F12 na A i B, RealTerm `cps`, `A us/det`, `sid/up/ret/bad`,
  samodzielny reset A i B, osłabienie RF. Punkt odniesienia przed tą
  optymalizacją: 16 704 B/s przy `cyc≈1437`, `spi≈116`, `ack≈230`,
  `ps≈206` µs, `ret=bad=0`. Nie ogłaszać zysku bez porównania.
- [ ] Jeśli opóźniona informacja ACK pogorszy Go-back-N/rekey,
  przeanalizować logi i poprawić sterowanie w istniejącym `testy`;
  zachować zaliczony `main` oraz wszystkie gałęzie.

## Optymalizacja GPIO/CSN — 2026-11-05, `testy`

- [x] Rozdzielono konfigurację wyjścia (`GPIO::outputHigh/Low`) od
  szybkich zmian zatrzasku PORT (`set/clear/flip`), bez `valid()` i
  ponownego zapisu DDR podczas każdego odczytu rejestru nRF. Wejście
  jest próbkowane wyłącznie przez `read()`, a przejścia open-drain
  są jawne. `Nrf24::begin()` konfiguruje wyjście, zachowując stary
  interfejs atrap Pin. Test GPIO i pozostałe 25 testów hostowych PASS.
- [x] Utrwalono styl C++ w `AGENTS.md` i `.editorconfig`:
  rzeczywisty TAB o szerokości 4, funkcje i `if` zawsze blokowe,
  pusta linia po funkcji, parametry liczbowe wyłącznie nazwane.
- [x] Cross-build AVR na `testy`: GitHub Actions run #41995708054,
  host i AVR **PASS**, `.text=23712`, `.data=96`, `.bss=1028`.
- [x] Właściciel zmierzył na dwóch RF-Nano, SID 44831, około
  **16 704 B/s**, `cyc≈1437`, `spi≈116`, `ack≈230`, `ps≈206` µs,
  `ret=bad=0`; wcześniej przy szczegółowej sondzie było 15 888 B/s,
  `cyc≈1505`, `spi≈123`, `ack≈262`, `ps≈251` µs. Jest to pozytywny
  pomiar całego firmware po zmianie GPIO, a nie izolowany benchmark GPIO.
  Próby resetu i tłumienia dla tej konkretnej wersji nadal otwarte.

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
- [x] **Sonda zmierzona na dwóch RF-Nano przy 2 Mb/s:**
  `cyc≈1567 µs`, `spi≈180 µs`, `ack≈299 µs`, `rf≈610 µs`,
  `cpu≈430 µs`, około 15,3 kB/s, `sid=22818`, `ret=bad=0`.
  Odczyty `mk≈208 µs`, `pr≈133 µs`, `bg≈176 µs`.
  Wynik i ograniczenia interpretacji zapisano w `history.md`.
- [x] **Pierwsza optymalizacja SPI na `testy`:** blokowe przesłanie
  komendy i danych w `SpiMaster`, bez wywołania funkcji dla każdego
  bajtu; limit czasu pozostał osobny dla każdego bajtu. Przenośny
  `Nrf24` automatycznie używa nowej metody albo starego API adaptera.
  Testy z mockiem obu ścieżek i awarii CSN, **25/25 host PASS**.
- [ ] **Dokończyć fizyczną weryfikację blokowego SPI:**
  prędkość i brak błędów w ustalonym łączu potwierdzone poniżej,
  ale nadal sprawdzić samodzielny reset B i A oraz odtworzenie
  `sid/up/ret/bad` pod obciążeniem i w osłabionym RF.
  Przyspieszenie SPI nie może pogorszyć odzyskiwania sesji.
- [x] **Blokowe SPI: pozytywny pomiar dwóch RF-Nano:**
  `spi≈123`, `ack≈227`, `cyc≈1427` µs, ~16 824 B/s, `ret=bad=0`.
  Około 140 µs/cykl szybciej niż sonda przed optymalizacją.
  AVR CI run #96 PASS; `.text=20200`, `.data=96`, `.bss=939`.
- [x] **Przygotowano drugi poziom sondy:** `A det` rozbija obsługę ACK
  (`OBSERVE_TX`, FIFO, DPL, IRQ), liczy liczbę i czas pollingów STATUS
  oraz dzieli `mk` na źródło/cache i kodowanie. Rozdziela rozłączne
  okresy od tych, które nakładają się na pracę radia. Host 25/25 PASS.
- [ ] **Rano zmierzyć nową sondę na dwóch RF-Nano** bez dodatkowych
  zmian protokołu: 5–10 par `A us` i `A det` z dobrego RF, razem z
  `A cps` i `B cps`. Sprawdzić `sid`, `up`, `ret`, `bad`, `arc` i
  powrót po resecie B/A. Wynik `cyc` może być nieco większy z powodu
  dodatkowych odczytów Timer1; to koszt diagnostyki, nie regresja PHY.
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

## Uproszczenie ścieżki TX, 2026-11-05 (`testy`)

- [x] Jeden ACK-retained ring na fizycznym RX UART, 256 B, wskaźniki atomowe 8-bitowe i deskryptory 4 ramek; zwalnianie miejsca wyłącznie po ACK aplikacyjnym, bez zerowania RAM.
- [x] Serializer odczytuje BODY do bufora ramki SPI bez drugiej kopii, działa przy zawinięciu indeksu.
- [x] Osobny test źródła dla generatora; ten sam algorytm ramek i retransmisji w obu trybach.
- [x] Histereza `pauseHostInput`: mniej niż 2×24 B wolnych oznacza STOP, po odzyskaniu ≥4×24 B oznacza RESUME; bez XON/XOFF w danych.
- [x] Skrócono sekcję atomową `UART::writeAll` do przełączenia UDRIE.
- [ ] Zmierzyć `rx_buffer_full/rx_overrun` po równoczesnym wysłaniu 255-bajtowych linii bez przerw wewnątrz linii, oraz porównać binarne capture obu portów.
- [ ] Potwierdzić osobny reset A i B, przekroczenie indeksu 255, długie retransmisje pod tłumieniem RF i brak danych IDLE przy aktywnym kanale.
- [ ] Jeśli pełne 115200 8N1 z obu hostów nie mieści się w przepustowości RF, podłączyć fizyczny flow control do `pauseHostInput` albo użyć nadawcy respektującego dwa bajty statusowe.

### Weryfikacja implementacji retained ring — 2026-11-05

- [x] Zmodyfikowano oczekiwania dwóch starszych testów, które zakładały opróżnianie `Port::input` zamiast przechowywania danych w nowym wspólnym pierścieniu; przy tym zachowano weryfikację liczby niesłanych bajtów.
- [x] Włączono poprawny nagłówek `RetainedByteRing` w adapterze mostu; obie kompilacje AVR PASS.
- [x] Test `uart_unthrottled_ingress_test` używa bezpośrednio współdzielonego retained ring jak UART na sprzęcie, a nie dwóch buforów pośrednich: 19 B/wymianę bez strat, 30 B/wymianę z wykrytym przepełnieniem.
- [x] Dodano `pp_uart_burst_retained_test`: jednoczesne 255 B A→B oraz B→A, następna porcja 255 B z zawinięciem indeksów, byte-for-byte, pełne ACK, aktywne IDLE po opróżnieniu.
- [x] CI [#42101240883](https://github.com/analityk/GPT_NRF24/actions/runs/42101240883): **34/34 testy hostowe PASS**, oba firmware AVR PASS. Bridge: 12724 B Flash, 1001 B statycznego SRAM; Synthetic: 19550 B Flash, 1439 B SRAM.
- [ ] Test dwóch fizycznych płytek: równoległy `Send File` bez opóźnień, capture w obu kierunkach i `verify`, następnie 255-bajtowe linie, resety A i B oraz tłumienie RF. W razie różnic odczytać `rx_buffer_full` i `rx_overrun` bez domieszki diagnostyki w UART.

## 2026-11-05 — jednoogniskowy pomiar przy dwóch wejściach UART

- [x] Włączono przełączalny build diagnostyczny `PP_UART_DIAG_ENABLED=1` dla BRIDGE. Normalne pakiety nie są zmienione; ASCII dopisuje się dopiero po 0,8 s bez danych/postępu.
- [x] 16-bitowy Timer1 mierzy **tylko start→start A** dla udanej wymiany z DATA w przynajmniej jednym kierunku, bez ARC; wynik `A rf n/cyc/lo/hi` w µs.
- [x] Oba UART mają osobne 16-bitowe liczniki przyjętych bajtów i rzeczywistych zapisów do UDR0. Raporty `A/B uart` (in/out/full/dor/fe/free) i `A/B pp` (ack/rx/ret/blk/bad/win/cr/sid) pozwalają zlokalizować granicę strat. Jedna 10ms przerwa po CRLF nie generuje raportu.
- [x] Dodano test portable ciszy, powtarzanych przerw 10ms, overflow16 i długości tekstu. Pierwsze CI [#42108719539](https://github.com/analityk/GPT_NRF24/actions/runs/42108719539): **35/35 testy hostowe PASS**, oba AVR PASS, BRIDGE statyczne SRAM 1107B, FLASH 15884B.
- [ ] Wykonać 2×RealTerm bez zwory TX–RX, `Delays=0,20` i `0,10`, odczytać komplet pięciu linii diagnostyki, porównać A/B osobno.
- [ ] Po zakończeniu pomiarów wyłączyć `PP_UART_DIAG_ENABLED` do powrotu do binarnego byte-exact capture i ocenić konieczność modyfikacji konkretnego wąskiego gardła.

- [x] Rozdzielono retransmisje PP od sprzętowego ARC: `A pp hw/mx` i `B pp af/rf` są dopisywane do istniejącego raportu po ciszy. Nie zmieniono PHY, protokołu ani momentu emisji.
- [ ] Próba `Delays=0,20` i `0,15`: porównać `A hw/mx` z `B ret` i `B af/rf`. Przed optymalizacją Go-Back-N ustalić, czy w ogóle istnieje rzeczywista strata radiowa.
