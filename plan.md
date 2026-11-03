# Plan rozwoju GPT_NRF24

## M0 — środowisko i programowanie

Praca w Microchip Studio / Atmel Studio, AVR-GCC C++. Obie płytki mają bootloader pozwalający programować je przez USB. Przygotowany proces programowania pozwala używać jednego pliku HEX dla dwóch urządzeń na oddzielnych portach szeregowych.

Szczegóły lokalnego narzędzia programującego i numery COM są konfiguracją stanowiska, a nie częścią firmware.

## M1 — dwukierunkowe łącze radiowe

### M1a — podstawowa transmisja

- Uporządkować warstwy UART, radia, protokołu i aplikacji.
- Uruchomić przesyłanie tekstu z jednej konsoli do drugiej przez radio.
- Potwierdzić wynik na fizycznych płytkach.

### M1b — symetria i logiczny full duplex

- Jeden firmware obsługuje obie płytki.
- Każde urządzenie może samodzielnie inicjować transmisję.
- Zapewnić współpracę kierunków mimo półdupleksowego charakteru nRF24L01+.
- Wprowadzić wykrywanie obecności drugiego urządzenia.

### M1c — odporność i odbiór

- Obsłużyć konflikt jednoczesnych prób nadawania.
- Określić i sprawdzić działanie buforów oraz zachowanie przy przeciążeniu.
- Obsłużyć utratę i ponowne uzyskanie łączności.
- Przetestować dwukierunkowy ruch na dwóch rzeczywistych urządzeniach.

## Zasady realizacji

- Nie przesądzać szczegółów protokołu bez analizy ograniczeń radia i warunków testu.
- Zachować rozdział sprzętu, transportu i aplikacji.
- Nie wprowadzać alokacji dynamicznej.
- Testy jednostkowe i kompilacja są potrzebne, ale nie zastępują testu radiowego na płytkach.
- Dalsze kamienie milowe wyznaczać dopiero po odebraniu M1.
