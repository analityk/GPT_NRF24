#include "../GPT_NRF2401/byte_ring.h"
#include <assert.h>
#include <stdint.h>

int main() {
    ByteRing<128> ring;
    assert(ring.available() == 0);
    assert(ring.freeSpace() == 127);
    uint8_t out = 0;
    assert(!ring.pop(out));
    for (unsigned i = 0; i < 127; ++i) assert(ring.push(uint8_t(i)));
    assert(!ring.push(0xFF));
    assert(ring.available() == 127);
    for (unsigned i = 0; i < 90; ++i) {
        assert(ring.pop(out));
        assert(out == uint8_t(i));
    }
    for (unsigned i = 127; i < 217; ++i) assert(ring.push(uint8_t(i)));
    assert(!ring.push(0xFF));
    for (unsigned i = 90; i < 217; ++i) {
        assert(ring.pop(out));
        assert(out == uint8_t(i));
    }
    assert(!ring.pop(out));
    assert(ring.freeSpace() == 127);
    ring.reset();
    assert(ring.available() == 0);
}
