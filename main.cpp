#define STRICT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include "d3d.h"
#include "dd.h"
#include "resource.h"
#include "stdio.h"
#include "types.h"
#include "movie.h"
#include <ddraw.h>

#include "hotkey.h"

#include "CommDlg.h"

#include "mednafen.h"
#include "general.h"

#include "CWindow.h"
#include "memView.h"
#include "ramwatch.h"
#include "ramsearch.h"
#include "Mmsystem.h"
#include "sound.h"
#include "aviout.h"
#include "waveout.h"
#include "video.h"
#include "recentroms.h"

#include "aggdraw.h"
#include "GPU_osd.h"

#include "replay.h"
#include "pcejin.h"
//#include "svnrev.h"
#include "xstring.h"
#include "lua-engine.h"

#include "shellapi.h"

#include "recentroms.h"
#include "ParseCmdLine.h"
#include "vb.h"

std::string GameName;


LARGE_INTEGER       _animationInterval;

volatile MDFN_Surface *VTBuffer[2] = { NULL, NULL };
MDFN_Rect *VTLineWidths[2] = { NULL, NULL };
volatile int VTBackBuffer = 0;

bool MixVideoOutput = false;
int DisplayLeftRightOutput = 0;
int SideBySidePixels = 16;

bool OpenConsoleWindow = true;

//uint16 PadData;//NEWTODO this sucks

Pcejin pcejin;

SoundDriver * soundDriver = 0;

EmulateSpecStruct espec;

uint8 convert_buffer[1024*768*3];

LRESULT CALLBACK WndProc( HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK LuaScriptProc(HWND, UINT, WPARAM, LPARAM);
std::vector<HWND> LuaScriptHWnds;
BOOL Register( HINSTANCE hInst );
HWND Create( int nCmdShow, int w, int h );

//adelikat: Bleh, for now
extern const unsigned int MAX_RECENT_ROMS = 10;	//To change the recent rom max, simply change this number
extern const unsigned int clearid = IDM_RECENT_RESERVED0;			// ID for the Clear recent ROMs item
extern const unsigned int baseid = IDM_RECENT_RESERVED1;			//Base identifier for the recent ROMs items


// Prototypes
std::string RemovePath(std::string filename);

// Message handlers
void OnDestroy(HWND hwnd);
void OnCommand(HWND hWnd, int iID, HWND hwndCtl, UINT uNotifyCode);
void OnPaint(HWND hwnd);

// Globals
int WndX = 0;	//Window position
int WndY = 0;

static char g_szAppName[] = "DDSamp";
HWND g_hWnd;
HINSTANCE g_hInstance;
bool g_bRunning;
void emulate();
void initialize();
void OpenConsole();
void LoadIniSettings();
void SaveIniSettings();
DWORD hKeyInputTimer;
int KeyInDelayMSec = 0;
int KeyInRepeatMSec = 16;

double interval =  0.0166666666666667;//60 fps
WNDCLASSEX winClass;
int WINAPI WinMain( HINSTANCE hInstance,
				   HINSTANCE hPrevInstance,
				   LPSTR lpCmdLine,
				   int nCmdShow )
{
	MSG uMsg;
	 
	LARGE_INTEGER nLast;
	LARGE_INTEGER nNow;
	LARGE_INTEGER nFreq;
	QueryPerformanceFrequency(&nFreq); 
	QueryPerformanceCounter(&nLast);
	_animationInterval.QuadPart = (LONGLONG)(interval * nFreq.QuadPart);

	memset(&uMsg,0,sizeof(uMsg));

	winClass.lpszClassName = "MY_WINDOWS_CLASS";
	winClass.cbSize = sizeof(WNDCLASSEX);
	winClass.style = CS_HREDRAW | CS_VREDRAW;
	winClass.lpfnWndProc = WndProc;
	winClass.hInstance = hInstance;
	winClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	winClass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	winClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	winClass.lpszMenuName = MAKEINTRESOURCE(IDC_CV);
	winClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	winClass.cbClsExtra = 0;
	winClass.cbWndExtra = 0;

	if( !RegisterClassEx(&winClass) )
		return E_FAIL;

	GetINIPath();

	OpenConsoleWindow = GetPrivateProfileBool("Display", "OpenConsoleWindow", false, IniName);

	if (OpenConsoleWindow)
		OpenConsole();

	pcejin.aspectRatio = GetPrivateProfileBool("Video", "aspectratio", false, IniName);
	pcejin.windowSize = GetPrivateProfileInt("Video", "pcejin.windowSize", 1, IniName);
	
	WndX = GetPrivateProfileInt("Main", "WndX", 0, IniName);
	WndY = GetPrivateProfileInt("Main", "WndY", 0, IniName);
	
	g_hWnd = CreateWindowEx( NULL, "MY_WINDOWS_CLASS",
		pcejin.versionName.c_str(),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		WndX, WndY, 348, 224, NULL, NULL, hInstance, NULL );

	if( g_hWnd == NULL )
		return E_FAIL;

	ScaleScreen((float)pcejin.windowSize);

	soundInit();

#if USE_D3D
	OculusInit();
#endif // #if USE_D3D

	LoadIniSettings();
	InitSpeedThrottle();

#if USE_D3D
	D3DInit();
#endif // #if USE_D3D

#if USE_DDRAW
	DirectDrawInit();
#endif // #if USE_DDRAW

	InitCustomControls();
	InitCustomKeys(&CustomKeys);
	LoadHotkeyConfig();
	LoadInputConfig();

	DragAcceptFiles(g_hWnd, true);

	extern void Agg_init();
	Agg_init();

	if (osd)  {delete osd; osd =NULL; }
	osd  = new OSDCLASS(-1);

	di_init();

	DWORD wmTimerRes;
	TIMECAPS tc;
	if (timeGetDevCaps(&tc, sizeof(TIMECAPS))== TIMERR_NOERROR)
	{
		wmTimerRes = std::min(std::max(tc.wPeriodMin, (UINT)1), tc.wPeriodMax);
		timeBeginPeriod (wmTimerRes);
	}
	else
	{
		wmTimerRes = 5;
		timeBeginPeriod (wmTimerRes);
	}

	if (KeyInDelayMSec == 0) {
		DWORD dwKeyboardDelay;
		SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &dwKeyboardDelay, 0);
		KeyInDelayMSec = 250 * (dwKeyboardDelay + 1);
	}
	if (KeyInRepeatMSec == 0) {
		DWORD dwKeyboardSpeed;
		SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &dwKeyboardSpeed, 0);
		KeyInRepeatMSec = (int)(1000.0/(((30.0-2.5)/31.0)*dwKeyboardSpeed+2.5));
	}
	if (KeyInRepeatMSec < (int)wmTimerRes)
		KeyInRepeatMSec = (int)wmTimerRes;
	if (KeyInDelayMSec < KeyInRepeatMSec)
		KeyInDelayMSec = KeyInRepeatMSec;

	hKeyInputTimer = timeSetEvent (KeyInRepeatMSec, 0, KeyInputTimer, 0, TIME_PERIODIC);

	ShowWindow( g_hWnd, nCmdShow );
	UpdateWindow( g_hWnd );

	initialize();
	
	if (lpCmdLine[0])ParseCmdLine(lpCmdLine, g_hWnd);

	while( uMsg.message != WM_QUIT )
	{
		if( PeekMessage( &uMsg, NULL, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &uMsg );
			DispatchMessage( &uMsg );
		}
		else {
			// lock fps @ 60fps
			//QueryPerformanceCounter(&nNow);
			//if (nNow.QuadPart - nLast.QuadPart > _animationInterval.QuadPart)
			//{
			//	nLast.QuadPart = nNow.QuadPart - (nNow.QuadPart % _animationInterval.QuadPart);
				emulate();
				render();
			//}
			//else
			/*{
				Sleep(1);
			}*/

			
		}
		if(!pcejin.started)
			Sleep(1);
	}

	// shutDown();

	timeEndPeriod (wmTimerRes);

	CloseAllToolWindows();

	UnregisterClass( "MY_WINDOWS_CLASS", winClass.hInstance );
	
	return uMsg.wParam;
}

#include <fcntl.h>
#include <io.h>
HANDLE hConsole;
void OpenConsole()
{
	//CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

	if (hConsole) return;
	AllocConsole();

	//redirect stdio
	long lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
	int hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	FILE *fp = _fdopen( hConHandle, "w" );
	*stdout = *fp;
}

/*HWND Create( int nCmdShow, int w, int h )
{
	RECT rc;

	// Calculate size of window based on desired client window size
	rc.left = 0;
	rc.top = 0;
	rc.right = w;
	rc.bottom = h;
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );

	HWND hwnd = CreateWindow(g_szAppName, g_szAppName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right-rc.left, rc.bottom-rc.top,
		NULL, NULL, g_hInstance, NULL);

	if (hwnd == NULL)
		return hwnd;

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	OpenConsole();

	return hwnd;
}*/

extern MDFNGI EmulatedVB;

static int16 *EmuModBuffer = NULL;
static int32 EmuModBufferSize = 0;	// In frames.

void sndinit() {
	EmuModBufferSize = (500 * 44100 + 999) / 1000;//format.rate
	EmuModBuffer = (int16 *)calloc(sizeof(int16) * 2, EmuModBufferSize);//format.channels
}
void vbjinInit() {

	return;
	MDFNGameInfo = &EmulatedVB;

	MDFNFILE* fp = new MDFNFILE();
	fp->Open("C:\\wario.vb",NULL);
	MDFNGameInfo->Load(NULL,fp);

//	initialized = true;

	//	extern void VBINPUT_SetInput(int port, const char *type, void *ptr);

	const char * typez;
	//const char * padData;
//	padData = malloc(8);
	typez = (const char*)malloc(16);
	MDFNGameInfo->SetInput(1,typez,&pcejin.pads[0]);
//	MDFNGameInfo->SetInput(1,typez,adData);

	sndinit();

	MDFNGameInfo->SetSoundRate(44100);



	MDFNGI *CurGame=NULL;

	CurGame = MDFNGameInfo;
	VTBuffer[0] = new MDFN_Surface(NULL, CurGame->pitch / sizeof(uint32), CurGame->fb_height, CurGame->pitch / sizeof(uint32), MDFN_COLORSPACE_RGB, 0, 8, 16, 24);
	VTBuffer[1] = new MDFN_Surface(NULL, CurGame->pitch / sizeof(uint32), CurGame->fb_height, CurGame->pitch / sizeof(uint32), MDFN_COLORSPACE_RGB, 0, 8, 16, 24);
	VTLineWidths[0] = (MDFN_Rect *)calloc(CurGame->fb_height, sizeof(MDFN_Rect));
	VTLineWidths[1] = (MDFN_Rect *)calloc(CurGame->fb_height, sizeof(MDFN_Rect));
}


void MDFNI_Emulate(EmulateSpecStruct *espec) {
	MDFNGameInfo->Emulate(espec);
}
EmulateSpecStruct vbjinEmulate() {

//	EmulateSpecStruct espec;

	memset(&espec, 0, sizeof(EmulateSpecStruct));
	espec.VideoFormatChanged = true;

	espec.surface = (MDFN_Surface *)VTBuffer[VTBackBuffer];
	espec.LineWidths = (MDFN_Rect *)VTLineWidths[VTBackBuffer];
	espec.skip = 0;

	espec.soundmultiplier = 1;
	espec.SoundVolume = 1;
	espec.NeedRewind = false;
	////

	espec.SoundBuf = EmuModBuffer;
	espec.SoundBufMaxSize = EmuModBufferSize;
	espec.SoundVolume = 1;//(double)MDFN_GetSettingUI("soundvol") / 100;

	static double average_time = 0;

	/*
	int color =5555555;
	int h = 224;

	for(uint32 i = 0; i < 320 * h; i++)
	{
		//	VTBuffer[VTBackBuffer]->pixels[i] = color;
	} */

//frames++;

//PadData = setPad();

	MDFNI_Emulate(&espec);


	return espec;
}

void UpdateTitleWithFilename(std::string filename)
{
	std::string temp = pcejin.versionName;
	temp.append(" ");
	temp.append(RemovePath(filename));

	SetWindowText(g_hWnd, temp.c_str());
}

void LoadGame(){

	char szChoice[MAX_PATH]={0};
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hWnd;
	ofn.lpstrFilter = "VB Files (*.vb, *.zip)\0*.vb;*.zip\0All files(*.*)\0*.*\0\0";
	ofn.lpstrFile = (LPSTR)szChoice;
	ofn.lpstrTitle = "Select a file to open";
	ofn.lpstrDefExt = "vb";
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	if(GetOpenFileName(&ofn)) {
		pcejin.romLoaded = true;
		pcejin.started = true;
	
		if(!MDFNI_LoadGame(NULL,szChoice)) {
			pcejin.started = false;
			pcejin.romLoaded = false;
			
		}
		if (AutoRWLoad)
		{
			//Open Ram Watch if its auto-load setting is checked
			OpenRWRecentFile(0);
			RamWatchHWnd = CreateDialog(winClass.hInstance, MAKEINTRESOURCE(IDD_RAMWATCH), g_hWnd, (DLGPROC) RamWatchProc);
		}
		FCEUI_StopMovie();
		UpdateRecentRoms(szChoice);
		ResetFrameCount();
		UpdateTitleWithFilename(szChoice);
		std::string romname = noExtension(RemovePath(szChoice));
		std::string temp = pcejin.versionName;
		temp.append(" ");
		temp.append(romname);
		
		SetWindowText(g_hWnd, temp.c_str());
	}
}

void RecordAvi()
{

	char szChoice[MAX_PATH]={0};
	std::string fname;
	//int x;
	std::wstring la = L"";
	OPENFILENAME ofn;

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hWnd;
	ofn.lpstrFilter = "Avi file (*.avi)\0*.avi\0All files(*.*)\0*.*\0\0";
	ofn.lpstrFile = (LPSTR)szChoice;
	ofn.lpstrTitle = "Avi Thingy";
	ofn.lpstrDefExt = "avi";
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    if(GetSaveFileName(&ofn))
		DRV_AviBegin(szChoice);
}

void UpdateToolWindows()
{
	Update_RAM_Search();	//Update_RAM_Watch() is also called; hotkey.cpp - HK_StateLoadSlot & State_Load also call these functions

	RefreshAllToolWindows();
}

void StopAvi()
{
	DRV_AviEnd();
}

/// Shows a Open File dialog and starts logging sound.
/// @return Flag that indicates failure (0) or success (1).
bool CreateSoundSave()
{
	const char filter[]="MS WAVE (*.wav)\0*.wav\0All Files (*.*)\0*.*\0\0";
	char nameo[2048];
	OPENFILENAME ofn;

	DRV_EndWaveRecord();

	memset(&ofn,0,sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hWnd;
	ofn.lpstrTitle = "Log Sound As...";
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = nameo;
	ofn.lpstrDefExt = "wav";
	ofn.nMaxFile = 256;
	ofn.Flags = OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;

	if(GetSaveFileName(&ofn))
		return DRV_BeginWaveRecord(nameo);

	return false;
}

void CloseWave()
{
	DRV_EndWaveRecord();
}

DWORD checkMenu(UINT idd, bool check)
{
	return CheckMenuItem(GetMenu(g_hWnd), idd, MF_BYCOMMAND | (check?MF_CHECKED:MF_UNCHECKED));
}

void LoadIniSettings(){
	MixVideoOutput = GetPrivateProfileBool("Display","MixLeftRight", false, IniName);
	MDFN_IEN_VB::SetSplitMode(GetPrivateProfileInt("Display","SplitMode", 0, IniName));
	MDFN_IEN_VB::SetColorMode(GetPrivateProfileInt("Display","ColorMode", 0, IniName));
	SideBySidePixels = GetPrivateProfileInt("Display","SideBySidePixels", 16, IniName);
	if (abs(SideBySidePixels) > 96)
		SideBySidePixels = 96;
	MDFN_IEN_VB::SetSideBySidePixels(abs(SideBySidePixels));
	MDFN_IEN_VB::SetMixVideoOutput(MixVideoOutput);
	DisplayLeftRightOutput = GetPrivateProfileInt("Display","ViewDisplay", 0, IniName);
	if (DisplayLeftRightOutput > 3)
		DisplayLeftRightOutput = 3;
	MDFN_IEN_VB::SetViewDisp(DisplayLeftRightOutput);
	Hud.FrameCounterDisplay = GetPrivateProfileBool("Display","FrameCounter", false, IniName);
	Hud.ShowInputDisplay = GetPrivateProfileBool("Display","Display Input", false, IniName);
	Hud.ShowLagFrameCounter = GetPrivateProfileBool("Display","Display Lag Counter", false, IniName);
	Hud.DisplayStateSlots = GetPrivateProfileBool("Display","Display State Slots", false, IniName);
	soundDriver->userMute = (GetPrivateProfileBool("Main", "Muted", false, IniName));
	if(soundDriver->userMute)
		soundDriver->mute();

	//RamWatch Settings
	AutoRWLoad = GetPrivateProfileBool("RamWatch", "AutoRWLoad", 0, IniName);
	RWSaveWindowPos = GetPrivateProfileBool("RamWatch", "SaveWindowPos", 0, IniName);
	ramw_x = GetPrivateProfileInt("RamWatch", "WindowX", 0, IniName);
	ramw_y = GetPrivateProfileInt("RamWatch", "WindowY", 0, IniName);
	
	for(int i = 0; i < MAX_RECENT_WATCHES; i++)
	{
		char str[256];
		sprintf(str, "Recent Watch %d", i+1);
		GetPrivateProfileString("Watches", str, "", &rw_recent_files[i][0], 1024, IniName);
	}
}

void SaveIniSettings(){

	WritePrivateProfileInt("Video", "aspectratio", pcejin.aspectRatio, IniName);
	WritePrivateProfileInt("Video", "pcejin.windowSize", pcejin.windowSize, IniName);
	WritePrivateProfileInt("Main", "Muted", soundDriver->userMute, IniName);
	WritePrivateProfileInt("Main", "WndX", WndX, IniName);
	WritePrivateProfileInt("Main", "WndY", WndY, IniName);

	//RamWatch Settings
	WritePrivateProfileBool("RamWatch", "AutoRWLoad", AutoRWLoad, IniName);
	WritePrivateProfileBool("RamWatch", "SaveWindowPos", RWSaveWindowPos, IniName);
	WritePrivateProfileInt("RamWatch", "WindowX", ramw_x, IniName);
	WritePrivateProfileInt("RamWatch", "WindowY", ramw_y, IniName);

	for(int i = 0; i < MAX_RECENT_WATCHES; i++)
		{
			char str[256];
			sprintf(str, "Recent Watch %d", i+1);
			WritePrivateProfileString("Watches", str, &rw_recent_files[i][0], IniName);	
		}
	SaveRecentRoms();

}

std::string a = "a";

void RecordMovie(HWND hWnd){
	char szChoice[MAX_PATH]={0};
	soundDriver->pause();
	OPENFILENAME ofn;

	// browse button
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = "Movie File (*.mc2)\0*.mc2\0All files(*.*)\0*.*\0\0";
	ofn.lpstrFile = (LPSTR)szChoice;
	ofn.lpstrTitle = "Record a new movie";
	ofn.lpstrDefExt = "mc2";
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

	if(GetSaveFileName(&ofn))
		FCEUI_SaveMovie(szChoice, a, 1);

	//If user did not specify an extension, add .mc2 for them
	// fname = szChoice;
	// x = fname.find_last_of(".");
	// if (x < 0)
	// fname.append(".mc2");

	// SetDlgItemText(hwndDlg, IDC_EDIT_FILENAME, fname.c_str());
	//if(GetSaveFileName(&ofn))
	// UpdateRecordDialogPath(hwndDlg,szChoice);

	pcejin.tempUnPause();

}

void PlayMovie(HWND hWnd){
	char szChoice[MAX_PATH]={0};

	OPENFILENAME ofn;

	soundDriver->pause();

	// browse button
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = "Movie File (*.mc2, *.mcm)\0*.mc2;*.mcm\0All files(*.*)\0*.*\0\0";
	ofn.lpstrFile = (LPSTR)szChoice;
	ofn.lpstrTitle = "Play a movie";
	ofn.lpstrDefExt = "mc2";
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	if(GetOpenFileName(&ofn)) {

		if(toupper(strright(szChoice,4)) == ".MC2")
			FCEUI_LoadMovie(szChoice, 1, 0, 0);
		else if(toupper(strright(szChoice,4)) == ".MCM")
			LoadMCM(szChoice, true);
	}
	
	pcejin.tempUnPause();

}

void setClientSize(int width, int height)
{
	int xborder, yborder;
	int ymenu, ymenunew;
	int ycaption;

	MENUBARINFO mbi;

	RECT wndRect;
	int finalx, finaly;

	/* Get the size of the border */
	xborder = GetSystemMetrics(SM_CXSIZEFRAME);
	yborder = GetSystemMetrics(SM_CYSIZEFRAME);

	/* Get the size of the menu bar */
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(g_hWnd, OBJID_MENU, 0, &mbi);
	ymenu = (mbi.rcBar.bottom - mbi.rcBar.top + 1);

	/* Get the size of the caption bar */
	ycaption = GetSystemMetrics(SM_CYCAPTION);

	/* Finally, resize the window */
	GetWindowRect(g_hWnd, &wndRect);
	finalx = (xborder + width + xborder);
	finaly = (ycaption + yborder + ymenu + height + yborder);
	MoveWindow(g_hWnd, wndRect.left, wndRect.top, finalx, finaly, TRUE);

	/* Oops, we also need to check if the height */
	/* of the menu bar has changed after the resize */
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(g_hWnd, OBJID_MENU, 0, &mbi);
	ymenunew = (mbi.rcBar.bottom - mbi.rcBar.top + 1);

	if(ymenunew != ymenu)
		MoveWindow(g_hWnd, wndRect.left, wndRect.top, finalx, (finaly + (ymenunew - ymenu)), TRUE);
}

void ScaleScreen(float factor)
{
	if(pcejin.windowSize == 0) {
		if(pcejin.aspectRatio)
			pcejin.width = 309;

		setClientSize(pcejin.width, pcejin.height);
	}
	else
	{
		if(factor==65535)
			factor = 1.5f;
		else if(factor==65534)
			factor = 2.5f;

		setClientSize((int)(pcejin.width * factor * (pcejin.aspectRatio ? ((float)309 / (float)384) : 1)), (int)(pcejin.height * factor));
	}

}

LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	switch(Message)
	{
	case WM_INITMENU:
		recentromsmenu = LoadMenu(g_hInstance, "RECENTROMS");
		GetRecentRoms();
		break;
	case WM_KEYDOWN:
		if(wParam != VK_PAUSE)
			break;
		// case WM_SYSKEYDOWN:
	case WM_CUSTKEYDOWN:
		{
			int modifiers = GetModifiers(wParam);
			if(!HandleKeyMessage(wParam,lParam, modifiers))
				return 0;
			break;
		}

	case WM_KEYUP:
		debug_callback(wParam);
		if(wParam != VK_PAUSE)
			break;
	case WM_SYSKEYUP:
	case WM_CUSTKEYUP:
		{
			int modifiers = GetModifiers(wParam);
			HandleKeyUp(wParam, lParam, modifiers);
		}
		break;
	case WM_SIZE:
		switch(wParam)
		{
		case SIZE_MINIMIZED:
			break;
		case SIZE_MAXIMIZED:
			pcejin.maximized = true;
			break;
		case SIZE_RESTORED:
			pcejin.maximized = false;
			break;
		default:
			break;
		}
		return 0;
	case WM_MOVE:
		RECT rect;
		GetWindowRect(hWnd,&rect);
		WndX = rect.left;
		WndY = rect.top;
		return 0;
	case WM_DROPFILES:
		{
			char filename[MAX_PATH] = "";
			DragQueryFile((HDROP)wParam,0,filename,MAX_PATH);
			DragFinish((HDROP)wParam);
			
			std::string fileDropped = filename;
			//-------------------------------------------------------
			//Check if Movie file
			//-------------------------------------------------------
			if (!(fileDropped.find(".mc2") == std::string::npos) && (fileDropped.find(".mc2") == fileDropped.length()-4))
			{
				if (!pcejin.romLoaded)	//If no ROM is loaded, prompt for one
				{
					soundDriver->pause();
					LoadGame();
					pcejin.tempUnPause();
				}	
				if (pcejin.romLoaded && !(fileDropped.find(".mc2") == std::string::npos))	
					FCEUI_LoadMovie(fileDropped.c_str(), 1, false, false);		 
			}
			
			//-------------------------------------------------------
			//Check if Savestate file
			//-------------------------------------------------------
			else if (!(fileDropped.find(".mc") == std::string::npos))	//Note: potential clash, mc2 will be loaded a movie file first
			{
				if (fileDropped.find(".mc") == fileDropped.length()-4)
				{
					if ((fileDropped[fileDropped.length()-1] >= '0' && fileDropped[fileDropped.length()-1] <= '9'))
					{
						extern int MDFNSS_Load(const char *fname, const char *suffix);
						MDFNSS_Load(filename, NULL);
#if USE_DDRAW
						ClearDirectDrawOutput();
#endif // #if USE_DDRAW
						UpdateToolWindows();
					}
				}
			}
			
			//-------------------------------------------------------
			//Check if Lua script file
			//-------------------------------------------------------
			else if (!(fileDropped.find(".lua") == std::string::npos) && (fileDropped.find(".lua") == fileDropped.length()-4))	 //ROM is already loaded and .dsm in filename
			{
				if(LuaScriptHWnds.size() < 16)
				{
					char temp [1024];
					strcpy(temp, fileDropped.c_str());
					HWND IsScriptFileOpen(const char* Path);
					if(!IsScriptFileOpen(temp))
					{
						HWND hDlg = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_LUA), hWnd, (DLGPROC) LuaScriptProc);
						SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,0,(LPARAM)temp);
					}
				}
			}
			
			//-------------------------------------------------------
			//Check if watchlist file
			//-------------------------------------------------------
			else if (!(fileDropped.find(".wch") == std::string::npos) && (fileDropped.find(".wch") == fileDropped.length()-4))	 //ROM is already loaded and .dsm in filename
			{
				if(!RamWatchHWnd)
				{
					RamWatchHWnd = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_RAMWATCH), hWnd, (DLGPROC) RamWatchProc);
				}
				else
					SetForegroundWindow(RamWatchHWnd);
				Load_Watches(true, fileDropped.c_str());
			}
			
			//-------------------------------------------------------
			//Else load it as a ROM
			//-------------------------------------------------------
			else if(MDFNI_LoadGame(NULL,filename))
			{
				pcejin.romLoaded = true;
				pcejin.started = true;
				//TODO: adelikat: This code is copied directly from the LoadGame() function, it should be come a separate function and called in both places
				////////////////////////////////
				if (AutoRWLoad)
				{
					//Open Ram Watch if its auto-load setting is checked
					OpenRWRecentFile(0);
					RamWatchHWnd = CreateDialog(winClass.hInstance, MAKEINTRESOURCE(IDD_RAMWATCH), g_hWnd, (DLGPROC) RamWatchProc);
				}
				UpdateRecentRoms(filename);
				////////////////////////////////
			}
		}
		return 0;
	case WM_ENTERMENULOOP:
		soundDriver->pause();
		EnableMenuItem(GetMenu(hWnd), IDM_RECORD_MOVIE, MF_BYCOMMAND | (movieMode == MOVIEMODE_INACTIVE && pcejin.romLoaded) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), ID_RAM_WATCH, MF_BYCOMMAND | (movieMode == MOVIEMODE_INACTIVE && pcejin.romLoaded) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), ID_RAM_SEARCH, MF_BYCOMMAND | (movieMode == MOVIEMODE_INACTIVE && pcejin.romLoaded) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_MEMORY, MF_BYCOMMAND | (movieMode == MOVIEMODE_INACTIVE && pcejin.romLoaded) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_PLAY_MOVIE, MF_BYCOMMAND | (movieMode == MOVIEMODE_INACTIVE && pcejin.romLoaded) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_STOPMOVIE, MF_BYCOMMAND | (movieMode != MOVIEMODE_INACTIVE) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_FILE_STOPAVI, MF_BYCOMMAND | (DRV_AviIsRecording()) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_FILE_RECORDAVI, MF_BYCOMMAND | (!DRV_AviIsRecording()) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_FILE_STOPWAV, MF_BYCOMMAND | (DRV_WaveRecordActive()) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_FILE_RECORDWAV, MF_BYCOMMAND | (!DRV_WaveRecordActive()) ? MF_ENABLED : MF_GRAYED);

		//Window Size
		checkMenu(IDC_WINDOW1X,  ((pcejin.windowSize==1)));
		checkMenu(IDC_WINDOW15X, ((pcejin.windowSize==65535)));
		checkMenu(IDC_WINDOW2X,  ((pcejin.windowSize==2)));
		checkMenu(IDC_WINDOW25X, ((pcejin.windowSize==65534)));
		checkMenu(IDC_WINDOW3X,  ((pcejin.windowSize==3)));
		checkMenu(IDC_WINDOW4X,  ((pcejin.windowSize==4)));
		checkMenu(IDC_ASPECT, ((pcejin.aspectRatio)));
		checkMenu(ID_VIEW_MIXLEFTRIGHT,MixVideoOutput);
		checkMenu(ID_COLOR_MODE_REDBLUE, ((MDFN_IEN_VB::GetColorMode()==0)));
		checkMenu(ID_COLOR_MODE_REDCYAN, ((MDFN_IEN_VB::GetColorMode()==1)));
		checkMenu(ID_COLOR_MODE_REDELECTRICCYAN, ((MDFN_IEN_VB::GetColorMode()==2)));
		checkMenu(ID_COLOR_MODE_REDGREEN, ((MDFN_IEN_VB::GetColorMode()==3)));
		checkMenu(ID_COLOR_MODE_GREENMAGENTA, ((MDFN_IEN_VB::GetColorMode()==4)));
		checkMenu(ID_COLOR_MODE_YELLOWBLUE, ((MDFN_IEN_VB::GetColorMode()==5)));
		checkMenu(ID_COLOR_MODE_GREYSCALE, ((MDFN_IEN_VB::GetColorMode()==6)));
		checkMenu(ID_COLOR_MODE_RED, ((MDFN_IEN_VB::GetColorMode()==7)));
		checkMenu(ID_SPLIT_MODE_ANAGLYPH, ((MDFN_IEN_VB::GetSplitMode()==MDFN_IEN_VB::VB3DMODE_ANAGLYPH)));
		checkMenu(ID_SPLIT_MODE_CSCOPE, ((MDFN_IEN_VB::GetSplitMode()==MDFN_IEN_VB::VB3DMODE_CSCOPE)));
		checkMenu(ID_SPLIT_MODE_SIDEBYSIDE, ((MDFN_IEN_VB::GetSplitMode()==MDFN_IEN_VB::VB3DMODE_SIDEBYSIDE)));
		checkMenu(ID_SPLIT_MODE_PBARRIER, ((MDFN_IEN_VB::GetSplitMode()==MDFN_IEN_VB::VB3DMODE_PBARRIER)));
		checkMenu(ID_SPLIT_MODE_OVR, ((MDFN_IEN_VB::GetSplitMode()==MDFN_IEN_VB::VB3DMODE_OVR)));
		checkMenu(ID_VIEW_DISP_BOTH, ((DisplayLeftRightOutput==0)));
		checkMenu(ID_VIEW_DISP_LEFT, ((DisplayLeftRightOutput==1)));
		checkMenu(ID_VIEW_DISP_RIGHT, ((DisplayLeftRightOutput==2)));
		checkMenu(ID_VIEW_DISP_DISABLE, ((DisplayLeftRightOutput==3)));
		checkMenu(ID_VIEW_FRAMECOUNTER,Hud.FrameCounterDisplay);
		checkMenu(ID_VIEW_DISPLAYINPUT,Hud.ShowInputDisplay);
		checkMenu(ID_VIEW_OPENCONSOLE,OpenConsoleWindow);
		checkMenu(ID_VIEW_DISPLAYSTATESLOTS,Hud.DisplayStateSlots);
		checkMenu(ID_VIEW_DISPLAYLAG,Hud.ShowLagFrameCounter);
		checkMenu(IDM_MUTE,soundDriver->userMute);
		break;
	case WM_EXITMENULOOP:
		pcejin.tempUnPause();
		break;

	case WM_CLOSE:
		{
			SaveIniSettings();
			PostQuitMessage(0);
		}

	case WM_DESTROY:
		{
			PostQuitMessage(0);
		}
		// HANDLE_MSG(hWnd, WM_DESTROY, OnDestroy);
		// HANDLE_MSG(hWnd, WM_PAINT, OnPaint);
		// HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
	case WM_COMMAND:
		if(wParam >= baseid && wParam <= baseid + MAX_RECENT_ROMS - 1)
			{
				int x = wParam - baseid;
				soundDriver->resume();
				OpenRecentROM(x);

			}
			else if(wParam == clearid)
			{
				/* Clear all the recent ROMs */
				if(IDOK == MessageBox(g_hWnd, "OK to clear recent ROMs list?","VBJin",MB_OKCANCEL))
					ClearRecentRoms();
			}
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDC_WINDOW1X:
			pcejin.windowSize=1;
			ScaleScreen((float)pcejin.windowSize);
			break;
		case IDC_WINDOW15X:
			pcejin.windowSize=65535;
			ScaleScreen((float)pcejin.windowSize);
			break;
		case IDC_WINDOW2X:
			pcejin.windowSize=2;
			ScaleScreen((float)pcejin.windowSize);
			break;
		case IDC_WINDOW25X:
			pcejin.windowSize=65534;
			ScaleScreen((float)pcejin.windowSize);
			break;
		case IDC_WINDOW3X:
			pcejin.windowSize=3;
			ScaleScreen((float)pcejin.windowSize);
			break;
		case IDC_WINDOW4X:
			pcejin.windowSize=4;
			ScaleScreen((float)pcejin.windowSize);
			break;
		case IDC_ASPECT:
			pcejin.aspectRatio ^= 1;
			ScaleScreen((float)pcejin.windowSize);
			break;
		case IDM_EXIT:
			SaveIniSettings();
			PostQuitMessage(0);
			break;
		case IDM_RESET:
//NEWTODO			PCE_Power();
			OpenRecentROM(0);
			break;
		case IDM_OPEN_ROM:
			soundDriver->pause();
			LoadGame();
			pcejin.tempUnPause();
			break;
		case IDM_RECORD_MOVIE:
			soundDriver->pause();
			MovieRecordTo();
			pcejin.tempUnPause();
			return 0;
		case IDM_PLAY_MOVIE:
			soundDriver->pause();
			Replay_LoadMovie();
			pcejin.tempUnPause();
			return 0;
		case IDM_INPUT_CONFIG:
			soundDriver->pause();
			DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_INPUTCONFIG), hWnd, DlgInputConfig);
			pcejin.tempUnPause();
			// RunInputConfig();
			break;
		case IDM_HOTKEY_CONFIG:
			{
				soundDriver->pause();

				DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_KEYCUSTOM), hWnd, DlgHotkeyConfig);
				pcejin.tempUnPause();

			}
			break;

		case ID_VIEW_MIXLEFTRIGHT:
			MixVideoOutput ^= true;
			MDFN_IEN_VB::SetMixVideoOutput(MixVideoOutput);
			WritePrivateProfileBool("Display", "MixLeftRight", MixVideoOutput, IniName);
			return 0;

		case ID_SPLIT_MODE_ANAGLYPH:
			MDFN_IEN_VB::SetSplitMode(MDFN_IEN_VB::VB3DMODE_ANAGLYPH);
			WritePrivateProfileInt("Display", "SplitMode", MDFN_IEN_VB::VB3DMODE_ANAGLYPH, IniName);
			break;
		case ID_SPLIT_MODE_CSCOPE:
			MDFN_IEN_VB::SetSplitMode(MDFN_IEN_VB::VB3DMODE_CSCOPE);
			WritePrivateProfileInt("Display", "SplitMode", MDFN_IEN_VB::VB3DMODE_CSCOPE, IniName);
			//Clear the DirectDraw buffers
#if USE_DDRAW
			ClearDirectDrawOutput();
#endif // #if USE_DDRAW
			break;
		case ID_SPLIT_MODE_SIDEBYSIDE:
			MDFN_IEN_VB::SetSplitMode(MDFN_IEN_VB::VB3DMODE_SIDEBYSIDE);
			WritePrivateProfileInt("Display", "SplitMode", MDFN_IEN_VB::VB3DMODE_SIDEBYSIDE, IniName);
			//Clear the DirectDraw buffers
#if USE_DDRAW
			ClearDirectDrawOutput();
#endif // #if USE_DDRAW
			break;
		case ID_SPLIT_MODE_PBARRIER:
			MDFN_IEN_VB::SetSplitMode(MDFN_IEN_VB::VB3DMODE_PBARRIER);
			WritePrivateProfileInt("Display", "SplitMode", MDFN_IEN_VB::VB3DMODE_PBARRIER, IniName);
			break;
		case ID_SPLIT_MODE_OVR:
			MDFN_IEN_VB::SetSplitMode(MDFN_IEN_VB::VB3DMODE_OVR);
			WritePrivateProfileInt("Display", "SplitMode", MDFN_IEN_VB::VB3DMODE_OVR, IniName);
			break;
		case ID_PIXEL_SEP_16:
			SideBySidePixels = 16;
			MDFN_IEN_VB::SetSideBySidePixels(SideBySidePixels);
#if USE_DDRAW
			ClearDirectDrawOutput();
#endif // #if USE_DDRAW
			WritePrivateProfileInt("Display", "SideBySidePixels", SideBySidePixels, IniName);
			break;
		case ID_PIXEL_SEP_32:
			SideBySidePixels = 32;
			MDFN_IEN_VB::SetSideBySidePixels(SideBySidePixels);
#if USE_DDRAW
			ClearDirectDrawOutput();
#endif // #if USE_DDRAW
			WritePrivateProfileInt("Display", "SideBySidePixels", SideBySidePixels, IniName);
			break;
		case ID_PIXEL_SEP_48:
			SideBySidePixels = 48;
			MDFN_IEN_VB::SetSideBySidePixels(SideBySidePixels);
#if USE_DDRAW
			ClearDirectDrawOutput();
#endif // #if USE_DDRAW
			WritePrivateProfileInt("Display", "SideBySidePixels", SideBySidePixels, IniName);
			break;
		case ID_PIXEL_SEP_64:
			SideBySidePixels = 64;
			MDFN_IEN_VB::SetSideBySidePixels(SideBySidePixels);
#if USE_DDRAW
			ClearDirectDrawOutput();
#endif // #if USE_DDRAW
			WritePrivateProfileInt("Display", "SideBySidePixels", SideBySidePixels, IniName);
			break;
		case ID_PIXEL_SEP_80:
			SideBySidePixels = 80;
			MDFN_IEN_VB::SetSideBySidePixels(SideBySidePixels);
#if USE_DDRAW
			ClearDirectDrawOutput();
#endif // #if USE_DDRAW
			WritePrivateProfileInt("Display", "SideBySidePixels", SideBySidePixels, IniName);
			break;
		case ID_PIXEL_SEP_96:
			SideBySidePixels = 96;
			MDFN_IEN_VB::SetSideBySidePixels(SideBySidePixels);
#if USE_DDRAW
			ClearDirectDrawOutput();
#endif // #if USE_DDRAW
			WritePrivateProfileInt("Display", "SideBySidePixels", SideBySidePixels, IniName);
			break;
		case ID_COLOR_MODE_REDBLUE:
			MDFN_IEN_VB::SetColorMode(0);
			WritePrivateProfileInt("Display", "ColorMode", 0, IniName);
			break;
		case ID_COLOR_MODE_REDCYAN:
			MDFN_IEN_VB::SetColorMode(1);
			WritePrivateProfileInt("Display", "ColorMode", 1, IniName);
			break;
		case ID_COLOR_MODE_REDELECTRICCYAN:
			MDFN_IEN_VB::SetColorMode(2);
			WritePrivateProfileInt("Display", "ColorMode", 2, IniName);
			break;
		case ID_COLOR_MODE_REDGREEN:
			MDFN_IEN_VB::SetColorMode(3);
			WritePrivateProfileInt("Display", "ColorMode", 3, IniName);
			break;
		case ID_COLOR_MODE_GREENMAGENTA:
			MDFN_IEN_VB::SetColorMode(4);
			WritePrivateProfileInt("Display", "ColorMode", 4, IniName);
			break;
		case ID_COLOR_MODE_YELLOWBLUE:
			MDFN_IEN_VB::SetColorMode(5);
			WritePrivateProfileInt("Display", "ColorMode", 5, IniName);
			break;
		case ID_COLOR_MODE_GREYSCALE:
			MDFN_IEN_VB::SetColorMode(6);
			WritePrivateProfileInt("Display", "ColorMode", 6, IniName);
			break;
		case ID_COLOR_MODE_RED:
			MDFN_IEN_VB::SetColorMode(7);
			WritePrivateProfileInt("Display", "ColorMode", 7, IniName);
			break;
		case ID_VIEW_DISP_BOTH:
			DisplayLeftRightOutput = 0;
			MDFN_IEN_VB::SetViewDisp(DisplayLeftRightOutput);
			WritePrivateProfileInt("Display", "ViewDisplay", DisplayLeftRightOutput, IniName);
			return 0;

		case ID_VIEW_DISP_LEFT:
			DisplayLeftRightOutput = 1;
			MDFN_IEN_VB::SetViewDisp(DisplayLeftRightOutput);
			WritePrivateProfileInt("Display", "ViewDisplay", DisplayLeftRightOutput, IniName);
			return 0;

		case ID_VIEW_DISP_RIGHT:
			DisplayLeftRightOutput = 2;
			MDFN_IEN_VB::SetViewDisp(DisplayLeftRightOutput);
			WritePrivateProfileInt("Display", "ViewDisplay", DisplayLeftRightOutput, IniName);
			return 0;

		case ID_VIEW_DISP_DISABLE:
			DisplayLeftRightOutput = 3;
			MDFN_IEN_VB::SetViewDisp(DisplayLeftRightOutput);
			// We're not saving this. Too many people would set it and forget it, then compain.
			// Someone can still set it manually in the ini file though.
			return 0;

		case ID_VIEW_FRAMECOUNTER:
			Hud.FrameCounterDisplay ^= true;
			WritePrivateProfileBool("Display", "FrameCounter", Hud.FrameCounterDisplay, IniName);
			return 0;

		case ID_VIEW_DISPLAYINPUT:
			Hud.ShowInputDisplay ^= true;
			WritePrivateProfileBool("Display", "Display Input", Hud.ShowInputDisplay, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_OPENCONSOLE:
			OpenConsoleWindow ^= true;
			WritePrivateProfileBool("Display", "OpenConsoleWindow", OpenConsoleWindow, IniName);
			return 0;

		case ID_VIEW_DISPLAYSTATESLOTS:
			Hud.DisplayStateSlots ^= true;
			WritePrivateProfileBool("Display", "Display State Slots", Hud.DisplayStateSlots, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_DISPLAYLAG:
			Hud.ShowLagFrameCounter ^= true;
			WritePrivateProfileBool("Display", "Display Lag Counter", Hud.ShowLagFrameCounter, IniName);
			osd->clear();
			return 0;
		case IDC_NEW_LUA_SCRIPT:
			if(LuaScriptHWnds.size() < 16)
			{
				CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_LUA), g_hWnd, (DLGPROC) LuaScriptProc);
			}
			break;

			break;
		case IDM_MUTE:
			soundDriver->doUserMute();
			break;
		case IDM_STOPMOVIE:
			FCEUI_StopMovie();
			return 0;
			break;
		case ID_RAM_SEARCH:
			if(!RamSearchHWnd)
			{
				if (pcejin.romLoaded)
				{
					InitRamSearch(false);
					RamSearchHWnd = CreateDialog(winClass.hInstance, MAKEINTRESOURCE(IDD_RAMSEARCH), hWnd, (DLGPROC) RamSearchProc);
				}
			}
			else
				SetForegroundWindow(RamSearchHWnd);
			break;

		case ID_RAM_WATCH:
			if(!RamWatchHWnd)
			{
				if(pcejin.romLoaded)
					RamWatchHWnd = CreateDialog(winClass.hInstance, MAKEINTRESOURCE(IDD_RAMWATCH), hWnd, (DLGPROC) RamWatchProc);
			}
			else
				SetForegroundWindow(RamWatchHWnd);
			return 0;
		case IDM_MEMORY:
			if (!RegWndClass("MemView_ViewBox", MemView_ViewBoxProc, 0, sizeof(CMemView*)))
				return 0;

			OpenToolWindow(new CMemView());
			return 0;
		case IDM_ABOUT:
			soundDriver->pause();

			DialogBox(winClass.hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			pcejin.tempUnPause();
			break;
		case IDM_FILE_RECORDAVI:
			soundDriver->pause();
			RecordAvi();
			pcejin.tempUnPause();
			break;
		case IDM_FILE_STOPAVI:
			StopAvi();
			break;
		case IDM_FILE_RECORDWAV:
			soundDriver->pause();
			CreateSoundSave();
			pcejin.tempUnPause();
			break;
		case IDM_FILE_STOPWAV:
			DRV_EndWaveRecord();
			break;
		}
		break;
	}
	return DefWindowProc(hWnd, Message, wParam, lParam);
}

// Command message handler
void OnCommand(HWND hWnd, int iID, HWND hwndCtl, UINT uNotifyCode)
{
	//switch (iID)
	//{
	//case IDM_QUIT:
	// OnDestroy(hWnd);
	// break;
	//}
}

// Handle WM_DESTROY
void OnDestroy(HWND )
{
	g_bRunning = false;
	PostQuitMessage(0);
}

// Window painting function
void OnPaint(HWND hwnd)
{

	// Let Windows know we've redrawn the Window - since we've bypassed
	// the GDI, Windows can't figure that out by itself.
	ValidateRect( hwnd, NULL );
}

/*
if (lpDDClipPrimary!=NULL) IDirectDraw7_Release(lpDDClipPrimary);
if (lpPrimary != NULL) IDirectDraw7_Release(lpPrimary);
if (lpBack != NULL) IDirectDraw7_Release(lpBack);
if (lpdd7 != NULL) IDirectDraw7_Release(lpdd7);*/

void initespec();
void initsound();



void initialize(){

	puts("initialize");

	MDFNGameInfo = &EmulatedVB;

	vbjinInit();

//	MDFNI_LoadGame(NULL,"c:\\wario.vb");
//	pcejin.started = true;
//	pcejin.romLoaded = true;
	initespec();
	initsound();
}

//int16 *sound;
//int32 ssize;

void initvideo();

void initinput(){
	//NEWTODO
	MDFNGameInfo->SetInput(1,NULL,&pcejin.pads[0]);
/*
	PCEINPUT_SetInput(0, "gamepad", &pcejin.pads[0]);
	PCEINPUT_SetInput(1, "gamepad", &pcejin.pads[1]);
	PCEINPUT_SetInput(2, "gamepad", &pcejin.pads[2]);
	PCEINPUT_SetInput(3, "gamepad", &pcejin.pads[3]);
	PCEINPUT_SetInput(4, "gamepad", &pcejin.pads[4]);*/
}

//const char* Buttons[8] = {"I ", "II ", "S", "Run ", "U", "R", "D", "L"};
const char* Buttons[16] = {"A ", "B ", "RS", "LS ", "RUp", "RRight", "LRight", "LLeft", "LDown", "LUp", "Start", "Select", "RLeft", "RDown"};
const char* Spaces[16] =  {" ", " ", " ", " ", " ", " ", " ", " "," ", " ", " ", " ", " ", " ", " ", " "};

char str[64];

//NEWTODO
void SetInputDisplayCharacters(uint16 new_data[]){

	strcpy(str, "");

//	for ( int i = 0; i < 5; i++) {

		for (int x = 0; x < 16; x++) {

			if(new_data[0] & (1 << x)) {
				strcat(str, Buttons[x]);
			}
			else
				strcat(str, Spaces[x]);
		}
//	}

//		printf("\n");
//		printf(str);
//		printf("\n");
	strcpy(pcejin.inputDisplayString, str);
}

void initespec(){
	VTBuffer[0] = new MDFN_Surface(NULL, MDFNGameInfo->pitch / sizeof(uint32), MDFNGameInfo->fb_height, MDFNGameInfo->pitch / sizeof(uint32), MDFN_COLORSPACE_RGB, 0, 8, 16, 24);
	VTBuffer[1] = new MDFN_Surface(NULL, MDFNGameInfo->pitch / sizeof(uint32), MDFNGameInfo->fb_height, MDFNGameInfo->pitch / sizeof(uint32), MDFN_COLORSPACE_RGB, 0, 8, 16, 24);
	VTLineWidths[0] = (MDFN_Rect *)calloc(MDFNGameInfo->fb_height, sizeof(MDFN_Rect));
	VTLineWidths[1] = (MDFN_Rect *)calloc(MDFNGameInfo->fb_height, sizeof(MDFN_Rect));
//NEWTODO
	/*
	VTBuffer[0] = (uint32 *)malloc(MDFNGameInfo->pitch * 256);
	VTBuffer[1] = (uint32 *)malloc(MDFNGameInfo->pitch * 256);
	VTLineWidths[0] = (MDFN_Rect *)calloc(256, sizeof(MDFN_Rect));
	VTLineWidths[1] = (MDFN_Rect *)calloc(256, sizeof(MDFN_Rect));
	*/
	initvideo();
	initinput();

}

void MDFNI_SetSoundVolume(uint32 volume)
{
	/*NEWTODO
	FSettings.SoundVolume=volume;
	if(MDFNGameInfo)
	{
		MDFNGameInfo->SetSoundVolume(volume);
	}*/
}

void MDFNI_Sound(int Rate)
{
	//NEWTODO
//	FSettings.SndRate=Rate;
	if(MDFNGameInfo)
	{
//NEWTODO
	//	EmulatedPCE.Sound(Rate);
	}
}

bool soundInit()
{
	if( S_OK != CoInitializeEx( NULL, COINIT_APARTMENTTHREADED ) ) {
		puts("IDS_COM_FAILURE");
		// systemMessage( IDS_COM_FAILURE, NULL );
	}

	soundDriver = newDirectSound();//systemSoundInit();
	if ( !soundDriver )
		return false;

	if (!soundDriver->init(44100))
		return false;

	soundDriver->resume();
	return true;
}

void initsound(){
	EmuModBufferSize = (500 * 44100 + 999) / 1000;//format.rate
	EmuModBuffer = (int16*)malloc(EmuModBufferSize * (sizeof(short)*2));
//	EmuModBuffer = (int16 *)calloc(sizeof(short) * 2, EmuModBufferSize);//format.channels
	MDFNGameInfo->SetSoundRate(44100);

	espec.SoundBuf = EmuModBuffer;
	espec.SoundBufMaxSize = EmuModBufferSize;

	//	MDFNI_Sound(44100);
	MDFNI_SetSoundVolume(100);
	//NEWTODO
//	FSettings.soundmultiplier = 1;
}


void initvideo(){
//NEWTODO
//	MDFNI_SetPixelFormat(0,8,16,24);
}

extern u32 joypads [8];

void emulate(){
	/*
	//NEWTODO fix these
	

	pcejin.pads[0] = joypads [0];
	vbjinEmulate();
	*/
	if(!pcejin.started  || !pcejin.romLoaded)
		return;
	
	if (startPaused) 
	{
		pcejin.pause();
		startPaused = false;	//This should never be set true except from ParseCmdLine!
	}
	pcejin.isLagFrame = true;

	S9xUpdateJoypadButtons();
	pcejin.pads[0] = joypads [0];

//	pcejin.pads[1] = joypads [1];
//	pcejin.pads[2] = joypads [2];
//	pcejin.pads[3] = joypads [3];
//	pcejin.pads[4] = joypads [4];

	FCEUMOV_AddInputState();

	VTLineWidths[VTBackBuffer][0].w = ~0;

	memset(&espec, 0, sizeof(EmulateSpecStruct));

	espec.surface = (MDFN_Surface *)VTBuffer[VTBackBuffer];
	espec.surface2 = (MDFN_Surface *)VTBuffer[VTBackBuffer == 1 ? 0 : 1];
//espec.surface->pixels = (uint32 *)VTBuffer[VTBackBuffer];
	espec.LineWidths = (MDFN_Rect *)VTLineWidths[VTBackBuffer];
//espec.SoundBuf = sound;
//espec.SoundBufSize = ssize;
	espec.soundmultiplier = 1;

	espec.SoundBuf = EmuModBuffer;
	espec.SoundBufMaxSize = EmuModBufferSize;

	if(pcejin.fastForward && currFrameCounter%30 && !DRV_AviIsRecording())
		espec.skip = 1;
	else
		espec.skip = 0;

	CallRegisteredLuaFunctions(LUACALL_BEFOREEMULATION);


	MDFNGameInfo->Emulate(&espec);

	CallRegisteredLuaFunctions(LUACALL_AFTEREMULATION);

	UpdateToolWindows();

	//NEWTODO
	soundDriver->write((u16*)espec.SoundBuf, espec.SoundBufSize);

	DRV_AviSoundUpdate((u16*)espec.SoundBuf, espec.SoundBufSize);
	DRV_AviVideoUpdate((uint16*)espec.surface->pixels, &espec);

	// espec.SoundBufSize is a count of the 16 bit channel pairs.
	// Casting to uint32* cycles through all the data accurately.
	DRV_WriteWaveData((uint32*)espec.SoundBuf, espec.SoundBufSize);

	if (pcejin.frameAdvance)
	{
		pcejin.frameAdvance = false;
		pcejin.started = false;
	}

	if (pcejin.isLagFrame)
		pcejin.lagFrameCounter++;


	if(!pcejin.fastForward)//pcejin.slow  && 
		while(SpeedThrottle())
		{
		}

	//adelikat: For commandline loadstate, I know this is a hacky place to do it, but 1 frame needs to be emulated before loadstates are possible
	if (stateToLoad != -1)
	{
		HK_StateLoadSlot(stateToLoad);
		stateToLoad = -1;				//Never set this other than from ParseCmdLine
	}
}

bool first;

void FrameAdvance(bool state)
{
	if(state) {
		if(first) {
			pcejin.started = true;
			// execute = TRUE;
			soundDriver->resume();
			pcejin.frameAdvance=true;
			first=false;
		}
		else {
			pcejin.started = true;
			soundDriver->resume();
			// execute = TRUE;
		}
	}
	else {
		first = true;
		if(pcejin.frameAdvance)
		{}
		else
		{
			pcejin.started = false;
			// emu_halt();
			soundDriver->pause();
			// SPU_Pause(1);
			// emu_paused = 1;
		}
	}
}


// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		SetDlgItemText(hDlg, IDC_TXT_VERSION, pcejin.versionName.c_str());
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

#define MCM_SIGNATURE "MDFNMOVI"
#define MCM_HDR_SIZE 0x100
#define MCM_FRAME_SIZE 0x0B

static void read_mcm(const char* path,
                     unsigned char** body, unsigned long* size)
{
    FILE* in;
    unsigned long filesize;
    unsigned char hdr[MCM_HDR_SIZE];

    if(!(in = fopen(path, "rb"))) printf("Can't open file");

    fseek(in, 0, SEEK_END);
    filesize = ftell(in);
    fseek(in, 0, SEEK_SET);
    if(filesize <= MCM_HDR_SIZE) printf("Not MCM file (filesize <= %d)", MCM_HDR_SIZE);
    if((filesize-MCM_HDR_SIZE) % MCM_FRAME_SIZE) printf("Broken MCM file?");

    fread(hdr, 1, MCM_HDR_SIZE, in);

    if(0 != memcmp(hdr, MCM_SIGNATURE, 8)) printf("Not MCM file (signature not found)");

    *size = filesize - MCM_HDR_SIZE;
	if(!(*body = (unsigned char*)malloc(*size))) {printf("malloc() failed"); return;};

    fread(*body, 1, *size, in);

    fclose(in);
}
//this doesn't convert commands, one controller only, and loses the rerecord count
static void dump_frames(const char* path, std::string mc2, const unsigned char* body, unsigned long size)
{
    const unsigned char* p;
    unsigned char pad;
    char pad_str[9];
    unsigned long frame_count;
    unsigned long i;

	freopen( mc2.c_str(), "w", stdout );

    p = body;
    pad_str[8] = '\0';
    frame_count = size / MCM_FRAME_SIZE;

	printf("%s\n", "version 1");
	printf("%s\n", "emuVersion 9");
	printf("%s\n", "rerecordCount 1");

    for(i = 0; i < frame_count; ++i){

        pad = p[1];
		pad_str[0] = (pad&0x10) ? 'U' : '.';
		pad_str[1] = (pad&0x40) ? 'D' : '.';
		pad_str[2] = (pad&0x80) ? 'L' : '.';
        pad_str[3] = (pad&0x20) ? 'R' : '.';
        pad_str[4] = (pad&0x01) ? '1' : '.';
        pad_str[5] = (pad&0x02) ? '2' : '.';
        pad_str[6] = (pad&0x08) ? 'N' : '.';
        pad_str[7] = (pad&0x04) ? 'S' : '.';

		int command = 0;
		
		printf("%s%d%s%s%s\n", "|", command, "|", pad_str, "|");

        p += MCM_FRAME_SIZE;
    }
}

static void mcmdump(const char* path, std::string mc2)
{
    unsigned long size;
    unsigned char* body;

    read_mcm(path, &body, &size);

    dump_frames(path, mc2, body, size);

    free(body);
}

std::string noExtension(std::string path) {

	for(int i = int(path.size()) - 1; i >= 0; --i)
	{
		if (path[i] == '.') {
			return path.substr(0, i);
		}
	}
	return path;
}

std::string LoadMCM(const char* path, bool load) {

	std::string mc2;
	mc2 = noExtension(std::string(path));
	mc2 += ".mc2";
	mcmdump(path, mc2);
	if(load)
		FCEUI_LoadMovie(mc2.c_str(), 1, 0, 0);

	return mc2;
}

//adelikat: Removes the path from a filename and returns the result
//C:\ROM\bob.pce becomes bob.pce
std::string RemovePath(std::string filename)
{
	std::string name = "";						//filename with path removed
	int pos = filename.find_last_of("\\") + 1;	//position of first character of name
	int length = filename.length() - pos;		//length of name
	
	if (length != -1)							//If there was a path
	{
		name.append(filename,pos,length);
		return name;
	}
	else										//Else there is no path so return what it gave us
	{
		return filename;	
	}
}

std::string GetGameName()
{
	return GameName;
}