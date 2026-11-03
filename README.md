# GPT_NRF24

Projekt w C++ dla dwóch płytek **RF-Nano (ATmega328P + nRF24L01+)**, rozwijany wyłącznie w **Microchip Studio / Atmel Studio**.

## Cel

Dwa urządzenia podłączone przez USB do osobnych terminali mają samodzielnie odnaleźć się i przesyłać informacje radiowo w obu kierunkach:

- tekst wpisany w terminalu A pojawia się w terminalu B;
- tekst wpisany w terminalu B pojawia się w terminalu A;
- obie płytki korzystają z tego samego firmware;
- użytkownik nie przełącza ręcznie ról nadajnika i odbiornika.

**Centralne zagadnienie projektu:** nRF24L01+ pracuje fizycznie półdupleksowo. Projekt ma zapewnić logiczny full duplex przez odpowiednie sterowanie transmisją i odbiorem. Szczegóły i kryteria odbioru: [goal.md](goal.md).

## Telemetria łącza

Po nawiązaniu połączenia oba urządzenia mają naprzemiennie nadawać stałe ramki 32 B, także bez danych użytkownika. W każdej ramce mieści się telemetria łącza i — jeśli dostępne — dane z UART. Priorytetem M1 jest wiarygodny pomiar jakości osobno dla obu kierunków. Opis: [plan.md](plan.md).

## UART — test peryferium

**Właściciel potwierdził echo bajtu na obu fizycznych RF-Nano (2026-11-03).** Nie przetestowano jeszcze długich serii, pełnego zakresu danych binarnych ani przepełnień. `main.cpp` aktualnie uruchamia nieblokujący **lokalny echo-test USART0** przez USB/CH340. Otwórz każdy port z ustawieniami **115200, 8N1, bez kontroli przepływu**; terminal nie powinien mieć włączonego local echo (inaczej widoczne będą podwójne znaki). Wysłane bajty są odsyłane niezmienione, niezależnie od ich wartości. Testuj każdą płytkę osobno, F7 kompilacja, F12 ręczne programowanie. Włączenie terminala może powodować reset ATmegi przez DTR.

Sterownik: `GPT_NRF2401/uart.h/.cpp`; pojedyncza instancja plikowa `static UART uart;` rezerwuje ok. 270 B statycznej pamięci SRAM poza stosem; statyczne kolejki `byte_ring.h`, RX/TX po 127 użytecznych bajtów. Przy długim napływie danych nieobsłużonych przez main RX overflow zwiększa licznik; przy pełnej TX queue `write` zwraca false. Liczniki błędów FE/DOR/UPE są dostępne przez `UART::counters()`; nie są wypisywane do terminala, aby nie zanieczyszczać danych. `tests/uart_ring_test.cpp` jest niezależnym od AVR testem struktury pierścienia.

Obie konfiguracje kompilacji (Debug i Release) wymagają i deklarują `-std=c++11`. F_CPU musi odpowiadać 16 MHz: U2X0=1, UBRR0=16 daje ok. 117647 baud (+2,12% względem 115200). PD0=RXD oraz PD1=TXD są zarezerwowane dla USART; bieżący test LED na PD0 został zastąpiony testem UART. Programowanie F12 i konfiguracja Post-Build pozostają bez zmian.

## Stan

**INITIAL — M1 nieukończony.** Komunikacja radiowa nie została jeszcze potwierdzona testem odbiorczym dwóch płytek dla docelowego firmware.

## Środowisko

- Windows i Microchip Studio / Atmel Studio;
- AVR-GCC C++, mikrokontroler ATmega328P;
- dwie płytki RF-Nano z wbudowanym nRF24L01+;
- programowanie bootloaderem USB i AVRDude.

### Budowanie i programowanie

Otwórz `GPT_NRF2401.atsln` w Microchip Studio. Projekt wykorzystuje standardową kompilację AVR. Programowanie obu portów COM tym samym plikiem HEX jest wywoływane ręcznie skrótem F12, niezależnie od kompilacji; Post-Build nie uruchamia programatora.

Porty COM i położenie AVRDude są zależne od stanowiska. Nie wpisujemy ich na stałe do protokołu ani aplikacji.

## Zasady rozwoju

Kod ma używać jawnie tworzonych obiektów i statycznych buforów; nie wolno stosować `new/delete` ani dynamicznej alokacji. Zmiany powinny zachowywać podział na obsługę sprzętu, komunikację i logikę użytkową.

## Dokumentacja projektu

- [AGENTS.md](AGENTS.md) — stałe instrukcje dla agenta.
- [goal.md](goal.md) — cel i warunki odbioru.
- [plan.md](plan.md) — etapy realizacji.
- [protocol-v1.md](protocol-v1.md) — projekt formatu binarnego ramki v1 i testowe wektory 32 B.
- [todo.md](todo.md) — bieżące zadania.
- [history.md](history.md) — ukończone prace i potwierdzone wyniki.
- [migracja.md](migracja.md) — samodzielna instrukcja kontynuacji.

Szczegółowe instrukcje testowania komunikacji zostaną uzupełnione wraz z implementacją i weryfikacją M1.

## GPIO

Dodano `GPT_NRF2401/gpio.h` i `gpio.cpp`: statyczne obiekty do sterowania pinami bez Arduino i alokacji dynamicznej. Przykład: `GPIO led(&PORTB, PB5); led.set(); led.clear();`. Metody `i2c_set` i `i2c_clr` zapewniają wejście z pull-up lub aktywne LOW (nie kompletny kontroler I²C). Wymagana weryfikacja kompilacji AVR w Studio.

## SPI + nRF24L01+ — etap testowania sprzętu

Aktualny `main.cpp` po starcie konfiguruje sprzętowe SPI w trybie 0, MSB first, **1 MHz (16 MHz / 16)** i wykonuje test SPI z nRF24L01+. Test rozpoczyna się po 110 ms od ustawienia CE=0 i CSN=1, ponieważ POR radia może trwać do 100 ms. **Nie rozpoczyna transmisji RF** i pozostawia `CONFIG=0x0C` (POWER_DOWN, CRC16).

Dla starszej wersji RF-Nano z ATmega328P zgodnie z dokumentacją producenta: **D9/PB1 = CSN, D10/PB2 = CE, D11/PB3 = MOSI, D12/PB4 = MISO, D13/PB5 = SCK**. W innych wersjach (np. V3) CE/CSN są inaczej podłączone: zmienić powiązania `radioCe` i `radioCsn` w `main.cpp`, nie sterownik nRF. **PB2 musi zawsze pozostać wyjściem sprzętowego SPI master (SS)**; nie wolno przypadkowo ustawić CE=HIGH podczas inicjalizacji magistrali.

Przy otwarciu każdego portu RealTerm (115200 8N1) program wysyła jeden wiersz. Wzorcowy wynik po resecie:
```text
NRF24 OK S=0E CFG=0C CH=4C RF=06 FIFO=11
```
Wartości STATUS i FIFO mogą się różnić w zależności od stanu FIFO. Wiersz `NRF24 FAIL C=xx REG=xx EXP=xx GOT=xx S=xx CH=xx` informuje o błędzie inicjalizacji: C=01 brak zakończenia transferu SPI, C=02 brak wiarygodnej odpowiedzi radia, C=03 niezgodność odczytu zwrotnego. Znak **`?`** wysłany z terminala ponawia test i wypisuje kolejny wiersz; pozostałe bajty nadal są odsyłane jako echo (w tym trybie `?` jest komendą, nie danymi).

Test sprawdza `STATUS`, zapis/odczyt dwóch różnych wartości `RF_CH` z odtworzeniem oryginalnej, konfigurację i odczyt zwrotny rejestrów nRF oraz 5-bajtowych adresów. Radio otrzymuje: kanał 76 (2476 MHz), 1 Mb/s, CRC16, Enhanced ShockBurst auto-ACK na pipe 0, szerokość 32 B, RF_SETUP=06, SETUP_RETR=3F, FEATURE/DYNPD=0. **Test SPI nie dowodzi jeszcze poprawnej transmisji radiowej ani połączenia między dwiema płytkami.**

Klasy `SpiMaster` (`spi.h/.cpp`) i `Nrf24<Spi, Pin>` (`nrf24.h`) są odseparowane. Sterownik radia nie posiada na stałe przypisanych pinów i nie zależy od AVR; test modelowy `tests/nrf24_mock_test.cpp` symuluje magistralę i rejestry. Przykładowa komenda kompilacji na hoście: `g++ -std=c++11 -Wall -Wextra -Werror -pedantic tests/nrf24_mock_test.cpp -o nrf24_mock_test`. Test wymaga niezależnego uruchomienia na hoście; firmware wymaga kompilacji F7 oraz ręcznego wgrania F12 i sprawdzenia wyniku na obu portach COM.

Źródła pinów i parametrów: [instrukcja RF-Nano V1.1](https://michiel.vanderwulp.be/domotica/Modules/RFNanoKeywish/RF-NANO-Operating-Instruction_V.1.1.pdf) oraz [nRF24L01+ Product Specification](https://www.sparkfun.com/datasheets/Wireless/Nordic/nRF24L01P_Product_Specification_1_0.pdf).

### Ostrzeżenia kompilatora o `packed`

Jeśli Microchip Studio pokazuje `ignoring packed attribute` dla pól referencyjnych sterownika radia, sprawdź ustawienie `Pack Structure Members` w opcjach **C i C++** dla **Debug oraz Release**. W tym projekcie ustawiono je na `False` — globalny `-fpack-struct` jest niepotrzebny i niebezpieczny dla struktur aplikacyjnych. Format radiowy serializuje się jawnie do 32-bajtowej tablicy, bez bezpośredniego mapowania na spakowane obiekty. O sukcesie builda rozstrzyga `Output > Build` i wynik `Rebuild All`, a nie wyłącznie `Error List`.

## Test radiowy RAW32 — bez protokołu v1

Aktualny wsad jest **etapem sprawdzenia sprzętu radiowego**, nie mostem UART↔RF ani implementacją `protocol-v1.md`. Obydwie płytki otrzymują **identyczny HEX**. Ich rolę w samym teście określa wyłącznie pomiar ADC0 po starcie:

- **A / CLASS=00** — połączyć ADC0/A0 z **GND**. Płytka okresowo wysyła 32-bajtowy wzorzec, oczekuje sprzętowego ACK i potem 32-bajtowej odpowiedzi.
- **B / CLASS=01** — połączyć ADC0/A0 z **3,3 V**. Płytka odbiera wzorzec w PRX, sprawdza każdy bajt, odczekuje na zakończenie sprzętowych ACK, przełącza się do PTX i odsyła odpowiedź; wraca do PRX.
- W obu przypadkach masy i zasilanie muszą należeć do **własnej** płytki. Nie łączyć ze sobą pinów radiowych ani UART obu płytek. Dla AVCC 5 V, 3,3 V na ADC0 daje ok. 675/1023, czyli CLASS=01. Jeśli A0 pozostanie niepodłączony, klasa jest nieokreślona.
- ADC0 używa odniesienia AVCC; wynik 10-bitowy jest uśredniany z ośmiu konwersji, zatrzaskiwany po uruchomieniu. Próg: `ADC < 512` → 0, `ADC >= 512` → 1. Nie ma tu enumeracji klas ani automatycznego discovery.

**Po F7 (Microchip Studio) i F12 (ręczne programowanie) otworzyć dwa porty RealTerm 115200 8N1**. Przykładowe odpowiedzi:

```text
COM A: NRF OK CLASS=00 ADC=0000 MODE=RAW32
COM A: A SEQ=0000 TX=ACK ARC=00 REPLY=OK RD=1
COM A: A SEQ=0001 TX=ACK ARC=00 REPLY=OK RD=1

COM B: NRF OK CLASS=01 ADC=02A3 MODE=RAW32
COM B: B SEQ=0000 RX=OK TX=ACK ARC=00 RD=1
COM B: B SEQ=0001 RX=OK TX=ACK ARC=00 RD=1
```

`TX=ACK` oznacza `TX_DS` (układ radiowy otrzymał sprzętowe ACK); `TX=MAX_RT` oznacza wyczerpanie retransmisji radia, `TX=TIMEOUT` lub `TX=SPI_ERR` wskazuje na niedomkniętą operację/błąd. `ARC` jest odczytem sprzętowego `ARC_CNT` dla ostatniej emisji, `RD=1` oznacza zaobserwowaną flagę `RX_DR` przy odbiorze. `REPLY=OK` po stronie A dodatkowo dowodzi, że w programie B przetworzono poprawny wzorzec i odesłano odpowiedź.

Znak `?` odpyta lokalne liczniki: `RX`, `BAD`, `TX_ACK`, `TX_FAIL`, `REPLY_TO`, `RX_DR`. Pozostałe znaki wysyłane z terminala są na tym etapie ignorowane (brak mostu UART-RF). Numery i wzorzec danych są **wyłącznie diagnostyczne**: bajty 0..2 to znacznik i numer testu, pozostałe 29 bajtów mają obliczalną zawartość. Odpowiedź odwraca wyłącznie znacznik. To nie jest nagłówek protokołu v1.

Scenariusze sprzętowe: uruchomić obie płytki, odłączyć zasilanie B (A powinno raportować MAX_RT/MISS), podłączyć ponownie (powinno samoczynnie wrócić ACK/REPLY=OK), powtórzyć reset A. Obecne stałe odstępy testowe służą tylko weryfikacji przełączania TX/RX; nie są docelowym harmonogramem slotów M1. Test nie zapewnia pełnej bezstratności, routingu, negocjacji ról ani logicznego full duplex dla dowolnych danych UART.

**Warstwy kodu:** `nrf24.h` (polecenia SPI/rejestry), `nrf24_phy.h` (sterowanie CE, PRX/PTX, ograniczone operacje TX/RX, przerwania/status), `adc0.h/.cpp` (sprzęt ADC), `board_clock.h` (wspólne F_CPU dla sterowników i opóźnień), `avr_delay.h` (adapter czasów), `rf_raw_pattern.h` (wzorzec testowy), `main.cpp` (wyłącznie scenariusz RAW32). Podstawowa transmisja wymaga prawdziwego testu na obu fizycznych radiach.

Na hoście dodano `tests/nrf24_phy_test.cpp` i `tests/rf_raw_pattern_test.cpp`; testy pomocnicze uruchomiono przez `g++ -std=c++11 -Wall -Wextra -Werror -pedantic` na odpowiednikach bieżącej logiki PHY i wzorca. Kompilacja AVR i fizyczny transfer RF nie zostały jeszcze zweryfikowane.


### Potwierdzenie RAW32 na fizycznych płytkach (2026-11-03)

Właściciel potwierdził działającą wymianę A→B→A na RF-Nano, COM10/COM11. Oba terminale pokazują zgodne kolejne `SEQ=000B..0019`, odpowiednio `A TX=ACK ARC=00 REPLY=OK RD=1` oraz `B RX=OK TX=ACK ARC=00 RD=1`. **Fizyczny test dwukierunkowych surowych 32-bajtowych pakietów jest zaliczony.** Opis wcześniejszego etapu wymagającego testu należy traktować jako instrukcję reprodukcji, nie aktualny brak potwierdzenia. Do przetestowania pozostają MAX_RT przy zaniku drugiego urządzenia, recovery, dalsze obciążenia oraz właściwy protokół i UART↔RF.

## Test ramki v1 z payloadem 0/1/23/24 B — następny etap

Od aktualnego `main.cpp` radio **nie wysyła już wzorca RAW32** z poprzedniej próby. Każdy pakiet nadal ma fizycznie **32 bajty**, ale teraz używa **8-bajtowego nagłówka v1** (`CTRL,SRC,DST,SID,TX_SEQ,RX_LAST,SRC_EP,DST_EP`) i **24-bajtowego obszaru BODY**. Parser i koder są odseparowane w `GPT_NRF2401/frame_v1.h` (bez AVR, bez alokacji i bez spakowanych struktur). Kompletna specyfikacja: `protocol-v1.md`.

Do **celowego testu długości** wysyłamy kolejno `BODY LEN=0,1,23,24`; niewykorzystane bajty są zerowane i weryfikowane. `LEN=0` to heartbeat; przy `LEN>0` używany jest **diagnostyczny endpoint 2**, generujący znany deterministyczny wzorzec binarny. Każdy węzeł porównuje treść ze spodziewaną, nie tylko ufa sprzętowemu CRC. Węzeł A rozpoznaje odpowiedź B także przez `RX_LAST`, który musi wskazywać właśnie przesłany `TX_SEQ`; obie strony numerują własne ramki niezależnie w zakresie 0–255.

**Ważne ograniczenie:** `SID=0x2A` jest w tym teście stałą, a nie wynikiem negocjacji `HELLO/READY`; endpoint 2 jest wyłącznie serwisem testowym, a nie gotowym transportem UART RELIABLE. Nie dodano jeszcze ACK aplikacyjnego, buforowania/ponawiania danych użytkownika ani UART↔radio. Ta wersja ma sprawdzić *format v1 i payload na prawdziwym radiu*, bez sugerowania gwarancji dostarczenia.

**Uruchomienie:** na A połączyć A0 z GND, na B A0 z dodatnim zasilaniem **tej samej płytki**; wgrać jeden HEX na obie RF-Nano (F7 kompilacja, F12 ręczne programowanie), otworzyć dwa porty RealTerm 115200 8N1.

Przykładowe oczekiwane linie pierwszych cykli:

```text
A: NRF OK CLASS=00 ADC=0000 MODE=V1PAYLOAD
A: A SEQ=00 LEN=00 TX=ACK ARC=00 REPLY=OK IN=17 RD=1
A: A SEQ=01 LEN=01 TX=ACK ARC=00 REPLY=OK IN=18 RD=1
A: A SEQ=02 LEN=17 TX=ACK ARC=00 REPLY=OK IN=00 RD=1
A: A SEQ=03 LEN=18 TX=ACK ARC=00 REPLY=OK IN=01 RD=1

B: NRF OK CLASS=01 ADC=03FF MODE=V1PAYLOAD
B: B SEQ=00 LEN=00 RX=OK TX=ACK ARC=00 OUT=17 RD=1
B: B SEQ=01 LEN=01 RX=OK TX=ACK ARC=00 OUT=18 RD=1
B: B SEQ=02 LEN=17 RX=OK TX=ACK ARC=00 OUT=00 RD=1
B: B SEQ=03 LEN=18 RX=OK TX=ACK ARC=00 OUT=01 RD=1
```

Wartości `LEN` i `IN/OUT` zapisane są **szesnastkowo**: `17` oznacza 23 bajty, `18` oznacza 24 bajty. Sekwencje B są niezależne od A, więc po utracie pakietów `IN/OUT` mogą się przesunąć względem przykładu. Warunkiem zaliczenia jest poprawne parsowanie, odbiór długości 0/1/23/24 oraz `REPLY=OK` i `RX=OK` przy prawidłowych ramkach. `TX=MAX_RT`/brak odpowiedzi oznaczają brak potwierdzenia przez radio lub logikę testu. Przepełnienie UART może zgubić wiersz diagnostyczny, ale nie zatrzymuje testu.

Znak `?` pokazuje liczniki z krótkimi etykietami `CNT R=` (poprawne RX), `B=` (błędne), `ACK=` (TX_DS), `F=` (błędy TX), `TO=` (brak odpowiedzi A), `RD=` (RX_DR), `REJ=` (odrzucone przez parser). Pozostałe znaki UART nie są w tej wersji transmitowane radiowo.

**Hostowe testy kodera:** `tests/frame_v1_test.cpp` (pięć dokładnych wektorów 32 B ze specyfikacji, nieznane podtypy, błędne długości, adresy, padding, sesja, ACK) i `tests/v1_probe_pattern_test.cpp` (pełne zakresy 8-bitowego numeru ramki i obu klas, mutacje każdego znaczącego bajtu). Testy logiczne wykonano na komputerze przy użyciu `g++ -std=c++11 -Wall -Wextra -Werror -pedantic`, zakończone PASS. **Nowy wariant firmware nie został jeszcze skompilowany w Microchip Studio ani sprawdzony fizycznie.**

Odłączenie zasilania całej płytki RF-Nano nie jest obecnie dobrym testem samodzielnej odporności radia: odcina również USB/CH340, RealTerm traci port COM, a jego ponowne otwarcie może resetować ATmegę. Właściciel potwierdził wcześniej reset A i przytrzymanie RESET B oraz powrót RAW32; pełny test odcięcia zasilania radia **odłożono do etapu z niezależnym zasilaniem i pomiarem**, nie jest warunkiem tej próby.
