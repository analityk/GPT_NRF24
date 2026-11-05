# AGENTS.md — zasady pracy nad GPT_NRF24

## Źródło prawdy i przejęcie projektu

Repozytorium Git jest trwałym źródłem kodu, decyzji i stanu projektu. Przed rozpoczęciem prac:

1. Przeczytaj w całości `migracja.md` i ten plik.
2. Sprawdź bieżącą gałąź, HEAD, ostatnie commity i stan plików (`git status`).
3. Przeczytaj `goal.md`, `plan.md`, `todo.md` oraz ostatnie wpisy `history.md`.
4. Porównaj dokumentację z faktycznym kodem i wynikami testów. W razie rozbieżności ujawnij ją.
5. Wybierz najbliższe zadanie z `todo.md`, chyba że właściciel projektu określił inny priorytet.

Instrukcje muszą być samowystarczalne. Fakty wymagające zachowania należy zapisywać w repozytorium, wraz z uzasadnieniem i dowodami ich sprawdzenia.

## Stałe decyzje projektowe

- **Środowisko:** wyłącznie Microchip Studio / Atmel Studio, AVR-GCC i C++ na ATmega328P. Nie proponuj migracji do innego IDE.
- **Pamięć:** bez `new`, `delete`, `malloc`, `free` i kontenerów wymagających alokacji dynamicznej. Obiekty mają być jawnie instancjonowane, a bufory mieć stałą pojemność i czas życia. Uwzględniaj ograniczenia SRAM.
- **GPIO:** do sterowania pojedynczymi pinami używaj klasy `GPIO` z `GPT_NRF2401/gpio.h`; SPI/USART obsługują własne peryferia. Uwzględniaj atomowość przy współdzieleniu portu z ISR.
- **Dwie płytki, jeden firmware:** nie uzależniaj roli urządzenia od portu COM ani jego podłączenia do komputera.
- **Rozdział odpowiedzialności:** obsługa sprzętu, transport/protokół radiowy, sterowanie łączem i logika konsoli powinny mieć oddzielne, testowalne granice.
- **Wielokrotne użycie:** każdy problem naprawiaj we właściwej warstwie, poprzez mechanizm nadający się do ponownego użycia. Rozwiązanie nie może zależeć od nazwy pliku, katalogu, projektu czy przypadku testowego.
- **Integracja:** zachowuj istniejący projekt Microchip Studio i działający mechanizm programowania przez USB. Nie nadpisuj ścieżek narzędzi ani ustawień portów bez potrzeby i zgody.
- **Zależności:** żadnych nowych frameworków lub bibliotek bez rzeczywistej potrzeby, uzasadnienia kosztu pamięci i zgodności z kompilatorem AVR.
- **Układ danych i ABI:** nie włączać globalnego `Pack Structure Members` / `-fpack-struct` dla C/C++. Formaty binarne radia i pamięci trwałej kodować jawnie bajtami; nie rzutować buforów na spakowane typy C++.

## Bezwzględny zakaz magicznych liczb w kodzie

- **Właściciel zabrania stosowania magicznych liczb.** Nie umieszczaj
  nienazwanych literałów liczbowych określających działanie programu
  w kodzie produkcyjnym ani w testach: obejmuje to liczby dziesiętne,
  szesnastkowe, maski bitowe, rozmiary ramek i buforów, indeksy pól,
  adresy rejestrów, wartości konfiguracyjne, czasy, timeouty, częstotliwości
  oraz limity ponowień. **Komentarz objaśniający liczbę nie jest wyjątkiem**
  i nie zastępuje nazwy.
- **Preferowany zapis: `#define NAZWA_PARAMETRU 123`.** Stosuj jednoznaczne,
  opisowe nazwy z prefiksem odpowiedzialnej warstwy i jednostką tam,
  gdzie to istotne, np. `#define NRF24_CE_PULSE_US 15U`,
  `#define NRF24_TRANSFER_TIMEOUT_TICKS 16000UL` lub
  `#define NRF24_IRQ_TERMINAL_MASK 0x30U`. Definicja ma znajdować się
  we właściwym module albo współdzielonym nagłówku — jedno źródło prawdy,
  bez kopiowania tych samych wartości w kilku miejscach.
- Alternatywą jest **nazwana stała** (`const` / `static constexpr`) tylko
  wtedy, gdy wartość rzeczywiście ma charakter stałej, a jawny typ lub
  zasięg C++ przynosi korzyść. Parametr zmienny lub konfigurowalny
  powinien mieć nazwany punkt konfiguracji, a nie ukryty literał.
- Przed ukończeniem każdej zmiany programistycznej sprawdź cały
  modyfikowany fragment i usuń z niego magiczne liczby. Nazwane definicje
  muszą pozwalać zrozumieć kod bez szukania komentarza lub noty
  katalogowej. Neutralne operacje języka, takie jak wyzerowanie licznika
  czy zwiększenie go o jeden, nie są parametrami sprzętu ani protokołu.

## Obowiązkowe formatowanie C/C++

- Wcięcie jest **prawdziwym znakiem tabulacji (TAB)**, wyświetlanym
  z szerokością **4 kolumn**. Nie zastępuj tabulatora czterema spacjami.
- Każda definicja funkcji ma ciało w osobnych wierszach, również gdy
  zawiera tylko jedną instrukcję. Nawias `{` należy do wiersza
  deklaracji, zamykający `}` jest w osobnym wierszu. Po każdej
  definicji funkcji zostaw pusty wiersz.
- Każda instrukcja sterująca (`if`, `else`, `for`, `while`, `switch`)
  ma jawny blok `{ ... }`, także gdy wykonuje tylko jedną instrukcję.
  Zakazane są konstrukcje `if(warunek)instrukcja;`, inne jednolinijkowe
  ciała sterujące i dopisywanie średnika po zamykającym `}` bloku `if`.
- Nie łącz kilku instrukcji w jednym wierszu. Przy edycji istniejącej
  funkcji uporządkuj **całe jej ciało** według tych zasad; nie zmieniaj
  automatycznie całego repozytorium bez odrębnego zadania.

Wymagany wzorzec (wcięcie przed `instrukcja();` jest znakiem TAB):

```cpp
void funkcja(uint8_t argument){
	instrukcja();
}

if(!radio.enableAckPayload(NAZWANA_KONFIGURACJA)){
	fatal();
}
```

## Fizyka łącza — sedno projektu

nRF24L01+ jest radiem **półdupleksowym**. Jednoczesne użytkowanie dwóch kierunków komunikacji ma zapewniać **logiczny full duplex**, realizowany przez sterowanie nadawaniem i odbiorem, koordynację transmisji i obsługę kolizji. Nie należy przedstawiać tego jako fizycznego jednoczesnego TX/RX.

W aktualnym etapie **jedyny aktywny format** to sparowany rdzeń 8 B nagłówka + 0–24 B nieinterpretowanego BODY, DPL 8–32 B. A inicjuje transmisje jako PTX, B pozostaje PRX, a zwrotne dane podróżują w hardware ACK Payload. Jakość obu kierunków mierzyć niezależnie; odróżniać luki sekwencji, retransmisje sprzętowe, błędy ACK i odrzucenia danych aplikacyjnych. Po utracie łącza protokół ma docelowo sam się resynchronizować. **SESSION_ID jest teraz uzgadniany przez osobny handshake z nonce dwóch uruchomień** (zob. `paired_session.h`), ale 16-bitowy CRC nie daje kryptograficznej identyfikacji. Od 2026-11-04 aktywny `UartReliableStream` podaje realne `RX_CREDIT` z wolnego miejsca UART TX i niezależny aplikacyjny ACK; bez RTS/CTS przeciążenie wejścia USART RX nadal może zgubić dane. Negocjacja sesji i automatyczny powrót po **pojedynczym resecie A oraz pojedynczym resecie B** zostały potwierdzone na dwóch fizycznych RF-Nano (2026-11-04); reset jednoczesny obu urządzeń i długotrwała utrata zasięgu pozostają do sprawdzenia.

Należy rozdzielać potwierdzenie sprzętowego odbioru ramki od faktycznego przekazania jej danych do aplikacji. Nie deklaruj bezstratności, odporności na kolizje czy czasu dostarczenia, jeśli nie potwierdzają tego testy.

## Bieżąca struktura firmware

W `main` utrzymujemy jeden program `GPT_NRF2401/main.cpp`. Kodek (`paired_frame.h`), stan łącza (`paired_link.h`), handshake (`paired_session.h`), opcjonalny niezależny transport UART (`uart_reliable_stream.h`), trwały dziennik EEPROM (`epoch_journal.h` + adapter `boot_epoch.cpp`) i sprzętowe PHY (`nrf24_ack_phy.h`) są oddzielne. Starsze eksperymenty znajdują się wyłącznie w archiwalnej gałęzi `archive/pre-paired-core-cleanup-2026-11-03`. Nowe zadania rozwijać w aktywnych, testowalnych warstwach, nie przywracać rozgałęzionej kompilacji `0/1/2` ani historycznych artefaktów bez wyraźnej potrzeby.

## Reguły implementacji i weryfikacji

1. Przed pisaniem kodu ustal warstwę, do której należy problem, wymagania wejścia/wyjścia i przypadki brzegowe.
2. Projektuj bufory, stan protokołu, obsługę błędów i ponowienia świadomie; nie ignoruj przepełnień ani utraconych połączeń.
3. Utrzymuj testowalną logikę protokołu niezależnie od konkretnego MCU, o ile jest to możliwe bez nadmiernej komplikacji.
4. Nie traktuj poprawnej kompilacji, programowania Flash, testu jednostkowego i testu dwóch fizycznych urządzeń jako równoważnych dowodów.
5. Nie nadpisuj niezwiązanych zmian użytkownika ani nie wykonuj destrukcyjnych operacji Git.
6. Po zakończeniu pracy aktualizuj `todo.md`, `history.md` i, gdy zmienia się punkt kontynuacji, `migracja.md`.

## Polityka gałęzi Git — obowiązkowa zgoda właściciela

- **Uzgodniony układ od 2026-11-05:** `main` przechowuje pierwszą
  zaakceptowaną sprzętowo wersję PP z automatycznym odzyskiwaniem;
  istniejąca odtąd gałąź o dokładnej nazwie **`testy`** służy do
  kolejnych eksperymentów i optymalizacji. Dalsze iteracje prowadź
  na `testy`, a przeniesienie ich do `main` wymaga ponownej decyzji
  właściciela po testach. Wcześniejsze branche zachowaj bez usuwania.
- **Jedno zadanie, jedna gałąź robocza.** Kolejne diagnozy, poprawki,
  nieudane próby i testy tego samego problemu wykonuj na tej samej
  gałęzi. Do zachowania historii i powrotu do poprzedniej wersji służą
  commity; nie zakładaj nowego brancha przy każdej iteracji.
- **Przed utworzeniem jakiejkolwiek nowej gałęzi najpierw zapytaj
  właściciela o sens jej tworzenia ORAZ o jej nazwę.** Przedstaw
  konkretny powód wydzielenia gałęzi i możliwość pozostania na
  obecnej. Poczekaj na jednoznaczną zgodę zarówno na utworzenie,
  jak i na uzgodnioną nazwę. Nie zgaduj nazwy i nie traktuj ogólnego
  pozwolenia na edycję repozytorium jako zgody na nowe gałęzie.
- Jeżeli właściciel wskazał gałąź, kontynuuj na niej. Jeśli nie wiadomo,
  która gałąź jest właściwa, ustal to z właścicielem przed zapisem zmian;
  nie mnoż eksperymentów dla własnej wygody.
- Nie pozostawiaj kolejnych porzuconych gałęzi eksperymentalnych jako
  domyślnego sposobu pracy. Scalenie, usunięcie lub przeniesienie
  istniejących gałęzi wymaga osobnej, wyraźnej zgody właściciela,
  po sprawdzeniu ich zawartości i możliwości odzyskania historii.

## Odpowiedzialności dokumentów

- `goal.md` — wymagany rezultat i kryteria odbioru.
- `plan.md` — strategia i kamienie milowe, bez nadmiernych szczegółów implementacyjnych.
- `todo.md` — bieżące, wykonalne zadania.
- `history.md` — wyłącznie rzeczywiście dokonane zmiany i potwierdzone wyniki.
- `migracja.md` — samodzielny punkt kontynuacji.
- `README.md` — opis i używanie projektu.

## Aktualna decyzja po teście obu UART — 2026-11-04

Wcześniejsze opisy stop-and-wait i 10ms pollingu są HISTORYCZNE. **Aktualny firmware stale wyzwala nRF DATA↔ACK Payload, także przy pustym UART; `UartReliableStream` ma WINDOW=4 (4×24B/kierunek), a kumulacyjny aplikacyjny ACK znajduje się w `RX_LAST` ośmiobajtowego nagłówka, nigdy w BODY.** Nie wprowadzać arbitralnych interwałów bezczynności ani czekania na ACK każdego segmentu przed wysłaniem następnego. Radio jest nadal półdupleksowe, ACK Payload nadal przygotowywany wcześniej. W DATA pole `TX_SEQ` jest numerem segmentu danych, `RX_LAST` jest potwierdzeniem segmentu zaakceptowanego do sinku. IDLE nie zużywa numeru i niesie ACK; nie utożsamiać tych pól jednocześnie z niezależnymi licznikami wszystkich ramek RF. Go-back-N i retransmisję implementować w warstwie łącza nad opaque 24B BODY, bez naruszania SID, AVR UART pełnodupleksowego, F7/F12 i reguł RAM. Użytkownik nie wymaga oszczędzania energii podczas idle. Niekontrolowany CH340 UART przy szybkości wyższej niż skuteczna zdolność PHY+CPU nadal wymaga wejściowego flow control; window=4 nie jest gwarancją przeciw overflow.

## Zapis dwóch profili — 2026-11-04

Obecny, fizycznie pokazany profil to oficjalnie **PP (Ping-Pong)**. Nie zamieniać jego ciągłego DATA↔ACK Payload w nowy tryb bez osobnego wyboru profilu. Niezmieniony punkt przywrócenia: `checkpoint/pp-2026-11-04` @ `784fd83`. Właściciel zamówił drugi profil **BURST20** z kwantami 5ms w stałej superramce20ms; aktualnie są tylko portable `burst20_schedule.h` i `profile_select.h`, brak rzeczywistego Burst PHY/runner, co ma pozostać jawne. Docelowy strap: **A1/PC1 internal pullup HIGH=PP, GND=BURST20**, A0 nadal rolą A/B. Jedyny firmware ma mieć PP i BURST po wykonaniu pełnej implementacji; do tego czasu `main.cpp` pozostaje uruchamiający PP. 32 ramki32B plus raport w5ms są fizycznie niemożliwe przy1Mbps. Przed pomiarem posługiwać się wyłącznie górnymi granicami i wyraźnie oznaczać estimates. Szczegóły w `burst-20ms.md`. 

## Aktywna kolejka UART RX na `testy` — 2026-11-05

- W `PP_TRAFFIC_MODE_UART_BRIDGE` przerwanie RX USART i `UartReliableStream` współdzielą **jeden** `RetainedByteRing` (256 fizycznych bajtów, 255 użytecznych). Nie wolno wprowadzać ponownie osobnych kopii `slots_[4][24]` ani tymczasowego BODY.
- Cursor `write` należy do ISR, `prepared` i `acknowledged` do protokołu; 8-bitowe różnice pozycji są atomowe na AVR. Cztery deskryptory (start/długość) dotyczą okna DATA. Po przyjęciu aplikacyjnego `RX_LAST` przesuwany jest `acknowledged`, **bez zerowania danych**.
- Retransmisja odczytuje ten sam obszar pamięci, w tym po zawinięciu indeksu 255→0. Serializacja BODY czyta pierścień bez dodatkowej tablicy. Brak danych nie wstrzymuje 8-bajtowych ramek IDLE.
- `pauseHostInput` ma histerezę: ≤48 B wolnych ustawia PAUSE, ≥96 B zwalnia PAUSE. RealTerm/CH340 nie respektuje tego automatycznie; nie umieszczać bajtów statusowych w binarnym strumieniu i nie deklarować bezstratności bez flow control przy arbitralnym przeciążeniu.
- Przed akceptacją zmian uruchomić `tests/run_host_tests.sh`, oba warianty `tests/build_avr.sh` oraz sprawdzić `pp_uart_burst_retained_test`, `uart_unthrottled_ingress_test` (model **współdzielonego** RX), regresje rekey/replay i bilans SRAM.

## Tymczasowy pomiar okresu transmisji UART/PP — 2026-11-05

- `testy` włączono do diagnostyki przy `PP_UART_DIAG_ENABLED=1`: tekst statusowy na UART jest emitowany **tylko po ≥0,8 s braku postępu** i jest **świadomym odstępstwem** od normalnej binarnej transparentności.
- Jedynym mierzonym czasem jest 16-bitowy odstęp PTX start→start dla poprawnej wymiany z DATA w przynajmniej jednym kierunku, bez ARC; długości DPL są zmienne. Nie przywracać wielofazowej sondy w tym przebiegu. Kumulowanie próbek może używać 32 bitów, ale odczyty czasu na granicy cyklu są tylko 16-bitowe.
- Liczniki `UART::Counters::rx_bytes/tx_bytes` istnieją wyłącznie w wariancie diagnostycznym; statystyki UART i PP są próbkowane co 10ms, nie w każdej ISR i funkcji protokołu.
- Wariant `PP_UART_DIAG_ENABLED=0` musi dalej kompilować się jako zwykły binary bridge, bez dołączania tekstu, i być wymagany do prób byte-for-byte.
- `pp-uart-cycle-diagnostic.md` to bieżąca procedura sprawdzenia granicy strat przy 128B+CRLF i `Delays=0,10` oraz `0,20`; brak JTAG/SWD jest celowy, do odczytu wystarczają oba RealTerm.
