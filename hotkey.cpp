#ifdef __MINGW32__
#define _WIN32_IE 0x0501
#define _WIN32_WINNT 0x0501
#endif

#define STRICT
#include <winsock2.h>
#include <windows.h>
#include <tchar.h>
#include <io.h>
#include "types.h"
#include "main.h"
#include "pcejin.h"
#include "mednafen.h"
#include "dd.h"
#include "d3d.h"

#include <string>

#if (((defined(_MSC_VER) && _MSC_VER >= 1300)) || defined(__MINGW32__))
	// both MINGW and VS.NET use fstream instead of fstream.h which is deprecated
	#include <fstream>
	using namespace std;
#else
	// for VC++ 6
	#include <fstream.h>
#endif

#include "hotkey.h"

#include "main.h"
#include "resource.h"

#define DIRECTINPUT_VERSION 0x0800
#include "directx/dinput.h"

static void ReadHotkey(const char* name, WORD& output);

typedef struct
{
    COLORREF crForeGnd;    // Foreground text colour
    COLORREF crBackGnd;    // Background text colour
    HFONT    hFont;        // The font
    HWND     hwnd;         // The control's window handle
} InputCust;
COLORREF CheckButtonKey( WORD Key);
COLORREF CheckHotKey( WORD Key, int modifiers);
InputCust * GetInputCustom(HWND hwnd);

#define CUSTKEY_ALT_MASK   0x01
#define CUSTKEY_CTRL_MASK  0x02
#define CUSTKEY_SHIFT_MASK 0x04

struct SJoypad {
    BOOL Enabled;
    WORD Left_Up;
    WORD Left_Down;
    WORD Left_Right;
    WORD Left_Left;
    WORD Right_Up;
    WORD Right_Down;
    WORD Right_Left;
    WORD Right_Right;
    WORD Start;
    WORD Select;
	WORD B;
    WORD A;
    WORD ___I;
	WORD ___II;
    WORD ___X;
    WORD ___Y;
    WORD L;
    WORD R;
};

//Right_Down, Right_Left, Select, Start, Left_Up, Left_Down, Left_Left, Left_Rigth, Right_Right, Right_Up, L, R, B, A

//these get shifted over by two in the vb core
#define A_MASK 0x0001
#define B_MASK 0x0002
#define R_MASK 0x0004
#define L_MASK 0x0008

//these should be correct but let's try...
#if 0
#define RIGHT_UP_MASK 0x0010
#define RIGHT_RIGHT_MASK 0x0020
#define LEFT_RIGHT_MASK 0x0040
#define LEFT_LEFT_MASK 0x0080

#define LEFT_DOWN_MASK 0x0100
#define LEFT_UP_MASK 0x0200
#define START_MASK 0x0400
#define SELECT_MASK 0x0800

#define RIGHT_LEFT_MASK 0x1000
#define RIGHT_DOWN_MASK 0x2000
#endif

//these. why did i have to swap right down and right right? something is probably messed in the hotkeys somewhere
#define RIGHT_UP_MASK 0x0010
#define RIGHT_RIGHT_MASK 0x2000
#define LEFT_RIGHT_MASK 0x0040
#define LEFT_LEFT_MASK 0x0080

#define LEFT_DOWN_MASK 0x0100
#define LEFT_UP_MASK 0x0200
#define START_MASK 0x0400
#define SELECT_MASK 0x0800

#define RIGHT_LEFT_MASK 0x1000
#define RIGHT_DOWN_MASK 0x0020

/*
#define SELECT_MASK 0x0004
#define START_MASK 0x0008
#define UP_MASK 0x0010
#define RIGHT_MASK 0x0020
#define DOWN_MASK 0x0040
#define LEFT_MASK 0x0080

#define LID_MASK 0x0040
#define DEBUG_MASK 0x0080
#define X_MASK 0x0400
#define Y_MASK 0x0800
*/

struct SJoyState{
    bool Attached;
    //JOYCAPS Caps;
    int Threshold;
    bool Left;
    bool Right;
    bool Up;
    bool Down;
	bool RightLeft;
	bool RightRight;
	bool RightUp;
	bool RightDown;
    bool PovLeft;
    bool PovRight;
    bool PovUp;
    bool PovDown;
    bool PovDnLeft;
    bool PovDnRight;
    bool PovUpLeft;
    bool PovUpRight;
    bool RUp;
    bool RDown;
    bool UUp;
    bool UDown;
    bool VUp;
    bool VDown;
    bool ZUp;
    bool ZDown;
    bool Button[32];
};

extern SJoypad Joypad[16];
extern SJoypad ToggleJoypadStorage[8];
//extern SCustomKeys CustomKeys;
extern SJoypad TurboToggleJoypadStorage[8];

void RunInputConfig();
void RunHotkeyConfig();
void input_process();
int GetNumHotKeysAssignedTo (WORD Key, int modifiers);

struct SGuitar {
    BOOL Enabled;
    WORD GREEN;
    WORD RED;
    WORD YELLOW;
    WORD BLUE;
};

extern SGuitar Guitar;

void TranslateKey(WORD keyz,char *out);
LRESULT InputCustom_OnPaint(InputCust *ccp, WPARAM wParam, LPARAM lParam);

// Gamepad Dialog Strings
#define INPUTCONFIG_TITLE "Input Configuration"
#define INPUTCONFIG_JPTOGGLE "Enabled"
//#define INPUTCONFIG_DIAGTOGGLE "Toggle Diagonals"
//#define INPUTCONFIG_OK "&OK"
//#define INPUTCONFIG_CANCEL "&Cancel"
#define INPUTCONFIG_JPCOMBO "Joypad #%d"
#define INPUTCONFIG_LABEL_UP "Up"
#define INPUTCONFIG_LABEL_DOWN "Down"
#define INPUTCONFIG_LABEL_LEFT "Left"
#define INPUTCONFIG_LABEL_RIGHT "Right"
#define INPUTCONFIG_LABEL_I "I"
#define INPUTCONFIG_LABEL_II "II"
#define INPUTCONFIG_LABEL_X "X"
#define INPUTCONFIG_LABEL_Y "Y"
#define INPUTCONFIG_LABEL_L "L"
#define INPUTCONFIG_LABEL_R "R"
#define INPUTCONFIG_LABEL_RUN "Run"
#define INPUTCONFIG_LABEL_SELECT "Select"
#define INPUTCONFIG_LABEL_UPLEFT "R-Up"
#define INPUTCONFIG_LABEL_UPRIGHT "R-Left"
#define INPUTCONFIG_LABEL_DOWNRIGHT "R-Down"
#define INPUTCONFIG_LABEL_DOWNLEFT "R-Right"
#define INPUTCONFIG_LABEL_BLUE "Blue means the button is already mapped.\nPink means it conflicts with a custom hotkey.\nRed means it's reserved by Windows.\nButtons can be disabled using Escape.\nGrayed buttons arent supported yet (sorry!)"
#define INPUTCONFIG_LABEL_UNUSED ""
#define INPUTCONFIG_LABEL_CLEAR_TOGGLES_AND_TURBO "Clear All"
#define INPUTCONFIG_LABEL_MAKE_TURBO "TempTurbo"
#define INPUTCONFIG_LABEL_MAKE_HELD "Autohold"
#define INPUTCONFIG_LABEL_MAKE_TURBO_HELD "Autofire"
#define INPUTCONFIG_LABEL_CONTROLLER_TURBO_PANEL_MOD " Turbo"

// Hotkeys Dialog Strings
#define HOTKEYS_TITLE "Hotkey Configuration"
#define HOTKEYS_CONTROL_MOD "Ctrl + "
#define HOTKEYS_SHIFT_MOD "Shift + "
#define HOTKEYS_ALT_MOD "Alt + "
#define HOTKEYS_LABEL_BLUE "Blue means the hotkey is already mapped.\nPink means it conflicts with a game button.\nRed means it's reserved by Windows.\nA hotkey can be disabled using Escape."
#define HOTKEYS_HKCOMBO "Page %d"

// gaming buttons and axes
#define GAMEDEVICE_JOYNUMPREFIX "(J%x)" // don't change this
#define GAMEDEVICE_JOYBUTPREFIX "#[%d]" // don't change this
#define GAMEDEVICE_XNEG "Left"
#define GAMEDEVICE_XPOS "Right"
#define GAMEDEVICE_YPOS "Up"
#define GAMEDEVICE_YNEG "Down"
#define GAMEDEVICE_R_XNEG "RS-Left"
#define GAMEDEVICE_R_XPOS "RS-Right"
#define GAMEDEVICE_R_YPOS "RS-Up"
#define GAMEDEVICE_R_YNEG "RS-Down"
#define GAMEDEVICE_POVLEFT "POV Left"
#define GAMEDEVICE_POVRIGHT "POV Right"
#define GAMEDEVICE_POVUP "POV Up"
#define GAMEDEVICE_POVDOWN "POV Down"
#define GAMEDEVICE_POVDNLEFT "POV Dn Left"
#define GAMEDEVICE_POVDNRIGHT "POV Dn Right"
#define GAMEDEVICE_POVUPLEFT  "POV Up Left"
#define GAMEDEVICE_POVUPRIGHT "POV Up Right"
#define GAMEDEVICE_ZPOS "Z Up"
#define GAMEDEVICE_ZNEG "Z Down"
#define GAMEDEVICE_RPOS "R Up"
#define GAMEDEVICE_RNEG "R Down"
#define GAMEDEVICE_UPOS "U Up"
#define GAMEDEVICE_UNEG "U Down"
#define GAMEDEVICE_VPOS "V Up"
#define GAMEDEVICE_VNEG "V Down"
#define GAMEDEVICE_BUTTON "Button %d"

// gaming general
#define GAMEDEVICE_DISABLED "Disabled"

// gaming keys
#define GAMEDEVICE_KEY "#%d"
#define GAMEDEVICE_NUMPADPREFIX "Numpad-%c"
#define GAMEDEVICE_VK_TAB "Tab"
#define GAMEDEVICE_VK_BACK "Backspace"
#define GAMEDEVICE_VK_CLEAR "Delete"
#define GAMEDEVICE_VK_RETURN "Enter"
#define GAMEDEVICE_VK_LSHIFT "LShift"
#define GAMEDEVICE_VK_RSHIFT "RShift"
#define GAMEDEVICE_VK_LCONTROL "LCtrl"
#define GAMEDEVICE_VK_RCONTROL "RCtrl"
#define GAMEDEVICE_VK_LMENU "LAlt"
#define GAMEDEVICE_VK_RMENU "RAlt"
#define GAMEDEVICE_VK_PAUSE "Pause"
#define GAMEDEVICE_VK_CAPITAL "Capslock"
#define GAMEDEVICE_VK_ESCAPE "Disabled"
#define GAMEDEVICE_VK_SPACE "Space"
#define GAMEDEVICE_VK_PRIOR "PgUp"
#define GAMEDEVICE_VK_NEXT "PgDn"
#define GAMEDEVICE_VK_HOME "Home"
#define GAMEDEVICE_VK_END "End"
#define GAMEDEVICE_VK_LEFT "Left"
#define GAMEDEVICE_VK_RIGHT "Right"
#define GAMEDEVICE_VK_UP "Up"
#define GAMEDEVICE_VK_DOWN "Down"
#define GAMEDEVICE_VK_SELECT "Select"
#define GAMEDEVICE_VK_PRINT "Print"
#define GAMEDEVICE_VK_EXECUTE "Execute"
#define GAMEDEVICE_VK_SNAPSHOT "SnapShot"
#define GAMEDEVICE_VK_INSERT "Insert"
#define GAMEDEVICE_VK_DELETE "Delete"
#define GAMEDEVICE_VK_HELP "Help"
#define GAMEDEVICE_VK_LWIN "LWinKey"
#define GAMEDEVICE_VK_RWIN "RWinKey"
#define GAMEDEVICE_VK_APPS "AppKey"
#define GAMEDEVICE_VK_MULTIPLY "Numpad *"
#define GAMEDEVICE_VK_ADD "Numpad +"
#define GAMEDEVICE_VK_SEPARATOR "Separator"
#define GAMEDEVICE_VK_OEM_1 "Semi-Colon"
#define GAMEDEVICE_VK_OEM_7 "Apostrophe"
#define GAMEDEVICE_VK_OEM_COMMA "Comma"
#define GAMEDEVICE_VK_OEM_PERIOD "Period"
#define GAMEDEVICE_VK_SUBTRACT "Numpad -"
#define GAMEDEVICE_VK_DECIMAL "Numpad ."
#define GAMEDEVICE_VK_DIVIDE "Numpad /"
#define GAMEDEVICE_VK_NUMLOCK "Num-lock"
#define GAMEDEVICE_VK_SCROLL "Scroll-lock"
#define GAMEDEVICE_VK_OEM_MINUS "-"
#define GAMEDEVICE_VK_OEM_PLUS "="
#define GAMEDEVICE_VK_SHIFT "Shift"
#define GAMEDEVICE_VK_CONTROL "Control"
#define GAMEDEVICE_VK_MENU "Alt"
#define GAMEDEVICE_VK_OEM_4 "["
#define GAMEDEVICE_VK_OEM_6 "]"
#define GAMEDEVICE_VK_OEM_5 "\\"
#define GAMEDEVICE_VK_OEM_2 "/"
#define GAMEDEVICE_VK_OEM_3 "`"
#define GAMEDEVICE_VK_F1 "F1"
#define GAMEDEVICE_VK_F2 "F2"
#define GAMEDEVICE_VK_F3 "F3"
#define GAMEDEVICE_VK_F4 "F4"
#define GAMEDEVICE_VK_F5 "F5"
#define GAMEDEVICE_VK_F6 "F6"
#define GAMEDEVICE_VK_F7 "F7"
#define GAMEDEVICE_VK_F8 "F8"
#define GAMEDEVICE_VK_F9 "F9"
#define GAMEDEVICE_VK_F10 "F10"
#define GAMEDEVICE_VK_F11 "F11"
#define GAMEDEVICE_VK_F12 "F12"
#define BUTTON_OK "&OK"
#define BUTTON_CANCEL "&Cancel"

static TCHAR szClassName[] = _T("InputCustom");
static TCHAR szHotkeysClassName[] = _T("InputCustomHot");
static TCHAR szGuitarClassName[] = _T("InputCustomGuitar");

static LRESULT CALLBACK InputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK HotInputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK GuitarInputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

SJoyState Joystick [16];
SJoyState JoystickF [16];
SJoypad ToggleJoypadStorage[8];
SJoypad TurboToggleJoypadStorage[8];
u32 joypads [8];

//the main input configuration:
SJoypad DefaultJoypad[16] = {
    {
        true,					/* Joypad 1 enabled */
			VK_UP, VK_DOWN, VK_RIGHT, VK_LEFT ,	/* Left, Right, Up, Down */
			0, 0, 0, 0,             /* Left_Up, Left_Down, Right_Up, Right_Down */
			VK_RETURN, VK_RSHIFT,    /* Start, Select */
			0, 0,					/* Lid, Debug */
			'F', 'D',				/* A B */
			'S', 'A',				/* X Y */
			'Q', 'W'				/* L R */
    },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 }
};

SJoypad Joypad[16];

SGuitar DefaultGuitar = { false, 'E', 'R', 'T', 'Y' };

SGuitar Guitar;
u8	guitarState = 0;

//extern volatile BOOL paused;

#define MAXKEYPAD 15

#define WM_CUSTKEYDOWN	(WM_USER+50)
#define WM_CUSTKEYUP	(WM_USER+51)

#define NUM_HOTKEY_CONTROLS 20

#define COUNT(a) (sizeof (a) / sizeof (a[0]))

const int IDC_LABEL_HK_Table[NUM_HOTKEY_CONTROLS] = {
	IDC_LABEL_HK1 , IDC_LABEL_HK2 , IDC_LABEL_HK3 , IDC_LABEL_HK4 , IDC_LABEL_HK5 ,
	IDC_LABEL_HK6 , IDC_LABEL_HK7 , IDC_LABEL_HK8 , IDC_LABEL_HK9 , IDC_LABEL_HK10,
	IDC_LABEL_HK11, IDC_LABEL_HK12, IDC_LABEL_HK13, IDC_LABEL_HK14, IDC_LABEL_HK15,
	IDC_LABEL_HK16, IDC_LABEL_HK17, IDC_LABEL_HK18, IDC_LABEL_HK19, IDC_LABEL_HK20,
};
const int IDC_HOTKEY_Table[NUM_HOTKEY_CONTROLS] = {
	IDC_HOTKEY1 , IDC_HOTKEY2 , IDC_HOTKEY3 , IDC_HOTKEY4 , IDC_HOTKEY5 ,
	IDC_HOTKEY6 , IDC_HOTKEY7 , IDC_HOTKEY8 , IDC_HOTKEY9 , IDC_HOTKEY10,
	IDC_HOTKEY11, IDC_HOTKEY12, IDC_HOTKEY13, IDC_HOTKEY14, IDC_HOTKEY15,
	IDC_HOTKEY16, IDC_HOTKEY17, IDC_HOTKEY18, IDC_HOTKEY19, IDC_HOTKEY20,
};

typedef char TcDIBuf[512];

TcDIBuf					cDIBuf;
LPDIRECTINPUT8			pDI;
LPDIRECTINPUTDEVICE8	pJoystick;
DIDEVCAPS				DIJoycap;
LPDIRECTINPUTEFFECT     pEffect;
char	JoystickName[255];
BOOL	Feedback;


static LPDIRECTINPUT8		tmp_pDI = NULL;
static BOOL					tmp_Feedback = FALSE;
static char					tmp_device_name[255] = { 0 };
static LPDIRECTINPUTDEVICE8 tmp_Device = NULL;
static LPDIRECTINPUTDEVICE8 tmp_Joystick = NULL;

BOOL CALLBACK EnumCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
	if ( FAILED( tmp_pDI->CreateDevice(lpddi->guidInstance, &tmp_Device, NULL) ) )
	{
		tmp_Device = NULL;
		return DIENUM_CONTINUE;
	}

	strcpy(tmp_device_name, lpddi->tszProductName);
	if (lpddi->guidFFDriver.Data1) tmp_Feedback = TRUE;
	return DIENUM_STOP;
}


LPDIRECTINPUTDEVICE8 EnumDevices(LPDIRECTINPUT8 pDI)
{
	tmp_pDI = pDI;
	tmp_Feedback = FALSE;
	memset(tmp_device_name, 0, 255);
	if( FAILED( pDI->EnumDevices(DI8DEVCLASS_GAMECTRL,
									EnumCallback,
									NULL,
									DIEDFL_ATTACHEDONLY) ) )
			return NULL;
	Feedback = tmp_Feedback;
	strcpy(JoystickName, tmp_device_name);
	return tmp_Device;
}

BOOL CALLBACK EnumObjects(const DIDEVICEOBJECTINSTANCE* pdidoi,VOID* pContext)
{
	if( pdidoi->dwType & DIDFT_AXIS )
	{
		DIPROPRANGE diprg; 
        diprg.diph.dwSize       = sizeof(DIPROPRANGE); 
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
        diprg.diph.dwHow        = DIPH_BYID; 
        diprg.diph.dwObj        = pdidoi->dwType;
        diprg.lMin              = -10000; 
        diprg.lMax              = 10000; 
   
        if( FAILED(tmp_Joystick->SetProperty(DIPROP_RANGE, &diprg.diph)) ) 
			return DIENUM_STOP;
	}
	return DIENUM_CONTINUE;
}

static void ReadControl(const char* name, const char* controller, WORD& output)
{
	UINT temp;
	temp = GetPrivateProfileInt(controller,name,-1,IniName);
	if(temp != -1) {
		output = temp;
	}
}

void LoadInputConfig()
{
	memcpy(&Joypad,&DefaultJoypad,sizeof(Joypad));
	
	//read from configuration file
	Joypad[0].Enabled = true;
	Joypad[1].Enabled = true;
	Joypad[2].Enabled = true;
	Joypad[3].Enabled = true;
	Joypad[4].Enabled = true;

	char temp[50];
	for(int i = 0; i < 10; i++) {
		snprintf(temp, 40, "Controller%d", i);

		ReadControl("Left_Left",temp,Joypad[i].Left_Left);
		ReadControl("Left_Right",temp,Joypad[i].Left_Right);
		ReadControl("Left_Up",temp,Joypad[i].Left_Up);
		ReadControl("Left_Down",temp,Joypad[i].Left_Down);

		ReadControl("Right_Left",temp,Joypad[i].Right_Left);
		ReadControl("Right_Right",temp,Joypad[i].Right_Right);
		ReadControl("Right_Up",temp,Joypad[i].Right_Up);
		ReadControl("Right_Down",temp,Joypad[i].Right_Down);

		ReadControl("Start",temp,Joypad[i].Start);
		ReadControl("Select",temp,Joypad[i].Select);
//		ReadControl("Lid",temp,Joypad[i].Lid);
//		ReadControl("Debug",temp,Joypad[i].Debug);
		ReadControl("B",temp,Joypad[i].B);
		ReadControl("A",temp,Joypad[i].A);
//		ReadControl("X",temp,Joypad[i].X);
//		ReadControl("Y",temp,Joypad[i].Y);
		ReadControl("L",temp,Joypad[i].L);
		ReadControl("R",temp,Joypad[i].R);
	}
}

static void WriteControl(char* name, char* controller, WORD val)
{
	WritePrivateProfileInt(controller,name,val,IniName);
}

static void SaveInputConfig()
{
	char temp[50];
	for(int i = 0; i < 10; i++) {
		snprintf(temp, 40, "Controller%d", i);
		WriteControl("Left_Left",temp,Joypad[i].Left_Left);
		WriteControl("Left_Right",temp,Joypad[i].Left_Right);
		WriteControl("Left_Up",temp,Joypad[i].Left_Up);
		WriteControl("Left_Down",temp,Joypad[i].Left_Down);
		WriteControl("Right_Left",temp,Joypad[i].Right_Left);
		WriteControl("Right_Right",temp,Joypad[i].Right_Right);
		WriteControl("Right_Up",temp,Joypad[i].Right_Up);
		WriteControl("Right_Down",temp,Joypad[i].Right_Down);
		WriteControl("Start",temp,Joypad[i].Start);
		WriteControl("Select",temp,Joypad[i].Select);
//		ReadControl("Lid",temp,Joypad[i].Lid);
//		ReadControl("Debug",temp,Joypad[i].Debug);
		WriteControl("B",temp,Joypad[i].B);
		WriteControl("A",temp,Joypad[i].A);
//		ReadControl("X",temp,Joypad[i].X);
//		ReadControl("Y",temp,Joypad[i].Y);
		WriteControl("L",temp,Joypad[i].L);
		WriteControl("R",temp,Joypad[i].R);
/*
		WriteControl("Left",temp,Joypad[i].Left);
		WriteControl("Right",temp,Joypad[i].Right);
		WriteControl("Up",temp,Joypad[i].Up);
		WriteControl("Down",temp,Joypad[i].Down);
		WriteControl("Left_Up",temp,Joypad[i].Left_Up);
		WriteControl("Left_Down",temp,Joypad[i].Left_Down);
		WriteControl("Right_Up",temp,Joypad[i].Right_Up);
		WriteControl("Right_Down",temp,Joypad[i].Right_Down);
		WriteControl("Run",temp,Joypad[i].Run);
		WriteControl("Select",temp,Joypad[i].Select);
		WriteControl("Lid",temp,Joypad[i].Lid);
		WriteControl("Debug",temp,Joypad[i].Debug);
		WriteControl("I",temp,Joypad[i].I);
		WriteControl("II",temp,Joypad[i].II);
		WriteControl("X",temp,Joypad[i].X);
		WriteControl("Y",temp,Joypad[i].Y);
		WriteControl("L",temp,Joypad[i].L);
		WriteControl("R",temp,Joypad[i].R);
		
		*/
	}
}

BOOL di_init()
{
	HWND hParentWnd = g_hWnd;

	pDI = NULL;
	pJoystick = NULL;
	Feedback = FALSE;
	memset(cDIBuf, 0, sizeof(cDIBuf));
	memset(JoystickName, 0, sizeof(JoystickName));

	if(FAILED(DirectInput8Create(GetModuleHandle(NULL),DIRECTINPUT_VERSION,IID_IDirectInput8,(void**)&pDI,NULL)))
		return FALSE;


	pJoystick = EnumDevices(pDI);

	if (pJoystick)
	{
		if(!FAILED(pJoystick->SetDataFormat(&c_dfDIJoystick2)))
		{
			if(FAILED(pJoystick->SetCooperativeLevel(hParentWnd,DISCL_BACKGROUND|DISCL_EXCLUSIVE)))
			{
				pJoystick->Release();
				pJoystick = NULL;
			}
			else
			{
				tmp_Joystick = pJoystick;
				pJoystick->EnumObjects(::EnumObjects, (VOID*)hParentWnd, DIDFT_ALL);
				memset(&DIJoycap,0,sizeof(DIDEVCAPS));
				DIJoycap.dwSize=sizeof(DIDEVCAPS);
				pJoystick->GetCapabilities(&DIJoycap);
			}
		}
		else
		{
			pJoystick->Release();
			pJoystick = NULL;
		}
	}

	if (pJoystick)
	{
		DIPROPDWORD dipdw;
		dipdw.diph.dwSize = sizeof(DIPROPDWORD);
		dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		dipdw.diph.dwObj = 0;
		dipdw.diph.dwHow = DIPH_DEVICE;
		dipdw.dwData = 0;
		if ( !FAILED( pJoystick->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph) ) )
		{
			DWORD		rgdwAxes[1] = { DIJOFS_Y };
			LONG		rglDirection[2] = { 0 };
			DICONSTANTFORCE		cf = { 0 };
			DIEFFECT	eff;

			cf.lMagnitude = (DI_FFNOMINALMAX * 100);
			
			memset(&eff, 0, sizeof(eff));
			eff.dwSize = sizeof(DIEFFECT);
			eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
			eff.dwDuration = INFINITE;
			eff.dwSamplePeriod = 0;
			eff.dwGain = DI_FFNOMINALMAX;
			eff.dwTriggerButton = DIEB_NOTRIGGER;
			eff.dwTriggerRepeatInterval = 0;
			eff.cAxes = 1;
			eff.rgdwAxes = rgdwAxes;
			eff.rglDirection = rglDirection;
			eff.lpEnvelope = 0;
			eff.cbTypeSpecificParams = sizeof( DICONSTANTFORCE );
			eff.lpvTypeSpecificParams = &cf;
			eff.dwStartDelay = 0;

			if( FAILED( pJoystick->CreateEffect(GUID_ConstantForce, &eff, &pEffect, NULL) ) )
				Feedback = FALSE;
		}
		else
			Feedback = FALSE;
	}
	printf("DirectX Input: \n");

//	INFO("DirectX Input: \n");
	if (pJoystick != NULL)
	{
		printf("   - gamecontrol successfully inited: %s\n", JoystickName);
	//	INFO("   - gamecontrol successfully inited: %s\n", JoystickName);
		if (Feedback) printf("\t\t\t\t      (with FeedBack support)\n");
	}

//	paused = FALSE;

	return TRUE;
}

BOOL JoystickEnabled()
{
	return (pJoystick==NULL?FALSE:TRUE);
}


HWND funky;
//WPARAM tid;

//
void JoystickChanged( short ID, short Movement)
{
	// don't allow two changes to happen too close together in time
	{
		static bool first = true;
		static DWORD lastTime = 0;
		if(first || timeGetTime() - lastTime > 300) // 0.3 seconds
		{
			first = false;
			lastTime = timeGetTime();
		}
		else
		{
			return; // too soon after last change
		}
	}

	WORD JoyKey;

    JoyKey = 0x8000;
	JoyKey |= (WORD)(ID << 8);
	JoyKey |= Movement;
	SendMessage(funky,WM_USER+45,JoyKey,0);
//	KillTimer(funky,tid);
}

int FunkyNormalize(int cur, int min, int max)
{
	int Result = 0;

    if ((max - min) == 0)

        return (Result);

    Result = cur - min;
    Result = (Result * 200) / (max - min);
    Result -= 100;

    return Result;
}



#define S9X_JOY_NEUTRAL 60

void CheckAxis (short joy, short control, int val,
                                       int min, int max,
                                       bool &first, bool &second)
{



    if (FunkyNormalize (val, min, max) < -S9X_JOY_NEUTRAL)

    {
        second = false;
        if (!first)
        {
            JoystickChanged (joy, control);
            first = true;

        }
    }
    else
        first = false;

    if (FunkyNormalize (val, min, max) > S9X_JOY_NEUTRAL)
    {
        first = false;
        if (!second)
        {
            JoystickChanged (joy, (short) (control + 1));
            second = true;
        }
    }
    else
        second = false;
}


void CheckAxis_game (int val, int min, int max, bool &first, bool &second)
{
    if (FunkyNormalize (val, min, max) < -S9X_JOY_NEUTRAL)
    {
        second = false;
        first = true;
    }
    else
        first = false;

    if (FunkyNormalize (val, min, max) > S9X_JOY_NEUTRAL)
    {
        first = false;
        second = true;
    }
    else
        second = false;
}

void S9xUpdateJoyState()
{
	memset(&Joystick[0],0,sizeof(Joystick[0]));

	int C = 0;
	if (pJoystick)
	{
		DIJOYSTATE2 JoyStatus;

		HRESULT hr=pJoystick->Poll();
		if (FAILED(hr))	
			pJoystick->Acquire();
		else
		{
			hr=pJoystick->GetDeviceState(sizeof(JoyStatus),&JoyStatus);
			if (FAILED(hr)) hr=pJoystick->Acquire();
			else
			{
				CheckAxis_game(JoyStatus.lX,-10000,10000,Joystick[0].Left,Joystick[0].Right);
				CheckAxis_game(JoyStatus.lY,-10000,10000,Joystick[0].Up,Joystick[0].Down);
		
				 switch (JoyStatus.rgdwPOV[0])
				{
             case JOY_POVBACKWARD:
                Joystick[C].PovDown = true;
                break;
			case 4500:
				//Joystick[C].PovUpRight = true;
				Joystick[C].PovUp = true;
				Joystick[C].PovRight = true;
				break;
			case 13500:
				//Joystick[C].PovDnRight = true;
				Joystick[C].PovDown = true;
				Joystick[C].PovRight = true;
				break;
			case 22500:
				//Joystick[C].PovDnLeft = true;
				Joystick[C].PovDown = true;
				Joystick[C].PovLeft = true;
				break;
			case 31500:
				//Joystick[C].PovUpLeft = true;
				Joystick[C].PovUp = true;
				Joystick[C].PovLeft = true;
				break;

            case JOY_POVFORWARD:
                Joystick[C].PovUp = true;
                break;

            case JOY_POVLEFT:
				Joystick[C].PovLeft = true;
                break;

            case JOY_POVRIGHT:
				Joystick[C].PovRight = true;
                break;

            default:
                break;
				}

   for(int B=0;B<128;B++)
        if( JoyStatus.rgbButtons[B] )
			Joystick[C].Button[B] = true;

			}
		}
	}
}

void di_poll_scan()
{
	int C = 0;
	if (pJoystick)
	{
		DIJOYSTATE2 JoyStatus;

		HRESULT hr=pJoystick->Poll();
		if (FAILED(hr))	
			pJoystick->Acquire();
		else
		{
			hr=pJoystick->GetDeviceState(sizeof(JoyStatus),&JoyStatus);
			if (FAILED(hr)) hr=pJoystick->Acquire();
			else
			{
				CheckAxis(0,0,JoyStatus.lX,-10000,10000,JoystickF[C].Left,JoystickF[C].Right);
				CheckAxis(0,2,JoyStatus.lY,-10000,10000,JoystickF[C].Down,JoystickF[C].Up);
				CheckAxis(0,8,JoyStatus.lRx,-10000,10000,JoystickF[C].RightLeft,JoystickF[C].RightRight);
				CheckAxis(0,10,JoyStatus.lRy,-10000,10000,JoystickF[C].RightDown,JoystickF[C].RightUp);
		
				 switch (JoyStatus.rgdwPOV[0])
				{
             case JOY_POVBACKWARD:
                if( !JoystickF[C].PovDown)
                {   JoystickChanged( C, 7); }

                JoystickF[C].PovDown = true;
                JoystickF[C].PovUp = false;
                JoystickF[C].PovLeft = false;
                JoystickF[C].PovRight = false;
				JoystickF[C].PovDnLeft = false;
				JoystickF[C].PovDnRight = false;
				JoystickF[C].PovUpLeft = false;
				JoystickF[C].PovUpRight = false;
                break;
			case 4500:
				if( !JoystickF[C].PovUpRight)
                {   JoystickChanged( C, 52); }
				JoystickF[C].PovDown = false;
                JoystickF[C].PovUp = false;
                JoystickF[C].PovLeft = false;
                JoystickF[C].PovRight = false;
				JoystickF[C].PovDnLeft = false;
				JoystickF[C].PovDnRight = false;
				JoystickF[C].PovUpLeft = false;
				JoystickF[C].PovUpRight = true;
				break;
			case 13500:
				if( !JoystickF[C].PovDnRight)
                {   JoystickChanged( C, 50); }
				JoystickF[C].PovDown = false;
                JoystickF[C].PovUp = false;
                JoystickF[C].PovLeft = false;
                JoystickF[C].PovRight = false;
				JoystickF[C].PovDnLeft = false;
				JoystickF[C].PovDnRight = true;
				JoystickF[C].PovUpLeft = false;
				JoystickF[C].PovUpRight = false;
				break;
			case 22500:
				if( !JoystickF[C].PovDnLeft)
                {   JoystickChanged( C, 49); }
				JoystickF[C].PovDown = false;
                JoystickF[C].PovUp = false;
                JoystickF[C].PovLeft = false;
                JoystickF[C].PovRight = false;
				JoystickF[C].PovDnLeft = true;
				JoystickF[C].PovDnRight = false;
				JoystickF[C].PovUpLeft = false;
				JoystickF[C].PovUpRight = false;
				break;
			case 31500:
				if( !JoystickF[C].PovUpLeft)
                {   JoystickChanged( C, 51); }
				JoystickF[C].PovDown = false;
                JoystickF[C].PovUp = false;
                JoystickF[C].PovLeft = false;
                JoystickF[C].PovRight = false;
				JoystickF[C].PovDnLeft = false;
				JoystickF[C].PovDnRight = false;
				JoystickF[C].PovUpLeft = true;
				JoystickF[C].PovUpRight = false;
				break;

            case JOY_POVFORWARD:
                if( !JoystickF[C].PovUp)
                {   JoystickChanged( C, 6); }

                JoystickF[C].PovDown = false;
                JoystickF[C].PovUp = true;
                JoystickF[C].PovLeft = false;
                JoystickF[C].PovRight = false;
				JoystickF[C].PovDnLeft = false;
				JoystickF[C].PovDnRight = false;
				JoystickF[C].PovUpLeft = false;
				JoystickF[C].PovUpRight = false;
                break;

            case JOY_POVLEFT:
                if( !JoystickF[C].PovLeft)
                {   JoystickChanged( C, 4); }

				JoystickF[C].PovDown = false;
				JoystickF[C].PovUp = false;
				JoystickF[C].PovLeft = true;
				JoystickF[C].PovRight = false;
				JoystickF[C].PovDnLeft = false;
				JoystickF[C].PovDnRight = false;
				JoystickF[C].PovUpLeft = false;
				JoystickF[C].PovUpRight = false;
                break;

            case JOY_POVRIGHT:
                if( !JoystickF[C].PovRight)
                {   JoystickChanged( C, 5); }

				JoystickF[C].PovDown = false;
				JoystickF[C].PovUp = false;
				JoystickF[C].PovLeft = false;
				JoystickF[C].PovRight = true;
				JoystickF[C].PovDnLeft = false;
				JoystickF[C].PovDnRight = false;
				JoystickF[C].PovUpLeft = false;
				JoystickF[C].PovUpRight = false;
                break;

            default:
                JoystickF[C].PovDown = false;
                JoystickF[C].PovUp = false;
                JoystickF[C].PovLeft = false;
                JoystickF[C].PovRight = false;
				JoystickF[C].PovDnLeft = false;
				JoystickF[C].PovDnRight = false;
				JoystickF[C].PovUpLeft = false;
				JoystickF[C].PovUpRight = false;
                break;
				}

   for(int B=0;B<128;B++)
        if( JoyStatus.rgbButtons[B] )
        {
            if( !JoystickF[C].Button[B])
            {
                JoystickChanged( C, (short)(12+B));
                JoystickF[C].Button[B] = true;
            }
        }
        else
        {   JoystickF[C].Button[B] = false; }


			}
		}
	}

}


void FunkyJoyStickTimer()
{
	di_poll_scan();
}

bool IsReserved (WORD Key, int modifiers);

int GetNumButtonsAssignedTo (WORD Key)
{
	int count = 0;
	for(int J = 0; J < 5*2; J++)
	{
		// don't want to report conflicts with disabled keys
		if(!Joypad[J%5].Enabled || Key == 0 || Key == VK_ESCAPE)
			continue;

		if(Key == Joypad[J].Left_Left)       count++;
		if(Key == Joypad[J].Left_Right)      count++;
		if(Key == Joypad[J].Left_Up)    count++;
		if(Key == Joypad[J].Left_Down)  count++;

		if(Key == Joypad[J].Right_Up)   count++;
		if(Key == Joypad[J].Right_Down) count++;
		if(Key == Joypad[J].Right_Left)         count++;
		if(Key == Joypad[J].Right_Right)       count++;

		if(Key == Joypad[J].Start)      count++;
		if(Key == Joypad[J].Select)     count++;
		if(Key == Joypad[J].A)          count++;
		if(Key == Joypad[J].B)          count++;
//		if(Key == Joypad[J].X)          count++;
//		if(Key == Joypad[J].Y)          count++;
		if(Key == Joypad[J].L)          count++;
		if(Key == Joypad[J].R)          count++;
//		if(Key == Joypad[J].Lid)          count++;
//		if(Key == Joypad[J].Debug)          count++;
    }
	return count;
}

COLORREF CheckButtonKey( WORD Key)
{
	COLORREF red,magenta,blue,white;
	red =RGB(255,0,0);
	magenta =RGB(255,0,255);
	blue = RGB(0,0,255);
	white = RGB(255,255,255);

	// Check for conflict with reserved windows keys
    if(IsReserved(Key,0))
		return red;

    // Check for conflict with Snes9X hotkeys
	if(GetNumHotKeysAssignedTo(Key,0) > 0)
        return magenta;

    // Check for duplicate button keys
    if(GetNumButtonsAssignedTo(Key) > 1)
        return blue;

    return white;
}

COLORREF CheckHotKey( WORD Key, int modifiers)
{
	COLORREF red,magenta,blue,white;
	red =RGB(255,0,0);
	magenta =RGB(255,0,255);
	blue = RGB(0,0,255);
	white = RGB(255,255,255);

	// Check for conflict with reserved windows keys
    if(IsReserved(Key,modifiers))
		return red;

    // Check for conflict with button keys
    if(modifiers == 0 && GetNumButtonsAssignedTo(Key) > 0)
        return magenta;

	// Check for duplicate Snes9X hotkeys
	if(GetNumHotKeysAssignedTo(Key,modifiers) > 1)
        return blue;

    return white;
}

void SetInputCustom(HWND hwnd, InputCust *icp);
static LRESULT CALLBACK InputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// retrieve the custom structure POINTER for THIS window
    InputCust *icp = GetInputCustom(hwnd);
	HWND pappy = (HWND__ *)GetWindowLongPtr(hwnd,GWL_HWNDPARENT);
	funky= hwnd;

	static HWND selectedItem = NULL;

	char temp[100];
	COLORREF col;
    switch(msg)
    {

	case WM_GETDLGCODE:
		return DLGC_WANTARROWS|DLGC_WANTALLKEYS|DLGC_WANTCHARS;
		break;


    case WM_NCCREATE:

        // Allocate a new CustCtrl structure for this window.
        icp = (InputCust *) malloc( sizeof(InputCust) );

        // Failed to allocate, stop window creation.
        if(icp == NULL) return FALSE;

        // Initialize the CustCtrl structure.
        icp->hwnd      = hwnd;
        icp->crForeGnd = GetSysColor(COLOR_WINDOWTEXT);
        icp->crBackGnd = GetSysColor(COLOR_WINDOW);
        icp->hFont     = (HFONT__ *) GetStockObject(DEFAULT_GUI_FONT);

        // Assign the window text specified in the call to CreateWindow.
        SetWindowText(hwnd, ((CREATESTRUCT *)lParam)->lpszName);

        // Attach custom structure to this window.
        SetInputCustom(hwnd, icp);

		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		selectedItem = NULL;

		SetTimer(hwnd,777,125,NULL);

        // Continue with window creation.
        return TRUE;

    // Clean up when the window is destroyed.
    case WM_NCDESTROY:
        free(icp);
        break;
	case WM_PAINT:
		return InputCustom_OnPaint(icp,wParam,lParam);
		break;
	case WM_ERASEBKGND:
		return 1;
	case WM_USER+45:
	case WM_KEYDOWN:
		TranslateKey(wParam,temp);
		col = CheckButtonKey(wParam);

		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		SendMessage(pappy,WM_USER+43,wParam,(LPARAM)hwnd);

		break;
	case WM_USER+44:

		TranslateKey(wParam,temp);
		if(IsWindowEnabled(hwnd))
		{
			col = CheckButtonKey(wParam);
		}
		else
		{
			col = RGB( 192,192,192);
		}
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		break;

	case WM_SETFOCUS:
	{
		selectedItem = hwnd;
		col = RGB( 0,255,0);
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
//		tid = wParam;

		break;
	}
	case WM_KILLFOCUS:
	{
		selectedItem = NULL;
		SendMessage(pappy,WM_USER+46,wParam,(LPARAM)hwnd); // refresh fields on deselect
		break;
	}

	case WM_TIMER:
		if(hwnd == selectedItem)
		{
			FunkyJoyStickTimer();
		}
		SetTimer(hwnd,777,125,NULL);
		break;
	case WM_LBUTTONDOWN:
		SetFocus(hwnd);
		break;
	case WM_ENABLE:
		COLORREF col;
		if(wParam)
		{
			col = RGB( 255,255,255);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		else
		{
			col = RGB( 192,192,192);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		return true;
    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK GuitarInputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
		// retrieve the custom structure POINTER for THIS window
    InputCust *icp = GetInputCustom(hwnd);
	HWND pappy = (HWND__ *)GetWindowLongPtr(hwnd,GWL_HWNDPARENT);
	funky= hwnd;

	static HWND selectedItem = NULL;

	char temp[100];
	COLORREF col;
    switch(msg)
    {

	case WM_GETDLGCODE:
		return DLGC_WANTARROWS|DLGC_WANTALLKEYS|DLGC_WANTCHARS;
		break;


    case WM_NCCREATE:

        // Allocate a new CustCtrl structure for this window.
        icp = (InputCust *) malloc( sizeof(InputCust) );

        // Failed to allocate, stop window creation.
        if(icp == NULL) return FALSE;

        // Initialize the CustCtrl structure.
        icp->hwnd      = hwnd;
        icp->crForeGnd = GetSysColor(COLOR_WINDOWTEXT);
        icp->crBackGnd = GetSysColor(COLOR_WINDOW);
        icp->hFont     = (HFONT__ *) GetStockObject(DEFAULT_GUI_FONT);

        // Assign the window text specified in the call to CreateWindow.
        SetWindowText(hwnd, ((CREATESTRUCT *)lParam)->lpszName);

        // Attach custom structure to this window.
        SetInputCustom(hwnd, icp);

		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		selectedItem = NULL;

		SetTimer(hwnd,777,125,NULL);

        // Continue with window creation.
        return TRUE;

    // Clean up when the window is destroyed.
    case WM_NCDESTROY:
        free(icp);
        break;
	case WM_PAINT:
		return InputCustom_OnPaint(icp,wParam,lParam);
		break;
	case WM_ERASEBKGND:
		return 1;
	case WM_USER+45:
	case WM_KEYDOWN:
		TranslateKey(wParam,temp);
		col = CheckButtonKey(wParam);

		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		SendMessage(pappy,WM_USER+43,wParam,(LPARAM)hwnd);

		break;
	case WM_USER+44:

		TranslateKey(wParam,temp);
		if(IsWindowEnabled(hwnd))
		{
			col = CheckButtonKey(wParam);
		}
		else
		{
			col = RGB( 192,192,192);
		}
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		break;

	case WM_SETFOCUS:
	{
		selectedItem = hwnd;
		col = RGB( 0,255,0);
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
//		tid = wParam;

		break;
	}
	case WM_KILLFOCUS:
	{
		selectedItem = NULL;
		SendMessage(pappy,WM_USER+46,wParam,(LPARAM)hwnd); // refresh fields on deselect
		break;
	}

	case WM_TIMER:
		if(hwnd == selectedItem)
		{
			FunkyJoyStickTimer();
		}
		SetTimer(hwnd,777,125,NULL);
		break;
	case WM_LBUTTONDOWN:
		SetFocus(hwnd);
		break;
	case WM_ENABLE:
		COLORREF col;
		if(wParam)
		{
			col = RGB( 255,255,255);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		else
		{
			col = RGB( 192,192,192);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		return true;
    default:
        break;
    }
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static bool keyPressLock = false;

static void set_buttoninfo(int index, HWND hDlg)
{

	SendDlgItemMessage(hDlg,IDC_UP,WM_USER+44,Joypad[index].Left_Up,0);
	SendDlgItemMessage(hDlg,IDC_LEFT,WM_USER+44,Joypad[index].Left_Left,0);
	SendDlgItemMessage(hDlg,IDC_DOWN,WM_USER+44,Joypad[index].Left_Down,0);
	SendDlgItemMessage(hDlg,IDC_RIGHT,WM_USER+44,Joypad[index].Left_Right,0);
	//NEWTODO
	SendDlgItemMessage(hDlg,IDC_UPLEFT,WM_USER+44,Joypad[index].Right_Up,0);
	SendDlgItemMessage(hDlg,IDC_UPRIGHT,WM_USER+44,Joypad[index].Right_Left,0);
	SendDlgItemMessage(hDlg,IDC_DWNLEFT,WM_USER+44,Joypad[index].Right_Down,0);
	SendDlgItemMessage(hDlg,IDC_DWNRIGHT,WM_USER+44,Joypad[index].Right_Right,0);

	SendDlgItemMessage(hDlg,IDC_I,WM_USER+44,Joypad[index].A,0);
	SendDlgItemMessage(hDlg,IDC_II,WM_USER+44,Joypad[index].B,0);
//	SendDlgItemMessage(hDlg,IDC_X,WM_USER+44,Joypad[index].X,0);
//	SendDlgItemMessage(hDlg,IDC_Y,WM_USER+44,Joypad[index].Y,0);
	SendDlgItemMessage(hDlg,IDC_L,WM_USER+44,Joypad[index].L,0);
	SendDlgItemMessage(hDlg,IDC_R,WM_USER+44,Joypad[index].R,0);
	SendDlgItemMessage(hDlg,IDC_RUN,WM_USER+44,Joypad[index].Start,0);
	SendDlgItemMessage(hDlg,IDC_SELECT,WM_USER+44,Joypad[index].Select,0);
//	SendDlgItemMessage(hDlg,IDC_LID,WM_USER+44,Joypad[index].Lid,0);
//	SendDlgItemMessage(hDlg,IDC_DEBUG,WM_USER+44,Joypad[index].Debug,0);
	if(index < 5)
	{
//		SendDlgItemMessage(hDlg,IDC_UPLEFT,WM_USER+44,Joypad[index].Left_Up,0);
//		SendDlgItemMessage(hDlg,IDC_UPRIGHT,WM_USER+44,Joypad[index].Right_Up,0);
//		SendDlgItemMessage(hDlg,IDC_DWNLEFT,WM_USER+44,Joypad[index].Left_Down,0);
//		SendDlgItemMessage(hDlg,IDC_DWNRIGHT,WM_USER+44,Joypad[index].Right_Down,0);
	}
}

void EnableDisableKeyFields (int index, HWND hDlg)
{
	bool enableUnTurboable;
	if(index < 5)
	{
		SetDlgItemText(hDlg,IDC_LABEL_RIGHT,INPUTCONFIG_LABEL_RIGHT);
		SetDlgItemText(hDlg,IDC_LABEL_UPLEFT,INPUTCONFIG_LABEL_UPLEFT);
		SetDlgItemText(hDlg,IDC_LABEL_UPRIGHT,INPUTCONFIG_LABEL_UPRIGHT);
		SetDlgItemText(hDlg,IDC_LABEL_DOWNRIGHT,INPUTCONFIG_LABEL_DOWNRIGHT);
		SetDlgItemText(hDlg,IDC_LABEL_UP,INPUTCONFIG_LABEL_UP);
		SetDlgItemText(hDlg,IDC_LABEL_LEFT,INPUTCONFIG_LABEL_LEFT);
		SetDlgItemText(hDlg,IDC_LABEL_DOWN,INPUTCONFIG_LABEL_DOWN);
		SetDlgItemText(hDlg,IDC_LABEL_DOWNLEFT,INPUTCONFIG_LABEL_DOWNLEFT);
		enableUnTurboable = true;
	}
	else
	{		
		SetDlgItemText(hDlg,IDC_LABEL_UP,INPUTCONFIG_LABEL_MAKE_TURBO);
		SetDlgItemText(hDlg,IDC_LABEL_LEFT,INPUTCONFIG_LABEL_MAKE_HELD);
		SetDlgItemText(hDlg,IDC_LABEL_DOWN,INPUTCONFIG_LABEL_MAKE_TURBO_HELD);
		SetDlgItemText(hDlg,IDC_LABEL_RIGHT,INPUTCONFIG_LABEL_CLEAR_TOGGLES_AND_TURBO);
//		SetDlgItemText(hDlg,IDC_LABEL_UPLEFT,INPUTCONFIG_LABEL_UNUSED);
//		SetDlgItemText(hDlg,IDC_LABEL_UPRIGHT,INPUTCONFIG_LABEL_UNUSED);
//		SetDlgItemText(hDlg,IDC_LABEL_DOWNLEFT,INPUTCONFIG_LABEL_UNUSED);
//		SetDlgItemText(hDlg,IDC_LABEL_DOWNRIGHT,INPUTCONFIG_LABEL_UNUSED);
//		SetDlgItemText(hDlg,IDC_UPLEFT,INPUTCONFIG_LABEL_UNUSED);
//		SetDlgItemText(hDlg,IDC_UPRIGHT,INPUTCONFIG_LABEL_UNUSED);
//		SetDlgItemText(hDlg,IDC_DWNLEFT,INPUTCONFIG_LABEL_UNUSED);
//		SetDlgItemText(hDlg,IDC_DWNRIGHT,INPUTCONFIG_LABEL_UNUSED);
		enableUnTurboable = false;
	}

	EnableWindow(GetDlgItem(hDlg,IDC_X), false);
	EnableWindow(GetDlgItem(hDlg,IDC_Y), false);
	EnableWindow(GetDlgItem(hDlg,IDC_L), true);
	EnableWindow(GetDlgItem(hDlg,IDC_R), true);
	EnableWindow(GetDlgItem(hDlg,IDC_UPLEFT), true);
	EnableWindow(GetDlgItem(hDlg,IDC_UPRIGHT), true);
	EnableWindow(GetDlgItem(hDlg,IDC_DWNRIGHT), true);
	EnableWindow(GetDlgItem(hDlg,IDC_DWNLEFT), true);
	EnableWindow(GetDlgItem(hDlg,IDC_DEBUG), false);
	EnableWindow(GetDlgItem(hDlg,IDC_LID), false);
}

INT_PTR CALLBACK DlgInputConfig(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HBITMAP hBmp;
	char temp[256];
	//short C;
	int i, which;
	static int index=0;
	
	
	static SJoypad pads[10]; //save Joypad here for undo if cancel
	
	
	//HBRUSH g_hbrBackground;

//	InitInputCustomControl();
switch(msg)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint (hDlg, &ps);
			if(hBmp)
			{
				BITMAP bmp;
				ZeroMemory(&bmp, sizeof(BITMAP));
				RECT r;
				GetClientRect(hDlg, &r);
				HDC hdc=GetDC(hDlg);
				HDC hDCbmp=CreateCompatibleDC(hdc);
				GetObject(hBmp, sizeof(BITMAP), &bmp);
				HBITMAP hOldBmp=(HBITMAP)SelectObject(hDCbmp, hBmp);
				StretchBlt(hdc, 0,0,r.right,r.bottom,hDCbmp,0,0,bmp.bmWidth,bmp.bmHeight,SRCCOPY);
				SelectObject(hDCbmp, hOldBmp);
				DeleteDC(hDCbmp);
				ReleaseDC(hDlg, hdc);
			}
			
			EndPaint (hDlg, &ps);
		}
		return true;
	case WM_INITDIALOG:
//		if(DirectX.Clipped) S9xReRefresh();
		SetWindowText(hDlg,INPUTCONFIG_TITLE);
		SetDlgItemText(hDlg,IDC_JPTOGGLE,INPUTCONFIG_JPTOGGLE);
		SetDlgItemText(hDlg,IDOK,BUTTON_OK);
		SetDlgItemText(hDlg,IDCANCEL,BUTTON_CANCEL);
///		SetDlgItemText(hDlg,IDC_DIAGTOGGLE,INPUTCONFIG_DIAGTOGGLE);
		SetDlgItemText(hDlg,IDC_LABEL_UP,INPUTCONFIG_LABEL_UP);
		SetDlgItemText(hDlg,IDC_LABEL_DOWN,INPUTCONFIG_LABEL_DOWN);
		SetDlgItemText(hDlg,IDC_LABEL_LEFT,INPUTCONFIG_LABEL_LEFT);
		SetDlgItemText(hDlg,IDC_LABEL_I,INPUTCONFIG_LABEL_I);
		SetDlgItemText(hDlg,IDC_LABEL_II,INPUTCONFIG_LABEL_II);
		SetDlgItemText(hDlg,IDC_LABEL_X,INPUTCONFIG_LABEL_X);
		SetDlgItemText(hDlg,IDC_LABEL_Y,INPUTCONFIG_LABEL_Y);
		SetDlgItemText(hDlg,IDC_LABEL_L,INPUTCONFIG_LABEL_L);
		SetDlgItemText(hDlg,IDC_LABEL_R,INPUTCONFIG_LABEL_R);
		SetDlgItemText(hDlg,IDC_LABEL_RUN,INPUTCONFIG_LABEL_RUN);
		SetDlgItemText(hDlg,IDC_LABEL_SELECT,INPUTCONFIG_LABEL_SELECT);
		SetDlgItemText(hDlg,IDC_LABEL_UPRIGHT,INPUTCONFIG_LABEL_UPRIGHT);
		SetDlgItemText(hDlg,IDC_LABEL_UPLEFT,INPUTCONFIG_LABEL_UPLEFT);
		SetDlgItemText(hDlg,IDC_LABEL_DOWNRIGHT,INPUTCONFIG_LABEL_DOWNRIGHT);
		SetDlgItemText(hDlg,IDC_LABEL_DOWNLEFT,INPUTCONFIG_LABEL_DOWNLEFT);
		SetDlgItemText(hDlg,IDC_LABEL_BLUE,INPUTCONFIG_LABEL_BLUE);

		for(i=5;i<10;i++)
			Joypad[i].Left_Up = Joypad[i].Right_Up = Joypad[i].Left_Down = Joypad[i].Right_Down = 0;

		hBmp=(HBITMAP)LoadImage(NULL, TEXT("PBortas.bmp"), IMAGE_BITMAP, 0,0, LR_CREATEDIBSECTION | LR_LOADFROMFILE);
		memcpy(pads, Joypad, 10*sizeof(SJoypad));

		for( i=0;i<256;i++)
			GetAsyncKeyState(i);

//		for( C = 0; C != 16; C ++)
//	        JoystickF[C].Attached = joyGetDevCaps( JOYSTICKID1+C, &JoystickF[C].Caps, sizeof( JOYCAPS)) == JOYERR_NOERROR;

		for(i=1;i<6;i++)
		{
			sprintf(temp,INPUTCONFIG_JPCOMBO,i);
			SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_ADDSTRING,0,(LPARAM)(LPCTSTR)temp);
		}

		for(i=6;i<11;i++)
		{
			sprintf(temp,INPUTCONFIG_JPCOMBO INPUTCONFIG_LABEL_CONTROLLER_TURBO_PANEL_MOD,i-5);
			SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_ADDSTRING,0,(LPARAM)(LPCTSTR)temp);
		}

		SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_SETCURSEL,(WPARAM)0,0);

		SendDlgItemMessage(hDlg,IDC_JPTOGGLE,BM_SETCHECK, Joypad[index].Enabled ? (WPARAM)BST_CHECKED : (WPARAM)BST_UNCHECKED, 0);
//		SendDlgItemMessage(hDlg,IDC_ALLOWLEFTRIGHT,BM_SETCHECK, Settings.UpAndDown ? (WPARAM)BST_CHECKED : (WPARAM)BST_UNCHECKED, 0);

		set_buttoninfo(index,hDlg);

		EnableDisableKeyFields(index,hDlg);

		PostMessage(hDlg,WM_COMMAND, CBN_SELCHANGE<<16, 0);
		
		SetFocus(GetDlgItem(hDlg,IDC_JPCOMBO));
		
		return true;
		break;
	case WM_CLOSE:
		EndDialog(hDlg, 0);
		return TRUE;
	case WM_USER+46:
		// refresh command, for clicking away from a selected field
		index = SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_GETCURSEL,0,0);
		set_buttoninfo(index,hDlg);
		return TRUE;
	case WM_USER+43:
//		MessageBox(hDlg,"USER+43 CAUGHT","moo",MB_OK);
		which = GetDlgCtrlID((HWND)lParam);
		switch(which)
		{
		case IDC_UP:
			Joypad[index].Left_Up = wParam;

			break;
		case IDC_DOWN:
			Joypad[index].Left_Down = wParam;

			break;
		case IDC_LEFT:
			Joypad[index].Left_Left = wParam;

			break;
		case IDC_RIGHT:
			Joypad[index].Left_Right = wParam;

			break;
		case IDC_I:
			Joypad[index].A = wParam;

			break;
		case IDC_II:
			Joypad[index].B = wParam;

			break;
		case IDC_X:
//			Joypad[index].X = wParam;

			break;		
		case IDC_Y:
//			Joypad[index].Y = wParam;

			break;
		case IDC_L:
			Joypad[index].L = wParam;
			break;

		case IDC_R:
			Joypad[index].R = wParam;
	
			break;
		case IDC_SELECT:
			Joypad[index].Select = wParam;

			break;
		case IDC_RUN:
			Joypad[index].Start = wParam;

			break;
			
		case IDC_UPLEFT:
			Joypad[index].Right_Up = wParam;

			break;
		case IDC_UPRIGHT:
			Joypad[index].Right_Left = wParam;

			break;
		case IDC_DWNLEFT:
			Joypad[index].Right_Down = wParam;

			break;
		case IDC_DWNRIGHT:
			Joypad[index].Right_Right = wParam;
			break;

		}

		set_buttoninfo(index,hDlg);

		PostMessage(hDlg,WM_NEXTDLGCTL,0,0);
		return true;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDCANCEL:
			memcpy(Joypad, pads, 10*sizeof(SJoypad));
			EndDialog(hDlg,0);
			if(hBmp)
			{
				DeleteObject(hBmp);
				hBmp=NULL;
			}
			break;

		case IDOK:
//			Settings.UpAndDown = IsDlgButtonChecked(hDlg, IDC_ALLOWLEFTRIGHT);
			SaveInputConfig();
			EndDialog(hDlg,0);
			if(hBmp)
			{
				DeleteObject(hBmp);
				hBmp=NULL;
			}
			break;

		case IDC_JPTOGGLE: // joypad Enable toggle
			index = SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_GETCURSEL,0,0);
			Joypad[index].Enabled=IsDlgButtonChecked(hDlg,IDC_JPTOGGLE);
			set_buttoninfo(index, hDlg); // update display of conflicts
			break;

		}
		switch(HIWORD(wParam))
		{
			case CBN_SELCHANGE:
				index = SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_GETCURSEL,0,0);
				SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_SETCURSEL,(WPARAM)index,0);
				if(index < 5)
				{
					SendDlgItemMessage(hDlg,IDC_JPTOGGLE,BM_SETCHECK, Joypad[index].Enabled ? (WPARAM)BST_CHECKED : (WPARAM)BST_UNCHECKED, 0);
					EnableWindow(GetDlgItem(hDlg,IDC_JPTOGGLE),TRUE);
				}
				else
				{
					SendDlgItemMessage(hDlg,IDC_JPTOGGLE,BM_SETCHECK, Joypad[index-5].Enabled ? (WPARAM)BST_CHECKED : (WPARAM)BST_UNCHECKED, 0);
					EnableWindow(GetDlgItem(hDlg,IDC_JPTOGGLE),FALSE);
				}

				set_buttoninfo(index,hDlg);

				EnableDisableKeyFields(index,hDlg);

				break;
		}
		return FALSE;

	}

	return FALSE;
}


bool S9xGetState (WORD KeyIdent)
{
	if(KeyIdent == 0 || KeyIdent == VK_ESCAPE) // if it's the 'disabled' key, it's never pressed
		return true;

	//TODO - option for background game keys
	if(g_hWnd != GetForegroundWindow())
		return true;

    if (KeyIdent & 0x8000) // if it's a joystick 'key':
    {
        int j = (KeyIdent >> 8) & 15;

//		S9xUpdateJoyState();

        switch (KeyIdent & 0xff)
        {
            case 0: return !Joystick [j].Left;
            case 1: return !Joystick [j].Right;
            case 2: return !Joystick [j].Up;
            case 3: return !Joystick [j].Down;
            case 4: return !Joystick [j].PovLeft;
            case 5: return !Joystick [j].PovRight;
            case 6: return !Joystick [j].PovUp;
            case 7: return !Joystick [j].PovDown;
			case 49:return !Joystick [j].PovDnLeft;
			case 50:return !Joystick [j].PovDnRight;
			case 51:return !Joystick [j].PovUpLeft;
			case 52:return !Joystick [j].PovUpRight;
            case 41:return !Joystick [j].ZUp;
            case 42:return !Joystick [j].ZDown;
            case 43:return !Joystick [j].RUp;
            case 44:return !Joystick [j].RDown;
            case 45:return !Joystick [j].UUp;
            case 46:return !Joystick [j].UDown;
            case 47:return !Joystick [j].VUp;
            case 48:return !Joystick [j].VDown;
            
            default:
                if ((KeyIdent & 0xff) > 40)
                    return true; // not pressed
                
                return !Joystick [j].Button [(KeyIdent & 0xff) - 12];
        }
    }

	// the pause key is special, need this to catch all presses of it
	// Both GetKeyState and GetAsyncKeyState cannot catch it anyway,
	// so this should be handled in WM_KEYDOWN message.
	if(KeyIdent == VK_PAUSE)
	{
		return true; // not pressed
//		if(GetAsyncKeyState(VK_PAUSE)) // not &'ing this with 0x8000 is intentional and necessary
//			return false;
	}

	SHORT gks = GetKeyState (KeyIdent);
    return ((gks & 0x80) == 0);
}

void S9xWinScanJoypads ()
{
    u32 PadState;

	S9xUpdateJoyState();

    for (int J = 0; J < 8; J++)
    {
        if (Joypad [J].Enabled)
        {
			// toggle checks
			{
       	     	PadState  = 0;
				PadState |= ToggleJoypadStorage[J].Left_Left||TurboToggleJoypadStorage[J].Left_Left			? LEFT_LEFT_MASK : 0;
				PadState |= ToggleJoypadStorage[J].Left_Right||TurboToggleJoypadStorage[J].Left_Right			? LEFT_RIGHT_MASK : 0;
				PadState |= ToggleJoypadStorage[J].Left_Up||TurboToggleJoypadStorage[J].Left_Up				? LEFT_UP_MASK : 0;
				PadState |= ToggleJoypadStorage[J].Left_Down||TurboToggleJoypadStorage[J].Left_Down			? LEFT_DOWN_MASK : 0;
				PadState |= ToggleJoypadStorage[J].Start||TurboToggleJoypadStorage[J].Start			? START_MASK : 0;
				PadState |= ToggleJoypadStorage[J].Select||TurboToggleJoypadStorage[J].Select		? SELECT_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].Lid||TurboToggleJoypadStorage[J].Lid				? LID_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].Debug||TurboToggleJoypadStorage[J].Debug			? DEBUG_MASK : 0;
				PadState |= ToggleJoypadStorage[J].A||TurboToggleJoypadStorage[J].A					? A_MASK : 0;
				PadState |= ToggleJoypadStorage[J].B||TurboToggleJoypadStorage[J].B					? B_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].X||TurboToggleJoypadStorage[J].X					? X_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].Y||TurboToggleJoypadStorage[J].Y					? Y_MASK : 0;
				PadState |= ToggleJoypadStorage[J].L||TurboToggleJoypadStorage[J].L					? L_MASK : 0;
				PadState |= ToggleJoypadStorage[J].R||TurboToggleJoypadStorage[J].R				    ? R_MASK : 0;
			}
			// auto-hold AND regular key/joystick presses
			if(S9xGetState(Joypad[J+5].Left_Left))//if the autohold modifier isn't held
			{
				PadState ^= (!S9xGetState(Joypad[J].R)||!S9xGetState(Joypad[J+5].R))      ?  R_MASK : 0;
				PadState ^= (!S9xGetState(Joypad[J].L)||!S9xGetState(Joypad[J+5].L))      ?  L_MASK : 0;
////				PadState ^= (!S9xGetState(Joypad[J].X)||!S9xGetState(Joypad[J+5].X))      ?  X_MASK : 0;
				PadState ^= (!S9xGetState(Joypad[J].A)||!S9xGetState(Joypad[J+5].A))      ? A_MASK : 0;
				PadState ^= (!S9xGetState(Joypad[J].Left_Right))  ?   LEFT_RIGHT_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Right_Up))  ? RIGHT_MASK + UP_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Right_Down)) ? RIGHT_MASK + DOWN_MASK : 0;
				PadState ^= (!S9xGetState(Joypad[J].Left_Left))   ?   LEFT_LEFT_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Left_Up)) ?   LEFT_MASK + UP_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Left_Down)) ?  LEFT_MASK + DOWN_MASK : 0;
				PadState ^= (!S9xGetState(Joypad[J].Left_Down))   ?   LEFT_DOWN_MASK : 0;
				PadState ^= (!S9xGetState(Joypad[J].Left_Up))     ?   LEFT_UP_MASK : 0;

				PadState ^= (!S9xGetState(Joypad[J].Right_Right))  ?   RIGHT_RIGHT_MASK : 0;
				PadState ^= (!S9xGetState(Joypad[J].Right_Left))   ?   RIGHT_LEFT_MASK : 0;
				PadState ^= (!S9xGetState(Joypad[J].Right_Down))   ?   RIGHT_DOWN_MASK : 0;
				PadState ^= (!S9xGetState(Joypad[J].Right_Up))     ?   RIGHT_UP_MASK : 0;


				PadState ^= (!S9xGetState(Joypad[J].Start)||!S9xGetState(Joypad[J+5].Start))  ?  START_MASK : 0;
				PadState ^= (!S9xGetState(Joypad[J].Select)||!S9xGetState(Joypad[J+5].Select)) ?  SELECT_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Y)||!S9xGetState(Joypad[J+5].Y))      ?  Y_MASK : 0;
				PadState ^= (!S9xGetState(Joypad[J].B)||!S9xGetState(Joypad[J+5].B))      ? B_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Lid)||!S9xGetState(Joypad[J+5].Lid))      ?  LID_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Debug)||!S9xGetState(Joypad[J+5].Debug))      ? DEBUG_MASK : 0;
			}
/*
			bool turbofy = !S9xGetState(Joypad[J+8].Up); // All Mod for turbo

			u32 TurboMask = 0;

			//handle turbo case! (autofire / auto-fire)
			if(turbofy || ((TurboMask&I_MASK))&&(PadState&I_MASK) || !S9xGetState(Joypad[J+8].I      )) PadState^=(joypads[J]&I_MASK);
			if(turbofy || ((TurboMask&II_MASK))&&(PadState&II_MASK) || !S9xGetState(Joypad[J+8].II     )) PadState^=(joypads[J]&II_MASK);
			if(turbofy || ((TurboMask&Y_MASK))&&(PadState&Y_MASK) || !S9xGetState(Joypad[J+8].Y       )) PadState^=(joypads[J]&Y_MASK);
			if(turbofy || ((TurboMask&X_MASK))&&(PadState&X_MASK) || !S9xGetState(Joypad[J+8].X       )) PadState^=(joypads[J]&X_MASK);
			if(turbofy || ((TurboMask&L_MASK))&&(PadState&L_MASK) || !S9xGetState(Joypad[J+8].L       )) PadState^=(joypads[J]&L_MASK);
			if(turbofy || ((TurboMask&R_MASK))&&(PadState&R_MASK) || !S9xGetState(Joypad[J+8].R       )) PadState^=(joypads[J]&R_MASK);
			if(turbofy || ((TurboMask&RUN_MASK))&&(PadState&RUN_MASK) || !S9xGetState(Joypad[J+8].Run )) PadState^=(joypads[J]&RUN_MASK);
			if(turbofy || ((TurboMask&SELECT_MASK))&&(PadState&SELECT_MASK) || !S9xGetState(Joypad[J+8].Select)) PadState^=(joypads[J]&SELECT_MASK);
			if(turbofy || ((TurboMask&DEBUG_MASK))&&(PadState&DEBUG_MASK) || !S9xGetState(Joypad[J+8].Debug)) PadState^=(joypads[J]&DEBUG_MASK);
			if(           ((TurboMask&LEFT_MASK))&&(PadState&LEFT_MASK)                                    ) PadState^=(joypads[J]&LEFT_MASK);
			if(           ((TurboMask&UP_MASK))&&(PadState&UP_MASK)                                      ) PadState^=(joypads[J]&UP_MASK);
			if(           ((TurboMask&RIGHT_MASK))&&(PadState&RIGHT_MASK)                                   ) PadState^=(joypads[J]&RIGHT_MASK);
			if(           ((TurboMask&DOWN_MASK))&&(PadState&DOWN_MASK)                                    ) PadState^=(joypads[J]&DOWN_MASK);
			if(           ((TurboMask&LID_MASK))&&(PadState&LID_MASK)                                    ) PadState^=(joypads[J]&LID_MASK);

			if(TurboToggleJoypadStorage[J].I     ) PadState^=(joypads[J]&I_MASK);
			if(TurboToggleJoypadStorage[J].II     ) PadState^=(joypads[J]&II_MASK);
			if(TurboToggleJoypadStorage[J].Y     ) PadState^=(joypads[J]&Y_MASK);
			if(TurboToggleJoypadStorage[J].X     ) PadState^=(joypads[J]&X_MASK);
			if(TurboToggleJoypadStorage[J].L     ) PadState^=(joypads[J]&L_MASK);
			if(TurboToggleJoypadStorage[J].R     ) PadState^=(joypads[J]&R_MASK);
			if(TurboToggleJoypadStorage[J].Run ) PadState^=(joypads[J]&RUN_MASK);
			if(TurboToggleJoypadStorage[J].Select) PadState^=(joypads[J]&SELECT_MASK);
			if(TurboToggleJoypadStorage[J].Left  ) PadState^=(joypads[J]&LEFT_MASK);
			if(TurboToggleJoypadStorage[J].Up    ) PadState^=(joypads[J]&UP_MASK);
			if(TurboToggleJoypadStorage[J].Right ) PadState^=(joypads[J]&RIGHT_MASK);
			if(TurboToggleJoypadStorage[J].Down  ) PadState^=(joypads[J]&DOWN_MASK);
			if(TurboToggleJoypadStorage[J].Lid  ) PadState^=(joypads[J]&LID_MASK);
			if(TurboToggleJoypadStorage[J].Debug ) PadState^=(joypads[J]&DEBUG_MASK);
			//end turbo case...
*/

			// enforce left+right/up+down disallowance here to
			// avoid recording unused l+r/u+d that will cause desyncs
			// when played back with l+r/u+d is allowed
			//if(!Settings.UpAndDown)
			//{
			//	if((PadState[1] & 2) != 0)
			//		PadState[1] &= ~(1);
			//	if((PadState[1] & 8) != 0)
			//		PadState[1] &= ~(4);
			//}

			if(PadState != 0)
				printf("%d",PadState);

            joypads [J] = PadState | 0x80000000;
        }
        else
            joypads [J] = 0;
    }

	// input from macro
	//for (int J = 0; J < 8; J++)
	//{
	//	if(MacroIsEnabled(J))
	//	{
	//		uint16 userPadState = joypads[J] & 0xFFFF;
	//		uint16 macroPadState = MacroInput(J);
	//		uint16 newPadState;

	//		switch(GUI.MacroInputMode)
	//		{
	//		case MACRO_INPUT_MOV:
	//			newPadState = macroPadState;
	//			break;
	//		case MACRO_INPUT_OR:
	//			newPadState = macroPadState | userPadState;
	//			break;
	//		case MACRO_INPUT_XOR:
	//			newPadState = macroPadState ^ userPadState;
	//			break;
	//		default:
	//			newPadState = userPadState;
	//			break;
	//		}

	//		PadState[0] = (uint8) ( newPadState       & 0xFF);
	//		PadState[1] = (uint8) ((newPadState >> 8) & 0xFF);

	//		// enforce left+right/up+down disallowance here to
	//		// avoid recording unused l+r/u+d that will cause desyncs
	//		// when played back with l+r/u+d is allowed
	//		if(!Settings.UpAndDown)
	//		{
	//			if((PadState[1] & 2) != 0)
	//				PadState[1] &= ~(1);
	//			if((PadState[1] & 8) != 0)
	//				PadState[1] &= ~(4);
	//		}

	//		joypads [J] = PadState [0] | (PadState [1] << 8) | 0x80000000;
	//	}
	//}

//#ifdef NETPLAY_SUPPORT
//    if (Settings.NetPlay)
//	{
//		// Send joypad position update to server
//		S9xNPSendJoypadUpdate (joypads [GUI.NetplayUseJoypad1 ? 0 : NetPlay.Player-1]);
//
//		// set input from network
//		for (int J = 0; J < NP_MAX_CLIENTS; J++)
//			joypads[J] = S9xNPGetJoypad (J);
//	}
//#endif
}

void input_feedback(BOOL enable)
{
	if (!Feedback) return;
	if (!pEffect) return;

	if (enable)
		pEffect->Start(2, 0);
	else
		pEffect->Stop();
}


void input_init()
{
	InitCustomControls();
	
	LoadInputConfig();
	LoadHotkeyConfig();

	di_init();
//	FeedbackON = input_feedback;
}

uint32 S9xReadJoypad (int which1)
{
    if (which1 > 4)
        return 0;

    if (which1 == 0 )//&& !Settings.NetPlay
        S9xWinScanJoypads ();

#ifdef NETPLAY_SUPPORT
    if (Settings.NetPlay)
	return (S9xNPGetJoypad (which1));
#endif

    return (joypads [which1]);
}

void S9xUpdateJoypadButtons ()
{
    int i;

	for (i = 0; i < 5; i++)
	{
	//	if (S9xLuaUsingJoypad(i))
	//		IPPU.Joypads[i] = S9xLuaReadJoypad(i);
	//	else
			joypads[i] = S9xReadJoypad (i);
	}

//	S9xMovieUpdate();
//	pad_read = false;
/*
	if(!Settings.UpAndDown)
	{
		for (i = 0; i < 5; i++)
		{
			if (IPPU.Joypads [i] & SNES_LEFT_MASK)
				IPPU.Joypads [i] &= ~SNES_RIGHT_MASK;
			if (IPPU.Joypads [i] & SNES_UP_MASK)
				IPPU.Joypads [i] &= ~SNES_DOWN_MASK;
		}
	}

    // BJ: This is correct behavior AFAICT (used to be Touhaiden hack)
    if (IPPU.Controller == SNES_JOYPAD || IPPU.Controller == SNES_MULTIPLAYER5)
    {
		for (i = 0; i < 5; i++)
		{
			if (IPPU.Joypads [i])
				IPPU.Joypads [i] |= 0xffff0000;
		}
    }*/
}

#include "hotkey.h"
#include "resource.h"
#include "ramwatch.h"		//In order to call UpdateRamWatch (for loadstate functions)
#include "ramsearch.h"		//In order to call UpdateRamSearch (for loadstate functions)
#include "../movie.h"

#include "types.h"
#include <commctrl.h>

//static TCHAR szHotkeysClassName[] = _T("InputCustomHot");

static LRESULT CALLBACK HotInputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

SCustomKeys CustomKeys;

bool AutoHoldPressed=false;

int SaveStateSlot=1;

void DisplayMessage(char* str) {

	MDFN_DispMessage(str);
}

///////////////////////////

#define INPUTCONFIG_LABEL_BLUE "Blue means the button is already mapped.\nPink means it conflicts with a custom hotkey.\nRed means it's reserved by Windows.\nButtons can be disabled using Escape.\nGrayed buttons arent supported yet (sorry!)"
#define INPUTCONFIG_LABEL_UNUSED ""

// gaming buttons and axes
#define GAMEDEVICE_JOYNUMPREFIX "(J%x)" // don't change this
#define GAMEDEVICE_JOYBUTPREFIX "#[%d]" // don't change this
#define GAMEDEVICE_XNEG "Left"
#define GAMEDEVICE_XPOS "Right"
#define GAMEDEVICE_YPOS "Up"
#define GAMEDEVICE_YNEG "Down"
#define GAMEDEVICE_R_XNEG "RS-Left"
#define GAMEDEVICE_R_XPOS "RS-Right"
#define GAMEDEVICE_R_YPOS "RS-Up"
#define GAMEDEVICE_R_YNEG "RS-Down"
#define GAMEDEVICE_POVLEFT "POV Left"
#define GAMEDEVICE_POVRIGHT "POV Right"
#define GAMEDEVICE_POVUP "POV Up"
#define GAMEDEVICE_POVDOWN "POV Down"
#define GAMEDEVICE_POVDNLEFT "POV Dn Left"
#define GAMEDEVICE_POVDNRIGHT "POV Dn Right"
#define GAMEDEVICE_POVUPLEFT  "POV Up Left"
#define GAMEDEVICE_POVUPRIGHT "POV Up Right"
#define GAMEDEVICE_ZPOS "Z Up"
#define GAMEDEVICE_ZNEG "Z Down"
#define GAMEDEVICE_RPOS "R Up"
#define GAMEDEVICE_RNEG "R Down"
#define GAMEDEVICE_UPOS "U Up"
#define GAMEDEVICE_UNEG "U Down"
#define GAMEDEVICE_VPOS "V Up"
#define GAMEDEVICE_VNEG "V Down"
#define GAMEDEVICE_BUTTON "Button %d"

// gaming general
#define GAMEDEVICE_DISABLED "Disabled"

// gaming keys
#define GAMEDEVICE_KEY "#%d"
#define GAMEDEVICE_NUMPADPREFIX "Numpad-%c"
#define GAMEDEVICE_VK_TAB "Tab"
#define GAMEDEVICE_VK_BACK "Backspace"
#define GAMEDEVICE_VK_CLEAR "Delete"
#define GAMEDEVICE_VK_RETURN "Enter"
#define GAMEDEVICE_VK_LSHIFT "LShift"
#define GAMEDEVICE_VK_RSHIFT "RShift"
#define GAMEDEVICE_VK_LCONTROL "LCtrl"
#define GAMEDEVICE_VK_RCONTROL "RCtrl"
#define GAMEDEVICE_VK_LMENU "LAlt"
#define GAMEDEVICE_VK_RMENU "RAlt"
#define GAMEDEVICE_VK_PAUSE "Pause"
#define GAMEDEVICE_VK_CAPITAL "Capslock"
#define GAMEDEVICE_VK_ESCAPE "Disabled"
#define GAMEDEVICE_VK_SPACE "Space"
#define GAMEDEVICE_VK_PRIOR "PgUp"
#define GAMEDEVICE_VK_NEXT "PgDn"
#define GAMEDEVICE_VK_HOME "Home"
#define GAMEDEVICE_VK_END "End"
#define GAMEDEVICE_VK_LEFT "Left"
#define GAMEDEVICE_VK_RIGHT "Right"
#define GAMEDEVICE_VK_UP "Up"
#define GAMEDEVICE_VK_DOWN "Down"
#define GAMEDEVICE_VK_SELECT "Select"
#define GAMEDEVICE_VK_PRINT "Print"
#define GAMEDEVICE_VK_EXECUTE "Execute"
#define GAMEDEVICE_VK_SNAPSHOT "SnapShot"
#define GAMEDEVICE_VK_INSERT "Insert"
#define GAMEDEVICE_VK_DELETE "Delete"
#define GAMEDEVICE_VK_HELP "Help"
#define GAMEDEVICE_VK_LWIN "LWinKey"
#define GAMEDEVICE_VK_RWIN "RWinKey"
#define GAMEDEVICE_VK_APPS "AppKey"
#define GAMEDEVICE_VK_MULTIPLY "Numpad *"
#define GAMEDEVICE_VK_ADD "Numpad +"
#define GAMEDEVICE_VK_SEPARATOR "Separator"
#define GAMEDEVICE_VK_OEM_1 "Semi-Colon"
#define GAMEDEVICE_VK_OEM_7 "Apostrophe"
#define GAMEDEVICE_VK_OEM_COMMA "Comma"
#define GAMEDEVICE_VK_OEM_PERIOD "Period"
#define GAMEDEVICE_VK_SUBTRACT "Numpad -"
#define GAMEDEVICE_VK_DECIMAL "Numpad ."
#define GAMEDEVICE_VK_DIVIDE "Numpad /"
#define GAMEDEVICE_VK_NUMLOCK "Num-lock"
#define GAMEDEVICE_VK_SCROLL "Scroll-lock"
#define GAMEDEVICE_VK_OEM_MINUS "-"
#define GAMEDEVICE_VK_OEM_PLUS "="
#define GAMEDEVICE_VK_SHIFT "Shift"
#define GAMEDEVICE_VK_CONTROL "Control"
#define GAMEDEVICE_VK_MENU "Alt"
#define GAMEDEVICE_VK_OEM_4 "["
#define GAMEDEVICE_VK_OEM_6 "]"
#define GAMEDEVICE_VK_OEM_5 "\\"
#define GAMEDEVICE_VK_OEM_2 "/"
#define GAMEDEVICE_VK_OEM_3 "`"
#define GAMEDEVICE_VK_F1 "F1"
#define GAMEDEVICE_VK_F2 "F2"
#define GAMEDEVICE_VK_F3 "F3"
#define GAMEDEVICE_VK_F4 "F4"
#define GAMEDEVICE_VK_F5 "F5"
#define GAMEDEVICE_VK_F6 "F6"
#define GAMEDEVICE_VK_F7 "F7"
#define GAMEDEVICE_VK_F8 "F8"
#define GAMEDEVICE_VK_F9 "F9"
#define GAMEDEVICE_VK_F10 "F10"
#define GAMEDEVICE_VK_F11 "F11"
#define GAMEDEVICE_VK_F12 "F12"
#define BUTTON_OK "&OK"
#define BUTTON_CANCEL "&Cancel"


////////////////////////////

// Hotkeys Dialog Strings
#define HOTKEYS_TITLE "Hotkey Configuration"
#define HOTKEYS_CONTROL_MOD "Ctrl + "
#define HOTKEYS_SHIFT_MOD "Shift + "
#define HOTKEYS_ALT_MOD "Alt + "
#define HOTKEYS_LABEL_BLUE "Blue means the hotkey is already mapped.\nPink means it conflicts with a game button.\nRed means it's reserved by Windows.\nA hotkey can be disabled using Escape."
#define HOTKEYS_HKCOMBO "Page %d"

#define CUSTKEY_ALT_MASK   0x01
#define CUSTKEY_CTRL_MASK  0x02
#define CUSTKEY_SHIFT_MASK 0x04

#define NUM_HOTKEY_CONTROLS 20

#define COUNT(a) (sizeof (a) / sizeof (a[0]))


static int lastTime = timeGetTime();

extern int KeyInDelayMSec;
extern int KeyInRepeatMSec;

VOID CALLBACK KeyInputTimer( UINT idEvent, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	bool S9xGetState (WORD KeyIdent);

	static DWORD lastTime = timeGetTime();
	DWORD currentTime = timeGetTime();

	static struct JoyState {
		bool wasPressed;
		DWORD firstPressedTime;
		DWORD lastPressedTime;
		WORD repeatCount;
	} joyState [256];
	static bool initialized = false;

	if(!initialized) {
		for(int i = 0; i < 256; i++) {
			joyState[i].wasPressed = false;
			joyState[i].repeatCount = 1;
		}
		initialized = true;
	}

	for (int i = 0; i < 256; i++) {
		bool active = !S9xGetState(i);

		if (active) {
			bool keyRepeat = (currentTime - joyState[i].firstPressedTime) >= (DWORD)KeyInDelayMSec;
			if (!joyState[i].wasPressed || keyRepeat) {
				if (!joyState[i].wasPressed)
					joyState[i].firstPressedTime = currentTime;
				joyState[i].lastPressedTime = currentTime;
				if (keyRepeat && joyState[i].repeatCount < 0xffff)
					joyState[i].repeatCount++;
				PostMessage(g_hWnd, WM_CUSTKEYDOWN, (WPARAM)(i),(LPARAM)(joyState[i].repeatCount | (joyState[i].wasPressed ? 0x40000000 : 0)));
			}
		}
		else {
			joyState[i].repeatCount = 1;
			if (joyState[i].wasPressed)
				PostMessage(g_hWnd, WM_CUSTKEYUP, (WPARAM)(i),(LPARAM)(joyState[i].repeatCount | (joyState[i].wasPressed ? 0x40000000 : 0)));
		}
		joyState[i].wasPressed = active;
	}
	lastTime = currentTime;
}


/////////////


bool IsReserved (WORD Key, int modifiers)
{
	// keys that do other stuff in Windows
	if(Key == VK_CAPITAL || Key == VK_NUMLOCK || Key == VK_SCROLL || Key == VK_SNAPSHOT
	|| Key == VK_LWIN    || Key == VK_RWIN    || Key == VK_APPS || Key == /*VK_SLEEP*/0x5F
	|| (Key == VK_F4 && (modifiers & CUSTKEY_ALT_MASK) != 0)) // alt-f4 (behaves unlike accelerators)
		return true;

	// menu shortcuts (accelerators) -- TODO: should somehow parse GUI.Accelerators for this information
	if(modifiers == CUSTKEY_CTRL_MASK
	 && (Key == 'O')
	|| modifiers == CUSTKEY_ALT_MASK
	 && (Key == VK_F5 || Key == VK_F7 || Key == VK_F8 || Key == VK_F9
	  || Key == 'R' || Key == 'T' || Key == /*VK_OEM_4*/0xDB || Key == /*VK_OEM_6*/0xDD
	  || Key == 'E' || Key == 'A' || Key == VK_RETURN || Key == VK_DELETE)
	  || Key == VK_MENU || Key == VK_CONTROL)
		return true;

	return false;
}

int HandleKeyUp(WPARAM wParam, LPARAM lParam, int modifiers)
{
	SCustomKey *key = &CustomKeys.key(0);

	while (!IsLastCustomKey(key)) {
		if (wParam == key->key && modifiers == key->modifiers && key->handleKeyUp) {
			key->handleKeyUp(key->param);
		}
		key++;
	}

	return 1;
}

int HandleKeyMessage(WPARAM wParam, LPARAM lParam, int modifiers)
{
	// update toggles
	for (int J = 0; J < 5; J++)
	{
		extern bool S9xGetState (WORD KeyIdent);
		if(Joypad[J].Enabled && (!S9xGetState(Joypad[J+5].Left_Left))) // enabled and Togglify
		{
			SJoypad & p = ToggleJoypadStorage[J];
			if(wParam == Joypad[J].L) p.L = !p.L;
			if(wParam == Joypad[J].R) p.R = !p.R;
			if(wParam == Joypad[J].A) p.A = !p.A;
			if(wParam == Joypad[J].B) p.B = !p.B;
//			if(wParam == Joypad[J].Y) p.Y = !p.Y;
//			if(wParam == Joypad[J].X) p.X = !p.X;
			if(wParam == Joypad[J].Start) p.Start = !p.Start;
			if(wParam == Joypad[J].Select) p.Select = !p.Select;
			if(wParam == Joypad[J].Left_Left) p.Left_Left = !p.Left_Left;
			if(wParam == Joypad[J].Left_Right) p.Left_Right = !p.Left_Right;
			if(wParam == Joypad[J].Left_Up) p.Left_Up = !p.Left_Up;
			if(wParam == Joypad[J].Left_Down) p.Left_Down = !p.Left_Down;
///					if(wParam == Joypad[J].Left_Down) p.Left_Down = !p.Left_Down;
///					if(wParam == Joypad[J].Left_Up) p.Left_Up = !p.Left_Up;
///					if(wParam == Joypad[J].Right_Down) p.Right_Down = !p.Right_Down;
///					if(wParam == Joypad[J].Right_Up) p.Right_Up = !p.Right_Up;
/*			if(!Settings.UpAndDown)
			{
				if(p.Left && p.Right)
					p.Left = p.Right = false;
				if(p.Up && p.Down)
					p.Up = p.Down = false;
			}*/
		}
/*		if(Joypad[J].Enabled && (!S9xGetState(Joypad[J+5].Down))) // enabled and turbo-togglify (TurboTog)
		{
			SJoypad & p = TurboToggleJoypadStorage[J];
			if(wParam == Joypad[J].L) p.L = !p.L;
			if(wParam == Joypad[J].R) p.R = !p.R;
			if(wParam == Joypad[J].A) p.A = !p.A;
			if(wParam == Joypad[J].B) p.B = !p.B;
			if(wParam == Joypad[J].Y) p.Y = !p.Y;
			if(wParam == Joypad[J].X) p.X = !p.X;
			if(wParam == Joypad[J].Start) p.Start = !p.Start;
			if(wParam == Joypad[J].Select) p.Select = !p.Select;
			if(wParam == Joypad[J].Left) p.Left = !p.Left;
			if(wParam == Joypad[J].Right) p.Right = !p.Right;
			if(wParam == Joypad[J].Up) p.Up = !p.Up;
			if(wParam == Joypad[J].Down) p.Down = !p.Down;
///					if(wParam == Joypad[J].Left_Down) p.Left_Down = !p.Left_Down;
///					if(wParam == Joypad[J].Left_Up) p.Left_Up = !p.Left_Up;
///					if(wParam == Joypad[J].Right_Down) p.Right_Down = !p.Right_Down;
///					if(wParam == Joypad[J].Right_Up) p.Right_Up = !p.Right_Up;
/*					if(!Settings.UpAndDown)
			{
				if(p.Left && p.Right && )
					p.Left = p.Right = false;
				if(p.Up && p.Down)
					p.Up = p.Down = false;
			}*/
//		}
		if(wParam == Joypad[J+5].Left_Right) // clear all
		{
			{
				SJoypad & p = ToggleJoypadStorage[J];
				p.L = false;
				p.R = false;
				p.A = false;
				p.B = false;
//				p.Y = false;
//				p.X = false;
				p.Start = false;
				p.Select = false;
				p.Left_Left = false;
				p.Left_Right = false;
				p.Left_Up = false;
				p.Left_Down = false;
			}
			{
				SJoypad & p = TurboToggleJoypadStorage[J];
				p.L = false;
				p.R = false;
				p.A = false;
				p.B = false;
//				p.Y = false;
//				p.X = false;
				p.Start = false;
				p.Select = false;
				p.Left_Left = false;
				p.Left_Right = false;
				p.Left_Up = false;
				p.Left_Down = false;
			}
			//MacroDisableAll();
//			MacroChangeState(J, false);
		}
	}
	bool hitHotKey = false;

	if(!(wParam == 0 || wParam == VK_ESCAPE)) // if it's the 'disabled' key, it's never pressed as a hotkey
	{
		SCustomKey *key = &CustomKeys.key(0);
		while (!IsLastCustomKey(key)) {
			if (wParam == key->key && modifiers == key->modifiers && key->handleKeyDown) {
				key->handleKeyDown(key->param);
				hitHotKey = true;
			}
			key++;
		}

		// don't pull down menu if alt is a hotkey or the menu isn't there, unless no game is running
		//if(!Settings.StopEmulation && ((wParam == VK_MENU || wParam == VK_F10) && (hitHotKey || GetMenu (GUI.hWnd) == NULL) && !GetAsyncKeyState(VK_F4)))
		/*if(((wParam == VK_MENU || wParam == VK_F10) && (hitHotKey || GetMenu (MainWindow->getHWnd()) == NULL) && !GetAsyncKeyState(VK_F4)))
		return 0;*/
		return 1;
	}

	return 1;
}

int GetModifiers(int key)
{
	int modifiers = 0;

	if (key == VK_MENU || key == VK_CONTROL || key == VK_SHIFT)
		return 0;

	if(GetAsyncKeyState(VK_MENU   )&0x8000) modifiers |= CUSTKEY_ALT_MASK;
	if(GetAsyncKeyState(VK_CONTROL)&0x8000) modifiers |= CUSTKEY_CTRL_MASK;
	if(GetAsyncKeyState(VK_SHIFT  )&0x8000) modifiers |= CUSTKEY_SHIFT_MASK;
	return modifiers;
}

void InitCustomControls()
{

    WNDCLASSEX wc;

    wc.cbSize         = sizeof(wc);
    wc.lpszClassName  = szClassName;
    wc.hInstance      = GetModuleHandle(0);
    wc.lpfnWndProc    = InputCustomWndProc;
    wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
    wc.hIcon          = 0;
    wc.lpszMenuName   = 0;
    wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
    wc.style          = 0;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = sizeof(InputCust *);
    wc.hIconSm        = 0;


    RegisterClassEx(&wc);

    wc.cbSize         = sizeof(wc);
    wc.lpszClassName  = szHotkeysClassName;
    wc.hInstance      = GetModuleHandle(0);
    wc.lpfnWndProc    = HotInputCustomWndProc;
    wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
    wc.hIcon          = 0;
    wc.lpszMenuName   = 0;
    wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
    wc.style          = 0;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = sizeof(InputCust *);
    wc.hIconSm        = 0;


    RegisterClassEx(&wc);

	wc.cbSize         = sizeof(wc);
    wc.lpszClassName  = szGuitarClassName;
    wc.hInstance      = GetModuleHandle(0);
    wc.lpfnWndProc    = GuitarInputCustomWndProc;
    wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
    wc.hIcon          = 0;
    wc.lpszMenuName   = 0;
    wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
    wc.style          = 0;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = sizeof(InputCust *);
    wc.hIconSm        = 0;


    RegisterClassEx(&wc);
}

int GetNumHotKeysAssignedTo (WORD Key, int modifiers)
{
	int count = 0;
	{
		#define MATCHES_KEY(k) \
			(Key != 0 && Key != VK_ESCAPE \
		   && ((Key == k->key && modifiers == k->modifiers) \
		   || (Key == VK_SHIFT   && k->modifiers & CUSTKEY_SHIFT_MASK) \
		   || (Key == VK_MENU    && k->modifiers & CUSTKEY_ALT_MASK) \
		   || (Key == VK_CONTROL && k->modifiers & CUSTKEY_CTRL_MASK) \
		   || (k->key == VK_SHIFT   && modifiers & CUSTKEY_SHIFT_MASK) \
		   || (k->key == VK_MENU    && modifiers & CUSTKEY_ALT_MASK) \
		   || (k->key == VK_CONTROL && modifiers & CUSTKEY_CTRL_MASK)))

		SCustomKey *key = &CustomKeys.key(0);
		while (!IsLastCustomKey(key)) {
			if (MATCHES_KEY(key)) {
				count++;
			}
			key++;
		}


		#undef MATCHES_KEY
	}
	return count;
}

void InitKeyCustomControl()
{

    WNDCLASSEX wc;

    wc.cbSize         = sizeof(wc);
    wc.lpszClassName  = szHotkeysClassName;
    wc.hInstance      = GetModuleHandle(0);
    wc.lpfnWndProc    = HotInputCustomWndProc;
    wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
    wc.hIcon          = 0;
    wc.lpszMenuName   = 0;
    wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
    wc.style          = 0;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = sizeof(InputCust *);
    wc.hIconSm        = 0;

}

void TranslateKey(WORD keyz,char *out)
{
//	sprintf(out,"%d",keyz);
//	return;

	char temp[128];
	if(keyz&0x8000)
	{
		sprintf(out,GAMEDEVICE_JOYNUMPREFIX,((keyz>>8)&0xF));
		switch(keyz&0xFF)
		{
		case 0:  strcat(out,GAMEDEVICE_XNEG); break;
		case 1:  strcat(out,GAMEDEVICE_XPOS); break;
        case 2:  strcat(out,GAMEDEVICE_YPOS); break;
		case 3:  strcat(out,GAMEDEVICE_YNEG); break;
		case 4:  strcat(out,GAMEDEVICE_POVLEFT); break;
		case 5:  strcat(out,GAMEDEVICE_POVRIGHT); break;
		case 6:  strcat(out,GAMEDEVICE_POVUP); break;
		case 7:  strcat(out,GAMEDEVICE_POVDOWN); break;
		case 8:  strcat(out,GAMEDEVICE_R_XNEG); break;
		case 9:  strcat(out,GAMEDEVICE_R_XPOS); break;
		case 10: strcat(out,GAMEDEVICE_R_YPOS); break;
		case 11: strcat(out,GAMEDEVICE_R_YNEG); break;
		case 49: strcat(out,GAMEDEVICE_POVDNLEFT); break;
		case 50: strcat(out,GAMEDEVICE_POVDNRIGHT); break;
		case 51: strcat(out,GAMEDEVICE_POVUPLEFT); break;
		case 52: strcat(out,GAMEDEVICE_POVUPRIGHT); break;
		case 41: strcat(out,GAMEDEVICE_ZPOS); break;
		case 42: strcat(out,GAMEDEVICE_ZNEG); break;
		case 43: strcat(out,GAMEDEVICE_RPOS); break;
		case 44: strcat(out,GAMEDEVICE_RNEG); break;
		case 45: strcat(out,GAMEDEVICE_UPOS); break;
		case 46: strcat(out,GAMEDEVICE_UNEG); break;
		case 47: strcat(out,GAMEDEVICE_VPOS); break;
		case 48: strcat(out,GAMEDEVICE_VNEG); break;
		default:
			if ((keyz & 0xff) > 40)
            {
				sprintf(temp,GAMEDEVICE_JOYBUTPREFIX,keyz&0xFF);
				strcat(out,temp);
				break;
            }

			sprintf(temp,GAMEDEVICE_BUTTON,(keyz&0xFF)-12);
			strcat(out,temp);
			break;

		}
		return;
	}
	sprintf(out,GAMEDEVICE_KEY,keyz);
	if((keyz>='0' && keyz<='9')||(keyz>='A' &&keyz<='Z'))
	{
		sprintf(out,"%c",keyz);
		return;
	}

	if( keyz >= VK_NUMPAD0 && keyz <= VK_NUMPAD9)
    {

		sprintf(out,GAMEDEVICE_NUMPADPREFIX,'0'+(keyz-VK_NUMPAD0));

        return ;
    }
	switch(keyz)
    {
        case 0:				sprintf(out,GAMEDEVICE_DISABLED); break;
        case VK_TAB:		sprintf(out,GAMEDEVICE_VK_TAB); break;
        case VK_BACK:		sprintf(out,GAMEDEVICE_VK_BACK); break;
        case VK_CLEAR:		sprintf(out,GAMEDEVICE_VK_CLEAR); break;
        case VK_RETURN:		sprintf(out,GAMEDEVICE_VK_RETURN); break;
        case VK_LSHIFT:		sprintf(out,GAMEDEVICE_VK_LSHIFT); break;
		case VK_RSHIFT:		sprintf(out,GAMEDEVICE_VK_RSHIFT); break;
        case VK_LCONTROL:	sprintf(out,GAMEDEVICE_VK_LCONTROL); break;
		case VK_RCONTROL:	sprintf(out,GAMEDEVICE_VK_RCONTROL); break;
        case VK_LMENU:		sprintf(out,GAMEDEVICE_VK_LMENU); break;
		case VK_RMENU:		sprintf(out,GAMEDEVICE_VK_RMENU); break;
        case VK_PAUSE:		sprintf(out,GAMEDEVICE_VK_PAUSE); break;
        case VK_CANCEL:		sprintf(out,GAMEDEVICE_VK_PAUSE); break; // the Pause key can resolve to either "Pause" or "Cancel" depending on when it's pressed
        case VK_CAPITAL:	sprintf(out,GAMEDEVICE_VK_CAPITAL); break;
        case VK_ESCAPE:		sprintf(out,GAMEDEVICE_VK_ESCAPE); break;
        case VK_SPACE:		sprintf(out,GAMEDEVICE_VK_SPACE); break;
        case VK_PRIOR:		sprintf(out,GAMEDEVICE_VK_PRIOR); break;
        case VK_NEXT:		sprintf(out,GAMEDEVICE_VK_NEXT); break;
        case VK_HOME:		sprintf(out,GAMEDEVICE_VK_HOME); break;
        case VK_END:		sprintf(out,GAMEDEVICE_VK_END); break;
        case VK_LEFT:		sprintf(out,GAMEDEVICE_VK_LEFT ); break;
        case VK_RIGHT:		sprintf(out,GAMEDEVICE_VK_RIGHT); break;
        case VK_UP:			sprintf(out,GAMEDEVICE_VK_UP); break;
        case VK_DOWN:		sprintf(out,GAMEDEVICE_VK_DOWN); break;
        case VK_SELECT:		sprintf(out,GAMEDEVICE_VK_SELECT); break;
        case VK_PRINT:		sprintf(out,GAMEDEVICE_VK_PRINT); break;
        case VK_EXECUTE:	sprintf(out,GAMEDEVICE_VK_EXECUTE); break;
        case VK_SNAPSHOT:	sprintf(out,GAMEDEVICE_VK_SNAPSHOT); break;
        case VK_INSERT:		sprintf(out,GAMEDEVICE_VK_INSERT); break;
        case VK_DELETE:		sprintf(out,GAMEDEVICE_VK_DELETE); break;
        case VK_HELP:		sprintf(out,GAMEDEVICE_VK_HELP); break;
        case VK_LWIN:		sprintf(out,GAMEDEVICE_VK_LWIN); break;
        case VK_RWIN:		sprintf(out,GAMEDEVICE_VK_RWIN); break;
        case VK_APPS:		sprintf(out,GAMEDEVICE_VK_APPS); break;
        case VK_MULTIPLY:	sprintf(out,GAMEDEVICE_VK_MULTIPLY); break;
        case VK_ADD:		sprintf(out,GAMEDEVICE_VK_ADD); break;
        case VK_SEPARATOR:	sprintf(out,GAMEDEVICE_VK_SEPARATOR); break;
		case /*VK_OEM_1*/0xBA:		sprintf(out,GAMEDEVICE_VK_OEM_1); break;
        case /*VK_OEM_2*/0xBF:		sprintf(out,GAMEDEVICE_VK_OEM_2); break;
        case /*VK_OEM_3*/0xC0:		sprintf(out,GAMEDEVICE_VK_OEM_3); break;
        case /*VK_OEM_4*/0xDB:		sprintf(out,GAMEDEVICE_VK_OEM_4); break;
        case /*VK_OEM_5*/0xDC:		sprintf(out,GAMEDEVICE_VK_OEM_5); break;
        case /*VK_OEM_6*/0xDD:		sprintf(out,GAMEDEVICE_VK_OEM_6); break;
		case /*VK_OEM_7*/0xDE:		sprintf(out,GAMEDEVICE_VK_OEM_7); break;
		case /*VK_OEM_COMMA*/0xBC:	sprintf(out,GAMEDEVICE_VK_OEM_COMMA );break;
		case /*VK_OEM_PERIOD*/0xBE:	sprintf(out,GAMEDEVICE_VK_OEM_PERIOD);break;
        case VK_SUBTRACT:	sprintf(out,GAMEDEVICE_VK_SUBTRACT); break;
        case VK_DECIMAL:	sprintf(out,GAMEDEVICE_VK_DECIMAL); break;
        case VK_DIVIDE:		sprintf(out,GAMEDEVICE_VK_DIVIDE); break;
        case VK_NUMLOCK:	sprintf(out,GAMEDEVICE_VK_NUMLOCK); break;
        case VK_SCROLL:		sprintf(out,GAMEDEVICE_VK_SCROLL); break;
        case /*VK_OEM_MINUS*/0xBD:	sprintf(out,GAMEDEVICE_VK_OEM_MINUS); break;
        case /*VK_OEM_PLUS*/0xBB:	sprintf(out,GAMEDEVICE_VK_OEM_PLUS); break;
        case VK_SHIFT:		sprintf(out,GAMEDEVICE_VK_SHIFT); break;
        case VK_CONTROL:	sprintf(out,GAMEDEVICE_VK_CONTROL); break;
        case VK_MENU:		sprintf(out,GAMEDEVICE_VK_MENU); break;
        case VK_F1:			sprintf(out,GAMEDEVICE_VK_F1); break;
        case VK_F2:			sprintf(out,GAMEDEVICE_VK_F2); break;
        case VK_F3:			sprintf(out,GAMEDEVICE_VK_F3); break;
        case VK_F4:			sprintf(out,GAMEDEVICE_VK_F4); break;
        case VK_F5:			sprintf(out,GAMEDEVICE_VK_F5); break;
        case VK_F6:			sprintf(out,GAMEDEVICE_VK_F6); break;
        case VK_F7:			sprintf(out,GAMEDEVICE_VK_F7); break;
        case VK_F8:			sprintf(out,GAMEDEVICE_VK_F8); break;
        case VK_F9:			sprintf(out,GAMEDEVICE_VK_F9); break;
        case VK_F10:		sprintf(out,GAMEDEVICE_VK_F10); break;
        case VK_F11:		sprintf(out,GAMEDEVICE_VK_F11); break;
        case VK_F12:		sprintf(out,GAMEDEVICE_VK_F12); break;
    }

    return ;



}
static void TranslateKeyWithModifiers(int wParam, int modifiers, char * outStr)
{

	// if the key itself is a modifier, special case output:
	if(wParam == VK_SHIFT)
		strcpy(outStr, "Shift");
	else if(wParam == VK_MENU)
		strcpy(outStr, "Alt");
	else if(wParam == VK_CONTROL)
		strcpy(outStr, "Control");
	else
	{
		// otherwise, prepend the modifier(s)
		if(wParam != VK_ESCAPE && wParam != 0)
		{
			if((modifiers & CUSTKEY_CTRL_MASK) != 0)
			{
				sprintf(outStr,HOTKEYS_CONTROL_MOD);
				outStr += strlen(HOTKEYS_CONTROL_MOD);
			}
			if((modifiers & CUSTKEY_ALT_MASK) != 0)
			{
				sprintf(outStr,HOTKEYS_ALT_MOD);
				outStr += strlen(HOTKEYS_ALT_MOD);
			}
			if((modifiers & CUSTKEY_SHIFT_MASK) != 0)
			{
				sprintf(outStr,HOTKEYS_SHIFT_MOD);
				outStr += strlen(HOTKEYS_SHIFT_MOD);
			}
		}

		// and append the translated non-modifier key
		TranslateKey(wParam,outStr);
	}
}

//static bool keyPressLock = false;

//HWND funky;

InputCust * GetInputCustom(HWND hwnd)
{
	return (InputCust *)GetWindowLong(hwnd, 0);
}

void SetInputCustom(HWND hwnd, InputCust *icp)
{
    SetWindowLong(hwnd, 0, (LONG)icp);
}
LRESULT InputCustom_OnPaint(InputCust *ccp, WPARAM wParam, LPARAM lParam)
{
    HDC				hdc;
    PAINTSTRUCT		ps;
    HANDLE			hOldFont;
    TCHAR			szText[200];
    RECT			rect;
	SIZE			sz;
	int				x,y;

    // Get a device context for this window
    hdc = BeginPaint(ccp->hwnd, &ps);

    // Set the font we are going to use
    hOldFont = SelectObject(hdc, ccp->hFont);

    // Set the text colours
    SetTextColor(hdc, ccp->crForeGnd);
    SetBkColor  (hdc, ccp->crBackGnd);

    // Find the text to draw
    GetWindowText(ccp->hwnd, szText, sizeof(szText));

    // Work out where to draw
    GetClientRect(ccp->hwnd, &rect);


    // Find out how big the text will be
    GetTextExtentPoint32(hdc, szText, lstrlen(szText), &sz);

    // Center the text
    x = (rect.right  - sz.cx) / 2;
    y = (rect.bottom - sz.cy) / 2;

    // Draw the text
    ExtTextOut(hdc, x, y, ETO_OPAQUE, &rect, szText, lstrlen(szText), 0);

    // Restore the old font when we have finished
    SelectObject(hdc, hOldFont);

    // Release the device context
    EndPaint(ccp->hwnd, &ps);

    return 0;
}

static LRESULT CALLBACK HotInputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// retrieve the custom structure POINTER for THIS window
    InputCust *icp = GetInputCustom(hwnd);
	HWND pappy = (HWND__ *)GetWindowLongPtr(hwnd,GWL_HWNDPARENT);
	funky= hwnd;

	static HWND selectedItem = NULL;

	char temp[100];
	COLORREF col;
    switch(msg)
    {

	case WM_GETDLGCODE:
		return DLGC_WANTARROWS|DLGC_WANTALLKEYS|DLGC_WANTCHARS;
		break;


    case WM_NCCREATE:

        // Allocate a new CustCtrl structure for this window.
        icp = (InputCust *) malloc( sizeof(InputCust) );

        // Failed to allocate, stop window creation.
        if(icp == NULL) return FALSE;

        // Initialize the CustCtrl structure.
        icp->hwnd      = hwnd;
        icp->crForeGnd = GetSysColor(COLOR_WINDOWTEXT);
        icp->crBackGnd = GetSysColor(COLOR_WINDOW);
        icp->hFont     = (HFONT__ *) GetStockObject(DEFAULT_GUI_FONT);

        // Assign the window text specified in the call to CreateWindow.
        SetWindowText(hwnd, ((CREATESTRUCT *)lParam)->lpszName);

        // Attach custom structure to this window.
        SetInputCustom(hwnd, icp);

		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		keyPressLock = false;

		selectedItem = NULL;

		SetTimer(hwnd,747,125,NULL);

        // Continue with window creation.
        return TRUE;

    // Clean up when the window is destroyed.
    case WM_NCDESTROY:
        free(icp);
        break;
	case WM_PAINT:
		return InputCustom_OnPaint(icp,wParam,lParam);
		break;
	case WM_ERASEBKGND:
		return 1;
/*
	case WM_KEYUP:
		{
			int count = 0;
			for(int i=0;i<256;i++)
				if(GetAsyncKeyState(i) & 1)
					count++;

			if(count < 2)
			{
				int p = count;
			}
			if(count < 1)
			{
				int p = count;
			}

			TranslateKey(wParam,temp);
			col = CheckButtonKey(wParam);

			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
			SetWindowText(hwnd,temp);
			InvalidateRect(icp->hwnd, NULL, FALSE);
			UpdateWindow(icp->hwnd);
			SendMessage(pappy,WM_USER+43,wParam,(LPARAM)hwnd);
		}
		break;
*/
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:

		{
			int count = 0;
			for(int i=2;i<256;i++)
			{
				if(i >= VK_LSHIFT && i <= VK_RMENU)
					continue;
				if(GetAsyncKeyState(i) & 1)
					count++;
			}

			if(count <= 1)
			{
				keyPressLock = false;
			}
		}

		// no break

	case WM_USER+45:
		// assign a hotkey:
		{
			// don't assign pure modifiers on key-down (they're assigned on key-up)
			if(wParam == VK_SHIFT || wParam == VK_MENU || wParam == VK_CONTROL)
				break;

			int modifiers = 0;
			if(GetAsyncKeyState(VK_MENU))
				modifiers |= CUSTKEY_ALT_MASK;
			if(GetAsyncKeyState(VK_CONTROL))
				modifiers |= CUSTKEY_CTRL_MASK;
			if(GetAsyncKeyState(VK_SHIFT))
				modifiers |= CUSTKEY_SHIFT_MASK;

			TranslateKeyWithModifiers(wParam, modifiers, temp);

			col = CheckHotKey(wParam,modifiers);
///			if(col == RGB(255,0,0)) // un-redify
///				col = RGB(255,255,255);

			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
			SetWindowText(hwnd,temp);
			InvalidateRect(icp->hwnd, NULL, FALSE);
			UpdateWindow(icp->hwnd);
			SendMessage(pappy,WM_USER+43,wParam,(LPARAM)hwnd);

			keyPressLock = true;

		}
		break;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		if(!keyPressLock)
		{
			int count = 0;
			for(int i=2;i<256;i++)
			{
				if(i >= VK_LSHIFT && i <= VK_RMENU)
					continue;
				if(GetAsyncKeyState(i) & 1) // &1 seems to solve an weird non-zero return problem, don't know why
					count++;
			}
			if(count <= 1)
			{
				if(wParam == VK_SHIFT || wParam == VK_MENU || wParam == VK_CONTROL)
				{
					if(wParam == VK_SHIFT)
						sprintf(temp, "Shift");
					if(wParam == VK_MENU)
						sprintf(temp, "Alt");
					if(wParam == VK_CONTROL)
						sprintf(temp, "Control");

					col = CheckHotKey(wParam,0);

					icp->crForeGnd = ((~col) & 0x00ffffff);
					icp->crBackGnd = col;
					SetWindowText(hwnd,temp);
					InvalidateRect(icp->hwnd, NULL, FALSE);
					UpdateWindow(icp->hwnd);
					SendMessage(pappy,WM_USER+43,wParam,(LPARAM)hwnd);
				}
			}
		}
		break;
	case WM_USER+44:

		// set a hotkey field:
		{
		int modifiers = lParam;

		TranslateKeyWithModifiers(wParam, modifiers, temp);

		if(IsWindowEnabled(hwnd))
		{
			col = CheckHotKey(wParam,modifiers);
///			if(col == RGB(255,0,0)) // un-redify
///				col = RGB(255,255,255);
		}
		else
		{
			col = RGB( 192,192,192);
		}
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		}
		break;

	case WM_SETFOCUS:
	{
		selectedItem = hwnd;
		col = RGB( 0,255,0);
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
//		tid = wParam;

		break;
	}
	case WM_KILLFOCUS:
	{
		selectedItem = NULL;
		SendMessage(pappy,WM_USER+46,wParam,(LPARAM)hwnd); // refresh fields on deselect
		break;
	}

	case WM_TIMER:
		if(hwnd == selectedItem)
		{
			//FunkyJoyStickTimer();
		}
		SetTimer(hwnd,747,125,NULL);
		break;
	case WM_LBUTTONDOWN:
		SetFocus(hwnd);
		break;
	case WM_ENABLE:
		COLORREF col;
		if(wParam)
		{
			col = RGB( 255,255,255);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		else
		{
			col = RGB( 192,192,192);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		return true;
    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
/////////////////

static void set_hotkeyinfo(HWND hDlg)
{
	HotkeyPage page = (HotkeyPage) SendDlgItemMessage(hDlg,IDC_HKCOMBO,CB_GETCURSEL,0,0);
	SCustomKey *key = &CustomKeys.key(0);
	int i = 0;

	while (!IsLastCustomKey(key) && i < NUM_HOTKEY_CONTROLS) {
		if (page == key->page) {
			SendDlgItemMessage(hDlg, IDC_HOTKEY_Table[i], WM_USER+44, key->key, key->modifiers);
			SetDlgItemTextW(hDlg, IDC_LABEL_HK_Table[i], key->name.c_str());
			ShowWindow(GetDlgItem(hDlg, IDC_HOTKEY_Table[i]), SW_SHOW);
			i++;
		}
		key++;
	}
	// disable unused controls
	for (; i < NUM_HOTKEY_CONTROLS; i++) {
		SendDlgItemMessage(hDlg, IDC_HOTKEY_Table[i], WM_USER+44, 0, 0);
		SetDlgItemText(hDlg, IDC_LABEL_HK_Table[i], (INPUTCONFIG_LABEL_UNUSED));
		ShowWindow(GetDlgItem(hDlg, IDC_HOTKEY_Table[i]), SW_HIDE);
	}
}

static void ReadHotkey(const char* name, WORD& output)
{
	UINT temp;
	temp = GetPrivateProfileIntA("Hotkeys",name,-1,IniName);
	if(temp != -1) {
		output = temp;
	}
}

void LoadHotkeyConfig()
{
	
	SCustomKey *key = &CustomKeys.key(0); //TODO

	while (!IsLastCustomKey(key)) {
		ReadHotkey(key->code,key->key); 
		std::string modname = (std::string)key->code + (std::string)" MOD";
		ReadHotkey(modname.c_str(),key->modifiers);
		key++;
	}
}


static void SaveHotkeyConfig()//TODO
{
	SCustomKey *key = &CustomKeys.key(0);

	while (!IsLastCustomKey(key)) {
		WritePrivateProfileInt("Hotkeys",(char*)key->code,key->key,IniName);
		std::string modname = (std::string)key->code + (std::string)" MOD";
		WritePrivateProfileInt("Hotkeys",(char*)modname.c_str(),key->modifiers,IniName);
		key++;
	}
}

// DlgHotkeyConfig
INT_PTR CALLBACK DlgHotkeyConfig(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int i, which;
	static HotkeyPage page = (HotkeyPage) 0;


	static SCustomKeys keys;

	//HBRUSH g_hbrBackground;
switch(msg)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint (hDlg, &ps);

			EndPaint (hDlg, &ps);
		}
		return true;
	case WM_INITDIALOG:
		//if(DirectX.Clipped) S9xReRefresh();
		SetWindowText(hDlg,HOTKEYS_TITLE);

		// insert hotkey page list items
		for(i = 0 ; i < NUM_HOTKEY_PAGE ; i++)
		{
			SendDlgItemMessage(hDlg, IDC_HKCOMBO, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)hotkeyPageTitle[i]);
		}

		SendDlgItemMessage(hDlg,IDC_HKCOMBO,CB_SETCURSEL,(WPARAM)0,0);

		InitCustomKeys(&keys);
		CopyCustomKeys(&keys, &CustomKeys);
		for( i=0;i<256;i++)
		{
			GetAsyncKeyState(i);
		}

		SetDlgItemText(hDlg,IDC_LABEL_BLUE,HOTKEYS_LABEL_BLUE);

		set_hotkeyinfo(hDlg);

		PostMessage(hDlg,WM_COMMAND, CBN_SELCHANGE<<16, 0);

		SetFocus(GetDlgItem(hDlg,IDC_HKCOMBO));


		return true;
		break;
	case WM_CLOSE:
		EndDialog(hDlg, 0);
		return TRUE;
	case WM_USER+46:
		// refresh command, for clicking away from a selected field
		page = (HotkeyPage) SendDlgItemMessage(hDlg, IDC_HKCOMBO, CB_GETCURSEL, 0, 0);
		set_hotkeyinfo(hDlg);
		return TRUE;
	case WM_USER+43:
	{
		//MessageBox(hDlg,"USER+43 CAUGHT","moo",MB_OK);
		int modifiers = GetModifiers(wParam);

		page = (HotkeyPage) SendDlgItemMessage(hDlg, IDC_HKCOMBO, CB_GETCURSEL, 0, 0);
		wchar_t text[256];

		which = GetDlgCtrlID((HWND)lParam);
		for (i = 0; i < NUM_HOTKEY_CONTROLS; i++) {
			if (which == IDC_HOTKEY_Table[i])
				break;
		}
		GetDlgItemTextW(hDlg, IDC_LABEL_HK_Table[i], text, COUNT(text));

		SCustomKey *key = &CustomKeys.key(0);
		while (!IsLastCustomKey(key)) {
			if (page == key->page) {
				if(text == key->name) {
					key->key = wParam;
					key->modifiers = modifiers;
					break;
				}
			}
			key++;
		}

		set_hotkeyinfo(hDlg);
		PostMessage(hDlg,WM_NEXTDLGCTL,0,0);
//		PostMessage(hDlg,WM_KILLFOCUS,0,0);
	}
		return true;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDCANCEL:
			CopyCustomKeys(&CustomKeys, &keys);
			EndDialog(hDlg,0);
			break;
		case IDOK:
			SaveHotkeyConfig();
			EndDialog(hDlg,0);
			break;
		}
		switch(HIWORD(wParam))
		{
			case CBN_SELCHANGE:
				page = (HotkeyPage) SendDlgItemMessage(hDlg, IDC_HKCOMBO, CB_GETCURSEL, 0, 0);
				SendDlgItemMessage(hDlg, IDC_HKCOMBO, CB_SETCURSEL, (WPARAM)page, 0);

				set_hotkeyinfo(hDlg);

				SetFocus(GetDlgItem(hDlg, IDC_HKCOMBO));

				break;
		}
		return FALSE;

	}

	return FALSE;
}
void RunHotkeyConfig()
{
	DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_KEYCUSTOM), g_hWnd, DlgHotkeyConfig);
}

////////////////////////////
bool IsLastCustomKey (const SCustomKey *key)
{
	return (key->key == 0xFFFF && key->modifiers == 0xFFFF);
}

void SetLastCustomKey (SCustomKey *key)
{
	key->key = 0xFFFF;
	key->modifiers = 0xFFFF;
}

void ZeroCustomKeys (SCustomKeys *keys)
{
	UINT i = 0;

	SetLastCustomKey(&keys->LastItem);
	while (!IsLastCustomKey(&keys->key(i))) {
		keys->key(i).key = 0;
		keys->key(i).modifiers = 0;
		i++;
	};
}


void CopyCustomKeys (SCustomKeys *dst, const SCustomKeys *src)
{
	UINT i = 0;

	do {
		dst->key(i) = src->key(i);
	} while (!IsLastCustomKey(&src->key(i++)));
}

//======================================================================================
//=====================================HANDLERS=========================================
//======================================================================================
//void HK_OpenROM(int) {OpenFile();}
void HK_Screenshot(int param)
{
void MDFNI_SaveSnapshot(void);
//NEWTODO
//MDFNI_SaveSnapshot();
int MDFN_SavePNGSnapshot(const char *fname, const MDFN_Surface *src, const MDFN_Rect *rect);
MDFN_SavePNGSnapshot("vbjin.png",espec.surface, (MDFN_Rect*)&espec.DisplayRect);
    /*
	OPENFILENAME ofn;
	char * ptr;
    char filename[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = (LPCWSTR)"png file (*.png)\0*.png\0Bmp file (*.bmp)\0*.bmp\0Any file (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile =  (LPWSTR)filename;
	ofn.lpstrTitle = (LPCWSTR)"Print Screen Save As";
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = (LPWSTR)"png";
	ofn.Flags = OFN_OVERWRITEPROMPT;
	GetSaveFileName(&ofn);

	ptr = strrchr(filename,'.');//look for the last . in the filename

	if ( ptr != 0 ) {
		if (( strcmp ( ptr, ".BMP" ) == 0 ) ||
			( strcmp ( ptr, ".bmp" ) == 0 )) 
		{
//			NDS_WriteBMP(filename);
		}
		if (( strcmp ( ptr, ".PNG" ) == 0 ) ||
			( strcmp ( ptr, ".png" ) == 0 )) 
		{
	//		NDS_WritePNG(filename);
		}
	}
	*/
	//adelikat: TODO: I guess we should delete the above code?
//	YuiScreenshot(g_hWnd);
}

extern int MDFNSS_Save(const char *fname, const char *suffix, const MDFN_Surface *surface, const MDFN_Rect *DisplayRect, const MDFN_Rect *LineWidths);

extern int MDFNSS_Load(const char *fname, const char *suffix);
void HK_StateSaveSlot(int num)
{
	CurrentState = num;
	//NEWTODO
	//	MDFNSS_Save(NULL, NULL, reinterpret_cast<const MDFN_Surface*> (VTBuffer[VTBackBuffer]), NULL, (MDFN_Rect *)VTLineWidths[VTBackBuffer]);
	MDFNSS_Save(NULL, NULL, espec.surface, (MDFN_Rect*)&espec.DisplayRect, (MDFN_Rect *)VTLineWidths[VTBackBuffer]);
//(MDFN_Rect *)&VTDisplayRects[VTBackBuffer]
}

void HK_StateLoadSlot(int num)
{
	CurrentState = num;
	//NEWTODO
	MDFNSS_Load(NULL, NULL);
#if USE_DDRAW
	ClearDirectDrawOutput();
#endif // #if USE_DDRAW
}

void HK_StateSetSlot(int num)
{
	char str[64];
	CurrentState=num;
	sprintf(str, "State %d selected.", CurrentState);	
	DisplayMessage(str);
}

void HK_StateQuickSaveSlot(int)
{
		MDFNSS_Save(NULL, NULL, espec.surface, (MDFN_Rect*)&espec.DisplayRect, (MDFN_Rect *)VTLineWidths[VTBackBuffer]);

//	MDFNSS_Save(NULL, NULL, (uint32 *)VTBuffer[VTBackBuffer], (MDFN_Rect *)VTLineWidths[VTBackBuffer]);
}

void HK_StateQuickLoadSlot(int)
{
	MDFNSS_Load(NULL, NULL);
#if USE_DDRAW
	ClearDirectDrawOutput();
#endif // #if USE_DDRAW
}

void HK_AutoHoldClearKeyDown(int) {
	
//	ClearAutoHold();
}

//NEWTODO extern void PCE_Power(void);
void HK_Reset(int) {}//NEWTODOPCE_Power();}
void HK_HardReset(int) {}//NEWTODOPCE_Power();}

void HK_RecordAVI(int) {RecordAvi();}
void HK_StopAVI(int) {StopAvi();}

//void HK_ToggleFrame(int) {frameCounterDisplay ^= true;}
//void HK_ToggleFPS(int) {FpsDisplay ^= true;}
//void HK_ToggleInput(int) {ShowInputDisplay ^= true;}
//void HK_ToggleLag(int) {ShowLagFrameCounter ^= true;}
void HK_ToggleReadOnly(int) 
{
	//MovieToggleReadOnly();
	ToggleReadOnly();
}

void HK_PlayMovie(int)   {PlayMovie(g_hWnd);}
void HK_PlayMovieFromBeginning(int)   {FCEUI_MoviePlayFromBeginning();}
void HK_RecordMovie(int) {}//RecordMovie(g_hWnd);}
void HK_StopMovie(int)   {FCEUI_StopMovie();}

bool VDC_ToggleLayer(int which);
//NEWTODO
void HK_ToggleNBG0(int){} //{VDC_ToggleLayer(0);}
void HK_ToggleNBG1(int) {}//{VDC_ToggleLayer(1);}
void HK_ToggleNBG2(int){} //{VDC_ToggleLayer(2);}
void HK_ToggleNBG3(int) {}//{VDC_ToggleLayer(3);}			
void HK_ToggleOSD(int) {};//ToggleFPS();}			
/*
void HK_AutoHoldKeyDown(int) {AutoHoldPressed = true;}
void HK_AutoHoldKeyUp(int) {AutoHoldPressed = false;}

void HK_TurboRightKeyDown(int) { Turbo.Right = true; }
void HK_TurboRightKeyUp(int) { Turbo.Right = false; }

void HK_TurboLeftKeyDown(int) { Turbo.Left = true; }
void HK_TurboLeftKeyUp(int) { Turbo.Left = false; }

void HK_TurboRKeyDown(int) { Turbo.R = true; }
void HK_TurboRKeyUp(int) { Turbo.R = false; }

void HK_TurboLKeyDown(int) { Turbo.L = true; }
void HK_TurboLKeyUp(int) { Turbo.L = false; }

void HK_TurboDownKeyDown(int) { Turbo.Down = true; }
void HK_TurboDownKeyUp(int) { Turbo.Down = false; }

void HK_TurboUpKeyDown(int) { Turbo.Up = true; }
void HK_TurboUpKeyUp(int) { Turbo.Up = false; }

void HK_TurboBKeyDown(int) { Turbo.B = true; }
void HK_TurboBKeyUp(int) { Turbo.B = false; }

void HK_TurboAKeyDown(int) { Turbo.A = true; }
void HK_TurboAKeyUp(int) { Turbo.A = false; }

void HK_TurboXKeyDown(int) { Turbo.X = true; }
void HK_TurboXKeyUp(int) { Turbo.X = false; }

void HK_TurboYKeyDown(int) { Turbo.Y = true; }
void HK_TurboYKeyUp(int) { Turbo.Y = false; }

void HK_TurboStartKeyDown(int) { Turbo.Start = true; }
void HK_TurboStartKeyUp(int) { Turbo.Start = false; }

void HK_TurboSelectKeyDown(int) { Turbo.Select = true; }
void HK_TurboSelectKeyUp(int) { Turbo.Select = false; }
*/

void HK_ToggleFullScreen(int) {};// ToggleFullScreenHK(); }

void HK_NextSaveSlot(int) { 
	
//	lastSaveState++; 
//	if(lastSaveState>9) 
//		lastSaveState=0; 
//	SaveStateMessages(lastSaveState,2);
}

void HK_PreviousSaveSlot(int) { 

//	if(lastSaveState==0) 
//		lastSaveState=9; 
//	else
//		lastSaveState--;
//	SaveStateMessages(lastSaveState,2); 
}

void FrameAdvance(bool state);

void HK_FrameAdvanceKeyDown(int) { FrameAdvance(true); }
void HK_FrameAdvanceKeyUp(int) { FrameAdvance(false); }

void HK_Pause(int) {pcejin.pause();}

void HK_FastForwardToggle(int) {pcejin.fastForward ^=1;}; //FastForward ^=1; }
void HK_FastForwardKeyDown(int) {pcejin.fastForward=true;}; //SpeedThrottleEnable(); }
void HK_FastForwardKeyUp(int) {pcejin.fastForward=false;}; //SpeedThrottleDisable(); }
void HK_IncreaseSpeed(int) { IncreaseSpeed(); }
void HK_DecreaseSpeed(int) { DecreaseSpeed(); }



//======================================================================================
//=====================================DEFINITIONS======================================
//======================================================================================

void InitCustomKeys (SCustomKeys *keys)
{
	UINT i = 0;

	SetLastCustomKey(&keys->LastItem);
	while (!IsLastCustomKey(&keys->key(i))) {
		SCustomKey &key = keys->key(i);
		key.key = 0;
		key.modifiers = 0;
		key.handleKeyDown = NULL;
		key.handleKeyUp = NULL;
		key.page = NUM_HOTKEY_PAGE;
		key.param = 0;

		//keys->key[i].timing = PROCESS_NOW;
		i++;
	};

	//Main Page---------------------------------------
/*	keys->OpenROM.handleKeyDown = HK_OpenROM;
	keys->OpenROM.code = "OpenROM";
	keys->OpenROM.name = L"Open ROM";
	keys->OpenROM.page = HOTKEY_PAGE_MAIN;
	keys->OpenROM.key = 'O';
	keys->OpenROM.modifiers = CUSTKEY_CTRL_MASK;*/

	keys->HardReset.handleKeyDown = HK_HardReset;
	keys->HardReset.code = "HardReset";
	keys->HardReset.name = L"Hard Reset";
	keys->HardReset.page = HOTKEY_PAGE_MAIN;
	keys->HardReset.key = 'P';
	keys->HardReset.modifiers = CUSTKEY_CTRL_MASK;

	keys->Pause.handleKeyDown = HK_Pause;
	keys->Pause.code = "Pause";
	keys->Pause.name = L"Pause";
	keys->Pause.page = HOTKEY_PAGE_MAIN;
	keys->Pause.key = VK_PAUSE;

	keys->FrameAdvance.handleKeyDown = HK_FrameAdvanceKeyDown;
	keys->FrameAdvance.handleKeyUp = HK_FrameAdvanceKeyUp;
	keys->FrameAdvance.code = "FrameAdvance";
	keys->FrameAdvance.name = L"Frame Advance";
	keys->FrameAdvance.page = HOTKEY_PAGE_MAIN;
	keys->FrameAdvance.key = 'N';

	keys->FastForward.handleKeyDown = HK_FastForwardKeyDown;
	keys->FastForward.handleKeyUp = HK_FastForwardKeyUp;
	keys->FastForward.code = "FastForward";
	keys->FastForward.name = L"Fast Forward";
	keys->FastForward.page = HOTKEY_PAGE_MAIN;
	keys->FastForward.key = VK_TAB;

	keys->FastForwardToggle.handleKeyDown = HK_FastForwardToggle;
	keys->FastForwardToggle.code = "FastForwardToggle";
	keys->FastForwardToggle.name = L"Fast Forward Toggle";
	keys->FastForwardToggle.page = HOTKEY_PAGE_MAIN;
	keys->FastForwardToggle.key = NULL;

	keys->IncreaseSpeed.handleKeyDown = HK_IncreaseSpeed;
	keys->IncreaseSpeed.code = "IncreaseSpeed";
	keys->IncreaseSpeed.name = L"Increase Speed";
	keys->IncreaseSpeed.page = HOTKEY_PAGE_MAIN;
	keys->IncreaseSpeed.key = VK_OEM_PLUS;

	keys->DecreaseSpeed.handleKeyDown = HK_DecreaseSpeed;
	keys->DecreaseSpeed.code = "DecreaseSpeed";
	keys->DecreaseSpeed.name = L"Decrease Speed";
	keys->DecreaseSpeed.page = HOTKEY_PAGE_MAIN;
	keys->DecreaseSpeed.key = VK_OEM_MINUS;
	

/*	keys->AutoHold.handleKeyDown = HK_AutoHoldKeyDown;
	keys->AutoHold.handleKeyUp = HK_AutoHoldKeyUp;
	keys->AutoHold.code = "AutoHold";
	keys->AutoHold.name = L"Auto-Hold";
	keys->AutoHold.page = HOTKEY_PAGE_MAIN;
	keys->AutoHold.key = NULL;

	keys->AutoHoldClear.handleKeyDown = HK_AutoHoldClearKeyDown;
	keys->AutoHoldClear.code = "AutoHoldClear";
	keys->AutoHoldClear.name = L"Auto-Hold Clear";
	keys->AutoHoldClear.page = HOTKEY_PAGE_MAIN;
	keys->AutoHoldClear.key = NULL;
*/

	keys->Screenshot.handleKeyDown = HK_Screenshot;
	keys->Screenshot.code = "Screenshot";
	keys->Screenshot.name = L"Screenshot";
	keys->Screenshot.page = HOTKEY_PAGE_MAIN;
	keys->Screenshot.key = VK_F12;

/*	keys->ToggleFrameCounter.handleKeyDown = HK_ToggleFrame;
	keys->ToggleFrameCounter.code = "ToggleFrameDisplay";
	keys->ToggleFrameCounter.name = L"Toggle Frame Display";
	keys->ToggleFrameCounter.page = HOTKEY_PAGE_MAIN;
	keys->ToggleFrameCounter.key = VK_OEM_PERIOD;

	keys->ToggleFPS.handleKeyDown = HK_ToggleFPS;
	keys->ToggleFPS.code = "ToggleFPSDisplay";
	keys->ToggleFPS.name = L"Toggle FPS Display";
	keys->ToggleFPS.page = HOTKEY_PAGE_MAIN;
	keys->ToggleFPS.key = NULL;

	keys->ToggleInput.handleKeyDown = HK_ToggleInput;
	keys->ToggleInput.code = "ToggleInputDisplay";
	keys->ToggleInput.name = L"Toggle Input Display";
	keys->ToggleInput.page = HOTKEY_PAGE_MAIN;
	keys->ToggleInput.key = VK_OEM_COMMA;

	keys->ToggleLag.handleKeyDown = HK_ToggleLag;
	keys->ToggleLag.code = "ToggleLagDisplay";
	keys->ToggleLag.name = L"Toggle Lag Display";
	keys->ToggleLag.page = HOTKEY_PAGE_MAIN;
	keys->ToggleLag.key = NULL;
*/
	keys->ToggleReadOnly.handleKeyDown = HK_ToggleReadOnly;
	keys->ToggleReadOnly.code = "ToggleReadOnly";
	keys->ToggleReadOnly.name = L"Toggle Read Only";
	keys->ToggleReadOnly.page = HOTKEY_PAGE_MOVIE;
	keys->ToggleReadOnly.key = (WORD)'NULL';

	keys->PlayMovie.handleKeyDown = HK_PlayMovie;
	keys->PlayMovie.code = "PlayMovie";
	keys->PlayMovie.name = L"Play Movie";
	keys->PlayMovie.page = HOTKEY_PAGE_MOVIE;
	keys->PlayMovie.key = NULL;

	keys->PlayMovieFromBeginning.handleKeyDown = HK_PlayMovieFromBeginning;
	keys->PlayMovieFromBeginning.code = "PlayMovieFromBeginning";
	keys->PlayMovieFromBeginning.name = L"Play From Beginning";
	keys->PlayMovieFromBeginning.page = HOTKEY_PAGE_MOVIE;
	keys->PlayMovieFromBeginning.key = NULL;

	keys->RecordMovie.handleKeyDown = HK_RecordMovie;
	keys->RecordMovie.code = "RecordMovie";
	keys->RecordMovie.name = L"Record Movie";
	keys->RecordMovie.page = HOTKEY_PAGE_MOVIE;
	keys->RecordMovie.key = (WORD)'NULL';
	keys->RecordMovie.modifiers = CUSTKEY_SHIFT_MASK;

	keys->StopMovie.handleKeyDown = HK_StopMovie;
	keys->StopMovie.code = "StopMovie";
	keys->StopMovie.name = L"Stop Movie";
	keys->StopMovie.page = HOTKEY_PAGE_MOVIE;
	keys->StopMovie.key = NULL;


	keys->RecordAVI.handleKeyDown = HK_RecordAVI;
	keys->RecordAVI.code = "RecordAVI";
	keys->RecordAVI.name = L"Record AVI";
	keys->RecordAVI.page = HOTKEY_PAGE_MAIN;
	keys->RecordAVI.key = NULL;

	keys->StopAVI.handleKeyDown = HK_StopAVI;
	keys->StopAVI.code = "StopAVI";
	keys->StopAVI.name = L"Stop AVI";
	keys->StopAVI.page = HOTKEY_PAGE_MAIN;
	keys->StopAVI.key = NULL;
/*
	//Turbo Page---------------------------------------
	keys->TurboRight.handleKeyDown = HK_TurboRightKeyDown;
	keys->TurboRight.handleKeyUp = HK_TurboRightKeyUp;
	keys->TurboRight.code = "TurboRight";
	keys->TurboRight.name = L"Turbo Right";
	keys->TurboRight.page = HOTKEY_PAGE_TURBO;
	keys->TurboRight.key = NULL;

	keys->TurboLeft.handleKeyDown = HK_TurboLeftKeyDown;
	keys->TurboLeft.handleKeyUp = HK_TurboLeftKeyUp;
	keys->TurboLeft.code = "TurboLeft";
	keys->TurboLeft.name = L"Turbo Left";
	keys->TurboLeft.page = HOTKEY_PAGE_TURBO;
	keys->TurboLeft.key = NULL;

	keys->TurboR.handleKeyDown = HK_TurboRKeyDown;
	keys->TurboR.handleKeyUp = HK_TurboRKeyUp;
	keys->TurboR.code = "TurboR";
	keys->TurboR.name = L"Turbo R";
	keys->TurboR.page = HOTKEY_PAGE_TURBO;
	keys->TurboR.key = NULL;

	keys->TurboL.handleKeyDown = HK_TurboLKeyDown;
	keys->TurboL.handleKeyUp = HK_TurboLKeyUp;
	keys->TurboL.code = "TurboL";
	keys->TurboL.name = L"Turbo L";
	keys->TurboL.page = HOTKEY_PAGE_TURBO;
	keys->TurboL.key = NULL;

	keys->TurboDown.handleKeyDown = HK_TurboDownKeyDown;
	keys->TurboDown.handleKeyUp = HK_TurboDownKeyUp;
	keys->TurboDown.code = "TurboDown";
	keys->TurboDown.name = L"Turbo Down";
	keys->TurboDown.page = HOTKEY_PAGE_TURBO;
	keys->TurboDown.key = NULL;

	keys->TurboUp.handleKeyDown = HK_TurboUpKeyDown;
	keys->TurboUp.handleKeyUp = HK_TurboUpKeyUp;
	keys->TurboUp.code = "TurboUp";
	keys->TurboUp.name = L"Turbo Up";
	keys->TurboUp.page = HOTKEY_PAGE_TURBO;
	keys->TurboUp.key = NULL;

	keys->TurboB.handleKeyDown = HK_TurboBKeyDown;
	keys->TurboB.handleKeyUp = HK_TurboBKeyUp;
	keys->TurboB.code = "TurboB";
	keys->TurboB.name = L"Turbo B";
	keys->TurboB.page = HOTKEY_PAGE_TURBO;
	keys->TurboB.key = NULL;

	keys->TurboA.handleKeyDown = HK_TurboAKeyDown;
	keys->TurboA.handleKeyUp = HK_TurboAKeyUp;
	keys->TurboA.code = "TurboA";
	keys->TurboA.name = L"Turbo A";
	keys->TurboA.page = HOTKEY_PAGE_TURBO;
	keys->TurboA.key = NULL;

	keys->TurboX.handleKeyDown = HK_TurboXKeyDown;
	keys->TurboX.handleKeyUp = HK_TurboXKeyUp;
	keys->TurboX.code = "TurboX";
	keys->TurboX.name = L"Turbo X";
	keys->TurboX.page = HOTKEY_PAGE_TURBO;
	keys->TurboX.key = NULL;

	keys->TurboY.handleKeyDown = HK_TurboYKeyDown;
	keys->TurboY.handleKeyUp = HK_TurboYKeyUp;
	keys->TurboY.code = "TurboY";
	keys->TurboY.name = L"Turbo Y";
	keys->TurboY.page = HOTKEY_PAGE_TURBO;
	keys->TurboY.key = NULL;

	keys->TurboSelect.handleKeyDown = HK_TurboSelectKeyDown;
	keys->TurboSelect.handleKeyUp = HK_TurboSelectKeyUp;
	keys->TurboSelect.code = "TurboSelect";
	keys->TurboSelect.name = L"Turbo Select";
	keys->TurboSelect.page = HOTKEY_PAGE_TURBO;
	keys->TurboSelect.key = NULL;

	keys->TurboStart.handleKeyDown = HK_TurboStartKeyDown;
	keys->TurboStart.handleKeyUp = HK_TurboStartKeyUp;
	keys->TurboStart.code = "TurboStart";
	keys->TurboStart.name = L"Turbo Start";
	keys->TurboStart.page = HOTKEY_PAGE_TURBO;
	keys->TurboStart.key = NULL;
*/

	keys->ToggleOSD.handleKeyDown = HK_ToggleOSD;
	keys->ToggleOSD.code = "ToggleOSD";
	keys->ToggleOSD.name = L"Toggle OSD";
	keys->ToggleOSD.page = HOTKEY_PAGE_MAIN;
	keys->ToggleOSD.key = NULL;

	keys->ToggleNBG0.handleKeyDown = HK_ToggleNBG0;
	keys->ToggleNBG0.code = "ToggleNBG0";
	keys->ToggleNBG0.name = L"Toggle Layer 0";
	keys->ToggleNBG0.page = HOTKEY_PAGE_MAIN;
	keys->ToggleNBG0.key = NULL;

	keys->ToggleNBG1.handleKeyDown = HK_ToggleNBG1;
	keys->ToggleNBG1.code = "ToggleNBG1";
	keys->ToggleNBG1.name = L"Toggle Layer 1";
	keys->ToggleNBG1.page = HOTKEY_PAGE_MAIN;
	keys->ToggleNBG1.key = NULL;

	keys->ToggleNBG2.handleKeyDown = HK_ToggleNBG2;
	keys->ToggleNBG2.code = "ToggleNBG2";
	keys->ToggleNBG2.name = L"Toggle Layer 2";
	keys->ToggleNBG2.page = HOTKEY_PAGE_MAIN;
	keys->ToggleNBG2.key = NULL;

	keys->ToggleNBG3.handleKeyDown = HK_ToggleNBG3;
	keys->ToggleNBG3.code = "ToggleNBG3";
	keys->ToggleNBG3.name = L"Toggle Layer 3";
	keys->ToggleNBG3.page = HOTKEY_PAGE_MAIN;
	keys->ToggleNBG3.key = NULL;
/*
	keys->ToggleRBG0.handleKeyDown = HK_ToggleRBG0;
	keys->ToggleRBG0.code = "ToggleRBG0";
	keys->ToggleRBG0.name = L"Toggle RBG0";
	keys->ToggleRBG0.page = HOTKEY_PAGE_MAIN;
	keys->ToggleRBG0.key = NULL;

	keys->ToggleVDP1.handleKeyDown = HK_ToggleVDP1;
	keys->ToggleVDP1.code = "ToggleVDP1";
	keys->ToggleVDP1.name = L"Toggle VDP1";
	keys->ToggleVDP1.page = HOTKEY_PAGE_MAIN;
	keys->ToggleVDP1.key = NULL;
*/
	keys->NextSaveSlot.handleKeyDown = HK_NextSaveSlot;
	keys->NextSaveSlot.code = "NextSaveSlot";
	keys->NextSaveSlot.name = L"Next Save Slot";
	keys->NextSaveSlot.page = HOTKEY_PAGE_STATE_SLOTS;
	keys->NextSaveSlot.key = NULL;

	keys->PreviousSaveSlot.handleKeyDown = HK_PreviousSaveSlot;
	keys->PreviousSaveSlot.code = "PreviousSaveSlot";
	keys->PreviousSaveSlot.name = L"Previous Save Slot";
	keys->PreviousSaveSlot.page = HOTKEY_PAGE_STATE_SLOTS;
	keys->PreviousSaveSlot.key = NULL;

	keys->ToggleFullScreen.handleKeyDown = HK_ToggleFullScreen;
	keys->ToggleFullScreen.code = "ToggleFullScreen";
	keys->ToggleFullScreen.name = L"Toggle Full Screen";
	keys->ToggleFullScreen.page = HOTKEY_PAGE_MAIN;
	keys->ToggleFullScreen.key = NULL;
	
	keys->QuickSave.handleKeyDown = HK_StateQuickSaveSlot;
	keys->QuickSave.code = "QuickSave";
	keys->QuickSave.name = L"Quick Save";
	keys->QuickSave.page = HOTKEY_PAGE_STATE_SLOTS;
	keys->QuickSave.key = 'I';

	keys->QuickLoad.handleKeyDown = HK_StateQuickLoadSlot;
	keys->QuickLoad.code = "QuickLoad";
	keys->QuickLoad.name = L"Quick Load";
	keys->QuickLoad.page = HOTKEY_PAGE_STATE_SLOTS;
	keys->QuickLoad.key = 'P';

	for(int i=0;i<10;i++) {
		static const char* saveNames[] = {"SaveToSlot0","SaveToSlot1","SaveToSlot2","SaveToSlot3","SaveToSlot4","SaveToSlot5","SaveToSlot6","SaveToSlot7","SaveToSlot8","SaveToSlot9"};
		static const char* loadNames[] = {"LoadFromSlot0","LoadFromSlot1","LoadFromSlot2","LoadFromSlot3","LoadFromSlot4","LoadFromSlot5","LoadFromSlot6","LoadFromSlot7","LoadFromSlot8","LoadFromSlot9"};
		static const char* slotNames[] = {"SelectSlot0","SelectSlot1","SelectSlot2","SelectSlot3","SelectSlot4","SelectSlot5","SelectSlot6","SelectSlot7","SelectSlot8","SelectSlot9"};

		WORD key = VK_F1 + i - 1;
		if(i==0) key = VK_F10;

		SCustomKey & save = keys->Save[i];
		save.handleKeyDown = HK_StateSaveSlot;
		save.param = i;
		save.page = HOTKEY_PAGE_STATE;
		wchar_t tmp[16];
		_itow(i,tmp,10);
		save.name = (std::wstring)L"Save To Slot " + (std::wstring)tmp;
		save.code = saveNames[i];
		save.key = key;
		save.modifiers = CUSTKEY_SHIFT_MASK;

		SCustomKey & load = keys->Load[i];
		load.handleKeyDown = HK_StateLoadSlot;
		load.param = i;
		load.page = HOTKEY_PAGE_STATE;
		_itow(i,tmp,10);
		load.name = (std::wstring)L"Load from Slot " + (std::wstring)tmp;
		load.code = loadNames[i];
		load.key = key;

		key = '0' + i;

		SCustomKey & slot = keys->Slot[i];
		slot.handleKeyDown = HK_StateSetSlot;
		slot.param = i;
		slot.page = HOTKEY_PAGE_STATE_SLOTS;
		_itow(i,tmp,10);
		slot.name = (std::wstring)L"Select Save Slot " + (std::wstring)tmp;
		slot.code = slotNames[i];
		slot.key = key;
	}
}


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

