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
