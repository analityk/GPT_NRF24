#ifndef GPT_SPI_REGISTER_MOCK_H
#define GPT_SPI_REGISTER_MOCK_H
#include <stdint.h>
bool spiMasterEnabled();
void spiWriteByte(uint8_t value);
bool spiByteReady();
uint8_t spiReadByte();
#endif
