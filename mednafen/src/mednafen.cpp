#include "git.h"
#include "settings.h"
#include "assert.h"
#include "general.h"
MDFNGI * MDFNGameInfo;
void MDFN_PrintError(const char *format, ...)
{
}

MDFNGI *MDFNI_LoadGame(const char *force_module, const char *name)
{
        MDFNFILE GameFile;
	//struct stat stat_buf;
	std::vector<FileExtensionSpecStruct> valid_iae;

	if(strlen(name) > 4 && (!strcasecmp(name + strlen(name) - 4, ".cue") || !strcasecmp(name + strlen(name) - 4, ".toc")))
	{
//	 return(MDFNI_LoadCD(force_module, name));
	}
	
//	if(!stat(name, &stat_buf) && !S_ISREG(stat_buf.st_mode))
	{
//	 return(MDFNI_LoadCD(force_module, name));
	}

//	MDFNI_CloseGame();

//	LastSoundMultiplier = 1;

	MDFNGameInfo = NULL;

	MDFN_printf("Loading %s...\n",name);

//MDFN_indent(1);

        GetFileBase(name);

//	if(MMPlay_Load(name) > 0)
//	{
//	 MDFNGameInfo = &MMPlayGI;
//	 goto SkipNormalLoad;
//	}
#if 0
	// Construct a NULL-delimited list of known file extensions for MDFN_fopen()
	for(unsigned int i = 0; i < MDFNSystems.size(); i++)
	{
	 const FileExtensionSpecStruct *curexts = MDFNSystems[i]->FileExtensions;

	 // If we're forcing a module, only look for extensions corresponding to that module
	 if(force_module && strcmp(MDFNSystems[i]->shortname, force_module))
	  continue;

	 if(curexts)	
 	  while(curexts->extension && curexts->description)
	  {
	   valid_iae.push_back(*curexts);
           curexts++;
 	  }
	}
	{
	 FileExtensionSpecStruct tmpext = { NULL, NULL };
	 valid_iae.push_back(tmpext);
	}
#endif
	if(!GameFile.Open(name, NULL, "game"))//&valid_iae[0]
        {
	 MDFNGameInfo = NULL;
	 return 0;
	}

	/*
	if(!LoadIPS(GameFile, MDFN_MakeFName(MDFNMKF_IPS, 0, 0).c_str()))
	{
	 MDFNGameInfo = NULL;
         GameFile.Close();
         return(0);
	}
*/
	MDFNGameInfo = NULL;
/*
	for(std::list<MDFNGI *>::iterator it = MDFNSystemsPrio.begin(); it != MDFNSystemsPrio.end(); it++)  //_unsigned int x = 0; x < MDFNSystems.size(); x++)
	{
		char tmpstr[256];
		trio_snprintf(tmpstr, 256, "%s.enable", (*it)->shortname);

		if(force_module)
		{
			if(!(*it)->Load)
				continue;

			if(!strcmp(force_module, (*it)->shortname))
			{
				MDFNGameInfo = *it;
				break;
			}
		}
		else
		{
			// Is module enabled?
			if(!MDFN_GetSettingB(tmpstr))
				continue; 

			if(!(*it)->Load || !(*it)->TestMagic)
				continue;

			if((*it)->TestMagic(name, &GameFile))
			{
				MDFNGameInfo = *it;
				break;
			}
		}
	}
*/
extern MDFNGI EmulatedVB;
	MDFNGameInfo= &EmulatedVB;

	if(!MDFNGameInfo)
	{
		GameFile.Close();

		MDFN_PrintError("Unrecognized file format.  Sorry.");
		//        MDFN_indent(-1);
		MDFNGameInfo = NULL;
		return 0;
	}

	MDFN_printf("Using module: %s(%s)\n\n", MDFNGameInfo->shortname, MDFNGameInfo->fullname);
	//MDFN_indent(1);

	assert(MDFNGameInfo->soundchan != 0);

	MDFNGameInfo->soundrate = 0;
	MDFNGameInfo->name = NULL;
	MDFNGameInfo->rotated = 0;

	if(MDFNGameInfo->Load(name, &GameFile) <= 0)
	{
		GameFile.Close();
		//        MDFN_indent(-2);
		MDFNGameInfo = NULL;
		return(0);
	}

	if(MDFNGameInfo->GameType != GMT_PLAYER)
	{
		// MDFN_LoadGameCheats(NULL);
		// MDFNMP_InstallReadPatches();
	}

	//SkipNormalLoad: ;

	#ifdef WANT_DEBUGGER
	MDFNDBG_PostGameLoad();
	#endif

	MDFNSS_CheckStates();
//MDFNMOV_CheckMovies();

	MDFN_ResetMessages();	// Save state, status messages, etc.

//MDFN_indent(-2);
#if 0
	if(!MDFNGameInfo->name)
	{
		unsigned int x;
		char *tmp;

		MDFNGameInfo->name = (UTF8 *)strdup(GetFNComponent(name));

		for(x=0;x<strlen((char *)MDFNGameInfo->name);x++)
		{
			if(MDFNGameInfo->name[x] == '_')
				MDFNGameInfo->name[x] = ' ';
		}
		if((tmp = strrchr((char *)MDFNGameInfo->name, '.')))
			*tmp = 0;
	}
#endif
	//VBlur_Init();

	MDFN_StateEvilBegin();

        return(MDFNGameInfo);
}