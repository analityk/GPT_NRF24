# Cel GPT_NRF24 — dwuwęzłowy szybki rdzeń z opcjonalnymi usługami

Dwie RF-Nano z tym samym firmware komunikują się poprzez nRF24L01+ w układzie A/PTX DATA i B/PRX ACK Payload. Warstwy PHY, łącza i negocjacji sesji nie interpretują 24-bajtowego BODY. Priorytet: szybkie wymiany z pełną diagnostyką i poprawną rekonwergencją po resetach, bez dynamicznej alokacji, na ATmega328P 2 KB SRAM.

## Kamienie milowe potwierdzone na urządzeniach

- Około **625 wymian/s** z 8+24 B testowego BODY na kierunek, bez negocjacji SID.
- Około **612–613 wymian/s** przy negocjowanej sesji; poprawne negocjowanie nowego SID po resecie A, B i obu płyt.
- Poprzedni, **odrębny** most UART po starym formacie został fizycznie potwierdzony i zachowany w historii, ale nie jest obecnym kodem.

## Aktywny etap 2026-11-04 — nowy UART ↔ RF ↔ UART

- Odbieranie arbitralnych bajtów z UART0 A i B oraz dostarczanie na przeciwny UART, bez dodatkowych logów i interpretacji.
- `uart_reliable_stream.h` jako niezależny transport stop-and-wait (22 bajty UART + 2 bajty sekwencji/ACK w BODY 24 B), z własnym ACK po przyjęciu do TX UART.
- Kredyty oparte na wolnym miejscu kolejki docelowej, odrzucanie duplikatów i powtórzenia, zgodne z ACK Payload i zmianą sesji.
- Test hostowy 4096 B na kierunek z symulowanymi stratami i opóźnieniem ACK: **PASS**. Cross-kompilacja ATmega328P AVR-GCC: **PASS**.
- **Odbiór sprzętowy NOWEGO mostu UART pozostaje otwarty.** Nie wolno przepisywać wyników starszego firmware jako dowodu osiągów nowego.

Nie implementujemy teraz plikowego protokołu aplikacyjnego, szyfrowania ani wielowęzłowego MAC. Warunek następnego odbioru: krótkie i dłuższe wiadomości w obu kierunkach w RealTerm, bez lokalnego echa i bez diagnostycznego tekstu, poprawne przenoszenie danych po ponownym zestawieniu sesji. Bezstratność przy nieograniczonym UART flood wymaga osobnego rozwiązania kontroli przepływu, obecny CH340 nie ma w projekcie aktywnego RTS/CTS.

F7 kompiluje, F12 ręcznie wgrywa; nigdy nie przywracać automatycznego flashowania Post-Build.

## Decyzja o ciągłym ping-pongu i potokowaniu — 2026-11-04

Warstwa nRF ma aktywnie wymieniać ramki **bez przerwy także przy braku ruchu UART**, aby nowa wiadomość nie musiała czekać na 10ms polling. Sprzętowy ACK Payload jest wykorzystywany na każdej udanej wymianie, a osobne APP_ACK to zawsze dane piggyback w 24B BODY. Poprzedni stop-and-wait został zastąpiony oknem **4 segmentów po 22B na kierunek** (plus retransmisja Go-back-N), bez modyfikacji 8B rdzenia, SID i fizycznych ról. Cel: przy ciągłym ruchu osiągnąć docelowo limit UART 115200 w obu kierunkach, na ile pozwoli rzeczywisty 1 Mb/s link i CPU; **nowy goodput fizyczny wymaga zmierzenia**. Niekontrolowany ingress większy od zdolności przesyłu nadal wymaga flow-control.
