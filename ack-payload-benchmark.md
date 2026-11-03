# Fizyczny test ping-ponga ACK Payload (2026-11-03)

**Cel:** zmierzyć faktyczne tempo pełnych, 32-bajtowych ramek radiowych DATA A→B i pełnych 32-bajtowych odpowiedzi B→A w **sprzętowym ACK Payload**. To osobny test rdzenia, nie docelowy UART transport, STREAM ani BURST.

## Budowanie i uruchamianie

1. `GPT_NRF2401/application_select.h` ma `#define GPT_NRF24_ACK_BENCHMARK 1`. Ustawienie **1** wybiera ten test; **0** przywraca poprzedni, sprawdzony most UART bez jego przepisywania. Nie zmieniaj ustawień F7/F12 ani Post-Build.
2. Zbuduj w Microchip Studio **F7** i ręcznie wgraj **F12** ten sam HEX na obie płytki. ADC0 do GND na A = PTX, ADC0 do dodatniego zasilania tej płytki na B = PRX. Piny nRF zostają CE PB2/D10, CSN PB1/D9, SPI PB3/PB4/PB5.
3. Otwórz oba RealTerm: 115200 baud, 8N1, bez lokalnego echa. Program od razu rozpocznie wymianę, nie trzeba nic wpisywać ani wysyłać pliku. Porty zależą od Windows (poprzednio A=COM10, B=COM11).
4. Po uruchomieniu A wypisuje `ACK BENCH A PTX 32B/32B 1Mbps SPI8`, B wypisuje `ACK BENCH B PRX 32B/32B 1Mbps SPI8`. Co około 1 s każdy węzeł pokazuje zmierzone liczniki za okres; `GAP` jest narastający od startu.
5. Jeśli inicjalizacja się nie uda, komunikat `ACK BENCH FAIL NRF_PROBE`, `FEATURE_OR_DPL`, `PTX`, `PRX`, `PRIME_ACK` lub `ADC0` identyfikuje etap awarii. Nie interpretuj pustego ekranu jako poprawnego startu.

## Liczniki i znaczenie

Przykładowa **składnia**, NIE prognozowana wartość liczb:

```text
A TX/s=... ACK/s=... EMPTY=... MAX=... ARC=... BAD=... IO=... GAP=...
B RX/s=... Q/s=... FULL=... BAD=... IO=... GAP=...
```

- `A TX/s`: liczba nowych 32-bajtowych DATA A, dla których radio otrzymało *jakikolwiek* sprzętowy ACK (TX_DS); nie uwzględnia `MAX_RT`.
- `A ACK/s`: liczba poprawnych, kompletnych **32 B ACK Payload B** odczytanych przez ATmegę i sprawdzonych ze wzorcem. Ta liczba może być niższa niż `TX/s`, jeśli B nie załaduje danych do FIFO na czas.
- `A EMPTY`: skuteczne ACK radiowe, lecz bez danych ACK Payload. `MAX`: wyczerpane retransmisje sprzętowe. `ARC`: suma sprzętowych retransmisji w okresie. `BAD`: zły wzorzec, błędna szerokość dynamicznego payloadu. `IO`: błąd SPI lub timeout PHY. `GAP`: narastający licznik brakujących kolejnych numerów B (nie jest to wyliczony BER).
- `B RX/s`: 32-bajtowe DATA A fizycznie pobrane z RX FIFO i zweryfikowane ze wzorcem. `Q/s`: nowe 32-bajtowe odpowiedzi B poprawnie włożone do sprzętowej kolejki ACK; **to nie jest potwierdzenie ich doręczenia**. `FULL`: momenty pełnej kolejki odpowiedzi; `BAD`, `IO`, `GAP`: analogicznie.
- Liczniki chwilowe powinny po rozgrzaniu stabilizować się; nie interpretuj jednego ekranu jako długookresowej średniej. Zapisz co najmniej 10–20 kolejnych raportów z obu węzłów.

**Surowa przepustowość payloadu:** A→B = `B RX/s * 32 B`; B→A = `A ACK/s * 32 B`. To jeszcze nie użyteczna przepustowość UART v1: ramka v1 zużywa 8 B na nagłówek, zaś DATA/ACK i telemetria zajmują kolejne bajty.

## Co robi kod

- `nrf24.h`: włącza FEATURE.EN_DPL + EN_ACK_PAY, DYNPD na pipe0, ustawia retry 0x23 (ARD 750 µs, ARC 3); wykorzystuje W_ACK_PAYLOAD i R_RX_PL_WID. W razie potrzeby próbuje aktywować FEATURE komendą ACTIVATE 0x73 po nieudanym readbacku; zwraca błąd, gdy chip tego nie obsługuje.
- `nrf24_ack_phy.h`: zdarzeniowo czeka na TX_DS/MAX_RT przez status SPI; 8 ms to **deadline awarii**, nie przerwa między ramkami. Raz po starcie radia następuje jego 2 ms PWR_UP, potem 15 µs impuls CE i fizyczny minimalny 130 µs guard między transakcjami PTX. B pozostaje PRX. Przed ACK musi istnieć uprzednio załadowana porcja B; pierwszą kolejkuje się przy uruchomieniu.
- `spi.cpp`: benchmark wybiera 8 MHz SPI (ATmega16MHz /2; nRF do 10 MHz), ale poprzedni most zachowuje konfigurację 1 MHz.
- `timer1_clock.*`: rozszerzony Timer1 przy 2 MHz (0,5 µs) pozwala mierzyć prawdziwy czas i watchdog PHY. Nie ma synchronizacji zegarów A/B.
- `ack_bench_pattern.h`: 32-bajtowa sygnatura + identyfikator kierunku + 32-bitowy licznik + treść wzorcowa, bez dynamicznej alokacji. `GAP` nie jest end-to-end aplikacyjnym ACK.
- `ack_bench_main.cpp`: **jeden** wsad A/B, dwa statyczne bufory radiowe, jeden raport tekstowy/s. Brak przerwań IRQ nRF: sygnał IRQ może nie być wyprowadzony na konkretnej RF-Nano; status polling SPI wystarcza.

## Granice eksperymentu

Nie implementowano UART RELIABLE, prawdziwego rozmiaru pliku, SACK ani przekazywania roli PTX. Ten benchmark mierzy rdzeń fizyczny ACK Payload, nie deklaruje bezstratności lub użytecznej przepustowości końcowej. Nie używa sztucznych opóźnień 20/25 ms. Wyniki na hostowych atrapach potwierdzają logikę, lecz **kompilacja AVR i działanie na dwóch płytkach wymagają sprawdzenia przez użytkownika**.

Podstawowe źródło hardware: Nordic nRF24L01+ Product Specification v1.0, rys. 21–24, tabela SPI: https://cdn.sparkfun.com/assets/3/d/8/5/1/nRF24L01P_Product_Specification_1_0.pdf
