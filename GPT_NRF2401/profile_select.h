#ifndef GPT_NRF24_PROFILE_SELECT_H
#define GPT_NRF24_PROFILE_SELECT_H
#include "hardware_bindings.h"
#include <stdint.h>
#include "foreground_ports.h"

// Profile selection is a boot-time policy, never a data-plane decision.
// ATmega adapter: use GPIO(&PORTC,PC1), pin A1, with its internal pull-up.
// A1 floating/HIGH = existing PP; A1 strapped GND = BURST20.
// Read twice after pull-up settling; unstable strap fails SAFE to PP.
// This reusable selector does not run or enable BURST firmware.
namespace ProfileSelect {
enum class Profile : uint8_t { PP = 0, BURST20 = 1 };
Profile fromStableLevel(bool high);
Profile latch(PinHandle pin, DelayHandle delay);
} // namespace ProfileSelect
#endif
