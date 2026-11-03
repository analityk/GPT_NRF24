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
