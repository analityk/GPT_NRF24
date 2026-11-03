# Koncepcja dwóch profili MAC opartych na ACK Payload (2026-11-03)

**Status: koncepcja architektoniczna omawiana z właścicielem, nie implementacja i nie zatwierdzone parametry czasowe.** Poprzedni most UART oparty na naprzemiennym PTX/PRX nadal pozostaje działającym punktem odniesienia. Nie utożsamiać pomysłów poniżej z pomiarami fizycznymi.

## Zasada wspólna

- Jeden format ramki v1 (8 B nagłówka, BODY do 24 B), jedno odkrywanie/sesja, adresacja/endpointy i jedna telemetria; **dwa profile dostępu do medium**, a nie dwa osobne protokoły całego stosu.
- Dwa niezależne logiczne kierunki danych: A→B jako zwykły payload PTX oraz B→A jako wcześniej przygotowany sprzętowy ACK Payload PRX. Każda operacja radiowa jest fizycznie półdupleksowa; **PTX inicjuje każdą wymianę**. PRX nie wyśle nic samodzielnie bez zapytania lub uzgodnionej zamiany ról.
- Sprzęt Enhanced ShockBurst wykonuje CRC16, ACK/retry i retransmisję pojedynczej ramki. W ACK Payload jest do 32 B, a jego zawartość musi być wcześniej zapisana do FIFO poleceniem W_ACK_PAYLOAD. Wymaga EN_ACK_PAY + EN_DPL/DPL_Px; nie jest to obecnie skonfigurowane.
- Zdarzenie radiowe TX_DS/ACK oznacza zakończenie wymiany sprzętowej, NIE przyjęcie danych do kolejki/plików odbiorcy. Warstwa transportowa ma własne ACK aplikacyjne, numerację, deduplikację, status BUSY/kredyty, retry i raporty stanu. Retransmitowane radio payloady i ACK payloady nie mogą powodować powtórnego wykonania efektu aplikacji.
- Nie należy utożsamiać PER (utrata ramek, MAX_RT, ARC) z BER: CRC odrzuca błędne ramki i nie pozwala poznać liczby błędnych bitów. Zanikanie A→B i B→A nie musi być idealnie symetryczne, nawet jeśli sprzętowe ACK sprzęga powodzenie obu etapów transakcji.
- Polityka dostępu do medium jest niezależna od transportu typu RAW/TRACKED/RELIABLE oraz od usługi przesyłania pliku.

## STREAM — dwukierunkowy, niska latencja

- A może stale pełnić PTX, B stale PRX. A nadaje dane lub krótkie ramki wyzwalające odpowiedź; B odsyła przygotowany wcześniej DATA/CONTROL jako ACK Payload.
- Oba strumienie są zgrane transakcjami RF, bez synchronizacji dwóch zegarów co pakiet. Sterownik PTX pilnuje TX_DS/MAX_RT, PHY FIFO oraz upływu timeoutu; przy braku sprzętowego IRQ można szybko odpytywać SPI STATUS.
- W idle rozważ 1–2 krótkie zapytania co 20 ms jako **politykę oszczędzania energii**, świadomie akceptując odpowiednią latencję dla pierwszego zdarzenia B. Przy wymaganiu krótszej latencji trzeba zwiększyć częstość zapytań albo przekazać rolę PTX. Pełne usypianie PRX wymaga zsynchronizowanego budzenia z guard time, bo śpiące radio nie odbierze zapytania.
- Przy DPL pusta wiadomość aplikacyjna może mieć znacznie krótszy fizyczny payload niż 32 B, przy zachowaniu 8-bajtowego nagłówka v1; to istotne dla szybkiego odpytywania tylko jednego kierunku.

## BURST — bloki z limitem czasu i przekazywanie prawa nadawania

- Właściciel prawa do medium pozostaje PTX i nadaje możliwie szybko ciąg pakietów przez **maksymalnie około 20 ms** (wstępny parametr do zmierzenia). PRX obsługuje automatyczne ACK sprzętowe, przygotowuje własne ACK Payload z kredytami, informacją o brakach, pilnymi komunikatami i żądaniem przejęcia roli.
- **Nie trzeba** zmieniać PTX/PRX po każdej ramce ani koniecznie po każdym 20-ms bloku. Role zmienia się tylko na wynegocjowanym punkcie granicznym, gdy przynosi to korzyść lub gdy odbiorca zażąda prawa do nadawania. Podczas BURST strumień zwrotny nadal może działać w ACK Payload.
- Potwierdzenia sprzętowe pozostają per pakiet; aplikacyjne ACK/SACK może obejmować całe okno. Nie dopuścić, by retransmisja sprzętowa przekroczyła ustalony deadline zmiany roli: ograniczyć liczbę prób/ARD i zarezerwować guard. Przerwanie B wyłącznie na podstawie własnego timera w środku ACK/retry byłoby błędem.
- Żądanie przejęcia (REQUEST), przyznanie prawa (GRANT z numerem epoki/slotu), potwierdzenie (COMMIT/ready) i ograniczony lease/timeout muszą być spójne, aby po utracie ramki **nie znalazły się dwa PTX**. W razie niepewności bezpieczny stan PRX i recovery/discovery.
- Priorytety: awaryjny krótki komunikat B można często przekazać bez przejmowania roli w następnej odpowiedzi ACK Payload; długa seria B może wymagać zmiany PTX. Starvation zabronione, ograniczyć maksymalną długość lease.
- Użyteczny czas 20 ms i częstotliwość idle 1–2 zapytań/20 ms to hipotezy, nie wyniki walidacji. Zmierz packet/s, latency p50/p99, zajętość FIFO, retries, per-direction goodput i pobór prądu.

## Usługa plikowa / TCP-like jako osobna warstwa

- Nie uruchamiać trzeciego MAC. Usługa file transfer na endpointzie może ogłaszać identyfikator transferu, rozmiar, licznik segmentów (np. 232), sumę integralności oraz wymagany tryb RELIABLE.
- Odbiornik z ATmega328P 2 KB SRAM **nie może buforować całych 232 pakietów**. Stosować małe okno (np. 2–4 segmenty, rozmiar dobrany po pomiarze SRAM), advertised credits, SACK/bitmapy, streaming do dostępnego sinku (kolejka UART/Flash/pamięć zewnętrzna); nie potwierdzać przyjęcia bez zapewnionego trwałego lub określonego w kontrakcie miejsca.
- Numer segmentu nie może ograniczać transferu do jednego obrotu 8-bitowego TX_SEQ: użyć wyższego transfer ID oraz offsetu lub licznika 16/32 bit w BODY, z wersjonowaniem endpointu. Weryfikację kompletności oddzielić od radiowego CRC16. Nie obiecywać dokładnie-raz po restarcie bez trwałego journalu po stronie sinku.
- W przyszłej płytce z większym SRAM/Flash można powiększyć window i bufory bez zmiany PHY/MAC ani formatu bazowego; negocjować możliwości i dostępne kredyty per peer.

## Najbliższa walidacja, bez przedwczesnej zmiany produkcyjnego UART

1. Uruchomić fizyczne 32 B A→B / 32 B w sprzętowym ACK Payload B→A, z poprawnym odczytem DPL/RX FIFO i obsługą spóźnionych/zdublowanych odpowiedzi.
2. Zmierzyć tempo sprzętowych dwustronnych transakcji bez 20–25 ms sleepów, po uprzednim uporządkowaniu czasów PHY i SPI. Testy hostowe na mock nRF, następnie obie płytki.
3. Wpiąć obecny binarny transport aplikacyjny i deduplikację przez nową abstrakcję PHY/MAC (bez duplikacji kodu i bez utraty niewysłanych danych).
4. Dopiero wtedy dodać BURST z ograniczonym czasem lease, zbiorczymi ACK, kredytami i bezpiecznym przekazaniem ról.

Źródło mechaniki ACK Payload: Nordic nRF24L01+ Product Specification v1.0, 7.4.1 i 7.5.2: https://devzone.nordicsemi.com/cfs-file/__key/communityserver-discussions-components-files/4/nRF24L01P_5F00_PS_5F00_v1.0.pdf
