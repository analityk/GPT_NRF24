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
