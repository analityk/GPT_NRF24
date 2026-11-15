#include "pp_recovery_control.h"

PpRecoveryControl::PpRecoveryControl(uint32_t expectedTurnTicks, uint32_t offlineTicks,
	UncertainPolicy policy, uint32_t warmGraceTicks)
	: expected_(expectedTurnTicks),
	  offline_(offlineTicks),
	  warmGrace_(warmGraceTicks),
	  lastSeen_(0),
	  policy_(policy),
	  state_(State::CONNECTING),
	  uncertain_(false),
	  held_(false),
	  warm_(0),
	  cold_(0),
	  suspicions_(0),
	  expired_(0) {
}

void PpRecoveryControl::reset(uint32_t now) {
	lastSeen_ = now;
	state_ = State::CONNECTING;
	uncertain_ = false;
	held_ = false;
	warm_ = cold_ = suspicions_ = expired_ = 0;
}

void PpRecoveryControl::established(uint32_t now) {
	lastSeen_ = now;
	state_ = State::HEALTHY;
}

void PpRecoveryControl::validPeerFrame(uint32_t now) {
	if (state_ == State::SUSPECT || state_ == State::OFFLINE) {
		++warm_;
	}
	lastSeen_ = now;
	state_ = State::HEALTHY;
}

void PpRecoveryControl::poll(uint32_t now) {
	if (state_ != State::HEALTHY && state_ != State::SUSPECT) {
		return;
	}
	const uint32_t elapsed = now - lastSeen_;
	if (elapsed >= offline_) {
		state_ = State::OFFLINE;
	} else if (elapsed >= expected_ * 3UL && state_ == State::HEALTHY) {
		state_ = State::SUSPECT;
		++suspicions_;
	}
}

void PpRecoveryControl::peerStateLost(uint32_t now, bool hadUnacknowledged) {
	lastSeen_ = now;
	state_ = State::CONNECTING;
	++cold_;
	if (hadUnacknowledged) {
		uncertain_ = true;
		held_ = (policy_ == UncertainPolicy::HOLD);
	}
}

bool PpRecoveryControl::warmExpired(uint32_t now) const {
	return state_ == State::OFFLINE && static_cast<uint32_t>(now - lastSeen_) >= warmGrace_;
}

void PpRecoveryControl::abandonWarm(uint32_t now, bool hadUnacknowledged) {
	peerStateLost(now, hadUnacknowledged);
	++expired_;
}

bool PpRecoveryControl::canProduce() const {
	return state_ == State::HEALTHY && !held_;
}

bool PpRecoveryControl::canSendData() const {
	return state_ == State::HEALTHY && !held_;
}

bool PpRecoveryControl::uncertainty() const {
	return uncertain_;
}

bool PpRecoveryControl::held() const {
	return held_;
}

void PpRecoveryControl::retryUncertain() {
	held_ = false;
}

void PpRecoveryControl::acknowledgeUncertainty() {
	uncertain_ = false;
}

PpRecoveryControl::State PpRecoveryControl::state() const {
	return state_;
}

uint16_t PpRecoveryControl::warmRecoveries() const {
	return warm_;
}

uint16_t PpRecoveryControl::coldRecoveries() const {
	return cold_;
}

uint16_t PpRecoveryControl::suspicions() const {
	return suspicions_;
}

uint16_t PpRecoveryControl::expiredRecoveries() const {
	return expired_;
}
