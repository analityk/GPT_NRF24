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
