# AGENTS.md — zasady pracy nad GPT_NRF24

## Źródło prawdy i przejęcie projektu

Repozytorium Git jest trwałym źródłem kodu, decyzji i stanu projektu. Przed rozpoczęciem prac:

1. Przeczytaj w całości `migracja.md` i ten plik.
2. Sprawdź bieżącą gałąź, HEAD, ostatnie commity i stan plików (`git status`).
3. Przeczytaj `goal.md`, `plan.md`, `todo.md` oraz ostatnie wpisy `history.md`.
4. Porównaj dokumentację z faktycznym kodem i wynikami testów. W razie rozbieżności ujawnij ją.
5. Wybierz najbliższe zadanie z `todo.md`, chyba że właściciel projektu określił inny priorytet.

Instrukcje muszą być samowystarczalne. Fakty wymagające zachowania należy zapisywać w repozytorium, wraz z uzasadnieniem i dowodami ich sprawdzenia.

## Stałe decyzje projektowe

- **Środowisko:** wyłącznie Microchip Studio / Atmel Studio, AVR-GCC i C++ na ATmega328P. Nie proponuj migracji do innego IDE.
- **Pamięć:** bez `new`, `delete`, `malloc`, `free` i kontenerów wymagających alokacji dynamicznej. Obiekty mają być jawnie instancjonowane, a bufory mieć stałą pojemność i czas życia. Uwzględniaj ograniczenia SRAM.
- **GPIO:** do sterowania pojedynczymi pinami używaj klasy `GPIO` z `GPT_NRF2401/gpio.h`; SPI/USART obsługują własne peryferia. Uwzględniaj atomowość przy współdzieleniu portu z ISR.
- **Dwie płytki, jeden firmware:** nie uzależniaj roli urządzenia od portu COM ani jego podłączenia do komputera.
- **Rozdział odpowiedzialności:** obsługa sprzętu, transport/protokół radiowy, sterowanie łączem i logika konsoli powinny mieć oddzielne, testowalne granice.
- **Wielokrotne użycie:** każdy problem naprawiaj we właściwej warstwie, poprzez mechanizm nadający się do ponownego użycia. Rozwiązanie nie może zależeć od nazwy pliku, katalogu, projektu czy przypadku testowego.
- **Integracja:** zachowuj istniejący projekt Microchip Studio i działający mechanizm programowania przez USB. Nie nadpisuj ścieżek narzędzi ani ustawień portów bez potrzeby i zgody.
- **Zależności:** żadnych nowych frameworków lub bibliotek bez rzeczywistej potrzeby, uzasadnienia kosztu pamięci i zgodności z kompilatorem AVR.

## Fizyka łącza — sedno projektu

nRF24L01+ jest radiem **półdupleksowym**. Jednoczesne użytkowanie dwóch kierunków komunikacji ma zapewniać **logiczny full duplex**, realizowany przez sterowanie nadawaniem i odbiorem, koordynację transmisji i obsługę kolizji. Nie należy przedstawiać tego jako fizycznego jednoczesnego TX/RX.

Należy rozdzielać potwierdzenie sprzętowego odbioru ramki od faktycznego przekazania jej danych do aplikacji. Nie deklaruj bezstratności, odporności na kolizje czy czasu dostarczenia, jeśli nie potwierdzają tego testy.

## Reguły implementacji i weryfikacji

1. Przed pisaniem kodu ustal warstwę, do której należy problem, wymagania wejścia/wyjścia i przypadki brzegowe.
2. Projektuj bufory, stan protokołu, obsługę błędów i ponowienia świadomie; nie ignoruj przepełnień ani utraconych połączeń.
3. Utrzymuj testowalną logikę protokołu niezależnie od konkretnego MCU, o ile jest to możliwe bez nadmiernej komplikacji.
4. Nie traktuj poprawnej kompilacji, programowania Flash, testu jednostkowego i testu dwóch fizycznych urządzeń jako równoważnych dowodów.
5. Nie nadpisuj niezwiązanych zmian użytkownika ani nie wykonuj destrukcyjnych operacji Git.
6. Po zakończeniu pracy aktualizuj `todo.md`, `history.md` i, gdy zmienia się punkt kontynuacji, `migracja.md`.

## Odpowiedzialności dokumentów

- `goal.md` — wymagany rezultat i kryteria odbioru.
- `plan.md` — strategia i kamienie milowe, bez nadmiernych szczegółów implementacyjnych.
- `todo.md` — bieżące, wykonalne zadania.
- `history.md` — wyłącznie rzeczywiście dokonane zmiany i potwierdzone wyniki.
- `migracja.md` — samodzielny punkt kontynuacji.
- `README.md` — opis i używanie projektu.
