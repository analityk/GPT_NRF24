**NAJNOWSZY PUNKT KONTYNUACJI: 2026-11-05 — GPIO/CSN na `testy` zoptymalizowane, pomiar sprzętowy oczekuje.**
Po przejrzeniu plików `.lss` i `.map` z Microchip Studio
zidentyfikowano wielokrotny `GPIO::valid()` i zapis DDR przy każdym
CSN HIGH/LOW. Na żądanie właściciela `GPIO` ma teraz osobne
`outputHigh()/outputLow()` do jednorazowego skonfigurowania wyjścia
bez impulsu przejściowego, a `set()/clear()/flip()` wykonują tylko
operację PORT bez sprawdzania wskaźników i kierunku. Konstruktor
domyślny usunięto; niepoprawna konfiguracja rejestrów jest błędem
programisty. `release()/i2c_set()/i2c_clr()` jawnie przełączają tryb,
`read()` tylko odczytuje PIN; zbędne `readOpenDrain/readPullUp` usunięto.
`Nrf24::begin()` wykorzystuje nową konfigurację, zachowując zgodność
ze starszymi atrapami Pin. `main` inicjalizuje CE LOW i CSN HIGH
przed startem radia. Testy hostowe **26/26 PASS**, GitHub Actions
run #41995708054: host i AVR **PASS** (`.text=23712`, `.data=96`,
`.bss=1028`). Test dwóch RF-Nano tej zmiany **jeszcze niepotwierdzony**.
W `AGENTS.md` oraz
`.editorconfig` zapisano obowiązkowy TAB 4 kolumny, blokowe funkcje
i instrukcje sterujące oraz zakaz magicznych liczb. Przed kolejną
optymalizacją SPI porównać sprzętowy `cps`, `ps` i `spi` do `9cf4da6`.
Nie zmieniono protokołu, radio nadal 2 Mb/s, `main` (branch) nietknięty.

**Poprzedni punkt kontynuacji: 2026-11-05 — blokowe SPI potwierdzone sprzętowo, szczegółowa sonda na `testy` do sprawdzenia rano.**
Właściciel uruchomił dwa RF-Nano z blokowym SPI na 2 Mb/s i
potwierdził ~16 824 B/s, `cyc≈1427`, `spi≈123`, `ack≈227` µs,
`sid=6380`, `ret=bad=0`. Poprzednio było ~15 336 B/s, `cyc≈1567`,
`spi≈180`, `ack≈299` µs: rzeczywisty zysk to ~140 µs/cykl.
GitHub Actions #96 potwierdził AVR i host PASS (`.text=20200`,
`.data=96`, `.bss=939`, SRAM=1035 B). Na prośbę właściciela
rozszerzono opcjonalne, przenośne sondy: PHY mierzy polling STATUS
i rozkład odczytu ACK, `UartReliableStream::makeTraced` mierzy
pracę źródła/cache i kodowanie. UART A wypisuje obok `A us` drugi
wiersz tej samej próbki `A det` z `polls/ps/ob/fi/dpl/irq/ro/src/enc/mo/rest`.
Znaczenie i ograniczenia: `pp-cycle-timing-test.md`. `ps` należy do
`rf`, a nowa sonda może sama spowolnić cykl. Host 25/25 PASS;
**nowa szczegółowa sonda nie ma jeszcze potwierdzenia AVR ani pomiaru
na płytkach**. `main` i wszystkie gałęzie bez zmian.

**Poprzedni punkt kontynuacji: 2026-11-05 — pomiar Timer1, blokowe SPI do weryfikacji.**
Właściciel wgrał sondę na dwie RF-Nano: przy 2 Mb/s i SID=22818,
`ret=bad=0` oraz około 15,3 kB/s wyniki Timer1 to
`cyc≈1567`, `spi≈180`, `grd≈16`, `ce≈28`, `rf≈610`,
`ack≈299`, `cpu≈430`, `mk≈208`, `pr≈133`, `bg≈176` µs.
To **rzeczywisty pomiar starszej implementacji SPI**, nie nowej
optymalizacji. Samo SPI zapisu i odczytu ACK pochłania około 479 µs.
Po zgodzie właściciela na `testy` dodano `SpiMaster::transferBlock()`
oraz kompilacyjny wybór tej metody w `Nrf24`; starszy jednobajtowy
interfejs i mocki nie są łamane. Nowy transfer nie potrzebuje ISR,
bufora ani dodatkowego protokołu, utrzymuje osobny timeout na bajt
i sprzętowe 8 MHz. Host **25/25 PASS**. Następny krok: wynik AVR CI,
następnie F7/ręczne F12 na obu płytkach i porównanie
`spi/ack/cyc/cps` ze wskazaną bazą, również po resetach A/B.
**Brak jeszcze zmierzonego zysku blokowego SPI; `main` bez zmian.**

**Poprzedni punkt kontynuacji: 2026-11-05 — sonda Timer1 na `testy`, przed pierwszym pomiarem.**
Właściciel potwierdził na RF-Nano nieblokujący PP: około 13,3 kB/s
przy 1 Mb/s i po ręcznej zmianie około 16,1 kB/s przy 2 Mb/s,
`ret=bad=0`, reset B powraca prawidłowo. To obserwacje **sprzed
instrumentacji**. Na wyraźne polecenie opracowano pomiar na A
przez opcjonalne, wielokrotnego użytku `NrfAckPayloadPhy::Timing`
oraz przenośne `PpCycleTimingWindow`: `spi/grd/ce/rf/ack` dla PHY,
`cyc/cpu` dla całej wymiany i `mk/pr/bg` dla składania pakietu,
protokołu oraz tła. Sonda używa Timer1 0,5µs, uśrednia 32–1024
pełnych DATA↔ACK 32B bez retry w zdrowej sesji i dopisuje tylko na
UART A odrębne linie `A us ...` obok istniejącego CPS. `rf` mierzy
czas do **wykrycia** statusu SPI, nie czystą fizyczną transmisję;
`bg` nakłada się na ten odcinek. Dla `testy` ustawiono jawnie
`NRF24_RF_SETUP_2_MBPS` na A i B (domyślne `Nrf24` nadal 1 Mb/s).
Testy hostowe **25/25 PASS**. Szczegółowe znaczenie pól i procedura
F7, ręczne F12, RealTerm, reset B/A, torebki i test bez sondy:
`pp-cycle-timing-test.md`. GitHub Actions run #93 dla `testy`
potwierdził host i AVR-GCC **PASS** (`.text=20430`, `.data=96`,
`.bss=939`, SRAM=1035/2048 B). **Nie ma jeszcze wyników mikrosekundowych
ani fizycznego testu tego commita**. `main` i wszystkie wcześniejsze
gałęzie pozostawione bez zmian; żadnego nowego brancha.

**Poprzedni punkt kontynuacji: 2026-11-05 — nieblokujący PHY na `testy`, oczekuje na odbiór sprzętowy.**
Po zgodzie właściciela na dalsze prace, w `testy` wdrożono pierwszy
etap kooperacyjnego pipeline PP: `NrfAckPayloadPhy::startTransfer()`
i `pollTransfer()` zastępują blokujące `transfer()` na A. Stany:
`IDLE`, `GUARD`, `CE_PULSE`, `ON_AIR`; Timer1 kontroluje 130µs po
poprzednim zakończeniu, impuls CE≥15µs i timeout 8ms, pojedynczy krok
`pollTransfer` nie wykonuje oczekiwania w pętli. P1 przetwarza stan
nRF i pobiera ACK, P2 raz podczas `ON_AIR` aktualizuje recovery,
ograniczony do 24B generator i diagnostykę. Na B pozostała już
nieblokująca kolejka ACK i priorytet odbioru FIFO. Zachowane zostały
window4, SID, Go-back-N, kredyty, drain/replay, 8+24B, 1 Mb/s,
bez nowego RAM cache i bez przerwań SPI. **Nie przygotowujemy następnej
ramki spekulacyjnie przed odebraniem poprzedniego ACK**, bo obecne
`stream.make` zużywa dane i modyfikuje okno — wymagałoby osobnego
transakcyjnego API i prób przy rekey.
`sh tests/run_host_tests.sh`: **24/24 PASS**, w tym nowe testy etapów
CE/status/timeout/timer wrap oraz porcjowania producenta. GitHub Actions
run #91 na `testy` potwierdza **PASS host i AVR-GCC** dla commita
`7829482`; `.text=17740`, `.data=96`, `.bss=878` bajtów, statyczny
SRAM=974/2048 B. Kompilatora AVR-GCC nie ma lokalnie.
Następny krok: F7, ręczne F12 obu, pomiar
`cps/arc/max/ret/bad/sid/up` w dobrym zasięgu, po resecie A/B/obu,
pod ekranowaniem i porównanie z zaakceptowanym `main`. **Nie scalać
eksperymentu do `main` bez wyraźnej akceptacji właściciela po wynikach.**

**Poprzedni punkt kontynuacji: 2026-11-05 — pierwszy zaakceptowany punkt PP.**
Właściciel zaakceptował `experiment/pp-window-drain` jako pierwszą
technicznie działającą wersję PP do przeniesienia do `main`.
Wersja kodu firmware (przed późniejszymi zmianami tylko dokumentacji):
GitHub `ff087b5f3640e9f606b4db8d25f4d02dc7a8ee27`.
Wszystkie 23 testy hostowe PASS; fizycznie potwierdzono start B→A,
samotny reset B i samotny reset A z powrotem do ~14,7 kB/s i zerowymi
`ret/bad`, a także ciepłe wznowienie bez zmiany SID podczas osłabienia
łączności metalizowanymi torebkami. Nie potwierdzono binarnej
bezstratności UART ani dokładnie jednokrotnego doręczenia przez reset;
lokalne próby zmiany szybkości RF i odstępu do 1300 taktów nie są
częścią zaakceptowanego kodu repozytorium.
Do dalszej pracy właściciel wskazał **jedną gałąź `testy`**, utworzoną
od zaakceptowanego `main`; wcześniejszych branchy nie usuwać.
**Zatrzymać się po uporządkowaniu repozytorium.** Dopiero po następnym
poleceniu właściciela rozwijać na `testy` nieblokującą maszynę stanów
PHY, z zachowaniem cache czterech segmentów, warstwy ACK i testów
odzyskiwania.

**Poprzedni punkt kontynuacji: 2026-11-04, test fizyczny `experiment/pp-window-drain`.**
Właściciel uruchomił rzeczywiste RF-Nano kolejno B→A, następnie osobno
zresetował B i osobno A. Na trzech zrzutach RealTerm obie strony
uzgadniają wspólny SID i wracają do ~14,7 kB/s z `ret=bad=0`:
po starcie `sid=49792`, po resecie B `sid=52044`, po resecie A
`sid=53023`. Podczas resetów występują przejściowe `cold` i
`uncertain`, ale nie pozostaje trwały stan ~9 kB/s ani potrzeba
ręcznego `w/s`. To pozytywna walidacja *tych trzech scenariuszy*,
nie dowód bezstratności UART, exactly-once czy odporności na
długotrwały brak radia. Dodatkowa próba obu płytek w metalizowanych
torebkach ESD utrzymała `sid=53023`, `up=1`, ~14,4–14,7 kB/s i
`ret=bad=0`, lecz A rejestrowało sprzętowe `arc≈9–43`, a B chwilowo
przeszło `link=2` (SUSPECT)→`link=1` (HEALTHY), `warm=1`.
To dowodzi ciepłego powrotu po krótkiej przerwie poprawnych ramek,
nie pełnej utraty zasięgu; brak pomiaru tłumienia i szczelności osłon.
Otwarte: re-open B przy działającym A, kontrolowana dłuższa utrata
zasięgu i zapis wyniku kompilacji AVR-GCC/F7.

**Poprzedni punkt kontynuacji: automatyczne wygaszanie lawiny.**
Na płytkach wcześniejszy kod po samotnym ponownym uruchomieniu B mógł
wejść w trwałe ~9 kB/s, `B ret≈378/s` i `A bad≈378/s`. Zmniejszenie
obciążenia `w` do ~100 CPS na 5–10 s i powrót `s` do maksimum bez
resetu, zmiany SID czy sprzętu przywróciły ~14,8 kB/s i `ret/bad=0`.
To dowodzi odwracalnej lawiny retransmisji pod obciążeniem, ale nie
identyfikuje jeszcze konkretnego zdarzenia inicjującego pierwszą lukę.

Nowa warstwa `UartReliableStream` po otrzymaniu NACK, po timeoutcie
aplikacyjnego ACK lub po nowej sesji z niepotwierdzonym cache przechodzi
w krótką fazę *drain window*: nadal ponawia DATA, niesie ACK/kredyt w
IDLE, ale **nie pobiera świeżych danych z portu**, dopóki dotychczasowe
okno nie zostanie zaakceptowane. Potem wraca do pełnego WINDOW4.
To automatyczna kontrola źródła *we właściwej warstwie transportowej*;
nie ma dodatkowych bajtów RF, ręcznych resetów ani opóźnień timerów.
Nowy `pp_recovery_drain_test` najpierw padał na poprzednim kodzie,
ponieważ świeży DATA3 przeskakiwał przed potwierdzeniem zaległych;
po zmianie wszystkie **23 testy hostowe PASS**. Brak AVR-GCC w lokalnym
środowisku. Fizyczny reset B i A pod obciążeniem został już sprawdzony
na wariancie `pp-window-drain`, bez manipulacji `s/w` (powyżej).
Wcześniejszy branch i produkcja zachowane do porównań.

**Poprzedni punkt kontynuacji: 2026-11-04, `experiment/pp-rekey-replay`.**
Fizyczna próba wcześniejszej poprawki `pp-recovery-ack-fix` pokazała
poprawny powrót ~14,8 kB/s po resecie B i obu, ale po resecie A
pozostawał stabilny stan ~9 kB/s: `B ret≈378/s`, `A bad≈378/s`.
Przyczyna zlokalizowana testem jednostkowym w `UartReliableStream`:
po przejściu do nowego SID nadajnik zachowuje 1–4 pakietów do replay,
lecz odbiór pierwszego *częściowego* ACK zerował cały plan powtórek,
pomijając jeszcze niewysłane DATA. Przykład: po ACK dla DATA0 B
zamiast odtworzyć DATA1..3 wysyłał świeży DATA4. Nowa regresja
`pp_partial_ack_rekey_test` padała przed poprawką; po zachowaniu
niepotwierdzonego ogona replay wszystkie **22 testy hostowe PASS**.
`pp_rekey_full_load_test` obejmuje teraz reset obu kierunków oddzielnie.
Nie ma nowych bajtów RF, zmian PHY/ADC/UART/main, dodatkowych liczników
ani alokacji. Lokalny AVR-GCC niedostępny, więc po F7/F12 obu modułów
**konieczny sprzętowy reset A pod obciążeniem**. Gałąź produkcyjna,
checkpoint i poprzednie warianty testowe pozostają nietknięte.

**Poprzedni punkt kontynuacji: 2026-11-04, `experiment/pp-recovery-ack-fix`.**
Odizolowana gałąź wychodzi z `experiment/pp-recovery-v2` i poprawia
transport Go-back-N po resecie B przy pełnym obciążeniu A. Dwie zmiany
przenośnego API: (1) przy zerowym `RX_CREDIT` nie wolno ponawiać
DATA — oczekujące bajty pozostają w pamięci, a IDLE nadal niesie ACK;
(2) odbiorca napotykający lukę sekwencji ustawia istniejący bit `URGENT`
w nagłówku DATA/IDLE, informując nadawcę, by bezzwłocznie ponowił od
`RX_LAST+1`. Nadawca reaguje najwyżej raz na daną lukę mimo opóźnionych
ACK Payload. **Nie ma dodatkowych bajtów radiowych**, nie zmieniono PHY,
konfiguracji 1Mb/s, programu main, A0, F7/F12, przechowywania pakietów
ani statusów hosta. Testy hostowe PASS; **wymagana kompilacja AVR oraz
fizyczny test z resetem B przy nasyconym A**. Wcześniejsze spadki do
~5,5 kB/s i `ret≈B bad≈230/s` są dowodem problemu, NIE dowodem naprawy.
`main`, `checkpoint/pp-2026-11-04` oraz poprzednia gałąź testowa
pozostają nietknięte.

**Wcześniejszy punkt kontynuacji: 2026-11-04, `experiment/pp-recovery-v2`.**
Przenośne `pp_recovery_control.h` + `host_flow_status.h` oddzielają zanik
łączności od utraty RAM partnera. Prawidłowa ramka z dotychczasowym SID
przywraca ten sam transport bez rekey; po 3×1,6 ms ciszy SUSPECT, po 1 s
OFFLINE, po 5 s nowa negocjacja. Reset partnera rozpoznaje nonce w
HELLO/BEACON; przy zmianie SID wcześniejsze niepotwierdzone dane mają
status UNCERTAIN. W **diagnostycznym** `main.cpp` ponawiamy je automatycznie
(*at-least-once*); domyślny mechanizm API wymaga decyzji hosta (*HOLD*).
`TX_STATUS` i `RX_STATUS` są **lokalne**, bez narzutu w radiu. Ostatnia
wersja testów hostowych PASS; **nie uruchomiono builda AVR ani dwóch
płytek, bo brak lokalnego kompilatora AVR i urządzeń**. Następny krok:
F7 / ręczne F12 obu RF-Nano, testy worka metalizowanego, resetu i zaniku
zasilania zgodnie z `pp-recovery-v2.md`, a potem decyzja na podstawie
logów o wdrożeniu. Pozostaw `main` i checkpoint PP nietknięte.

**Stan produkcyjny na 2026-11-04: obecny firmware to PP (Ping-Pong), nie zmieniany przy pracy nad BURST20. Fizyczny zrzut jednoczesnych strumieni poprawił się po przejściu na 8+24B i WINDOW=4; brak binary checksum capture. PP checkpoint `checkpoint/pp-2026-11-04` SHA 784fd83. BURST20 ma czysto logiczny harmonogram i boot-mode selector testowane hostowo, NIE ma jeszcze BURST PHY ani wyboru A1 w main.**

**Uwaga dla gałęzi `experiment/pp-echo-ramp-2026-11-04`:** jest to
tymczasowy wariant pomiarowy, z tym samym PP PHY/MAC/sesją/transportem,
ale z generatorem bajtów na A i lokalnym echem radiowym na B. Po
fizycznym wyniku RealTerm ~12 000 CPS (na COM10 A, nasycenie UART)
wariant eksperymentalny uruchamia od razu maksymalne obciążenie źródła,
weryfikuje echo **w RAM** i raportuje na obu UART (ASCII, 1 Hz)
`cps/gen/fwd/arc/max/ret/dup/bad/blk/gap/err/rec/sid/up`. Źródło może
być zwolnione przez `w` co100 µs i przyspieszone przez `s` co100 µs.
Radio nadal pracuje przy **1 Mb/s**, PP 8+24B WINDOW4/ACK Payload.
Normalny UART↔RF↔UART nadal znajduje się na niezmienionym `main`
(`6b94a43`). Szczegóły i przywracanie: `echo-ramp-test.md`.
Fizycznie wykryto **histerezę**: po pełnym obciążeniu od zimnego startu
~4700 CPS/`ret≈195`, po cyklu `w`→`s` ~12 552 CPS/`ret=0`; reset A
odtwarza wolny stan. Nowa eksperymentalna telemetria `A ack/win` oraz
`B lg/ls/rf/af` wykorzystuje istniejące liczniki i odczyt FIFO bez
dodatkowego SPI; czeka na jeden porównawczy pomiar sprzętowy. Własna
symulacja pełnego źródła nie reprodukuje problemu, przyczyna nieznana.
Nie zmieniać docelowego PP do wyniku tej diagnostyki.
Nowy test ma tylko symulację hostową, nie ma jeszcze wyników sprzętowych;
oba MCU wymagają ręcznego F12. Nie utożsamiać `gap=0` z bezstratnością
przez reset lub utratę wielokrotności256 bajtów.

# Migracja / punkt kontynuacji GPT_NRF24

**Aktualny stan: 2026-11-04 — ciągły sprzętowy ping-pong DATA↔ACK Payload, WINDOW=4 w obu kierunkach, pełne 24B danych UART na ramkę (bez preambuły BODY), DATA_SEQ/APP_ACK w MAC TX_SEQ/RX_LAST. Host i AVR-GCC PASS, fizyczny test tego nowego formatu jeszcze otwarty.**

## Źródło prawdy

- Repozytorium: https://github.com/analityk/GPT_NRF24, gałąź `main`.
- Dotychczasowy działający, kompletny stan **sprzed porządkowania**: commit `d91630509ba32fb62ab0437ff2d2b5770feae31c`, gałąź `archive/pre-paired-core-cleanup-2026-11-03`. Znajdują się tam osobny dawny UART bridge, stary v1, jego handshake, poprzedni 32+32 benchmark i testy. Nie przenosić ich z powrotem bez konkretnego wymagania.
- W bieżącym źródle pozostaje jeden program `GPT_NRF2401/main.cpp`. **Nie ma już `application_select.h`, `paired_core_main.cpp`, `ack_bench_main.cpp`, `frame_v1.h` ani trzech wariantów firmware.** Moduł `boot_epoch.cpp` **jest obecnie ponownie potrzebny**, ale w nowej architekturze: adapter EEPROM dla przenośnego `epoch_journal.h`.
- Zmiany wyników i uzasadnienia z wcześniejszych faz zachowuje `history.md`; bieżący kontrakt binarny w `paired-core-v2.md`, plan w `plan.md`, zadania w `todo.md`.

## Stałe wymagania

- RF-Nano ATmega328P 16 MHz, nRF24L01+, CH340, SPI CE PB2/D10, CSN PB1/D9, MOSI PB3, MISO PB4, SCK PB5.
- A0 GND → A/PTX; A0 dodatnie zasilanie → B/PRX. Jeden identyczny HEX na obie.
- Microchip Studio/Atmel Studio, AVR C++11, brak dynamicznej alokacji, globalny `PackStructureMembers=False`.
- Programowanie **tylko ręcznie F12** po F7; **nigdy nie przywracać automatycznego Post-Build flashowania**.
- Radio 1 Mb/s, CRC16, Enhanced ShockBurst, hardware ACK Payload, SPI 8 MHz, Timer1 0,5 µs.
- DPL: 8–32 B. Nagłówek: TX_SEQ, RX_LAST, dwa nibbles LOSS, dwa nibbles ARC_LAST/RX_DROP, RX_CREDIT, TYPE_FLAGS, SID16; BODY 0–24 B nieinterpretowane.

## Co zbadano fizycznie

W RealTerm, na dwóch RF-Nano, nowy rdzeń 8+24 osiągnął **~625 transakcji/s**: A `TX/s=625 ACK/s=625`, B `RX/s=625 Q/s=625`, obserwowane ARC/MAX/BAD/IO/GAB/GBA/DROP/FULL=0. To 15 000 B/s wzorca testowego na kierunek, okres średni 1,6 ms, a **nie latencja ani realne przesłanie UART**. Poprzedni benchmark 32+32 wykazał 540/s. Nie zmieniono trybu radiowego po każdym pakiecie: B pozostaje PRX, A PTX.

## Granice i następne zadania

- Stałe testowe `SESSION_ID=0x4E52` **zastąpiono handshake** `paired_session.h` z nonce każdej strony `boot_epoch32+attempt16`; SID16 pochodzi z CRC16 obu nonce. Warstwy sesji/PHY chronią przed typowymi starymi odpowiedziami i negocjują restart. **Zaliczone testy hostowe i cross-build AVR; brak jeszcze testu fizycznego nowej wersji.**
- `RX_CREDIT=1` jest symulacją, nie stanem prawdziwego odbiornika.
- Sprzętowy ACK potwierdza odbiór ramki przez radio, nie przyjęcie przez UART lub plik.
- Wyższe protokoły usług (TCP-like, pliki, szyfrowanie) są świadomie poza bieżącą warstwą; zachować opaque BODY.
- Następny etap: test fizyczny handshake/odtwarzania po resetach A, B i obu; wspólny niezerowy `SID` i `ST=4` w obu RealTerm. Potem włączyć rzeczywistą informację o dostępności bufora, nie pogarszając rdzenia.

## Testy i zachowanie danych

`sh tests/run_host_tests.sh` kompiluje automatycznie wszystkie aktywne `tests/*_test.cpp`. `sh tests/build_avr.sh` kompiluje jeden program dla ATmega328P bez programowania sprzętu. GitHub Actions wykonuje obie czynności. W Microchip Studio nadal F7/F12; nie mieszać hostowego PASS, krzyżowego build PASS i fizycznego testu.

Porządkowanie zmienia bieżące pliki źródłowe, **nie usuwa historii Git**; stary kod można odczytać w gałęzi `archive/pre-paired-core-cleanup-2026-11-03`. Żadna zmiana nie wymaga nazwy portu COM ani stałej ścieżki Windows.

## Nowy handshake i trwałość sesji — 2026-11-04

Zaimplementowano `GPT_NRF2401/paired_session.h` jako maszynę stanów MCU-independent dla A/PTX oraz B/PRX. Wiadomości CONTROL, `SID=0`, `RESYNC=1`: HELLO 15B DPL, BEACON 15B, WELCOME 21B, READY 23B, READY_ACK 23B. A przechodzi ACTIVE dopiero po READY_ACK od B; B dopiero po pierwszym poprawnym DATA/IDLE z nowym SID. SID16 to CRC16-CCITT z uporządkowanych A/B 6-bajtowych nonce, NIE uwierzytelnienie. Persistent `boot_epoch32` z `boot_epoch.cpp` zapisuje się w rotacyjnym dzienniku 16×6B EEPROM z CRC8 i markerem commit; `attempt16` jest w RAM i rośnie przy renegocjacji. To mechanizm wielokrotnego użytku.

Na granicach handshake B `replacePrxReply()` czeka 550µs, czyści TX/RX FIFO i kolejkuje tylko aktualny ACK; A `resetPtxFifos()` czyści stare dane. Brak 20/25ms delayów per DATA. Watchdog A ACTIVE=250ms, A WAIT_READY_ACK=1000ms, B ACTIVE=750ms, B WAIT_READY/WAIT_CONFIRM=2000ms; DISCOVER nie podbija nonce przy wielogodzinnej nieobecności peer. Na bieżącym `main.cpp` w RealTerm przy stabilnej sesji oczekiwane wspólne niezerowe `SID`, `ST=4`, `R` jest licznikiem odzyskiwań; przejściowe `BAD/MAX` przy resetach są możliwe.

W CI hostowe testy protokołu, sekwencji, resetów A/B, opóźnionych ACK, dziennika EEPROM z przerwanym zapisem, nRF SPI i PHY oraz kompilacja AVR-GCC **PASS**. Nowy handshake nie został dotąd uruchomiony na dwóch rzeczywistych RF-Nano (w przeciwieństwie do poprzedniego benchmarku 625/s); **nie przypisywać mu prędkości 625/s bez pomiaru**. Obecny UART jest tylko konsolą diagnostyczną. Kod bez `malloc/new`, Microchip Studio i ręczne F12 bez zmian. Specyfikacja: `paired-core-v2.md`.

## Uzupełnienie regresji sesji — 2026-11-04

Główna gałąź ma już handshake `PairedSession` z trwałą epoką EEPROM, a ostatnia interwencja **nie przepisywała go**. Dodano test `tests/paired_session_pipeline_test.cpp` symulujący prawdziwe opóźnienie ACK Payload B: odpowiedź jest wcześniej w FIFO, B odbiera ramkę A i dopiero przygotowuje następną odpowiedź. Testuje parowanie, 300 aktywnych wymian, restart jednej/obu stron, odrzucone odpowiedzi i przerwę w kierunku PTX. Host PASS na GitHub Actions. Nie zmienia fizycznego firmware; to większe pokrycie regresji testowej. Nadal brak **rzeczywistego** testu na dwóch RF-Nano najnowszej wersji z handshake. Do sprawdzenia: `ST=4`, wspólne niezerowe `SID` i przejściowy licznik `R`, po RESET A/B/obu. F7/F12 ręcznie, bez Post-Build.

## Rzeczywisty wynik handshake po pojedynczych resetach — 2026-11-04

Użytkownik przesłał dwa zrzuty z RealTerm obu RF-Nano po **osobnych resetach B oraz A**. Po resecie B oba mają **ST=4 i SID=12715**, po resecie A oba mają **ST=4 i SID=16598**. Działający podczas resetu partner ma R=1, restartowany R=0. W stanie ustalonym **612–613 wymian/s** w obu kierunkach, widoczne ARC/MAX/BAD/IO/GAB/GBA/DROP/FULL=0; jest to fizyczne potwierdzenie obecnego handshake'u i jego automatycznej rekonwergencji przy pojedynczych resetach, nie tylko host test. Przy fizycznym braku B A notowało `ARC=330 MAX=110` i `ARC=657 MAX=219`: **3 sprzętowe retransmisje na każde MAX_RT**, jak ustawiono w `SETUP_RETR=0x23`. Tymczasowe TX=0/s w DISCOVER są oczekiwane.

W porównaniu z 625 wymian/s poprzedniego programu narzut przy zdrowej sesji wynosi około **2%**, ~31–34 µs dodatkowego średniego odstępu na transakcję, prawdopodobnie wskutek pracy MCU podczas przetwarzania sesji i diagnostyki; to wnioskowanie, nie oddzielny pomiar czasu CPU. Podczas restartu głównym czynnikiem są rzeczywiste powtórki i czas do handshake'u, nie tylko procesor. **Jednoczesny reset obu płytek nie został tymi zrzutami potwierdzony.** Stałe RADIO/SPI/F12 zachowane. Jest to wyłącznie dopisanie wyniku testów do dokumentacji, bez zmiany kodu.

## Fizyczny jednoczesny restart obu ATmeg + identyfikacja pary — 2026-11-04

Dodatkowy zrzut dwóch RealTerm pokazuje **obydwa** bannery startowe oraz wznowienie w nowej wspólnej sesji `SID=13618` (wcześniej `16598`), `ST=4 R=0` po obu stronach, stabilne ~612–613 wymian/s bez widocznych błędów. Tym samym sprzętowo potwierdzono restart A, B oraz obu naraz. Nadal brak próby rzeczywistej izolowanej utraty zasilania radia, co jest osobnym scenariuszem.

Użytkownik rozważa stały „SSID/SID” po resecie. **Nie utrwalać SID sesji bez projektu ochrony przed starą ramką/ACK Payload.** Obecny SID16 jest celowo zmienny: `PairedSession::derive()` liczy CRC16 z 32-bitowych epok obu płyt zapisanych przez `BootEpoch::advance()` w EEPROM oraz 16-bitowych prób. Propozycja: trwałe `device_id`/`pair_id` zapisane przy parowaniu w EEPROM i wyświetlane niezależnie od `SID`; sesyjny SID nadal zmienny. Nie zaimplementowano tego pomysłu, brak zmian firmware.

## Aktywny UART↔RF↔UART po negocjacji sesji — 2026-11-04

Właściciel zamówił realny transfer użytecznych bajtów z UART obu RF-Nano do UART partnera. Zaimplementowano odrębną od PHY i `PairedLink` klasę `GPT_NRF2401/uart_reliable_stream.h` (port-adapter UART jako szablon, testowalny hostowo). W 24B opaque BODY: `DATA_SEQ` (1B), `ACK_SEQ` (1B), do **22B dowolnego strumienia UART**. Nie naruszono 8B nagłówka RF ani mechanizmu ACK Payload. Stop-and-wait niezależny w obu kierunkach zapewnia retry i deduplikację **wewnątrz aktywnej sesji**. Potwierdzenie aplikacyjne pojawia się dopiero po całkowitym `UART::writeAll()` do TX ring. `RX_CREDIT` to aktualne `UART::freeSpace()/24` zamiast testowego 1; nie usuwa strat wejścia przy CH340 flood bez RTS/CTS.

Aktywny `GPT_NRF2401/main.cpp` nie wysyła **żadnego** tekstu diagnostycznego; USART0 w obu rolach przenosi wyłącznie użytkowe bajty. Handshake `PairedSession`, trwałe epoki EEPROM i automatyczny rekey zachowane. Niezatwierdzony segment pozostaje w RAM lokalnie działającej płytki przez renegocjację, ale nie ma gwarancji exactly-once po resecie odbiorcy bez trwałego journalu. W idle A wyzwala B ACK Payload co około 10 ms, pod obciążeniem bez opóźnień 20/25 ms.

Hostowe testy `tests/uart_reliable_stream_test.cpp` i `tests/uart_reliable_stream_stress_test.cpp` PASS (w tym 4096B **w każdym kierunku** ze stratami i opóźnionymi ACK); cross AVR build nowej aplikacji PASS w GitHub Actions 2026-11-04. **Fizycznie nieprzetestowane jeszcze na obu RF-Nano** — poprzednie 612–613 wymian/s to inny firmware. Następna próba: F7 i ręcznie F12 na obu, A0=GND/VCC; RealTerm 115200 8N1, local echo OFF, wyślij krótkie i >22B teksty w obie strony, sprawdź brak dodatkowych bajtów, potem binarne capture. Bez przywracania Post-Build. `README.md`, `paired-core-v2.md`, `plan.md`, `todo.md`, `goal.md` zaktualizowane.

## Fizyczne dwukierunkowe przeciążenie UART i test diagnostyczny — 2026-11-04

Na RF-Nano nowy UART bridge jest **sprawdzony z jednym kierunkiem**. Jednoczesne długie wysyłanie z obu RealTerm powoduje zgłaszane przez właściciela ubytki. RealTerm wysyła pliki bez sprzętowego flow control; użyte 0ms/char, 10ms/line może generować długie bursty. UART ATmega obsługuje RX i TX asynchronicznie, z osobnymi ISR i ringami 127B: nie jest to ograniczenie do półdupleksu. Problem przepustowości powstaje w transporcie **stop-and-wait 22B z aplikacyjnym ACK i co najmniej jedną wymianą opóźnienia**, orientacyjnie 22*613/2 ≈ **6,74 kB/s/kierunek** w porównaniu z maksimum 115200 8N1 **11,52 kB/s/kierunek**. W niekontrolowanym teście ingress ring 127B może się zapełniać w ~27ms. To wyjaśnienie przez architekturę, nie bezpośrednio zmierzony strumień użytkownika.

**Nowy powtarzalny test** `tests/uart_unthrottled_ingress_test.cpp`: `3B/wymianę` bez odrzucenia, `19B/wymianę` powoduje przepełnienie RX na obu portach bez błędów RF, 600 wymian, 11400B wejścia na stronę; test host PASS, AVR build PASS. Dotychczasowy 4096B stress podawał tylko 3B/wymianę i pauzował, kiedy nie było miejsca: nie dowodził odporności na flood. Brak produkcyjnej zmiany PHY i stop-and-wait. `UART::counters().rx_buffer_full` wskazuje software ring drop; `rx_overrun` sprzętowy DOR0. Nie wypisywać ich na binarny UART bez wyraźnego diagnostycznego trybu/oddzielnego kanału.

**Dalsza praca:** przetestować oba kierunki z charakter delay 1ms (pierwsze pole RealTerm `Delays`) i sprawdzić captured data; następnie zaprojektować **okno transportowe 2+** (wspólny sposób obsługi potoku ACK Payload) i regulację ingress. Samo zwiększanie ringów bez RTS/CTS lub ograniczenia źródła nie zapewnia bezstratnego transferu. F7/F12 manualne, jeden firmware, brak alokacji dynamicznej.

## Najnowsza implementacja pełnego ping-ponga UART — 2026-11-04

**Aktualne** (nadpisuje wcześniejsze opisy stop-and-wait i 10ms polling wyżej): użytkownik wyraźnie wymaga, aby radio wymieniało pakiety bez przerwy, a każdy sprzętowy ACK Payload mógł przenosić niezależny nowy DATA z B. W `main.cpp` usunięto warunek 10ms okresowego odpytywania, więc PTX wyzwala ciągłe wymiany nawet przy pustym UART. `UartReliableStream` przebudowano na wielokrotnego użytku Go-back-N z **WINDOW=4** niezależnie na każdej ATmedze, 4×22B cache, `DATA_SEQ`, kumulacyjne `ACK_SEQ`, replay po 8 własnych wymianach bez postępu. `RX_CREDIT` nadal liczy się z realnego UART TX i ogranicza zbyt duże okno; poprawne `UART::writeAll` nadal jedynym kryterium APP_ACK. Pakiet RF ma 8B MAC+2B transport+0..22B user, więc pozostaje binarnie transparentny. Nie zmieniono negocjacji SID, EEPROM, pinout, SPI, F7/F12 czy Post-Build.

W teście nowej regresji odkryto błąd renumeracji oczekujących segmentów przy rekey po wzroście baseSeq: indeksowanie cache według starego seq%4 zgubiłoby porządek. Poprawiono w warstwie `UartReliableStream` przez `headIndex_` i offset od najstarszego DATA. Testy hostowe PASS, w tym nowy `uart_pipeline_rekey_test.cpp` z 3 zaległymi segmentami i utraconym pierwszym pakietem, `uart_unthrottled_ingress_test.cpp`: **19 B/wymianę ×600=11400 B/stronę bez strat w modelu**, 24B/wymianę z overflow. Cross-build AVR PASS: `.text=10858B, .data=96B, .bss=591B`, 687B statycznego SRAM. Pamiętać, że są to wyłącznie wyniki host i kompilatora, NIE nowego pomiaru na dwóch RF-Nano.

**Następny sprzętowy krok:** po F7/ręcznym F12 obu płyt RealTerm oba COM 115200 8N1, local echo OFF, dwa pliki przy `Delays=0,0` równocześnie, binarny Capture i porównanie SHA256/byte count. Potem pomiar rzeczywistego goodput i liczniki USART RX overflow vs DOR0 przy ewentualnym dalszym ubytku. Z samego UART bez RTS/CTS nie ma absolutnej bezstratności, gdy oferowane źródło jest szybsze niż realne PHY/CPU. `Delays=1ms/char` w poprzednim eksperymencie sztucznie obcinał do ~1000 B/s na kanał i nie testował docelowych ~11,52 kB/s.

## Warstwowa korekta nagłówka 8 B i pełnego 24 B BODY — 2026-11-04

Użytkownik zgłosił, że nawet po poprzedniej zmianie na WINDOW=4 dwukierunkowe wysyłanie z `Delays=0,0` nie działa jeszcze idealnie; zażądał przede wszystkim **stałego DATA↔ACK ping-ponga i braku metadanych w 24B BODY**. Zweryfikowano kod: A nie czeka na UART, wysyła również IDLE, przy `PHY` istnieje jedynie strażnik 130µs (260 taktów Timer1 2MHz), CE15µs, rzeczywisty czas RF/ACK i retransmisje. **Brak sztucznych pauz aplikacyjnych**.

Zmiana warstwowa: `PairedLink::makeStream/observeStream` i `UartReliableStream` używają TX_SEQ jako numeru nowego segmentu 24B oraz RX_LAST jako jego kumulacyjnego ACK po **atomowym przyjęciu do UART TX**. Payload `BODY[0..23]` jest całkowicie nieinterpretowany przez PHY/MAC; `UartReliableStream` traktuje go jako 0..24B bajtów UART, bez wtórnego dwubajtowego nagłówka. Ramka bez danych jest `IDLE` o długości 8B DPL, wysyła ACK i kredyty. Ponowiony DATA zachowuje numer segmentu, ale może przynosić nowy ACK — nie filtrować na poziomie `PairedLink`. Usunięto starsze, sprzeczne API `make/receive` pozostawione z eksperymentów, testy linku są na nowym kontrakcie. `LOSS` może teraz mierzyć obserwowane luki segmentów, **nie liczbę wszystkich fizycznych ramek** (IDLE nie zmienia TX_SEQ), a ARC/MAX pozostają metrykami radia.

WINDOW=4×24B, Go-back-N, SID16, EEPROM, 1Mb/s, SPI8 MHz, DPL8..32 i ręczne F7/F12 są bez zmian. Testy hostowe + cross AVR PASS: full-duplex 4096B/kierunek przy modelowanych stratach, pełne 24B, IDLE8B, duplikat DATA z nowszym ACK, rekey z trzema segmentami, 8bit wrap, UART full i unthrottled ingress. **Nowy format nie ma jeszcze potwierdzenia fizycznego**. Podczas kolejnego testu RealTerm `Delays=0,0` w obie strony, binarny capture i porównanie plików, w razie strat diagnoza osobnych UART rx_buffer_full/DOR0 i nRF MAX_RT/ARC. Wynik historycznych 612–613/s nie jest gwarancją w tej wersji.

## PP checkpoint i rozpoczęcie BURST20 — 2026-11-04

Użytkownik oficjalnie nazwał aktualny protokół **PP (Ping-Pong)**, zgłosił poprawę transferu przy obu RealTerm `Delays=0,0` na zrzucie; bez binarnego hash capture nie deklarować zupełnej bezstratności. Wersję PP 8+24B, WINDOW=4, ciągłe ACK Payload zachowano na branchu `checkpoint/pp-2026-11-04` z commita `784fd83dc1000c09af88c6ff0a92c5469e1dce59`. Nie zmieniono PP firmware.

Drugi profil to **BURST20**: nadrzędna ramka20ms, 5ms kwant, podziały A5/B15, A10/B10, A15/B5; A nadaje pierwszy. W każdym slocie DATA do 1,3ms przed końcem, REPORT 1ms, GUARD 0,3ms. Na nRF 1Mbps pełna ramka32B ma 329µs samego airtime, maks.15 w5ms bez innych kosztów; 32 DATA+1 CONTROL w5ms fizycznie niemożliwe. Kod `burst20_schedule.h` i `profile_select.h` są niezależne od sprzętu i testowane hostowo. Strap A1/PC1 pullup HIGH=PP, GND=BURST20, sprawdzany dwukrotnie przy boot **docelowo**; aktualny main nie odczytuje A1, bo nie ma jeszcze PHY BURST. Zachować A0 rolę. Format raportu i interakcje: `burst-20ms.md`.

Nie buforować 32×32B na ATmega: stream DATA do UART i rolling CRC32/bitmask, bez gwarancji retry po utracie RAM. 5/15 przy UART115200 wymaga min~173B na wejściu w czasie 15ms bez slotu — więcej niż127B ring. Wstępne testy zacząć od10/10 i realnego pomiaru FIFO/deadline/PHY. F7/F12 ręcznie, brak Post-Build.

Codec `GPT_NRF2401/burst20_report.h` i `tests/burst20_report_test.cpp` dodano: 24B REPORT, CRC32/ISO-HDLC obliczany podczas nadawania (bez bufora całego bloku), bitmapa do32 odebranych pakietów i walidacja struktury. **Hostowe testy PASS**. W `main.cpp` nic nie przełączono: wyłącznie PP, A1 nieaktywne do czasu implementacji kompletnego BURST PHY.
