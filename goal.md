# Cel GPT_NRF24 — pojedynczy dwuwęzłowy rdzeń radiowy

## Cel bieżącego etapu

Dwa identyczne RF-Nano (ATmega328P + nRF24L01+) wymieniają pakiety nRF24L01+ **DATA ↔ sprzętowy ACK Payload**, tworząc dwa logiczne kierunki komunikacji na fizycznie półdupleksowym radiu. Jeden firmware wybiera rolę lokalnie przez ADC0: A/PTX albo B/PRX. Ramka ma 8 B diagnostycznego nagłówka oraz 0–24 bajty **nieinterpretowanego BODY**.

Rdzeń powinien poprawnie raportować sekwencje, braki, retransmisje sprzętowe, zapełnienie bufora i stan sesji, rozdzielając potwierdzenie sprzętowe od dostarczenia aplikacyjnego. Nie może uzależniać przepustowości od arbitralnych, wielomilisekundowych opóźnień. Musi działać przy ograniczeniach 2 KB SRAM, bez dynamicznej alokacji.

## Potwierdzone wyniki fizyczne

- Dwa RF-Nano, firmware 8+24 B: około **625 wymian DATA/ACK Payload na sekundę**, przy braku widocznych błędów w zarejestrowanym teście; 24 bajty testowego BODY w każdym kierunku = około **15 kB/s/kierunek**.
- Niezależnie przetestowano wcześniej UART bridge starego formatu i benchmark 32+32 B (540 wymian/s). Oba programy są w gałęzi archiwalnej, nie w aktualnej kompilacji.

## Jeszcze nieukończone

- Negocjowanie i utrzymywanie identyfikatora sesji oraz automatyczne odzyskiwanie po resecie **zaimplementowano i sprawdzono hostowo/AVR (2026-11-04)**. Pozostaje **obowiązkowy test fizyczny** resetów obu RF-Nano; to nie jest jeszcze potwierdzony wynik sprzętowy.
- Udostępnianie odbiorcom z prawdziwą pamięcią właściwych `RX_CREDIT` i `RX_DROP`, diagnostyka utraty/zwłoki w obu kierunkach.
- Test dynamicznych, krótkich IDLE i CONTROL w realnym radiu oraz dokładne pomiary latencji, jittera i obciążenia.
- Możliwość podłączenia usług nad nieinterpretowanym BODY (np. UART, pliki, komunikaty, ewentualnie szyfrowane dane) przez osobne kontrakty. W tej fazie nie implementujemy ani nie definiujemy jeszcze szczegółów tych usług.

**Warunek zaliczenia bieżącego rdzenia:** dwukierunkowe DATA↔ACK Payload, wiarygodne metadane i automatyczne odzyskiwanie sesji w rzeczywistym sprzęcie. Obecny test **potwierdza szybki transfer i podstawową diagnostykę**, nie potwierdza jeszcze pełnej odporności na restarty i przeciążenia.

Zachowujemy manualne F7/F12 w Microchip Studio oraz jeden aktualny program; historia wcześniejszych etapów znajduje się w `history.md`.
