#ifndef FALLOUT_CLOCK_H_
#define FALLOUT_CLOCK_H_

namespace fallout {

// Gameplay clock and its legacy elapsed-time helpers.
unsigned int getTicks();
unsigned int getTicksSince(unsigned int start);
unsigned int getTicksBetween(unsigned int end, unsigned int start);

// Unscaled clock for input, scrolling, and script get_uptime.
unsigned int getRealTicks();

} // namespace fallout

#endif /* FALLOUT_CLOCK_H_ */
