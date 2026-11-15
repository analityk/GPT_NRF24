#ifndef GPT_RELIABLE_INPUT_SOURCE_H
#define GPT_RELIABLE_INPUT_SOURCE_H
#include "hardware_bindings.h"
#include "foreground_ports.h"

class ReliableInputSource {
  public:
	explicit ReliableInputSource(StreamHandle port);
	void pump();
	RetainedByteRing& ring() const;
	bool hasInput() const;

  private:
	StreamHandle port_;
};
#endif
