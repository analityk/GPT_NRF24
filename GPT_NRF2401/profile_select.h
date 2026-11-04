#ifndef GPT_NRF24_PROFILE_SELECT_H
#define GPT_NRF24_PROFILE_SELECT_H
#include <stdint.h>

// Profile selection is a boot-time policy, never a data-plane decision.
// ATmega adapter: use GPIO(&PORTC,PC1), pin A1, with its internal pull-up.
// A1 floating/HIGH = existing PP; A1 strapped GND = BURST20.
// Read twice after pull-up settling; unstable strap fails SAFE to PP.
// This reusable selector does not run or enable BURST firmware.
namespace ProfileSelect {
    enum class Profile : uint8_t { PP=0, BURST20=1 };
    inline Profile fromStableLevel(bool high) {
        return high?Profile::PP:Profile::BURST20;
    }
    template<class DigitalInput,class Delay>
    Profile latch(DigitalInput& pin,Delay& delay) {
        pin.i2c_set(); // digital input with internal pull-up
        delay.us(50);
        const uint8_t first=pin.read();
        delay.us(50);
        const uint8_t second=pin.read();
        return (first==second)?
               fromStableLevel(first!=0U):Profile::PP;
    }
}
#endif
