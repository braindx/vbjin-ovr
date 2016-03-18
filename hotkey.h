#ifndef HOTKEY_H_INCLUDED
#define HOTKEY_H_INCLUDED

#include <winsock2.h>
#include <windows.h>
#include <tchar.h>
#include <string>
#include "main.h"

#define WM_CUSTKEYDOWN	(WM_USER+50)
#define WM_CUSTKEYUP	(WM_USER+51)

enum HotkeyPage {
	HOTKEY_PAGE_MAIN=0,
	HOTKEY_PAGE_MOVIE,
	HOTKEY_PAGE_STATE,
	HOTKEY_PAGE_STATE_SLOTS,
	HOTKEY_PAGE_TURBO,
	NUM_HOTKEY_PAGE,
};

static LPCTSTR hotkeyPageTitle[] = {
	_T("Main"),
	_T("Movie"),
	_T("Savestates"),
	_T("Savestate Slots"),
	_T("Turbo"),
	_T("NUM_HOTKEY_PAGE"),
};


struct SCustomKey
{
	typedef void (*THandler) (int param);
	WORD key;
	WORD modifiers;
	THandler handleKeyDown;
	THandler handleKeyUp;
	HotkeyPage page;
	std::wstring name;
	const char* code;
	int param;
	//HotkeyTiming timing;
};

struct SCustomKeys
{

	

//	SCustomKey OpenROM, FrameAdvance, FastForwardToggle, IncreaseSpeed, DecreaseSpeed;
/*
	SCustomKey TurboRight, TurboLeft, TurboDown, TurboUp, TurboSelect, TurboStart, TurboB, TurboA, TurboY, TurboX, TurboR, TurboL;

	SCustomKey AutoHold, AutoHoldClear;
*/
SCustomKey FastForwardToggle, IncreaseSpeed, DecreaseSpeed;
	SCustomKey RecordAVI, StopAVI;
	SCustomKey PlayMovieFromBeginning;
/*
	SCustomKey ToggleFrameCounter;
	SCustomKey ToggleFPS;
	SCustomKey ToggleInput;
	SCustomKey ToggleLag;
	*/

	SCustomKey Screenshot;

	SCustomKey FastForward;
	SCustomKey ToggleNBG0, ToggleNBG1, ToggleNBG2, ToggleNBG3;

	SCustomKey ToggleFullScreen;
	SCustomKey PlayMovie;
	SCustomKey RecordMovie;
	SCustomKey StopMovie;
	SCustomKey ToggleReadOnly;

	SCustomKey ToggleOSD;
	SCustomKey HardReset;
	SCustomKey Pause;
	SCustomKey FrameAdvance;

	SCustomKey IPDFactorDecrease;
	SCustomKey IPDFactorIncrease;

	SCustomKey QuickSave, QuickLoad, NextSaveSlot, PreviousSaveSlot;
	SCustomKey Save[10];
	SCustomKey Load[10];
	SCustomKey Slot[10];
	SCustomKey LastItem; // dummy, must be last

	//--methods--
	SCustomKey &key(int i) { return ((SCustomKey*)this)[i]; }
	SCustomKey const &key(int i) const { return ((SCustomKey*)this)[i]; }
};
//SCustomKey key[];

extern SCustomKeys CustomKeys;

bool IsLastCustomKey (const SCustomKey *key);
void CopyCustomKeys (SCustomKeys *dst, const SCustomKeys *src);
void InitCustomKeys (SCustomKeys *keys);
int GetModifiers(int key);
int HandleKeyMessage(WPARAM wParam, LPARAM lParam, int modifiers);
int HandleKeyUp(WPARAM wParam, LPARAM lParam, int modifiers);
INT_PTR CALLBACK DlgHotkeyConfig(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
void InitCustomControls();
void LoadHotkeyConfig();

extern DWORD hKeyInputTimer;
extern VOID CALLBACK KeyInputTimer( UINT idEvent, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

//HOTKEY HANDLERS
void HK_PrintScreen(int);
void HK_StateSaveSlot(int);
void HK_StateLoadSlot(int);
void HK_StateSetSlot(int);
void HK_Pause(int);
void HK_FastForward(int);
void HK_StateQuickSaveSlot(int); 
void HK_StateQuickLoadSlot(int); 

extern bool AutoHoldPressed;

#endif //HOTKEY_H_INCLUDED

/**********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com),
                             Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2002 - 2004  Matthew Kendora

  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)

  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)

  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2006  funkyass (funkyass@spam.shaw.ca),
                             Kris Bleakley (codeviolation@hotmail.com)

  (c) Copyright 2002 - 2007  Brad Jorsch (anomie@users.sourceforge.net),
                             Nach (n-a-c-h@users.sourceforge.net),
                             zones (kasumitokoduck@yahoo.com)

  (c) Copyright 2006 - 2007  nitsuja


  BS-X C emulator code
  (c) Copyright 2005 - 2006  Dreamer Nom,
                             zones

  C4 x86 assembler and some C emulation code
  (c) Copyright 2000 - 2003  _Demo_ (_demo_@zsnes.com),
                             Nach,
                             zsKnight (zsknight@zsnes.com)

  C4 C++ code
  (c) Copyright 2003 - 2006  Brad Jorsch,
                             Nach

  DSP-1 emulator code
  (c) Copyright 1998 - 2006  _Demo_,
                             Andreas Naive (andreasnaive@gmail.com)
                             Gary Henderson,
                             Ivar (ivar@snes9x.com),
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora,
                             Nach,
                             neviksti (neviksti@hotmail.com)

  DSP-2 emulator code
  (c) Copyright 2003         John Weidman,
                             Kris Bleakley,
                             Lord Nightmare (lord_nightmare@users.sourceforge.net),
                             Matthew Kendora,
                             neviksti


  DSP-3 emulator code
  (c) Copyright 2003 - 2006  John Weidman,
                             Kris Bleakley,
                             Lancer,
                             z80 gaiden

  DSP-4 emulator code
  (c) Copyright 2004 - 2006  Dreamer Nom,
                             John Weidman,
                             Kris Bleakley,
                             Nach,
                             z80 gaiden

  OBC1 emulator code
  (c) Copyright 2001 - 2004  zsKnight,
                             pagefault (pagefault@zsnes.com),
                             Kris Bleakley,
                             Ported from x86 assembler to C by sanmaiwashi

  SPC7110 and RTC C++ emulator code
  (c) Copyright 2002         Matthew Kendora with research by
                             zsKnight,
                             John Weidman,
                             Dark Force

  S-DD1 C emulator code
  (c) Copyright 2003         Brad Jorsch with research by
                             Andreas Naive,
                             John Weidman

  S-RTC C emulator code
  (c) Copyright 2001-2006    byuu,
                             John Weidman

  ST010 C++ emulator code
  (c) Copyright 2003         Feather,
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora

  Super FX x86 assembler emulator code
  (c) Copyright 1998 - 2003  _Demo_,
                             pagefault,
                             zsKnight,

  Super FX C emulator code
  (c) Copyright 1997 - 1999  Ivar,
                             Gary Henderson,
                             John Weidman

  Sound DSP emulator code is derived from SNEeSe and OpenSPC:
  (c) Copyright 1998 - 2003  Brad Martin
  (c) Copyright 1998 - 2006  Charles Bilyue'

  SH assembler code partly based on x86 assembler code
  (c) Copyright 2002 - 2004  Marcus Comstedt (marcus@mc.pp.se)

  2xSaI filter
  (c) Copyright 1999 - 2001  Derek Liauw Kie Fa

  HQ2x, HQ3x, HQ4x filters
  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)

  Win32 GUI code
  (c) Copyright 2003 - 2006  blip,
                             funkyass,
                             Matthew Kendora,
                             Nach,
                             nitsuja

  Mac OS GUI code
  (c) Copyright 1998 - 2001  John Stiles
  (c) Copyright 2001 - 2007  zones


  Specific ports contains the works of other authors. See headers in
  individual files.


  Snes9x homepage: http://www.snes9x.com

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
**********************************************************************************/

