#ifndef __VB_VIP_H
#define __VB_VIP_H

#include "git.h"

namespace MDFN_IEN_VB
{

//bool VIP_Init(void);
void VIP_Power(void);

void VIP_Set3DMode(uint32 mode);
uint32 VIP_GetColorMode();
void VIP_SetColorMode(uint32 mode);
void VIP_SetParallaxDisable(bool disabled);
int VIP_GetSideBySidePixels();
void VIP_SetSideBySidePixels(int pixels);
void VIP_SetViewDisp(int display);
//void VIP_SetDefaultColor(uint32 default_color);
void VIP_SetAnaglyphColors(uint32 lcolor, uint32 rcolor);	// R << 16, G << 8, B << 0

v810_timestamp_t MDFN_FASTCALL VIP_Update(const v810_timestamp_t timestamp);
void VIP_ResetTS(void);

void VIP_StartFrame(EmulateSpecStruct *espec);

uint8 VIP_Read8(v810_timestamp_t &timestamp, uint32 A);
uint16 VIP_Read16(v810_timestamp_t &timestamp, uint32 A);


void VIP_Write8(v810_timestamp_t &timestamp, uint32 A, uint8 V);
void VIP_Write16(v810_timestamp_t &timestamp, uint32 A, uint16 V);



int VIP_StateAction(StateMem *sm, int load, int data_only);

}
#endif
