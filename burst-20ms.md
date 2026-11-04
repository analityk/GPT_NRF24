# BURST20 — drugi profil radiowy (projekt i przenośny harmonogram)

Status 2026-11-04: **nowy profil projektowany, NIE jest jeszcze aktywnym
firmware nRF24**. Obecny, fizycznie pokazany protokół **PP (Ping-Pong)**
jest zachowany na osobnej gałęzi checkpointowej. Aktualny main nadal
uruchamia PP na obu płytkach. Dwa protokoły mają ostatecznie współistnieć
w jednym HEX, wybierane strap pinem przy starcie, bez runtime przełączania.

## Cele i twarde ograniczenia

1. Każda superramka trwa dokładnie **20 ms** według zegara lidera A.
2. Jednostka przydziału pasma: **5 ms**. Dopuszczalne warianty: A/B =
   **5/15**, **10/10**, **15/5 ms**. A nadaje jako pierwszy; potem B.
3. W KAŻDYM slocie (także 5ms) zarezerwowany jest czas na **ostatnią
   ramkę REPORT 32B RF** i guard przed przełączeniem kierunku.
4. DATA wysyłane strumieniowo i tylko do deadline; nie ma obowiązku
   wysyłania z góry określonej liczby ramek na 5ms. Brak nowego DATA
   nie wydłuża slotu. Wszelkie zaległości są przenoszone na następny slot.
5. Podstawowy profil burst: pojedynczy kierunek PTX→PRX w danym slocie;
   dane mogą iść **NO_ACK** z CRC16 sprzętowym; raporty blokowe wyrażają
   straty, nie obiecują automatycznej retransmisji całego bloku. Tryb
   NO_ACK i jego bezpieczne przełączanie ról **wymagają nowego PHY**,
   nie wolno próbować używać obecnego `NrfAckPayloadPhy` jako zamiennika.
6. Przekazywanie BODY danych użytkownika bez interpretacji na PHY/MAC
   (wspólny wzorzec 8B MAC + do24B danych). PROFILE_ID jest obowiązkowy
   w handshake; BURST nie może fałszywie przyjąć ramki PP. Stałe ID pary
   jest odrębne od zmiennego SID sesji.
7. Zero dynamicznej alokacji i brak bufora 32×32B na ATmega328P:
   `DATA` z UART jest odczytywane/wypychane strumieniowo, CRC i licznik
   aktualizowane przy kolejnych pakietach, a odbiorca ma bitmapę
   odebranych numerów. Jeśli odbiorca nie może przyjąć DATA do UART TX,
   zgłasza overflow w następnym raporcie. Nie deklarujemy naprawienia
   strat bez przechowania danych źródłowych.

## Fizyka — dlaczego 32+1 w 5ms nie działa

Zgodnie z nRF24L01+ Product Specification v1.0, 32-bajtowy pakiet ESB
przy 5-bajtowym adresie i CRC16 zajmuje w eterze:
1B preamble + 5B address + 9b PCF + 32B payload + 2B CRC =
**329 bitów**, czyli **329 µs przy 1 Mb/s**. Przy 2 Mb/s jest to
**164,5 µs**. Samo 33×329µs = **10,857ms** przy 1Mbps, a
33×164,5µs = **5,4285ms** przy 2Mbps, bez czasu startu,
sterowania SPI, przełączania PTX/PRX i oczekiwania na potwierdzenia.
**33 ramek 32B w 5ms nie da się nadać nawet przy 2Mbps.**

Absolutna granica na samej fali przy 1Mbps wynosi
floor(5000/329)=**15 pełnych ramek**, razem z raportem.
Realne minimum czasu obejmuje ~130µs Standby→TX/RX, ~33µs
uploadu 32B przez SPI8MHz, obsługę flag, RX FIFO (3 miejsca),
ewentualne hardware ACK i rezerwację czasu przełączenia kierunku.
Z **wybranym budżetem 1ms REPORT i 0,3ms GUARD** w 5ms
zostaje tylko 3,7ms na DATA. Wstępny szacunek projektowy:
**6–10 łącznych ramek 32B w 5ms** przy 1Mbps zależnie od
PHY/ACK/MCU; **nie jest to wynik pomiaru**. Próby trzeba wykonać
z fizycznym NO_ACK burst i rejestrować liczniki pakietów.

Źródło: Nordic Semiconductor, nRF24L01+ Product Specification v1.0,
rozdziały 7.3 i 7.7, strony 28 i 42–43:
https://www.sparkfun.com/datasheets/Components/SMD/nRF24L01Pluss_Preliminary_Product_Specification_v1_0.pdf

## Plan superramki i rezerwy czasu

W każdej połowie wyróżniamy DATA, REPORT i GUARD:
- DATA: od początku slotu do **1,3ms przed jego końcem**
- REPORT: następne **1,0ms** — przygotowanie i próba nadania
  pojedynczej ramki kontrolnej; jej opóźnienie/utrata nie może
  przedłużyć slotu
- GUARD: ostatnie **0,3ms** — brak nowych transmisji i zmiana
  PTX/PRX; strażnik należy zweryfikować w pomiarze na nRF

| Przydział | A DATA | A REPORT+GUARD | B DATA | B REPORT+GUARD |
|---|---:|---:|---:|---:|
| A5/B15 | 3,7ms | 1,0+0,3ms | 13,7ms | 1,0+0,3ms |
| A10/B10 | 8,7ms | 1,0+0,3ms | 8,7ms | 1,0+0,3ms |
| A15/B5 | 13,7ms | 1,0+0,3ms | 3,7ms | 1,0+0,3ms |

Ograniczenie na rozpoczęcie ramki: przed końcem fazy DATA/REPORT
musi pozostać **co najmniej jej najgorszy zakładany czas transmisji**.
`Burst20Schedule::Position::canStart()` nie wydaje prawa do
spóźnionego startu. W przypadku problemu PHY ma zatrzymać próbę
na deadline, wyczyścić zaległy TX FIFO w standby, a nie nadać
na początku slotu drugiego urządzenia. Control report nie stanowi
zegara jedynego — timeout lokalny kończy slot nawet przy jego utracie.

**Synchronizacja:** A jest liderem zegara; B synchronizuje
`cycleStart` po prawidłowym komunikacie kontrolnym zawierającym
cykl i podział. Bez synchronizacji B nie nadaje; po zgubieniu
kolejnych raportów schodzi do DISCOVER i oczekuje na nowy beacon.
Timer1 2MHz zapewnia rozdzielczość 0,5µs, nie gwarantuje
dokładności fazy na dwóch płytkach bez regularnej korekty.
Synchronizować **co superramkę (20ms)**, nie raz na minutę.
Dokładny protokół inicjacji i offset timestamp należy zweryfikować
przed włączeniem na fizycznym radiu.

## Ramka DATA i REPORT

Proponowany wspólny 8B nagłówek o semantyce profilu BURST:
- `TX_SEQ`: numer DATA w cyklu, niezmienny dla duplikatu;
- `RX_LAST`: status poprzedniego bloku, nie udawać ACK każdej ramki;
- `LOSS/ARC/RX_DROP/RX_CREDIT`: lokalna telemetria, przydatna
  do oceny utraty i pełnej kolejki;
- `TYPE_FLAGS`: DATA lub CONTROL;
- `SID16`: odseparowanie poprzedniej sesji po resecie.
- BODY DATA: do **24B surowych bajtów**. Jeden DATA to jedna
  sekwencja w lokalnym bloku, a nie kolejna 32B porcja z osobnym
  2B nagłówkiem użytkownika.

REPORT jest **jedną 32B ramką: 8B nagłówka + 24B BODY kontrolnego**.
Proponowany układ 24B BODY (offsety od początku BODY):

| Offset | Dane | Rozmiar |
|---|---|---:|
| 0 | wersja raportu = 1 | 1 |
| 1 | przydział A/B (1, 2 lub 3 kwanty dla A) | 1 |
| 2 | nadawca raportu (A=0, B=1) | 1 |
| 3 | flagi: overflow, RESYNC, report lost, alarm | 1 |
| 4–5 | nr superramki (modulo65536) | 2 |
| 6 | pierwszy TX_SEQ pakietów bloku | 1 |
| 7 | liczba nadanych pakietów DATA | 1 |
| 8–9 | suma bajtów danych bloku | 2 |
| 10–13 | CRC32 nad użytkowymi bajtami wysłanego bloku | 4 |
| 14–15 | nr ostatniego cyklu partnera z raportem RX | 2 |
| 16–19 | bitmapa 32 pozycji pakietów DATA odebranych od partnera | 4 |
| 20 | liczba pakietów partnera przyjętych | 1 |
| 21 | liczba pakietów zgubionych (według raportu poprzedniego bloku) | 1 |
| 22 | kredyt aktualnego odbiorcy w jednostkach 24B | 1 |
| 23 | rezerwa / przyszłe rozszerzenia | 1 |

CRC16 sprzętowy bada każdą ramkę, a CRC32 bloku zapewnia
dodatkowy test integralności **dopiero gdy komplet pakietów
dotarł**. Raport TX_END nadawcy i raport RX partnera są
z natury rozdzielone w czasie: nadawca nie zna aktualnych strat
odbiorcy przed jego odpowiedzią. Bitmapa dotyczy zatem
ostatniego zamkniętego bloku partnera, nie bieżących danych.
Jeśli ginie REPORT, jest to osobno wykrywane; potrzebna będzie
redundancja raportów w kolejnym cyklu.

To format projektowy, **jeszcze nie firmware wire-v1**.
Przed wdrożeniem ustalić CRC32 polynomial / kolejność bajtów,
obsługę zgubionego REPORT, SYNCHRONIZATION i retry.

## SRAM i UART

Nie rezerwować 32×32 B = 1024B na AVR, jeśli profil ma służyć
wyłącznie do przesyłania strumieniowego i późniejszej diagnostyki.
Po stronie nadawczej wystarcza ramka na 32B, zliczany CRC32 i
ewentualna ograniczona liczba retransmisji; po stronie odbiorcy
ramka na 32B, 4B bitmapy, liczniki i UART ring. **Nie ma danych
do retransmisji po fakcie**, jeśli były już usunięte z UART RX.
W BULK niezawodność na poziomie całego bloku wymaga świadomie
utrzymywanego bufora lub retransmisji przez warstwę wyższą.

UART 115200 8N1 dostarcza do ~11520B/s do każdego węzła.
Przez 15ms bez prawa TX źródło może wytworzyć ~173B,
więcej niż obecne **127B w UART RX**. Ustawienie 5/15 nie
jest automatycznie bezstratne na ATmedze przy ciągłym,
niekontrolowanym transferze. Testy startowe: **10/10**,
potem 5/15 przy ograniczonym wejściu lub po świadomym
rozszerzeniu pojemności RX do min. 256B (plus margines).
Sama zmiana rozmiaru bufora nie gwarantuje obsługi stałego
przeciążenia. W wariancie przyszłego szybszego MCU
liczbę ramek i bufor można zwiększać bez zmiany 20ms modelu.

## Jeden firmware i strap pin

Zachować `A0` dla roli A/B (już sprawdzone sprzętowo).
Zaprojektowano osobny `A1/PC1` z wewnętrznym pull-upem:
- **HIGH/niepodłączony** → PP (dzisiejszy firmware);
- **LOW/GND** → BURST20.
Wybór odczytać i zatrzasnąć **tylko po resecie**. Obie
płytki muszą wybrać ten sam profil, inaczej mają nie
rozpoczynać transmisji użytkownika i zgłaszać niespójność
w negocjacji. Przenośna polityka w `profile_select.h`,
testowalna z mockiem GPIO/Delay. Jest to **przygotowanie**:
obecny `main.cpp` NIE używa jeszcze A1 i NIE uruchamia
niedokończonego profilu BURST20. Cel: wspólne wejście,
`ProfileRunner<PP>` / `ProfileRunner<BURST20>`,
a nie dwie alternatywne kopie globalnych sterowników.

## Co jest już dostępne

- `GPT_NRF2401/burst20_schedule.h`: czysto logiczny, deterministyczny
  podział 20ms, faza DATA/REPORT/GUARD, deadline, wrap licznika
  Timer1, wybór A5/10/15.
- `GPT_NRF2401/profile_select.h`: boot-latch pinu z pull-up i dwoma
  pomiarami, fail-safe PP w razie niespójnego odczytu.
- `tests/burst20_schedule_test.cpp`,
  `tests/profile_select_test.cpp`: testy hostowe C++11.
- PP snapshot: gałąź `checkpoint/pp-2026-11-04`
  przy commit `784fd83dc1000c09af88c6ff0a92c5469e1dce59`.
- `GPT_NRF2401/burst20_report.h`: testowalny C++11 codec 24B
  końcowego REPORT z walidacją, bieżącym CRC32/ISO-HDLC,
  32-bitowym bitmapowym potwierdzeniem innego bloku. Test znanego
  CRC32(`123456789`)=0xCBF43926 oraz pełne pakowanie
  `PairedFrame` 8+24B w `tests/burst20_report_test.cpp`.
- **NIE ma jeszcze**: Burst PHY (przełączanie PRX/PTX/NO_ACK),
  nadawania i odbioru BURST DATA/REPORT na prawdziwym nRF,
  synchronizacji zegarów, obsługi UART w BURST ani dispatchu
  A1 w `main.cpp`.

**Zatwierdzona semantyka kodeka, nie uruchomionej transmisji:** kod zwraca `false` na złe VERSION, brak 24B, niepoprawny udział 5/10/15, niewłaściwe liczniki/owner i niezerowy bajt rezerwy, bez mutacji obiektu wyjściowego. CRC32 liczony przyrostowo, w endianie LE na drucie; bitmapa idempotentna po duplikacie DATA, 32 pozycje. Rzeczywiste raporty będą wymagały powiązania z radio `TX_SEQ` i cyklem sesji.
