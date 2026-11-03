# Projekt minimalnej ramki P2P po sprzętowym ACK Payload — 2026-11-03

> **Decyzja właściciela z 2026-11-03 (aktualna): 8 bajtów metadanych + do 24 bajtów danych użytkownika.** Poprzednia koncepcja 4+28 opisana poniżej pozostaje historycznym wariantem porównawczym i **nie jest wyborem docelowym**. Zmierzone 540 pełnych wymian/s daje arytmetyczne 12 960 B/s użytecznego BODY na kierunek dla 24 B, jeszcze bez transportu i rzeczywistych kosztów CPU. Przejście do 4+28 zwiększałoby teoretyczny limit o 2160 B/s, ale odbierałoby połowę miejsca na diagnostykę i kontrolę przepływu. Obecny UART 115200 8N1 ma limit 11 520 B/s na kierunek.

> **Kontrakt warstw:** MAC/PHY transportuje nieinterpretowaną treść BODY oraz utrzymuje stan łącza. Nie wie, czy BODY zawiera komendy, dane pliku, rekordy transportu TCP-like, szyfrogram lub inny protokół. Tylko opcjonalne ramki CONTROL są interpretowane przez warstwę sterowania. Nie implementować teraz routingu, formatu pliku, szyfrowania ani niezawodnego transportu end-to-end; zachować możliwość ich późniejszego dołożenia jako usług ponad BODY.

> **Nie utożsamiać 8 B miejsca z 8 B już użytecznych pól diagnostycznych.** Aktualne `frame_v1.h` ma 8 B nagłówka, lecz zajmuje je m.in. SRC, DST, SID i EP; przed budową nowego STREAM trzeba jawnie rozstrzygnąć, które pola są zbędne po sparowaniu, a które służą jakości łącza. Wymaga to wersjonowania/negocjacji, nie reinterpretacji ramek starej sesji bez uzgodnienia.

**Status:** propozycja formatu do przetestowania; NIE zmienia istniejącego `protocol-v1.md`, nie jest bieżącym formatem benchmarku ani gotowym firmware. Punktem wyjścia jest fizycznie potwierdzony eksperyment pełnych 32 B DATA + 32 B ACK Payload przy 540 transakcjach/s i 0 widocznych błędów w zgłoszonym wycinku.

## Założenia

- Wyłącznie dwa już sparowane urządzenia, jeden aktywny fizyczny PTX i jeden PRX. Kierunek A→B: standardowy DATA PTX; kierunek B→A: sprzętowy ACK Payload PRX. W przyszłości możliwe uzgodnione odwrócenie ról.
- nRF24L01+ zapewnia adresowanie radiowe, CRC16, auto-ACK i próby retransmisji. ATmega zarządza aplikacyjnymi sekwencjami, przyjęciem do kolejki, telemetrią, przepełnieniami i ponowieniem na wyższej warstwie.
- **Maksymalnie 4 B nagłówka + 28 B danych**, bez rzutowania bufora na struktury packed.
- **Włączyć DPL** (w ACK Payload już włączony): przesyłać od 4 do 32 B, a długość użytkowa = długość sprzętowego payloadu − 4. Dla 0 danych nadawać krótki 4B heartbeat/status zamiast stale 32 B. Pole długości w nagłówku byłoby powieleniem nRF DPL.
- Rozszerzone CONTROL zajmuje część BODY i tylko wtedy niesie więcej metadanych (nawet do 8B lub więcej). Możliwe uzgodnienie profilu P2P podczas istniejącego HELLO/READY v1; stary i nowy format nie mogą być interpretowane jako te same bajty bez negocjacji.

## Próbny układ 4 bajtów

| Offset | Bity | Wartość | Znaczenie |
|---|---|---|---|
| 0 | 7..0 | TX_SEQ | Numer kolejnej **nowej logicznej** ramki lokalnego nadajnika (mod 256); retransmisja tego samego segmentu nie zwiększa numeru |
| 1 | 7..0 | RX_LAST | Ostatni odebrany, poprawnie przyjęty do zakontraktowanego bufora partnera numer ramki; dla RELIABLE: ostatni **ciągły** numer zaakceptowany, nigdy tylko sam `RX_DR` |
| 2 | 3..0 | MISS_AB | Czterobitowy, narastający modulo 16 licznik brakujących segmentów A→B; właścicielem licznika jest B, A tylko powtarza ostatnio otrzymany stan |
| 2 | 7..4 | MISS_BA | Analogiczny licznik brakujących segmentów B→A; właścicielem jest A, B tylko powtarza ostatni stan |
| 3 | 3..0 | RX_CREDIT | Ile kolejnych pełnych segmentów 28 B odbiornik deklaruje gotowość przyjąć, przy uwzględnieniu ramek już będących w locie; 0 = wstrzymaj nowe DATA |
| 3 | 5..4 | TYPE | DATA, CONTROL, IDLE, RESERVED |
| 3 | 6 | PRIORITY | Pilny komunikat/żądanie obsługi |
| 3 | 7 | RESYNC | Żądanie resynchronizacji/raportu (nie zastępuje numeru epoki sesji) |

Byte 2 przechowuje **dwa kierunki jakości po 4 bity**. Każda strona ma tylko jeden autorytatywny licznik — dla ramek, które sama odbiera. Pole partnera jest echem, a nie drugim niezależnym pomiarem. Należy utrzymywać lokalne, szersze (np. 16–32-bitowe) liczniki; nibble to bieżąca telemetria. Jeśli między próbkami wypadnie ≥16 ramek, modulo 16 nie pozwala odtworzyć prawdziwej liczby; pełne statystyki zapewnia rzadszy CONTROL/LINK_REPORT.

`MISS` należy precyzyjnie zdefiniować: luka numeracji nie jest tym samym co fizyczna transmisja z przekłamanymi bitami (CRC zatrzymuje ją w radiu), nie dowodzi też trwałej straty, dopóki możliwe jest późniejsze ponowienie przez warstwę aplikacji. W normalnych transmisjach z auto-ACK `ARC_CNT` może rosnąć bez żadnego MISS. Brak jakichkolwiek pakietów nie zwiększa sam z siebie MISS; wymaga timeoutu LINK_DOWN.

## Warunki poprawności do ustalenia przed implementacją

1. **Sesja:** usunięcie pola SID8 z każdej ramki wymaga alternatywnego zabezpieczenia przed starą ramką i starym ACK po resecie. W parze można uzgadniać i zmieniać **40-bitowy adres radiowy dla każdej epoki sesji**, po wcześniejszym HELLO na adresie discovery. Wymaga protokołu bezpiecznej zmiany adresu, flush FIFO, epoch/nonce oraz testów obustronnych resetów. To możliwość do weryfikacji, nie gotowa specyfikacja i nie uwierzytelnienie kryptograficzne. Bez tego rozwiązania potrzebny jest co najmniej SID w nagłówku lub inny identyfikator epoki.
2. **Semantyka ACK:** sprzętowy ACK nie znaczy, że użytkownik otrzymał bajty UART. `RX_LAST` musi mieć jednoznaczny kontrakt (odbiór do kolejki vs przyjęcie przez usługę). W RELIABLE oznacza ostatni ciągły *zaakceptowany*, a brakujące wcześniejsze segmenty wymagają retransmisji i ewentualnie osobnych SACK. Jeśli odbiornik pracuje tylko best-effort, raportowane `RX_LAST` nie jest gwarancją dostarczenia.
3. **ACK Payload jest o jeden krok opóźniony:** PRX musi zapełnić FIFO *przed* otrzymaniem wyzwalającej ramki. Wysłany tam `RX_LAST` oraz `RX_CREDIT` są informacją z poprzedniej transakcji. Przed rozpoczęciem serii trzeba zachować zapas miejsca na dane w locie.
4. **Wrap 8-bit:** różnice modulo 256 są jednoznaczne tylko przy ograniczonym oknie i bez wielkich przerw/restartów. Dla ciągłego transferu liczba 232 segmentów nie może być traktowana jako bezwzględny limit całego pliku; osobna usługa file transfer ma 16/32-bitowy offset, ID transferu i potwierdzenia SACK w CONTROL.
5. **Kredyty a RAM:** obecna ATmega328P ma 2KB SRAM i UART TX z 127 miejscami. Zwykły RX_CREDIT=0..4 segmentów ×28B może odpowiadać fizycznemu miejscu, ale trzeba odjąć miejsce zajęte lub obiecane przez ramki w locie. Przyszła ATmega/MCU z większym SRAM może udostępnić więcej kredytów bez zmiany układu ramki.
6. **Kontrola jakości:** nRF24L01+ nie raportuje BER. Rozdzielać `ARC_CNT`, `MAX_RT`, przerwy numeracji, nieprzyjęte dane aplikacyjne, overflow FIFO/UART. Cztery bity odzwierciedlają tylko bieżącą wskazówkę; szerszy raport okresowy ma podawać prawdziwe sumy.
7. **DPL:** aktualna aplikacja benchmarkowa używa pełnych 32 B; proponowany kompaktowy format wymaga wielokrotnego użytku w warstwie PHY: transmisji 4..32 B i odczytu długości przez `R_RX_PL_WID`. Nie zmieniać obecnie działającego benchmarku bez osobnych testów.

## Cel przepustowości — wyłącznie arytmetyczna projekcja

Przy **tym samym** zmierzonym tempie 540 wymian/s (nie gwarancja po zmianie firmware):
- 28 B danych na ramkę = 15 120 B/s na kierunek.
- 24 B danych na ramkę = 12 960 B/s na kierunek.
- Dla porównania UART 115200 8N1 daje maksymalnie 11 520 B/s na kierunek (10 bitów na bajt).

Surowa wydajność linku przewyższa więc przy obecnym pomiarze przepustowość UART, lecz rzeczywista implementacja musi uwzględnić przepływ, bufory, ARQ, puste ramki i obsługę CPU.

**Następny krok:** przetestować na hoście dekodowanie/enkodowanie kompaktowej ramki, opóźnione ACK Payload i wrap 8/4 bit, a następnie eksperymentalnie porównać full 28 B DATA z krótkimi ramkami DPL i zbiorczym raportem błędów. Nie przepisywać istniejącego v1 bez odrębnej decyzji o wersjonowaniu.

Źródło: Nordic nRF24L01+ Product Specification v1.0, sekcje 7.3.3, 7.4.1, 7.5, 8.3 (dynamiczny payload, ACK Payload, FIFO, SPI).

## Kierunek aktywny: 8+24 B (decyzja 2026-11-03)

Przykładowy, **niezamrożony** podział ośmiu bajtów może pozostawić: TX_SEQ (1 B), RX_LAST (1 B), dwa 4-bitowe wskaźniki braków (wspólnie 1 B), status FIFO/receiver credit (1 B), pole TYPE/FLAGS (1 B), identyfikator epoki lub wymiany (1 B) i 2 B na statystyki bądź przyszłe sterowanie. Konkretne offsety i semantyka byte 5..7 wymagają osobnego, testowalnego schematu. Nie zamieniać automatycznie `RX_LAST` w gwarancję dostarczenia aplikacyjnego.

Dane użytkownika mają być **nieinterpretowane przez rdzeń**. Długość rzeczywiście odebranego radiowego payloadu udostępnia DPL; zwykła ramka DATA może mieć 8..32 B, więc 0..24 B BODY. Ramka z danymi szyfrowanymi kiedyś nadal pozostanie dla rdzenia zwykłymi bajtami. Jeśli wymagana będzie ochrona integralności/uwierzytelnianie end-to-end, narzut na nonce/tag i sposób negocjacji należą do wyższej warstwy, nie do obecnego projektu.

Zachować testowany hardware ACK Payload i tempo 540/s jako **punkt odniesienia**, nie prognozę gwarantowanego end-to-end goodput. Dalsze badania: jakość niezależnych kierunków, rozróżnienie sprzętowych prób od aplikacyjnych braków, korekta po resetach, wersja 8+24 w RAM, zero/kilka/24 B BODY oraz praca z dwoma krótkimi strumieniami. Wszystkie nowe elementy bez alokacji dynamicznej, z testami hostowymi i sprzętowymi.
