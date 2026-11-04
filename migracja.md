# Migracja / punkt kontynuacji GPT_NRF24

**Aktualny stan: 2026-11-04 — jeden aktywny rdzeń 8+24 B z hardware ACK Payload i nowym handshake SID16 (host+AVR PASS, fizyczny test sesji otwarty).**

## Źródło prawdy

- Repozytorium: https://github.com/analityk/GPT_NRF24, gałąź `main`.
- Dotychczasowy działający, kompletny stan **sprzed porządkowania**: commit `d91630509ba32fb62ab0437ff2d2b5770feae31c`, gałąź `archive/pre-paired-core-cleanup-2026-11-03`. Znajdują się tam osobny dawny UART bridge, stary v1, jego handshake, poprzedni 32+32 benchmark i testy. Nie przenosić ich z powrotem bez konkretnego wymagania.
- W bieżącym źródle pozostaje jeden program `GPT_NRF2401/main.cpp`. **Nie ma już `application_select.h`, `paired_core_main.cpp`, `ack_bench_main.cpp`, `frame_v1.h` ani trzech wariantów firmware.** Moduł `boot_epoch.cpp` **jest obecnie ponownie potrzebny**, ale w nowej architekturze: adapter EEPROM dla przenośnego `epoch_journal.h`.
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

- Stałe testowe `SESSION_ID=0x4E52` **zastąpiono handshake** `paired_session.h` z nonce każdej strony `boot_epoch32+attempt16`; SID16 pochodzi z CRC16 obu nonce. Warstwy sesji/PHY chronią przed typowymi starymi odpowiedziami i negocjują restart. **Zaliczone testy hostowe i cross-build AVR; brak jeszcze testu fizycznego nowej wersji.**
- `RX_CREDIT=1` jest symulacją, nie stanem prawdziwego odbiornika.
- Sprzętowy ACK potwierdza odbiór ramki przez radio, nie przyjęcie przez UART lub plik.
- Wyższe protokoły usług (TCP-like, pliki, szyfrowanie) są świadomie poza bieżącą warstwą; zachować opaque BODY.
- Następny etap: test fizyczny handshake/odtwarzania po resetach A, B i obu; wspólny niezerowy `SID` i `ST=4` w obu RealTerm. Potem włączyć rzeczywistą informację o dostępności bufora, nie pogarszając rdzenia.

## Testy i zachowanie danych

`sh tests/run_host_tests.sh` kompiluje automatycznie wszystkie aktywne `tests/*_test.cpp`. `sh tests/build_avr.sh` kompiluje jeden program dla ATmega328P bez programowania sprzętu. GitHub Actions wykonuje obie czynności. W Microchip Studio nadal F7/F12; nie mieszać hostowego PASS, krzyżowego build PASS i fizycznego testu.

Porządkowanie zmienia bieżące pliki źródłowe, **nie usuwa historii Git**; stary kod można odczytać w gałęzi `archive/pre-paired-core-cleanup-2026-11-03`. Żadna zmiana nie wymaga nazwy portu COM ani stałej ścieżki Windows.

## Nowy handshake i trwałość sesji — 2026-11-04

Zaimplementowano `GPT_NRF2401/paired_session.h` jako maszynę stanów MCU-independent dla A/PTX oraz B/PRX. Wiadomości CONTROL, `SID=0`, `RESYNC=1`: HELLO 15B DPL, BEACON 15B, WELCOME 21B, READY 23B, READY_ACK 23B. A przechodzi ACTIVE dopiero po READY_ACK od B; B dopiero po pierwszym poprawnym DATA/IDLE z nowym SID. SID16 to CRC16-CCITT z uporządkowanych A/B 6-bajtowych nonce, NIE uwierzytelnienie. Persistent `boot_epoch32` z `boot_epoch.cpp` zapisuje się w rotacyjnym dzienniku 16×6B EEPROM z CRC8 i markerem commit; `attempt16` jest w RAM i rośnie przy renegocjacji. To mechanizm wielokrotnego użytku.

Na granicach handshake B `replacePrxReply()` czeka 550µs, czyści TX/RX FIFO i kolejkuje tylko aktualny ACK; A `resetPtxFifos()` czyści stare dane. Brak 20/25ms delayów per DATA. Watchdog A ACTIVE=250ms, A WAIT_READY_ACK=1000ms, B ACTIVE=750ms, B WAIT_READY/WAIT_CONFIRM=2000ms; DISCOVER nie podbija nonce przy wielogodzinnej nieobecności peer. Na bieżącym `main.cpp` w RealTerm przy stabilnej sesji oczekiwane wspólne niezerowe `SID`, `ST=4`, `R` jest licznikiem odzyskiwań; przejściowe `BAD/MAX` przy resetach są możliwe.

W CI hostowe testy protokołu, sekwencji, resetów A/B, opóźnionych ACK, dziennika EEPROM z przerwanym zapisem, nRF SPI i PHY oraz kompilacja AVR-GCC **PASS**. Nowy handshake nie został dotąd uruchomiony na dwóch rzeczywistych RF-Nano (w przeciwieństwie do poprzedniego benchmarku 625/s); **nie przypisywać mu prędkości 625/s bez pomiaru**. Obecny UART jest tylko konsolą diagnostyczną. Kod bez `malloc/new`, Microchip Studio i ręczne F12 bez zmian. Specyfikacja: `paired-core-v2.md`.

## Uzupełnienie regresji sesji — 2026-11-04

Główna gałąź ma już handshake `PairedSession` z trwałą epoką EEPROM, a ostatnia interwencja **nie przepisywała go**. Dodano test `tests/paired_session_pipeline_test.cpp` symulujący prawdziwe opóźnienie ACK Payload B: odpowiedź jest wcześniej w FIFO, B odbiera ramkę A i dopiero przygotowuje następną odpowiedź. Testuje parowanie, 300 aktywnych wymian, restart jednej/obu stron, odrzucone odpowiedzi i przerwę w kierunku PTX. Host PASS na GitHub Actions. Nie zmienia fizycznego firmware; to większe pokrycie regresji testowej. Nadal brak **rzeczywistego** testu na dwóch RF-Nano najnowszej wersji z handshake. Do sprawdzenia: `ST=4`, wspólne niezerowe `SID` i przejściowy licznik `R`, po RESET A/B/obu. F7/F12 ręcznie, bez Post-Build.

## Rzeczywisty wynik handshake po pojedynczych resetach — 2026-11-04

Użytkownik przesłał dwa zrzuty z RealTerm obu RF-Nano po **osobnych resetach B oraz A**. Po resecie B oba mają **ST=4 i SID=12715**, po resecie A oba mają **ST=4 i SID=16598**. Działający podczas resetu partner ma R=1, restartowany R=0. W stanie ustalonym **612–613 wymian/s** w obu kierunkach, widoczne ARC/MAX/BAD/IO/GAB/GBA/DROP/FULL=0; jest to fizyczne potwierdzenie obecnego handshake'u i jego automatycznej rekonwergencji przy pojedynczych resetach, nie tylko host test. Przy fizycznym braku B A notowało `ARC=330 MAX=110` i `ARC=657 MAX=219`: **3 sprzętowe retransmisje na każde MAX_RT**, jak ustawiono w `SETUP_RETR=0x23`. Tymczasowe TX=0/s w DISCOVER są oczekiwane.

W porównaniu z 625 wymian/s poprzedniego programu narzut przy zdrowej sesji wynosi około **2%**, ~31–34 µs dodatkowego średniego odstępu na transakcję, prawdopodobnie wskutek pracy MCU podczas przetwarzania sesji i diagnostyki; to wnioskowanie, nie oddzielny pomiar czasu CPU. Podczas restartu głównym czynnikiem są rzeczywiste powtórki i czas do handshake'u, nie tylko procesor. **Jednoczesny reset obu płytek nie został tymi zrzutami potwierdzony.** Stałe RADIO/SPI/F12 zachowane. Jest to wyłącznie dopisanie wyniku testów do dokumentacji, bez zmiany kodu.

## Fizyczny jednoczesny restart obu ATmeg + identyfikacja pary — 2026-11-04

Dodatkowy zrzut dwóch RealTerm pokazuje **obydwa** bannery startowe oraz wznowienie w nowej wspólnej sesji `SID=13618` (wcześniej `16598`), `ST=4 R=0` po obu stronach, stabilne ~612–613 wymian/s bez widocznych błędów. Tym samym sprzętowo potwierdzono restart A, B oraz obu naraz. Nadal brak próby rzeczywistej izolowanej utraty zasilania radia, co jest osobnym scenariuszem.

Użytkownik rozważa stały „SSID/SID” po resecie. **Nie utrwalać SID sesji bez projektu ochrony przed starą ramką/ACK Payload.** Obecny SID16 jest celowo zmienny: `PairedSession::derive()` liczy CRC16 z 32-bitowych epok obu płyt zapisanych przez `BootEpoch::advance()` w EEPROM oraz 16-bitowych prób. Propozycja: trwałe `device_id`/`pair_id` zapisane przy parowaniu w EEPROM i wyświetlane niezależnie od `SID`; sesyjny SID nadal zmienny. Nie zaimplementowano tego pomysłu, brak zmian firmware.
