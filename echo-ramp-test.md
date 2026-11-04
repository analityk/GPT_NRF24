# Tymczasowy test PP: nasycenie radia, echo i pomiar błędów

## AKTUALNY WARIANT — pomiar granicy PP (po aktualizacji gałęzi)

Po teście fizycznym 12 000 CPS z surowym echem na UART eksperymentalny
firmware zmieniono na **RADIO_STRESS**. Obydwa urządzenia uruchamiają ten
sam HEX, a układ A od początku generuje tyle danych, ile pozwala bounded
stream PP (`periodUs=0`). Układ B odbija dane radiowo, tak jak poprzednio.
**A nie wysyła już bajtów `00..FF` na UART**, tylko weryfikuje kolejność
w pamięci. UART A i UART B wysyłają wyłącznie jedną linię ASCII metryk
na sekundę, więc szybkość wyświetlania/CH340 nie ogranicza goodput radia.

Podłącz RealTerm **COM10 (A)** i opcjonalnie **COM11 (B)** na 115200 8N1.
Przełącz oba okna na wyświetlanie **ASCII**, bez aktywnego wysyłania pliku;
nie używaj już `Hex[space]`, bo obecnie wyświetlana jest telemetria.
Komendy wpisuj **w RealTerm A/COM10**, zakładka Send, przycisk Send ASCII:

- `w` dodaje 100 µs do odstępu źródła (od 0 do 10 000 µs), spowalnia;
- `s` odejmuje 100 µs, przyspiesza (dolna granica 0);
- po włączeniu test **nie wymaga komend**, pracuje na maksimum.

Linie wyglądają przykładowo tak (wartości poniżej są ilustracyjne):

```
A cps=18000 gen=18048 arc=0 max=0 ret=0 dup=0 bad=0 blk=0 gap=0 err=0 rec=0 sid=12345 up=1
B cps=18024 fwd=18024 ret=0 dup=0 bad=0 blk=0 err=0 rec=0 sid=12345 up=1
```

Każdy odczyt odnosi się do poprzedniego okna około 1 s:

| Pole | Znaczenie |
| --- | --- |
| `A cps` | zaakceptowane bajty echa B→A na sekundę; na A sprawdzana jest kolejność modulo 256 |
| `A gen` | bajty nowo wygenerowane przez A na sekundę, niekoniecznie jeszcze zaakceptowane przez B |
| `B cps` | bajty A→B przyjęte przez B do bufora echa na sekundę |
| `B fwd` | bajty pobrane przez transport B z bufora echa do nadawania, nie gwarancja dostarczenia do A |
| `arc` (A) | suma sprzętowych retransmisji nRF `OBSERVE_TX.ARC_CNT` w oknie; B/PRX nie ma analogicznego licznika retransmisji ACK Payload |
| `max` (A) | liczba wyników `MAX_RT` (wyczerpano sprzętowe próby) |
| `ret` | powtórnie nadane segmenty DATA Go-back-N w danym kierunku, niezależne od `arc` |
| `dup` | duplikaty segmentów DATA rozpoznane przez transport lokalny |
| `bad` | niekolejne/przyszłe segmenty DATA i błędne dane na poziomie transportu |
| `blk` | odmowa przyjęcia całego segmentu do lokalnego bufora docelowego; nadawca musi ponowić |
| `gap` (A) | nieciągłości sekwencji 00..FF w odebranym echu (liczone w RAM) |
| `err` | pozostałe błędy radio/PHY/dekodowania odnotowane w `main.cpp` |
| `rec` | liczba ponownych negocjacji sesji od uruchomienia (wartość łączna) |
| `sid` | bieżący identyfikator sesji |
| `up` | `1` aktywna sesja, `0` sesja nieaktywna/negocjowana |

Wszystkie liczniki zdarzeń poza `rec`, `sid`, `up` dotyczą danego
sekundowego okna, nie są sumą od startu. CPS jest normalizowane rzeczywistą
długością okna Timer1; krótkie różnice od 1 s są uwzględniane.

**Scenariusz metalizowanego worka:** najpierw odczytaj kilka sekund
stabilnych `cps`, `arc`, `max`, `ret`, `gap`. Następnie stopniowo ekranuj
jedno lub oba radia. `arc` i `max` mogą rosnąć, choć `gap` zostaje zerem,
bo sprzętowe i aplikacyjne retry ukrywają straty w odebranym strumieniu.
Gdy łącze zniknie, CPS może spaść do zera i `up` może przejść na 0;
po odsłonięciu obserwuj nowy SID, `rec`, ewentualne `gap` i wznowienie CPS.
Nie jest to obietnica exactly-once przy resecie/rekey. Sekwencja 8-bitowa
nie wykryje straty dokładnie wielokrotności 256 bajtów, dlatego `gap=0`
nie jest dowodem absolutnej bezstratności. Uszkodzone radiowo ramki
odrzucone przez CRC w nRF nie są osobno zliczane programowo; ich skutki
zwykle ujawnią się przez retry, MAX_RT lub spadek CPS.
Jeśli zamiast metryk pojawi się pojedyncze `FATAL`, firmware wykrył
nieodwracalny błąd lokalny (np. SPI/EEPROM) i zatrzymał test.

Pozostawiono poprzedni `PpEchoRampPort::Mode::UART_ECHO` jako port
testowalny hostowo; aktywna aplikacja wybiera `Mode::RADIO_STRESS`.
Warstwy produkcyjne PP, sesja, format 8+24B, 1 Mb/s, SPI 8 MHz,
EEPROM oraz ręczne F7/F12 są niezmienione.

## Archiwalna instrukcja wcześniejszego trybu echo→UART

Poniższa instrukcja dotyczy **pierwszego commita** tej gałęzi z surowym
echem i początkowymi 10 ms/bajt. Nie odnosi się do obecnego firmware'u
po aktualizacji. Kod tamtego stanu pozostał w historii Git.

**Tylko gałąź `experiment/pp-echo-ramp-2026-11-04`.** Bazowy `main`
pozostaje na commicie `6b94a43` i nie został zmieniony. Ten wariant
zastępuje zastosowanie UART↔UART diagnostyką, ale zachowuje istniejące
`NrfAckPayloadPhy`, `PairedFrame`, `PairedLink`, `PairedSession`, EEPROM
SID oraz `UartReliableStream` z oknem 4×24 B i Go-back-N. To nie jest BURST20.

## Połączenie i obsługa

1. W Microchip Studio otwórz rozwiązanie z tej gałęzi. **F7 kompiluje,
   F12 ręcznie programuje**; brak automatycznego Post-Build.
2. Wgraj **ten sam HEX** na obie RF-Nano. A0 na A do GND (PTX), A0 na B
   do dodatniego napięcia tej płytki (PRX). A1 nie ma znaczenia.
3. RealTerm podłącz do **A**, UART **115200 8N1**, wyłącz local echo.
   Ustaw wyświetlanie **Hex[space]** i włącz binarny Capture, jeżeli chcesz
   później sprawdzić strumień bajt po bajcie.
4. Wysyłaj pojedyncze znaki poleceniem **Send ASCII**: `s` lub `w`.
   Komendy nie wracają w strumieniu binarnym. Na B port COM nie jest używany
   do echa; B odbija DATA przez **następny hardware ACK Payload**.

## Generator

- Początkowo 1 bajt na **10 000 µs (10 ms)**.
- Każde otrzymane `s` zmniejsza odstęp o dokładnie **100 µs**.
- Każde otrzymane `w` zwiększa odstęp o dokładnie **100 µs**.
- Zakres: **0..10 000 µs**, bez zawijania lub przekraczania zakresu.
- Zero oznacza generowanie tak szybko, jak opróżnia się ograniczona kolejka;
  nie oznacza zerowego czasu pojedynczej transmisji radiowej.
- Kolejne wartości **00 01 02 ... FE FF 00 01 ...** pojawiają się tylko
  po udanym umieszczeniu bajtu w lokalnej kolejce do radia.
- Kolejka A ma 127 bajtów użytecznej pojemności, ale generator dopisuje
  najwyżej do **120 bajtów**. Przy takim poziomie przerywa dopisywanie,
  zachowuje następny numer i czeka na zwolnienie miejsca. Nie nadpisuje
  danych; nie jest to bezpośrednie zapełnianie sprzętowego TX FIFO nRF.

Harmonogram opiera się na zegarze Timer1 2 MHz (0,5 µs/takt), z czasem
monotonicznym i wyrównaniem do zadanych terminów. Radio PP nadal wykonuje
ciągłe wymiany również między generowanymi bajtami. Przy bardzo małym
odstępie rzeczywisty transfer ograniczą okno 4 i przepustowość radiowa,
nie wartość zadanego generatora. Żadne polecenie nie wyłącza ani nie zmienia
sprzętowych potwierdzeń nRF.

## Diagnostyka bez zanieczyszczania UART

Na A, w debuggerze (lub w późniejszym wydzielonym kanale diagnostycznym):

- `diagnosticPort.periodUs()` — aktualny odstęp,
- `diagnosticPort.generated()` — bajty dopisane przez generator,
- `diagnosticPort.pressureEvents()` — przypadki nasycenia kolejki źródła,
- `diagnosticPort.echoed()` — bajty zwrócone i przyjęte do UART TX,
- `diagnosticPort.echoGaps()` — nieciągłości sekwencji w zwróconych bajtach,
- `stream.retransmits()`, `stream.blocked()`, `stream.badFrames()` — stan
  istniejącego transportu PP.

Echo wypisywane na UART A jest surowe, w tym `00`, `0A`, `0D` oraz `FF`.
Wygląd tekstu w oknie RealTerm **nie** jest dowodem poprawności — używaj
Capture binarnego i porównania kolejności bajtów. Wzrost liczby retransmisji
nie musi oznaczać błędu echo, ponieważ Go-back-N może ukrywać straty RF.
Nasycenie kolejki źródła oznacza ograniczenie wydajności, nawet jeśli nie
ma jeszcze uszkodzonych bajtów. Po restarcie urządzenia lub zmianie SID
nie deklarujemy exactly-once przez utratę RAM.

## Weryfikacja i powrót do normalnego PP

- Test hostowy: `sh tests/run_host_tests.sh` (w tym `pp_echo_ramp_test`).
- AVR: `sh tests/build_avr.sh` lub F7 w Microchip Studio. Kompilacja
  i test hostowy **nie zastępują** próby na dwóch fizycznych RF-Nano.
- Przywrócenie aplikacji: przełącz źródła z powrotem na gałąź **`main`**,
  zbuduj F7 i **ręcznie wgraj F12 na obie płytki**. Samo przełączenie
  gałęzi nie zmienia już zaprogramowanych mikrokontrolerów.

Nie scalać tej gałęzi z `main` jako produkcyjnego UART bridge. Po pomiarze
można ją zachować jako powtarzalne narzędzie diagnostyczne albo usunąć.
