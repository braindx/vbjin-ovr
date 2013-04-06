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

#include "mednafen.h"
#include <errno.h>
#include <string.h>
#include <vector>
#include <string>
//#include <trio/trio.h>
#include <map>
#include <list>
#include "settings.h"
#include "md5.h"
#include "types.h"
//#include "string/world_strtod.h"
#include "assert.h"

#include "windows.h"
#include "types.h"
#include "main.h"
#include <string>


char IniName[MAX_PATH];

void GetINIPath()
{
	char vPath[MAX_PATH], *szPath; //, currDir[MAX_PATH];
	/*if (*vPath)
	szPath = vPath;
	else
	{*/
	char *p;
	ZeroMemory(vPath, sizeof(vPath));
	GetModuleFileName(NULL, vPath, sizeof(vPath));
	p = vPath + lstrlen(vPath);
	while (p >= vPath && *p != '\\') p--;
	if (++p >= vPath) *p = 0;
	szPath = vPath;
	if (strlen(szPath) + strlen("vbjin.ini") < MAX_PATH)
	{
		sprintf(IniName, "%s\\vbjin.ini",szPath);
	} else if (MAX_PATH> strlen(".\\vbjin.ini")) {
		sprintf(IniName, ".\\vbjin.ini");
	} else
	{
		memset(IniName,0,MAX_PATH) ;
	}
}

void WritePrivateProfileBool(char* appname, char* keyname, bool val, char* file)
{
	char temp[256] = "";
	sprintf(temp, "%d", val?1:0);
	WritePrivateProfileString(appname, keyname, temp, file);
}

bool GetPrivateProfileBool(const char* appname, const char* keyname, bool defval, const char* filename)
{
	return GetPrivateProfileInt(appname,keyname,defval?1:0,filename) != 0;
}

void WritePrivateProfileInt(char* appname, char* keyname, int val, char* file)
{
	char temp[256] = "";
	sprintf(temp, "%d", val);
	WritePrivateProfileString(appname, keyname, temp, file);
}

#define trio_fprintf fprintf

typedef struct
{
 char *name;
 char *value;
} UnknownSetting_t;

std::multimap <uint32, MDFNCS> CurrentSettings;
std::vector<UnknownSetting_t> UnknownSettings;

static std::string fname; // TODO: remove

static bool TranslateSettingValueUI(const char *value, unsigned long long &tlated_value)
{
 char *endptr = NULL;

 if(value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
  tlated_value = strtoull(value + 2, &endptr, 16);
 else
  tlated_value = strtoull(value, &endptr, 10);

 if(!endptr || *endptr != 0)
 {
  return(false);
 }
 return(true);
}

static bool TranslateSettingValueI(const char *value, long long &tlated_value)
{
 char *endptr = NULL;

 if(value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
  tlated_value = strtoll(value + 2, &endptr, 16);
 else
  tlated_value = strtoll(value, &endptr, 10);

 if(!endptr || *endptr != 0)
 {
  return(false);
 }
 return(true);
}


static bool ValidateSetting(const char *value, const MDFNSetting *setting)
{
 MDFNSettingType base_type = (MDFNSettingType)(setting->type & MDFNST_BASE_MASK);

 if(base_type == MDFNST_UINT)
 {
  unsigned long long ullvalue;

  if(!TranslateSettingValueUI(value, ullvalue))
  {
   MDFN_PrintError("Setting \"%s\", value \"%s\", is not set to a valid unsigned integer.", setting->name, value);
   return(0);
  }
  if(setting->minimum)
  {
   unsigned long long minimum;

   TranslateSettingValueUI(setting->minimum, minimum);
   if(ullvalue < minimum)
   {
    MDFN_PrintError("Setting \"%s\" is set too small(\"%s\"); the minimum acceptable value is \"%s\".", setting->name, value, setting->minimum);
    return(0);
   }
  }
  if(setting->maximum)
  {
   unsigned long long maximum;

   TranslateSettingValueUI(setting->maximum, maximum);
   if(ullvalue > maximum)
   {
    MDFN_PrintError("Setting \"%s\" is set too large(\"%s\"); the maximum acceptable value is \"%s\".", setting->name, value, setting->maximum);
    return(0);
   }
  }
 }
 else if(base_type == MDFNST_INT)
 {
  long long llvalue;

  if(!TranslateSettingValueI(value, llvalue))
  {
   MDFN_PrintError("Setting \"%s\", value \"%s\", is not set to a valid signed integer.", setting->name, value);
   return(0);
  }
  if(setting->minimum)
  {
   long long minimum;

   TranslateSettingValueI(setting->minimum, minimum);
   if(llvalue < minimum)
   {
    MDFN_PrintError("Setting \"%s\" is set too small(\"%s\"); the minimum acceptable value is \"%s\".", setting->name, value, setting->minimum);
    return(0);
   }
  }
  if(setting->maximum)
  {
   long long maximum;

   TranslateSettingValueI(setting->maximum, maximum);
   if(llvalue > maximum)
   {
    MDFN_PrintError("Setting \"%s\" is set too large(\"%s\"); the maximum acceptable value is \"%s\".", setting->name, value, setting->maximum);
    return(0);
   }
  }
 }
 else if(base_type == MDFNST_FLOAT)
 {
  double dvalue;
  char *endptr = NULL;

//  dvalue = world_strtod(value, &endptr);

  if(!endptr || *endptr != 0)
  {
   MDFN_PrintError("Setting \"%s\", value \"%s\", is not set to a floating-point(real) number.", setting->name, value);
   return(0);
  }
  if(setting->minimum)
  {
   double minimum;

//   minimum = world_strtod(setting->minimum, NULL);
   if(dvalue < minimum)
   {
    MDFN_PrintError("Setting \"%s\" is set too small(\"%s\"); the minimum acceptable value is \"%s\".", setting->name, value, setting->minimum);
    return(0);
   }
  }
  if(setting->maximum)
  {
   double maximum;

//   maximum = world_strtod(setting->maximum, NULL);
   if(dvalue > maximum)
   {
    MDFN_PrintError("Setting \"%s\" is set too large(\"%s\"); the maximum acceptable value is \"%s\".", setting->name, value, setting->maximum);
    return(0);
   }
  }
 }
 else if(base_type == MDFNST_BOOL)
 {
  if(strlen(value) != 1 || (value[0] != '0' && value[0] != '1'))
  {
   MDFN_PrintError("Setting \"%s\", value \"%s\",  is not a valid boolean value.", setting->name, value);
   return(0);
  }
 }
 else if(base_type == MDFNST_ENUM)
 {
  const MDFNSetting_EnumList *enum_list = setting->enum_list;
  bool found = false;
  std::string valid_string_list;

  assert(enum_list);

  do
  {
   if(!strcasecmp(value, enum_list->string))
   {
    found = true;
    break;
   }
   valid_string_list = valid_string_list + std::string(enum_list->string) + (enum_list[1].string ? " " : "");
  } while((++enum_list)->string);
  if(!found)
  {
   MDFN_PrintError("Setting \"%s\", value \"%s\", is not a recognized string.  Recognized strings: %s", setting->name, value, valid_string_list.c_str());
   return(0);
  }
 }


 if(setting->validate_func && !setting->validate_func(setting->name, value))
 {
  if(base_type == MDFNST_STRING)
   MDFN_PrintError("Setting \"%s\" is not set to a valid string: \"%s\"", setting->name, value);
  else
   MDFN_PrintError("Setting \"%s\" is not set to a valid unsigned integer: \"%s\"", setting->name, value);
  return(0);
 }

 return(1);
}

static uint32 MakeNameHash(const char *name)
{
 uint32 name_hash;

 name_hash = crc32(0, (const Bytef *)name, strlen(name));

 return(name_hash);
}

bool MFDN_LoadSettings(const char *basedir)
{
 FILE *fp;

 fname = basedir;
// fname += PSS;
 fname += "mednafen-09x.cfg";

 MDFN_printf("Loading settings from \"%s\"...", fname.c_str());

 //printf("%s\n", fname.c_str());
 if(!(fp = fopen(fname.c_str(), "rb")))
 {
////  ErrnoHolder ene(errno);

//  MDFN_printf("Failed: %s\n", ene.StrError());

//  if(ene.Errno() == ENOENT) // Don't return failure if the file simply doesn't exist.
   return(1);
 // else
   return(0);
 }
 MDFN_printf("\n");

 char linebuf[1024];

 while(fgets(linebuf, 1024, fp) > 0)
 {
  char *spacepos = strchr(linebuf, ' ');
  uint32 name_hash;

  if(!spacepos) continue;	// EOF or bad line

  if(spacepos == linebuf) continue;	// No name(key)
  if(spacepos[1] == 0) continue;	// No value
  if(spacepos[0] == ';') continue;	// Comment

  // FIXME
  if(linebuf[0] == ';')
   continue;

  *spacepos = 0;
 
  char *lfpos = strchr(spacepos + 1, '\n');
  if(lfpos) *lfpos = 0;
  lfpos = strchr(spacepos + 1, '\r');
  if(lfpos) *lfpos = 0;

  if(spacepos[1] == 0) continue;        // No value

  name_hash = MakeNameHash(linebuf);

  //printf("%016llx\n", name_hash);

  std::pair<std::multimap <uint32, MDFNCS>::iterator, std::multimap <uint32, MDFNCS>::iterator> sit_pair;
  std::multimap <uint32, MDFNCS>::iterator sit;
  bool SettingRecognized = false;

  sit_pair = CurrentSettings.equal_range(name_hash);

  //printf("%s\n\n", linebuf);
  for(sit = sit_pair.first; sit != sit_pair.second; sit++)
  {
   //printf("%s %s\n", sit->second.name, linebuf);
   if(!strcmp(sit->second.name, linebuf))
   {
    if(sit->second.value) 
     free(sit->second.value);

    sit->second.value = strdup(spacepos + 1);
    sit->second.name_hash = name_hash;

    if(!ValidateSetting(sit->second.value, sit->second.desc))
     return(0);
    SettingRecognized = true;
    break;
   }
  }
 
  if(!SettingRecognized)
  {
   UnknownSetting_t unks;

   unks.name = strdup(linebuf);
   unks.value = strdup(spacepos + 1);

   UnknownSettings.push_back(unks);
  }
  
 }
 fclose(fp);
 return(1);
}

static INLINE void MergeSettingSub(const MDFNSetting *setting)
{
  MDFNCS TempSetting;
  uint32 name_hash;

  name_hash = MakeNameHash(setting->name);

  TempSetting.name = strdup(setting->name);
  TempSetting.value = strdup(setting->default_value);
  TempSetting.name_hash = name_hash;
  TempSetting.desc = setting;
  TempSetting.ChangeNotification = setting->ChangeNotification;
  TempSetting.netplay_override = NULL;

  CurrentSettings.insert(std::pair<uint32, MDFNCS>(name_hash, TempSetting)); //[name_hash] = TempSetting;
}


bool MDFN_MergeSettings(const MDFNSetting *setting)
{
 while(setting->name != NULL)
 {
  MergeSettingSub(setting);
  setting++;
 }
 return(1);
}

bool MDFN_MergeSettings(const std::vector<MDFNSetting> &setting)
{
 for(unsigned int x = 0; x < setting.size(); x++)
  MergeSettingSub(&setting[x]);

 return(1);
}

static bool compare_sname(MDFNCS *first, MDFNCS *second)
{
 return(strcmp(first->name, second->name) < 0);
}

bool MDFN_SaveSettings(void)
{
 std::multimap <uint32, MDFNCS>::iterator sit;
 std::list<MDFNCS *> SortedList;
 std::list<MDFNCS *>::iterator lit;

 FILE *fp;

 if(!(fp = fopen(fname.c_str(), "wb")))
  return(0);

// trio_fprintf(fp, ";VERSION %s\n", MEDNAFEN_VERSION);

 trio_fprintf(fp, ";Edit this file at your own risk!\n");
 trio_fprintf(fp, ";File format: <key><single space><value><LF or CR+LF>\n\n");

 for(sit = CurrentSettings.begin(); sit != CurrentSettings.end(); sit++)
 {
  SortedList.push_back(&sit->second);
  //trio_fprintf(fp, ";%s\n%s %s\n\n", _(sit->second.desc->description), sit->second.name, sit->second.value);
  //free(sit->second.name);
  //free(sit->second.value);
 }

 SortedList.sort(compare_sname);

 for(lit = SortedList.begin(); lit != SortedList.end(); lit++)
 {
  trio_fprintf(fp, ";%s\n%s %s\n\n", (*lit)->desc->description, (*lit)->name, (*lit)->value);
  free((*lit)->name);
  free((*lit)->value);
 }

 if(UnknownSettings.size())
 {
  trio_fprintf(fp, "\n;\n;Unrecognized settings follow:\n;\n\n");
  for(unsigned int i = 0; i < UnknownSettings.size(); i++)
  {
   trio_fprintf(fp, "%s %s\n\n", UnknownSettings[i].name, UnknownSettings[i].value);

   free(UnknownSettings[i].name);
   free(UnknownSettings[i].value);
  }
 }


 CurrentSettings.clear();	// Call after the list is all handled
 UnknownSettings.clear();
 fclose(fp);
 return(1);
}

static const MDFNCS *FindSetting(const char *name)
{

 assert(false);
 const MDFNCS *ret = NULL;
 uint32 name_hash;

 //printf("Find: %s\n", name);

 name_hash = MakeNameHash(name);

 std::pair<std::multimap <uint32, MDFNCS>::iterator, std::multimap <uint32, MDFNCS>::iterator> sit_pair;
 std::multimap <uint32, MDFNCS>::iterator sit;

 sit_pair = CurrentSettings.equal_range(name_hash);

 for(sit = sit_pair.first; sit != sit_pair.second; sit++)
 {
  //printf("Found: %s\n", sit->second.name);
  if(!strcmp(sit->second.name, name))
  {
   ret = &sit->second;
  }
 }

 if(!ret)
 {
	 assert(false);
  printf("\n\nINCONCEIVABLE!  Setting not found: %s\n\n", name);
  exit(1);
 }
 return(ret);
}

static const char *GetSetting(const MDFNCS *setting)
{
 const char *value;

 if(setting->netplay_override)
  value = setting->netplay_override;
 else
  value = setting->value;

 return(value);
}

static int GetEnum(const MDFNCS *setting, const char *value)
{
 const MDFNSetting_EnumList *enum_list = setting->desc->enum_list;
 int ret = 0;
 bool found = false;

 assert(enum_list);

 do
 {
  if(!strcasecmp(value, enum_list->string))
  {
   found = true;
   ret = enum_list->number;
   break;
  }
 } while((++enum_list)->string);
 assert(found);
 return(ret);
}
/*
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
*/
uint64 MDFN_GetSettingUI(const char *name)
{

	//if(!strcmp("vb.3dmode",name))
	//	return 4;//VB3DMODE_ANAGLYPH
	if(!strcmp("vb.anaglyph.lcolor",name))
		return 0xffba00;
	if(!strcmp("vb.anaglyph.rcolor",name))
		return 0x00baff;
	if(!strcmp("vb.default_color",name))
		return 0xF0F0F0;

 const MDFNCS *setting = FindSetting(name);
 const char *value = GetSetting(setting);

 if((setting->desc->type & MDFNST_BASE_MASK) == MDFNST_ENUM)
  return(GetEnum(setting, value));
 else
 {
  unsigned long long ret;
  TranslateSettingValueUI(value, ret);
  return(ret);
 }
}

int64 MDFN_GetSettingI(const char *name)
{
	if(!strcmp(name,"vb.cpu_emulation"))
		return 1; //1=Accuracy
	if(!strcmp(name,"vb.anaglyph.preset"))
		return 1;

 const MDFNCS *setting = FindSetting(name);
 const char *value = GetSetting(FindSetting(name));


 if((setting->desc->type & MDFNST_BASE_MASK) == MDFNST_ENUM)
  return(GetEnum(setting, value));
 else
 {
  long long ret;
  TranslateSettingValueI(value, ret);
  return(ret);
 }
}

double MDFN_GetSettingF(const char *name)
{ 
	assert(false);
	//TODO
// return(world_strtod(GetSetting(FindSetting(name)), (char **)NULL));
	return 1.0;
}

bool MDFN_GetSettingB(const char *name)
{

	if(!strcmp("dfmd5",name))
		return true;
	if(!strcmp("filesys.sav_samedir",name))
		return true;
	if(!strcmp("filesys.state_samedir",name))
		return true;
	if(!strcmp("filesys.disablesavegz",name))
		return true;
	if(!strcmp("vb.disable_parallax",name))
		return false;
	if(!strcmp("vb.input.instant_read_hack"	,name))
		return false;
	if(!strcmp("cheats"	,name))
		return false;

	return(strtoull(GetSetting(FindSetting(name)), NULL, 10));
}

std::string MDFN_GetSettingS(const char *name)
{
 return(std::string(GetSetting(FindSetting(name))));
}

const std::multimap <uint32, MDFNCS> *MDFNI_GetSettings(void)
{
 return(&CurrentSettings);
}

bool MDFNI_SetSetting(const char *name, const char *value, bool NetplayOverride)
{
 uint32 name_hash;

 name_hash = MakeNameHash(name);

 std::pair<std::multimap <uint32, MDFNCS>::iterator, std::multimap <uint32, MDFNCS>::iterator> sit_pair;
 std::multimap <uint32, MDFNCS>::iterator sit;

 sit_pair = CurrentSettings.equal_range(name_hash);

 for(sit = sit_pair.first; sit != sit_pair.second; sit++)
 {
  if(!strcmp(sit->second.name, name))
  {
   if(!ValidateSetting(value, sit->second.desc))
   {
    return(0);
   }

   // TODO:  When NetplayOverride is set, make sure the setting is an emulation-related setting, 
   // and that it is safe to change it(changing paths to BIOSes and such is not safe :b).
   if(NetplayOverride)
   {
    if(sit->second.netplay_override) 
     free(sit->second.netplay_override);
    sit->second.netplay_override = strdup(value);
   }
   else
   {
    if(sit->second.value) 
     free(sit->second.value);
    sit->second.value = strdup(value);
   }

   // TODO, always call driver notification function, regardless of whether a game is loaded.
   if(sit->second.ChangeNotification)
   {
    if(MDFNGameInfo)
     sit->second.ChangeNotification(name);
   }
   return(1);
  }
 }

 MDFN_PrintError("Unknown setting \"%s\"", name);

 return(0);
}

#if 0
// TODO after a game is loaded, but should we?
void MDFN_CallSettingsNotification(void)
{
 for(unsigned int x = 0; x < CurrentSettings.size(); x++)
 {
  if(CurrentSettings[x].ChangeNotification)
  {
   // TODO, always call driver notification function, regardless of whether a game is loaded.
   if(MDFNGameInfo)
    CurrentSettings[x].ChangeNotification(CurrentSettings[x].name);
  }
 }
}
#endif

bool MDFNI_SetSettingB(const char *name, bool value)
{
 char tmpstr[2];
 tmpstr[0] = value ? '1' : '0';
 tmpstr[1] = 0;

 return(MDFNI_SetSetting(name, tmpstr, FALSE));
}

bool MDFNI_SetSettingUI(const char *name, uint64 value)
{
 char tmpstr[32];

// trio_snprintf(tmpstr, 32, "%llu", value);
 return(MDFNI_SetSetting(name, tmpstr, FALSE));
}

