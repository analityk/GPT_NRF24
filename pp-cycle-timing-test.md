# Test czasu jednej wymiany PP — gałąź `testy`

## Po co mierzymy

Na dwóch fizycznych RF-Nano otrzymano około 13,3 kB/s przy 1 Mb/s
i około 16,1 kB/s po ręcznym przejściu na 2 Mb/s. Nie wiadomo jeszcze,
co zajmuje pozostały czas pojedynczej transakcji DATA ↔ ACK Payload.
Samo porównanie szybkości modulacji nie mierzy obciążenia CPU ani
oczekiwania na sprzętowy status nRF24.

To **sonda diagnostyczna**, a nie nowy protokół ani dowód zysku.
`main` pozostaje w wersji zaakceptowanej przez właściciela; próbę
uruchamiamy wyłącznie na `testy`.

## Konfiguracja i pomiar

- Jeden HEX na oba RF-Nano; A0 wybiera rolę jak dotychczas.
- `PP_TEST_RF_SETUP_VALUE` wybiera `NRF24_RF_SETUP_2_MBPS` w tym teście
  **dla obu** płytek; sterownik `Nrf24` nadal ma domyślnie 1 Mb/s.
- `PP_PTX_TIMING_PROBE_ENABLED` włącza sondę tylko na A. Wartość
  `false` wyłącza jej wywołania, co umożliwia próbę kontrolną.
- Sprzętowy Timer1 pracuje z rozdzielczością 0,5 µs; brak ISR od SPI.
  Wyniki są średnią z 32–1024 sąsiednich, pełnych wymian po 32 B,
  z poprawnym 32-bajtowym ACK, bez sprzętowego retry, w stabilnym SID
  oraz stanie HEALTHY. Reset, negotiation, krótszy pakiet i retry
  **nie są włączane do średnich**.
- Raport trafia tylko na konsolę A, mniej więcej co sekundę, z
  przesunięciem o pół sekundy względem raportu przepustowości.
  Raport odkładamy, gdy UART ma za mało miejsca; nie przerywamy
  działania PHY w oczekiwaniu na miejsce.

Przykładowy **format, nie wynik pomiaru**:

```text
A us n=512 cyc=1490 spi=42 grd=110 ce=16 rf=520 ack=72 cpu=730 mk=30 pr=45 bg=90
A det n=512 polls=45 ps=90 ob=6 fi=6 dpl=45 irq=5 ro=10 src=18 enc=8 mo=4 prf=40 rest=675
```

Wszystkie czasy oprócz `n` są w **mikrosekundach**:

| Pole | Co naprawdę mierzy |
| --- | --- |
| `n` | Liczba pełnych, porównywalnych cykli w średniej |
| `cyc` | Od zakończenia poprzedniej wymiany do zakończenia bieżącej |
| `spi` | Zapis bieżącego TX FIFO przez SPI, łącznie z wywołaniem sterownika |
| `grd` | Koniec zapisu FIFO → CE HIGH; zawiera faktyczne czekanie na guard i koszty scheduler-a |
| `ce` | CE HIGH → CE LOW; odmierzony impuls i narzut obsługi pętli |
| `rf` | CE LOW → odczyt terminalnego `TX_DS`/`MAX_RT` przez SPI; **nie jest czystym czasem w eterze** |
| `ack` | Odczyt `OBSERVE_TX`/FIFO/ACK Payload i skasowanie IRQ przez SPI |
| `cpu` | Reszta okresu: `cyc - spi - grd - ce - rf - ack`; czas poza wymienionymi etapami, a nie całkowity czas wykonywania instrukcji CPU |
| `mk` | Zmierzony koszt złożenia ramki (`makePacket`) bieżącej wymiany; podzbiór `cpu` |
| `pr` | Obsługa ACK i protokołu poprzedniej wymiany. W wariancie z odroczonym ACK część tej pracy odbywa się podczas `rf`; nie jest już w całości podzbiorem `cpu` |
| `bg` | Praca generatora/recovery/diagnostyki podczas ON_AIR; **nakłada się na `rf`**, nie dodawać do sumy |

Zależność interpretacyjna: `cyc ≈ spi + grd + ce + rf + ack + cpu`.
`bg` nie jest dodatkowym składnikiem sumy; status nRF może wystąpić
w czasie obsługi tła, a polling wykryje go dopiero po jej zakończeniu.
To pozwala znaleźć koszt czekania i obsługi bez przypisywania czasu
samej modulacji całej fazie `rf`.

Druga linia `A det` pochodzi **z tego samego zestawu ramek** co
poprzedzająca ją `A us`, ale jest wypisywana odrobinę później,
gdy UART zwolni miejsce. `polls` jest liczbą odpytań statusu na
wymianę; wszystkie pozostałe pola podane są w µs:

| Pole | Znaczenie |
| --- | --- |
| `ps` | Łączny czas transakcji SPI odczytujących `STATUS` podczas ON_AIR — **zawiera się w `rf`** |
| `ob` | Odczyt `OBSERVE_TX` po terminalnym IRQ |
| `fi` | Odczyt `FIFO_STATUS` |
| `dpl` | Pobranie sprzętowej długości DPL i całego ACK Payload |
| `irq` | Kasowanie IRQ oraz kod między odczytem payloadu a kasowaniem |
| `ro` | Reszta `ack` po odjęciu `ob+fi+dpl+irq` (pomocnicze instrukcje/odczyty zegara) |
| `src` | Praca źródła i okna transmisyjnego wewnątrz `stream.make` przed zakodowaniem |
| `enc` | `link.makeStream` — kodowanie nagłówka i kopiowanie BODY |
| `mo` | Reszta `mk` po odjęciu `src+enc` |
| `prf` | Część `pr` wykonana, kiedy **następna ramka jest już ON_AIR**. Podzbiór `pr` i `rf`, nie dodawać do okresu wymiany |
| `rest` | Reszta `cpu` po odjęciu `mk+(pr-prf)`; nie musi oznaczać bezczynności |

`ps` i `bg` są dwoma rozłącznymi rodzajami pracy procesora w czasie
`rf`, ale **nakładają się na transmisję w eterze**. Nie dodawać ich
do `cyc` ani nie utożsamiać `rf-ps-bg` z czystym czasem radiowym.
W wersji z odroczoną odpowiedzią również `prf` nakłada się na `rf`;
`prf` i `bg` obejmują różne czynności wykonywane kolejno. Pole `prf`
jest nowe, starsze raporty go nie zawierały.
Nowa sonda wywołuje `Timer1Clock::now()` częściej niż stara i sama
może wydłużyć cykl. Porównanie z wcześniejszym testem wymaga zatem
uwzględnienia jej narzutu.

## Próba na urządzeniach

1. Zbuduj F7 w Microchip Studio, ręcznie F12 wgraj ten sam HEX na A i B.
   Otwórz RealTerm A i B: 115200, 8N1. Uruchom najpierw B, potem A.
2. Po uzgodnieniu wspólnego `sid`, `up=1`, przy `ret=0`, `bad=0`,
   zanotuj z A co najmniej **pięć kolejnych par** `A us` + `A det` i
   odpowiadające `A cps` oraz `B cps`. Prześlij zrzuty A+B.
3. Resetuj tylko B, następnie osobno tylko A. Po ponownym uzgodnieniu
   SID sprawdź, czy raporty `A us` wróciły i czy `cps` jest stabilne.
   Nowy potok nie może pozostawić utrzymującego się `ret/bad`,
   zablokowanego okna ani błędnego SID po pojedynczym resecie.
4. Powtórz pod osłabieniem RF metalizowanymi torebkami, ale
   **nie uśredniaj** wierszy z retry/resetem razem z dobrym łączem.
5. Dla porównania zakomentowanie sondy zastąp przełączeniem
   `PP_PTX_TIMING_PROBE_ENABLED` na `false`, pozostawiając 2 Mb/s.
   Zmierz różnicę CPS: sonda sama wykonuje dodatkowe odczyty Timer1.

Przed tą optymalizacją użytkownik zmierzył na dwóch RF-Nano stabilne
**16 704 B/s** i `cyc≈1437`, `spi≈116`, `rf≈598`, `ack≈230`,
`pr≈134` µs (SID 44831, `ret=bad=0`). Jest to punkt porównawczy,
**nie wynik nowej wersji**.

Testy hostowe i kompilacja AVR weryfikują kod, ale wynik z dwóch
fizycznych urządzeń rozstrzygnie o realnym zysku lub regresji.
