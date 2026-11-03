# Plan rozwoju aktualnego rdzenia RF 8+24 B

## Architektura

- **`Nrf24` / SPI / sprzęt:** kontrola rejestrów, FIFO, dynamicznych szerokości, CRC16/auto-ACK i timeoutów; nie interpretuje BODY.
- **`NrfAckPayloadPhy`:** stabilna rola A/PTX, B/PRX, minimalne czasy radiowe, zdarzenia `TX_DS/MAX_RT/RX_DR`, przenoszenie ramek 8–32 B.
- **`PairedFrame`:** jeden schemat 8 B nagłówka, do 24 B opaque BODY; niezależny od AVR, jawne kodowanie bajtów.
- **`PairedLink`:** numeracja i statystyki dwóch kierunków, odrzucanie starych duplikatów i obcych sesji, kredyty i licznik odrzuceń.
- **`main.cpp`:** tymczasowa aplikacja fizycznego testu rdzenia, odpowiada za testowe BODY i konsolę diagnostyczną, nie za transport plików.

## Ustalone wyniki

1. **DONE** — sprzętowy ping-pong pełnych DATA + ACK Payload, wcześniej 540/s.
2. **DONE** — nowa ramka 8+24, 625 wymian/s na dwóch RF-Nano, w obserwowanych oknach zero widocznych błędów, statyczne bufory.
3. **DONE** — jedna ścieżka aplikacyjna i projektowa, bez pozostawionych fragmentów historycznego firmware; starszy stan zapisany w gałęzi `archive/pre-paired-core-cleanup-2026-11-03`.
4. **DONE** — hostowe testy kodeka/PHY/radia/USART i automatyczny cross-build AVR-GCC, bez automatycznego flashowania.

## Najbliższe kroki

1. Uzgadnianie `SESSION_ID16`, identyfikatorów/numerów epoki i automatyczne odtwarzanie po resecie; wyraźne rozdzielenie `DISCOVERY` od aktywnej sesji.
2. Poprawna kontrola przepływu: `RX_CREDIT` wynikające z rzeczywistego sinku, zapas na opóźniony ACK Payload, statusy przeciążenia; testy negatywne.
3. Stan łącza i diagnostyka: rozdzielić hardware retry `ARC`, `MAX_RT`, luki w TX_SEQ, lokalne odrzucenia i timeouty; testować realną utratę łączności bez oszukiwania fizyki.
4. Przełączenie na krótkie dynamiczne ramki IDLE/CONTROL w spoczynku; zmierzyć przepustowość, latencję i zużycie energii przy różnym obciążeniu.
5. Dopiero po ukończeniu rdzenia dopuścić usługi transportowe nad BODY, nie ingerując w jego znaczenie z poziomu MAC/PHY.

**Nie rozwijamy kilku alternatywnych firmware jednocześnie.** Jedyne aktywne źródło działania i specyfikacja to `main.cpp` i `paired-core-v2.md`. Dawne warianty są dostępne przez historię Git.
