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
