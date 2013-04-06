#ifndef __VBA_SOUND_DRIVER_H__
#define __VBA_SOUND_DRIVER_H__

#include "types.h"

/**
 * Sound driver abstract interface for the core to use to output sound.
 * Subclass this to implement a new sound driver.
 */
class SoundDriver
{
public:

	/**
	 * Destructor. Free the resources allocated by the sound driver.
	 */
	virtual ~SoundDriver() { };

	/**
	 * Initialize the sound driver.
	 * @param sampleRate In Hertz
	 */
	virtual bool init(long sampleRate) = 0;

	/**
	 * Tell the driver that the sound stream has paused
	 */
	virtual void pause() = 0;

	/**
	 * Reset the sound driver
	 */
	virtual void reset() = 0;

	/**
	 * Tell the driver that the sound stream has resumed
	 */
	virtual void resume() = 0;

	/**
	 * Write length bytes of data from the finalWave buffer to the driver output buffer.
	 */
	virtual void write(u16 * finalWave, int length) = 0;

	virtual void setThrottle(unsigned short throttle) { };

	bool userMute;

	virtual void mute() = 0;

	virtual void unMute() = 0;

	virtual void doUserMute() = 0;

};

#endif // __VBA_SOUND_DRIVER_H__