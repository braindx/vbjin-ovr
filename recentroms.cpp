#include <string>
#include <vector>
#include <sstream>
#include "windows.h"
#include <commctrl.h>
#include <shlwapi.h>
#include "movie.h"
#include "recentroms.h"
#include "resource.h"
#include "mednafen.h"
#include "pcejin.h"

using namespace std;

#define IDM_RECENT_RESERVED0                    65500
#define IDM_RECENT_RESERVED1                    65501

//----Recent ROMs menu globals----------
vector<string> RecentRoms;					//The list of recent ROM filenames
const unsigned int MAX_RECENT_ROMS = 10;	//To change the recent rom max, simply change this number
const unsigned int clearid = IDM_RECENT_RESERVED0;			// ID for the Clear recent ROMs item
const unsigned int baseid = IDM_RECENT_RESERVED1;			//Base identifier for the recent ROMs items
HMENU recentromsmenu;				//Handle to the recent ROMs submenu
//--------------------------------------

//adelikat: blah blah hacky
extern char IniName[MAX_PATH];
extern HWND g_hWnd;

void UpdateRecentRomsMenu()
{
	//This function will be called to populate the Recent Menu
	//The array must be in the proper order ahead of time

	//UpdateRecentRoms will always call this
	//This will be always called by GetRecentRoms on PCEjin startup


	//----------------------------------------------------------------------
	//Get Menu item info

	MENUITEMINFO moo;
	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_SUBMENU | MIIM_STATE;

	GetMenuItemInfo(GetSubMenu(GetMenu(g_hWnd), 0), ID_FILE_RECENTROM, FALSE, &moo);
	moo.hSubMenu = GetSubMenu(recentromsmenu, 0);
	//moo.fState = RecentRoms[0].c_str() ? MFS_ENABLED : MFS_GRAYED;
	moo.fState = MFS_ENABLED;
	SetMenuItemInfo(GetSubMenu(GetMenu(g_hWnd), 0), ID_FILE_RECENTROM, FALSE, &moo);

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
	//Clear the current menu items
	for(int x = 0; x < MAX_RECENT_ROMS; x++)
	{
		DeleteMenu(GetSubMenu(recentromsmenu, 0), baseid + x, MF_BYCOMMAND);
	}

	if(RecentRoms.size() == 0)
	{
		EnableMenuItem(GetSubMenu(recentromsmenu, 0), clearid, MF_GRAYED);

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_STATE | MIIM_TYPE;

		moo.cch = 5;
		moo.fType = 0;
		moo.wID = baseid;
		moo.dwTypeData = "None";
		moo.fState = MF_GRAYED;

		InsertMenuItem(GetSubMenu(recentromsmenu, 0), 0, TRUE, &moo);

		return;
	}

	EnableMenuItem(GetSubMenu(recentromsmenu, 0), clearid, MF_ENABLED);
	DeleteMenu(GetSubMenu(recentromsmenu, 0), baseid, MF_BYCOMMAND);

	HDC dc = GetDC(g_hWnd);

	//-----------------------------------------------------------------------
	//Update the list using RecentRoms vector
	for(int x = RecentRoms.size()-1; x >= 0; x--)	//Must loop in reverse order since InsertMenuItem will insert as the first on the list
	{
		string tmp = RecentRoms[x];
		LPSTR tmp2 = (LPSTR)tmp.c_str();

		PathCompactPath(dc, tmp2, 500);

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;

		moo.cch = tmp.size();
		moo.fType = 0;
		moo.wID = baseid + x;
		moo.dwTypeData = tmp2;
		//LOG("Inserting: %s\n",tmp.c_str());  //Debug
		InsertMenuItem(GetSubMenu(recentromsmenu, 0), 0, 1, &moo);
	}
	ReleaseDC(g_hWnd, dc);
	//-----------------------------------------------------------------------

	DrawMenuBar(g_hWnd);
}

void UpdateRecentRoms(const char* filename)
{
	//This function assumes filename is a ROM filename that was successfully loaded

	string newROM = filename; //Convert to std::string

	//--------------------------------------------------------------
	//Check to see if filename is in list
	vector<string>::iterator x;
	vector<string>::iterator theMatch;
	bool match = false;
	for (x = RecentRoms.begin(); x < RecentRoms.end(); ++x)
	{
		if (newROM == *x)
		{
			//We have a match
			match = true;	//Flag that we have a match
			theMatch = x;	//Hold on to the iterator	(Note: the loop continues, so if for some reason we had a duplicate (which wouldn't happen under normal circumstances, it would pick the last one in the list)
		}
	}
	//----------------------------------------------------------------
	//If there was a match, remove it
	if (match)
		RecentRoms.erase(theMatch);

	RecentRoms.insert(RecentRoms.begin(), newROM);	//Add to the array

	//If over the max, we have too many, so remove the last entry
	if (RecentRoms.size() == MAX_RECENT_ROMS)	
		RecentRoms.pop_back();

	//Debug
	//for (int x = 0; x < RecentRoms.size(); x++)
	//	LOG("Recent ROM: %s\n",RecentRoms[x].c_str());

	UpdateRecentRomsMenu();
	extern void SaveIniSettings();	//adelikat: Addign this function call to fix recent roms.  Buy WHY does this work?!
	SaveIniSettings();
}

void RemoveRecentRom(std::string filename)
{

	vector<string>::iterator x;
	vector<string>::iterator theMatch;
	bool match = false;
	for (x = RecentRoms.begin(); x < RecentRoms.end(); ++x)
	{
		if (filename == *x)
		{
			//We have a match
			match = true;	//Flag that we have a match
			theMatch = x;	//Hold on to the iterator	(Note: the loop continues, so if for some reason we had a duplicate (which wouldn't happen under normal circumstances, it would pick the last one in the list)
		}
	}
	//----------------------------------------------------------------
	//If there was a match, remove it
	if (match)
		RecentRoms.erase(theMatch);

	UpdateRecentRomsMenu();
}

void GetRecentRoms()
{
	//This function retrieves the recent ROMs stored in the .ini file
	//Then is populates the RecentRomsMenu array
	//Then it calls Update RecentRomsMenu() to populate the menu

	stringstream temp;
	char tempstr[256];

	// Avoids duplicating when changing the language.
	RecentRoms.clear();

	//Loops through all available recent slots
	for (int x = 0; x < MAX_RECENT_ROMS; x++)
	{
		temp.str("");
		temp << "Recent Rom " << (x+1);

		GetPrivateProfileString("General",temp.str().c_str(),"", tempstr, 256, IniName);
		if (tempstr[0])
			RecentRoms.push_back(tempstr);
	}
	UpdateRecentRomsMenu();
}

void SaveRecentRoms()
{
	//This function stores the RecentRomsMenu array to the .ini file

	stringstream temp;

	//Loops through all available recent slots
	for (int x = 0; x < MAX_RECENT_ROMS; x++)
	{
		temp.str("");
		temp << "Recent Rom " << (x+1);
		if (x < (int)RecentRoms.size())	//If it exists in the array, save it
			WritePrivateProfileString("General",temp.str().c_str(),RecentRoms[x].c_str(),IniName);
		else						//Else, make it empty
			WritePrivateProfileString("General",temp.str().c_str(), "",IniName);
	}
}

void ClearRecentRoms()
{
	RecentRoms.clear();
	SaveRecentRoms();
	UpdateRecentRomsMenu();
}

void OpenRecentROM(int listNum)
{
	if (listNum > MAX_RECENT_ROMS) return; //Just in case
	char filename[MAX_PATH];

	// Sanity check for reset, in case the list is cleared
	if (!RecentRoms.size())
		return;
	soundDriver->pause();
	strcpy(filename, RecentRoms[listNum].c_str());
	
	pcejin.romLoaded = true;
	pcejin.started = true;
	
	if(!MDFNI_LoadGame(NULL,filename)) 
	{
		pcejin.started = false;
		pcejin.romLoaded = false;		

		string str = "Could not open ";
		str.append(filename);
		str.append("\n\nRemove from list?");
		if (MessageBox(g_hWnd, str.c_str(), "File error", MB_YESNO) == IDYES)
		{
			RemoveRecentRom(RecentRoms[listNum]);
		}	
	} else {
		UpdateRecentRoms(filename);
		UpdateTitleWithFilename(filename);
	}
	
	pcejin.tempUnPause();
	ResetFrameCount();
	
}