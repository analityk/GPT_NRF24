# TODO — GPT_NRF24

**Etap: INITIAL**  
**Aktualny kamień milowy: M1 — dwukierunkowa komunikacja radiowa**

## Zadania

- [x] Spisać projekt formatu binarnego v1, parsera, telemetrii, retransmisji i wektorów testowych (`protocol-v1.md`); bez implementacji i testów sprzętowych.

- [ ] Ustalić architekturę komunikacji dwóch równorzędnych urządzeń.
- [ ] Zaimplementować podstawową obsługę nRF24L01+.
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
