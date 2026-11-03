# TODO — GPT_NRF24

**Etap: INITIAL**  
**Aktualny kamień milowy: M1 — dwukierunkowa komunikacja radiowa**

## Zadania

- [x] Spisać projekt formatu binarnego v1, parsera, telemetrii, retransmisji i wektorów testowych (`protocol-v1.md`); bez implementacji i testów sprzętowych.

- [ ] Ustalić architekturę komunikacji dwóch równorzędnych urządzeń.
- [x] Dodać nieblokujący sterownik USART0 z kolejkami RX/TX i programem echo; kompilacja AVR oraz test sprzętowy pozostają do potwierdzenia.
- [ ] Przetestować UART 115200 8N1 na obu RF-Nano: pojedyncze bajty, dane binarne, długie serie, przepełnienia i błędy USART.
- [x] Dodać rejestrową obsługę SPI i nRF24L01+, bezpieczne inicjowanie POWER_DOWN i diagnostykę przez UART (oczekuje na weryfikację F7 i sprzętową).
- [ ] Zweryfikować SPI/nRF na obu RF-Nano: F7 (Debug/Release), F12, wynik `NRF24 OK`, ponowienie diagnostyki znakiem `?`, readback i odporność na brak odpowiedzi układu.
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
