# Punkt kontynuacji projektu GPT_NRF24

**Repozytorium:** https://github.com/analityk/GPT_NRF24  
**Gałąź główna:** `main` — każdorazowo potwierdź aktualną gałąź i HEAD.  
**Stan:** INITIAL, kamień milowy M1 nieukończony.

## Instrukcja przejęcia

1. Sprawdź bieżący stan repozytorium: gałąź, HEAD, ostatnie commity oraz `git status`.
2. Przeczytaj całe `AGENTS.md`. To obowiązujące reguły rozwoju.
3. Przeczytaj `goal.md`, `plan.md` i `todo.md` oraz najnowsze wpisy `history.md`.
4. Zajrzyj do `README.md` i do aktualnej implementacji w katalogu projektu Microchip Studio.
5. Zestaw stan zapisany w dokumentacji z kodem i rzeczywistymi dowodami testów.
6. Podejmij pierwsze niezakończone zadanie M1 lub nowsze, jednoznaczne polecenie właściciela repozytorium.

Nie zakładaj, że starszy opis pozostaje zgodny z HEAD. Nie deklaruj, że program działa na sprzęcie, jeżeli brak potwierdzonej próby na obu płytkach.

## Istota projektu

Dwie identyczne płytki RF-Nano, ATmega328P + nRF24L01+, połączone radiowo. Dwa niezależne terminale USB: dane z A trafiają do B, a dane z B trafiają do A. Ten sam wsad na obu urządzeniach, bez ręcznego wyznaczania ról.

**Najważniejsze wyzwanie:** nRF24L01+ jest fizycznie półdupleksowy, a projekt ma zapewnić **logiczny full duplex**. Trzeba świadomie zaprojektować współdzielenie radia, inicjowanie transmisji przez obie strony oraz zachowanie w razie kolizji.

## Nieprzekraczalne decyzje

- Microchip Studio / Atmel Studio i AVR-GCC C++.
- Brak `new/delete`, brak alokacji dynamicznej; jawne instancje i bufory o stałych rozmiarach.
- Jeden firmware dla obu urządzeń.
- Rozwiązania wielokrotnego użytku, rozdzielone odpowiedzialności architektoniczne.
- Zachowanie istniejącego procesu programowania obu urządzeń przez USB.

## Co jest potwierdzone, a co nie

- Istnieje projekt Microchip Studio oraz możliwość programowania obu płytek przez bootloader USB przy użyciu AVRDude.
- Obecny kamień milowy nie ma jeszcze potwierdzonego odbioru transmisji radiowej w obu kierunkach.
- O bieżącym stanie implementacji rozstrzyga kod na aktualnym HEAD, nie ten skrót.

## Najbliższa praca

Rozpocząć od architektury dwukierunkowego łącza dla dwóch równorzędnych urządzeń. Następnie realizować M1a–M1c według `plan.md` i odznaczać rzeczywiście wykonane pozycje z `todo.md`.

Po zmianach aktualizować dokumentację i wskazywać wyniki sprawdzeń; niezależnie weryfikować działanie na fizycznych płytkach.

## Checkpoint 2026-11-03

Dodano `gpio.h` i `gpio.cpp` do projektu; `plan.md` opisuje szczegółową kolejność UART → SPI/nRF24 → transfer → protokół → discovery → arbitraż full duplex → testy. Następny krok: kompilacja GPIO w Microchip Studio, potem UART. Nie ma jeszcze potwierdzonego testu sprzętowego GPIO ani RF.

## Decyzja o ciągłym pomiarze łącza — 2026-11-03

Po zestawieniu łącza urządzenia mają wymieniać na przemian ramki o **stałym payloadzie 32 B**, także gdy UART milczy. Każda ramka niesie telemetryczny nagłówek, a reszta jest dostępna dla danych UART; rozmiar danych użytkownika jest więc mniejszy niż 32 B. Najważniejsza jest wiarygodna, niezależna dla obu kierunków informacja o stratach i ponowieniach. Timeout i odzyskanie synchronizacji slotów są obowiązkowe. Szczegóły w `plan.md`.

Właściciel potwierdził kompilację GPIO i miganie diod na obu fizycznych RF-Nano; aktualny `main.cpp` zawiera ten test (commit `9d251e1`). Programowanie zostało przeniesione na ręcznie wywoływane F12; Post-Build w projekcie jest pusty. UART, SPI i łącze radiowe nie zostały jeszcze wdrożone.

## Specyfikacja protokołu v1 — 2026-11-03

Nowy plik `protocol-v1.md` jest źródłem prawdy dla projektu binarnego 32 B: 8 B nagłówka, 24 B BODY, wersjonowanie, SID, TX_SEQ, RX_LAST, endpointy, discovery, transport RELIABLE i wektory testowe. M1 wymaga klas 0 i 1 zatrzaśniętych po starcie z ADC0 (adc10>>2, próg 128). Nie implementowano jeszcze specyfikacji ani nie testowano jej na sprzęcie. Przed implementacją zweryfikować nonce/SID, czas slotów i wektory na hoście.

## UART0 — etap uruchomienia, 2026-11-03

Kod UART0 w `GPT_NRF2401/uart.h/.cpp`, wielokrotnego użytku pierścień SPSC w `GPT_NRF2401/byte_ring.h`, host-test pierścienia `tests/uart_ring_test.cpp`. `main.cpp` aktualnie realizuje echo po CH340 (bez RF), 115200 8N1, U2X0=1, UBRR=16 przy zegarze 16 MHz, `sei()` w main. RX/TX: 128 slotów, pojemność użytkowa 127 bajtów każda. Bez dynamicznej alokacji. Błędy FE0, DOR0, UPE0 oraz overflow mają liczniki; TX nie blokuje. LED test na PD0 usunięty z bieżącego main, ponieważ PD0 jest RXD; zachowany w historii commit `9d251e1`. Następna czynność: F7 kompilacja w Microchip Studio, F12 programowanie obu płyt, echo na terminalach obu COM (115200 8N1), dane binarne i przeciążenie. Nie zgłoszono jeszcze wyników sprzętowych.

## Potwierdzenie UART i korekta rozmieszczenia pamięci — 2026-11-03

Właściciel potwierdził na **obu fizycznych płytkach** echo wysyłanych bajtów UART. Dodał `-std=c++11` do konfiguracji Release w commicie `4b300e2`; dodano tę samą flagę dla Debug. `main.cpp` tworzy teraz jawny obiekt plikowy `static UART uart;` (około 270 B statycznego SRAM). Poprzednia zmienna automatyczna zajmowała stos, nie heap. Testy długich serii, danych binarnych i przepełnień nie są potwierdzone; nowy układ pamięci oraz konfiguracja Debug wymagają weryfikacji w Microchip Studio. GPIO, F12 i Post-Build nietknięte.

## Uruchomienie SPI i nRF24L01+ — 2026-11-03

Właściciel potwierdził stabilny UART po wysłaniu całego pliku RealTerm bez delayów. Aktualny `main.cpp` zastąpiono testem diagnostycznym SPI/nRF (status startowy na obu UART, powtórzenie przez `?`; zwykłe bajty nadal są echem). Kod: `spi.h/.cpp`, szablon `nrf24.h`, test modelowy `tests/nrf24_mock_test.cpp`, połączenie w `.cppproj`. Na starszym RF-Nano ATmega328P obowiązuje CSN D9/PB1, CE D10/PB2, MOSI D11, MISO D12, SCK D13; w innych rewizjach może być inaczej. Po 110 ms od inicjalizacji pinów: test RF_CH, konfiguracja i readback, POWER_DOWN. Spodziewany zapis: `NRF24 OK S=0E CFG=0C CH=4C RF=06 FIFO=11`. Przyszła obsługa RX/TX i slotów nie została jeszcze zaimplementowana. **Nie potwierdzono kompilacji AVR ani testów rzeczywistego SPI.** F7 kompiluje, F12 programuje ręcznie (nie zmieniać).

## Porządkowanie ostrzeżeń kompilatora — 2026-11-03

Na podstawie zrzutów właściciela: Rebuild All SPI/nRF zakończony sukcesem (4558 B Flash i 482 B Data w pokazanej kompilacji), ale 3 ostrzeżenia o ignorowaniu atrybutu packed dla referencji C++ oraz mylący wpis `required from here` w Error List. W projekcie wyłączono globalne `PackStructureMembers` dla C i C++ w obu konfiguracjach Debug/Release; nie zmieniono instancji Nrf24. Po aktualizacji zalecana czysta przebudowa i kontrola Output > Build. Realne działanie SPI/nRF nadal niepotwierdzone.

## SPI/nRF na obu fizycznych płytkach — potwierdzone zrzutami, 2026-11-03

Dwa zrzuty RealTerm dostarczone przez właściciela: COM11 i COM10, oba wielokrotnie pokazują `NRF24 OK S=0E CFG=0C CH=4C RF=06 FIFO=11`. Test `probeAndConfigure()` wykonuje transakcje SPI, dwa wzorce RF_CH, readback konfiguracji oraz kontrolę adresów; komunikat OK jest końcowym wynikiem tej procedury. **SPI i komunikacja rejestrowa nRF24 są sprawdzone sprzętowo na obu płytkach.** Stan radia nadal POWER_DOWN; nie było jeszcze fizycznego transferu RF, ACK czy testu kolizji. Następne zadanie: sterowanie RX/TX i jednostronna transmisja 32 B, następnie kierunek odwrotny, bez utrwalania ról w firmware. Nie ponawiać prośby o potwierdzenie `NRF24 OK`, bo dowód jest już dostępny w rozmowie.

## RAW32 dwukierunkowy test fizycznego RF — 2026-11-03

Na aktualnym kodzie `main.cpp` działają dwie role wyznaczane po starcie z ADC0, identyczny firmware; A0=GND → CLASS 00 (A, inicjuje i oczekuje odpowiedzi), A0=3,3 V → CLASS 01 (B, PRX, następnie odpowiada). `nrf24_phy.h` steruje CE, trybami RX/TX i sprzętowym ACK, statusami MAX_RT/TX_DS, RX_DR, FIFO i timeoutami. `rf_raw_pattern.h` generuje/werifikuje surowe 32-bajtowe wzorce, nie protokół v1. `adc0.h/.cpp` próbuje AVCC ADC0 i ustala rolę; `board_clock.h` trwale ujednolica F_CPU=16 MHz przed util/delay. `avr_delay.h` adaptuje opóźnienia dla PHY. `main.cpp` jest diagnostyką, bez bridge UART i bez protokołu v1.

F7 zbudować, F12 zaprogramować oba RF-Nano, otworzyć RealTerm 115200 8N1. A oczekuje `A SEQ=.... TX=ACK ARC=.. REPLY=OK RD=1`, B `B SEQ=.... RX=OK TX=ACK ARC=.. RD=1`. `?` wypisuje liczniki, inne bajty z UART są w tej wersji testowej ignorowane. Test odłączenia i ponownego podłączenia partnera jest istotny. **Potwierdzone dotąd tylko SPI (COM10/COM11), nie transmisja RF.** Testy hosta PHY/wzorca przeszły, AVR i fizycznego RF nie weryfikowano. Docelowy protokół v1 pozostaje w `protocol-v1.md`; nie włączać go do diagnostyki RAW32.

## Odbiór sprzętowy RAW32 — potwierdzony 2026-11-03

Właściciel dostarczył zrzut **obu terminali jednocześnie**: COM10 (A) `A SEQ=000B..0019 TX=ACK ARC=00 REPLY=OK RD=1`; COM11 (B) `B SEQ=000B..0019 RX=OK TX=ACK ARC=00 RD=1`. **Rzeczywista komunikacja radiowa w obie strony potwierdzona**, 32-bajtowy wzorzec jest weryfikowany w obu urządzeniach; działa automatyczne ACK nRF i przełączanie RX/TX. To test surowych ramek, nie protokół v1 ani most UART-RF. Nie ponawiać prośby o test podstawowego RAW32. Najbliższe sensowne testy fizyczne: odłączyć jedno urządzenie, zaobserwować MAX_RT i powrót po ponownym zasileniu; następnie implementować warstwę protokołu bez modyfikowania sprawnego SPI/PHY bez powodu.

## Test resetów i odzyskania RAW32 — potwierdzony 2026-11-03

Właściciel przesłał trzy zrzuty testu: reset A → przytrzymanie RESET B → zwolnienie RESET B. Po resecie A numeracja startuje od `0000`, B nadal przyjmuje poprawne ramki. Przy RESET B A przechodzi od `TX=ACK REPLY=MISS` do `TX=MAX_RT ARC=0F REPLY=MISS`. Po zwolnieniu RESET B oba logi wykazują od `SEQ=0185` ponownie poprawne `TX=ACK`, `RX=OK`, `REPLY=OK`, `RD=1` i `ARC=00` **bez restartu A**. Test fizycznego powrotu po resetach mikrokontrolerów zaliczony; nadal nie potwierdzono pełnego odcięcia zasilania samego nRF24 ani obsługi utraty sesji docelowego protokołu. Nie ponawiać tego samego testu; kolejne prace: protokół v1 i walidacja rzeczywistych trybów awaryjnych.

## Payload v1 — pierwszy kod i plan weryfikacji, 2026-11-03

Po zaliczonym fizycznie RAW32 i resecie/odzyskiwaniu użytkownik zdecydował przejść do payloadu. Nowy plik `frame_v1.h` zapewnia czysty koder/parser 32 B, 8 B nagłówka, 24 B BODY. `v1_probe_pattern.h` generuje **długości 0/1/23/24** w każdej grupie 4 ramkowych prób. Aktualny `main.cpp` zamienia test RAW32 na radio ping-pong **v1 payload** na działającym nRF24 PHY, z prawdziwymi niezależnymi TX_SEQ i RX_LAST, walidacją nagłówka/paddingu/treści, raportami `A SEQ=.. LEN=.. TX=.. REPLY=.. IN=..` i `B SEQ=.. LEN=.. RX=.. OUT=..`. A0 GND = A/class0; A0 plus tej samej płytki = B/class1. SID=0x2A **to tylko stała testowa**, endpoint2 nie jest UART RELIABLE; brak negocjacji sesji i użytkowych danych UART. Hostowe testy nowych plików przeszły; sprzętowa kompilacja i próba NOWEJ wersji firmware pozostają do sprawdzenia.

Pełne odłączenie zasilania radia na obecnej RF-Nano odłącza też COM RealTerm, a ponowne otwarcie portu może resetować ATmegę. Właściciel uznał ten eksperyment za niewykonalny w izolacji przy tej konfiguracji; odroczony, a udokumentowane restarty A/B pozostają wystarczające do rozpoczęcia pracy nad payloadem. Szczegóły uruchomienia, oczekiwane logi oraz znaczenie 0x17/0x18: najnowsza sekcja `README.md`. Następnie zaimplementować prawdziwy transport z negocjowaną sesją, ACK aplikacyjnym, kolejkami i retransmisjami, nie mylić testu formatu z gwarantowaniem dostarczenia.

## Payload v1 — zaliczony na dwóch płytkach, 2026-11-03

Właściciel przesłał zrzut dwóch terminali: COM11/B i COM10/A, ramki `SEQ=45..53`, poprawna cykliczna długość `LEN=00,01,17,18` (0/1/23/24 B), odpowiedzi A `TX=ACK ARC=00 REPLY=OK`, B `RX=OK TX=ACK ARC=00`. Użytkownik sprawdził reset jednej lub obu płytek: testowa wymiana samoczynnie wraca. **Test sprzętowego payloadu v1 zakończony pomyślnie; nie prosić ponownie o tę samą próbę.** `RD=0` w niektórych liniach B przy `RX=OK` oznacza brak widocznej w danej chwili flagi `RX_DR`, nie brak poprawnej ramki w RX FIFO. `SID=0x2A` pozostaje stałą testową, nie prawdziwą negocjacją sesji. Następny etap: niezawodne przekazanie rzeczywistych danych UART, aplikacyjny ACK/deduplikacja, limity kolejek i mechanizm sesji; odrębnie testować złośliwe/błędne ramki i przeciążenie. Pełny power-cycle radia pozostaje odroczony z powodu USB/CH340.

## UART↔radio RELIABLE — checkpoint kodu, 2026-11-03

Na gałęzi main jest już pierwszy dwukierunkowy most: `main.cpp` wysyła strumień UART ↔ v1 DATA/DATA_ACK przez istniejący nRF PHY. A0 przy GND = klasa0/inicjator, A0 przy własnym dodatnim zasilaniu = klasa1/responder; identyczny firmware. Start protokołu HELLO→HELLO_REPLY→READY→READY_ACK w `session_v1.h`, 32-bit nonce z wear-level EEPROM `boot_epoch.cpp`, SID CRC8, 8-bajtowy header/24 B BODY. `uart_transport_v1.h` prowadzi niezależny stop-and-wait obu kierunków i aplikacyjny ACK, z DATA_ACK mieszczącym ACK i 1–18 bajtów danych; UART TX ma atomowy writeAll. Dane UART są binarne i bez wydruków diagnostycznych. Puste pakiety są nadal wysyłane cyklicznie w już zestawionej sesji.

Host: lokalny test handshake i transportu oraz 2×1000 B z symulowanymi stratami zakończył się sukcesem (g++ C++11); pełny skrypt `tests/run_host_tests.sh` i workflow należy jeszcze odczytać / potwierdzić. Test kompilacji Microchip Studio oraz fizyczny UART↔radio jeszcze **nie jest wykonany**. F7/F12 bez zmian; nie ustawiać automatycznego Post-Build. RealTerm: 115200 8N1, bez local echo, przesyłać z A do B i odwrotnie krótkie teksty i binarne bajty. **Brak wypisywanych logów po UART jest celowy**. RX UART tylko 127 B, brak hardware flow control; nie wolno uznać niekontrolowanego strumienia pełnego pliku za bezstratny. Resety mogą spowodować powtórzenie ostatniego segmentu, SID 8-bit jest kolizyjny. Dodatkowe testy sprzętowe otwarte, pełne odłączenie zasilania pozostaje odroczone z powodu USB/CH340. Patrz najnowsza sekcja README i protokół v1.
