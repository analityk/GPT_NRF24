# Migracja / punkt kontynuacji GPT_NRF24

**Aktualny stan: 2026-11-03 — jeden aktywny rdzeń 8+24 B z hardware ACK Payload.**

## Źródło prawdy

- Repozytorium: https://github.com/analityk/GPT_NRF24, gałąź `main`.
- Dotychczasowy działający, kompletny stan **sprzed porządkowania**: commit `d91630509ba32fb62ab0437ff2d2b5770feae31c`, gałąź `archive/pre-paired-core-cleanup-2026-11-03`. Znajdują się tam osobny dawny UART bridge, stary v1, jego handshake, poprzedni 32+32 benchmark i testy. Nie przenosić ich z powrotem bez konkretnego wymagania.
- W bieżącym źródle pozostaje jeden program `GPT_NRF2401/main.cpp`. **Nie ma już `application_select.h`, `paired_core_main.cpp`, `ack_bench_main.cpp`, `boot_epoch.cpp`, `frame_v1.h` ani trzech wariantów firmware.**
- Zmiany wyników i uzasadnienia z wcześniejszych faz zachowuje `history.md`; bieżący kontrakt binarny w `paired-core-v2.md`, plan w `plan.md`, zadania w `todo.md`.

## Stałe wymagania

- RF-Nano ATmega328P 16 MHz, nRF24L01+, CH340, SPI CE PB2/D10, CSN PB1/D9, MOSI PB3, MISO PB4, SCK PB5.
- A0 GND → A/PTX; A0 dodatnie zasilanie → B/PRX. Jeden identyczny HEX na obie.
- Microchip Studio/Atmel Studio, AVR C++11, brak dynamicznej alokacji, globalny `PackStructureMembers=False`.
- Programowanie **tylko ręcznie F12** po F7; **nigdy nie przywracać automatycznego Post-Build flashowania**.
- Radio 1 Mb/s, CRC16, Enhanced ShockBurst, hardware ACK Payload, SPI 8 MHz, Timer1 0,5 µs.
- DPL: 8–32 B. Nagłówek: TX_SEQ, RX_LAST, dwa nibbles LOSS, dwa nibbles ARC_LAST/RX_DROP, RX_CREDIT, TYPE_FLAGS, SID16; BODY 0–24 B nieinterpretowane.

## Co zbadano fizycznie

W RealTerm, na dwóch RF-Nano, nowy rdzeń 8+24 osiągnął **~625 transakcji/s**: A `TX/s=625 ACK/s=625`, B `RX/s=625 Q/s=625`, obserwowane ARC/MAX/BAD/IO/GAB/GBA/DROP/FULL=0. To 15 000 B/s wzorca testowego na kierunek, okres średni 1,6 ms, a **nie latencja ani realne przesłanie UART**. Poprzedni benchmark 32+32 wykazał 540/s. Nie zmieniono trybu radiowego po każdym pakiecie: B pozostaje PRX, A PTX.

## Granice i następne zadania

- Testowe `SESSION_ID=0x4E52` jest stałe, więc nie ma jeszcze prawdziwej negocjacji pary ani zabezpieczenia przed starym ACK po resecie.
- `RX_CREDIT=1` jest symulacją, nie stanem prawdziwego odbiornika.
- Sprzętowy ACK potwierdza odbiór ramki przez radio, nie przyjęcie przez UART lub plik.
- Wyższe protokoły usług (TCP-like, pliki, szyfrowanie) są świadomie poza bieżącą warstwą; zachować opaque BODY.
- Następny etap: handshake sesji, reset recovery i włączenie rzeczywistej informacji o dostępności bufora, bez pogorszenia sprawdzonego rdzenia.

## Testy i zachowanie danych

`sh tests/run_host_tests.sh` kompiluje automatycznie wszystkie aktywne `tests/*_test.cpp`. `sh tests/build_avr.sh` kompiluje jeden program dla ATmega328P bez programowania sprzętu. GitHub Actions wykonuje obie czynności. W Microchip Studio nadal F7/F12; nie mieszać hostowego PASS, krzyżowego build PASS i fizycznego testu.

Porządkowanie zmienia bieżące pliki źródłowe, **nie usuwa historii Git**; stary kod można odczytać w gałęzi `archive/pre-paired-core-cleanup-2026-11-03`. Żadna zmiana nie wymaga nazwy portu COM ani stałej ścieżki Windows.
