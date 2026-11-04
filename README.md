# GPT_NRF24 — UART ↔ nRF24L01+ ACK Payload ↔ UART

**Aktualny etap (2026-11-04):** jeden firmware na dwie identyczne płytki RF-Nano (ATmega328P 16 MHz + nRF24L01+ + CH340), z prawdziwym **dwukierunkowym strumieniem binarnych bajtów UART** przez sprzętowy DATA/ACK Payload. Hostowe testy C++11, symulacja strat i krzyżowy AVR build **PASS**. **Nowego mostu UART w tym profilu nie zweryfikowano jeszcze na fizycznych RF-Nano**.

## Uruchomienie w Microchip Studio

1. Sklonuj / zaktualizuj `main` i otwórz `GPT_NRF2401.atsln`.
2. **F7** kompiluje, **F12** ręcznie programuje. **Brak automatycznego flashowania Post-Build**.
3. Wgraj ten sam HEX na obie RF-Nano. Na A ADC0/A0 do **GND** (PTX), na B ADC0/A0 do **dodatniego zasilania tej płytki** (PRX).
4. Otwórz oba RealTerm na odpowiednich COM, **115200 8N1**, wyłącz **local echo** i nadmiarowe dopisywanie CR/LF przez aplikację terminalową (chyba że właśnie chcesz te bajty wysłać).
5. Wyślij z A `ala ma kota`, sprawdź pojawienie się wyłącznie na B. Wyślij z B `12345`, sprawdź pojawienie się wyłącznie na A. Następnie wyślij dwie różne wiadomości równocześnie, również dłuższe niż 22 bajty.

**UART przesyła wyłącznie dane użytkownika** — także `00`, `FF`, CR/LF. Nie zobaczysz w nowym wsadzie tekstu `PAIR CORE`, `ST=4` ani liczników; diagnostyka nie może zanieczyszczać binarnego strumienia. `SID` nadal automatycznie negocjowany jest wewnątrz firmware, lecz nie drukowany na COM.

## Aktualny stos i format

- **PHY:** nRF24L01+, 1 Mb/s, CRC16, SPI 8 MHz; A stale PTX, B stale PRX, a B odsyła zwrotne dane w uprzednio przygotowanym **sprzętowym ACK Payload**.
- **Nagłówek rdzenia:** bez zmian: 8 bajtów (`TX_SEQ`, `RX_LAST`, `LOSS_AB/BA`, `ARC_LAST/RX_DROP`, `RX_CREDIT`, `TYPE_FLAGS`, `SESSION_ID16`).
- **BODY:** do 24 bajtów pozostaje nieinterpretowane w PHY/MAC. Samodzielna usługa `UartReliableStream` wykorzystuje **2 bajty BODY na `DATA_SEQ` i aplikacyjny `ACK_SEQ`**, więc przenosi **0–22 bajty oryginalnego UART na jedną ramkę**.
- **Niezawodność:** oddzielny stop-and-wait w obu kierunkach; dane wypisywane są do UART **atomowo całym segmentem**. Sprzętowy ACK nie jest mylony z dostarczeniem do kolejki UART. Powtórzenia tej samej porcji nie duplikują bajtów w ramach aktywnej sesji.
- **Kontrola przepływu:** `RX_CREDIT` pochodzi teraz z rzeczywistego wolnego miejsca w kolejce UART TX (jednostka: 24 bajty), a nie ze stałej `1`. Przy zapełnieniu odbiorcy nie przyjmuje się nowego segmentu, nie wysyła dla niego aplikacyjnego ACK i następuje ponowienie.
- **Sesja:** HELLO/BEACON/WELCOME/READY/READY_ACK, identyfikator `SID16` z epok EEPROM i liczników prób, auto-odnowienie po resecie. W razie zmiany sesji niepotwierdzony segment utrzymywany jest w RAM strony, która nie została zresetowana.
- **Bezczynność:** A wykonuje krótkie zapytanie o gotowość/dane B mniej więcej raz na 10 ms; gdy przychodzą dane, nadaje szybko, bez starych opóźnień 20/25 ms.

## Zachowanie i ograniczenia

**Brak RTS/CTS.** USART RX ma pojemność 127 bajtów. Jeśli RealTerm bez ograniczeń poda plik szybciej, niż radio i odbiornik mogą odbierać, lub w czasie awarii łącza, ta kolejka może się przepełnić. Nie jest to sytuacja, którą radiowy ACK naprawi wstecznie. Pierwszy test: krótkie komunikaty; przy pliku w RealTerm włącz niewielkie opóźnienia i porównaj pełne pliki capture bajt po bajcie.

Stop-and-wait jest **wersją funkcjonalną**, nie maksymalnie wydajnym BULK ani pełną usługą plikową. Każdy kierunek ma jeden segment w locie, czyli dalsze zwiększanie przepustowości będzie wymagało większego okna. Testowany UART 115200 8N1 daje maksymalnie 11 520 B/s na kierunek; **obecnej użytecznej przepustowości UART nie zmierzono jeszcze**.

**Reset i dokładnie-jeden-raz:** bieżący MCU zachowuje niepotwierdzoną porcję w RAM podczas renegocjacji sesji po resecie partnera, lecz reset własnego MCU usuwa dane niezapisane trwale. Jeśli partner zdążył wypisać bajty na UART, a jego potwierdzenie zaginęło przed resetem, możliwe jest ich ponowienie po nowej sesji. Pełnej gwarancji exactly-once między resetami bez trwałego dziennika transakcji nie deklarujemy.

## Pliki

| Warstwa | Źródła | Zastosowanie |
|---|---|---|
| Aplikacja AVR | `GPT_NRF2401/main.cpp` | jeden program A/B, przezroczysty most UART |
| USART / ring | `uart.*`, `byte_ring.h` | nieblokujący UART 115200, licznik overflow |
| Usługa transportowa | `uart_reliable_stream.h` | DATA_SEQ/ACK_SEQ, retry, deduplikacja, UART credit |
| Rama RF | `paired_frame.h`, `paired_link.h` | 8+24 B, stan jakości łącza |
| Sesja / EEPROM | `paired_session.h`, `epoch_journal.h`, `boot_epoch.*` | negocjacja SID, ochrona przed starym ACK |
| Radio i sprzęt | `nrf24.h`, `nrf24_ack_phy.h`, `spi.*`, `gpio.*`, `adc0.*`, `timer1_clock.*` | Enhanced ShockBurst i platforma |

## Weryfikacja i historia wyników

`sh tests/run_host_tests.sh` odkrywa aktualne testy hostowe. `sh tests/build_avr.sh` kompiluje jeden program dla AVR bez programowania płytki. [GitHub Actions](https://github.com/analityk/GPT_NRF24/actions) uruchamia oba zadania. Nowe testy UART obejmują opóźnione ACK, brak miejsca w UART TX, renumerację sesji, zawinięcie numeracji oraz **4096 bajtów dowolnych danych w obu kierunkach** z symulowaną utratą ramek i ACK Payload — oba strumienie odtwarzają się dokładnie w symulacji.

**Historyczne pomiary sprzętowe (inny firmware):** rdzeń 8+24 B bez handshake'u około 625 wymian/s; z handshake'em około 612–613 wymian/s, automatyczny powrót po resetach A, B i obu płyt. Nie są to pomiary nowej usługi UART. Wcześniejsze implementacje zachowane w gałęzi [archive/pre-paired-core-cleanup-2026-11-03](https://github.com/analityk/GPT_NRF24/tree/archive/pre-paired-core-cleanup-2026-11-03).

Szczegóły formatu i semantyki: [paired-core-v2.md](paired-core-v2.md). Historia fizycznych prób: [history.md](history.md). Bieżące zadania: [todo.md](todo.md).

## Uwaga po pierwszym sprzętowym teście dwukierunkowego UART (2026-11-04)

Wysłanie pliku **w jednym kierunku działa**. Równoczesny intensywny transfer w obu kierunkach ujawnił **zgłaszane ubytki bajtów**; NIE zaliczono bezstratnego dwukierunkowego flood. USART ATmega jest pełnodupleksowy i asynchroniczny — problemem może być przeciążenie wejściowych kolejek 127 B, gdy transport stop-and-wait przesyła najwyżej 22 B/segment i czeka na aplikacyjne ACK. Przy referencyjnych ~613 wymian/s przybliżony limit stop-and-wait to ~6,7 kB/s/kierunek, niższy niż pełne 115200 8N1 (~11,52 kB/s); aktualnej rzeczywistej prędkości nie zmierzono.

**Próba rozstrzygająca:** w RealTerm ustaw dla OBU nadawców w zakładce Send / Dump File / `Delays` **pierwsze pole = 1** (1 ms na znak), **drugie = 0** (brak dodatkowej przerwy na koniec linii). Wyślij oba pliki i porównaj binarny capture z oryginałami. Gdy to działa, zmniejszaj pierwszy delay, aby wyznaczyć granicę obecnego transportu. `Delays=0,10` z pokazanego eksperymentu to 0ms na znak i 10ms po linii: może nadal pozwalać długim liniom zapełnić 127B RX ring.

Nowy `tests/uart_unthrottled_ingress_test.cpp` jest powtarzalną regresją tego przypadku: symuluje UART bez RTS/CTS przy 3 i 19 B/iterację RF, z rzeczywistym `UartReliableStream` i opóźnionym ACK. Przy 19 B/iterację przepełnia oba wejścia nawet bez błędów radia, przy 3 B nie. Dotychczasowy stress test 4096B na kierunek **sztucznie zwalniał wejście**, więc nie udowadniał odporności na flood. Nie zmieniono jeszcze transportu produkcyjnego; do poprawy architektonicznej potrzebne większe okno lub flow-control, najlepiej oba.
