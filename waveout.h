//#include "types.h"

void DRV_WriteWaveData(uint32 *Buffer, int Count);
int DRV_EndWaveRecord();
bool DRV_BeginWaveRecord(const char *fn);
bool DRV_WaveRecordActive();