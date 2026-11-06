# Jednoogniskowy pomiar PP przy równoczesnym UART A ↔ B

**Gałąź:** `testy`, wersja diagnostyczna na 2026-11-05.

## Co mierzymy

Głównym pomiarem czasu jest **odstęp między rozpoczęciem dwóch kolejnych
transakcji PTX (A)**, ale tylko gdy poprzednia wymiana była poprawna, miała zerowy ARC
i przeniosła dane użytkownika w co najmniej jednym kierunku.
Dopuszczamy krótsze ramki DATA, aby próba przy 6 kB/s też dała próbki.
Mierzone są różnice odczytów 16-bitowego Timer1 (`TCNT1`, 2 MHz),
z rozdzielczością 0,5 µs, bez 32-bitowego znacznika czasu dla cyklu.
Miernik zachowuje średnią, minimum i maksimum do 1024 próbek.
Odcinek musi trwać mniej niż 32,768 ms. Nie należy utożsamiać
`cyc` z samym czasem przebywania pakietu w eterze: obejmuje również
pracę MCU, SPI, polling, procedurę ACK i przerwę między startami.

Pozostałe informacje to **liczniki**, nie kolejne sondy czasowe.
Co około 10 ms firmware odczytuje liczby przerwań UART i postępu
transportu; ISR aktualizuje jedynie 16-bitowe liczniki bajtów.
Po **0,8 sekundy bez nowych danych z UART, utraty RX i postępu
aplikacyjnego ACK/RX** firmware dopisuje tekstowe wyniki do lokalnego
UART. Krótkie przerwy RealTerm po końcach linii nie uruchamiają raportu.

Ten wsad celowo dodaje na końcu strumienia tekst diagnostyczny.
Przechwycony plik nie będzie byte-for-byte równy źródłu.
Dla testów bez dodatków należy wyłączyć
`PP_UART_DIAG_ENABLED` (np. `-DPP_UART_DIAG_ENABLED=0`) i
skompilować ponownie. Ta zmiana nie wyłącza protokołu ani ramek IDLE.
Nie trzeba podpinać JTAG/SWD ani żadnych dodatkowych przewodów.

## Przeprowadzenie próby

1. Uaktualnij istniejący `testy` przez `git pull --ff-only origin testy`
   po upewnieniu się, że nie nadpiszesz własnych lokalnych zmian.
2. F7 i ręczne F12 na obydwu RF-Nano. Zdejmij zworę TX–RX. Zostaw
   niezależne COM A oraz COM B, 115200 8N1, bez RTS/CTS, XON/XOFF i
   local echo. Otwórz porty, odczekaj na handshake po automatycznym
   resecie ATmegi.
3. Uruchom równoczesny `Send File` w obu RealTerm z identycznymi
   plikami tekstowymi po ~100 linii × 128 bajtów plus CRLF.
   **Pierwsza próba: `Delays=0,20`**, druga **`Delays=0,10`**.
   Nie zmieniaj innych parametrów między próbami.
4. Po zakończeniu `Send File` pozostaw obydwa porty otwarte.
   Po około 0,8 s ciszy powinny pojawić się raporty. Zapisz zdjęcie
   z całymi liniami raportów na obu COM. Przed nową próbą możesz
   wyczyścić okno RealTerm (Clear), nie zamykając portu.

Przykładowe etykiety raportów (liczby są rzeczywistymi pomiarami,
nie wartościami oczekiwanymi):

```text
A uart in=... out=... full=... dor=... fe=... free=...
A pp ack=... rx=... ret=... blk=... bad=... win=... cr=... hw=... mx=... sid=...
A rf n=... cyc=... lo=... hi=...
B uart in=... out=... full=... dor=... fe=... free=...
B pp ack=... rx=... ret=... blk=... bad=... win=... cr=... af=... rf=... sid=...
```

Znaczenie:

- `uart in` — bajty zaakceptowane do RX ring przez przerwanie USART;
  `uart out` — bajty przekazane do rejestru nadawania USART (z licznika
  odjęto poprzednie linie diagnostyczne).
- `full` — bajty odrzucone, gdy 255-bajtowy retained RX ring był pełny.
  `dor` — sprzętowe nadpisanie bajtu wejścia USART przed obsługą;
  `fe` — framing error.
- `pp ack` — bajty, które odbiorca po drugiej stronie potwierdził
  aplikacyjnie. `pp rx` — bajty zaakceptowane lokalnie z radia
  do bufora wyjściowego UART.
- `hw` (A) — suma sprzętowych retransmisji nRF24 (ARC_CNT);
  `mx` — liczba zdarzeń MAX_RT. To **inne liczniki** niż `ret`
  (powtórzenia Go-Back-N w protokole PP).
- `af` (B) — liczba prób dopisania ACK Payload do pełnego TX FIFO
  odbiornika; `rf` — liczba odczytów RX FIFO stwierdzających FULL.
  `af` i `rf` są licznikami obserwacji pełnych FIFO, nie liczbą
  utraconych pakietów.
  Duże `ret` przy niewielkim `hw` sugeruje opóźnione potwierdzenia,
  kolejkę ACK lub wyzwalanie Go-Back-N bez błędów warstwy radiowej.
- `ret` — retransmitowane ramki PP, `blk` — odrzucone przyjęcie
  pakietu z powodu pełnego UART TX, `bad` — błędy kolejności ramek,
  `win` — aktualnie niepotwierdzone ramki, `cr` — aktualny
  kredyt ogłoszony przez partnera, `sid` — sesja radiowa.
- `A rf n/cyc/lo/hi` — liczba, średnia, minimum, maksimum
  poprawnych cykli z DATA w przynajmniej jednym kierunku. `n=0`
  oznacza mniej niż 32 takie wymiany bez retransmisji sprzętowych;
  długość payloadu może być różna od 32 B.

## Rozstrzygnięcie

Dla jednego strumienia A→B porównaj kolejno:
`A uart in` → `A pp ack` → `B pp rx` → `B uart out`.
Analogicznie B→A. Przy przerwanym torze pierwszy licznik, od którego
wartości przestają się zgadzać, lokalizuje wąskie gardło. Jeśli
`full>0` lub `dor>0`, wejście UART traci bajty przed PP. Jeżeli
`uart in` jest poprawne, ale `pp ack` dużo mniejsze, skontroluj
`ret/blk/bad/win/cr` i `A rf cyc`. Jeśli `n=0`, sam czas pełnego
cyklu nie jest dostępny; nie wolno zgadywać wartości.

Przy UART 115200 8N1 pełne 128 B zajmuje 11,11 ms, a 130 B
(128+CRLF) zajmuje około 11,28 ms. Z przerwą 10 ms daje to modelowe
około 6,1 kB/s na kierunek; z przerwą 20 ms około 4,2 kB/s.
Rzeczywisty profil `Send File` i opóźnienia Windows należy ocenić
po licznikach: nie zakładać idealnego zegara przerw RealTerm.

Liczniki raportu mają 16 bitów i rozróżniają poprawnie próbę do
65 535 bajtów na licznik. Do dłuższych prób użyj krótszych plików
lub restartu między próbami; pomiar dotyczy wydzielonego okna
aktywności, nie czasu od uruchomienia mikrokontrolera.
Wynik testów hostowych i kompilacji AVR nie zastępuje zrzutu
z fizycznych płytek.
