#include "dxsound.h"
#include "windows.h"
#include "git.h"

//#include "filter/filter.h"

void ScaleScreen(float factor);

void WritePrivateProfileInt(char* appname, char* keyname, int val, char* file);
extern char IniName[MAX_PATH];

void GetINIPath();

extern HWND g_hWnd;
extern HINSTANCE g_hInstance;

//int MDFNSS_Save(const char *fname, const char *suffix, uint32 *fb, MDFN_Rect *LineWidths);
//int MDFNSS_Load(const char *fname, const char *suffix);
extern volatile MDFN_Surface *VTBuffer[2]; 
extern MDFN_Rect *VTLineWidths[2]; 
extern volatile int VTBackBuffer;

extern int CurrentState;

extern void LoadInputConfig();
extern BOOL di_init();
extern INT_PTR CALLBACK DlgInputConfig(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
extern void PCE_Power();
extern void PCEINPUT_SetInput(int port, const char *type, void *ptr);
extern SoundDriver *newDirectSound();
extern void input_process();
extern SoundDriver * soundDriver;

void IncreaseSpeed();
void DecreaseSpeed();

LRESULT CALLBACK BiosSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

void SetInputDisplayCharacters(uint16 new_data[]);
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
extern EmulateSpecStruct espec;

extern uint8 convert_buffer[1024*768*3];

void WritePrivateProfileBool(char* appname, char* keyname, bool val, char* file);
bool GetPrivateProfileBool(const char* appname, const char* keyname, bool defval, const char* filename);

void S9xUpdateJoypadButtons ();
bool soundInit();
void PlayMovie(HWND hWnd);

void RecordAvi();
void StopAvi();

void UpdateToolWindows();

void IncreaseSpeed();
void DecreaseSpeed();
void InitSpeedThrottle();
int SpeedThrottle();
std::string LoadMCM(const char* path, bool load);
void UpdateTitleWithFilename(std::string filename);

std::string GetGameName();
std::string noExtension(std::string path);