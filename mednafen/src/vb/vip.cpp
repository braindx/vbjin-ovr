/* Mednafen - Multi-system Emulator
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "vb.h"
#include "vip.h"
#include <math.h>
#include "endian.h"
#include "git.h"
#include "d3d.h"

#define VIP_DBGMSG(format, ...) { }
//#define VIP_DBGMSG(format, ...) printf(format "\n", ## __VA_ARGS__)

namespace MDFN_IEN_VB
{

static const uint32 AnaglyphPreset_Colors[][2] =
{
 { 0xFF0000, 0x0000FF }, // Red Blue
 { 0xFF0000, 0x00B7EB },
 { 0xFF0000, 0x00FFFF },
 { 0xFF0000, 0x00FF00 },
 { 0x00FF00, 0xFF00FF },
 { 0xFFFF00, 0x0000FF },
 { 0xFFFFFF, 0xFFFFFF }, //Greyscale
 { 0xFF0000, 0xFF0000 },
};

static uint8 FB[2][2][0x6000];
static uint8 CHR_RAM[0x8000];
static uint8 DRAM[0x20000];

#define INT_SCAN_ERR	0x0001
#define INT_LFB_END	0x0002
#define INT_RFB_END	0x0004
#define INT_GAME_START	0x0008
#define INT_FRAME_START	0x0010

#define INT_SB_HIT	0x2000
#define INT_XP_END	0x4000
#define INT_TIME_ERR	0x8000

static uint16 InterruptPending;
static uint16 InterruptEnable;

static uint8 BRTA, BRTB, BRTC, REST;
static uint8 Repeat;

static void CopyFBColumnToTarget_Anaglyph(int SideBySideSep) ;//__attribute__((noinline));
static void CopyFBColumnToTarget_AnaglyphSlow(int SideBySideSep);// __attribute__((noinline));
static void CopyFBColumnToTarget_CScope(int SideBySideSep) ;//__attribute__((noinline));
static void CopyFBColumnToTarget_SideBySide(int SideBySideSep) ;//__attribute__((noinline));
static void CopyFBColumnToTarget_PBarrier(int SideBySideSep) ;//__attribute__((noinline));
static void CopyFBColumnToTarget_OVR(int SideBySideSep) ;//__attribute__((noinline));
static void (*CopyFBColumnToTarget)(int SideBySideSep) = NULL;
static uint32 VB3DMode;
static uint32 VBColorMode;
static uint32 ColorLUT[2][256];
static int32 BrightnessCache[4];
static uint32 BrightCLUT[2][4];

static double ColorLUTNoGC[2][256][3];
static uint32 AnaSlowColorLUT[256][256];

// A few settings:
static bool ParallaxDisabled;
static uint32 Anaglyph_Colors[2];
//static uint32 Default_Color;
static int DisplayLeftRightOutputInternal;
static int SideBySideSep;

static void MakeColorLUT(const MDFN_PixelFormat &format)
{
 for(int lr = 0; lr < 2; lr++)
 {
  for(int i = 0; i < 256; i++)
  {
   double r, g, b;
   double r_prime, g_prime, b_prime;

   r = g = b = (double)i / 255;

   // TODO: Use correct gamma curve, instead of approximation.
   r_prime = pow(r, 1.0 / 2.2);
   g_prime = pow(g, 1.0 / 2.2);
   b_prime = pow(b, 1.0 / 2.2);

   switch(VBColorMode)
   {
    /*case 6: //Greyscale. Previously all others default
    r_prime = r_prime * ((Default_Color >> 16) & 0xFF) / 255;
    g_prime = g_prime * ((Default_Color >> 8) & 0xFF) / 255;
    b_prime = b_prime * ((Default_Color >> 0) & 0xFF) / 255;
	break;*/
    default: //Previously Anaglyph default
	r_prime = r_prime * ((Anaglyph_Colors[lr] >> 16) & 0xFF) / 255;
	g_prime = g_prime * ((Anaglyph_Colors[lr] >> 8) & 0xFF) / 255;
	b_prime = b_prime * ((Anaglyph_Colors[lr] >> 0) & 0xFF) / 255;
	break;
   }
   ColorLUTNoGC[lr][i][0] = pow(r_prime, 2.2 / 1.0);
   ColorLUTNoGC[lr][i][1] = pow(g_prime, 2.2 / 1.0);
   ColorLUTNoGC[lr][i][2] = pow(b_prime, 2.2 / 1.0);

   ColorLUT[lr][i] = format.MakeColor((int)(r_prime * 255), (int)(g_prime * 255), (int)(b_prime * 255), 0);
  }
 }

 // Anaglyph slow-mode LUT calculation
 for(int l_b = 0; l_b < 256; l_b++)
 {
  for(int r_b = 0; r_b < 256; r_b++)
  {
   double r, g, b;
   double r_prime, g_prime, b_prime;

   r = ColorLUTNoGC[0][l_b][0] + ColorLUTNoGC[1][r_b][0];
   g = ColorLUTNoGC[0][l_b][1] + ColorLUTNoGC[1][r_b][1];
   b = ColorLUTNoGC[0][l_b][2] + ColorLUTNoGC[1][r_b][2];

   if(r > 1.0)
    r = 1.0;
   if(g > 1.0)
    g = 1.0;
   if(b > 1.0)
    b = 1.0;

   r_prime = pow(r, 1.0 / 2.2);
   g_prime = pow(g, 1.0 / 2.2);
   b_prime = pow(b, 1.0 / 2.2);

   AnaSlowColorLUT[l_b][r_b] = format.MakeColor((int)(r_prime * 255), (int)(g_prime * 255), (int)(b_prime * 255), 0);
  }
 }
}

static void RecalcBrightnessCache(void)
{
// printf("BRTA: %d, BRTB: %d, BRTC: %d, Rest: %d\n", BRTA, BRTB, BRTC, REST);
 int32 CumulativeTime = (BRTA + 1 + BRTB + 1 + BRTC + 1 + REST + 1) + 1;
 int32 MaxTime = 128;

 BrightnessCache[0] = 0;
 BrightnessCache[1] = 0;
 BrightnessCache[2] = 0;
 BrightnessCache[3] = 0;

 for(int i = 0; i < Repeat + 1; i++)
 {
  int32 btemp[4];

  if((i * CumulativeTime) >= MaxTime)
   break;

  btemp[1] = (i * CumulativeTime) + BRTA;
  if(btemp[1] > MaxTime)
   btemp[1] = MaxTime;
  btemp[1] -= (i * CumulativeTime);
  if(btemp[1] < 0)
   btemp[1] = 0;


  btemp[2] = (i * CumulativeTime) + BRTA + 1 + BRTB;
  if(btemp[2] > MaxTime)
   btemp[2] = MaxTime;
  btemp[2] -= (i * CumulativeTime) + BRTA + 1;
  if(btemp[2] < 0)
   btemp[2] = 0;

  btemp[3] = (i * CumulativeTime) + BRTA + 1 + BRTB + 1 + BRTC;
  if(btemp[3] > MaxTime)
   btemp[3] = MaxTime;
  btemp[3] -= (i * CumulativeTime);
  if(btemp[3] < 0)
   btemp[3] = 0;

  BrightnessCache[1] += btemp[1];
  BrightnessCache[2] += btemp[2];
  BrightnessCache[3] += btemp[3];
 }

 //printf("BC: %d %d %d %d\n", BrightnessCache[0], BrightnessCache[1], BrightnessCache[2], BrightnessCache[3]);

 for(int i = 0; i < 4; i++)
  BrightnessCache[i] = 255 * BrightnessCache[i] / MaxTime;

 for(int lr = 0; lr < 2; lr++)
  for(int i = 0; i < 4; i++)
  {
   BrightCLUT[lr][i] = ColorLUT[lr][BrightnessCache[i]];
  }
}

static void Recalc3DModeStuff(bool non_rgb_output = false)
{

 switch(VB3DMode)
 {
  default: 

	   if(((Anaglyph_Colors[0] & 0xFF) && (Anaglyph_Colors[1] & 0xFF)) ||
		((Anaglyph_Colors[0] & 0xFF00) && (Anaglyph_Colors[1] & 0xFF00)) ||
		((Anaglyph_Colors[0] & 0xFF0000) && (Anaglyph_Colors[1] & 0xFF0000)) ||
		non_rgb_output)
	   {
            CopyFBColumnToTarget = CopyFBColumnToTarget_AnaglyphSlow;
	   }
           else
            CopyFBColumnToTarget = CopyFBColumnToTarget_Anaglyph;
           break;

  case VB3DMODE_CSCOPE:
           CopyFBColumnToTarget = CopyFBColumnToTarget_CScope;
           break;

  case VB3DMODE_SIDEBYSIDE:
           CopyFBColumnToTarget = CopyFBColumnToTarget_SideBySide;
           break;

  case VB3DMODE_PBARRIER:
           CopyFBColumnToTarget = CopyFBColumnToTarget_PBarrier;
           break;

  case VB3DMODE_OVR:
#if USE_D3D
		   SetupOculus( true );
#endif // #if USE_D3D
	       CopyFBColumnToTarget = CopyFBColumnToTarget_OVR;
	       break;
 }
 RecalcBrightnessCache();
}

void VIP_Set3DMode(uint32 mode)
{
	if ( VB3DMode != mode )
	{
		VB3DMode = mode;	
		Recalc3DModeStuff();
	}
}

void VIP_SetViewDisp(int display)
{
	DisplayLeftRightOutputInternal = display;
}

int VIP_GetSideBySidePixels()
{
	return SideBySideSep;
}

void VIP_SetSideBySidePixels(int pixels)
{
	SideBySideSep = pixels;
}

void VIP_SetParallaxDisable(bool disabled)
{
 ParallaxDisabled = disabled;
}

/*void VIP_SetDefaultColor(uint32 default_color)
{
 Default_Color = default_color;
 Recalc3DModeStuff();
}*/

void VIP_SetAnaglyphColors(uint32 lcolor, uint32 rcolor)
{
 Anaglyph_Colors[0] = lcolor;
 Anaglyph_Colors[1] = rcolor;
 Recalc3DModeStuff();
}

static uint16 FRMCYC;

static uint16 DPCTRL;
static bool DisplayActive;

#define XPCTRL_XP_RST	0x0001
#define XPCTRL_XP_EN	0x0002
static uint16 XPCTRL;
static uint16 SBCMP;	// Derived from XPCTRL

static uint16 SPT[4];	// SPT0~SPT3, 5f848~5f84e
static uint16 GPLT[4];
static uint8 GPLT_Cache[4][4];

static INLINE void Recalc_GPLT_Cache(int which)
{
 for(int i = 0; i < 4; i++)
  GPLT_Cache[which][i] = (GPLT[which] >> (i * 2)) & 3;
}

static uint16 JPLT[4];
static uint8 JPLT_Cache[4][4];

static INLINE void Recalc_JPLT_Cache(int which)
{
 for(int i = 0; i < 4; i++)
  JPLT_Cache[which][i] = (JPLT[which] >> (i * 2)) & 3;
}


static uint16 BKCOL;

//
//
//
static int32 CalcNextEvent(void);

static int32 last_ts;

static int32 Column;
static int32 ColumnCounter;

static int32 DisplayRegion;
static bool DisplayFB;

static int32 GameFrameCounter;

static int32 DrawingCounter;
static bool DrawingActive;
static bool DrawingFB;
static uint32 DrawingBlock;
static int32 SB_Latch;
static int32 SBOUT_InactiveTime;

//static uint8 CTA_L, CTA_R;

static void CheckIRQ(void)
{
 VBIRQ_Assert(VBIRQ_SOURCE_VIP, (bool)(InterruptEnable & InterruptPending));

 #if 0
 printf("%08x\n", InterruptEnable & InterruptPending);
 if((bool)(InterruptEnable & InterruptPending))
  puts("IRQ asserted");
 else
  puts("IRQ not asserted"); 
 #endif
}


/*bool VIP_Init(void)
{
 //ParallaxDisabled = false; //Force Set Elsewhere
 //DisplayLeftRightOutputInternal = 0 // Force Set Elsewhere
 //Anaglyph_Colors[0] = 0xFF0000; //Force Set Elsewhere
 //Anaglyph_Colors[1] = 0x0000FF; //Force Set Elsewhere
 //VB3DMode = VB3DMODE_ANAGLYPH; // Force Set Elsewhere
 //Default_Color = 0xFFFFFF; //Unused now

 //Recalc3DModeStuff();

 return(true);
}*/

void VIP_Power(void)
{
 Repeat = 0;
 SB_Latch = 0;
 SBOUT_InactiveTime = -1;
 last_ts = 0;

 Column = 0;
 ColumnCounter = 259;

 DisplayRegion = 0;
 DisplayFB = 0;

 GameFrameCounter = 0;

 DrawingCounter = 0;
 DrawingActive = false;
 DrawingFB = 0;
 DrawingBlock = 0;

 DPCTRL = 2;
 DisplayActive = false;



 memset(FB, 0, 0x6000 * 2 * 2);
 memset(CHR_RAM, 0, 0x8000);
 memset(DRAM, 0, 0x20000);

 InterruptPending = 0;
 InterruptEnable = 0;

 BRTA = 0;
 BRTB = 0;
 BRTC = 0;
 REST = 0;

 FRMCYC = 0;

 XPCTRL = 0;
 SBCMP = 0;

 for(int i = 0; i < 4; i++)
 {
  SPT[i] = 0;
  GPLT[i] = 0;
  JPLT[i] = 0;

  Recalc_GPLT_Cache(i);
  Recalc_JPLT_Cache(i);
 }

 BKCOL = 0;
}

static INLINE uint16 ReadRegister(int32 &timestamp, uint32 A)
{
 uint16 ret = 0;	//0xFFFF;

 if(A & 1)
  VIP_DBGMSG("Misaligned VIP Read: %08x", A);

 switch(A & 0xFE)
 {
  default: VIP_DBGMSG("Unknown VIP register read: %08x", A);
	   break;

  case 0x00: ret = InterruptPending;
	     break;

  case 0x02: ret = InterruptEnable;
	     break;

  case 0x20: //printf("Read DPSTTS at %d\n", timestamp);
	     ret = DPCTRL & 0x702;
	     if((DisplayRegion & 1) && DisplayActive)
	     {
	      unsigned int DPBSY = 1 << ((DisplayRegion >> 1) & 1);

	      if(DisplayFB)
	       DPBSY <<= 2;

	      ret |= DPBSY << 2;
	     }
	     //if(!(DisplayRegion & 1))	// FIXME? (Had to do it this way for Galactic Pinball...)
              ret |= 1 << 6;
	     break;

  // Note: Upper bits of BRTA, BRTB, BRTC, and REST(?) are 0 when read(on real hardware)
  case 0x24: ret = BRTA;
             break;

  case 0x26: ret = BRTB;
             break;

  case 0x28: ret = BRTC;
             break;

  case 0x2A: ret = REST;
             break;

  case 0x30: ret = 0xFFFF;
	     break;

  case 0x40: ret = XPCTRL & 0x2;
	     if(DrawingActive)
	     {
	      ret |= (1 + DrawingFB) << 2;
	     }
	     if(timestamp < SBOUT_InactiveTime)
	     {
	      ret |= 0x8000;
	      ret |= /*DrawingBlock*/SB_Latch << 8;
	     }
	     break;     // XPSTTS, read-only

  case 0x48:
  case 0x4a:
  case 0x4c:
  case 0x4e: ret = SPT[(A >> 1) & 3];
             break;

  case 0x60:
  case 0x62:
  case 0x64:
  case 0x66: ret = GPLT[(A >> 1) & 3];
             break;

  case 0x68:
  case 0x6a:
  case 0x6c:
  case 0x6e: ret = JPLT[(A >> 1) & 3];
             break;

  case 0x70: ret = BKCOL;
             break;
 }

 return(ret);
}

static INLINE void WriteRegister(int32 &timestamp, uint32 A, uint16 V)
{
 if(A & 1)
  VIP_DBGMSG("Misaligned VIP Write: %08x %04x", A, V);

 switch(A & 0xFE)
 {
  default: VIP_DBGMSG("Unknown VIP register write: %08x %04x", A, V);
           break;

  case 0x00: break; // Interrupt pending, read-only

  case 0x02: {
	      InterruptEnable = V & 0xE01F;

	      VIP_DBGMSG("Interrupt Enable: %04x", V);

	      if(V & 0x2000)
	       VIP_DBGMSG("Warning: VIP SB Hit Interrupt enable: %04x\n", V);
	      CheckIRQ();
	     }
	     break;

  case 0x04: InterruptPending &= ~V;
	     CheckIRQ();
	     break;

  case 0x20: break; // Display control, read-only.

  case 0x22: DPCTRL = V & (0x703); // Display-control, write-only
	     if(V & 1)
	     {
	      DisplayActive = false;
	      InterruptPending &= ~(INT_TIME_ERR | INT_FRAME_START | INT_GAME_START | INT_RFB_END | INT_LFB_END | INT_SCAN_ERR);
	      CheckIRQ();
	     }
	     break;

  case 0x24: BRTA = V & 0xFF;	// BRTA
	     RecalcBrightnessCache();
	     break;

  case 0x26: BRTB = V & 0xFF;	// BRTB
	     RecalcBrightnessCache();
	     break;

  case 0x28: BRTC = V & 0xFF;	// BRTC
	     RecalcBrightnessCache();
	     break;

  case 0x2A: REST = V & 0xFF;	// REST
	     RecalcBrightnessCache();
	     break;

  case 0x2E: FRMCYC = V & 0xF;	// FRMCYC, write-only?
	     break;

  case 0x30: break;	// CTA, read-only(

  case 0x40: break;	// XPSTTS, read-only

  case 0x42: XPCTRL = V & 0x0002;	// XPCTRL, write-only
	     SBCMP = (V >> 8) & 0x1F;

	     if(V & 1)
	     {
	      VIP_DBGMSG("XPRST");
	      DrawingActive = 0;
	      DrawingCounter = 0;
              InterruptPending &= ~(INT_SB_HIT | INT_XP_END | INT_TIME_ERR);
	      CheckIRQ();
	     }
	     break;

  case 0x44: break;	// Version Control, read-only?

  case 0x48:
  case 0x4a:
  case 0x4c:
  case 0x4e: SPT[(A >> 1) & 3] = V & 0x3FF;
	     break;

  case 0x60:
  case 0x62: 
  case 0x64:
  case 0x66: GPLT[(A >> 1) & 3] = V & 0xFC;
	     Recalc_GPLT_Cache((A >> 1) & 3);
	     break;

  case 0x68:
  case 0x6a:
  case 0x6c:
  case 0x6e: JPLT[(A >> 1) & 3] = V & 0xFC;
             Recalc_JPLT_Cache((A >> 1) & 3);
             break;

  case 0x70: BKCOL = V & 0x3;
	     break;

 }
}

//
// Don't update the VIP state on reads/writes, the event system will update it with enough precision as far as VB software cares.
//

uint8 VIP_Read8(int32 &timestamp, uint32 A)
{
 uint8 ret = 0; //0xFF;

 //VIP_Update(timestamp);

 switch(A >> 16)
 {
  case 0x0:
  case 0x1:
           if((A & 0x7FFF) >= 0x6000)
           {
            ret = CHR_RAM[(A & 0x1FFF) | ((A >> 2) & 0x6000)];
           }
           else
           {
            ret = FB[(A >> 15) & 1][(A >> 16) & 1][A & 0x7FFF];
           }
           break;

  case 0x2:
  case 0x3: ret = DRAM[A & 0x1FFFF];
            break;

  case 0x4:
  case 0x5: if(A >= 0x5E000)
	     ret = (uint8)(ReadRegister(timestamp, A)&0xFF);
	    else
	     VIP_DBGMSG("Unknown VIP Read: %08x", A);
            break;

  case 0x6: break;

  case 0x7: if(A >= 0x8000)
            {
             ret = CHR_RAM[A & 0x7FFF];
            }
	    else
	     VIP_DBGMSG("Unknown VIP Read: %08x", A);
            break;

  default: VIP_DBGMSG("Unknown VIP Read: %08x", A);
	   break;
 }


 //VB_SetEvent(VB_EVENT_VIP, timestamp + CalcNextEvent());

 return(ret);
}

uint16 VIP_Read16(int32 &timestamp, uint32 A)
{
 uint16 ret = 0; //0xFFFF;

 //VIP_Update(timestamp); 

 switch(A >> 16)
 {
  case 0x0:
  case 0x1:
           if((A & 0x7FFF) >= 0x6000)
           {
            ret = le16toh(*(uint16 *)&CHR_RAM[(A & 0x1FFF) | ((A >> 2) & 0x6000)]);
           }
           else
           {
            ret = le16toh(*(uint16 *)&FB[(A >> 15) & 1][(A >> 16) & 1][A & 0x7FFF]);
           }
           break;

  case 0x2:
  case 0x3: ret = le16toh(*(uint16 *)&DRAM[A & 0x1FFFF]);
            break;

  case 0x4:
  case 0x5: 
	    if(A >= 0x5E000)
	     ret = ReadRegister(timestamp, A);
            else
             VIP_DBGMSG("Unknown VIP Read: %08x", A);
            break;

  case 0x6: break;

  case 0x7: if(A >= 0x8000)
            {
             ret = le16toh(*(uint16 *)&CHR_RAM[A & 0x7FFF]);
            }
	    else
	     VIP_DBGMSG("Unknown VIP Read: %08x", A);
            break;

  default: VIP_DBGMSG("Unknown VIP Read: %08x", A);
           break;
 }


 //VB_SetEvent(VB_EVENT_VIP, timestamp + CalcNextEvent());
 return(ret);
}

void VIP_Write8(int32 &timestamp, uint32 A, uint8 V)
{
 //VIP_Update(timestamp); 

 //if(A >= 0x3DC00 && A < 0x3E000)
 // printf("%08x %02x\n", A, V);

 switch(A >> 16)
 {
  case 0x0:
  case 0x1:
	   if((A & 0x7FFF) >= 0x6000)
	    CHR_RAM[(A & 0x1FFF) | ((A >> 2) & 0x6000)] = V;
	   else
	    FB[(A >> 15) & 1][(A >> 16) & 1][A & 0x7FFF] = V;
	   break;

  case 0x2:
  case 0x3: DRAM[A & 0x1FFFF] = V;
	    break;

  case 0x4:
  case 0x5: if(A >= 0x5E000)
 	     WriteRegister(timestamp, A, V);
            else
             VIP_DBGMSG("Unknown VIP Write: %08x %02x", A, V);
	    break;

  case 0x6: VIP_DBGMSG("Unknown VIP Write: %08x %02x", A, V);
	    break;

  case 0x7: if(A >= 0x8000)
	     CHR_RAM[A & 0x7FFF] = V;
	    else
	     VIP_DBGMSG("Unknown VIP Write: %08x %02x", A, V);
	    break;

  default: VIP_DBGMSG("Unknown VIP Write: %08x %02x", A, V);
           break;
 }

 //VB_SetEvent(VB_EVENT_VIP, timestamp + CalcNextEvent());
}

void VIP_Write16(int32 &timestamp, uint32 A, uint16 V)
{
 //VIP_Update(timestamp); 

 //if(A >= 0x3DC00 && A < 0x3E000)
 // printf("%08x %04x\n", A, V);

 switch(A >> 16)
 {
  case 0x0:
  case 0x1:
           if((A & 0x7FFF) >= 0x6000)
            *(uint16 *)&CHR_RAM[(A & 0x1FFF) | ((A >> 2) & 0x6000)] = htole16(V);
           else
            *(uint16 *)&FB[(A >> 15) & 1][(A >> 16) & 1][A & 0x7FFF] = htole16(V);
           break;

  case 0x2:
  case 0x3: *(uint16 *)&DRAM[A & 0x1FFFF] = htole16(V);
            break;

  case 0x4:
  case 0x5: if(A >= 0x5E000)
 	     WriteRegister(timestamp, A, V);
            else
             VIP_DBGMSG("Unknown VIP Write: %08x %04x", A, V);
            break;

  case 0x6: VIP_DBGMSG("Unknown VIP Write: %08x %04x", A, V);
	    break;

  case 0x7: if(A >= 0x8000)
             *(uint16 *)&CHR_RAM[A & 0x7FFF] = htole16(V);
	    else
	     VIP_DBGMSG("Unknown VIP Write: %08x %04x", A, V);
            break;

  default: VIP_DBGMSG("Unknown VIP Write: %08x %04x", A, V);
           break;
 }


 //VB_SetEvent(VB_EVENT_VIP, timestamp + CalcNextEvent());
}

static MDFN_Surface *surface;
static MDFN_Surface *surface2;
static bool skip;
static bool recalculatemode = true;

uint32 VIP_GetColorMode() {
 return VBColorMode;
}

void VIP_SetColorMode(uint32 mode) {

 VBColorMode = mode;
 
 VIP_SetAnaglyphColors(AnaglyphPreset_Colors[VBColorMode][0], AnaglyphPreset_Colors[VBColorMode][1]);
 
 recalculatemode=true;
}

void VIP_StartFrame(EmulateSpecStruct *espec)
{
// puts("Start frame");
//	

 if(recalculatemode || espec->VideoFormatChanged)
 {
	 if(recalculatemode)
		 recalculatemode = false;
  MakeColorLUT(espec->surface->format);
  Recalc3DModeStuff(espec->surface->format.colorspace != MDFN_COLORSPACE_RGB);
 }

 #if 0
 puts("\n\n\nMeow:");
 for(int i = 0x1DC00; i < 0x1DE00; i += 2)
  printf("%04x\n", *(uint16 *)&DRAM[i]);
 #endif

 espec->DisplayRect.x = 0;
 espec->DisplayRect.y = 0;

 switch(VB3DMode)
 {
  default:
	espec->DisplayRect.w = 384; //PBarrier ? 768 : 384;
	espec->DisplayRect.h = 224;
	break;

  case VB3DMODE_PBARRIER:
	espec->DisplayRect.w = 768;
	espec->DisplayRect.h = 224;
	break;

  case VB3DMODE_CSCOPE:
	espec->DisplayRect.w = 512;
	espec->DisplayRect.h = 384;
	break;

  case VB3DMODE_SIDEBYSIDE:
	espec->DisplayRect.w = 768 + SideBySideSep;	//768;
	espec->DisplayRect.h = 224;
	break;
 }

 surface = espec->surface;
 surface2 = espec->surface2;
 skip = (bool)(espec->skip);
}

void VIP_ResetTS(void)
{
 if(SBOUT_InactiveTime >= 0)
  SBOUT_InactiveTime -= last_ts;
 last_ts = 0;
}

static int32 CalcNextEvent(void)
{
 return(ColumnCounter);
}

#include "vip_draw.inc"

//For debugging purposes, you can paste the contents of vip_draw.inc into the below space,
//comment out the include temporarily, and use the IDE to work on it. After you finish,
//you can cut it out and overwrite the original file contents.
//#include "vip_draw.inc" //begin

//#include "vip_draw.inc" //end

static INLINE void CopyFBColumnToTarget_Anaglyph_BASE(const bool DisplayActive_arg, const int lr)
{
	const int fb = DisplayFB;
	uint32 *target = surface->pixels + Column;
	const int32 pitch32 = surface->pitch32;
	const uint8 *fb_source = &FB[fb][lr][64 * Column];

	for(int y = 56; y; y--)
	{
		uint32 source_bits = *fb_source;

		for(int y_sub = 4; y_sub; y_sub--)
		{
			uint32 pixel = BrightCLUT[lr][source_bits & 3];

			if(!DisplayActive_arg)
				pixel = 0;

			if(lr)
				*target |= pixel;
			else
				*target = pixel;

			source_bits >>= 2;
			target += pitch32;
		}
		fb_source++;


	}
}

static void CopyFBColumnToTarget_Anaglyph(int SideBySideSep)
{
 const int lr = (DisplayRegion & 2) >> 1;

 if(!DisplayActive)
 {
  if(!lr)
   CopyFBColumnToTarget_Anaglyph_BASE(0, 0);
  else
   CopyFBColumnToTarget_Anaglyph_BASE(0, 1);
 }
 else
 {
  if(!lr)
   CopyFBColumnToTarget_Anaglyph_BASE(1, 0);
  else
   CopyFBColumnToTarget_Anaglyph_BASE(1, 1);
 }
}

static uint32 AnaSlowBuf[384][224];

static INLINE void CopyFBColumnToTarget_AnaglyphSlow_BASE(const bool DisplayActive_arg, const int lr)
{
     const int fb = DisplayFB;
     const uint8 *fb_source = &FB[fb][lr][64 * Column];

     if(!lr)
     {
      uint32 *target = AnaSlowBuf[Column];

      for(int y = 56; y; y--)
      {
       uint32 source_bits = *fb_source;

       for(int y_sub = 4; y_sub; y_sub--)
       {
        uint32 pixel = BrightnessCache[source_bits & 3];

        if(!DisplayActive_arg)
         pixel = 0;

        *target = pixel;
        source_bits >>= 2;
        target++;
       }
       fb_source++;
      }

     }
     else
     {
      uint32 *target = surface->pixels + Column;
      const uint32 *left_src = AnaSlowBuf[Column];
      const int32 pitch32 = surface->pitch32;

      for(int y = 56; y; y--)
      {
       uint32 source_bits = *fb_source;

       for(int y_sub = 4; y_sub; y_sub--)
       {
        uint32 pixel = AnaSlowColorLUT[*left_src][DisplayActive_arg ? BrightnessCache[source_bits & 3] : 0];

        *target = pixel;

        source_bits >>= 2;
        target += pitch32;
        left_src++;
       }
       fb_source++;
      }
     }
}

static void CopyFBColumnToTarget_AnaglyphSlow(int SideBySideSep)
{
 const int lr = (DisplayRegion & 2) >> 1;

 if(!DisplayActive)
 {
  if(!lr)
   CopyFBColumnToTarget_AnaglyphSlow_BASE(0, 0);
  else
   CopyFBColumnToTarget_AnaglyphSlow_BASE(0, 1);
 }
 else
 {
  if(!lr)
   CopyFBColumnToTarget_AnaglyphSlow_BASE(1, 0);
  else
   CopyFBColumnToTarget_AnaglyphSlow_BASE(1, 1);
 }
}


static void CopyFBColumnToTarget_CScope_BASE(const bool DisplayActive_arg, const int lr)
{
     const int fb = DisplayFB;
     uint32 *target = surface->pixels + (lr ? 512 - 16 - 1 : 16) + (lr ? Column : 383 - Column) * surface->pitch32;
     const uint8 *fb_source = &FB[fb][lr][64 * Column];

     for(int y = 56; y; y--)
     {
      uint32 source_bits = *fb_source;

      for(int y_sub = 4; y_sub; y_sub--)
      {
       if(DisplayActive_arg)
        *target = BrightCLUT[lr][source_bits & 3];
       else
	*target = 0;

       source_bits >>= 2;
       if(lr)
        target--;
       else
	target++;
      }
      fb_source++;
     }
}

static void CopyFBColumnToTarget_CScope(int SideBySideSep)
{
 const int lr = (DisplayRegion & 2) >> 1;

 if(!DisplayActive)
 {
  if(!lr)
   CopyFBColumnToTarget_CScope_BASE(0, 0);
  else
   CopyFBColumnToTarget_CScope_BASE(0, 1);
 }
 else
 {
  if(!lr)
   CopyFBColumnToTarget_CScope_BASE(1, 0);
  else
   CopyFBColumnToTarget_CScope_BASE(1, 1);
 }
}

static void CopyFBColumnToTarget_SideBySide_BASE(const bool DisplayActive_arg, const int lr, int SideBySideSep)
{
     const int fb = DisplayFB;
	 // 400 = 384 + 16
	 // 480 = 384 + 96
	 // 864 = (384 * 2) + 96

	 MDFN_Surface* eyeSurface = surface;

     uint32 *target = eyeSurface->pixels + Column + (lr ? 384 + SideBySideSep : 0);
     const int32 pitch32 = eyeSurface->pitch32;
     const uint8 *fb_source = &FB[fb][lr][64 * Column];

     for(int y = 56; y; y--)
     {
      uint32 source_bits = *fb_source;

      for(int y_sub = 4; y_sub; y_sub--)
      {
       if(DisplayActive_arg)
        *target = BrightCLUT[lr][source_bits & 3];
       else
	*target = 0;
       source_bits >>= 2;
       target += pitch32;
      }
      fb_source++;
     }
}

static void CopyFBColumnToTarget_SideBySide(int SideBySideSep)
{
 const int lr = (DisplayRegion & 2) >> 1;

 if(!DisplayActive)
 {
  if(!lr)
   CopyFBColumnToTarget_SideBySide_BASE(0, 0, SideBySideSep);
  else
   CopyFBColumnToTarget_SideBySide_BASE(0, 1, SideBySideSep);
 }
 else
 {
  if(!lr)
   CopyFBColumnToTarget_SideBySide_BASE(1, 0, SideBySideSep);
  else
   CopyFBColumnToTarget_SideBySide_BASE(1, 1, SideBySideSep);
 }
}

static void CopyFBColumnToTarget_OVR_BASE(const bool DisplayActive_arg, const int lr)
{
	const int fb = DisplayFB;
	// 400 = 384 + 16
	// 480 = 384 + 96
	// 864 = (384 * 2) + 96

	MDFN_Surface* eyeSurface = lr ? surface : surface2;

	uint32 *target = eyeSurface->pixels + Column;
	const int32 pitch32 = eyeSurface->pitch32;
	const uint8 *fb_source = &FB[fb][lr][64 * Column];

	for(int y = 56; y; y--)
	{
		uint32 source_bits = *fb_source;

		for(int y_sub = 4; y_sub; y_sub--)
		{
			if(DisplayActive_arg)
				*target = BrightCLUT[lr][source_bits & 3];
			else
				*target = 0;
			source_bits >>= 2;
			target += pitch32;
		}
		fb_source++;
	}
}

static void CopyFBColumnToTarget_OVR(int SideBySideSep)
{
	const int lr = (DisplayRegion & 2) >> 1;

	if(!DisplayActive)
	{
		if(!lr)
			CopyFBColumnToTarget_OVR_BASE(0, 0);
		else
			CopyFBColumnToTarget_OVR_BASE(0, 1);
	}
	else
	{
		if(!lr)
			CopyFBColumnToTarget_OVR_BASE(1, 0);
		else
			CopyFBColumnToTarget_OVR_BASE(1, 1);
	}
}

static INLINE void CopyFBColumnToTarget_PBarrier_BASE(const bool DisplayActive_arg, const int lr)
{
     const int fb = DisplayFB;
     uint32 *target = surface->pixels + Column * 2 + lr;
     const int32 pitch32 = surface->pitch32;
     const uint8 *fb_source = &FB[fb][lr][64 * Column];

     for(int y = 56; y; y--)
     {
      uint32 source_bits = *fb_source;

      for(int y_sub = 4; y_sub; y_sub--)
      {
       if(DisplayActive_arg)
        *target = BrightCLUT[0][source_bits & 3];
       else
        *target = 0;

       source_bits >>= 2;
       target += pitch32;
      }
      fb_source++;
     }
}

static void CopyFBColumnToTarget_PBarrier(int SideBySideSep)
{
 const int lr = (DisplayRegion & 2) >> 1;

 if(!DisplayActive)
 {
  if(!lr)
   CopyFBColumnToTarget_PBarrier_BASE(0, 0);
  else
   CopyFBColumnToTarget_PBarrier_BASE(0, 1);
 }
 else
 {
  if(!lr)
   CopyFBColumnToTarget_PBarrier_BASE(1, 0);
  else
   CopyFBColumnToTarget_PBarrier_BASE(1, 1);
 }
}


v810_timestamp_t MDFN_FASTCALL VIP_Update(const v810_timestamp_t timestamp)
{
 int32 clocks = timestamp - last_ts;
 int32 running_timestamp = timestamp;

 while(clocks > 0)
 {
  int32 chunk_clocks = clocks;

  if (DisplayLeftRightOutputInternal == 3) {
	DrawingCounter = 0;
	skip=true;
  }

  if(DrawingCounter > 0 && chunk_clocks > DrawingCounter)
   chunk_clocks = DrawingCounter;
  if(chunk_clocks > ColumnCounter)
   chunk_clocks = ColumnCounter;

  running_timestamp += chunk_clocks;

 if(DrawingCounter > 0)
  {
   DrawingCounter -= chunk_clocks;
   if(DrawingCounter <= 0)
   {
    __declspec(align(8)) uint8 DrawingBuffers[2][512 * 8] ;// __attribute__((aligned(8)));	// Don't decrease this from 512 unless you adjust vip_draw.inc(including areas that draw off-visible >= 384 and >= -7 for speed reasons)

    VIP_DrawBlock(DrawingBlock, DrawingBuffers[0] + 8, DrawingBuffers[1] + 8);

	/*
	for(int zz = 0; zz < 512 * 8; zz++) {
		if(DrawingBuffers[0][zz] != 0 || DrawingBuffers[1][zz] != 0)
			printf("wtf");
	}
	*/
    int lrtemp;

    for(int lr = 0; lr < 2; lr++)
    {
     uint8 *FB_Target = FB[DrawingFB][lr] + DrawingBlock * 2;

	 if (DisplayLeftRightOutputInternal == 1)
	  lrtemp = 0;
	 else if (DisplayLeftRightOutputInternal == 2)
	  lrtemp = 1;
	 else
	  lrtemp = lr;

     for(int x = 0; x < 384; x++)
     {
      FB_Target[64 * x + 0] = (DrawingBuffers[lrtemp][8 + x + 512 * 0] << 0)
				  | (DrawingBuffers[lrtemp][8 + x + 512 * 1] << 2)
				  | (DrawingBuffers[lrtemp][8 + x + 512 * 2] << 4)
				  | (DrawingBuffers[lrtemp][8 + x + 512 * 3] << 6);

      FB_Target[64 * x + 1] = (DrawingBuffers[lrtemp][8 + x + 512 * 4] << 0) 
                                  | (DrawingBuffers[lrtemp][8 + x + 512 * 5] << 2)
                                  | (DrawingBuffers[lrtemp][8 + x + 512 * 6] << 4) 
                                  | (DrawingBuffers[lrtemp][8 + x + 512 * 7] << 6);

//	  if(FB_Target[64 * x + 0] != 0 || FB_Target[64 * x + 1] != 0)
//		  printf("wtf");


     }
    }

    SBOUT_InactiveTime = running_timestamp + 1120;
    SB_Latch = DrawingBlock;	// Not exactly correct, but probably doesn't matter.

    DrawingBlock++;
    if(DrawingBlock == 28)
    {
     DrawingActive = false;

     InterruptPending |= INT_XP_END;
     CheckIRQ();
    }
    else
     DrawingCounter += 1120 * 4;
   }
  }

  ColumnCounter -= chunk_clocks;
  if(ColumnCounter == 0)
  {
   if(DisplayRegion & 1)
   {
    if(!(Column & 3))
    {
     const int lr = (DisplayRegion & 2) >> 1;
     uint16 ctdata = *(uint16 *)&DRAM[0x1DFFE - ((Column >> 2) * 2) - (lr ? 0 : 0x200)];

     if((ctdata >> 8) != Repeat)
     {
      Repeat = ctdata >> 8;
      RecalcBrightnessCache();
     }
    }
    if(!skip)
     CopyFBColumnToTarget(SideBySideSep);
   }

   ColumnCounter = 259;
   Column++;
   if(Column == 384)
   {
    Column = 0;

    if(DisplayActive)
    {
     if(DisplayRegion & 1)	// Did we just finish displaying an active region?
     {
      if(DisplayRegion & 2)	// finished displaying right eye
       InterruptPending |= INT_RFB_END;
      else		// Otherwise, left eye
       InterruptPending |= INT_LFB_END;

      CheckIRQ();
     }
    }

    DisplayRegion = (DisplayRegion + 1) & 3;

    if(DisplayRegion == 0)	// New frame start
    {
     DisplayActive = (bool)(DPCTRL & 0x2);

     if(DisplayActive)
     {
      InterruptPending |= INT_FRAME_START;
      CheckIRQ();
     }
     GameFrameCounter++;
     if(GameFrameCounter > FRMCYC) // New game frame start?
     {
      InterruptPending |= INT_GAME_START;
      CheckIRQ();

      if(XPCTRL & XPCTRL_XP_EN)
      {
       DisplayFB ^= 1;

       DrawingBlock = 0;
       DrawingActive = true;
       DrawingCounter = 1120 * 4;
       DrawingFB = DisplayFB ^ 1;
      }

      GameFrameCounter = 0;
     }
     VB_ExitLoop();
    }
   }
  }

  clocks -= chunk_clocks;
 }

 last_ts = timestamp;

 return(timestamp + CalcNextEvent());
}


int VIP_StateAction(StateMem *sm, int load, int data_only)
{
 SFORMAT StateRegs[] =
 {
  SFARRAY(FB[0][0], 0x6000 * 2 * 2),
  SFARRAY(CHR_RAM, 0x8000),
  SFARRAY(DRAM, 0x20000),

  SFVAR(InterruptPending),
  SFVAR(InterruptEnable),

  SFVAR(BRTA),
  SFVAR(BRTB), 
  SFVAR(BRTC),
  SFVAR(REST),

  SFVAR(FRMCYC),
  SFVAR(DPCTRL),

  SFVAR(DisplayActive),

  SFVAR(XPCTRL),
  SFVAR(SBCMP),
  SFARRAY16(SPT, 4),
  SFARRAY16(GPLT, 4),	// FIXME
  SFARRAY16(JPLT, 4),
  
  SFVAR(BKCOL),

  SFVAR(Column),
  SFVAR(ColumnCounter),

  SFVAR(DisplayRegion),
  SFVAR(DisplayFB),
  
  SFVAR(GameFrameCounter),

  SFVAR(DrawingCounter),

  SFVAR(DrawingActive),
  SFVAR(DrawingFB),
  SFVAR(DrawingBlock),

  SFVAR(SB_Latch),
  SFVAR(SBOUT_InactiveTime),

  SFVAR(Repeat),
  SFEND
 };

 int ret = MDFNSS_StateAction(sm, load, data_only, StateRegs, "VIP");

 if(load)
 {
  RecalcBrightnessCache();
  for(int i = 0; i < 4; i++)
  {
   Recalc_GPLT_Cache(i);
   Recalc_JPLT_Cache(i);
  }
 }

 return(ret);
}

uint8* getDRAM() {
	return DRAM;
}

}

