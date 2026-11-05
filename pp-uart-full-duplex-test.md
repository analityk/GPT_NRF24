# PP: jednoczesna transmisja plików UART A ↔ B

## Wersja testowa

Gałąź `testy` uruchamia domyślnie tryb
`PP_TEST_TRAFFIC_MODE=PP_TRAFFIC_MODE_UART_BRIDGE`
(`GPT_NRF2401/pp_test_traffic_mode.h`).
Jeden identyczny firmware trafia na obie RF-Nano. A0 przy masie
wybiera rolę **A/PTX**, A0 przy 3,3 V rolę **B/PRX**.
W obu przypadkach USART0 115200 8N1 jest strumieniem binarnym:
RX z PC -> `UartReliableStream` -> RF -> UART TX drugiej płytki.
Nie ma wewnętrznego generatora, komend `s/w`, tekstowego statusu,
CR/LF ani dodatkowego echa. Stos radiowy PP 8+24 B, WINDOW=4,
ACK Payload, SID/rekey i ochronę przed duplikatami pozostawiono.
Radio nadal pracuje na 2 Mb/s, SPI na 8 MHz.

Dla powrotu do pomiaru przepustowości radia wystarczy przed kompilacją
ustawić `PP_TEST_TRAFFIC_MODE` na
`PP_TRAFFIC_MODE_SYNTHETIC_RADIO` w tym samym nagłówku
(lub przez flagę kompilatora).
To świadomy wybór adaptera aplikacyjnego, nie druga implementacja
protokołu.

## RealTerm: test równoczesny

1. `git switch testy`, `git pull --ff-only origin testy`,
   w Microchip Studio F7, ręcznie F12 na obie płytki.
2. Otworzyć obydwa porty w RealTerm, każdy **115200 8N1**,
   bez programowego XON/XOFF i bez sprzętowego RTS/CTS.
   Wysokość logiczna A0 definiuje role, nie numer COM.
   Otwarcie portu może zresetować ATmegę przez bootloader;
   po otwarciu obu portów odczekać na automatyczne zestawienie sesji.
3. Na komputerze z Python 3 przygotować pliki testowe
   (wartości zerowe, `FF`, `s/w`, CR/LF w środku):
   
   ```bat
   python tools\duplex_uart_vectors.py create --directory C:\temp\pp_uart --size 8192
   ```

4. W RealTerm na A: **Capture** -> raw/binary -> plik
   `C:\temp\pp_uart\capture_at_a.bin`. Na B: Capture ->
   `C:\temp\pp_uart\capture_at_b.bin`. Rozpocząć przechwytywanie
   na obu portach **przed** wysyłaniem. Upewnić się, że capture jest
   surowy (bez formatowania HEX, znaczników czasu i dodanych CR/LF).
5. W zakładce **Send** użyć `Send File` (binarna wysyłka pliku)
   z A: `a_to_b.bin`; z B: `b_to_a.bin`. Rozpocząć obie wysyłki
   możliwie jednocześnie. Nie włączać `Local Echo`, `Echo Port`,
   dodawania `EOL` ani filtrów tekstowych; `Delays=0,0`.
6. Po zakończeniu obu transmisji i opróżnieniu odbioru zatrzymać
   obie sesje Capture. **Nie otwierać ponownie portów w czasie próby**
   (otwarcie może resetować mikrokontroler).
7. Sprawdzić **oba niezależne kierunki**:
   
   ```bat
   python tools\duplex_uart_vectors.py verify --directory C:\temp\pp_uart
   ```

   `a_to_b.bin` ma być identyczny z
   `capture_at_b.bin`; `b_to_a.bin` z
   `capture_at_a.bin`. Program porównuje dokładnie bajty,
   rozmiary i podaje offset pierwszej rozbieżności.
   Dla kolejnej próby wyczyścić poprzedni capture, inaczej
   porównanie może wykazać nadmiar danych.

Na pierwszą próbę można zmniejszyć `--size` do 512, następnie
8192 i później większych wartości. Test rozpocząć od obu
nieobciążonych płyt; po pozytywnym porównaniu sprawdzić reset A,
reset B i osłabienie RF. **Nie resetować w połowie porównywanego
strumienia bez zapisania granicy próby** — RAM nadajnika
zresetowanej płytki zostanie utracony.

## Interpretacja

Maksymalna prędkość jednego UART 115200 8N1 to 11520 B/s
w kierunku RX i osobno TX. Dwa porty mogą więc w idealnych
warunkach utrzymywać po 11520 B/s A->B i B->A jednocześnie.
Wcześniejsze `cps≈21720` dotyczyły pełnego generatora RF,
**nie** pełnego dupleksu przez UART; bez porównania dwóch
capture nie potwierdzają bezstratności.

RX UART ma 127 B użytecznej pojemności, TX też 127 B. Nie ma
sygnalizacji RTS/CTS. Gdy radio nie nadąża, link padnie albo
komputer wyśle dane w nadmiernym burst, **RX UART może zgubić
bajty przed przyjęciem do retransmitowanego okna**. Radiowe ACK
nie może ich odzyskać; to ograniczenie interfejsu hosta, nie
błąd CRC radiowego. Test 0/0 ma właśnie ujawnić takie przypadki,
zamiast je ukrywać. Zmianę źródła na pełne stabilne 115200 8N1
ocenimy w kolejnej iteracji, na podstawie plików i licznika
przepełnień odczytywanego przez debugger, bez zakłócania UART.

Jeżeli brakuje odbioru, najpierw potwierdzić zasilanie, A0,
tryb Capture i czy oba porty są już otwarte. Brak tekstu
`A cps` lub `B cps` w tym firmware jest **zamierzony**.

## Weryfikacja automatyczna firmware i narzędzi

GitHub Actions [#42075780034](https://github.com/analityk/GPT_NRF24/actions/runs/42075780034):
32 testy C++ na hoście PASS, oba tryby firmware AVR PASS,
porównywarka plików poprawnie wykrywa celowo wprowadzony nadmiarowy
bajt. UART BRIDGE: 12712 B Flash, 961 B statycznego SRAM; SYNTHETIC
RADIO: 19214 B Flash, 1139 B SRAM. Kompilacja i mock nie zastępują
pomiaru dwóch prawdziwych RF-Nano.
