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
