/* V810 Emulator
 *
 * Copyright (C) 2006 David Tucker
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

/* Alternatively, the V810 emulator code(and all V810 emulation header files) can be used/distributed under the following license(you can adopt either
   license exclusively for your changes by removing one of these license headers, but it's STRONGLY preferable
   to keep your changes dual-licensed as well):

This Reality Boy emulator is copyright (C) David Tucker 1997-2008, all rights
reserved.   You may use this code as long as you make no money from the use of
this code and you acknowledge the original author (Me).  I reserve the right to
dictate who can use this code and how (Just so you don't do something stupid
with it).
   Most Importantly, this code is swap ware.  If you use It send along your new
program (with code) or some other interesting tidbits you wrote, that I might be
interested in.
   This code is in beta, there are bugs!  I am not responsible for any damage
done to your computer, reputation, ego, dog, or family life due to the use of
this code.  All source is provided as is, I make no guaranties, and am not
responsible for anything you do with the code (legal or otherwise).
   Virtual Boy is a trademark of Nintendo, and V810 is a trademark of NEC.  I am
in no way affiliated with either party and all information contained hear was
found freely through public domain sources.
*/

//////////////////////////////////////////////////////////
// CPU routines

#include "mednafen.h"
#include "math_ops.h"
#include "memory.h"
//#include "pcfx.h"
//#include "debug.h"

#include <string.h>
#include <errno.h>

#include "v810_opt.h"
#include "v810_cpu.h"
#include "v810_cpuD.h"

//#include "fpu-new/softfloat.h"

V810::V810()
{
#ifdef WANT_DEBUGGER
 MemPeek8 = NULL;
 MemPeek16 = NULL;
 MemPeek32 = NULL;
#endif
 CPUHook = NULL;
 ADDBT = NULL;

 MemRead8 = NULL;
 MemRead16 = NULL;
 MemRead32 = NULL;

 IORead8 = NULL;
 IORead16 = NULL;
 IORead32 = NULL;

 MemWrite8 = NULL;
 MemWrite16 = NULL;
 MemWrite32 = NULL;

 IOWrite8 = NULL;
 IOWrite16 = NULL;
 IOWrite32 = NULL;

 memset(v810_fast_map, 0, sizeof(v810_fast_map));
 memset(v810_fast_map_nf, 0, sizeof(v810_fast_map_nf));

 memset(MemReadBus32, 0, sizeof(MemReadBus32));
 memset(MemWriteBus32, 0, sizeof(MemWriteBus32));


}

V810::~V810()
{


}


// TODO: "An interrupt that occurs during restore/dump/clear operation is internally held and is accepted after the
// operation in progress is finished. The maskable interrupt is held internally only when the EP, NP, and ID flags
// of PSW are all 0."
//
// This behavior probably doesn't have any relevance on the PC-FX, unless we're sadistic
// and try to restore cache from an interrupt acknowledge register or dump it to a register
// controlling interrupt masks...  I wanna be sadistic~

void V810::CacheClear(uint32 start, uint32 count)
{
 printf("Cache clear: %08x %08x\n", start, count);
 for(uint32 i = 0; i < count && (i + start) < 128; i++)
  memset(&Cache[i + start], 0, sizeof(V810_CacheEntry_t));
}

INLINE void V810::CacheOpMemStore(uint32 A, uint32 V)
{
 if(MemWriteBus32[A >> 24])
 {
  v810_timestamp += 2;
  MemWrite32(v810_timestamp, A, V);
 }
 else
 {
  v810_timestamp += 2;
  MemWrite16(v810_timestamp, A, V & 0xFFFF);

  v810_timestamp += 2;
  MemWrite16(v810_timestamp, A | 2, V >> 16);
 }
}

INLINE uint32 V810::CacheOpMemLoad(uint32 A)
{
 if(MemReadBus32[A >> 24])
 {
  v810_timestamp += 2;
  return(MemRead32(v810_timestamp, A));
 }
 else
 {
  uint32 ret;

  v810_timestamp += 2;
  ret = MemRead16(v810_timestamp, A);

  v810_timestamp += 2;
  ret |= MemRead16(v810_timestamp, A | 2) << 16;
  return(ret);
 }
}

void V810::CacheDump(const uint32 SA)
{
 printf("Cache dump: %08x\n", SA);

 for(int i = 0; i < 128; i++)
 {
  CacheOpMemStore(SA + i * 8 + 0, Cache[i].data[0]);
  CacheOpMemStore(SA + i * 8 + 4, Cache[i].data[1]);
 }

 for(int i = 0; i < 128; i++)
 {
  uint32 icht = Cache[i].tag | ((int)Cache[i].data_valid[0] << 22) | ((int)Cache[i].data_valid[1] << 23);

  CacheOpMemStore(SA + 1024 + i * 4, icht);
 }

}

void V810::CacheRestore(const uint32 SA)
{
 printf("Cache restore: %08x\n", SA);

 for(int i = 0; i < 128; i++)
 {
  Cache[i].data[0] = CacheOpMemLoad(SA + i * 8 + 0);
  Cache[i].data[1] = CacheOpMemLoad(SA + i * 8 + 4);
 }

 for(int i = 0; i < 128; i++)
 {
  uint32 icht;

  icht = CacheOpMemLoad(SA + 1024 + i * 4);

  Cache[i].tag = icht & ((1 << 22) - 1);
  Cache[i].data_valid[0] = (icht >> 22) & 1;
  Cache[i].data_valid[1] = (icht >> 23) & 1;
 }
}


INLINE uint32 V810::RDCACHE(uint32 addr)
{
 const int CI = (addr >> 3) & 0x7F;
 const int SBI = (addr & 4) >> 2;

 if(Cache[CI].tag == (addr >> 10))
 {
  if(!Cache[CI].data_valid[SBI])
  {
   v810_timestamp += 2;       // or higher?  Penalty for cache miss seems to be higher than having cache disabled.
   if(MemReadBus32[addr >> 24])
    Cache[CI].data[SBI] = MemRead32(v810_timestamp, addr & ~0x3);
   else
   {
    v810_timestamp++;
    Cache[CI].data[SBI] = MemRead16(v810_timestamp, addr & ~0x3) | ((MemRead16(v810_timestamp, (addr & ~0x3) | 0x2) << 16));
   }
   Cache[CI].data_valid[SBI] = TRUE;
  }
 }
 else
 {
  Cache[CI].tag = addr >> 10;

  v810_timestamp += 2;	// or higher?  Penalty for cache miss seems to be higher than having cache disabled.
  if(MemReadBus32[addr >> 24])
   Cache[CI].data[SBI] = MemRead32(v810_timestamp, addr & ~0x3);
  else
  {
   v810_timestamp++;
   Cache[CI].data[SBI] = MemRead16(v810_timestamp, addr & ~0x3) | ((MemRead16(v810_timestamp, (addr & ~0x3) | 0x2) << 16));
  }
  //Cache[CI].data[SBI] = MemRead32(v810_timestamp, addr & ~0x3);
  Cache[CI].data_valid[SBI] = TRUE;
  Cache[CI].data_valid[SBI ^ 1] = FALSE;
 }

 //{
 // // Caution: This can mess up DRAM page change penalty timings
 // uint32 dummy_timestamp = 0;
 // if(Cache[CI].data[SBI] != mem_rword(addr & ~0x3, dummy_timestamp))
 // {
 //  printf("Cache/Real Memory Mismatch: %08x %08x/%08x\n", addr & ~0x3, Cache[CI].data[SBI], mem_rword(addr & ~0x3, dummy_timestamp));
 // }
 //}

 return(Cache[CI].data[SBI]);
}

INLINE uint16 V810::RDOP(uint32 addr, uint32 meow)
{
 uint16 ret;

 if(S_REG[CHCW] & 0x2)
 {
  uint32 d32 = RDCACHE(addr);
  ret = d32 >> ((addr & 2) * 8);
 }
 else
 {
  v810_timestamp += meow; //++;
  ret = MemRead16(v810_timestamp, addr);
 }
 return(ret);
}

#define BRANCH_ALIGN_CHECK(x)	{ if((S_REG[CHCW] & 0x2) && (x & 0x2)) { ADDCLOCK(1); } }

// Reinitialize the defaults in the CPU
void V810::Reset() 
{
 v810_timestamp = 0;
 next_event_ts = 0x7FFFFFFF; // fixme

 memset(&Cache, 0, sizeof(Cache));

 memset(P_REG, 0, sizeof(P_REG));
 memset(S_REG, 0, sizeof(S_REG));
 memset(Cache, 0, sizeof(Cache));

 P_REG[0]      =  0x00000000;
 SetPC(0xFFFFFFF0);

 S_REG[ECR]    =  0x0000FFF0;
 S_REG[PSW]    =  0x00008000;

 if(VBMode)
  S_REG[PIR]	= 0x00005346;
 else
  S_REG[PIR]    =  0x00008100;

 S_REG[TKCW]   =  0x000000E0;
 Halted = HALT_NONE;
 ilevel = -1;

 lastop = 0;

 in_bstr = FALSE;
}

bool V810::Init(V810_Emu_Mode mode, bool vb_mode)
{
 EmuMode = mode;
 VBMode = vb_mode;

 in_bstr = FALSE;
 in_bstr_to = 0;

 memset(v810_fast_map_nf, 0, sizeof(v810_fast_map_nf));

 if(mode == V810_EMU_MODE_FAST)
 {
  memset(DummyRegion, 0, V810_FAST_MAP_PSIZE);

  for(unsigned int i = V810_FAST_MAP_PSIZE; i < V810_FAST_MAP_PSIZE + V810_FAST_MAP_TRAMPOLINE_SIZE; i += 2)
  {
   DummyRegion[i + 0] = 0;
   DummyRegion[i + 1] = 0x36 << 2;
  }
  for(uint64 A = 0; A < (1ULL << 32); A += V810_FAST_MAP_PSIZE)
   v810_fast_map[A / V810_FAST_MAP_PSIZE] = DummyRegion - A;
 }

 return(TRUE);
}

void V810::Kill(void)
{
 for(uint32 i = 0; i < (1ULL << 32) / V810_FAST_MAP_PSIZE; i++)
 {
  if(v810_fast_map_nf[i])
  {
   MDFN_free(v810_fast_map_nf[i]);
   v810_fast_map_nf[i] = NULL;
  }
 }
}

void V810::SetInt(int level)
{
 assert(level >= -1 && level <= 15);

 ilevel = level;
}

uint8 *V810::SetFastMap(uint32 addresses[], uint32 length, unsigned int num_addresses, const char *name)
{
 uint8 *ret = NULL;

 for(unsigned int i = 0; i < num_addresses; i++)
 {
  assert((addresses[i] & (V810_FAST_MAP_PSIZE - 1)) == 0);
 }
 assert((length & (V810_FAST_MAP_PSIZE - 1)) == 0);

 if(!(ret = (uint8 *)MDFN_malloc(length + V810_FAST_MAP_TRAMPOLINE_SIZE, name)))
 {
  return(NULL);
 }

 for(unsigned int i = length; i < length + V810_FAST_MAP_TRAMPOLINE_SIZE; i += 2)
 {
  ret[i + 0] = 0;
  ret[i + 1] = 0x36 << 2;
 }

 for(unsigned int i = 0; i < num_addresses; i++)
 {  
  for(uint64 addr = addresses[i]; addr != (uint64)addresses[i] + length; addr += V810_FAST_MAP_PSIZE)
  {
   //printf("%08x, %d, %s\n", addr, length, name);
   assert(NULL == v810_fast_map_nf[addr / V810_FAST_MAP_PSIZE]);
   //assert(NULL == v810_fast_map[addr / V810_FAST_MAP_PSIZE]);

   v810_fast_map[addr / V810_FAST_MAP_PSIZE] = ret - addresses[i];
  }
 }
 v810_fast_map_nf[addresses[0] / V810_FAST_MAP_PSIZE] = ret;

 return(ret);
}


void V810::SetMemReadBus32(uint8 A, bool value)
{
 MemReadBus32[A] = value;
}

void V810::SetMemWriteBus32(uint8 A, bool value)
{
 MemWriteBus32[A] = value;
}

void V810::SetMemReadHandlers(uint8 MDFN_FASTCALL (*read8)(v810_timestamp_t &, uint32), uint16 MDFN_FASTCALL (*read16)(v810_timestamp_t &, uint32), uint32 MDFN_FASTCALL (*read32)(v810_timestamp_t &, uint32))
{
 MemRead8 = read8;
 MemRead16 = read16;
 MemRead32 = read32;
}

void V810::SetMemWriteHandlers(void MDFN_FASTCALL (*write8)(v810_timestamp_t &, uint32, uint8), void MDFN_FASTCALL (*write16)(v810_timestamp_t &, uint32, uint16), void MDFN_FASTCALL (*write32)(v810_timestamp_t &, uint32, uint32))
{
 MemWrite8 = write8;
 MemWrite16 = write16;
 MemWrite32 = write32;
}

void V810::SetIOReadHandlers(uint8 MDFN_FASTCALL (*read8)(v810_timestamp_t &, uint32), uint16 MDFN_FASTCALL (*read16)(v810_timestamp_t &, uint32), uint32 MDFN_FASTCALL (*read32)(v810_timestamp_t &, uint32))
{
 IORead8 = read8;
 IORead16 = read16;
 IORead32 = read32;
}

void V810::SetIOWriteHandlers(void MDFN_FASTCALL (*write8)(v810_timestamp_t &, uint32, uint8), void MDFN_FASTCALL (*write16)(v810_timestamp_t &, uint32, uint16), void MDFN_FASTCALL (*write32)(v810_timestamp_t &, uint32, uint32))
{
 IOWrite8 = write8;
 IOWrite16 = write16;
 IOWrite32 = write32;
}


INLINE void V810::SetFlag(uint32 n, bool condition)
{
 S_REG[PSW] &= ~n;

 if(condition)
  S_REG[PSW] |= n;
}
	
INLINE void V810::SetSZ(uint32 value)
{
 SetFlag(PSW_Z, !value);
 SetFlag(PSW_S, (bool)(value & 0x80000000));
}

#ifdef WANT_DEBUGGER
void V810::CheckBreakpoints(void (*callback)(int type, uint32 address, unsigned int len))
{
 unsigned int opcode;
 uint16 tmpop;
 uint16 tmpop_high;
 int32 ws_dummy = v810_timestamp;

 // FIXME: Peek, not Read
 tmpop      = MemRead16(ws_dummy, PC);
 tmpop_high = MemRead16(ws_dummy, PC + 2);

 opcode = tmpop >> 10;

 // Uncomment this out later if necessary.
 //if((tmpop & 0xE000) == 0x8000)        // Special opcode format for
 // opcode = (tmpop >> 9) & 0x7F;    // type III instructions.

 switch(opcode)
 {
	case CAXI: break;

	default: break;

	case LD_B: callback(BPOINT_READ, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFF, 1); break;
	case LD_H: callback(BPOINT_READ, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFE, 2); break;
	case LD_W: callback(BPOINT_READ, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFC, 4); break;

	case ST_B: callback(BPOINT_WRITE, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFF, 1); break;
	case ST_H: callback(BPOINT_WRITE, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFE, 2); break;
	case ST_W: callback(BPOINT_WRITE, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFC, 4); break;

	case IN_B: callback(BPOINT_IO_READ, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFF, 1); break;
	case IN_H: callback(BPOINT_IO_READ, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFE, 2); break;
	case IN_W: callback(BPOINT_IO_READ, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFC, 4); break;

	case OUT_B: callback(BPOINT_IO_WRITE, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFF, 1); break; 
	case OUT_H: callback(BPOINT_IO_WRITE, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFE, 2); break;
	case OUT_W: callback(BPOINT_IO_WRITE, (sign_16(tmpop_high)+P_REG[tmpop & 0x1F])&0xFFFFFFFC, 4); break;
 }

}
#endif

#define SetPREG(n, val) { P_REG[n] = val; }

INLINE void V810::SetSREG(unsigned int which, uint32 value)
{
             if(which == PSW)
             {
              S_REG[PSW] = value & 0xFF3FF;
              //printf("%08x\n", value & 0xFF3FF);
             }
             else if(which == ADDTRE)
             {
              S_REG[ADDTRE] = value & 0xFFFFFFFE;
              printf("Address trap(unemulated): %08x\n", value);
             }
             else if(which == CHCW)
             {
              S_REG[CHCW] = value & 0x2;

              switch(value & 0x31)
              {
               default: printf("Undefined cache control bit combination: %08x\n", value);
                        break;

               case 0x00: break;

               case 0x01: CacheClear((value >> 20) & 0xFFF, (value >> 8) & 0xFFF);
                          break;

               case 0x10: CacheDump(value & ~0xFF);
                          break;

               case 0x20: CacheRestore(value & ~0xFF);
                          break;
              }
             }
             else if(which != PIR && which != TKCW && which != ECR)
              S_REG[which] = value;
}

INLINE uint32 V810::GetSREG(unsigned int which)
{
	uint32 ret;

	if(which != 24 && which != 25 && which >= 8)
	{
         printf("STSR Reserved!  %08x %02x\n", PC, which);
        }

	ret = S_REG[which];

	return(ret);
}

#define RB_SETPC(new_pc_raw) 										\
			  {										\
			   const uint32 new_pc = new_pc_raw;	/* So RB_SETPC(RB_GETPC()) won't mess up */	\
			   if(RB_AccurateMode)								\
			    PC = new_pc;								\
			   else										\
			   {										\
			    PC_ptr = &v810_fast_map[(new_pc) >> V810_FAST_MAP_SHIFT][(new_pc)];		\
			    PC_base = PC_ptr - (new_pc);						\
			   }										\
			  }

#define RB_PCRELCHANGE(delta) { 				\
				if(RB_AccurateMode)		\
				 PC += (delta);			\
				else				\
				{				\
				 uint32 PC_tmp = RB_GETPC();	\
				 PC_tmp += (delta);		\
				 RB_SETPC(PC_tmp);		\
				}					\
			      }

#define RDOP_FAST(PC_offset) (*(uint16 *)&PC_ptr[PC_offset])

#define RB_RDOP(PC_offset, ...) (RB_AccurateMode ? RDOP(PC + PC_offset, ## __VA_ARGS__) : RDOP_FAST(PC_offset) )

#define RB_INCPCBY2()	{ if(RB_AccurateMode) PC += 2; else PC_ptr += 2; }
#define RB_INCPCBY4()   { if(RB_AccurateMode) PC += 4; else PC_ptr += 4; }

#define RB_DECPCBY2()   { if(RB_AccurateMode) PC -= 2; else PC_ptr -= 2; }
#define RB_DECPCBY4()   { if(RB_AccurateMode) PC -= 4; else PC_ptr -= 4; }

#define RB_GETPC()      PC

void V810::Run_Accurate(int32 MDFN_FASTCALL (*event_handler)(const v810_timestamp_t timestamp))
{
 const bool RB_AccurateMode = true;
 const bool RB_DebugMode = false;

/* V810 Emulator
 *
 * Copyright (C) 2006 David Tucker
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

    uint32 opcode;
    uint32 tmp2;
    int val = 0;

    #define ADDCLOCK(__n) { v810_timestamp += __n; }

    #define CHECK_HALTED();	{ if(Halted && v810_timestamp < next_event_ts) { v810_timestamp = next_event_ts; } }

    while(Running)
    {
     uint32 tmpop;

     assert(v810_timestamp <= next_event_ts);

     if(!WillInterruptOccur())
     {
      if(Halted)
      {
       v810_timestamp = next_event_ts;
      }
      else if(in_bstr)
      {
       tmpop = in_bstr_to;
       opcode = tmpop >> 9;
       goto op_BSTR;
      }
     }

     while(v810_timestamp < next_event_ts)
     {
	if(ilevel >= 0)
	{ 
	 int temp_clocks = Int(ilevel);
	 ADDCLOCK(temp_clocks);
	}

        P_REG[0] = 0; //Zero the Zero Reg!!!

	if(RB_DebugMode)
	{
	 if(CPUHook)
	  CPUHook(RB_GETPC());
	}

	{
	 //printf("%08x\n", RB_GETPC());
	 tmpop = RB_RDOP(0, 0);
       	 opcode = tmpop >> 9;
	 //printf("%02x\n", opcode >> 1);
#if 0
	static const void *const op_goto_table[128] =
	{

&&op_MOV, &&op_MOV, &&op_ADD, &&op_ADD, &&op_SUB, &&op_SUB, &&op_CMP, &&op_CMP, 
&&op_SHL, &&op_SHL, &&op_SHR, &&op_SHR, &&op_JMP, &&op_JMP, &&op_SAR, &&op_SAR, 
&&op_MUL, &&op_MUL, &&op_DIV, &&op_DIV, &&op_MULU, &&op_MULU, &&op_DIVU, &&op_DIVU, 
&&op_OR, &&op_OR, &&op_AND, &&op_AND, &&op_XOR, &&op_XOR, &&op_NOT, &&op_NOT, 

&&op_MOV_I, &&op_MOV_I, &&op_ADD_I, &&op_ADD_I, &&op_SETF, &&op_SETF, &&op_CMP_I, &&op_CMP_I, 
&&op_SHL_I, &&op_SHL_I, &&op_SHR_I, &&op_SHR_I, &&op_EI, &&op_EI, &&op_SAR_I, &&op_SAR_I, 
&&op_TRAP, &&op_TRAP, &&op_RETI, &&op_RETI, &&op_HALT, &&op_HALT, &&op_INVALID, &&op_INVALID, 
&&op_LDSR, &&op_LDSR, &&op_STSR, &&op_STSR, &&op_DI, &&op_DI, &&op_BSTR, &&op_BSTR, 

&&op_BV, &&op_BL, &&op_BE, &&op_BNH, &&op_BN, &&op_BR, &&op_BLT, &&op_BLE, 
&&op_BNV, &&op_BNL, &&op_BNE, &&op_BH, &&op_BP, &&op_NOP, &&op_BGE, &&op_BGT, 
&&op_MOVEA, &&op_MOVEA, &&op_ADDI, &&op_ADDI, &&op_JR, &&op_JR, &&op_JAL, &&op_JAL, 
&&op_ORI, &&op_ORI, &&op_ANDI, &&op_ANDI, &&op_XORI, &&op_XORI, &&op_MOVHI, &&op_MOVHI, 

&&op_LD_B, &&op_LD_B, &&op_LD_H, &&op_LD_H, &&op_INVALID, &&op_INVALID, &&op_LD_W, &&op_LD_W, 
&&op_ST_B, &&op_ST_B, &&op_ST_H, &&op_ST_H, &&op_INVALID, &&op_INVALID, &&op_ST_W, &&op_ST_W, 
&&op_IN_B, &&op_IN_B, &&op_IN_H, &&op_IN_H, &&op_CAXI, &&op_CAXI, &&op_IN_W, &&op_IN_W, 
&&op_OUT_B, &&op_OUT_B, &&op_OUT_H, &&op_OUT_H, &&op_FPP, &&op_FPP, &&op_OUT_W, &&op_OUT_W, 

	};
#endif

	//AAAAAGGGGGGHHHHHH
	switch(tmpop >> 9) {

//&&op_MOV, &&op_MOV, &&op_ADD, &&op_ADD, &&op_SUB, &&op_SUB, &&op_CMP, &&op_CMP, 

		case 0: goto op_MOV; break;
		case 1: goto op_MOV; break;
		case 2: goto op_ADD; break;
		case 3: goto op_ADD; break;
		case 4: goto op_SUB; break;
		case 5: goto op_SUB; break;
		case 6: goto op_CMP; break;
		case 7: goto op_CMP; break;
//&&op_SHL, &&op_SHL, &&op_SHR, &&op_SHR, &&op_JMP, &&op_JMP, &&op_SAR, &&op_SAR, 

		case 8: goto op_SHL; break;
		case 9: goto op_SHL; break;
		case 10: goto op_SHR; break;
		case 11: goto op_SHR; break;
		case 12: goto op_JMP; break;
		case 13: goto op_JMP; break;
		case 14: goto op_SAR; break;
		case 15: goto op_SAR; break;
//&&op_MUL, &&op_MUL, &&op_DIV, &&op_DIV, &&op_MULU, &&op_MULU, &&op_DIVU, &&op_DIVU, 

		case 16: goto op_MUL; break;
		case 17: goto op_MUL; break;
		case 18: goto op_DIV; break;
		case 19: goto op_DIV; break;
		case 20: goto op_MULU; break;
		case 21: goto op_MULU; break;
		case 22: goto op_DIVU; break;
		case 23: goto op_DIVU; break;
//&&op_OR, &&op_OR, &&op_AND, &&op_AND, &&op_XOR, &&op_XOR, &&op_NOT, &&op_NOT, 
		case 24: goto op_OR; break;
		case 25: goto op_OR; break;
		case 26: goto op_AND; break;
		case 27: goto op_AND; break;
		case 28: goto op_XOR; break;
		case 29: goto op_XOR; break;
		case 30: goto op_NOT; break;
		case 31: goto op_NOT; break;

//&&op_MOV_I, &&op_MOV_I, &&op_ADD_I, &&op_ADD_I, &&op_SETF, &&op_SETF, op_CMP_I&&, &&op_CMP_I, 

		case 32: goto op_MOV_I; break;
		case 33: goto op_MOV_I; break;
		case 34: goto op_ADD_I; break;
		case 35: goto op_ADD_I; break;
		case 36: goto op_SETF; break;
		case 37: goto op_SETF; break;
		case 38: goto op_CMP_I; break;
		case 39: goto op_CMP_I; break;
//&&op_SHL_I, &&op_SHL_I, &&op_SHR_I, &&op_SHR_I, &&op_EI, &&op_EI, &&op_SAR_I, &&op_SAR_I, 

		case 40: goto op_SHL_I; break;
		case 41: goto op_SHL_I; break;
		case 42: goto op_SHR_I; break;
		case 43: goto op_SHR_I; break;
		case 44: goto op_EI; break;
		case 45: goto op_EI; break;
		case 46: goto op_SAR_I; break;
		case 47: goto op_SAR_I; break;
//&&op_TRAP, &&op_TRAP, &&op_RETI, &&op_RETI, &&op_HALT, &&op_HALT, &&op_INVALID, &&op_INVALID, 

		case 48: goto op_TRAP; break;
		case 49: goto op_TRAP; break;
		case 50: goto op_RETI; break;
		case 51: goto op_RETI; break;
		case 52: goto op_HALT; break;
		case 53: goto op_HALT; break;
		case 54: goto op_INVALID; break;
		case 55: goto op_INVALID; break;
//&&op_LDSR, &&op_LDSR, &&op_STSR, &&op_STSR, &&op_DI, &&op_DI, &&op_BSTR, &&op_BSTR, 
		case 56: goto op_LDSR; break;
		case 57: goto op_LDSR; break;
		case 58: goto op_STSR; break;
		case 59: goto op_STSR; break;
		case 60: goto op_DI; break;
		case 61: goto op_DI; break;
		case 62: goto op_BSTR; break;
		case 63: goto op_BSTR; break;
//&&op_BV, &&op_BL, &&op_BE, &&op_BNH, &&op_BN, &&op_BR, &&op_BLT, &&op_BLE, 

		case 64: goto op_BV; break;
		case 65: goto op_BL; break;
		case 66: goto op_BE; break;
		case 67: goto op_BNH; break;
		case 68: goto op_BN; break;
		case 69: goto op_BR; break;
		case 70: goto op_BLT; break;
		case 71: goto op_BLE; break;
//&&op_BNV, &&op_BNL, &&op_BNE, &&op_BH, &&op_BP, &&op_NOP, &&op_BGE, &&op_BGT, 

		case 72: goto op_BNV; break;
		case 73: goto op_BNL; break;
		case 74: goto op_BNE; break;
		case 75: goto op_BH; break;
		case 76: goto op_BP; break;
		case 77: goto op_NOP; break;
		case 78: goto op_BGE; break;
		case 79: goto op_BGT; break;
//&&op_MOVEA, &&op_MOVEA, &&op_ADDI, &&op_ADDI, &&op_JR, &&op_JR, &&op_JAL, &&op_JAL, 

		case 80: goto op_MOVEA; break;
		case 81: goto op_MOVEA; break;
		case 82: goto op_ADDI; break;
		case 83: goto op_ADDI; break;
		case 84: goto op_JR; break;
		case 85: goto op_JR; break;
		case 86: goto op_JAL; break;
		case 87: goto op_JAL; break;
//&&op_ORI, &&op_ORI, &&op_ANDI, &&op_ANDI, &&op_XORI, &&op_XORI, &&op_MOVHI, &&op_MOVHI, 
		case 88: goto op_ORI; break;
		case 89: goto op_ORI; break;
		case 90: goto op_ANDI; break;
		case 91: goto op_ANDI; break;
		case 92: goto op_XORI; break;
		case 93: goto op_XORI; break;
		case 94: goto op_MOVHI; break;
		case 95: goto op_MOVHI; break;
//&&op_LD_B, &&op_LD_B, &&op_LD_H, &&op_LD_H, &&op_INVALID, &&op_INVALID, &&op_LD_W, &&op_LD_W, 

		case 96: goto op_LD_B; break;
		case 97: goto op_LD_B; break;
		case 98: goto op_LD_H; break;
		case 99: goto op_LD_H; break;
		case 100: goto op_INVALID; break;
		case 101: goto op_INVALID; break;
		case 102: goto op_LD_W; break;
		case 103: goto op_LD_W; break;
//&&op_ST_B, &&op_ST_B, &&op_ST_H, &&op_ST_H, &&op_INVALID, &&op_INVALID, &&op_ST_W, &&op_ST_W, 

		case 104: goto op_ST_B; break;
		case 105: goto op_ST_B; break;
		case 106: goto op_ST_H; break;
		case 107: goto op_ST_H; break;
		case 108: goto op_INVALID; break;
		case 109: goto op_INVALID; break;
		case 110: goto op_ST_W; break;
		case 111: goto op_ST_W; break;
//&&op_IN_B, &&op_IN_B, &&op_IN_H, &&op_IN_H, &&op_CAXI, &&op_CAXI, &&op_IN_W, &&op_IN_W, 

		case 112: goto op_IN_B; break;
		case 113: goto op_IN_B; break;
		case 114: goto op_IN_H; break;
		case 115: goto op_IN_H; break;
		case 116: goto op_CAXI; break;
		case 117: goto op_CAXI; break;
		case 118: goto op_IN_W; break;
		case 119: goto op_IN_W; break;
//&&op_OUT_B, &&op_OUT_B, &&op_OUT_H, &&op_OUT_H, &&op_FPP, &&op_FPP, &&op_OUT_W, &&op_OUT_W, 
		case 120: goto op_OUT_B; break;
		case 121: goto op_OUT_B; break;
		case 122: goto op_OUT_H; break;
		case 123: goto op_OUT_H; break;
		case 124: goto op_FPP; break;
		case 125: goto op_FPP; break;
		case 126: goto op_OUT_W; break;
		case 127: goto op_OUT_W; break;

	}

//	goto *op_goto_table[tmpop >> 9];

	// Bit string subopcodes
        #define DO_AM_BSTR()							\
            const uint32 arg1 = (tmpop >> 5) & 0x1F;				\
            const uint32 arg2 = (tmpop & 0x1F);					\
            RB_INCPCBY2();


        #define DO_AM_FPP()							\
            const uint32 arg1 = (tmpop >> 5) & 0x1F;				\
            const uint32 arg2 = (tmpop & 0x1F);					\
            const uint32 arg3 = ((RB_RDOP(2) >> 10)&0x3F);			\
	    RB_INCPCBY4();


        #define DO_AM_UDEF()					\
            RB_INCPCBY2();

        #define DO_AM_I()					\
            const uint32 arg1 = tmpop & 0x1F;			\
            const uint32 arg2 = (tmpop >> 5) & 0x1F;		\
            RB_INCPCBY2();
						
	#define DO_AM_II() DO_AM_I();


        #define DO_AM_IV()					\
	    const uint32 arg1 = ((tmpop & 0x000003FF) << 16) | RB_RDOP(2);	\


        #define DO_AM_V()					\
            const uint32 arg3 = (tmpop >> 5) & 0x1F;		\
            const uint32 arg2 = tmpop & 0x1F;			\
            const uint32 arg1 = RB_RDOP(2);	\
            RB_INCPCBY4();						


        #define DO_AM_VIa()					\
            const uint32 arg1 = RB_RDOP(2);	\
            const uint32 arg2 = tmpop & 0x1F;			\
            const uint32 arg3 = (tmpop >> 5) & 0x1F;		\
            RB_INCPCBY4();						\


        #define DO_AM_VIb()					\
            const uint32 arg1 = (tmpop >> 5) & 0x1F;		\
            const uint32 arg2 = RB_RDOP(2);	\
            const uint32 arg3 = (tmpop & 0x1F);			\
            RB_INCPCBY4();					\

        #define DO_AM_IX()					\
            const uint32 arg1 = (tmpop & 0x1);			\
            RB_INCPCBY2();					\

        #define DO_AM_III()					\
            const uint32 arg1 = tmpop & 0x1FE;

	#include "v810_do_am.h"

	 #define BEGIN_OP(meowtmpop) { op_##meowtmpop: DO_##meowtmpop ##_AM();
	 #define END_OP()	goto OpFinished; }
	 #define END_OP_SKIPLO()       goto OpFinishedSkipLO; }

	BEGIN_OP(MOV);
	    ADDCLOCK(1);
            SetPREG(arg2, P_REG[arg1]);
	END_OP();


	BEGIN_OP(ADD);
             ADDCLOCK(1);
             uint32 temp = P_REG[arg2] + P_REG[arg1];

             SetFlag(PSW_OV, (bool)(((P_REG[arg2]^(~P_REG[arg1]))&(P_REG[arg2]^temp))&0x80000000));
             SetFlag(PSW_CY, temp < P_REG[arg2]);

             SetPREG(arg2, temp);
	     SetSZ(P_REG[arg2]);
	END_OP();


	BEGIN_OP(SUB);
             ADDCLOCK(1);
	     uint32 temp = P_REG[arg2] - P_REG[arg1];

             SetFlag(PSW_OV, (bool)(((P_REG[arg2]^P_REG[arg1])&(P_REG[arg2]^temp))&0x80000000));
             SetFlag(PSW_CY, temp > P_REG[arg2]);

	     SetPREG(arg2, temp);
	     SetSZ(P_REG[arg2]);
	END_OP();


	BEGIN_OP(CMP);
             ADDCLOCK(1);
 	     uint32 temp = P_REG[arg2] - P_REG[arg1];

	     SetSZ(temp);
             SetFlag(PSW_OV, (bool)(((P_REG[arg2]^P_REG[arg1])&(P_REG[arg2]^temp))&0x80000000));
	     SetFlag(PSW_CY, temp > P_REG[arg2]);
	END_OP();


	BEGIN_OP(SHL);
            ADDCLOCK(1);
            val = P_REG[arg1] & 0x1F;

            // set CY before we destroy the regisrer info....
            SetFlag(PSW_CY, (val != 0) && ((P_REG[arg2] >> (32 - val))&0x01) );
	    SetFlag(PSW_OV, FALSE);
            SetPREG(arg2, P_REG[arg2] << val);
	    SetSZ(P_REG[arg2]);            
	END_OP();
			
	BEGIN_OP(SHR);
            ADDCLOCK(1);
            val = P_REG[arg1] & 0x1F;
            // set CY before we destroy the regisrer info....
            SetFlag(PSW_CY, (val) && ((P_REG[arg2] >> (val-1))&0x01));
	    SetFlag(PSW_OV, FALSE);
	    SetPREG(arg2, P_REG[arg2] >> val);
	    SetSZ(P_REG[arg2]);
	END_OP();

	BEGIN_OP(JMP);

	    (void)arg2;		// arg2 is unused.

            ADDCLOCK(3);
            RB_SETPC((P_REG[arg1] & 0xFFFFFFFE));
	    if(RB_AccurateMode)
	    {
	     BRANCH_ALIGN_CHECK(PC);
	    }
	    if(RB_DebugMode)
	     ADDBT(RB_GETPC());
	END_OP();

	BEGIN_OP(SAR);
            ADDCLOCK(1);
            val = P_REG[arg1] & 0x1F;
			
	    SetFlag(PSW_CY, (val) && ((P_REG[arg2]>>(val-1))&0x01) );
	    SetFlag(PSW_OV, FALSE);

	    SetPREG(arg2, (uint32) ((int32)P_REG[arg2] >> val));
            
	    SetSZ(P_REG[arg2]);
	END_OP();

	BEGIN_OP(OR);
            ADDCLOCK(1);
            SetPREG(arg2, P_REG[arg1] | P_REG[arg2]);
	    SetFlag(PSW_OV, FALSE);
	    SetSZ(P_REG[arg2]);
	END_OP();

	BEGIN_OP(AND);
            ADDCLOCK(1);
            SetPREG(arg2, P_REG[arg1] & P_REG[arg2]);
	    SetFlag(PSW_OV, FALSE);
	    SetSZ(P_REG[arg2]);
	END_OP();

	BEGIN_OP(XOR);
            ADDCLOCK(1);
	    SetPREG(arg2, P_REG[arg1] ^ P_REG[arg2]);
	    SetFlag(PSW_OV, FALSE);
	    SetSZ(P_REG[arg2]);
	END_OP();

	BEGIN_OP(NOT);
            ADDCLOCK(1);
	    SetPREG(arg2, ~P_REG[arg1]);
	    SetFlag(PSW_OV, FALSE);
	    SetSZ(P_REG[arg2]);
	END_OP();

	BEGIN_OP(MOV_I);
            ADDCLOCK(1);
            SetPREG(arg2,sign_5(arg1));
	END_OP();

	BEGIN_OP(ADD_I);
             ADDCLOCK(1);
             uint32 temp = P_REG[arg2] + sign_5(arg1);

             SetFlag(PSW_OV, (bool)(((P_REG[arg2]^(~sign_5(arg1)))&(P_REG[arg2]^temp))&0x80000000));
	     SetFlag(PSW_CY, (uint32)temp < P_REG[arg2]);

             SetPREG(arg2, (uint32)temp);
	     SetSZ(P_REG[arg2]);
	END_OP();


	BEGIN_OP(SETF);
		ADDCLOCK(1);

		//SETF may contain bugs
		P_REG[arg2] = 0;

		//if(arg1 != 0xe)
		//printf("SETF: %02x\n", arg1);
		//snortus();
		switch (arg1 & 0x0F) 
		{
			case COND_V:
				if (TESTCOND_V) P_REG[arg2] = 1;
				break;
			case COND_C:
				if (TESTCOND_C) P_REG[arg2] = 1;
				break;
			case COND_Z:
				if (TESTCOND_Z) P_REG[arg2] = 1;
				break;
			case COND_NH:
				if (TESTCOND_NH) P_REG[arg2] = 1;
				break;
			case COND_S:
				if (TESTCOND_S) P_REG[arg2] = 1;
				break;
			case COND_T:
				P_REG[arg2] = 1;
				break;
			case COND_LT:
				if (TESTCOND_LT) P_REG[arg2] = 1;
				break;
			case COND_LE:
				if (TESTCOND_LE) P_REG[arg2] = 1;
				break;
			case COND_NV:
				if (TESTCOND_NV) P_REG[arg2] = 1;
				break;
			case COND_NC:
				if (TESTCOND_NC) P_REG[arg2] = 1;
				break;
			case COND_NZ:
				if (TESTCOND_NZ) P_REG[arg2] = 1;
				break;
			case COND_H:
				if (TESTCOND_H) P_REG[arg2] = 1;
				break;
			case COND_NS:
				if (TESTCOND_NS) P_REG[arg2] = 1;
				break;
			case COND_F:
				//always false! do nothing more
				break;
			case COND_GE:
				if (TESTCOND_GE) P_REG[arg2] = 1;
				break;
			case COND_GT:
				if (TESTCOND_GT) P_REG[arg2] = 1;
				break;
		}
	END_OP();

	BEGIN_OP(CMP_I);
             ADDCLOCK(1);
	     uint32 temp = P_REG[arg2] - sign_5(arg1);

	     SetSZ(temp);
             SetFlag(PSW_OV, (bool)(((P_REG[arg2]^(sign_5(arg1)))&(P_REG[arg2]^temp))&0x80000000));
	     SetFlag(PSW_CY, temp > P_REG[arg2]);
	END_OP();

	BEGIN_OP(SHR_I);
            ADDCLOCK(1);
	    SetFlag(PSW_CY, arg1 && ((P_REG[arg2] >> (arg1-1))&0x01) );
            // set CY before we destroy the regisrer info....
            SetPREG(arg2, P_REG[arg2] >> arg1);
	    SetFlag(PSW_OV, FALSE);
	    SetSZ(P_REG[arg2]);
	END_OP();

	BEGIN_OP(SHL_I);
            ADDCLOCK(1);
            SetFlag(PSW_CY, arg1 && ((P_REG[arg2] >> (32 - arg1))&0x01) );
            // set CY before we destroy the regisrer info....

            SetPREG(arg2, P_REG[arg2] << arg1);
	    SetFlag(PSW_OV, FALSE);
	    SetSZ(P_REG[arg2]);
	END_OP();

	BEGIN_OP(SAR_I);
            ADDCLOCK(1);
 	    SetFlag(PSW_CY, arg1 && ((P_REG[arg2]>>(arg1-1))&0x01) );

            SetPREG(arg2, (uint32) ((int32)P_REG[arg2] >> arg1));

	    SetFlag(PSW_OV, FALSE);
	    SetSZ(P_REG[arg2]);
	END_OP();

	BEGIN_OP(LDSR);		// Loads a Sys Reg with the value in specified PR
             ADDCLOCK(1);	// ?
	     SetSREG(arg1 & 0x1F, P_REG[arg2 & 0x1F]);
	END_OP();

	BEGIN_OP(STSR);		// Loads a PR with the value in specified Sys Reg
             ADDCLOCK(1);	// ?
             P_REG[arg2 & 0x1F] = GetSREG(arg1 & 0x1F);
	END_OP();

        BEGIN_OP(EI);
	    (void)arg1;         // arg1 is unused.
	    (void)arg2;         // arg2 is unused.

	    if(VBMode)
	    {
             ADDCLOCK(1);
             S_REG[PSW] = S_REG[PSW] &~ PSW_ID;
	    }
	    else
	    {
	     puts("EI");
	     ADDCLOCK(1);
	     RB_DECPCBY2();
             Exception(INVALID_OP_HANDLER_ADDR, ECODE_INVALID_OP);
             CHECK_HALTED();
	    }
        END_OP();

	BEGIN_OP(DI);
            (void)arg1;         // arg1 is unused.
            (void)arg2;         // arg2 is unused.

            if(VBMode)
            {
             ADDCLOCK(1);
             S_REG[PSW] |= PSW_ID;
	    }
	    else
            {
	     puts("DI");
             ADDCLOCK(1);
	     RB_DECPCBY2();
             Exception(INVALID_OP_HANDLER_ADDR, ECODE_INVALID_OP);
             CHECK_HALTED();
            }
	END_OP();


	#define COND_BRANCH(cond)			\
		if(cond) 				\
		{ 					\
		 ADDCLOCK(3);				\
		 RB_PCRELCHANGE(sign_9(arg1) & 0xFFFFFFFE);	\
		 if(RB_AccurateMode)			\
		 {					\
		  BRANCH_ALIGN_CHECK(PC);		\
		 }					\
		 if(RB_DebugMode)			\
		 {					\
		  ADDBT(RB_GETPC());			\
		 }					\
		}					\
		else					\
		{					\
		 ADDCLOCK(1);				\
		 RB_INCPCBY2();				\
		}

	BEGIN_OP(BV);
		COND_BRANCH(TESTCOND_V);
	END_OP();


	BEGIN_OP(BL);
        	COND_BRANCH(TESTCOND_L);
	END_OP();

	BEGIN_OP(BE);
        	COND_BRANCH(TESTCOND_E);
	END_OP();

	BEGIN_OP(BNH);
          	COND_BRANCH(TESTCOND_NH);
	END_OP();

	BEGIN_OP(BN);
          	COND_BRANCH(TESTCOND_N);
	END_OP();

	BEGIN_OP(BR);
          	COND_BRANCH(TRUE);
	END_OP();

	BEGIN_OP(BLT);
          	COND_BRANCH(TESTCOND_LT);
	END_OP();

	BEGIN_OP(BLE);
          	COND_BRANCH(TESTCOND_LE);
	END_OP();

	BEGIN_OP(BNV);
          	COND_BRANCH(TESTCOND_NV);
	END_OP();

	BEGIN_OP(BNL);
          	COND_BRANCH(TESTCOND_NL);
	END_OP();

	BEGIN_OP(BNE);
          	COND_BRANCH(TESTCOND_NE);
	END_OP();

	BEGIN_OP(BH);
          	COND_BRANCH(TESTCOND_H);
	END_OP();

	BEGIN_OP(BP);
          	COND_BRANCH(TESTCOND_P);
	END_OP();

	BEGIN_OP(BGE);
          	COND_BRANCH(TESTCOND_GE);
	END_OP();

	BEGIN_OP(BGT);
          	COND_BRANCH(TESTCOND_GT);
	END_OP();

	BEGIN_OP(JR);
            ADDCLOCK(3);
            RB_PCRELCHANGE(sign_26(arg1) & 0xFFFFFFFE);
            if(RB_AccurateMode)
            {
             BRANCH_ALIGN_CHECK(PC);
            }
            if(RB_DebugMode)
             ADDBT(RB_GETPC());
	END_OP();

	BEGIN_OP(JAL);
            ADDCLOCK(3);
	    P_REG[31] = RB_GETPC() + 4;
            RB_PCRELCHANGE(sign_26(arg1) & 0xFFFFFFFE);
            if(RB_AccurateMode)
            {
             BRANCH_ALIGN_CHECK(PC);
            }
            if(RB_DebugMode)
             ADDBT(RB_GETPC());
	END_OP();

	BEGIN_OP(MOVEA);
            ADDCLOCK(1);
	    SetPREG(arg3, P_REG[arg2] + sign_16(arg1));
	END_OP();

	BEGIN_OP(ADDI);
             ADDCLOCK(1);
             uint32 temp = P_REG[arg2] + sign_16(arg1);

             SetFlag(PSW_OV, (bool)(((P_REG[arg2]^(~sign_16(arg1)))&(P_REG[arg2]^temp))&0x80000000));
	     SetFlag(PSW_CY, (uint32)temp < P_REG[arg2]);

             SetPREG(arg3, (uint32)temp);
	     SetSZ(P_REG[arg3]);
	END_OP();

	BEGIN_OP(ORI);
            ADDCLOCK(1);
            SetPREG(arg3, arg1 | P_REG[arg2]);
	    SetFlag(PSW_OV, FALSE);
	    SetSZ(P_REG[arg3]);
	END_OP();

	BEGIN_OP(ANDI);
            ADDCLOCK(1);
            SetPREG(arg3, (arg1 & P_REG[arg2]));
	    SetFlag(PSW_OV, FALSE);
	    SetSZ(P_REG[arg3]);
	END_OP();

	BEGIN_OP(XORI);
            ADDCLOCK(1);
	    SetPREG(arg3, arg1 ^ P_REG[arg2]);
	    SetFlag(PSW_OV, FALSE);
	    SetSZ(P_REG[arg3]);
	END_OP();

	BEGIN_OP(MOVHI);
            ADDCLOCK(1);
            SetPREG(arg3, (arg1 << 16) + P_REG[arg2]);
	END_OP();

	// LD.B
	BEGIN_OP(LD_B);
		        ADDCLOCK(1);
			tmp2 = (sign_16(arg1)+P_REG[arg2])&0xFFFFFFFF;
			
			SetPREG(arg3, sign_8(MemRead8(v810_timestamp, tmp2)));

			//should be 3 clocks when executed alone, 2 when precedes another LD, or 1
			//when precedes an instruction with many clocks (I'm guessing FP, MUL, DIV, etc)
			if(lastop >= 0)
			{
				if(lastop == LASTOP_LD)
				{ 
				 ADDCLOCK(1);
				}
				else
				{
				 ADDCLOCK(2);
				}
			}
			lastop = LASTOP_LD;
	END_OP_SKIPLO();

	// LD.H
	BEGIN_OP(LD_H);
                        ADDCLOCK(1);
			tmp2 = (sign_16(arg1)+P_REG[arg2]) & 0xFFFFFFFE;
		        SetPREG(arg3, sign_16(MemRead16(v810_timestamp, tmp2)));

		        if(lastop >= 0)
			{
                                if(lastop == LASTOP_LD)
				{
				 ADDCLOCK(1);
				}
                                else
				{
				 ADDCLOCK(2);
				}
                        }
			lastop = LASTOP_LD;
	END_OP_SKIPLO();


	// LD.W
	BEGIN_OP(LD_W);
                        ADDCLOCK(1);

                        tmp2 = (sign_16(arg1)+P_REG[arg2]) & 0xFFFFFFFC;

	                if(MemReadBus32[tmp2 >> 24])
			{
			 SetPREG(arg3, MemRead32(v810_timestamp, tmp2));
			
			 if(lastop >= 0)
			 {
				if(lastop == LASTOP_LD)
				{
				 ADDCLOCK(1);
				}
				else
				{
				 ADDCLOCK(2);
				}
			 }
			}
			else
			{
                         SetPREG(arg3, MemRead16(v810_timestamp, tmp2) | (MemRead16(v810_timestamp, tmp2 | 2) << 16));

                         if(lastop >= 0)
                         {
                                if(lastop == LASTOP_LD)
				{
				 ADDCLOCK(3);
				}
                                else
				{
				 ADDCLOCK(4);
				}
                         }
			}
			lastop = LASTOP_LD;
	END_OP_SKIPLO();

	// ST.B
	BEGIN_OP(ST_B);
             ADDCLOCK(1);
             MemWrite8(v810_timestamp, sign_16(arg2)+P_REG[arg3], P_REG[arg1] & 0xFF);

             if(lastop == LASTOP_ST)
	     {
	      ADDCLOCK(1); 
	     }
	     lastop = LASTOP_ST;
	END_OP_SKIPLO();

	// ST.H
	BEGIN_OP(ST_H);
             ADDCLOCK(1);

             MemWrite16(v810_timestamp, (sign_16(arg2)+P_REG[arg3])&0xFFFFFFFE, P_REG[arg1] & 0xFFFF);

             if(lastop == LASTOP_ST)
	     {
	      ADDCLOCK(1);
	     }
	     lastop = LASTOP_ST;
	END_OP_SKIPLO();

	// ST.W
	BEGIN_OP(ST_W);
             ADDCLOCK(1);
  	     tmp2 = (sign_16(arg2)+P_REG[arg3]) & 0xFFFFFFFC;

	     if(MemWriteBus32[tmp2 >> 24])
	     {
	      MemWrite32(v810_timestamp, tmp2, P_REG[arg1]);

              if(lastop == LASTOP_ST)
	      {
	       ADDCLOCK(1);
	      }
	     }
	     else
	     {
              MemWrite16(v810_timestamp, tmp2, P_REG[arg1] & 0xFFFF);
              MemWrite16(v810_timestamp, tmp2 | 2, P_REG[arg1] >> 16);

              if(lastop == LASTOP_ST)
	      {
	       ADDCLOCK(3);
	      }
	     }
	     lastop = LASTOP_ST;
	END_OP_SKIPLO();

	// IN.B
	BEGIN_OP(IN_B);
	    {
             ADDCLOCK(3);
             SetPREG(arg3, IORead8(v810_timestamp, sign_16(arg1)+P_REG[arg2]));
	    }
	    lastop = LASTOP_IN;
	END_OP_SKIPLO();


	// IN.H
	BEGIN_OP(IN_H);
	    {
             ADDCLOCK(3);
             SetPREG(arg3, IORead16(v810_timestamp, (sign_16(arg1)+P_REG[arg2]) & 0xFFFFFFFE));
	    }
	    lastop = LASTOP_IN;
	END_OP_SKIPLO();


	// IN.W
	BEGIN_OP(IN_W);
	     if(IORead32)
	     {
              ADDCLOCK(3);
              SetPREG(arg3, IORead32(v810_timestamp, (sign_16(arg1)+P_REG[arg2]) & 0xFFFFFFFC));
	     }
	     else
	     {
	      uint32 eff_addr = (sign_16(arg1) + P_REG[arg2]) & 0xFFFFFFFC;

	      ADDCLOCK(5);
              SetPREG(arg3, IORead16(v810_timestamp, eff_addr) | ((IORead16(v810_timestamp, eff_addr | 2) << 16)));
	     }
	     lastop = LASTOP_IN;
	END_OP_SKIPLO();


	// OUT.B
	BEGIN_OP(OUT_B);
             ADDCLOCK(1);
             IOWrite8(v810_timestamp, sign_16(arg2)+P_REG[arg3],P_REG[arg1]&0xFF);

	     if(lastop == LASTOP_OUT)
	     {
	      ADDCLOCK(1); 
	     }
	     lastop = LASTOP_OUT;
	END_OP_SKIPLO();


	// OUT.H
	BEGIN_OP(OUT_H);
             ADDCLOCK(1);
             IOWrite16(v810_timestamp, (sign_16(arg2)+P_REG[arg3])&0xFFFFFFFE,P_REG[arg1]&0xFFFF);

             if(lastop == LASTOP_OUT)
             {
              ADDCLOCK(1);
             }
	     lastop = LASTOP_OUT;
	END_OP_SKIPLO();


	// OUT.W
	BEGIN_OP(OUT_W);
             ADDCLOCK(1);

	     if(IOWrite32)
              IOWrite32(v810_timestamp, (sign_16(arg2)+P_REG[arg3])&0xFFFFFFFC,P_REG[arg1]);
	     else
	     {
	      uint32 eff_addr = (sign_16(arg2)+P_REG[arg3])&0xFFFFFFFC;
              IOWrite16(v810_timestamp, eff_addr, P_REG[arg1] & 0xFFFF);
              IOWrite16(v810_timestamp, eff_addr | 2, P_REG[arg1] >> 16);
	     }

             if(lastop == LASTOP_OUT)
             {
	      if(IOWrite32)
              {
	       ADDCLOCK(1);
	      }
	      else
	      {
	       ADDCLOCK(3);
	      }	      
             }
	     lastop = LASTOP_OUT;
	END_OP_SKIPLO();

	BEGIN_OP(NOP);
            (void)arg1;         // arg1 is unused.

            ADDCLOCK(1);
	    RB_INCPCBY2();
	END_OP();

	BEGIN_OP(RETI);
            (void)arg1;         // arg1 is unused.

            ADDCLOCK(10);

            //Return from Trap/Interupt
            if(S_REG[PSW] & PSW_NP) { // Read the FE Reg
                RB_SETPC(S_REG[FEPC] & 0xFFFFFFFE);
                S_REG[PSW] = S_REG[FEPSW];
            } else { 	//Read the EI Reg Interupt
                RB_SETPC(S_REG[EIPC] & 0xFFFFFFFE);
                S_REG[PSW] = S_REG[EIPSW];
            }
            if(RB_DebugMode)
             ADDBT(RB_GETPC());
	END_OP();

	BEGIN_OP(MUL);
             ADDCLOCK(13);

             uint64 temp = (int64)(int32)P_REG[arg1] * (int32)P_REG[arg2];

             SetPREG(30, (uint32)(temp >> 32));
             SetPREG(arg2, (uint32)temp);
	     SetSZ(P_REG[arg2]);
	     SetFlag(PSW_OV, temp != (uint32)temp);
	     lastop = -1;
	END_OP_SKIPLO();

	BEGIN_OP(MULU);
             ADDCLOCK(13);
             uint64 temp = (uint64)P_REG[arg1] * (uint64)P_REG[arg2];

             SetPREG(30, (uint32)(temp >> 32));
 	     SetPREG(arg2, (uint32)temp);

	     SetSZ(P_REG[arg2]);
	     SetFlag(PSW_OV, temp != (uint32)temp);
	     lastop = -1;
	END_OP_SKIPLO();

	BEGIN_OP(DIVU);
            ADDCLOCK(36);
            if(P_REG[arg1] == 0) // Divide by zero!
	    {
	     RB_DECPCBY2();
	     Exception(ZERO_DIV_HANDLER_ADDR, ECODE_ZERO_DIV);
	     CHECK_HALTED();
            } 
	    else 
	    {
	     // Careful here, since arg2 can be == 30
	     uint32 quotient = (uint32)P_REG[arg2] / (uint32)P_REG[arg1];
	     uint32 remainder = (uint32)P_REG[arg2] % (uint32)P_REG[arg1];

	     SetPREG(30, remainder);
             SetPREG(arg2, quotient);

	     SetFlag(PSW_OV, FALSE);
	     SetSZ(quotient);
            }
	    lastop = -1;
	END_OP_SKIPLO();

	BEGIN_OP(DIV);
             //if(P_REG[arg1] & P_REG[arg2] & 0x80000000)
             //{
             // printf("Div: %08x %08x\n", P_REG[arg1], P_REG[arg2]);
             //}

            ADDCLOCK(38);
            if((uint32)P_REG[arg1] == 0) // Divide by zero!
	    { 
	     RB_DECPCBY2();
	     Exception(ZERO_DIV_HANDLER_ADDR, ECODE_ZERO_DIV);
	     CHECK_HALTED();
            } 
	    else 
	    {
                if((P_REG[arg2]==0x80000000)&&(P_REG[arg1]==0xFFFFFFFF)) 
		{
			SetFlag(PSW_OV, TRUE);
			P_REG[30]=0;
	                SetPREG(arg2, 0x80000000);
	                SetSZ(P_REG[arg2]);
		}
		else
		{
		     // Careful here, since arg2 can be == 30
        	     uint32 quotient = (int32)P_REG[arg2] / (int32)P_REG[arg1];
	             uint32 remainder = (int32)P_REG[arg2] % (int32)P_REG[arg1];

	             SetPREG(30, remainder);
	             SetPREG(arg2, quotient);

	             SetFlag(PSW_OV, FALSE);
	             SetSZ(quotient);
		}
	    }
	    lastop = -1;
	END_OP_SKIPLO();

	BEGIN_OP(FPP);
            ADDCLOCK(1);
	    fpu_subop(v810_timestamp, arg3, arg1, arg2);
	    lastop = -1;
	    CHECK_HALTED();
	END_OP_SKIPLO();

	BEGIN_OP(BSTR);
	    if(!in_bstr)
	    {
             ADDCLOCK(1);
	    }

            if(bstr_subop(v810_timestamp, arg2, arg1))
	    {
	     RB_DECPCBY2();
             in_bstr = TRUE;
             in_bstr_to = tmpop;
	    }
	    else
	    {
	     in_bstr = FALSE;
	     have_src_cache = have_dst_cache = FALSE;
	    }
	END_OP();

	BEGIN_OP(HALT);
            (void)arg1;         // arg1 is unused.

            ADDCLOCK(1);
	    Halted = HALT_HALT;
            //printf("Untested opcode: HALT\n");
	END_OP();

	BEGIN_OP(TRAP);
            (void)arg2;         // arg2 is unused.

            ADDCLOCK(15);

	    Exception(TRAP_HANDLER_BASE + (arg1 & 0x10), ECODE_TRAP_BASE + (arg1 & 0x1F));
	    CHECK_HALTED();
	END_OP();

	BEGIN_OP(CAXI);
            //printf("Untested opcode: caxi\n");

	    // Lock bus(N/A)

            ADDCLOCK(26);

	    {
	     uint32 addr, tmp, compare_temp;
	     uint32 to_write;

             addr = sign_16(arg1) + P_REG[arg2];
	     addr &= ~3;

	     if(MemReadBus32[addr >> 24])
	      tmp = MemRead32(v810_timestamp, addr);
	     else
	      tmp = MemRead16(v810_timestamp, addr) | (MemRead16(v810_timestamp, addr | 2) << 16);

             compare_temp = P_REG[arg3] - tmp;

             SetSZ(compare_temp);
             SetFlag(PSW_OV, (bool)(((P_REG[arg3]^tmp)&(P_REG[arg3]^compare_temp))&0x80000000));
             SetFlag(PSW_CY, compare_temp > P_REG[arg3]);

	     if(!compare_temp) // If they're equal...
	      to_write = P_REG[30];
	     else
	      to_write = tmp;

	     if(MemWriteBus32[addr >> 24])
	      MemWrite32(v810_timestamp, addr, to_write);
	     else
	     {
              MemWrite16(v810_timestamp, addr, to_write & 0xFFFF);
              MemWrite16(v810_timestamp, addr | 2, to_write >> 16);
	     }
	     P_REG[arg3] = tmp;
	    }

	    // Unlock bus(N/A)

	END_OP();

	BEGIN_OP(INVALID);
	    RB_DECPCBY2();
	    if(!RB_AccurateMode)
	    {
	     RB_SETPC(RB_GETPC());
	     if((uint32)(RB_RDOP(0, 0) >> 9) != opcode)
	     {
	      //printf("Trampoline: %08x %02x\n", RB_GETPC(), opcode >> 1);
	     }
	     else
	     {
              ADDCLOCK(1);
              Exception(INVALID_OP_HANDLER_ADDR, ECODE_INVALID_OP);
              CHECK_HALTED();
	     }
	    }
	    else
	    {
	     ADDCLOCK(1);
	     Exception(INVALID_OP_HANDLER_ADDR, ECODE_INVALID_OP);
	     CHECK_HALTED();
	    }
	END_OP();

	}

	OpFinished:	;
	lastop = opcode;
	OpFinishedSkipLO: ;
     }	// end  while(v810_timestamp < next_event_ts)
     next_event_ts = event_handler(v810_timestamp);
     //printf("Next: %d, Cur: %d\n", next_event_ts, v810_timestamp);
    }

}

#ifdef WANT_DEBUGGER
void V810::Run_Accurate_Debug(int32 MDFN_FASTCALL (*event_handler)(const v810_timestamp_t timestamp))
{
 const bool RB_AccurateMode = true;
 const bool RB_DebugMode = true;

 #include "v810_oploop.inc"
}
#endif


#undef RB_GETPC
#define RB_GETPC()      ((uint32)(PC_ptr - PC_base))

void V810::Run_Fast(int32 MDFN_FASTCALL (*event_handler)(const v810_timestamp_t timestamp))
{
 const bool RB_AccurateMode = false;
 const bool RB_DebugMode = false;

 #include "v810_oploop.inc"
}

#ifdef WANT_DEBUGGER
void V810::Run_Fast_Debug(int32 MDFN_FASTCALL (*event_handler)(const v810_timestamp_t timestamp))
{
 const bool RB_AccurateMode = false;
 const bool RB_DebugMode = true;

 #include "v810_oploop.inc"
}
#endif

#undef RB_GETPC

v810_timestamp_t V810::Run(int32 MDFN_FASTCALL (*event_handler)(const v810_timestamp_t timestamp))
{
 Running = true;
#ifdef WANT_DEBUGGER
 if(CPUHook)
 {
  if(EmuMode == V810_EMU_MODE_FAST)
   Run_Fast_Debug(event_handler);
  else
   Run_Accurate_Debug(event_handler);
 }
 else
#endif
 {
  if(EmuMode == V810_EMU_MODE_FAST)
   Run_Fast(event_handler);
  else
   Run_Accurate(event_handler);
 }
 return(v810_timestamp);
}

void V810::Exit(void)
{
 Running = false;
}
#ifdef WANT_DEBUGGER
void V810::SetCPUHook(void (*newhook)(uint32 PC), void (*new_ADDBT)(uint32 PC))
{
 CPUHook = newhook;
 ADDBT = new_ADDBT;
}
#endif
//#endif

uint32 V810::GetPC(void)
{
 if(EmuMode == V810_EMU_MODE_ACCURATE)
  return(PC);
 else
 {
  return(PC_ptr - PC_base);
 }
}

void V810::SetPC(uint32 new_pc)
{
 if(EmuMode == V810_EMU_MODE_ACCURATE)
  PC = new_pc;
 else
 {
  PC_ptr = &v810_fast_map[new_pc >> V810_FAST_MAP_SHIFT][new_pc];
  PC_base = PC_ptr - new_pc;
 }
}

uint32 V810::GetPR(const unsigned int which)
{
 assert(which <= 0x1F);


 return(which ? P_REG[which] : 0);
}

void V810::SetPR(const unsigned int which, uint32 value)
{
 assert(which <= 0x1F);

 if(which)
  P_REG[which] = value;
}

uint32 V810::GetSR(const unsigned int which)
{
 assert(which <= 0x1F);

 return(GetSREG(which));
}

void V810::SetSR(const unsigned int which, uint32 value)
{
 assert(which <= 0x1F);

 SetSREG(which, value);
}


#define BSTR_OP_MOV dst_cache &= ~(1 << dstoff); dst_cache |= ((src_cache >> srcoff) & 1) << dstoff;
#define BSTR_OP_NOT dst_cache &= ~(1 << dstoff); dst_cache |= (((src_cache >> srcoff) & 1) ^ 1) << dstoff;

#define BSTR_OP_XOR dst_cache ^= ((src_cache >> srcoff) & 1) << dstoff;
#define BSTR_OP_OR dst_cache |= ((src_cache >> srcoff) & 1) << dstoff;
#define BSTR_OP_AND dst_cache &= ~((((src_cache >> srcoff) & 1) ^ 1) << dstoff);

#define BSTR_OP_XORN dst_cache ^= (((src_cache >> srcoff) & 1) ^ 1) << dstoff;
#define BSTR_OP_ORN dst_cache |= (((src_cache >> srcoff) & 1) ^ 1) << dstoff;
#define BSTR_OP_ANDN dst_cache &= ~(((src_cache >> srcoff) & 1) << dstoff);

INLINE uint32 V810::BSTR_RWORD(v810_timestamp_t &timestamp, uint32 A)
{
 if(MemReadBus32[A >> 24])
 {
  timestamp += 2;
  return(MemRead32(timestamp, A));
 }
 else
 {
  uint32 ret;

  timestamp += 2;
  ret = MemRead16(timestamp, A);
 
  timestamp += 2;
  ret |= MemRead16(timestamp, A | 2) << 16;
  return(ret);
 }
}

INLINE void V810::BSTR_WWORD(v810_timestamp_t &timestamp, uint32 A, uint32 V)
{
 if(MemWriteBus32[A >> 24])
 {
  timestamp += 2;
  MemWrite32(timestamp, A, V);
 }
 else
 {
  timestamp += 2;
  MemWrite16(timestamp, A, V & 0xFFFF);

  timestamp += 2;
  MemWrite16(timestamp, A | 2, V >> 16);
 }
}

#define DO_BSTR(op) { 						\
                while(len)					\
                {						\
                 if(!have_src_cache)                            \
                 {                                              \
		  have_src_cache = TRUE;			\
                  src_cache = BSTR_RWORD(timestamp, src);       \
                  src += 4;                                     \
                 }                                              \
								\
		 if(!have_dst_cache)				\
		 {						\
		  have_dst_cache = TRUE;			\
                  dst_cache = BSTR_RWORD(timestamp, dst);       \
                 }                                              \
								\
		 op;						\
                 srcoff = (srcoff + 1) & 0x1F;			\
                 dstoff = (dstoff + 1) & 0x1F;			\
		 len--;						\
								\
		 if(!srcoff)					\
		  have_src_cache = FALSE;			\
								\
                 if(!dstoff)                                    \
                 {                                              \
                  BSTR_WWORD(timestamp, dst, dst_cache);        \
                  dst += 4;                                     \
		  have_dst_cache = FALSE;			\
		  if(timestamp >= next_event_ts)		\
		   break;					\
                 }                                              \
                }						\
                if(have_dst_cache)				\
                 BSTR_WWORD(timestamp, dst, dst_cache);		\
		}

INLINE bool V810::Do_BSTR_Search(v810_timestamp_t &timestamp, const int inc_mul, unsigned int bit_test)
{
        uint32 srcoff = (P_REG[27] & 0x1F);
        uint32 len = P_REG[28];
        uint32 bits_skipped = P_REG[29];
        uint32 src = (P_REG[30] & 0xFFFFFFFC);
	bool found = false;

	#if 0
	// TODO: Better timing.
	if(!in_bstr)	// If we're just starting the execution of this instruction(kind of spaghetti-code), so FIXME if we change
			// bstr handling in v810_oploop.inc
	{
	 timestamp += 13 - 1;
	}
	#endif

	while(len)
	{
		if(!have_src_cache)
		{
		 have_src_cache = TRUE;
		 timestamp++;
		 src_cache = BSTR_RWORD(timestamp, src);
		 src += inc_mul * 4;
		}

		if(((src_cache >> srcoff) & 1) == bit_test)
		{
		 found = true;

		 /* Fix the bit offset and word address to "1 bit before" it was found */
		 srcoff -= inc_mul * 1;
		 if(srcoff & 0x20)		/* Handles 0x1F->0x20(0x00) and 0x00->0xFFFF... */
		 {
		  src -= inc_mul * 4;
		  srcoff &= 0x1F;
		 }
		 break;
		}
	        srcoff = (srcoff + inc_mul * 1) & 0x1F;
		bits_skipped++;
	        len--;

	        if(!srcoff)
		{
	         have_src_cache = FALSE;
		 if(timestamp >= next_event_ts)
		  break;
		}
	}

        P_REG[27] = srcoff;
        P_REG[28] = len;
        P_REG[29] = bits_skipped;
        P_REG[30] = src;


        if(found)               // Set Z flag to 0 if the bit was found
         SetFlag(PSW_Z, 0);
        else if(!len)           // ...and if the search is over, and the bit was not found, set it to 1
         SetFlag(PSW_Z, 1);

        if(found)               // Bit found, so don't continue the search.
         return(false);

        return((bool)(len));      // Continue the search if any bits are left to search.
}

bool V810::bstr_subop(v810_timestamp_t &timestamp, int sub_op, int arg1)
{
 if((sub_op >= 0x10) || (!(sub_op & 0x8) && sub_op >= 0x4))
 {
  printf("%08x\tBSR Error: %04x\n", PC,sub_op);

  SetPC(GetPC() - 2);
  Exception(INVALID_OP_HANDLER_ADDR, ECODE_INVALID_OP);

  return(false);
 }

// printf("BSTR: %02x, %02x %02x; src: %08x, dst: %08x, len: %08x\n", sub_op, P_REG[27], P_REG[26], P_REG[30], P_REG[29], P_REG[28]);

 if(sub_op & 0x08)
 {
	uint32 dstoff = (P_REG[26] & 0x1F);
	uint32 srcoff = (P_REG[27] & 0x1F);
	uint32 len =     P_REG[28];
	uint32 dst =    (P_REG[29] & 0xFFFFFFFC);
	uint32 src =    (P_REG[30] & 0xFFFFFFFC);

	switch(sub_op)
	{
	 case ORBSU: DO_BSTR(BSTR_OP_OR); break;

	 case ANDBSU: DO_BSTR(BSTR_OP_AND); break;

	 case XORBSU: DO_BSTR(BSTR_OP_XOR); break;

	 case MOVBSU: DO_BSTR(BSTR_OP_MOV); break;

	 case ORNBSU: DO_BSTR(BSTR_OP_ORN); break;

	 case ANDNBSU: DO_BSTR(BSTR_OP_ANDN); break;

	 case XORNBSU: DO_BSTR(BSTR_OP_XORN); break;

	 case NOTBSU: DO_BSTR(BSTR_OP_NOT); break;
	}

        P_REG[26] = dstoff; 
        P_REG[27] = srcoff;
        P_REG[28] = len;
        P_REG[29] = dst;
        P_REG[30] = src;

	return((bool)(P_REG[28]));
 }
 else
 {
  printf("BSTR Search: %02x\n", sub_op);
  return(Do_BSTR_Search(timestamp, ((sub_op & 1) ? -1 : 1), (sub_op & 0x2) >> 1));
 }
 assert(0);
 return(false);
}

INLINE void V810::SetFPUOPNonFPUFlags(uint32 result)
{
                 // Now, handle flag setting
                 SetFlag(PSW_OV, 0);

                 if(!(result & 0x7FFFFFFF)) // Check to see if exponent and mantissa are 0
		 {
		  // If Z flag is set, S and CY should be clear, even if it's negative 0(confirmed on real thing with subf.s, at least).
                  SetFlag(PSW_Z, 1);
                  SetFlag(PSW_S, 0);
                  SetFlag(PSW_CY, 0);
		 }
                 else
		 {
                  SetFlag(PSW_Z, 0);
                  SetFlag(PSW_S, (bool)(result & 0x80000000));
                  SetFlag(PSW_CY, (bool)(result & 0x80000000));
		 }
                 //printf("MEOW: %08x\n", S_REG[PSW] & (PSW_S | PSW_CY));
}

INLINE bool V810::CheckFPInputException(uint32 fpval)
{
 // Zero isn't a subnormal! (OR IS IT *DUN DUN DUNNN* ;b)
 if(!(fpval & 0x7FFFFFFF))
  return(false);

 switch((fpval >> 23) & 0xFF)
 {
  case 0x00: // Subnormal		
  case 0xFF: // NaN or infinity
	{
	 //puts("New FPU FRO");

	 S_REG[PSW] |= PSW_FRO;

	 SetPC(GetPC() - 4);
	 Exception(FPU_HANDLER_ADDR, ECODE_FRO);
	}
	return(true);	// Yes, exception occurred
 }
 return(false);	// No, no exception occurred.
}

bool V810::FPU_DoesExceptionKillResult(void)
{
 if(float_exception_flags & float_flag_invalid)
  return(true);

 if(float_exception_flags & float_flag_divbyzero)
  return(true);


 // Return false here, so that the result of this calculation IS put in the output register.
 // (Incidentally, to get the result of operations on overflow to match a real V810, required a bit of hacking of the SoftFloat code to "wrap" the exponent
 // on overflow,
 // rather than generating an infinity.  The wrapping behavior is specified in IEE 754 AFAIK, and is useful in cases where you divide a huge number
 // by another huge number, and fix the result afterwards based on the number of overflows that occurred.  Probably requires some custom assembly code,
 // though.  And it's the kind of thing you'd see in an engineering or physics program, not in a perverted video game :b).
 // Oh, and just a note to self, FPR is NOT set when an overflow occurs.  Or it is in certain cases?
 if(float_exception_flags & float_flag_overflow)
  return(false);

 return(false);
}

void V810::FPU_DoException(void)
{
 if(float_exception_flags & float_flag_invalid)
 {
  //puts("New FPU Invalid");

  S_REG[PSW] |= PSW_FIV;

  SetPC(GetPC() - 4);
  Exception(FPU_HANDLER_ADDR, ECODE_FIV);

  return;
 }

 if(float_exception_flags & float_flag_divbyzero)
 {
  //puts("New FPU Divide by Zero");

  S_REG[PSW] |= PSW_FZD;

  SetPC(GetPC() - 4);
  Exception(FPU_HANDLER_ADDR, ECODE_FZD);

  return;
 }

 if(float_exception_flags & float_flag_underflow)
 {
  //puts("New FPU Underflow");

  S_REG[PSW] |= PSW_FUD;
 }

 if(float_exception_flags & float_flag_inexact)
 {
  S_REG[PSW] |= PSW_FPR;
  //puts("New FPU Precision Degradation");
 }

 // FPR can be set along with overflow, so put the overflow exception handling at the end here(for Exception() messes with PSW).
 if(float_exception_flags & float_flag_overflow)
 {
  //puts("New FPU Overflow");

  S_REG[PSW] |= PSW_FOV;

  SetPC(GetPC() - 4);
  Exception(FPU_HANDLER_ADDR, ECODE_FOV);
 }
}

bool V810::IsSubnormal(uint32 fpval)
{
 if( ((fpval >> 23) & 0xFF) == 0 && (fpval & ((1 << 23) - 1)) )
  return(true);

 return(false);
}

INLINE void V810::FPU_Math_Template(float32 (*func)(float32, float32), uint32 arg1, uint32 arg2)
{
 if(CheckFPInputException(P_REG[arg1]) || CheckFPInputException(P_REG[arg2]))
 {

 }
 else
 {
  uint32 result;

  float_exception_flags = 0;
  result = func(P_REG[arg1], P_REG[arg2]);

  if(IsSubnormal(result))
  {
   float_exception_flags |= float_flag_underflow;
   float_exception_flags |= float_flag_inexact;
  }

  //printf("Result: %08x, %02x; %02x\n", result, (result >> 23) & 0xFF, float_exception_flags);

  if(!FPU_DoesExceptionKillResult())
  {
   // Force it to +/- zero before setting S/Z based off of it(confirmed with subf.s on real V810, at least).
   if(float_exception_flags & float_flag_underflow)
    result &= 0x80000000;

   SetFPUOPNonFPUFlags(result);
   SetPREG(arg1, result);
  }
  FPU_DoException();
 }
}

void V810::fpu_subop(v810_timestamp_t &timestamp, int sub_op, int arg1, int arg2)
{
 //printf("FPU: %02x\n", sub_op);
 if(VBMode)
 {
  switch(sub_op)
  {
   case XB: timestamp++;	// Unknown
	    P_REG[arg1] = (P_REG[arg1] & 0xFFFF0000) | ((P_REG[arg1] & 0xFF) << 8) | ((P_REG[arg1] & 0xFF00) >> 8);
	    return;

   case XH: timestamp++;	// Unknown
	    P_REG[arg1] = (P_REG[arg1] << 16) | (P_REG[arg1] >> 16);
	    return;

   // Does REV use arg1 or arg2 for the source register?
   case REV: timestamp++;	// Unknown
		printf("Revvie bits\n");
	     {
	      // Public-domain code snippet from: http://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel
      	      uint32 v = P_REG[arg2]; // 32-bit word to reverse bit order

	      // swap odd and even bits
	      v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1);
	      // swap consecutive pairs
	      v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2);
	      // swap nibbles ... 
	      v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4);
	      // swap bytes
	      v = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8);
	      // swap 2-byte long pairs
	      v = ( v >> 16             ) | ( v               << 16);

	      P_REG[arg1] = v;
	     }
	     return;

   case MPYHW: timestamp += 9 - 1;	// Unknown?
	       P_REG[arg1] = (int32)(int16)(P_REG[arg1] & 0xFFFF) * (int32)(int16)(P_REG[arg2] & 0xFFFF);
	       return;
  }
 }

 switch(sub_op) 
 {
        // Virtual-Boy specific(probably!)
	default:
		{
		 SetPC(GetPC() - 4);
                 Exception(INVALID_OP_HANDLER_ADDR, ECODE_INVALID_OP);
		}
		break;

	case CVT_WS: 
		timestamp += 5;
		{
		 uint32 result;

                 float_exception_flags = 0;
		 result = int32_to_float32((int32)P_REG[arg2]);

		 if(!FPU_DoesExceptionKillResult())
		 {
		  SetPREG(arg1, result);
		  SetFPUOPNonFPUFlags(result);
		 }
		 else
		 {
		  puts("Exception on CVT.WS?????");	// This shouldn't happen, but just in case there's a bug...
		 }
		 FPU_DoException();
		}
		break;	// End CVT.WS

	case CVT_SW:
		timestamp += 8;
                if(CheckFPInputException(P_REG[arg2]))
                {

                }
		else
		{
		 int32 result;

                 float_exception_flags = 0;
		 result = float32_to_int32(P_REG[arg2]);

		 if(!FPU_DoesExceptionKillResult())
		 {
		  SetPREG(arg1, result);
                  SetFlag(PSW_OV, 0);
                  SetSZ(result);
		 }
		 FPU_DoException();
		}
		break;	// End CVT.SW

	case ADDF_S: timestamp += 8;
		     FPU_Math_Template(float32_add, arg1, arg2);
		     break;

	case SUBF_S: timestamp += 11;
		     FPU_Math_Template(float32_sub, arg1, arg2);
		     break;

        case CMPF_S: timestamp += 6;
		     // Don't handle this like subf.s because the flags
		     // have slightly different semantics(mostly regarding underflow/subnormal results) (confirmed on real V810).
                     if(CheckFPInputException(P_REG[arg1]) || CheckFPInputException(P_REG[arg2]))
                     {

                     }
		     else
		     {
		      SetFlag(PSW_OV, 0);

		      if(float32_eq(P_REG[arg1], P_REG[arg2]))
		      {
		       SetFlag(PSW_Z, 1);
		       SetFlag(PSW_S, 0);
		       SetFlag(PSW_CY, 0);
		      }
		      else
		      {
		       SetFlag(PSW_Z, 0);

		       if(float32_lt(P_REG[arg1], P_REG[arg2]))
		       {
		        SetFlag(PSW_S, 1);
		        SetFlag(PSW_CY, 1);
		       }
		       else
		       {
		        SetFlag(PSW_S, 0);
		        SetFlag(PSW_CY, 0);
                       }
		      }
		     }	// else of if(CheckFP...
                     break;

	case MULF_S: timestamp += 7;
		     FPU_Math_Template(float32_mul, arg1, arg2);
		     break;

	case DIVF_S: timestamp += 43;
		     FPU_Math_Template(float32_div, arg1, arg2);
		     break;

	case TRNC_SW:
                timestamp += 7;

		if(CheckFPInputException(P_REG[arg2]))
		{

		}
		else
                {
                 int32 result;

		 float_exception_flags = 0;
                 result = float32_to_int32_round_to_zero(P_REG[arg2]);

                 if(!FPU_DoesExceptionKillResult())
                 {
                  SetPREG(arg1, result);
		  SetFlag(PSW_OV, 0);
		  SetSZ(result);
                 }
		 FPU_DoException();
                }
                break;	// end TRNC.SW
	}
}

bool V810::WillInterruptOccur(void)
{
 if(ilevel < 0)
  return(false);

 if(Halted == HALT_FATAL_EXCEPTION)
  return(false);

 if(S_REG[PSW] & (PSW_NP | PSW_EP | PSW_ID))
  return(false);

 // If the interrupt level is lower than the interrupt enable level, don't
 // accept it.
 if((unsigned int)ilevel < ((S_REG[PSW] & PSW_IA) >> 16))
  return(false);

 return(true);
}

// Process interrupt level iNum
int V810::Int(uint32 iNum) 
{
 // If CPU is halted because of a fatal exception, don't let an interrupt
 // take us out of this halted status.
 if(Halted == HALT_FATAL_EXCEPTION) 
  return(0);

 // If the NMI pending, exception pending, and/or interrupt disabled bit
 // is set, don't accept any interrupts.
 if(S_REG[PSW] & (PSW_NP | PSW_EP | PSW_ID))
  return(0);

 // If the interrupt level is lower than the interrupt enable level, don't
 // accept it.
 if(iNum < ((S_REG[PSW] & PSW_IA) >> 16))
  return(0);

 S_REG[EIPC]  = GetPC();
 S_REG[EIPSW] = S_REG[PSW];

 SetPC(0xFFFFFE00 | (iNum << 4));
    
 S_REG[ECR] = 0xFE00 | (iNum << 4);

 S_REG[PSW] |= PSW_EP;
 S_REG[PSW] |= PSW_ID;
 S_REG[PSW] &= ~PSW_AE;

 // Now, set need to set the interrupt enable level to he level that is being processed + 1,
 // saturating at 15.
 iNum++;

 if(iNum > 0x0F) 
  iNum = 0x0F;

 S_REG[PSW] &= ~PSW_IA;
 S_REG[PSW] |= iNum << 16;

 // Accepting an interrupt takes us out of normal HALT status, of course!
 Halted = HALT_NONE;

 // Invalidate our bitstring state(forces the instruction to be re-read, and the r/w buffers reloaded).
 if(in_bstr)
 {
  //puts("bstr moo!");

  if(have_src_cache)
  {
   P_REG[30] -= 4;
  }
 }

 in_bstr = FALSE;
 have_src_cache = FALSE;
 have_dst_cache = FALSE;

 // Interrupt overhead is unknown...
 return(0);
}


// Generate exception
void V810::Exception(uint32 handler, uint16 eCode) 
{
 // Exception overhead is unknown.

    printf("Exception: %08x %04x\n", handler, eCode);

    // Invalidate our bitstring state(forces the instruction to be re-read, and the r/w buffers reloaded).
    in_bstr = FALSE;
    have_src_cache = FALSE;
    have_dst_cache = FALSE;

    if(S_REG[PSW] & PSW_NP) // Fatal exception
    {
     printf("Fatal exception; Code: %08x, ECR: %08x, PSW: %08x, PC: %08x\n", eCode, S_REG[ECR], S_REG[PSW], PC);
     Halted = HALT_FATAL_EXCEPTION;
     return;
    }
    else if(S_REG[PSW] & PSW_EP)  //Double Exception
    {
     S_REG[FEPC] = GetPC();
     S_REG[FEPSW] = S_REG[PSW];

     S_REG[ECR] = (S_REG[ECR] & 0xFFFF) | (eCode << 16);
     S_REG[PSW] |= PSW_NP;
     S_REG[PSW] |= PSW_ID;
     S_REG[PSW] &= ~PSW_AE;

     SetPC(0xFFFFFFD0);
     return;
    }
    else 	// Regular exception
    {
     S_REG[EIPC] = GetPC();
     S_REG[EIPSW] = S_REG[PSW];
     S_REG[ECR] = (S_REG[ECR] & 0xFFFF0000) | eCode;
     S_REG[PSW] |= PSW_EP;
     S_REG[PSW] |= PSW_ID;
     S_REG[PSW] &= ~PSW_AE;

     SetPC(handler);
     return;
    }
}

int V810::StateAction(StateMem *sm, int load, int data_only)
{
 uint32 *cache_tag_temp = NULL;
 uint32 *cache_data_temp = NULL;
 bool *cache_data_valid_temp = NULL;
 uint32 PC_tmp = GetPC();

 if(EmuMode == V810_EMU_MODE_ACCURATE)
 {
  cache_tag_temp = (uint32 *)malloc(sizeof(uint32 *) * 128);
  cache_data_temp = (uint32 *)malloc(sizeof(uint32 *) * 128 * 2);
  cache_data_valid_temp = (bool *)malloc(sizeof(bool *) * 128 * 2);

  if(!cache_tag_temp || !cache_data_temp || !cache_data_valid_temp)
  {
   if(cache_tag_temp)
    free(cache_tag_temp);

   // I don't think this IF was supposed to be terminated with a ;
   if(cache_data_temp)
    free(cache_data_temp);

   if(cache_data_valid_temp)
    free(cache_data_valid_temp);

   return(0);
  }
  if(!load)
  {
   for(int i = 0; i < 128; i++)
   {
    cache_tag_temp[i] = Cache[i].tag;

    cache_data_temp[i * 2 + 0] = Cache[i].data[0];
    cache_data_temp[i * 2 + 1] = Cache[i].data[1];

    cache_data_valid_temp[i * 2 + 0] = Cache[i].data_valid[0];
    cache_data_valid_temp[i * 2 + 1] = Cache[i].data_valid[1];
   }
  }
  else // If we're loading, go ahead and clear the cache temporaries,
       // in case the save state was saved while in fast mode
       // and the cache data isn't present and thus won't be loaded.
  {
   memset(cache_tag_temp, 0, sizeof(uint32) * 128);
   memset(cache_data_temp, 0, sizeof(uint32) * 128 * 2);
   memset(cache_data_valid_temp, 0, sizeof(bool) * 128 * 2);
  }
 }

 SFORMAT StateRegs[] =
 {
  SFARRAY32(P_REG, 32),
  SFARRAY32(S_REG, 32),
  SFVARN(PC_tmp, "PC"),
  SFVAR(Halted),

  SFVAR(lastop),

  SFARRAY32(cache_tag_temp, 128),
  SFARRAY32(cache_data_temp, 128 * 2),
  SFARRAYB(cache_data_valid_temp, 128 * 2),

  SFVAR(ilevel),	// Perhaps remove in future?
  SFVAR(next_event_ts),	// This too

  // Bitstring stuff:
  SFVAR(src_cache),
  SFVAR(dst_cache),
  SFVAR(have_src_cache),
  SFVAR(have_dst_cache),
  SFVAR(in_bstr),
  SFVAR(in_bstr_to),

  SFEND
 };

 int ret = MDFNSS_StateAction(sm, load, data_only, StateRegs, "V810");

 if(load)
 {
  SetPC(PC_tmp);
  if(EmuMode == V810_EMU_MODE_ACCURATE)
  {
   for(int i = 0; i < 128; i++)
   {
    Cache[i].tag = cache_tag_temp[i];

    Cache[i].data[0] = cache_data_temp[i * 2 + 0];
    Cache[i].data[1] = cache_data_temp[i * 2 + 1];

    Cache[i].data_valid[0] = cache_data_valid_temp[i * 2 + 0];
    Cache[i].data_valid[1] = cache_data_valid_temp[i * 2 + 1];

    printf("%d %08x %08x %08x %d %d\n", i, Cache[i].tag << 10, Cache[i].data[0], Cache[i].data[1], Cache[i].data_valid[0], Cache[i].data_valid[1]);
   }
  }
 }

 if(EmuMode == V810_EMU_MODE_ACCURATE)
 {
  free(cache_tag_temp);
  free(cache_data_temp);
  free(cache_data_valid_temp);
 }

 return(ret);
}
