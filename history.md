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

## 2026-11-03 — propozycja minimalnego nagłówka 4 B / 28 B danych

- Użytkownik zaproponował specjalizowany dwuwęzłowy format 24–28 B danych, dwie niezależne ośmiobitowe sekwencje kierunków (TX i RX), skrócone liczniki strat w nibbles i pozostałe 4-bitowe pola sterujące. Celem jest maksymalna transparentność przy sprzętowym ACK Payload.
- Spisano projekt `compact-pair-frame-proposal.md` z 4-bajtowym nagłówkiem: TX_SEQ, RX_LAST, MISS_AB/MISS_BA po 4 bity, RX_CREDIT 4 bity i typ/priorytet/resync w pozostałych 4 bitach; wykorzystuje sprzętową DPL, żeby nie przesyłać osobnego LEN. Zwrócono uwagę na problem utraty SID przy redukcji headera: możliwe uzgadnianie adresu radiowego na epokę sesji, lecz wymaga dodatkowego projektu i testu.
- **To tylko projekt, nie zaimplementowany format.** Istniejący protokół v1 8+24, działający most UART i benchmark ACK Payload pozostają bez zmian. Arytmetyczna projekcja przy 540 wymianach/s: 15 120 B/s/kierunek dla 28 B użytecznych, 12 960 B/s dla 24 B — nie są to pomiary gotowego transportu.

## 2026-11-03 — wybór większego nagłówka 8 B i neutralnego transportu BODY

- Po porównaniu 4+28 i 8+24 właściciel **wybrał 8 bajtów metadanych oraz do 24 bajtów nieinterpretowanych danych w payloadzie nRF24**. Priorytet: telemetria i diagnostyka, nie maksymalna liczba bajtów aplikacyjnych. Poprzednia propozycja 4+28 pozostała wariantem historycznym, nie aktualną rekomendacją.
- Przy zmierzonych 540 transakcjach/s matematyczny pułap wynosi 12 960 B/s/kierunek dla BODY 24 B wobec 15 120 B/s dla 28 B — różnica 2160 B/s. To arytmetyka, nie pomiar nowego formatu. UART 115200 8N1 ogranicza pełny strumień do 11 520 B/s/kierunek.
- Warstwa MAC/PHY nie powinna interpretować BODY: przyszły transfer plików, TCP-like, komendy, ewentualnie kryptografia należą do niezależnych wyższych warstw. Obecny `frame_v1.h` ma nagłówek 8 B, ale jego faktyczne pola nie gwarantują od razu rozbudowanej diagnostyki; konieczna jest osobna decyzja o rozmieszczeniu pól i kompatybilności.
- Wyłącznie decyzja i dokumentacja: **brak zmian firmware**, konfiguracji ACK benchmarku, F7/F12 czy Post-Build.

## 2026-11-03 — implementacja uzgodnionego 8+24 B z kontrolą dwukierunkowej jakości

- Wprowadzono **oddzielny od legacy v1** kod `paired_frame.h`: 8B TX_SEQ/RX_LAST, dwa 4-bitowe LOSS_AB/LOSS_BA, 4-bitowy ARC_LAST, 4-bitowy RX_DROP, RX_CREDIT (pełne 24B segmenty), TYPE_FLAGS, 16-bitowy SESSION_ID little-endian. Do 24B BODY pozostaje nieinterpretowanym ciągiem bajtów. DPL odczytuje prawdziwy rozmiar pakietu 8..32 B, bez redundantnego LEN.
- `paired_link.h` to niezależna od AVR maszyna telemetrii: numeracja modulo256, pomiary luk/modulo16, echo jakości partnera, deduplikacja, odrzucanie obcego SID, monotoniczne przyjmowanie kredytów (duplikat nie cofa statusu); statyczny stan linku pojedynczej płytki z konfigurowalną rolą.
- Rozbudowano istniejące `nrf24.h` oraz `nrf24_ack_phy.h` o zmienną długość pakietu. Poprzedni fizyczny benchmark 32+32B nadal korzysta z oryginalnego API, a nowy `paired_core_main.cpp` używa 8B IDLE i 8+24B DATA w DATA/ACK Payload. W nowym wariancie UART służy **wyłącznie do diagnostyki**; treść danych jest binarnym wzorcem testowym, bez transportu UART. Rolę wybiera ADC0 na obu identycznych wsadach.
- `application_select.h` wybiera tryb 0=dotychczasowy UART bridge, 1=benchmark 32+32B z potwierdzonym 540/s, **2=nowy rdzeń 8+24B (domyślnie)**. Nie zmieniono F12, nie ma Post-Build flash. `SESSION_ID=0x4E52` jest **stałym fixture** w nowym eksperymencie, a nie negocjowaną/bezpieczną sesją. Recovery po resetach, dane użytkownika z UART, pliki i szyfrowanie nie są tu zaimplementowane.
- Host: g++ C++11 testy nowego kodeka, stanu, variable DPL i PHY na GitHub Actions **PASS**. Dodano CI z AVR-GCC/avr-libc, krzyżowa kompilacja wszystkich trzech wariantów (0/1/2) **PASS** w workflow z 2026-11-03: https://github.com/analityk/GPT_NRF24/actions/runs/41722466033. To nie jest test ani build Microchip Studio na Windows, nie jest test fizyczny.
- Specyfikacja i instrukcja: `paired-core-v2.md`. Poprzedni wynik 540/s nie został przeniesiony na ten nowy format; potrzebny test obu RF-Nano i pomiar nowych wartości.

## 2026-11-03 — fizyczny test szybkiego rdzenia 8+24 B: stabilne ~625 wymian/s

- Użytkownik przekazał zrzut dwóch działających RF-Nano, na obu ten sam firmware w trybie `GPT_NRF24_APPLICATION=2`; terminal A/PTX (COM10) i B/PRX (COM11) pokazują wielokrotne kolejne raporty.
- Po początkowym rozruchu: **A TX/s=625, ACK/s=625**; **B RX/s=625, Q/s=625**, sporadycznie B pokazuje 626 ze względu na niezależne okna pomiarowe. Wszystkie widoczne wartości `ARC=0`, `MAX=0`, `BAD=0`, `IO=0`, `GAB=0`, `GBA=0`, `DROP=0`, `FULL=0`. `ACK/s` na A oznacza *poprawnie zweryfikowany nowy* ACK Payload, `Q/s` B tylko jego umieszczenie w kolejce sprzętowej.
- Podczas ustalonego ruchu test generuje **24 bajty binarne na ramkę DATA w obu kierunkach** (8B nagłówka, 32B payload RF). Zatem wynik daje około **625×24=15000 B/s użytecznych bajtów wzorca/kierunek**, 30000 B/s łącznie. To **nie jest** zweryfikowany UART/file end-to-end goodput. Cały 32B payload daje 20000 B/s/kierunek (nagłówek + dane).
- Średnia liczba wymian wzrosła z **540 do 625 na sekundę**, czyli o około **15,7%**, przy średnim odstępie **1000/625=1,6 ms**. To odstęp między transakcjami, **nie oddzielny pomiar opóźnienia pojedynczej wiadomości**. Oba testy przy nasyconym DATA mają pełne 32B radiowego payloadu, więc poprawy nie przypisywać samej zmianie rozmiaru nagłówka.
- Wynik testu fizycznego profilu 8+24 **PASS dla ciągłego dwukierunkowego wzorca danych i widocznej telemetrii**. Zero widocznych błędów nie dowodzi zerowego PER/BER w innych warunkach. Czterobitowe liczniki strat mogą się zawijać; nie testowano zakłóceń, resetów w aktywnej sesji, kredytów przy zapełnionym sinku ani aplikacyjnego transportu UART. SESSION_ID=0x4E52 pozostaje *stałą testową*, nie negocjowaną sesją.
- Zmiana wyłącznie dokumentacyjna; `F7/F12`, kod i wybór trybu 2 pozostają bez zmian.

## 2026-11-03 — konsolidacja repozytorium do jednego aktywnego protokołu

- Na wyraźne polecenie właściciela usunięto **z bieżącego drzewa** dawne warianty: pierwszego mostu UART/v1, surowego benchmarku 32+32, stare sesje, stary kod transportu, pliki wzorców i ich nieużywane testy. **Wszystkie te pliki przed zmianą zabezpieczono** w gałęzi `archive/pre-paired-core-cleanup-2026-11-03`, wskazującej commit `d91630509ba32fb62ab0437ff2d2b5770feae31c`; historia Git pozostaje nienaruszona. Nic nie trzeba odtwarzać ręcznie, dopóki nie ma nowego wymagania.
- Funkcja dotychczasowego `paired_core_main.cpp` została przeniesiona do **jedynego `GPT_NRF2401/main.cpp`**, bez zmiany kolejności transakcji radiowych, konfiguracji radia, testowego SID ani raportów UART. Usunięto przełącznik `GPT_NRF24_APPLICATION` i zbędne entrypointy, zachowując jeden wspólny firmware.
- `nrf24_ack_phy.h` ma już tylko wielokrotnego użytku API zmiennej długości 8..32 B, `nrf24.h` nie ma nieużywanych poleceń sztywnego 32B; nRF sterowanie i timing ACK Payload zachowane. Projekt Microchip Studio ma tylko aktywne pliki. Zostawiono niezbędne SPI, GPIO, ADC0, UART TX do diagnostyki, Timer1, stałe ring buffery i kodek/stan `paired_frame.h` + `paired_link.h`.
- Testy hostowe uruchamiają się automatycznie na wszystkich `tests/*_test.cpp` (bez sztywnego katalogu nazwanego w skrypcie), a skrypt AVR buduje **jeden program**. Występują osobne zadania host i AVR w GitHub Actions. Cross-build **PASS** i host tests **PASS** w workflow https://github.com/analityk/GPT_NRF24/actions/runs/41735111145. Nie jest to ponowiony test fizyczny; ostatni zmierzony wynik 625/s pozostaje dotychczasowym punktem odniesienia.
- README, goal, plan, todo, migracja, paired-core-v2 i AGENTS odzwierciedlają teraz jedną aktywną architekturę, nie historyczną konfigurację 0/1/2. Usunięte z bieżącego drzewa historyczne dokumenty i szkice są nadal dostępne w archiwum Git.
- Bez naruszania F7/F12, kompilatora C++11, globalnego `PackStructureMembers=False`, pinoutu i braku automatycznego Post-Build flashowania. Test fizyczny po refaktoryzacji strukturalnej może dodatkowo potwierdzić brak regresji; nie deklarować go przed wykonaniem.

## 2026-11-04 — handshake pary z reset-recovery i trwałymi nonce, host/AVR PASS

- Na polecenie użytkownika po pozytywnym sprzętowym teście poprzedniego rdzenia wdrożono następny etap w tej samej aktywnej aplikacji: **automatyczną negocjację SID16** ze stanami DISCOVER/WAIT_READY/WAIT_READY_ACK/WAIT_CONFIRM/ACTIVE oraz wiadomościami HELLO, BEACON, WELCOME, READY, READY_ACK w sprzętowym ACK Payload. BODY aplikacji nadal nieinterpretowane przez MAC.
- `paired_session.h` jest przenośną maszyną stanów; każda strona niesie 32-bitową epokę uruchomienia i 16-bitową próbę, SID16 wynika z CRC16 pełnych obu nonce. Bezpieczeństwo przed typowymi starymi pakietami przez weryfikację nonce przy handshake; SID16 nie jest uwierzytelnieniem kryptograficznym ani absolutną gwarancją niekolizyjności.
- `epoch_journal.h` rozwiązuje wielokrotnego użytku rotacyjny zapis 16 rekordów EEPROM z CRC8, markowaniem poprawności zapisu na końcu oraz testami urwania zapisu; `boot_epoch.cpp` jest platformowym adapterem AVR. EEPROM aktualizuje się na reboot, nie na każdy pakiet i nie na każde HELLO.
- `nrf24_ack_phy.h` otrzymał czyszczenie starej sesji A i bezpieczną wymianę ACK Payload B przy zmianie stanu (550µs guard, flush obu FIFO, 150µs PRX rearm). Obsługa timeoutów ma ograniczone terminy i nie resetuje w nieskończoność nonce w DISCOVER; nie naruszono naprzemiennej fizyki DATA↔ACK.
- Testy hostowe obejmują przynajmniej: pipeline BEACON→HELLO→WELCOME→READY→READY_ACK→DATA, reset B, reset A, stare BEACON, niepoprawne SID/CONTROL, ponowne nonce, zawinięcie licznika prób, przerwany zapis EEPROM, czyszczenie FIFO. Krzyżowa kompilacja AVR-GCC dla ATmega328P i host testy **PASS** (workflow po zmianach). Nie uruchomiono jeszcze tej nowej wersji na fizycznych RF-Nano; ostatni potwierdzony wynik **625/s dotyczy poprzedniego firmware bez handshake'u**.
- Dokumenty README, paired-core-v2, plan, todo, goal, migracja i AGENTS zmieniono zgodnie z faktyczną implementacją i brakiem weryfikacji fizycznej. Nadal brak prawdziwego sinku/`RX_CREDIT` i usług TCP-like/file/szyfrowania.

## 2026-11-04 — pełna symulacja potoku sprzętowego ACK Payload podczas negocjacji

- Zastano na HEAD gotową implementację `paired_session.h`, `epoch_journal.h`, adapter `boot_epoch.cpp` i integrację w `main.cpp`, wraz z testami hostowymi i kompilacją AVR. Nie zastąpiono jej alternatywnym handshake ani nie nadpisano zmian innego autora.
- Dodano niezależny, wielokrotnego użytku hostowy model wymiany w `tests/paired_session_pipeline_test.cpp`, który odwzorowuje kluczową semantykę nRF: odpowiedź PRX jest przygotowana w kolejce przed następnym pakietem PTX, a informacja o zmianie sesji może przyjść z opóźnieniem.
- Test PASS w GitHub Actions (2026-11-04): początkowe sparowanie, 300 aktywnych wymian, zmiana epoki po resecie B, A i obu, seria utraconych ACK w negocjacji, restart próby po timeoutcie, przerwa w kierunku A→B. Potwierdza zgodność modelu stanów, nie działanie resetów na fizycznym radiu ani EEPROM podczas przerw zasilania.
- W bieżącej interwencji **nie zmieniano produkcyjnego firmware**, pinów, czasu transmisji ani F7/F12. Fizyczny test `ST=4` i zgodnego niezerowego `SID` po resetach pozostaje otwarty.

## 2026-11-04 — potwierdzony sprzętowo handshake i wznowienie po pojedynczych resetach

- Dwa zrzuty RealTerm od użytkownika pokazują działający nowy firmware z `PairedSession` i sprzętowym ACK Payload po **oddzielnych resetach B i A**. W stanie ustalonym oba urządzenia są `ST=4`, utrzymują **ten sam niezerowy `SID`** i około **612–613 kompletnych wymian/s**.
- Po restarcie **B/PRX**, wcześniej aktywna sesja (na B `SID=47256`) została zastąpiona wspólnym `SID=12715` (`ST=4` na A i B). Na A, które pozostało uruchomione, licznik `R=1`; po rozruchu B `R=0`. W czasie niedostępności B A raportuje `ARC=330/MAX=110` oraz `ARC=657/MAX=219`: dokładnie **3 retransmisje na MAX_RT**, zgodnie z `SETUP_RETR=0x23`; w części okien `TX/s=0`. Po nawiązaniu nowej sesji A `TX/s=613 ACK/s=613`, B `RX/s=612–613 Q/s=612–613`.
- Po restarcie **A/PTX**, B pozostający w pracy przechodzi przez `ST=0 SID=0` i `R=1`, po czym obie strony osiągają `ST=4 SID=16598`; A po własnym resecie ma `R=0`. Powraca stabilne `613/s` na A i `612–613/s` na B, `ARC/MAX/BAD/IO/GAB/GBA/DROP/FULL=0` w widocznych oknach stabilnych.
- W porównaniu z fizycznym testem **625/s** bez nowego handshake'u, wynik **612–613/s** oznacza **~1,9–2,1%** niższe tempo, a średni odstęp wzrasta z **1600 µs** do **1631–1634 µs** (~31–34 µs). Radio pozostaje 1 Mb/s, 32 B przy pełnych DATA, SPI 8 MHz i sprzętowym ACK Payload; mały narzut prawdopodobnie wynika z dodatkowego kodu sesji/diagnostyki i nie został osobno profilowany. Podczas resetu występują **realne retransmisje nRF** i timeouty negocjacji, więc znaczny chwilowy spadek nie jest wyłącznie narzutem procesora.
- Około **612–613 × 24 = 14 688–14 712 B/s** binarnego BODY **na kierunek**, w warunkach testu; nie jest to goodput końcowego UART. Średni okres 1,63 ms nie jest osobnym pomiarem latencji pojedynczej ramki.
- **Zaliczone:** samoczynny powrót po pojedynczym resecie A i B, ze zgodnym świeżym SID i bez trwałego zakleszczenia; nie zaobserwowano błędów w ustalonej wymianie. **Niezweryfikowane:** reset obu płytek jednocześnie, rzeczywisty power-loss radia, dłuższa utrata zasięgu, pełna kontrola RX_CREDIT. Dokumentacyjny zapis, **bez zmian firmware**, F7/F12 i repozytoryjnego PHY.

## 2026-11-04 — fizyczny jednoczesny restart pary i tożsamość trwała vs sesyjna

- Kolejny zrzut RealTerm pokazuje komunikaty startowe na **obu** RF-Nano (`PAIR CORE B PRX ...` oraz `PAIR CORE A PTX ...`), nowy wspólny `SID=13618` i stabilne `ST=4 R=0` po obu stronach. Poprzednia wspólna sesja na zrzucie: `SID=16598`. Oznacza to **potwierdzony w praktyce restart obu ATmeg i automatyczny powrót handshake do sesji ACTIVE**. A pokazuje stabilne `TX/s=613 ACK/s=613`; B `RX/s=612–613 Q/s=612–613`; w stanie ustalonym widoczne `ARC/MAX/BAD/IO/GAB/GBA/DROP/FULL=0`. Niewielka przejściowa `GAB=2` w starym stanie przed resetem nie jest oznaką trwałego błędu nowej sesji.
- Objaśniono użytkownikowi rozdział **stabilnego identyfikatora urządzenia/pary** (może być zachowywany w EEPROM i prezentowany po restarcie) od **zmiennego 16-bitowego SID sesji** służącego odrzucaniu starych DATA i zalegających ACK Payload po restarcie. W aktualnym `PairedSession::derive()` SID powstaje z obu `Identity{bootEpoch32, attempt16}` poprzez CRC16; epoka jest inkrementowana w dzienniku EEPROM przy starcie. Stały SID bez osobnej generacji/epoki osłabiłby wykrywanie starych ramek. Stabilne pair/device ID to pomysł na oddzielną warstwę parowania, nie obecna funkcjonalność.
- Ta aktualizacja dokumentacyjna **nie zmienia formatów ramek, implementacji sesji, EEPROM ani F7/F12**; brak programowego wymuszania stałego SID.

## 2026-11-04 — binarny most UART w obu kierunkach na istniejącym rdzeniu ACK Payload

- Zaimplementowano wielokrotnego użytku `UartReliableStream<Port>`, niezależny od AVR, plików i projektu. BODY do 24 B jest nadal opaque dla rdzenia; usługa interpretuje 1B DATA_SEQ, 1B ACK_SEQ i przekazuje do 22B niezmodyfikowanych bajtów UART. Dwa kierunki działają niezależnie w jednej wymianie DATA/Ack Payload, bez przełączania ról radia.
- Transport: stop-and-wait (okno=1 na kierunek), retransmisja porcji aż do aplikacyjnego ACK, deduplikacja, `UART::writeAll` atomowe, `RX_CREDIT` wyliczane z faktycznego wolnego miejsca w TX UART, brak ACK dla nieprzyjętych danych. Pending segment utrzymywany w RAM nadajnika podczas rekey po resecie partnera. Pełne exactly-once między restartami ani nieograniczony flood bez RTS/CTS nie są gwarantowane.
- Przepisano jedyną aplikację `main.cpp` z generatora wzorca na realny UART↔radio↔UART. Brak UART logów, aby nawet bajty 00/FF/CR/LF przechodziły transparentnie. Gdy idle, PTX wyzwala ACK Payload około 100 razy/s; przy danych wraca do szybkiego cyklu RF. Osiągnięta przepustowość end-to-end **nie została jeszcze zmierzona fizycznie**.
- Dodano `tests/uart_reliable_stream_test.cpp` oraz test długiego obustronnego strumienia `tests/uart_reliable_stream_stress_test.cpp` (4096B/kierunek, losopodobny deterministyczny wzorzec, utraty ramek i ACK Payload, zatrzymany sink, wrap seq), oba PASS. GitHub Actions host+AVR-GCC C++11 **PASS**. Nie wykonano jeszcze F7 w Windows ani ręcznego F12, ani fizycznego testu nowego mostu.
- Zachowano wcześniej fizycznie przetestowany handshake A/B/obu, SID16 i EEPROM; odłączono tylko tekstową diagnostykę od jedynego UART. Utrzymano pinout, C++11, `PackStructureMembers=False`, brak malloc/new i brak automatycznego Post-Build.

## 2026-11-04 — fizyczny UART: pojedynczy strumień poprawny, jednoczesny flood ujawnia ubytki

- Użytkownik potwierdził **działanie fizycznego transferu UART↔nRF24↔UART z jednym nadawcą**. W teście równoczesnego wysyłania plików z obu RealTerm obserwuje **brak części bajtów**. Na zrzucie RealTerm konfiguracja nie ma hardware flow control (`None`), a przy wysyłaniu obu plików są ustawione `Delays=0,10` (pierwszy per-character bez przerwy, drugi per-line 10ms); średniej liczby bajtów/s ani liczników UART z fizycznych płytek nie odczytano. Odbiór dwukierunkowego flood jako niezawodny jest **niezaliczony**; dokładnego miejsca utraty na płytkach nie dowiedziono.
- Analiza kodu: UART jest asynchronicznym **pełnym dupleksem** z osobnymi ISR RX i UDRE oraz osobnymi 128-slotowymi pierścieniami (127B użytecznej pojemności). SPI nRF działa z włączonymi przerwaniami, poza krótkimi sekcjami ATOMIC. Zatem sprzęt UART nie wymusza półdupleksu.
- Stop-and-wait wysyła max **22 B na segment** i musi otrzymać oddzielny aplikacyjny ACK; ACK Payload jest przygotowany przed odebraniem bieżącej ramki. Dla referencyjnych **~613 RF wymian/s** i ~jednego nowego segmentu co 2 wymiany ograniczenie orientacyjne wynosi **~6,74 kB/s/kierunek**. UART 115200 8N1 może podawać maksymalnie **11,52 kB/s/kierunek**. Różnica ~4,78kB/s potrafi zapełnić 127B RX ring w około **27 ms** przy nieprzerwanym maksimum; konkretne tempo nowego mostu w flood nie zostało zmierzone. Gdy oba kierunki używają pełnych 32B pakietów RF, wymiany są dodatkowo dłuższe niż 32B+krótki 10B ACK przy nadawaniu tylko w jedną stronę.
- Wcześniejszy `uart_reliable_stream_stress_test.cpp` podawał jedynie **3 bajty na wymianę** i nie podawał następnych, gdy wejście się zapełniło, co symulowało *idealny flow control*, którego użytkownik w realnym RealTerm nie ma. Ten test potwierdza poprawność stop-and-wait przy niskim obciążeniu, nie dostępność 115200 bodów w dwóch kierunkach.
- Dodano wielokrotnego użytku regresję `tests/uart_unthrottled_ingress_test.cpp`: źródła wprowadzają 3 albo 19 B na każdą z 600 transakcji RF, **ignorując wolne miejsce UART RX**; rzeczywisty stop-and-wait i 1-wymianowy delay ACK, brak celowych błędów RF. Sprawdzono **brak utraconych bajtów przy 3 B/wymianę i reprodukowalne zapełnienie obu RX przy 19 B/wymianę** (11 400 oferowanych bajtów/stronę); wyniki host PASS i krzyżowy AVR build PASS. Test hostowy dowodzi mechanizmu przeciążenia, ale nie izoluje sprzętowej przyczyny każdego braku na zrzucie.
- Aktualny `UART::Counters` już udostępnia `rx_buffer_full` (software ring drop), `rx_overrun` (hardware USART DOR0), `rx_framing` i `rx_parity`. `UartReliableStream::blocked` oznacza świadome odmówienie zapisu do TX, nie utratę RX. Statystyki nie są drukowane na jedynym COM, bo zepsułyby transparentność. Przy testach sprzętowych najlepiej obserwować je debugerem lub przez zaprojektowany osobny kanał diagnostyczny.
- Nie zmieniono jeszcze produkcyjnego rdzenia, parametrów F7/F12 i fizycznej konfiguracji. Następny krok: próba obu kierunków przy `Delays` **1 ms/znak, 0 ms/linia**, potem stopniowe przyspieszanie; pomiar danych przez binary capture i stanów UART. Docelowa poprawa: pipeline z oknem 2+ i zapobieganie przepełnieniu wejścia poprzez realny flow-control lub kontrolowane tempo nadawcy. Sam wzrost bufora bez kontroli obciążenia tylko opóźnia stratę.

## 2026-11-04 — okno 4 zamiast stop-and-wait, ciągły ping-pong bez idle sleep

- Po jednoznacznym wymaganiu użytkownika usunięto z `main.cpp` świadomie wprowadzony 10ms idle polling; A/PTX wyzwala kolejną wymianę nRF DATA↔hardware ACK Payload również bez danych. Pozostawiono tylko minimalne timingi PHY i rzeczywiste retry.
- Odróżniono **sprzętowy ACK** (nRF potwierdza każdą poprawnie odebraną transmisję; załadowany ACK Payload odsyła przygotowane wcześniej dane) od **APP_ACK** w BODY (potwierdza przyjęcie użytkowych bajtów do UART TX). Poprzedni stop-and-wait zatrzymywał nowe DATA do nadejścia APP_ACK, mimo że radio mogło w tym czasie obsłużyć kolejne pakiety.
- `UartReliableStream<Port>` jest teraz wielokrotnego użytku transportem **window=4**; 4×22B static cache, 8-bit DATA_SEQ, kumulacyjne ACK_SEQ, Go-back-N z retransmisją od najstarszego niezackowanego segmentu po 8 wymianach bez postępu. Odbiornik akceptuje wyłącznie kolejny DATA_SEQ i atomowo wykonuje `writeAll`. Nowe dane nie muszą czekać na ACK poprzedniego segmentu, a 8B MAC i 24B opaque BODY pozostają bez zmian.
- Wykryto i naprawiono warstwowo problem renumeracji kilku segmentów oczekujących po rekey: fizyczny indeks bufora jest teraz relatywny do `headIndex_`/najstarszego oczekującego segmentu, nie do starego `seq%WINDOW`. Test regresyjny obejmuje baseSeq≠0 oraz trzy oczekujące segmenty po renegocjacji.
- Host CI PASS na 11 zestawach testów: 4096 B/kierunek pod stratami, brak pierwszej ramki, retransmisje i odtworzenie, reset SID z 3 segmentami, wrap numeracji, UART TX full, unthrottled producer. Symulacja idealnego RF z 600 transakcjami i 19 B na transakcję na każdym UART: **11400B oferowane per node, 0 overflow**; 24 B/transakcję: **14400B oferowane i odtworzone overflow 2855/2811 B** (A/B). Test nie jest pomiarem rzeczywistego UART ani RF.
- Cross-build AVR-GCC PASS: ELF `.text=10858B`, `.data=96B`, `.bss=591B`, 687B stałego SRAM, reszta z 2048B na stos i inne potrzeby. Ręczne Microchip Studio F7/F12 niezmienione, brak Post-Build. Nowej wersji **nie wgrywano i nie testowano jeszcze na dwóch RF-Nano**.
- Istniejący limit UART115200 8N1 = 11520 B/s/kierunek. Matematycznie 22B user × 613 wymian/s ≈ 13486B/s/kierunek to tylko potencjał, nie potwierdzony goodput. Przepustowość i rozkład opóźnień nowego kodu wymagają fizycznego pomiaru.

## 2026-11-04 — protokół 8+24 bez drugiego nagłówka w BODY

- Właściciel potwierdził, że przy dwóch jednoczesnych nadawcach z `Delays=0,0` nadal są widoczne nieprawidłowości strumienia, i jednoznacznie odrzucił dodatkowy dwubajtowy transportowy preambuł w 24B BODY. BODY ma być **w całości danymi użytkownika**, a radio ma stale wykonywać DATA↔hardware ACK Payload, także na pusto.
- Zweryfikowano aktualny `main.cpp`: A/PTX wykonuje kolejną transmisję w każdej iteracji stanu ACTIVE bez czekania na wejście UART. B/PRX ma wcześniej przygotowany ACK Payload; konieczne timingi PHY to strażnik **130 µs** po zakończeniu operacji PTX (260 taktów Timer1 2 MHz), impuls CE 15 µs, rzeczywista transmisja i ewentualny retry. **Nie ma 10/20/25 ms sztucznych przerw ani stop-and-wait.** Oczekiwanie na flagi TX_DS/MAX_RT jest pracą przy rzeczywistym nRF, nie aktywnym czekaniem na dane aplikacji.
- Odkryto dublowanie metadanych: poprzedni MAC miał `TX_SEQ` i `RX_LAST` dla każdej RF ramki, a `UartReliableStream` wpisywał ponownie `DATA_SEQ` i `APP_ACK` do BODY[0:2], pozostawiając tylko 22 B. Dodatkowo `main` odrzucał duplikaty na poziomie `PairedLink`, zanim warstwa UART mogła odczytać z nich **nowsze potwierdzenie drugiego kierunku**.
- Nowa, jedyna aktywna semantyka: `DATA.TX_SEQ` jest **numerem nowego segmentu użytkownika modulo256**, powtarzanym bez zmiany przy retransmisji; `DATA/IDLE.RX_LAST` jest **kumulacyjnym potwierdzeniem ostatniej kolejnej porcji atomowo przyjętej do UART TX**. Bez danych pakiet `IDLE` ma **8 B całkowitej szerokości DPL** i bieżące ACK/credit, a nie 10 B; nie zajmuje numeru segmentu. Przy pełnym ruchu pakiet `DATA` ma **8 B nagłówka + 24 B surowych bajtów UART**, bez metadanych w BODY. Nadal WINDOW=4, Go-back-N, negotiated SID16 i hardware ACK Payload.
- `PairedLink` oferuje pojedynczy interfejs `makeStream/observeStream`; usunięto sprzeczną semantycznie stary `make/receive` używany wyłącznie w dawnych testach. `observeStream` zlicza fizycznie odebrane pakiety i zgłasza duplikaty/gapy **numerów segmentów danych**. Nie wolno przedstawiać pola TX_SEQ jako niezależnego licznika wszystkich ramek radiowych — nie da się mieć obu znaczeń w tym samym bajcie. FRAME CONTROL podczas handshake'u ma własną numerację w `PairedSession`.
- `UartReliableStream` nie modyfikuje BODY, utrzymuje 4×24B statycznego cache, przyjmuje ACK ze wszystkich poprawnych DATA/IDLE, w tym z retransmitowanego DATA. Odbiornik nadal potwierdza dopiero po atomowym `UART::writeAll`; ACK nRF nie jest dostarczeniem użytkowych bajtów.
- Hostowe testy: `paired_link_test` aktualizuje semantykę nagłówka, `uart_reliable_stream_test` sprawdza pełne 24B, czyste 8B IDLE oraz nowszy ACK w retransmitowanym DATA, `uart_pipeline_rekey_test` sprawdza okno przy rekey, `uart_reliable_stream_stress_test` 4096B/kierunek, `uart_unthrottled_ingress_test` oversubscription. **Host PASS i AVR-GCC cross-build PASS** (workflow CI 2026-11-04). To **nie** jest fizyczny pomiar nowego formatu na RF-Nano.
- Brak zmian w F7/F12, EEPROM sesji, konfiguracji radia, pinoucie, CRC16, SPI8 MHz, globalnym PackStructureMembers=False lub Post-Build. Przepustowość 24×612≈14,7kB/s/kierunek jest tylko hipotetycznym sufitem przy **historycznym** tempie RF; aktualnej szybkości i integracji dwóch UART nie zmierzono.

## 2026-11-04 — PP nazwany i zachowany; rozpoczęty projekt profilu BURST20

- Właściciel pozytywnie ocenił ostatni fizyczny zrzut jednoczesnych przesyłek UART w obu kierunkach na PP, nadal bez binarnego porównania plików. Nadał protokołowi oficjalną nazwę **PP (Ping-Pong)**. Zabezpieczono niezmieniony PP commit `784fd83` na dedykowanej gałęzi `checkpoint/pp-2026-11-04`.
- Policzono rzeczywisty czas 32B ESB przy 1Mbps, 5B adresem i CRC16: `329 bits = 329us`; 5ms mieści najwyżej 15 takich ram przy pominięciu całości narzutów. Koncepcja 32 DATA +1 REPORT w5ms jest fizycznie niemożliwa (10,857ms samego eteru). Przy 2Mbps wymaga 5,4285ms samego eteru. Źródło Nordic Product Specification v1.0 rozdz. 7.3/7.7.
- Zaprojektowano drugi profil **BURST20**, 20ms stałego cyklu, kwanty 5ms, przydziały A/B=5/15,10/10,15/5; ostatnie 1,0ms każdego slotu na REPORT, 0,3ms GUARD na przełączenie kierunku. `GPT_NRF2401/burst20_schedule.h` to przenośny scheduler z deadline i bezpieczną odmową nadawania gdy nie zsynchronizowany. Testy trzech przydziałów, granic slotów, wrap Timer1 i długiego skoku czasu w `tests/burst20_schedule_test.cpp`.
- `GPT_NRF2401/profile_select.h` implementuje politykę wyboru trybu z A1/PC1 (internal pullup: HIGH/floating=PP, LOW/GND=BURST20), dwa odczyty po 50us i fail-safe PP gdy pin niestabilny. Mock test `tests/profile_select_test.cpp`.
- **Nie zmieniono `main.cpp`, firmware nadal uruchamia tylko PP.** Wdrażanie rzeczywistego 20ms nadawania, NO_ACK, przełączania ról, ramki kontrolnej i dyspozytora strap nastąpi dopiero w kolejnych krokach. Nie przypisywać nowemu profilowi prędkości ani poprawnego działania na sprzęcie. Dokument: `burst-20ms.md`.

- Uzupełniono BURST20 o portable `burst20_report.h` — 24B body końcowej ramki kontrolnej z walidacją, serializacją little-endian, przyrostowym CRC32/ISO-HDLC i idempotentną bitmapą 32 pakietów. `tests/burst20_report_test.cpp` weryfikuje kodowanie 32B `PairedFrame`, granice, brak mutacji po błędzie, znany wektor CRC32 oraz deduplikację bitmapy. Host i AVR cross build PASS; raport nie jest jeszcze wysyłany w fizycznym BURST20.

# 2026-11-04 — gałąź eksperymentalna PP echo/ramp (nie `main`)

- Na odrębnej gałęzi `experiment/pp-echo-ramp-2026-11-04` z bazy
  `6b94a43` przygotowano tymczasowy adapter `pp_echo_ramp_port.h`:
  A generuje kolejno 00..FF od 10 ms z krokiem 100 µs na `s`/`w`, B
  odbija dane RF do własnego nadajnika ACK Payload, A wypisuje surowe
  echo na UART. Nadal działa PP z oknem czterech segmentów, SID i
  retransmisjami. Nie zmieniono samego kodu PHY/transportu.
- Źródło A jest ograniczone do 120/127 bajtów w kolejce; przy nasyceniu
  nie nadpisuje ani nie pomija numeru. Zliczane są bajty źródłowe,
  nasycenia, echo i nieciągłości sekwencji, bez tekstu na UART.
- `pp_echo_ramp_test.cpp` sprawdza krok/limity, wrap zegara i wartości
  bajtów, atomowość echa B oraz model PP z opóźnionym ACK Payload.
- Wariant nie był jeszcze testowany fizycznie; instrukcja oraz procedura
  przywrócenia normalnego PP znajdują się w `echo-ramp-test.md`.

## 2026-11-04 — fizyczny wynik echo 12 000 CPS; test graniczny PP bez UART payload

- Właściciel uruchomił eksperyment na dwóch RF-Nano, po poprawnym wysłaniu
  komend `s` na A/COM10 pokazał ciągłą sekwencję `00..FF` i około
  **12 000 CPS** w RealTerm 115200 8N1. To wskazanie RealTerm w pobliżu
  granicy UART, nie niezależny pomiar przepustowości radiowej; nie
  wykonano binarnego porównania całego strumienia.
- Na tej samej gałęzi testowej dodano tryb `RADIO_STRESS` do przenośnego
  `PpEchoRampPort`: A generuje od razu z nasyceniem źródła, odbierane
  echo sprawdza w RAM bez TX UART i wystawia stałe niezależne od UART
  kredyty. B liczy zaakceptowane i przygotowywane do zwrotu bajty.
  Źródło nadal ma limit 120/127B i obsługuje `s`/`w` co100 µs.
- Dodano przenośny `pp_stress_telemetry.h`: 1-sekundowe okno pomiarowe
  Timer1 (z obsługą zawinięcia), liczniki CPS i zdarzeń (sprzętowe
  `OBSERVE_TX.ARC_CNT`/`MAX_RT` na A, aplikacyjne replay/duplikaty,
  nieciągłości echa, odmowy bufora, inne błędy i odzyskiwanie SID)
  oraz bezpieczny ograniczony format linii ASCII na UART A i B.
  W raporcie nie ma binarnych danych użytkownika; nie zmienia to PP PHY,
  handshake'u ani formatu radiowego. B nie udaje własnego licznika ARC.
- Rozbudowane testy hostowe obejmują tryb bez UART, nasycenie kolejki,
  przesył przy modelowych stratach ramek/ACK, poprawną sekwencję,
  wrap zegara i licznika16, częstotliwości, format i granice bufora.
  **Testy hostowe PASS**; kompilacja AVR w tej sesji nie była dostępna.
  Brak fizycznego testu nowego firmware i wyników izolowania radii.

## 2026-11-04 — potwierdzona histereza przy pełnym obciążeniu, diagnostyka warstwowa

- Właściciel pokazał powtarzalne ~4700 B/s po uruchomieniu A od razu z
  pełną kolejką (`A ret≈195/s`, `B bad≈195/s`, `arc=0`, `max=0`), ale
  ~12 552 B/s bez retry po zejściu `w` do ~100 B/s i powrocie `s` do
  maksimum. Sam reset A ponownie wywołał wolny stan w nowym SID.
  `gap=0` w odebranym echu nie dowodzi braku strat przy każdej zmianie
  sesji. Osłabienie sygnału radiowego nie wyjaśnia stabilnego podziału.
- Hostowa symulacja bez limitu sprzętowego RX FIFO nie odtworzyła
  degradacji pełnego obciążenia. Wersja eksperymentalna została zatem
  wyposażona w **pasywne** liczniki: A `ack` (aplikacyjnie potwierdzone
  bajty/s), `win` (liczba oczekujących segmentów); B `lg` (MAC gaps/s),
  `ls` (stare DATA/s), `rf` (odczyty RX_FULL/s przy istniejącym pollingu
  FIFO_STATUS), `af` (ACK TX FIFO FULL/s). `rf` ani `af` nie są
  bezpośrednim licznikiem fizycznych ubytków. Nie ma nowych transakcji
  SPI ani zrzutu każdego pakietu po UART.
- Testy hostowe dla zmian pola RX_FULL, wrap liczników i zmiany SID;
  **do pomiaru na dwóch RF-Nano, bez ustalonej jeszcze przyczyny**.
  To świadomie jednorazowy instrument eksperymentalny, nie zmiana
  produkcyjnego PP. `main` i checkpoint PP nietknięte.

## 2026-11-04 — PP recovery v2: ciepłe/zimne, niepewne dane, status hosta

- Gałąź `experiment/pp-recovery-v2`, od poprzedniego wariantu testowego;
  produkcyjne `main` i `checkpoint/pp-2026-11-04` nietknięte.
- Analiza kodu wykazała, że wcześniejsze watchdogy aktywnego linku
  restartowały handshake wyłącznie wskutek milczenia radia (A 250 ms,
  B 750 ms), przez co możliwe było nieskoordynowane wyzerowanie SID i
  okna mimo zachowanej pamięci drugiej strony. Nie stanowi to jeszcze
  dowodu przyczyny konkretnego spadku CPS.
- Zaimplementowano przenośną `PpRecoveryControl` bez zależności AVR:
  po ~4,8 ms bez poprawnego peer DATA/IDLE `SUSPECT`, po 1 s `OFFLINE`,
  po 5 s rozmyślne porzucenie ciepłego wznowienia i świeży handshake.
  Przy krótszym zanikaniu SID, sekwencje i niepotwierdzone porcje
  pozostają bez zmian. B respektuje to samo rozróżnienie bez inicjowania
  transakcji nRF. OFFLINE A wysyła IDLE co 20 ms.
- `UartReliableStream::make(...,permitData)` pozwala wysłać samo IDLE
  z aktualnym ACK, bez poboru nowych bajtów i bez replay starej DATA.
  `PpEchoRampPort::service(now,permit)` atomowo zatrzymuje lokalny
  generator, nie nadpisując kolejki ani nie gubiąc numerów wzorca.
- `HostFlowStatus` definiuje **dwa bajty lokalne**, nie radiowe:
  STOP/UNCERTAIN + wolne miejsce kolejki TX, PEER_OFFLINE/RX_PRESSURE +
  wolne miejsce sinku RX. Domyślny tryb wielokrotnego użytku **HOLD**
  wstrzymuje niepewne porcje do świadomej decyzji hosta. **Tymczasowy
  eksperyment** wybiera `RETRY_AT_LEAST_ONCE`, aby automatycznie wznowić
  test po resecie, zachowując flagę możliwego zdublowania bajtów.
- Krótkie linie diagnostyczne o zmianach stanu poza dotychczasowym
  1 Hz CPS. Nie ma zmian 8B nagłówka, 24B opaque BODY, ACK Payload,
  radia 1 Mb/s, UART 115200, EEPROM boot, pinów i F7/F12.
- **Host PASS:** 18 automatycznie odnalezionych testów C++11, w tym nowe
  `pp_recovery_control_test`, `pp_recovery_handshake_test`, wydłużenie
  `pp_echo_ramp_test`. Symulacja obejmuje reset samego A, samego B, obu,
  ciepłe odzyskanie starego SID, 5 s braku kontaktu, zapełnienie,
  wstrzymanie producenta i rollover32. **Brak cross-build AVR oraz testu
  fizycznego:** lokalnie nie było `avr-g++` ani fizycznych RF-Nano.
- Instrukcja F7/F12 i próby na obu urządzeniach: `pp-recovery-v2.md`.
- Na tej gałęzi rozszerzono filtrowanie uruchomień GitHub Actions `push`
  z `main` także na `experiment/**`, żeby eksperyment zidentyfikował
  niepowodzenie AVR-GCC PRZED ręcznym flashowaniem; produkcyjna gałąź
  nadal jest niezależna. Wyniku CI nie uznawać za próbę radiową.

## 2026-11-04 — naprawa transportu PP po utracie ACK, test do wykonania na RF-Nano

- Po pokazanych przez właściciela restartach pojedynczych płytek
  zauważono stan A `cps≈5544, ret≈230/s`, B `bad≈230/s`, bez dużej
  wartości ARC/MAX, a po resecie A również `cps≈14880, ret=0`.
  To wskazuje na pętlę brakującego segmentu i powtarzanych prób;
  zależność od resetu B jest obserwacją sprzętową, przyczyna wymaga
  potwierdzenia poprawki na płytkach.
- Na osobnej gałęzi `experiment/pp-recovery-ack-fix` poprawiono
  przenośny `UartReliableStream`: powtórka wcześniej mogła omijać
  `RX_CREDIT=0`, choć odbiorca nie mógł przyjąć segmentu. Teraz
  pozostaje samo IDLE/ACK, a pakiet nie jest usuwany ani nadpisywany.
- Odbiorca Go-back-N oznacza brakujący segment istniejącym bitem
  `PairedFrame::URGENT`, a nadawca po `RX_LAST` dostaje jeden natychmiastowy
  impuls replay na dany brakujący numer; starsze skopiowane ACK Payload
  nie wyzwalają replay bez końca. Semantyka bidirectional, 8B header,
  24B opaque BODY i WINDOW4 bez zmian; PHY i diagnostyki nie rozbudowywano.
- Nowe testy hostowe: `pp_credit_stall_test`, `pp_fast_retransmit_test`,
  `pp_rekey_full_load_test` oraz dostosowanie dwóch istniejących testów
  do inicjalnego kredytu zero. **Wszystkie 21 testów hostowych PASS**
  przez `sh tests/run_host_tests.sh` z g++ C++11 i `-Werror`.
  Model nasyconego radia nie symuluje wiernie kolejki ACK Payload
  nRF24, zatem sam nie odtwarza fizycznego spowolnienia.
- Lokalnego `avr-g++` i pary RF-Nano brak. Cross-build AVR, F7/F12 oraz
  test „reset samego B pod pełnym obciążeniem” pozostają otwarte.
  Główna gałąź i poprzednie eksperymenty bez zmian.

## 2026-11-04 — reprodukcja utraty ogona replay po częściowym ACK

- Właściciel wgrał eksperyment `pp-recovery-ack-fix` i pokazał
  rzeczywistą poprawę po resecie B i obu naraz (~14,8 kB/s,
  `ret=bad=0`). Natomiast **reset A** wywołał utrzymujące się
  `B ret≈377–378/s`, `A bad≈377–378/s`, przepustowość około 9 kB/s.
  `arc/max` przy ustalonej sesji były bliskie zeru. Trzy zrzuty z
  RealTerm nie zawierają binarnego porównania bajtów.
- Na gałęzi `experiment/pp-rekey-replay` rozszerzono model pełnego
  obciążenia o zimny restart A przy ocalałym B i jego kolejce ACK.
  Idealny model RF nadal nie odtworzył plateau; nie wystarcza on do
  potwierdzenia fizycznego rozwiązania.
- **Odizolowany błąd transportu odtworzony czerwonym testem:** po
  czterech niepotwierdzonych DATA B renegocjuje SID, ponawia DATA0,
  odbiera ACK0 i nieprawidłowo wyrzuca replay DATA1..3, od razu
  wysyłając nowy DATA4. `tests/pp_partial_ack_rekey_test.cpp` padał
  dokładnie na błędnym `TX_SEQ`, przed zmianą kodu.
- Poprawiono `UartReliableStream::acceptAck`: przesuwa się tylko
  potwierdzony prefiks i zachowuje jeszcze niewysłany ogon replay,
  również przy opóźnionych ACK, bez dodatkowych bajtów, liczników,
  stanu pamięci trwałej i zmian w PHY. Nowy test jest zielony.
- `sh tests/run_host_tests.sh`: **22/22 PASS**, g++ C++11 `-Werror`.
  Kompilator AVR i urządzenia nadal dostępne wyłącznie u właściciela;
  F7/F12 i fizyczne sprawdzenie pozostają otwarte.

## 2026-11-04 — po spowolnieniu i przyspieszeniu źródła samoczynny powrót pełnej prędkości

- Właściciel fizycznie powtórzył test na dwóch RF-Nano z gałęzi
  `experiment/pp-rekey-replay`: start B→A i A→B stabilizuje się na
  ~14,8 kB/s; ponowne otwarcie portu B przy działającym A może dawać
  utrzymujące się ~9 kB/s z `B ret≈378/s`, `A bad≈378/s`.
- Kluczowe potwierdzenie: bez zmiany `sid=54543`, resetu ani zmiany
  fizycznego radia ręczne obniżenie tempa `w` do ~100 CPS i ponowne
  przyspieszenie `s` przywróciło ~14,8 kB/s oraz `ret=bad=0`.
  Wskazuje to na samopodtrzymujące się retransmisje pod pełnym
  obciążeniem, nie na trwałą utratę RAM sesji. Nadal brak pomiaru
  liczby hardware ACK_EMPTY; `max=0` nie dowodzi dostarczenia danych
  z osobnej kolejki ACK Payload.
- W nowej osobnej gałęzi `experiment/pp-window-drain` dodano w
  `UartReliableStream` fazę „drain existing window” po NACK, timeoutcie
  lub nowym SID z niepotwierdzonym cache. Nowe DATA czeka, aż wszystkie
  zaległe segmenty zostaną potwierdzone; IDLE, ACK, kredyty i retransmisje
  są nadal czynne. Brak dodatkowego bajtu radiowego, rozbudowy
  diagnostyki, zmian w `main.cpp`, PHY oraz konfiguracji radiowej.
- Test `pp_recovery_drain_test` **reprodukował błąd**: przy aktywnej
  luce i opóźnionym ACK nadajnik wypuszczał nowy DATA3 z pełną
  prędkością, nie czekając na zakończenie powtórek. Na naprawionym
  kodzie nadaje IDLE, potem wznawia DATA3–4 i zachowuje 120B.
- `sh tests/run_host_tests.sh`: **23/23 PASS** (C++11, `-Werror`).
  **Bez wyników AVR build / F12 / sprzętowej próby nowego wariantu**;
  stabilna prędkość na RF-Nano pozostaje hipotezą do potwierdzenia.

## 2026-11-04 — próba rzeczywista `pp-window-drain`: oba pojedyncze resety odzyskują pełną prędkość

- Właściciel wgrał wariant `experiment/pp-window-drain` na dwa RF-Nano
  i pokazał kolejno: start **najpierw B, potem A**, reset samego B,
  reset samego A. Zrzuty RealTerm: `obraz(20261104-204708).png`,
  `obraz(20261104-204805).png`, `obraz(20261104-204834).png`.
- Po uruchomieniu obu urządzeń: obie strony `sid=49792`, `up=1`,
  około 14,7 kB/s i `ret=bad=0` w stanie ustalonym.
- Po resecie B: przejściowe renegocjacje; końcowo wspólne
  `sid=52044`, `up=1`, około 14,7 kB/s i `ret=bad=0`.
- Po resecie A: przejściowe renegocjacje; końcowo wspólne
  `sid=53023`, `up=1`, około 14,7 kB/s i `ret=bad=0`.
- **Potwierdzono sprzętowo samoczynny powrót pełnej szybkości** w tych
  trzech scenariuszach, bez ręcznej redukcji `w` i ponownego `s`.
  Wcześniejsze trwałe plateau około 9 kB/s z ~378 `ret/bad` na sekundę
  nie wystąpiło. Chwilowe `cold/uncertain` w trakcie zmiany SID nie są
  dowodem błędu, lecz `uncertain` nadal sygnalizuje możliwość
  niejednoznacznego dostarczenia na granicy resetu.
- Nie wykonano w tej serii osobnego testu długiego zaniku radia,
  ponownego otwarcia B przy otwartym A ani binarnego porównania
  strumieni. Brak lokalnego AVR-GCC i logu F7; test sprzętowy nie
  zastępuje formalnego raportu kompilacji.

## 2026-11-04 — dwie RF-Nano w metalizowanych torebkach ESD

- Właściciel umieścił **obie** aktywne płytki w silnie metalizowanych
  torebkach po komponentach wrażliwych na ESD. Zrzut RealTerm:
  `obraz(20261104-205556).png`. Nie podano pomiaru tłumienia w dB,
  szczelności zamknięć ani odległości anten.
- Łącze nadal osiąga ~14,4–14,7 kB/s w obu kierunkach; wspólne
  `sid=53023`, `up=1`, aplikacyjne `ret=0`, `bad=0`.
- A zgłasza sprzętowe ponowienia `arc≈9–43` w kolejnych próbkach;
  B pokazuje krótkie `link=2` (`SUSPECT`) i powrót `link=1`
  (`HEALTHY`), `warm=1`, `cold=2`, `uncertain=1`, bez zmiany SID.
  `uncertain=1` mogło pochodzić jeszcze z wcześniejszych resetów.
- Wniosek ograniczony: poprawna ramka wznowiła ten sam stan po
  krótkiej przerwie (warm recovery); metalizowane torebki nie
  spowodowały w tej próbie trwałego zaniku RF ani zimnej negocjacji.
  Nie jest to jeszcze test długiej utraty zasięgu lub integralności
  strumienia bajtów.

## 2026-11-04 — ręczna próba dłuższego odstępu między wymianami PP

- Właściciel zmienił **lokalnie**, wyłącznie dla diagnostyki,
  warunek w `NrfAckPayloadPhy::transfer` z `260UL` na `1300UL`
  taktów Timer1 (z 130 µs na 650 µs od **końca poprzedniej wymiany**
  do kolejnego impulsu CE). Zrzuty: `obraz(20261104-223914).png`,
  `obraz(20261104-223959).png`, `obraz(20261104-224128).png`.
- Po wgraniu tej próby oba urządzenia ustabilizowały się na
  `sid=12126`, `up=1`, około **12 408 B/s**, `ret=bad=0`,
  `A arc=0`, `win=1`. Wcześniejszy wariant 2 Mb/s z krótszym
  odstępem osiągał około 14,7 kB/s.
- Różnica ~0,31 ms na skuteczny segment 24 B mimo podwyższenia
  progu o 0,52 ms sugeruje, że przygotowanie kolejnego pakietu już
  częściowo wypełnia odstęp. **Nie dowodzi**, że pierwotne 130 µs
  blokuje wykonanie przy pełnej prędkości. Warunek `lastCompleted_`
  sprawdza upływ czasu, a nie wykonuje zawsze pełnego opóźnienia.
- Eksperyment pozostaje lokalną obserwacją użytkownika; w repozytorium
  nie zmieniano wartości czasowych ani kodu PHY. Następny krok przed
  optymalizacją to pomiar rzeczywistego czasu oczekiwania na warunek.

## 2026-11-05 — pierwsza potwierdzona i zaakceptowana wersja PP

- **Właściciel zaakceptował bieżący protokół PP jako technicznie
  poprawną bazę produkcyjną**, pomimo otwartej możliwości poprawy
  przepustowości. Polecił scalić całe `experiment/pp-window-drain`
  do `main` przed kolejnymi eksperymentami. Wcześniejsze gałęzie mają
  pozostać nienaruszone.
- **Identyfikator wersji firmware przetestowanej na dwóch RF-Nano:**
  `ff087b5f3640e9f606b4db8d25f4d02dc7a8ee27` — ostatnia zmiana
  kodu przed późniejszymi commitami dokumentacji na tej samej gałęzi.
  Ustalenie to dotyczy kodu repozytorium; niezacommitowane lokalne
  modyfikacje użytkownika (1/2 Mb/s, dłuższy odstęp 1300 taktów) były
  jednorazowymi próbami, nie częścią tej bazy.
- Dowody odbioru: start B→A oraz osobne resety B i A, każdorazowo
  wspólny SID i stabilne ~14,7 kB/s przy `ret=bad=0` bez ręcznego `w/s`;
  dodatkowo próba w metalizowanych torebkach pokazuje ciepłe wznowienie
  `SUSPECT→HEALTHY` bez utraty SID. `sh tests/run_host_tests.sh`:
  **23/23 PASS**, powtórzone przed scaleniem 2026-11-05.
- Granice potwierdzenia: brak formalnego wyniku kompilacji AVR-GCC,
  pomiaru rzeczywistych bajtów UART przy nadmiarowym napływie,
  gwarancji exactly-once przez reset i testu wielosekundowego
  całkowitego zaniku radia. Akceptacja nie usuwa tych ograniczeń.
- Właściciel wskazał nową, stałą gałąź **`testy`** wychodzącą z
  zaakceptowanego `main` jako jedyne miejsce dalszych eksperymentów.
  Prace nad maszyną stanów PHY wolno zacząć dopiero po zakończeniu
  operacji repozytorium i następnym poleceniu właściciela.

## 2026-11-05 — etap 1 nieblokującego PP, wyłącznie `testy`

- Po osobnym poleceniu właściciela rozpoczęto eksperyment na `testy`,
  bez zmian w zaakceptowanym `main` i bez tworzenia/usuwania branchy.
- W `NrfAckPayloadPhy` zastąpiono synchroniczne `transfer()` jawną
  maszyną `IDLE → GUARD → CE_PULSE → ON_AIR → IDLE`. `startTransfer()`
  wypełnia FIFO, a `pollTransfer()` wykonuje tylko pojedynczy krok:
  czas do CE i impuls CE są odmierzane Timer1, a wynik `TX_DS/MAX_RT`
  odczytywany jest poprzez SPI bez aktywnego czekania wewnątrz PHY.
  Prawidłowy ACK Payload jest pobierany przed udostępnieniem nowego TX.
- Pętla A ma pierwszeństwo dla obsługi PHY; w trakcie `ON_AIR` wykonuje
  jedną porcję producenta i diagnostyki. Pracę generatora ograniczono
  do maksymalnie 24 nowych bajtów na jeden przebieg, z zachowaniem
  ciągłości danych i dotychczasowego pełnego napełniania przy wywołaniu
  z domyślnym budżetem. B zachowuje priorytet odbioru i przygotowania
  kolejnego ACK Payload. Nie zastosowano przerwań SPI.
- **Celowo nie budujemy kolejnej ramki przed obsłużeniem poprzedniego
  ACK:** `UartReliableStream::make` zatwierdza zużycie bajtów i stan
  Go-back-N, więc spekulacyjne odrzucenie takiej ramki po zmianie SID,
  kredytu lub NACK mogłoby naruszyć dostarczalność. Istniejące 4×24 B
  cache retransmisji pozostaje jedynym oknem; nie dodano 4×32 B kopii.
- Nowy `pp_async_phy_test` bada zmianę stanu, 15µs CE, 130µs guard,
  brak blokady SPI, odebranie ACK, przepełnienie licznika Timer1,
  MAX_RT, timeout i błąd statusu. `pp_echo_ramp_test` bada porcjowanie
  producenta; zachowano resztę testów sesji, warm/cold recovery i replay.
  `sh tests/run_host_tests.sh`: **24/24 PASS**. Lokalnie brak AVR-GCC;
  nie wykonano jeszcze F7/F12 ani testów na rzeczywistych RF-Nano.
  Workflow CI obsługuje teraz również push do `testy` (host i AVR-GCC).
- **Brak potwierdzonego zysku w kB/s i brak sprzętowego odbioru tej
  wersji.** Konieczny pomiar na obu urządzeniach i porównanie z
  zatwierdzonym `main`, zwłaszcza po samotnym resecie A, B oraz przy
  celowym pogorszeniu zasięgu. Bez tej próby nie wolno scalać do `main`.
