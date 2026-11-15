#ifndef GPT_NRF24_HOST_FLOW_STATUS_H
#define GPT_NRF24_HOST_FLOW_STATUS_H
#include <stdint.h>
#include "pp_recovery_control.h"

// Exactly TWO local host<->MCU status bytes, never included in RF packets.
// Low six bits encode free buffer space in 4-byte units (0..63, saturated).
// TX: bit7=STOP writing, bit6=delivery of earlier bytes uncertain.
// RX: bit7=peer offline, bit6=local RX has < one 24-byte RF body of space.
// Status is only advisory to a cooperative producer. The atomic commit
// below is the authoritative check, including the case of <4 free bytes.
class HostFlowStatus {
  public:
	enum { STOP = 0x80, UNCERTAIN = 0x40, PEER_OFFLINE = 0x80, RX_PRESSURE = 0x40 };

	struct Pair {
		uint8_t tx, rx;
		Pair(uint8_t t = 0, uint8_t r = 0);
	};

	static Pair snapshot(const PpRecoveryControl& recovery, uint16_t txFree, uint16_t rxFree,
		bool inputPause = false);
	// No partial write: a host integration calls this before committing
	// a whole logical chunk, then commits atomically in the same context.
	static bool canCommit(
		const PpRecoveryControl& recovery, uint16_t exactFree, uint16_t requested);

  private:
	static uint8_t units(uint16_t available);
};
#endif
