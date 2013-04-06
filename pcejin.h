#include <string>
#include "utils\svnrev.h"
#include "main.h"

class Pcejin
{
public:

	Pcejin() : started(false), 
		aspectRatio(true), 
		romLoaded(false), 
		frameAdvance(false), 
		width(384), 
		height(224), 
		versionName(std::string("VBjin-ovr v") + std::string(SVN_REV_STR)),
		slow(false) {
	}

	bool aspectRatio;
	bool started;
	bool romLoaded;
	bool maximized;

	bool isLagFrame;
	int lagFrameCounter;
	char inputDisplayString[128];
	int height;
	int width;
	int windowSize;

	std::string versionName;

	bool frameAdvance;
	bool fastForward;
	bool slow;

	u16 pads[5];

	virtual void pause() {

		if(romLoaded) {
			started ^=1;

		if(started)
			soundDriver->resume();
		else
			soundDriver->pause();
		}
	}

	virtual void tempUnPause() {
		if(started)
			soundDriver->resume();
	}
};

extern Pcejin pcejin;
