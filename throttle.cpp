#include "types.h"
#include "pcejin.h"
#include <windows.h>
#include "GPU_osd.h"

int FastForward=0;
static u64 tmethod,tfreq;
static const u64 core_desiredfps =3294723;//~50.2  Calc: ((20000000 / (259 * 384 * 4)) * 65536) //Old: 3276800
static u64 desiredfps = core_desiredfps;
static u64 desiredFpsScaler = 256;

void isSlow() {
	if(desiredfps < core_desiredfps)
		pcejin.slow = true;
	else
		pcejin.slow = false;
}

void IncreaseSpeed(void) {

	desiredFpsScaler*=2;
	desiredfps = core_desiredfps * desiredFpsScaler / 256;
	if(desiredfps > core_desiredfps) {
		desiredfps = core_desiredfps;
		desiredFpsScaler = 256;
	}
	isSlow();
	printf("Throttle fps scaling increased to: %f\n",desiredFpsScaler/256.0);
	osd->addLine("Speed: %f\n",desiredFpsScaler/256.0);
}

void DecreaseSpeed(void) {

	desiredFpsScaler/=2;
	desiredfps = core_desiredfps * desiredFpsScaler / 256;
	if(desiredfps < 245760) {
		desiredfps = 245760;
		desiredFpsScaler = 16;
	}
	isSlow();
	printf("Throttle fps scaling decreased to: %f\n",desiredFpsScaler/256.0);
	osd->addLine("Speed: %f\n",desiredFpsScaler/256.0);
}

static u64 GetCurTime(void)
{
	if(tmethod)
	{
		u64 tmp;

		/* Practically, LARGE_INTEGER and u64 differ only by signness and name. */
		QueryPerformanceCounter((LARGE_INTEGER*)&tmp);

		return(tmp);
	}
	else
		return((u64)GetTickCount());

}

void InitSpeedThrottle(void)
{
	tmethod=0;
	if(QueryPerformanceFrequency((LARGE_INTEGER*)&tfreq))
	{
		tmethod=1;
	}
	else
		tfreq=1000;
	tfreq<<=16;    /* Adjustment for fps returned from FCEUI_GetDesiredFPS(). */
}

static bool behind=false;
bool ThrottleIsBehind() {
	return behind;
}

int SpeedThrottle(void)
{
	static u64 ttime,ltime;

	if(FastForward)
		return (0);

	behind = false;

waiter:

	ttime=GetCurTime();


	if( (ttime-ltime) < (tfreq/desiredfps) )
	{
		u64 sleepy;
		sleepy=(tfreq/desiredfps)-(ttime-ltime);  
		sleepy*=1000;
		if(tfreq>=65536)
			sleepy/=tfreq>>16;
		else
			sleepy=0;
		if(sleepy>100)
		{
			// block for a max of 100ms to
			// keep the gui responsive
			Sleep(100);
			return 1;
		}
		Sleep((DWORD)sleepy);
		goto waiter;
	}
	if( (ttime-ltime) >= (tfreq*4/desiredfps))
		ltime=ttime;
	else
	{
		ltime+=tfreq/desiredfps;

		if( (ttime-ltime) >= (tfreq/desiredfps) ) // Oops, we're behind!
		{
			behind = true;
			return 0;
		}
	}
	return(0);
}
