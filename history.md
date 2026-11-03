# Historia projektu GPT_NRF24

## INITIAL

Brak ukończonych i odebranych kamieni milowych.

Przyszłe wpisy powinny zawierać datę, zakres faktycznie wykonanej zmiany, commit i wyniki weryfikacji. Nie wpisywać planów ani deklaracji testów bez dowodów.

## 2026-11-03 — przygotowanie GPIO i planu M1

- Dodano klasę GPIO i dołączono ją do projektu Microchip Studio.
- Rozpisano techniczny plan wdrożenia M1.
- Nie przeprowadzono kompilacji AVR ani testu GPIO na fizycznym urządzeniu; łączność RF pozostaje niewdrożona.

## 2026-11-03 — test GPIO i decyzja o telemetrii łącza

- Właściciel potwierdził miganie diod na obu płytkach po kompilacji w Microchip Studio; kod testowy znajduje się w commicie `9d251e1`. Nie wykonywano niezależnego testu przez agenta.
- Wyłączono automatyczne programowanie w Post-Build; programowanie jest uruchamiane ręcznie przez F12.
- Zatwierdzono kierunek projektu: 32-bajtowe ramki nRF24L01+, cykliczna naprzemienna wymiana, telemetria strat w obu kierunkach i wykorzystanie wolnej części ramki do danych UART.
- Firmware radia i UART nie zostało jeszcze zaimplementowane ani przetestowane.

## 2026-11-03 — projekt protokołu v1

- Dodano `protocol-v1.md`: format 8+24 B, rozkład bitów, sesja, discovery, rozszerzenia, stop-and-wait, diagnostyka i pięć pełnych wektorów 32 B.
- Klasa 0/1 wyprowadzana z zatrzaśniętego po starcie ADC0; przyszłe duplikaty klas wymagają enumeracji, której M1 nie implementuje.
- Zmiana wyłącznie dokumentacyjna; firmware, GPIO, programowanie F12 bez zmian. Brak testów AVR i RF dla specyfikacji.

## 2026-11-03 — nieblokujący UART0, etap uruchomienia

- Porównano starszą bibliotekę `analityk/nau8822/nau8822/nau8822/src/interfaces/serial/serial.h/.cpp`: korzystała z bezpośrednich rejestrów USART0 i blokującego polling.
- Dodano `GPT_NRF2401/byte_ring.h` (SPSC, stałe 128 slotów, 127 efektywnych), `uart.h/.cpp` (RX i UDRE ISR, 115200 8N1, U2X0=1, UBRR0=16, liczniki błędów i przepełnień), integrację z `.cppproj` oraz test pierścienia na hoście.
- `main.cpp` przełączono z testu LED na lokalne nieblokujące echo USB UART. Poprzedni test GPIO pozostaje w historii Git, a PD0/PD1 są zwolnione na potrzeby USART.
- Nie zmieniono F12, Post-Build, środowiska i programowania; brak niezależnej kompilacji AVR i testów na RF-Nano.

## 2026-11-03 — potwierdzenie UART na dwóch urządzeniach i pamięć statyczna

- Właściciel potwierdził, że **obie fizyczne ATmegi** zwracają bajty wysyłane do ich portów USB/CH340. Kompilacja w Microchip Studio działa po ustawieniu standardu C++11. Nie potwierdzono jeszcze długich serii, danych binarnych i testów przepełnienia.
- Właściciel zapisał w repo commit `4b300e2` dodający `-std=c++11` w konfiguracji Release. Uzupełniono tę samą flagę dla Debug, aby konfiguracje nie różniły się językiem.
- Przeniesiono jedyną instancję `UART` w `main.cpp` do statycznego SRAM. Poprzednia lokalna instancja zajmowała stos, **nie stertę dynamiczną**; rozmiar obiektu to około 270 B (2 kolejki po 128 + indeksy i liczniki). Sterownik i ISR bez zmian.
- Nie wykonywano niezależnej kompilacji AVR ani ponownego testu na sprzęcie po tej korekcie.

## 2026-11-03 — test UART plikiem i dodanie SPI/nRF24L01+

- Właściciel potwierdził **stabilny** UART na obu ATmegach, także przy przesyłaniu pełnego pliku przez RealTerm bez sztucznych opóźnień. Nie odnotował błędów w tej próbie; nie ma jeszcze niezależnego pomiaru liczników błędów.
- Dodano sprzętowy SPI master 1 MHz, mode 0, MSB first, z ograniczeniem czasu transferu oraz niezależny od MCU sterownik poleceń/rejestrów/FIFO nRF24L01+.
- Program `main.cpp` realizuje read/write/restore probe, konfigurację i odczyt zwrotny, wynik ASCII na UART, ponowienie znakiem `?`. W RF ustawiono 32 B, 1 Mb/s, CRC16, pipe 0 auto-ACK; radio pozostaje POWER_DOWN, bez transmisji między węzłami.
- Pinout starszego RF-Nano z ATmega328P: D9 CSN, D10 CE (odwrotność typowego modułu zewnętrznego); źródło: RF-Nano V1.1 producenta. Piny nie są zaszyte w sterowniku radiowym, tylko w `main.cpp`.
- Dodano `tests/nrf24_mock_test.cpp` (mock SPI i rejestrów, testy inicjalizacji, FIFO, braku radia, błędnego readbacku i awarii SPI). W tym środowisku nie wykonano kompilacji AVR ani testu na RF-Nano; wyniki z obu płytek oczekiwane od właściciela.

## 2026-11-03 — wyłączenie globalnego pakowania struktur (ostrzeżenia C++)

- Właściciel pokazał `Rebuild All: 1 succeeded, 0 failed, 0 skipped` oraz trzy ostrzeżenia `ignoring packed attribute` wskazujące na pola referencyjne `Spi&` i `Pin&` w `Nrf24`. Error List wyświetlał dodatkowo `required from here` jako 1 Error mimo poprawnego builda; nie jest to diagnostyka błędu kompilacji.
- Przyczyną są cztery ustawienia `PackStructureMembers=True` w `.cppproj` (C i C++, Debug i Release). Ustawiono `False` w obu językach i konfiguracjach, bez zmieniania referencji i API radia. Protokoły bajtowe nie powinny polegać na globalnym `-fpack-struct`.
- Nie zmieniono `-std=c++11`, integracji F12 ani Post-Build. Poprawka konfiguracyjna wymaga sprawdzenia w Microchip Studio po czystej przebudowie.

## 2026-11-03 — decyzja właściciela: pozostawić wyłączone Pack Structure Members

- Potwierdzono świadomą zmianę wprowadzoną przez asystenta w commicie `307e52b`: **jawnie wyłączono globalne `Pack Structure Members`** (`PackStructureMembers=False`) dla kompilatora C i C++ w obu konfiguracjach Debug i Release projektu ATmega328P.
- Właściciel zgłosił wątpliwość dotyczącą wpływu na 2 KB SRAM, ale zdecydował **pozostawić aktualną konfigurację i kontynuować prace nad SPI/nRF24**, bez cofania zmiany na tym etapie.
- Nie wykonano porównawczych pomiarów `sizeof`, Flash i SRAM przed/po zmianie; nie twierdzić, że wpływ na pamięć wynosi zero. Pokazana wcześniej kompilacja zakończyła się sukcesem, lecz zawierała ostrzeżenia. Sterownik nRF24 nadal wymaga potwierdzenia działania SPI na obu fizycznych płytkach.

## 2026-11-03 — potwierdzenie fizycznego SPI i nRF24L01+ na obu RF-Nano

- Właściciel dostarczył dwa zrzuty RealTerm z dwóch fizycznych płytek: **COM11** i **COM10**. Oba porty pokazują powtarzające się wyniki `NRF24 OK S=0E CFG=0C CH=4C RF=06 FIFO=11`.
- To potwierdza wykonanie programu diagnostycznego na obu ATmega328P, komunikację SPI z radiem oraz zakończenie procedury testowej zawierającej zapis/odczyt rejestru RF_CH i konfiguracji nRF24 (przy założeniu wynikającej z kodu, że komunikat OK jest emitowany po wszystkich sprawdzeniach).
- **Etap inicjalizacji sprzętowego SPI i dostępu rejestrowego nRF24 uznany za zaliczony na obu płytkach.** Nie testowano jeszcze nadawania/odbioru fal radiowych, auto-ACK, IRQ, rzeczywistych strat ani naprzemiennych slotów. Nie należy mylić napisu `NRF24 OK` wysłanego przez UART/CH340 z komunikatem odebranym drogą radiową.
- Poprzedni wpis mówiący, że test SPI jest niepotwierdzony, był aktualny podczas jego sporządzania; niniejszy wpis dokumentuje późniejsze dowody. Decyzja o pozostawieniu wyłączonego `Pack Structure Members` pozostaje bez zmian.

## 2026-11-03 — pierwszy test wymiany ramek RAW32 (oczekuje na sprzęt)

- Właściciel zlecił **pierwsze rzeczywiste połączenie radiowe** dwóch RF-Nano, na razie bez wdrażania protokołu v1. Przygotowano ten sam firmware dla obu płytek; **ADC0** (GND/3,3 V, próg surowy 512) zatrzaskuje rolę inicjatora / respondera po starcie.
- Dodano `nrf24_phy.h` (osobna od rejestrów nRF warstwa CE/PRX/PTX, ACK/TX_DS, MAX_RT, timeouty, obsługa RX FIFO i flagi RX_DR), `adc0.h/.cpp`, `avr_delay.h`, `board_clock.h`, `rf_raw_pattern.h`. Aktualny `main.cpp` nadaje wzorce 32 B A→B, B sprawdza i odpowiada 32 B B→A; oba urządzenia raportują w RealTerm wyniki transmisji, ARC_CNT i RX_DR.
- `board_clock.h` stanowi wspólną definicję F_CPU=16 MHz dla opóźnień, USART, SPI i ADC0; wcześniej sam `adc0.cpp` nie miał gwarantowanej konfiguracji F_CPU przed `util/delay.h`.
- Dodano `tests/nrf24_phy_test.cpp` i `tests/rf_raw_pattern_test.cpp`. Lokalnie uruchomiono równoważne testy modelowe g++ C++11 z `-Wall -Wextra -Werror -pedantic`: PASS dla ACK, MAX_RT, timeoutu, błędu SPI, odbioru FIFO, wszystkich bajtów wzorca (mutacje) i progu ADC.
- Test radiowy wykorzystuje sprzętowe CRC16 i auto-ACK z poprzedniej konfiguracji nRF. Stałe opóźnienia testowe są świadomie częścią diagnostyki, nie przyszłego scheduler/MAC.
- **Nie wykonano kompilacji AVR ani próby dwóch fizycznych radii dla nowego wsadu; brak potwierdzenia transmisji RF.** Zmiany nie dotyczą F12, Post-Build ani wyłączenia Pack Structure Members.

## 2026-11-03 — fizyczna wymiana RAW32 A→B→A potwierdzona

- Właściciel przesłał zrzut z jednocześnie otwartymi terminalami RealTerm: **COM10 = A (inicjator)**, **COM11 = B (odpowiadający)**, obie płytki RF-Nano z identycznym wsadem.
- Widoczny wycinek kolejnych numerów ramki **000B–0019** jest zgodny po obu stronach. A raportuje `TX=ACK ARC=00 REPLY=OK RD=1`; B raportuje `RX=OK TX=ACK ARC=00 RD=1`. Potwierdza to nadanie i poprawne odebranie surowych 32 B A→B, przetworzenie wzorca przez B, nadanie odpowiedzi B→A oraz jej zweryfikowanie przez A. Sprzętowe potwierdzenia, RX_DR i przełączanie PTX/PRX działają dla pokazanego przebiegu.
- W widocznym fragmencie nie odnotowano retransmisji ani błędnych pakietów (`ARC=00`, `REPLY=OK`, `RX=OK`). Nie oznacza to zmierzonego PER=0 w długim okresie ani odporności na zakłócenia.
- **Etap fizycznego testu dwukierunkowego RAW32 zaliczony.** Nadal brak protokołu v1, mostu UART↔RF, discovery, docelowego arbitrażu, testów `MAX_RT` przy odłączeniu odbiorcy i automatycznego odzyskiwania łączności. Dalsze testy awaryjne pozostają otwarte.

## 2026-11-03 — test restartów A i B oraz automatycznego powrotu RAW32

- Właściciel przesłał trzy zrzuty RealTerm przedstawiające sekwencję testową: **(1) restart A**, **(2) wciśnięcie i przytrzymanie RESET B**, **(3) zwolnienie RESET B i oczekiwanie na odzyskanie łączności**. Ustalone fizyczne role: A0 przy GND = A/CLASS 00, A0 przy dodatnim zasilaniu = B/CLASS 01.
- Po restarcie A log pokazuje `NRF OK CLASS=00 ... MODE=RAW32`, ponowną numerację od `SEQ=0000` oraz poprawne `TX=ACK REPLY=OK`; B przyjmuje kolejne ramki i odsyła odpowiedzi.
- Przy trzymaniu RESET B na A widać etap `TX=ACK` przy `REPLY=MISS`, następnie `TX=MAX_RT ARC=0F REPLY=MISS` (m.in. okolice `SEQ=00B7..00BC`). Jest to spodziewane rozróżnienie: **sprzętowy ACK radia nie oznacza odpowiedzi aplikacyjnej**. Podczas resetu samej ATmegi nRF24 może pozostać zasilony i przez krótki czas nadal odpowiadać sprzętowo.
- Po zwolnieniu RESET B ponownie uruchamia się program B (`NRF OK CLASS=01 ... MODE=RAW32`), a na obu stronach widać od `SEQ=0185` poprawną wymianę: A `TX=ACK ARC=00 REPLY=OK RD=1`, B `RX=OK TX=ACK ARC=00 RD=1`. A nie była dodatkowo restartowana. **Odzyskiwanie RAW32 po resetach mikrokontrolerów potwierdzone sprzętowo.**
- Zakres dowodu: reset ATmega328P, **nie pełne odcięcie zasilania nRF24**, zakłócenia RF, wiele węzłów ani docelowe wykrywanie sesji w protokole v1. Nie deklarować ogólnej niezawodności bez testów protokołu, buforów i strat.

## 2026-11-03 — projekt payloadu v1 i próba ramki 8+24 B w działającym RF

- Właściciel ustalił ostatecznie dla obecnych prób: **A ma A0 na GND, B ma A0 na dodatnim zasilaniu**. Oba nRF-Nano wcześniej przeszły test RAW32 A↔B, reset A, przytrzymanie RESET B, `MAX_RT` i automatyczny powrót po zwolnieniu resetu.
- Właściciel wskazał fizyczne ograniczenie testu całkowitego wyłączenia zasilania radia: USB/CH340 zostaje odłączony, RealTerm traci COM, a ponowne otwarcie COM wywołuje reset. Zdecydowano **odroczyć** osobne badanie pełnego odcięcia zasilania, nie powtarzać tego warunku przed rozwojem protokołu.
- Dodano czysty C++11 `frame_v1.h`, jawnie kodujący i walidujący stałą ramkę 32 B (nagłówek 8 B, BODY do 24 B), pole wersji i długości, src/dst, SID, TX_SEQ, RX_LAST, endpointy, dozwolone podtypy CONTROL, nieznane typy i zerowy padding. Żadnego `packed`, AVR ani heap.
- Dodano `v1_probe_pattern.h`: testowe długości 0/1/23/24 B, binarny wzorzec kontrolny oraz testową stałą SID=0x2A. Aktualny `main.cpp` korzysta z działającego PHY i wymienia **ramki v1** z weryfikacją `BODY`, `RX_LAST` i własnych `TX_SEQ` w obu kierunkach. **To nadal nie jest negocjacja sesji ani niezawodny UART↔radio.**
- Dodano testy `tests/frame_v1_test.cpp` i `tests/v1_probe_pattern_test.cpp`; hostowy g++ C++11 z `-Wall -Wextra -Werror -pedantic` wykonał te przypadki pomyślnie (pięć wektorów, negatywne parsowanie, 256 numerów sekwencyjnych, obie klasy i różne długości). Wcześniejszy błąd w teście był skutkiem testowania READY z adresem broadcast, który parser poprawnie odrzucał przed kontrolą SID; skorygowano konstrukcję testową.
- Skrócono etykiety linii liczników UART, aby nie przekroczyć istniejącego 80-bajtowego bufora diagnostycznego. Zachowano konfigurację SPI/nRF, pinów CE/CSN, C++11, F12 i wyłączenie pakowania struktur.
- **Nie przeprowadzono jeszcze kompilacji AVR ani fizycznego testu nowej ramki v1**. Potwierdzone wcześniej testy RAW32 nie są dowodem działania nowej warstwy kodowania.

### Korekta formatu LINK_REPORT przed implementacją transportu

- Zauważono błąd w pierwotnym projekcie `protocol-v1.md`: `LINK_REPORT` zajmuje **10 bajtów BODY** (opcode 1 + base_seq 1 + bitmap32 4 + hw_retry16 2 + max_rt16 2), a nie 9. Skorygowano dokument i tabelę długości w `frame_v1.h` jednocześnie; dodano wektor i test negatywny dla długości 9. Zmiana nie wpływa na diagnostyczne ramki DATA o długościach 0/1/23/24.

## 2026-11-03 — test fizyczny payloadu v1 na obu RF-Nano zaliczony

- Właściciel dostarczył zrzut dwóch jednocześnie działających terminali RealTerm: **COM11 = B, COM10 = A**. Pokazano zgodne numery `SEQ=45..53` oraz cykl `LEN=00,01,17,18` (hex: 0, 1, 23, 24 bajty znaczącego BODY). A raportuje `TX=ACK ARC=00 REPLY=OK` oraz `IN` odpowiadające `OUT` w B; B raportuje `RX=OK TX=ACK ARC=00`. Jest to **potwierdzenie działania nagłówka/payloadu v1 i jego kontroli w kodzie na obu fizycznych radiach**. Wymiana w obie strony pozostaje poprawna.
- Właściciel dodatkowo potwierdził próby resetowania jednej lub obu ATmeg i automatyczne wznowienie wymiany bez trwałego zablokowania. Ten dowód dotyczy obecnego **testowego SID=0x2A**, nie właściwej negocjacji/odtwarzania sesji v1.
- W części wierszy B występuje `RD=0` przy `RX=OK` i `TX=ACK`. Nie stanowi to sprzeczności: `RX=OK` pochodzi z odczytu i weryfikacji ramki w FIFO, natomiast `RD` oznacza zaobserwowaną w chwili próbkowania flagę przerwania `RX_DR`, która może być już skasowana. Flaga nie jest obowiązkowym niezależnym potwierdzeniem każdej ramki FIFO; nie interpretować `RD=0` jako utraty danych. Obecna obserwacja nie jest jeszcze diagnostyką przerwań nRF w warunkach przeciążenia.
- Nie było radiowego wstrzykiwania uszkodzonych ramek, testu pełnego zerowania paddingu w odbiorniku z celowo błędnymi ramkami ani obciążenia losowymi danymi użytkownika; negatywne testy parsera wykonywano na hoście. **Most UART↔radio, negocjacja SID, transport RELIABLE, aplikacyjne ACK i retry nadal nie istnieją.**

## 2026-11-03 — pierwszy rzeczywisty most UART↔RF z aplikacyjnym ACK

- Dodano `uart_transport_v1.h`: niezależny od AVR stop-and-wait w obu kierunkach, DATA_SEQ oddzielny od TX_SEQ, atomowe przyjęcie całej porcji do UART TX, BUSY/ACCEPTED/REJECTED, deduplikację i retransmisje. `DATA_ACK` łączy ACK i przesył własnych bajtów; segment w aktualnym moście ma 1–18 B.
- Rozszerzono `frame_v1.h` o podtyp CONTROL `0x12 DATA_ACK`; format opisano w `protocol-v1.md`. `UART::writeAll` gwarantuje zapis całego segmentu albo żadnego bajtu do stałej kolejki 127 B.
- Dodano `session_v1.h`: 4-etapowe uzgadnianie sesji `HELLO, HELLO_REPLY, READY, READY_ACK`; dodano `boot_epoch.h/.cpp`: 16-slotowy, odporny na przerwany zapis dziennik EEPROM liczników startu (nie klucz kryptograficzny). `main.cpp` zapewnia jeden firmware dla A/GND i B/+AVCC, cykliczne puste ramki i przełączanie kierunku oraz oddziela diagnostyczne liczniki od binarnego UART.
- Dodano testy hostowe sesji, transportu, DATA_ACK i `tests/uart_transport_stress_test.cpp` (po 1000 bajtów binarnych w każdą stronę z symulowanymi stratami ramek/ACK, w lokalnym modelu g++ C++11 PASS). Dodatkowy hostowy test podstawowego handshake/DATA_ACK przeszedł. Dodano powtarzalny `tests/run_host_tests.sh` i opcjonalne uruchamianie GitHub Actions. **Nie ogłaszać całej serii CI jako PASS bez odczytu statusu workflow**; testy AVR nie zostały wykonane.
- Przy braku akceptacji aplikacyjnej po 64 próbach uruchamiana jest renegocjacja sesji z zachowaniem porcji w pamięci RAM. Utrata ACK sprzętowego nie jest traktowana jako aplikacyjne potwierdzenie. Brak dokładnie-raz-po-resecie i brak RTS/CTS: pełny strumień 115200 może przepełnić wejściową kolejkę 127 B.
- `F12`, Post-Build, piny, podstawowy sterownik nRF/SPI i `-std=c++11` pozostają bez zmian. Nowy program musi zostać jeszcze skompilowany i przetestowany na obu fizycznych RF-Nano.

## 2026-11-03 — pierwszy dwukierunkowy most UART↔RF potwierdzony sprzętowo

- Właściciel przesłał zrzut obu okien RealTerm z faktyczną wymianą krótkich tekstów: **COM10→COM11** wysłano `ala ma kota`, co jest widoczne w terminalu COM11; **COM11→COM10** wysłano `kot ma ale`, co jest widoczne w terminalu COM10. W obu kierunkach tekst dotarł bez widocznych dodatkowych znaków.
- Dowód obejmuje realne dane UART przechodzące przez kod negocjacji sesji, koder v1 i nRF24 na dwóch RF-Nano z identycznym wsadem. Oznacza to **zaliczenie pierwszego fizycznego testu funkcjonalnego mostu dwukierunkowego**; poprzedni wpis o braku testu był aktualny przed dostarczeniem zrzutu.
- Widoczny zrzut **nie dowodzi** dostarczenia przy utraconym ACK, pełnym buforze, resecie w trakcie segmentu, transmisji równoczesnej ani długim pliku; nie mierzy też rzeczywistego PER lub przepustowości. Brak RTS/CTS nadal ogranicza bezstratność ciągłego strumienia wejściowego UART 115200.
- Firmware, F12, Post-Build, pinout i pliki protokołu pozostają w tym wpisie bez zmian — aktualizacja wyłącznie dokumentacyjna.

## 2026-11-03 — równoczesna transmisja UART w obu kierunkach pod obciążeniem

- Właściciel przesłał dwa zrzuty z jednoczesnym wysyłaniem plików przez RealTerm w obu kierunkach mostu RF-Nano. Oba terminale nadal odbierają wiersze danych tekstowych; drugi zrzut pokazuje odpowiednio około **5236** i **4552** znaków naliczonych w terminalach. Brak widocznego zakleszczenia i utrzymuje się przepływ w obu kierunkach.
- Na zrzutach przy opcji **Dump File to Port** pola `Delays` mają niezerowe wartości **5** i **3**. Jest to test z ustawionym opóźnieniem/dawkowaniem po stronie RealTerm, a nie zweryfikowany nieograniczony strumień UART 115200 baud.
- **Równoczesny ruch dwukierunkowy i transmisja wielosegmentowa są fizycznie potwierdzone funkcjonalnie.** Zrzuty nie dostarczają jednak porównania bajt po bajcie, sum SHA-256 ani wiarygodnego bilansu danych wysłanych/odebranych; nie dowodzą zerowej liczby strat, braku duplikatów, osiągniętej przepustowości ani pracy przy pełnym obciążeniu 115200 bez opóźnień.
- Następnym krokiem walidacyjnym jest przechwycenie pełnych strumieni obu kierunków i porównanie długości/hashów z plikami źródłowymi, wraz z odczytem liczników przepełnienia UART i retransmisji na poziomie warstw. Brak RTS/CTS nadal uniemożliwia gwarantowaną bezstratność przy wejściu szybszym niż rzeczywista przepustowość radiowa.
- Zmiana wyłącznie dokumentacyjna; firmware i konfiguracja F7/F12 bez zmian.

## 2026-11-03 — punkt odniesienia przepustowości i analiza opóźnień nRF24

- **Oszacowanie z bieżącego kodu, NIE pomiar:** ~18–20 pełnych cykli A→B→A na sekundę, czyli ~18–20 ramkowych transmisji/s w każdym kierunku (~36–40 ramek RF/s łącznie). Przy obecnym stop-and-wait i 18 bajtach danych UART/ramkę daje to **około 324–360 B/s użytecznego UART na kierunek w stanie wysycenia** (wartość modelowa; zrzuty RealTerm nie dostarczają uśrednionego pomiaru). Właściciel uznał wynik za słaby i zlecił analizę wąskich gardeł.
- **Największe ograniczenia:** w `main.cpp` responder B ma bezwarunkowe `delay.ms(25)` przed przejściem RX→TX, inicjator A `delay.ms(20)` po każdym cyklu; w `nrf24_phy.h` każde `transmit()` i `listen()` czeka `delay.ms(2)`, czyli czterokrotnie na cykl. To daje **co najmniej 25+20+4×2 = 53 ms** narzucone przez firmware na cykl, jeszcze bez SPI, RF i odpytywania statusu.
- Pozostałe ograniczenia: sprzętowe SPI tylko **1 MHz** (`spi.cpp`; samo wgranie lub odczyt 32 B z komendą kosztuje ~264 µs plus narzut CPU), polling `STATUS` w `transmit()` co **1 ms**, odpytywanie odbioru z `FIFO_STATUS` i `STATUS`, stop-and-wait (do 18 B UART/ramkę) oraz brak kontroli przepływu UART (127 B RX).
- **Istotne rozróżnienie fizyczne:** przy włączonym PWR_UP nRF24L01+ potrzebuje około **130 µs** na przejście Standby→TX/RX; ~1,5 ms dotyczy początkowego uruchomienia oscylatora z POWER_DOWN, nie każdej ramki. Odpowiedź B wymaga synchronizacji z fazą auto-ACK nadawcy, zwłaszcza przy utracie ACK. `SETUP_RETR=0x3F` oznacza odstęp retransmisji 1000 µs i 15 ponowień; nie usuwać `25 ms` bez zastąpienia go poprawnym mechanizmem ograniczonego oczekiwania/synchronizacji.
- nRF24L01+ posiada **aktywny stanem niskim IRQ** dla `RX_DR`, `TX_DS`, `MAX_RT`; flagi w STATUS kasuje MCU przez SPI. Nie ma niezależnego pinu `ready for new packet`. Miejsce w FIFO określają `FIFO_STATUS` / `TX_FULL`; zakończenie nadania — `TX_DS` albo `MAX_RT`. Należy zweryfikować na schemacie/PCB konkretnego RF-Nano, czy IRQ jest podłączony do ATmega328P; dokumentacja starszych RF-Nano wylicza CE/CSN/SPI, lecz nie podaje przypisania IRQ, więc **nie zakładać połączenia**. Jeśli IRQ jest NC, można odpytywać STATUS przez szybkie SPI NOP, bez milisekundowych sleepów.
- Po uporządkowaniu warstwy PHY: timer mikrosekundowy/timestampy zdarzeń, oczekiwanie na IRQ lub STATUS z timeoutem, stan PWR_UP utrzymywany między ramkami, przyspieszenie SPI do ≤8 MHz (nRF24L01+ do 10 MHz; zegar AVR16 daje preskaler /2 → 8 MHz), pomiar cykli i liczników utraty/ponowień. Rozważyć **ACK payload** (`W_ACK_PAYLOAD`), aby strona pozostająca w PRX odsyłała dane bez zmiany PRX↔PTX; odpowiedź musi być przygotowana przed odbiorem wyzwalającym ACK, zatem wymaga potokowego ACK aplikacyjnego i poprawnej deduplikacji.
- Docelowe wartości klatek/s po optymalizacjach są **hipotezą do weryfikacji**, a nie wynikiem testów. Nie zmieniono firmware, ustawień F7/F12, protokołu ani statycznych buforów; ten commit dokumentuje problem i plan eksperymentalny.
- Referencje sprzętowe: Nordic nRF24L01+ Product Specification, rozdz. 6–8 (tryby, 130 µs, IRQ, FIFO, ACK payload, spi max 10 MHz); RF-Nano V1.0 schematic i instrukcja emakefun/Keywish (niepewne wyprowadzenie IRQ na konkretnej PCB).

## 2026-11-03 — propozycja architektury ACK Payload: STREAM i BURST

- Właściciel przedstawił dwa docelowe profile MAC: STREAM oparty o szybko wyzwalane sprzętowe ACK Payload w dwóch kierunkach bez ciągłego przełączania PTX/PRX; BURST jako serie do około 20 ms, z możliwością przekazania prawa do nadawania drugiej stronie na żądanie. W stanie bezczynności zaproponował 1–2 ramki na 20 ms, ewentualnie oszczędzanie energii.
- Rozróżniono usługę niezawodnego przesyłania plików (metadane transferu, SACK, credits, streaming do sinku) od profilu MAC. Dla przykładowych 232 segmentów nie ma wymogu jednoczesnego buforowania całości — szczególnie wobec 2 KB SRAM obecnej ATmegi. Rozważana jest większa płytka w przyszłości, bez zmiany formatu 8+24 B.
- Dodano `mac-profiles-proposal.md` jako **koncepcję**, z wymaganiami dotyczącymi sprzętowego ACK vs aplikacyjnego potwierdzenia, timeoutów, FIFO, zmian ról i zapobiegania dwóm PTX, rozbieżności PER/BER oraz testów. **Nie zmieniono firmware, timerów, SPI ani programowania F12.**

## 2026-11-03 — szybki ping-pong sprzętowy 32 B / ACK Payload 32 B

- Na życzenie właściciela zamiast od razu wdrażać STREAM/BURST dodano izolowany **test rdzenia fizycznego**: A niezmiennie PTX wysyła pełne 32 B, B niezmiennie PRX odsyła pełne 32 B w sprzętowym ACK Payload. Jeden HEX na obie płytki, ADC0 wybiera klasę. Bez aplikacyjnego UART transportu i bez stałych opóźnień 20/25 ms.
- Nowe wielokrotnego użytku API: `Nrf24::enableAckPayload`, `writeAckPayload`, `readPayloadWidth`, `readDynamicPayload`, `SpiMaster::setSpeed`; nowy testowalny `nrf24_ack_phy.h` realizuje PTX/PRX, pełne FIFO, szybki STATUS polling, 8 ms deadline awarii i fizyczną przerwę 130 µs PTX. Timer1 2MHz służy do pomiaru transakcji/s, a `ack_bench_pattern.h` weryfikuje każdy bajt i numer ramki.
- Konfiguracja ACK Payload FEATURE=0x06, DYNPD=0x01, retry SETUP_RETR=0x23, SPI w tym trybie 8 MHz. Poprzedni projekt i działający bridge nadal są dostępne po zmianie `application_select.h` na 0. Zachowano F12, brak automatycznego Post-Build, GPIO i C++11.
- Dodano testy hostowe `tests/ack_bench_pattern_test.cpp`, `tests/nrf24_ack_phy_test.cpp` oraz rozszerzono mock SPI nRF o pełne ACK Payload, szerokość DPL i przypadek ACTIVATE. Lokalnie wykonano z g++ C++11 `-Wall -Wextra -Werror -pedantic` testy wzorca i PHY na zgodnej kopii kodu: **PASS**; cały projekt AVR, pełna seria CI i rzeczywisty test obu nRF **nie zostały jeszcze wykonane**.
- Wyświetlane po UART raporty to `A TX/s, ACK/s, EMPTY, MAX, ARC, BAD, IO, GAP` oraz `B RX/s, Q/s, FULL, BAD, IO, GAP`. `TX/s` i `ACK/s` są wartościami zmierzonymi po wgraniu i uruchomieniu, a nie założeniami. Szczegóły w `ack-payload-benchmark.md`.

## 2026-11-03 — ACK Payload ping-pong: pierwszy pomiar na obu RF-Nano, 540 transakcji/s

- Zrzut użytkownika z dwóch RealTerm pokazuje wiele kolejnych identycznych okien: **A TX/s=540 ACK/s=540 EMPTY=0 MAX=0 ARC=0 BAD=0 IO=0 GAP=0**; **B RX/s=540 Q/s=540 FULL=0 BAD=0 IO=0 GAP=0**.
- Jest to **fizycznie potwierdzony** test izolowanego rdzenia: 540 poprawnie potwierdzonych sprzętowych transmisji 32 B A→B/s i 540 zweryfikowanych pełnych odpowiedzi ACK Payload 32 B B→A/s. `Q/s` oznacza załadowanie do sprzętowej kolejki, a nie samodzielne potwierdzenie doręczenia; po stronie A `ACK/s=540` potwierdza odbiór tych odpowiedzi.
- Surowe payloady (bez narzutu protokołu v1) = **540×32=17280 B/s (17,28 kB/s dziesiętnie) w każdym kierunku**, 34560 B/s zsumowane. 540 kompletnych transakcji/s daje średni odstęp **1000/540=1,85185 ms**, a nie 600 µs. Sam czas radiowy transakcji z 32 B DATA + 32 B ACK Payload przy 1 Mb/s/5 B address/CRC16 ma minimum ~329+130+329=788 µs; screenshot NIE mierzy osobnej latencji pojedynczej ramki ani jej rozkładu.
- Jeżeli przyszły transport zachowa 18 B netto danych UART na każdą wymianę i identyczne tempo, **hipotetyczny upper-bound** wyniesie 540×18=9720 B/s (9,72 kB/s) na kierunek, przed stratami, pustymi pakietami, kontrolą przepływu i dalszą logiką. Ten benchmark testuje surowe 32B, **nie osiągnięty end-to-end UART goodput**.
- Stabilny obraz liczników bez retransmisji, błędów wzorca, dziur sekwencji, pustych ACK i pełnego FIFO to bardzo dobry wynik sprzętowy dla tego układu/warunków; nie dowodzi zerowego PER w każdych warunkach i nie wyznacza osiągalnego minimum czasu wymiany.
- Zmieniono wyłącznie dokumentację, bez zmiany firmware; tryb testowy nadal wybrany `GPT_NRF24_ACK_BENCHMARK=1`. Zachowano F12, brak Post-Build.
