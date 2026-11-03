# TODO — GPT_NRF24

**Etap: INITIAL**  
**Aktualny kamień milowy: M1 — dwukierunkowa komunikacja radiowa**

## Zadania

- [x] Spisać projekt formatu binarnego v1, parsera, telemetrii, retransmisji i wektorów testowych (`protocol-v1.md`); bez implementacji i testów sprzętowych.

- [ ] Ustalić architekturę komunikacji dwóch równorzędnych urządzeń.
- [x] Dodać nieblokujący sterownik USART0 z kolejkami RX/TX i programem echo; kompilacja AVR oraz test sprzętowy pozostają do potwierdzenia.
- [ ] Przetestować UART 115200 8N1 na obu RF-Nano: pojedyncze bajty, dane binarne, długie serie, przepełnienia i błędy USART.
- [x] Dodać i uruchomić na obu fizycznych RF-Nano obsługę SPI oraz diagnostykę rejestrów nRF24L01+; potwierdzenie COM10/COM11 w RealTerm, 2026-11-03.
- [x] Zweryfikować inicjalizację SPI/nRF na obu RF-Nano: oba raportują `NRF24 OK S=0E CFG=0C CH=4C RF=06 FIFO=11`, a procedura OK obejmuje readback rejestrów.
- [ ] Dodatkowa walidacja: build obu konfiguracji, obsługa odłączenia/awarii SPI i przypadki graniczne; brak jeszcze testu transmisji RF.
- [x] Przygotować firmware i sterownik PHY dla surowych 32-bajtowych ramek A→B i B→A, z auto-ACK, MAX_RT, ARC_CNT i RX_DR; testy modelowe przeszły.
- [ ] Zweryfikować na obu fizycznych RF-Nano przesyłanie RAW32 A→B→A, przydział roli ADC0, powtarzalność ACK i poprawność flag RX_DR/ARC_CNT.
- [ ] Przetestować brak partnera, ponowny start i powrót do transmisji po MAX_RT bez ręcznego resetowania drugiego radia.
- [ ] Zaimplementować przesyłanie danych UART ↔ radio.
- [ ] Zapewnić komunikację w obu kierunkach przy identycznym firmware.
- [ ] Utrzymywać naprzemienną wymianę 32-bajtowych ramek również bez danych UART.
- [ ] Wprowadzić wiarygodne, oddzielne statystyki jakości A→B i B→A, z rozróżnieniem strat i retransmisji.
- [ ] Współdzielić miejsce w ramce pomiędzy telemetrią i danymi użytkownika bez blokowania pomiarów.
- [ ] Zaimplementować automatyczne wykrywanie partnera.
- [ ] Obsłużyć jednoczesne próby nadawania bez trwałego zakleszczenia.
- [ ] Obsłużyć utratę i odzyskanie połączenia.
- [ ] Zweryfikować rezultat na dwóch fizycznych płytkach.

## Warunek zakończenia M1

Tekst wpisany do dowolnej z dwóch konsol pojawia się na drugiej. Obie strony mogą niezależnie inicjować transmisję, także podczas nakładających się prób nadawania. Urządzenia odnajdują się i odzyskują łączność po rozłączeniu.

**Status: M1 nieukończony.**

Szczegóły techniczne należą do kodu, testów i ewentualnej dokumentacji konkretnych warstw, a nie do tej listy.
