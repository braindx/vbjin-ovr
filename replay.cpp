#include <io.h>
#include <fstream>
#include "resource.h"
#include "replay.h"
//#include "common.h"
#include "main.h"
#include "movie.h"
#include "utils/xstring.h"

#undef close

bool LoadFM2(MovieData& movieData, std::istream* fp, int size, bool stopAfterHeader);

bool replayreadonly=1;

//adelikat: TODO: put this in one of the header files
template<int BUFSIZE>
inline std::wstring GetDlgItemTextW(HWND hDlg, int nIDDlgItem) {
	wchar_t buf[BUFSIZE];
	GetDlgItemTextW(hDlg, nIDDlgItem, buf, BUFSIZE);
	return buf;
}

template<int BUFSIZE>
inline std::string GetDlgItemText(HWND hDlg, int nIDDlgItem) {
	char buf[BUFSIZE];
	GetDlgItemText(hDlg, nIDDlgItem, buf, BUFSIZE);
	return buf;
}


static char playfilename[MAX_PATH] = "";

void Describe(HWND hwndDlg)
{
	std::fstream fs (playfilename);
	MovieData md;
	LoadFM2(md, &fs, INT_MAX, false);
	fs.close();

	u32 num_frames = md.records.size();

	double tempCount = num_frames / (20000000.0/259/4/384);
	int num_seconds = (int)tempCount;
	int fraction = ((int)(tempCount * 100)) % 100;
	int seconds = num_seconds % 60;
	int minutes = (num_seconds / 60) % 60;
	int hours = (num_seconds / 60 / 60) % 60;
	char tmp[256];
	sprintf(tmp, "%02d:%02d:%02d.%02d", hours, minutes, seconds, fraction);

	SetDlgItemText(hwndDlg,IDC_MLENGTH,tmp);
	SetDlgItemInt(hwndDlg,IDC_MFRAMES,num_frames,FALSE);
	SetDlgItemText(hwndDlg, PM_FILENAME, playfilename);
	SetDlgItemInt(hwndDlg,IDC_MRERECORDCOUNT,md.rerecordCount,FALSE);
//	SetDlgItemText(hwndDlg,IDC_MROM,md.romSerial.c_str());
}

//Replay movie dialog
BOOL CALLBACK ReplayDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	OPENFILENAME ofn;
	char szChoice[MAX_PATH]={0};
	char filename[MAX_PATH] = "";

	switch(uMsg)
	{
	case WM_INITDIALOG:
		SendDlgItemMessage(hwndDlg, IDC_CHECK_READONLY, BM_SETCHECK, replayreadonly?BST_CHECKED:BST_UNCHECKED, 0);			
		
		//Clear fields
		SetWindowText(GetDlgItem(hwndDlg, IDC_MLENGTH), "");
		SetWindowText(GetDlgItem(hwndDlg, IDC_MFRAMES), "");
		SetWindowText(GetDlgItem(hwndDlg, IDC_MRERECORDCOUNT), "");
		SetWindowText(GetDlgItem(hwndDlg, IDC_MROM), "");
		return FALSE;

	case WM_COMMAND:	
		int wID = LOWORD(wParam);
		switch(wID)
		{
			case ID_BROWSE:

				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = g_hWnd;
				ofn.lpstrFilter = "Movie File (*.mc2)\0*.mc2\0All files(*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile =  filename;
				ofn.lpstrTitle = "Replay Movie from File";
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "mc2";
				GetOpenFileName(&ofn);
				strcpy(playfilename, filename);
				Describe(hwndDlg);
				return true;
		
			case IDC_CHECK_READONLY:
				replayreadonly ^= 1;
				return true;

			case IDOK:	
				FCEUI_LoadMovie(playfilename, replayreadonly, false, 80000);
				ZeroMemory(&playfilename, sizeof(playfilename));
				EndDialog(hwndDlg, 0);
				return true;

			case IDCANCEL:
				ZeroMemory(&playfilename, sizeof(playfilename));
				EndDialog(hwndDlg, 0);
				return true;
		}
	}

	return false;
}
int controllers=1;
std::string sramfname;

//Record movie dialog
static BOOL CALLBACK RecordDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static struct CreateMovieParameters* p = NULL;
	std::string author = "";
	std::string fname;
	int x;	//temp vairable
	switch(uMsg)
	{
	case WM_INITDIALOG:
//		CheckDlgButton(hwndDlg, IDC_START_FROM_SRAM, ((flag == 1) ? BST_CHECKED : BST_UNCHECKED));
		//Dropbox for numbers of players
		SendDlgItemMessage(hwndDlg, IDC_2_PLAYER,(UINT) CB_ADDSTRING, 0,(LPARAM) "1 Player" );
		SendDlgItemMessage(hwndDlg, IDC_2_PLAYER,(UINT) CB_ADDSTRING, 0,(LPARAM) "2 Players" );
		SendDlgItemMessage(hwndDlg, IDC_2_PLAYER,(UINT) CB_ADDSTRING, 0,(LPARAM) "3 Players" );
		SendDlgItemMessage(hwndDlg, IDC_2_PLAYER,(UINT) CB_ADDSTRING, 0,(LPARAM) "4 Players" );
		SendDlgItemMessage(hwndDlg, IDC_2_PLAYER,(UINT) CB_ADDSTRING, 0,(LPARAM) "5 Players" );
		SendDlgItemMessage(hwndDlg, IDC_2_PLAYER, CB_SETCURSEL, 0, 0);
		return false;
		case WM_COMMAND:
		
	
		switch(LOWORD(wParam))
			{
			case IDOK: {
				controllers = (SendDlgItemMessage(hwndDlg, IDC_2_PLAYER,(UINT) CB_GETCURSEL, 0,0)+1);
				author = GetDlgItemText<500>(hwndDlg,IDC_EDIT_AUTHOR);
				fname = GetDlgItemText<MAX_PATH>(hwndDlg,IDC_EDIT_FILENAME);
				if (fname.length())
				{
					FCEUI_SaveMovie(fname.c_str(), author, controllers);//, flag, sramfname);
					EndDialog(hwndDlg, 0);
				}
				return true;
				}

			case IDCANCEL:
				EndDialog(hwndDlg, 0);
				return true;
	
			case IDC_BUTTON_BROWSEFILE:
			{
				OPENFILENAME ofn;
				char szChoice[MAX_PATH]={0};

				// browse button
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = g_hWnd;
				ofn.lpstrFilter = "Movie File (*.mc2)\0*.mc2\0All files(*.*)\0*.*\0\0";
				ofn.lpstrFile = szChoice;
				ofn.lpstrTitle = "Record a new movie";
				ofn.lpstrDefExt = "mc2";
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
				GetSaveFileName(&ofn);
			
				//If user did not specify an extension, add .mc2 for them
				fname = szChoice;
				x = fname.find_last_of(".");
				if (x < 0)
					fname.append(".mc2");

				SetDlgItemText(hwndDlg, IDC_EDIT_FILENAME, fname.c_str());
				//if(GetSaveFileName(&ofn))
				//	UpdateRecordDialogPath(hwndDlg,szChoice);

				return true;
			}
			case IDC_BUTTON_BROWSESRAM:
			{/*
				OPENFILENAME ofn;
				char szChoice[MAX_PATH]={0};

				// browse button
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = g_hWnd;
				ofn.lpstrFilter = "Desmume SRAM File (*.dsv)\0*.dsv\0All files(*.*)\0*.*\0\0";
				ofn.lpstrFile = szChoice;
				ofn.lpstrTitle = "Choose SRAM";
				ofn.lpstrDefExt = "dsv";
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
				GetOpenFileName(&ofn);
			
				//If user did not specify an extension, add .dsm for them
				fname = szChoice;
				x = fname.find_last_of(".");
				if (x < 0)
					fname.append(".dsv");

				SetDlgItemText(hwndDlg, IDC_EDIT_SRAMFILENAME, fname.c_str());
				sramfname=(std::string)fname;
				//if(GetSaveFileName(&ofn))
				//	UpdateRecordDialogPath(hwndDlg,szChoice);
				*/
				return true;
			}
		}
	}

	HWND cur = GetDlgItem(hwndDlg, IDC_EDIT_SRAMFILENAME);
	
//	IsDlgButtonChecked(hwndDlg, IDC_START_FROM_SRAM) ? flag=1 : flag=0;
	IsDlgButtonChecked(hwndDlg, IDC_START_FROM_SRAM) ? EnableWindow(cur, TRUE) : EnableWindow(cur, FALSE);

	cur = GetDlgItem(hwndDlg, IDC_BUTTON_BROWSESRAM);
	IsDlgButtonChecked(hwndDlg, IDC_START_FROM_SRAM) ? EnableWindow(cur, TRUE) : EnableWindow(cur, FALSE);

//	IsDlgButtonChecked(hwndDlg, IDC_2_PLAYER) ? controllers=2 : controllers=1;

	return false;
}


//Show the play movie dialog and play a movie
void Replay_LoadMovie()
{
	char* fn = (char*)DialogBoxParam(g_hInstance, "IDD_REPLAYINP", g_hWnd, ReplayDialogProc, false);

	if(fn)
	{
		FCEUI_LoadMovie(fn, movie_readonly, false, -1);

		free(fn);
	}
}

//Show the record movie dialog and record a movie.
void MovieRecordTo()
{
	DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_RECORDMOVIE), g_hWnd, RecordDialogProc, (LPARAM)0);
}
