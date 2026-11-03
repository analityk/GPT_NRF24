#ifndef GPT_NRF24_RADIO_H
#define GPT_NRF24_RADIO_H

#include <stdint.h>

// A testable nRF24L01+ SPI command/registry/FIFO layer.
// Spi must implement begin() and bool transfer(uint8_t, uint8_t&).
// Pin must implement set() (HIGH) and clear() (LOW).
// No heap, no virtual dispatch, no clock waits and no MCU dependencies.
// CE timing and TX/RX state transitions belong to the future MAC/controller.
template <typename Spi, typename Pin>
class Nrf24 {
public:
    enum Register : uint8_t {
        CONFIG=0x00, EN_AA=0x01, EN_RXADDR=0x02, SETUP_AW=0x03,
        SETUP_RETR=0x04, RF_CH=0x05, RF_SETUP=0x06, STATUS=0x07,
        OBSERVE_TX=0x08, RX_ADDR_P0=0x0A, TX_ADDR=0x10,
        RX_PW_P0=0x11, FIFO_STATUS=0x17, DYNPD=0x1C, FEATURE=0x1D
    };
    enum class Result : uint8_t { OK=0, SPI_ERROR=1, NO_RADIO=2, VERIFY_ERROR=3 };
    struct Probe {
        Result result;
        uint8_t reg;        // register at which failure occurred, if applicable
        uint8_t expected;
        uint8_t observed;
        uint8_t status;
        uint8_t channel;
        uint8_t config;
        uint8_t rfSetup;
        uint8_t fifoStatus;
        Probe() : result(Result::SPI_ERROR), reg(0), expected(0), observed(0),
                  status(0), channel(0), config(0), rfSetup(0), fifoStatus(0) {}
    };

    Nrf24(Spi& spi, Pin& ce, Pin& csn) : spi_(spi), ce_(ce), csn_(csn) {}

    void begin() {
        ce_.clear();    // standby/power-down: never pulse CE during SPI setup
        csn_.set();     // deselect chip
        spi_.begin();
    }

    // Hardware CE belongs to this radio instance; PHY owns its transitions.
    void ceHigh() { ce_.set(); }
    void ceLow() { ce_.clear(); }

    bool status(uint8_t& value) { return transaction(0xFF, 0, 0, 0, &value); }
    bool readRegister(uint8_t reg, uint8_t& value) {
        if (reg > 0x1F) return false;
        return transaction(reg, 0, &value, 1, 0);
    }
    bool writeRegister(uint8_t reg, uint8_t value) {
        if (reg > 0x1D || (reg >= 0x18 && reg <= 0x1B)) return false;
        return transaction(static_cast<uint8_t>(0x20U | reg), &value, 0, 1, 0);
    }
    bool readRegisters(uint8_t reg, uint8_t* dst, uint8_t count) {
        if (reg > 0x1F || !dst || count == 0 || count > 5) return false;
        return transaction(reg, 0, dst, count, 0);
    }
    bool writeRegisters(uint8_t reg, const uint8_t* src, uint8_t count) {
        if (reg > 0x1D || (reg >= 0x18 && reg <= 0x1B) || !src || count == 0 || count > 5) return false;
        return transaction(static_cast<uint8_t>(0x20U | reg), src, 0, count, 0);
    }
    bool flushRx() { return transaction(0xE2, 0, 0, 0, 0); }
    bool flushTx() { return transaction(0xE1, 0, 0, 0, 0); }
    bool clearIrq(uint8_t flags) {
        return writeRegister(STATUS, static_cast<uint8_t>(flags & 0x70U));
    }
    bool readPayload(uint8_t* dst) {
        return dst && transaction(0x61, 0, dst, 32, 0);
    }
    bool writePayload(const uint8_t* src) {
        return src && transaction(0xA0, src, 0, 32, 0);
    }


    // Enhanced ShockBurst ACK payload support (no change to the default
    // fixed-payload configuration used by the UART bridge).
    // Some devices lock FEATURE until ACTIVATE 0x73; probe before toggling,
    // never blindly toggle a device where FEATURE already works.
    bool enableAckPayload(uint8_t retries=0x23) {
        uint8_t feature=0;
        if (!writeRegister(FEATURE,0x06) ||
            !readRegister(FEATURE,feature)) return false;
        if (feature!=0x06) {
            const uint8_t activate=0x73;
            if (!transaction(0x50,&activate,0,1,0) ||
                !writeRegister(FEATURE,0x06) ||
                !readRegister(FEATURE,feature) || feature!=0x06)
                return false;
        }
        uint8_t dynamic=0;
        uint8_t retryRead=0;
        if (!writeRegister(DYNPD,0x01) ||
            !readRegister(DYNPD,dynamic) || dynamic!=0x01 ||
            !writeRegister(SETUP_RETR,retries) ||
            !readRegister(SETUP_RETR,retryRead) || retryRead!=retries)
            return false;
        return clearIrq(0x70) && flushRx() && flushTx();
    }

    bool readPayloadWidth(uint8_t& width) {
        return transaction(0x60,0,&width,1,0);
    }

    // Dynamic payload width must be read from the chip's RX FIFO. A corrupt
    // value is recovered by flushing RX, as required by the data sheet.
    bool readDynamicPayload(uint8_t* dst,uint8_t& width) {
        if (!dst || !readPayloadWidth(width)) return false;
        if (width==0 || width>32) {
            (void)flushRx();
            return false;
        }
        return transaction(0x61,0,dst,width,0);
    }

    // W_TX_PAYLOAD with a variable 1..32B payload. DPL must already be
    // enabled on the selected pipe. No fixed-width padding is transmitted.
    bool writeDynamicPayload(const uint8_t* bytes,uint8_t count) {
        return bytes && count>=1 && count<=32 &&
               transaction(0xA0,bytes,0,count,0);
    }

    bool writeAckPayload(uint8_t pipe,const uint8_t* bytes,uint8_t count) {
        if (pipe>5 || !bytes || count==0 || count>32) return false;
        return transaction(static_cast<uint8_t>(0xA8U|pipe),
                           bytes,0,count,0);
    }

    // Safe bring-up: register read/write/restore test, then deterministic
    // RF settings, leaving PWR_UP=0 and CE LOW. No RF transmission occurs.
    Probe probeAndConfigure() {
        Probe p;
        begin();
        if (!status(p.status)) return failure(p, Result::SPI_ERROR, STATUS);
        if (!readRegister(RF_CH, p.channel)) return failure(p, Result::SPI_ERROR, RF_CH);
        if (p.status == 0xFFU || p.channel > 125U)
            return failure(p, Result::NO_RADIO, RF_CH);

        const uint8_t oldChannel = p.channel;
        const uint8_t patterns[] = { 0x2A, 0x55 };
        for (uint8_t i=0; i<2; ++i) {
            if (!writeRegister(RF_CH, patterns[i])) {
                (void)writeRegister(RF_CH, oldChannel);
                return failure(p, Result::SPI_ERROR, RF_CH);
            }
            uint8_t received=0;
            if (!readRegister(RF_CH, received)) {
                (void)writeRegister(RF_CH, oldChannel);
                return failure(p, Result::SPI_ERROR, RF_CH);
            }
            if (received != patterns[i]) {
                (void)writeRegister(RF_CH, oldChannel);
                p.expected = patterns[i];
                p.observed = received;
                return failure(p, Result::VERIFY_ERROR, RF_CH);
            }
        }
        if (!writeRegister(RF_CH, oldChannel))
            return failure(p, Result::SPI_ERROR, RF_CH);

        // CRC16, radio powered down, fixed 32 B, 1 Mbps, 5 B addressing,
        // auto ACK on pipe0 only; chosen channel 76 (2476 MHz).
        const uint8_t pairs[][2] = {
            { CONFIG, 0x0C }, { EN_AA, 0x01 }, { EN_RXADDR, 0x01 },
            { SETUP_AW, 0x03 }, { SETUP_RETR, 0x3F }, { RF_CH, 76 },
            { RF_SETUP, 0x06 }, { RX_PW_P0, 32 }, { DYNPD, 0 },
            { FEATURE, 0 }
        };
        const uint8_t pairCount = static_cast<uint8_t>(sizeof(pairs) / sizeof(pairs[0]));
        for (uint8_t i=0; i<pairCount; ++i) {
            const uint8_t reg = pairs[i][0];
            const uint8_t expected = pairs[i][1];
            if (!writeRegister(reg, expected))
                return failure(p, Result::SPI_ERROR, reg);
            uint8_t observed=0;
            if (!readRegister(reg, observed))
                return failure(p, Result::SPI_ERROR, reg);
            if (observed != expected) {
                p.expected = expected;
                p.observed = observed;
                return failure(p, Result::VERIFY_ERROR, reg);
            }
        }

        // Identical discovery address for now; later MAC assigns unicast pipes.
        const uint8_t address[5] = { 0xD2, 0xF0, 0xF0, 0xF0, 0xA1 };
        const uint8_t addrRegs[2] = { RX_ADDR_P0, TX_ADDR };
        for (uint8_t a=0; a<2; ++a) {
            const uint8_t reg = addrRegs[a];
            if (!writeRegisters(reg, address, 5))
                return failure(p, Result::SPI_ERROR, reg);
            uint8_t readBack[5] = {};
            if (!readRegisters(reg, readBack, 5))
                return failure(p, Result::SPI_ERROR, reg);
            for (uint8_t i=0; i<5; ++i) {
                if (readBack[i] != address[i]) {
                    p.reg=reg; p.expected=address[i]; p.observed=readBack[i];
                    return failure(p, Result::VERIFY_ERROR, reg);
                }
            }
        }

        if (!clearIrq(0x70) || !flushRx() || !flushTx())
            return failure(p, Result::SPI_ERROR, STATUS);
        if (!status(p.status) || !readRegister(CONFIG,p.config) ||
            !readRegister(RF_CH,p.channel) || !readRegister(RF_SETUP,p.rfSetup) ||
            !readRegister(FIFO_STATUS,p.fifoStatus))
            return failure(p, Result::SPI_ERROR, STATUS);
        p.result = Result::OK;
        return p;
    }

private:
    Spi& spi_;
    Pin& ce_;
    Pin& csn_;

    static Probe failure(Probe& p, Result result, uint8_t reg) {
        p.result=result;
        p.reg=reg;
        return p;
    }

    // One CE-independent SPI transaction; releases CSN even on SPI failure.
    bool transaction(uint8_t command, const uint8_t* tx,
                     uint8_t* rx, uint8_t count, uint8_t* statusByte) {
        uint8_t received=0;
        csn_.clear();
        bool ok = spi_.transfer(command, received);
        if (statusByte) *statusByte=received;
        if (ok) {
            for (uint8_t i=0; i<count; ++i) {
                const uint8_t outgoing=tx ? tx[i] : 0xFFU;
                if (!spi_.transfer(outgoing, received)) {
                    ok=false;
                    break;
                }
                if (rx) rx[i]=received;
            }
        }
        csn_.set();
        return ok;
    }
};
#endif
