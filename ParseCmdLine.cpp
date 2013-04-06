#include <windows.h>
#include "resource.h"
#include "main.h"
#include "mednafen.h"
#include "pcejin.h"
#include "movie.h"
#include "ramwatch.h"

using namespace std;

bool startPaused;
int stateToLoad = -1;	//-1 since 0 will be slot 0
//To add additional commandline options
//1) add the identifier (-rom, -play, etc) into the argCmds array
//2) add a variable to store the argument in the list under "Strings that will get parsed"
//3) add an entry in the switch statement in order to assign the variable
//4) add code under the "execute commands" section to handle the given commandline

void ParseCmdLine(LPSTR lpCmdLine, HWND HWnd)
{
	string argumentList;					//Complete command line argument
	argumentList.assign(lpCmdLine);			//Assign command line to argumentList
	int argLength = argumentList.size();	//Size of command line argument

	//List of valid commandline args
	string argCmds[] = {"-cfg", "-rom", "-play", "-readwrite", "-loadstate", "-pause", "-lua", ""};	//Hint:  to add new commandlines, start by inserting them here.

	//Strings that will get parsed:
	string CfgToLoad = "";		//Cfg filename
	string RomToLoad = "";		//ROM filename
	string MovieToLoad = "";	//Movie filename
	string StateToLoad = "";	//Savestate filename
	vector<string> ScriptsToLoad;	//Lua script filenames
	string FileToLoad = "";		//Any file
	string PauseGame = "";		//adelikat: If user puts anything after -pause it will flag true, documentation will probably say put "1".  There is no case for "-paused 0" since, to my knowledge, it would serve no purpose
	string ReadWrite = "";		//adelikat: Read Only is the default so this will be the same situation as above, any value will set to read+write status

	//Temps for finding string list
	int commandBegin = 0;	//Beginning of Command
	int commandEnd = 0;		//End of Command
	string newCommand;		//Will hold newest command being parsed in the loop
	string trunc;			//Truncated argList (from beginning of command to end of argumentList

	//--------------------------------------------------------------------------------------------
	//Commandline parsing loop
	for (int x = 0; x < (sizeof argCmds / sizeof string); x++)
	{
		if (argumentList.find(argCmds[x]) != string::npos)
		{
			commandBegin = argumentList.find(argCmds[x]) + argCmds[x].size() + (argCmds[x].empty()?0:1);	//Find beginning of new command
			trunc = argumentList.substr(commandBegin);								//Truncate argumentList
			commandEnd = trunc.find(" ");											//Find next space, if exists, new command will end here
			if(argumentList[commandBegin] == '\"')									//Actually, if it's in quotes, extend to the end quote
			{
				commandEnd = trunc.find('\"', 1);
				if(commandEnd >= 0)
					commandBegin++, commandEnd--;
			}
			if (commandEnd < 0) commandEnd = argLength;								//If no space, new command will end at the end of list
			newCommand = argumentList.substr(commandBegin, commandEnd);				//assign freshly parsed command to newCommand
		}
		else
			newCommand = "";

		//Assign newCommand to appropriate variable
		switch (x)
		{
		case 0:	//-cfg
			CfgToLoad = newCommand;
			break;
		case 1:	//-rom
			RomToLoad = newCommand;
			break;
		case 2:	//-play
			MovieToLoad = newCommand;
			break;
		case 3:	//-readwrite
			ReadWrite = newCommand;
			break;
		case 4:	//-loadstate
			StateToLoad = newCommand;
			break;
		case 5:	//-pause
			PauseGame = newCommand;
			break;
		case 6:	//-lua
			ScriptsToLoad.push_back(newCommand);
			break;
		case 7: //  (a filename on its own, this must come BEFORE any other options on the commandline)
			if(newCommand[0] != '-')
				FileToLoad = newCommand;
			break;
		}
	}
	//--------------------------------------------------------------------------------------------
	//Execute commands
	
	// anything (rom, movie, cfg, luascript, etc.)
	//adelikat: for now assume ROM
	if (FileToLoad[0])
	{
		//adelikat: This code is currently in LoadGame, Drag&Drop, Recent ROMs, and here, time for a function
		pcejin.romLoaded = true;
		pcejin.started = true;
		if(!MDFNI_LoadGame(false, FileToLoad.c_str())) 
		{
			pcejin.started = false;
			pcejin.romLoaded = false;
		}
	}
	//{
	//	GensOpenFile(FileToLoad.c_str());
	//}

	//Cfg
	//if (CfgToLoad[0])
	//{
	//	Load_Config((char*)CfgToLoad.c_str(), NULL);
	//	strcpy(Str_Tmp, "config loaded from ");
	//	strcat(Str_Tmp, CfgToLoad.c_str());
	//	Put_Info(Str_Tmp, 2000);
	//}

	//ROM
	if (RomToLoad[0]) 
	{
		//adelikat: This code is currently in LoadGame, Drag&Drop, Recent ROMs, and here, time for a function
		pcejin.romLoaded = true;
		pcejin.started = true;
		if(!MDFNI_LoadGame(false, RomToLoad.c_str()))
		{
			pcejin.started = false;
			pcejin.romLoaded = false;		
		}
	}
	
	//Read+Write
	bool readwrite = true;
	if (ReadWrite[0]) readwrite = false;

	//Movie
	if (MovieToLoad[0])
	{
		if (pcejin.romLoaded)	
			FCEUI_LoadMovie(MovieToLoad.c_str(), readwrite, false, false);	
		if (AutoRWLoad)
		{
			//Open Ram Watch if its auto-load setting is checked
			OpenRWRecentFile(0);
			RamWatchHWnd = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_RAMWATCH), g_hWnd, (DLGPROC) RamWatchProc);
		}
	}
	
	//Loadstate
	if (StateToLoad[0])
	{
		//Load_State((char*)StateToLoad.c_str()); //adelikat: PCEjin doesn't do loadstate as
		//adelikat for now let's just load slot 0 no matter what
		stateToLoad = 0;
	}

	//Lua Scripts
	//for(unsigned int i = 0; i < ScriptsToLoad.size(); i++)
	//{
	//	if(ScriptsToLoad[i][0])
	//	{
	//		const char* error = GensOpenScript(ScriptsToLoad[i].c_str());
	//		if(error)
	//			fprintf(stderr, "failed to start script \"%s\" because: %s\n", ScriptsToLoad[i].c_str(), error);
	//	}
	//}

	//Paused
	if (PauseGame[0]) startPaused = true; 
}