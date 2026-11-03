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
