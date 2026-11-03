# Cel projektu GPT_NRF24

## M1 — logiczny full duplex przez dwa nRF24L01+

Dwie identyczne płytki RF-Nano z ATmega328P i wbudowanym nRF24L01+ mają automatycznie odnaleźć się i zestawić łączność radiową. Każda płytka jest podłączona przez USB do własnego terminala.

- To, co zostanie wysłane do konsoli urządzenia A, ma pojawić się na konsoli urządzenia B.
- To, co zostanie wysłane do konsoli urządzenia B, ma pojawić się na konsoli urządzenia A.
- Oba kierunki mają działać niezależnie z perspektywy użytkownika, również podczas nakładających się prób nadawania.
- Obie płytki mają działać na identycznym firmware, bez stałego przypisania roli nadajnika/odbiornika.
- Urządzenia mają samodzielnie odnajdywać partnera i odzyskiwać łączność po chwilowym zaniku.

### Kluczowe założenie fizyczne

**Pojedynczy nRF24L01+ jest półdupleksowy.** „Full duplex” jest tutaj celem **warstwy logicznej**, a nie cechą fizycznego radia. Rozwiązanie wymaga takiego sterowania TX/RX i rozstrzygania konfliktów, by użytkownicy obu konsol mogli przesyłać dane w obu kierunkach bez ręcznego przełączania ról.

To główne wyzwanie techniczne i centralny cel projektu.

## Kryterium odbioru M1

Na dwóch fizycznych płytkach z identycznym wsadem:

1. Oba urządzenia nawiązują łączność bez ręcznego wybierania nadajnika i odbiornika.
2. Tekst wprowadzony do każdej konsoli pojawia się w konsoli drugiej płytki.
3. Naprzemienne i niemal jednoczesne nadawanie z obu stron nie prowadzi do trwałego zakleszczenia.
4. Po odłączeniu i ponownym podłączeniu partnera komunikacja wraca.
5. Implementacja nie używa alokacji dynamicznej.

M1 uznajemy za zakończony dopiero po pozytywnych testach rzeczywistego sprzętu.

## Obecny status

**INITIAL.** Kamień milowy M1 nie został jeszcze odebrany. Dalsze funkcje pozostają poza zakresem obecnego etapu.
