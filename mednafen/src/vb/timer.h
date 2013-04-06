#ifndef __MDFN_VB_TIMER_H
#define __MDFN_VB_TIMER_H

namespace MDFN_IEN_VB
{

v810_timestamp_t TIMER_Update(v810_timestamp_t timestamp);
void TIMER_ResetTS(void);
uint8 TIMER_Read(const v810_timestamp_t &timestamp, uint32 A);
void TIMER_Write(const v810_timestamp_t &timestamp, uint32 A, uint8 V);

void TIMER_Power(void);

int TIMER_StateAction(StateMem *sm, int load, int data_only);

}

#endif
