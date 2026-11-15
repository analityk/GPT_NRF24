#include "burst20_schedule.h"

Burst20Schedule::Position::Position()
	: owner(Owner::A),
	  phase(Phase::GUARD),
	  cycle(0),
	  slotRemaining(0),
	  phaseRemaining(0),
	  cycleRemaining(0) {
}

bool Burst20Schedule::Position::canStart(uint32_t worstCaseTicks) const {
	return (phase == Phase::DATA || phase == Phase::REPORT) && worstCaseTicks <= phaseRemaining;
}

Burst20Schedule::Burst20Schedule()
	: anchor_(0),
	  cycle_(0),
	  share_(Share::A10_B10),
	  aligned_(false) {
}

bool Burst20Schedule::valid(Share share) {
	return share == Share::A5_B15 || share == Share::A10_B10 || share == Share::A15_B5;
}

uint8_t Burst20Schedule::aQuanta(Share share) {
	return valid(share) ? static_cast<uint8_t>(share) : 0U;
}

uint8_t Burst20Schedule::bQuanta(Share share) {
	return valid(share) ? static_cast<uint8_t>(4U - aQuanta(share)) : 0U;
}

Burst20Schedule::Error Burst20Schedule::align(
	uint32_t cycleStartTicks, Share share, uint16_t cycle) {
	if (!valid(share)) {
		return Error::BAD_SHARE;
	}
	anchor_ = cycleStartTicks;
	cycle_ = cycle;
	share_ = share;
	aligned_ = true;
	return Error::NONE;
}

void Burst20Schedule::invalidate() {
	aligned_ = false;
}

bool Burst20Schedule::aligned() const {
	return aligned_;
}

Burst20Schedule::Share Burst20Schedule::share() const {
	return share_;
}

uint16_t Burst20Schedule::cycle() const {
	return cycle_;
}

Burst20Schedule::Error Burst20Schedule::advance(uint32_t nowTicks) {
	if (!aligned_) {
		return Error::NOT_ALIGNED;
	}
	const uint32_t elapsed = nowTicks - anchor_;
	const uint32_t jumps = elapsed / CYCLE_TICKS;
	if (jumps) {
		anchor_ += jumps * CYCLE_TICKS;
		cycle_ = cycle_ + jumps;
	}
	return Error::NONE;
}

Burst20Schedule::Position Burst20Schedule::position(uint32_t nowTicks) const {
	Position p;
	if (!aligned_) {
		return p;
	}
	// Call advance() before querying if the next 20ms boundary passed.
	const uint32_t offset = nowTicks - anchor_;
	if (offset >= CYCLE_TICKS) {
		return p;
	} // fail closed: no stale TX rights
	const uint32_t aTicks = static_cast<uint32_t>(aQuanta(share_)) * QUANTUM_TICKS;
	const bool isA = (offset < aTicks);
	p.owner = isA ? Owner::A : Owner::B;
	const uint32_t local = isA ? offset : (offset - aTicks);
	const uint32_t slotLength = isA ? aTicks : (CYCLE_TICKS - aTicks);
	const uint32_t dataEnd = slotLength - REPORT_TICKS - GUARD_TICKS;
	const uint32_t controlEnd = slotLength - GUARD_TICKS;
	p.slotRemaining = slotLength - local;
	p.cycleRemaining = CYCLE_TICKS - offset;
	p.cycle = cycle_;
	if (local < dataEnd) {
		p.phase = Phase::DATA;
		p.phaseRemaining = dataEnd - local;
	} else if (local < controlEnd) {
		p.phase = Phase::REPORT;
		p.phaseRemaining = controlEnd - local;
	} else {
		p.phase = Phase::GUARD;
		p.phaseRemaining = slotLength - local;
	}
	return p;
}
