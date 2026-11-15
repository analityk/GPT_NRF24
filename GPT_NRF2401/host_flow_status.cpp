#include "host_flow_status.h"

HostFlowStatus::Pair::Pair(uint8_t t, uint8_t r)
	: tx(t),
	  rx(r) {
}

HostFlowStatus::Pair HostFlowStatus::snapshot(
	const PpRecoveryControl& recovery, uint16_t txFree, uint16_t rxFree, bool inputPause) {
	const bool ready = recovery.canProduce() && !inputPause;
	return Pair(static_cast<uint8_t>(
					(ready ? units(txFree) : 0U) | (ready ? 0U : static_cast<uint8_t>(STOP)) |
					(recovery.uncertainty() ? static_cast<uint8_t>(UNCERTAIN) : 0U)),
		static_cast<uint8_t>(units(rxFree) |
							 (recovery.state() == PpRecoveryControl::State::HEALTHY
									 ? 0U
									 : static_cast<uint8_t>(PEER_OFFLINE)) |
							 (rxFree < 24U ? static_cast<uint8_t>(RX_PRESSURE) : 0U)));
}

bool HostFlowStatus::canCommit(
	const PpRecoveryControl& recovery, uint16_t exactFree, uint16_t requested) {
	return recovery.canProduce() && requested <= exactFree;
}

uint8_t HostFlowStatus::units(uint16_t available) {
	const uint16_t n = available / 4U;
	return static_cast<uint8_t>(n > 63U ? 63U : n);
}
