# PP bez szablonów — test_c_like, 2026-11-15

Baza: `main` @ `e0580e2`. Przebudowa na zamówionej gałęzi `test_c_like`.
Nie zmieniono gałęzi `main` ani `testy`.

## Odpowiedzialności i pliki

- `uart.cpp`: globalne wektory RX/UDRE i globalne dane USART0. Brak aktywnego
  wskaźnika na instancję UART, wywołań metod i callbacków w przerwaniach.
- `timer1_clock.cpp`: globalny ISR zwiększa globalny licznik przepełnień.
- `spi.cpp`: zwykłe funkcje i pętla przesuwająca bajty, z ograniczeniem liczby
  odczytów statusu. Usunięto `SpiBlockTransfer` i jego specjalizacje.
- `retained_byte_ring.cpp`, `byte_ring.cpp`: operacje na buforach w foreground.
- `uart_reliable_stream.cpp`, `paired_frame.cpp`, `paired_link.cpp`,
  `paired_session.cpp`, `nrf24.cpp`, `nrf24_ack_phy.cpp`: osobne implementacje
  transportu, formatu, łącza, sesji i sprzętu. Nagłówki zawierają deklaracje.
- `pp_application.cpp`: inicjalizacja i instancje stanu aplikacji;
  `pp_runner.cpp`: pętle A/B i integracja protokołu;
  `pp_console_diagnostics.cpp`: opcjonalne raportowanie.
  `main.cpp` wyłącznie wywołuje `ppRun()`.
- `hardware_bindings.h`: jawny wybór konkretnych sterowników. W firmware
  wywołania SPI/GPIO/radia/zegara/UART są bezpośrednie. Testy hostowe używają
  jawnych tablic funkcji zamiast szablonów. Opcjonalne sondy i adapter EEPROM
  również mogą korzystać z callbacków, wyłącznie poza przerwaniami.

Nie ma deklaracji szablonów, alokacji dynamicznej ani metod wirtualnych.
Formatowanie opisuje `.clang-format`: TAB 4, bloki sterujące, osobne instrukcje.
Usunięto zbędne konwersje przy przypisaniu do docelowego typu całkowitego.
Zachowano konieczne konwersje enumów, rozszerzenia przed przesunięciem oraz
zawijanie sekwencji przed porównaniem — AVR ma 16-bitowy `int`.

## Granica ISR i bufora

RX ISR pobiera UCSR0A przed UDR0, odrzuca bajt z błędem sprzętowym, sprawdza
czy następna pozycja jest wolna, zapisuje bajt i publikuje indeks. Rzadkie
ścieżki błędów zwiększają globalne liczniki; osobne liczniki wszystkich
bajtów istnieją tylko w jawnie włączonym wariancie diagnostycznym.
Nie ma w ISR obsługi ACK, liczenia wolnego miejsca, histerezy ani kopiowania
ramek. Pozostawiono sprawdzenie pełności: bez niego nowe dane nadpisałyby
bajty oczekujące na retransmisję. Nie użyto ISR_NAKED ani ręcznego assemblera.

`uart_rx_buffer` jest globalnym buforem 256 B, z 255 użytecznymi komórkami.
ISR zmienia tylko `write_position`; pętla główna zmienia `prepared_position`
i `acknowledged_position`. Ostatni indeks przesuwa się dopiero po APP_ACK.
Zajętość to różnica write−acknowledged modulo 256, a nie porównanie `>`.
Przykład: zapis=10, ACK=250 oznacza 16 zajętych bajtów. Jedna wolna komórka
rozróżnia pełny i pusty bufor, bez współdzielonego licznika read-modify-write.
Nie ma dodatkowej kolejki ani kopii danych retransmisyjnych.

TX ma osobne globalne dane i indeksy. Foreground najpierw zapisuje cały
segment, potem publikuje pozycję zapisu. UDRE ISR tylko pobiera kolejny bajt
lub wyłącza własne przerwanie po opróżnieniu kolejki.

## Weryfikacja

36 testów hostowych PASS, również z `-fsanitize=undefined` i przerwaniem
przy pierwszym naruszeniu. Zachowano scenariusze strat, opóźnionego ACK,
przeciążenia, zawijania sekwencji i rekey. Nowy `uart_isr_test.cpp` wykonuje
rzeczywiste funkcje ISR z `uart.cpp` na atrapach rejestrów: pełny RX nie
nadpisuje niepotwierdzonych danych, wrap, błędy USART, publikacja TX i pusty
UDRE. Test nie symuluje czasu instrukcji ani równoległego działania układu.
Porównywarka binarnych capture wykrywa również dopisany bajt: PASS.

Pięć wariantów AVR PASS (cztery Release i domyślny Debug). Kontrola `.lss` wymaga braku CALL/RCALL/ICALL w ISR,
zbilansowanych PUSH/POP i ograniczonego użycia stosu. Kontrola źródeł wykrywa
powrót szablonów oraz brakujące jednostki kompilacji w projekcie Studio.

Porównanie tego samego AVR-GCC 14.2.0, `-Os`, ATmega328P, bez LTO:

| Domyślny most | main e0580e2 | test_c_like |
|---|---:|---:|
| `.text` | 12724 B | 14342 B |
| `.data + .bss` | 1001 B | 822 B |
| PUSH w RX ISR | 15 | 8 |
| PUSH w UDRE ISR | 15 | 5 |
| PUSH w Timer1 ISR | 15 | 3 |
| Wywołania funkcji w każdym ISR | 1 | 0 |

PUSH obejmuje także zapis SREG; liczby nie oznaczają tylu unikalnych
rejestrów ani całkowitego czasu ISR. Debug `-Og`: RX/TX/Timer1 9/7/5 PUSH,
również bez wywołań funkcji. Flash z inicjalizatorami `.data`:
12820 → 14438 B. RAM zmalał m.in. przez wykluczenie konstrukcji wyłączonych
sond pomiarowych. To statyczny bilans, bez stosu. Diagnostyczny UART zajmuje
1074 B statycznego SRAM, a generator syntetyczny 1313 B.

Pełna implementacja w `.cpp` zwiększa liczbę zwykłych wywołań poza ISR
w kompilacji bez LTO. Nie potwierdzono jeszcze zmiany realnego goodput ani
latencji. Wyniki starego firmware nie są testem tej przebudowy.

## Kompilacja i następny test

Projekt Microchip Studio zawiera wszystkie nowe `.cpp`. Jawnie włączono
sekcje funkcji/danych i usuwanie nieużywanych sekcji podczas linkowania,
zgodnie z kompilacją CI. Ustawienia
programatora, F7 i ręczne F12 pozostają zachowane. Nagłówki i `.cpp` muszą
pochodzić z tej samej rewizji; po przełączeniu wykonać Rebuild Solution.

`sh tests/run_host_tests.sh` uruchamia testy hostowe.
`sh tests/build_avr.sh` kompiluje pięć wariantów. Zmienna `AVR_OUTPUT_DIR`
pozwala zachować ELF, MAP i LSS w wybranym katalogu; bez niej katalog
roboczy jest usuwany. CI udostępnia te pliki jako artefakt `avr-firmware`.
Dotychczasowe pliki w `GPT_NRF2401/Release` są historyczne i nie opisują
nowej przebudowy.

Następny odbiór: ten sam wsad na A i B, binarne capture w obu kierunkach,
krótkie wiadomości, pełne obciążenie, reset osobno A/B. Nadal PP 2 Mb/s,
WINDOW4, 24 B BODY bez bajtu kontrolnego, UART 115200 8N1. Domyślnie brak
raportów po bajcie i po ciszy (`PP_UART_DIAG_ENABLED=0`). Bez RTS/CTS
nieograniczony napływ UART może nadal przepełnić wejście.
