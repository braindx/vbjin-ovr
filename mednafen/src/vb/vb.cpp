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
#include "timer.h"
#include "vsu.h"
#include "vip.h"
//#include "../netplay.h"
#include "debug.h"
#include "input.h"
#include "general.h"
#include "md5.h"
#include "mempatcher.h"
//#include <iconv.h>
#include "endian.h"
#include "file.h"
#include "mednafen.h"

namespace MDFN_IEN_VB
{

enum
{
 ANAGLYPH_PRESET_DISABLED = 0,
 ANAGLYPH_PRESET_RED_BLUE,
 ANAGLYPH_PRESET_RED_CYAN,
 ANAGLYPH_PRESET_RED_ELECTRICCYAN,
 ANAGLYPH_PRESET_RED_GREEN,
 ANAGLYPH_PRESET_GREEN_MAGENTA,
 ANAGLYPH_PRESET_YELLOW_BLUE,
};

static uint32 VB3DMode;

static Blip_Buffer sbuf[2];

static uint8 *WRAM = NULL;

static uint8 *GPRAM = NULL;
static uint32 GPRAM_Mask;

void clearGPRAM(){
memset(GPRAM, 0, GPRAM_Mask + 1);
}

static uint8 *GPROM = NULL;
static uint32 GPROM_Mask;

V810 *VB_V810 = NULL;

static VSU *VB_VSU = NULL;
static uint32 VSU_CycleFix;

static uint8 WCR;

static int32 next_vip_ts, next_timer_ts, next_input_ts;


static uint32 IRQ_Asserted;

static INLINE void RecalcIntLevel(void)
{
 int ilevel = -1;

 for(int i = 4; i >= 0; i--)
 {
  if(IRQ_Asserted & (1 << i))
  {
   ilevel = i;
   break;
  }
 }

 VB_V810->SetInt(ilevel);
}

void VBIRQ_Assert(int source, bool assert)
{
 assert(source >= 0 && source <= 4);

 IRQ_Asserted &= ~(1 << source);

 if(assert)
  IRQ_Asserted |= 1 << source;
 
 RecalcIntLevel();
}



static uint8 HWCTRL_Read(v810_timestamp_t &timestamp, uint32 A)
{
 uint8 ret = 0;

 if(A & 0x3)
 { 
  puts("HWCtrl Bogus Read?");
  return(ret);
 }

 switch(A & 0xFF)
 {
  default: printf("Unknown HWCTRL Read: %08x\n", A);
	   break;

  case 0x18:
  case 0x1C:
  case 0x20: ret = TIMER_Read(timestamp, A);
	     break;

  case 0x24: ret = WCR | 0xFC;
	     break;

  case 0x10:
  case 0x14:
  case 0x28: ret = VBINPUT_Read(timestamp, A);
             break;

 }

 return(ret);
}

static void HWCTRL_Write(v810_timestamp_t &timestamp, uint32 A, uint8 V)
{
 if(A & 0x3)
 {
  puts("HWCtrl Bogus Write?");
  return;
 }

 switch(A & 0xFF)
 {
  default: printf("Unknown HWCTRL Write: %08x %02x\n", A, V);
           break;

  case 0x18:
  case 0x1C:
  case 0x20: TIMER_Write(timestamp, A, V);
	     break;

  case 0x24: WCR = V & 0x3;
	     break;

  case 0x10:
  case 0x14:
  case 0x28: VBINPUT_Write(timestamp, A, V);
	     break;
 }
}

uint8 MDFN_FASTCALL MemRead8(v810_timestamp_t &timestamp, uint32 A)
{
 uint8 ret = 0;
 A &= (1 << 27) - 1;

 //if((A >> 24) <= 2)
 // printf("Read8: %d %08x\n", timestamp, A);

 switch(A >> 24)
 {
  case 0: ret = VIP_Read8(timestamp, A);
	  break;

  case 1: break;

  case 2: ret = HWCTRL_Read(timestamp, A);
	  break;

  case 3: break;
  case 4: break;

  case 5: ret = WRAM[A & 0xFFFF];
	  break;

  case 6: if(GPRAM)
	   ret = GPRAM[A & GPRAM_Mask];
	  else
	   printf("GPRAM(Unmapped) Read: %08x\n", A);
	  break;

  case 7: ret = GPROM[A & GPROM_Mask];
	  break;
 }
 return(ret);
}

uint16 MDFN_FASTCALL MemRead16(v810_timestamp_t &timestamp, uint32 A)
{
 uint16 ret = 0;

 A &= (1 << 27) - 1;

 //if((A >> 24) <= 2)
 // printf("Read16: %d %08x\n", timestamp, A);


 switch(A >> 24)
 {
  case 0: ret = VIP_Read16(timestamp, A);
	  break;

  case 1: break;

  case 2: ret = HWCTRL_Read(timestamp, A);
	  break;

  case 3: break;

  case 4: break;

  case 5: ret = le16toh(*(uint16 *)&WRAM[A & 0xFFFF]);
	  break;

  case 6: if(GPRAM)
           ret = le16toh(*(uint16 *)&GPRAM[A & GPRAM_Mask]);
	  else printf("GPRAM(Unmapped) Read: %08x\n", A);
	  break;

  case 7: ret = le16toh(*(uint16 *)&GPROM[A & GPROM_Mask]);
	  break;
 }
 return(ret);
}

void MDFN_FASTCALL MemWrite8(v810_timestamp_t &timestamp, uint32 A, uint8 V)
{
 A &= (1 << 27) - 1;

 //if((A >> 24) <= 2)
 // printf("Write8: %d %08x %02x\n", timestamp, A, V);

 switch(A >> 24)
 {
  case 0: VIP_Write8(timestamp, A, V);
          break;

  case 1: VB_VSU->Write((timestamp + VSU_CycleFix) >> 2, A, V);
          break;

  case 2: HWCTRL_Write(timestamp, A, V);
          break;

  case 3: break;

  case 4: break;

  case 5: WRAM[A & 0xFFFF] = V;
          break;

  case 6: if(GPRAM)
           GPRAM[A & GPRAM_Mask] = V;
          break;

  case 7: // ROM, no writing allowed!
          break;
 }
}

void MDFN_FASTCALL MemWrite16(v810_timestamp_t &timestamp, uint32 A, uint16 V)
{
 A &= (1 << 27) - 1;

 //if((A >> 24) <= 2)
 // printf("Write16: %d %08x %04x\n", timestamp, A, V);

 switch(A >> 24)
 {
  case 0: VIP_Write16(timestamp, A, V);
          break;

  case 1: VB_VSU->Write((timestamp + VSU_CycleFix) >> 2, A, (uint8)(V&0xFF));
          break;

  case 2: HWCTRL_Write(timestamp, A, (uint8)(V&0xFF));
          break;

  case 3: break;

  case 4: break;

  case 5: *(uint16 *)&WRAM[A & 0xFFFF] = htole16(V);
          break;

  case 6: if(GPRAM)
           *(uint16 *)&GPRAM[A & GPRAM_Mask] = htole16(V);
          break;

  case 7: // ROM, no writing allowed!
          break;
 }
}

static void FixNonEvents(void)
{
 if(next_vip_ts & 0x40000000)
  next_vip_ts = VB_EVENT_NONONO;

 if(next_timer_ts & 0x40000000)
  next_timer_ts = VB_EVENT_NONONO;

 if(next_input_ts & 0x40000000)
  next_input_ts = VB_EVENT_NONONO;
}

static void EventReset(void)
{
 next_vip_ts = VB_EVENT_NONONO;
 next_timer_ts = VB_EVENT_NONONO;
 next_input_ts = VB_EVENT_NONONO;
}

static INLINE int32 CalcNextTS(void)
{
 int32 next_timestamp = next_vip_ts;

 if(next_timestamp > next_timer_ts)
  next_timestamp  = next_timer_ts;

 if(next_timestamp > next_input_ts)
  next_timestamp  = next_input_ts;

 return(next_timestamp);
}

static void RebaseTS(const v810_timestamp_t timestamp)
{
 //printf("Rebase: %08x %08x %08x\n", timestamp, next_vip_ts, next_timer_ts);

 assert(next_vip_ts > timestamp);
 assert(next_timer_ts > timestamp);
 assert(next_input_ts > timestamp);

 next_vip_ts -= timestamp;
 next_timer_ts -= timestamp;
 next_input_ts -= timestamp;
}

void VB_SetEvent(const int type, const v810_timestamp_t next_timestamp)
{
 assert(next_timestamp > VB_V810->v810_timestamp);

 if(type == VB_EVENT_VIP)
  next_vip_ts = next_timestamp;
 else if(type == VB_EVENT_TIMER)
  next_timer_ts = next_timestamp;
 else if(type == VB_EVENT_INPUT)
  next_input_ts = next_timestamp;

 if(next_timestamp < VB_V810->GetEventNT())
  VB_V810->SetEventNT(next_timestamp);
}


static int32 MDFN_FASTCALL EventHandler(const v810_timestamp_t timestamp)
{
 if(timestamp >= next_vip_ts)
  next_vip_ts = VIP_Update(timestamp);

 if(timestamp >= next_timer_ts)
  next_timer_ts = TIMER_Update(timestamp);

 if(timestamp >= next_input_ts)
  next_input_ts = VBINPUT_Update(timestamp);

 return(CalcNextTS());
}

static void ForceEventUpdates(const v810_timestamp_t timestamp)
{
 next_vip_ts = VIP_Update(timestamp);
 next_timer_ts = TIMER_Update(timestamp);
 next_input_ts = VBINPUT_Update(timestamp);

 VB_V810->SetEventNT(CalcNextTS());
 //printf("FEU: %d %d %d\n", next_vip_ts, next_timer_ts, next_input_ts);
}

void VB_Power(void)
{
 memset(WRAM, 0, 65536);

 VIP_Power();
 VB_VSU->Power();
 TIMER_Power();
 VBINPUT_Power();

 EventReset();
 IRQ_Asserted = 0;
 RecalcIntLevel();
 VB_V810->Reset();

 VSU_CycleFix = 0;
 WCR = 0;


 ForceEventUpdates(VB_V810->v810_timestamp);
}

void SetMixVideoOutput(bool disabled) {
	VIP_SetParallaxDisable(disabled);
}

void SetSideBySidePixels(int pixels) {
	VIP_SetSideBySidePixels(pixels);
}

void SetViewDisp(int display) {
	VIP_SetViewDisp(display);
}

uint32 GetSplitMode() {
	return VB3DMode;
}

void SetSplitMode(uint32 mode) {
	VB3DMode = mode;
	VIP_Set3DMode(mode);
}

uint32 GetColorMode() {
	return VIP_GetColorMode();
}

void SetColorMode(uint32 mode) {
	VIP_SetColorMode(mode);
}

static bool TestMagic(const char *name, MDFNFILE *fp)
{
 if(!strcasecmp(fp->ext, "vb") || !strcasecmp(fp->ext, "bin")) // TODO: fixme
  return(true);

 return(false);
}

static int Load(const char *name, MDFNFILE *fp)
{
 V810_Emu_Mode cpu_mode;
 md5_context md5;


 cpu_mode = (V810_Emu_Mode)MDFN_GetSettingI("vb.cpu_emulation");

 if(fp->size != round_up_pow2((uint32)fp->size))
 {
  puts("VB ROM image size is not a power of 2???");
  return(0);
 }

 if(fp->size < 256)
 {
  puts("VB ROM image size is too small??");
  return(0);
 }

 if(fp->size > (1 << 24))
 {
  puts("VB ROM image size is too large??");
  return(0);
 }

 md5.starts();
 md5.update(fp->data, (uint32)fp->size);
 md5.finish(MDFNGameInfo->MD5);

// iconv_t sjis_ict = iconv_open("UTF-8", "shift_jis");
 char game_title[256];
/*
 if(sjis_ict != (iconv_t)-1)
 {
  char *in_ptr, *out_ptr;
  size_t ibl, obl;

  ibl = 20;
  obl = 256;

  in_ptr = (char*)fp->data + (0xFFFFFDE0 & (fp->size - 1));
  out_ptr = game_title;

  iconv(sjis_ict, (ICONV_CONST char **)&in_ptr, &ibl, &out_ptr, &obl);
  iconv_close(sjis_ict);

  *out_ptr = 0;

  MDFN_trim(game_title);
 }
 else*/
  game_title[0] = 0;

 MDFN_printf("Title:     %s\n", game_title);
 MDFN_printf("Game ID Code: %u\n", MDFN_de32lsb(fp->data + (0xFFFFFDFB & (fp->size - 1))));
 MDFN_printf("Manufacturer Code: %d\n", MDFN_de16lsb(fp->data + (0xFFFFFDF9 & (fp->size - 1))));
 MDFN_printf("Version:   %u\n", fp->data[0xFFFFFDFF & (fp->size - 1)]);

 MDFN_printf("ROM:       %dKiB\n", (int)(fp->size / 1024));
 MDFN_printf("ROM MD5:   0x%s\n", md5_context::asciistr(MDFNGameInfo->MD5, 0).c_str());
 
 MDFN_printf("\n");

 MDFN_printf("V810 Emulation Mode: %s\n", (cpu_mode == V810_EMU_MODE_ACCURATE) ? "Accurate" : "Fast");

 VB_V810 = new V810();
 VB_V810->Init(cpu_mode, true);

 VB_V810->SetMemReadHandlers(MemRead8, MemRead16, NULL);
 VB_V810->SetMemWriteHandlers(MemWrite8, MemWrite16, NULL);

 VB_V810->SetIOReadHandlers(MemRead8, MemRead16, NULL);
 VB_V810->SetIOWriteHandlers(MemWrite8, MemWrite16, NULL);

 for(int i = 0; i < 256; i++)
 {
  VB_V810->SetMemReadBus32(i, false);
  VB_V810->SetMemWriteBus32(i, false);
 }

 std::vector<uint32> Map_Addresses;

 for(uint64 A = 0; A < 1ULL << 32; A += (1 << 27))
 {
  for(uint64 sub_A = 5 << 24; sub_A < (6 << 24); sub_A += 65536)
  {
   Map_Addresses.push_back((uint32)(A + sub_A));
  }
 }

 WRAM = VB_V810->SetFastMap(&Map_Addresses[0], 65536, Map_Addresses.size(), "WRAM");
 Map_Addresses.clear();


 // Round up the ROM size to 65536(we mirror it a little later)
 GPROM_Mask = (uint32)((fp->size < 65536) ? (65536 - 1) : (fp->size - 1));

 for(uint64 A = 0; A < 1ULL << 32; A += (1 << 27))
 {
  for(uint64 sub_A = 7 << 24; sub_A < (8 << 24); sub_A += GPROM_Mask + 1)
  {
   Map_Addresses.push_back((uint32)(A + sub_A));
   //printf("%08x\n", (uint32)(A + sub_A));
  }
 }


 GPROM = VB_V810->SetFastMap(&Map_Addresses[0], GPROM_Mask + 1, Map_Addresses.size(), "Cart ROM");
 Map_Addresses.clear();

 // Mirror ROM images < 64KiB to 64KiB
 for(uint64 i = 0; i < 65536; i += fp->size)
 {
  memcpy(GPROM + i, fp->data, (size_t)fp->size);
 }

 GPRAM_Mask = 0xFFFF;

 for(uint64 A = 0; A < 1ULL << 32; A += (1 << 27))
 {
  for(uint64 sub_A = 6 << 24; sub_A < (7 << 24); sub_A += GPRAM_Mask + 1)
  {
   //printf("GPRAM: %08x\n", A + sub_A);
   Map_Addresses.push_back((uint32)(A + sub_A));
  }
 }


 GPRAM = VB_V810->SetFastMap(&Map_Addresses[0], GPRAM_Mask + 1, Map_Addresses.size(), "Cart RAM");
 Map_Addresses.clear();

 memset(GPRAM, 0, GPRAM_Mask + 1);

 {
  gzFile gp = gzopen(MDFN_MakeFName(MDFNMKF_SAV, 0, "sav").c_str(), "rb");

  if(gp)
  {
   if(gzread(gp, GPRAM, 65536) != 65536)
    puts("Error reading GPRAM");
   gzclose(gp);
  }
 }

 //VB3DMode = (uint32)MDFN_GetSettingUI("vb.3dmode");

 VIP_Set3DMode(VB3DMode);

 //VIP_Init();

 //VIP_SetParallaxDisable(MDFN_GetSettingB("vb.disable_parallax"));
 
 /*{
  uint32 lcolor = (uint32)MDFN_GetSettingUI("vb.anaglyph.lcolor"), rcolor = (uint32)MDFN_GetSettingUI("vb.anaglyph.rcolor");
  int preset = (int)MDFN_GetSettingI("vb.anaglyph.preset");

  if(preset != ANAGLYPH_PRESET_DISABLED)
  {
   lcolor = AnaglyphPreset_Colors[preset][0];
   rcolor = AnaglyphPreset_Colors[preset][1];
  }
  VIP_SetAnaglyphColors(lcolor, rcolor);
 }*/
 //VIP_SetDefaultColor((uint32)MDFN_GetSettingUI("vb.default_color"));


 VB_VSU = new VSU(&sbuf[0], &sbuf[1]);

 VBINPUT_Init();
 VBINPUT_SetInstantReadHack(MDFN_GetSettingB("vb.input.instant_read_hack"));

 MDFNGameInfo->fps = (int64)20000000 * 65536 * 256 / (259 * 384 * 4);


 VB_Power();


 #ifdef WANT_DEBUGGER
 VBDBG_Init();
 #endif

//See MDFNGI EmulatedVB declaration for pitch size.
//Use declared pitch for all to stop convert32 errors
 MDFNGameInfo->nominal_width = 384;
 MDFNGameInfo->nominal_height = 224;
 MDFNGameInfo->pitch = 864 * sizeof(uint32);
 MDFNGameInfo->fb_height = 256;

 switch(VB3DMode)
 {
  default: break;

  case VB3DMODE_PBARRIER:
        MDFNGameInfo->nominal_width = 768; //384;	// Which makes more sense to the user?
        MDFNGameInfo->nominal_height = 224;
        MDFNGameInfo->pitch = 864 * sizeof(uint32);
        MDFNGameInfo->fb_height = 512;
        break;

  case VB3DMODE_CSCOPE:
	MDFNGameInfo->nominal_width = 512;
	MDFNGameInfo->nominal_height = 384;
	MDFNGameInfo->pitch = 864 * sizeof(uint32);
	MDFNGameInfo->fb_height = 512;
	break;

  case VB3DMODE_SIDEBYSIDE:
	MDFNGameInfo->nominal_width = 768 + MDFN_IEN_VB::VIP_GetSideBySidePixels();	//768;
  	MDFNGameInfo->nominal_height = 224;
  	MDFNGameInfo->pitch = 864 * sizeof(uint32);	//768 * sizeof(uint32);
 	MDFNGameInfo->fb_height = 256;
	break;
 }

 MDFNMP_Init(32768, ((uint64)1 << 27) / 32768);
 MDFNMP_AddRAM(65536, 5 << 24, WRAM);
 if((GPRAM_Mask + 1) >= 32768)
  MDFNMP_AddRAM(GPRAM_Mask + 1, 6 << 24, GPRAM);
 return(1);
}

static void CloseGame(void)
{
 // Only save cart RAM if it has been modified.
 for(unsigned int i = 0; i < GPRAM_Mask + 1; i++)
 {
  if(GPRAM[i])
  {
   if(!MDFN_DumpToFile(MDFN_MakeFName(MDFNMKF_SAV, 0, "sav").c_str(), 6, GPRAM, 65536))
   {

   }
   break;
  }
 }
 //VIP_Kill();
 
 if(VB_VSU)
 {
  delete VB_VSU;
  VB_VSU = NULL;
 }

 /*
 if(GPRAM)
 {
  MDFN_free(GPRAM);
  GPRAM = NULL;
 }

 if(GPROM)
 {
  MDFN_free(GPROM);
  GPROM = NULL;
 }
 */

 if(VB_V810)
 {
  VB_V810->Kill();
  delete VB_V810;
  VB_V810 = NULL;
 }
}

void VB_ExitLoop(void)
{
 VB_V810->Exit();
}

static void Emulate(EmulateSpecStruct *espec)
{
 v810_timestamp_t v810_timestamp;

 MDFNMP_ApplyPeriodicCheats();

 VBINPUT_Frame();

 VIP_StartFrame(espec);

 v810_timestamp = VB_V810->Run(EventHandler);

 FixNonEvents();
 ForceEventUpdates(v810_timestamp);

 VB_VSU->EndFrame((v810_timestamp + VSU_CycleFix) >> 2);

 if(espec->SoundBuf)
 {
  for(int y = 0; y < 2; y++)
  {
   sbuf[y].end_frame((v810_timestamp + VSU_CycleFix) >> 2);
   espec->SoundBufSize = sbuf[y].read_samples(espec->SoundBuf + y, espec->SoundBufMaxSize, 1);
  }
 }

 VSU_CycleFix = (v810_timestamp + VSU_CycleFix) & 3;

 espec->MasterCycles = v810_timestamp;

 TIMER_ResetTS();
 VBINPUT_ResetTS();
 VIP_ResetTS();

 RebaseTS(v810_timestamp);

 VB_V810->ResetTS();

}

uint8* getWRAM() {
	return WRAM;
}

}

using namespace MDFN_IEN_VB;
#if 0
static DebuggerInfoStruct DBGInfo =
{
 4,
 2,             // Instruction alignment(bytes)
 32,
 32,
 0x00000000,
 ~0,

 VBDBG_MemPeek,
 VBDBG_Disassemble,
 NULL,
 NULL,	//ForceIRQ,
 NULL,
 VBDBG_FlushBreakPoints,
 VBDBG_AddBreakPoint,
 VBDBG_SetCPUCallback,
 VBDBG_SetBPCallback,
 VBDBG_GetBranchTrace,
 NULL, 	//KING_SetGraphicsDecode,
 VBDBG_SetLogFunc,
};
#endif


static int StateAction(StateMem *sm, int load, int data_only)
{
 int ret = 1;
 SFORMAT StateRegs[] =
 {
  SFARRAY(WRAM, 65536),
  SFARRAY(GPRAM, GPRAM_Mask ? (GPRAM_Mask + 1) : 0),
  SFVAR(WCR),
  SFVAR(IRQ_Asserted),
  SFVAR(VSU_CycleFix),


  // TODO: Remove these(and recalc on state load)
  SFVAR(next_vip_ts), 
  SFVAR(next_timer_ts),
  SFVAR(next_input_ts),

  SFEND
 };

 ret &= MDFNSS_StateAction(sm, load, data_only, StateRegs, "MAIN");

 ret &= VB_V810->StateAction(sm, load, data_only);

 ret &= VB_VSU->StateAction(sm, load, data_only);
 ret &= TIMER_StateAction(sm, load, data_only);
 ret &= VBINPUT_StateAction(sm, load, data_only);
 ret &= VIP_StateAction(sm, load, data_only);

 if(load)
 {

 }
 return(ret);
}

static bool SetSoundRate(uint32 rate)
{
 for(int y = 0; y < 2; y++)
 {
  sbuf[y].set_sample_rate(rate ? rate : 44100, 50);
  sbuf[y].clock_rate((long)(VB_MASTER_CLOCK / 4));
  sbuf[y].bass_freq(20);
 }

 return(TRUE);
} 

static bool ToggleLayer(int which)
{
 return(1);
}

static void DoSimpleCommand(int cmd)
{
 switch(cmd)
 {
  case MDFNNPCMD_POWER:
  case MDFNNPCMD_RESET: VB_Power(); break;
 }
}

static const MDFNSetting_EnumList V810Mode_List[] =
{
 { "fast", (int)V810_EMU_MODE_FAST },
 { "accurate", (int)V810_EMU_MODE_ACCURATE },
 { NULL, 0 },
};

static const MDFNSetting_EnumList VB3DMode_List[] =
{
 { "anaglyph", VB3DMODE_ANAGLYPH },
 { "cscope",  VB3DMODE_CSCOPE },
 { "sidebyside", VB3DMODE_SIDEBYSIDE },
// { "overunder", VB3DMODE_OVERUNDER },
 { "pbarrier", VB3DMODE_PBARRIER },

 { NULL, 0 },
};

static const MDFNSetting_EnumList AnaglyphPreset_List[] =
{
 { "disabled", ANAGLYPH_PRESET_DISABLED },
 { "0", ANAGLYPH_PRESET_DISABLED },

 { "red_blue", ANAGLYPH_PRESET_RED_BLUE },
 { "red_cyan", ANAGLYPH_PRESET_RED_CYAN },
 { "red_electriccyan", ANAGLYPH_PRESET_RED_ELECTRICCYAN },
 { "red_green", ANAGLYPH_PRESET_RED_GREEN },
 { "green_magenta", ANAGLYPH_PRESET_GREEN_MAGENTA },
 { "yellow_blue", ANAGLYPH_PRESET_YELLOW_BLUE },
};

static MDFNSetting VBSettings[] =
{
 { "vb.cpu_emulation", gettext_noop("Select CPU emulation mode."), MDFNST_ENUM, "fast", NULL, NULL, NULL, NULL, V810Mode_List },
 { "vb.input.instant_read_hack", gettext_noop("Hack to return the current pad state, rather than latched state, to reduce latency."), MDFNST_BOOL, "1" },

 { "vb.3dmode", gettext_noop("3D mode."), MDFNST_ENUM, "anaglyph", NULL, NULL, NULL, NULL, VB3DMode_List },
 { "vb.disable_parallax", gettext_noop("Disable parallax for BG and OBJ rendering."), MDFNST_BOOL, "0" },
 { "vb.default_color", gettext_noop("Default maximum-brightness color to use in non-anaglyph 3D modes."), MDFNST_UINT, "0xF0F0F0", "0", "0xFFFFFF" },
 { "vb.anaglyph.preset", gettext_noop("Anaglyph preset colors."), MDFNST_ENUM, "red_blue", NULL, NULL, NULL, NULL, AnaglyphPreset_List },
 { "vb.anaglyph.lcolor", gettext_noop("Anaglyph maximum-brightness color for left view."), MDFNST_UINT, "0xffba00", "0", "0xFFFFFF" },
 { "vb.anaglyph.rcolor", gettext_noop("Anaglyph maximum-brightness color for right view."), MDFNST_UINT, "0x00baff", "0", "0xFFFFFF" },
 { NULL }
};


static const InputDeviceInputInfoStruct IDII[] =
{
 { "a", "A", 7, IDIT_BUTTON_CAN_RAPID,  NULL },
 { "b", "B", 6, IDIT_BUTTON_CAN_RAPID, NULL },
 { "rt", "Right-Back", 13, IDIT_BUTTON, NULL },
 { "lt", "Left-Back", 12, IDIT_BUTTON, NULL },

 { "up-r", "UP ↑ (Right D-Pad)", 8, IDIT_BUTTON, "down-r" },
 { "right-r", "RIGHT → (Right D-Pad)", 11, IDIT_BUTTON, "left-r" },

 { "right-l", "RIGHT → (Left D-Pad)", 3, IDIT_BUTTON, "left-l" },
 { "left-l", "LEFT ← (Left D-Pad)", 2, IDIT_BUTTON, "right-l" },
 { "down-l", "DOWN ↓ (Left D-Pad)", 1, IDIT_BUTTON, "up-l" },
 { "up-l", "UP ↑ (Left D-Pad)", 0, IDIT_BUTTON, "down-l" },

 { "start", "Start", 5, IDIT_BUTTON, NULL },
 { "select", "Select", 4, IDIT_BUTTON, NULL },

 { "left-r", "LEFT ← (Right D-Pad)", 10, IDIT_BUTTON, "right-r" },
 { "down-r", "DOWN ↓ (Right D-Pad)", 9, IDIT_BUTTON, "up-r" },
};

static InputDeviceInfoStruct InputDeviceInfo[] =
{
 {
  "gamepad",
  "Gamepad",
  NULL,
  sizeof(IDII) / sizeof(InputDeviceInputInfoStruct),
  IDII,
 }
};

static const InputPortInfoStruct PortInfo[] =
{
 { 0, "builtin", "Built-In", sizeof(InputDeviceInfo) / sizeof(InputDeviceInfoStruct), InputDeviceInfo }
};

static InputInfoStruct InputInfo =
{
 sizeof(PortInfo) / sizeof(InputPortInfoStruct),
 PortInfo
};


static const FileExtensionSpecStruct KnownExtensions[] =
{
 { ".vb", gettext_noop("Nintendo Virtual Boy") },
 { NULL, NULL }
};

MDFNGI EmulatedVB =
{
 "vb",
 "Virtual Boy",
 KnownExtensions,
 MODPRIO_INTERNAL_HIGH,
 #ifdef WANT_DEBUGGER
 &DBGInfo,
 #else
 NULL,		// Debug info
 #endif
 &InputInfo,	//
 Load,
 TestMagic,
 NULL,
 NULL,
 CloseGame,
 ToggleLayer,
 "",		// Layer names, null-delimited
 NULL,
 NULL,
 NULL,
 StateAction,
 Emulate,
 VBINPUT_SetInput,
 NULL,		//PCFX_CDInsert,
 NULL,		//PCFX_CDEject,
 NULL,		//PCFX_CDSelect,
 SetSoundRate,
 DoSimpleCommand,
 VBSettings,
 MDFN_MASTERCLOCK_FIXED((int64)VB_MASTER_CLOCK),
 0,
 false, // Multires possible?
 384,   // Nominal width
 224,    // Nominal height
 864 * sizeof(uint32), // Framebuffer pitch
 384,                  // Framebuffer height (384 for CScope. Otherwise, 256 works fine)

 2,     // Number of output sound channels
};