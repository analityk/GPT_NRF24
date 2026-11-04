**Stan na 2026-11-04: obecny firmware to PP (Ping-Pong), nie zmieniany przy pracy nad BURST20. Fizyczny zrzut jednoczesnych strumieni poprawił się po przejściu na 8+24B i WINDOW=4; brak binary checksum capture. PP checkpoint `checkpoint/pp-2026-11-04` SHA 784fd83. BURST20 ma czysto logiczny harmonogram i boot-mode selector testowane hostowo, NIE ma jeszcze BURST PHY ani wyboru A1 w main.**

**Uwaga dla gałęzi `experiment/pp-echo-ramp-2026-11-04`:** jest to
tymczasowy wariant pomiarowy, z tym samym PP PHY/MAC/sesją/transportem,
ale z generatorem bajtów na A i lokalnym echem radiowym na B. Normalny
UART↔RF↔UART nadal znajduje się na niezmienionym `main` (`6b94a43`).
Szczegóły i przywracanie: `echo-ramp-test.md`. Nie przedstawiać testu
hostowego jako odbioru sprzętowego; oba MCU wymagają ręcznego F12.

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
