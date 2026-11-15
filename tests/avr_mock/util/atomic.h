#ifndef GPT_TEST_AVR_ATOMIC_H
#define GPT_TEST_AVR_ATOMIC_H
// Sequential register-level tests. Interrupt interleavings are explicitly
// invoked by the test; this macro does not simulate instruction timing.
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(mode) for (bool atomic_once = true; atomic_once; atomic_once = false)
#endif
