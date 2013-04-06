#include <assert.h>
#include <limits.h>
#include <fstream>
//#include "utils/guid.h"
#include "utils/xstring.h"
#include "movie.h"
#include "main.h"
#include "pcejin.h"

extern void PCE_Power();

#include "utils/readwrite.h"

using namespace std;
bool freshMovie = false;	  //True when a movie loads, false when movie is altered.  Used to determine if a movie has been altered since opening
bool autoMovieBackup = true;

#define FCEU_PrintError LOG

#define MOVIE_VERSION 1
#define DESMUME_VERSION_NUMERIC 1

//----movie engine main state
extern bool PCE_IsCD;

EMOVIEMODE movieMode = MOVIEMODE_INACTIVE;

//this should not be set unless we are in MOVIEMODE_RECORD!
fstream* osRecordingMovie = 0;

//////////////////////////////////////////////////////////////////////////////

int LagFrameFlag;
int FrameAdvanceVariable=0;
int currFrameCounter;
char MovieStatusM[40];
int LagFrameCounter;
extern "C" {
int AutoAdvanceLag;
}
//////////////////////////////////////////////////////////////////////////////

uint32 cur_input_display = 0;
int pauseframe = -1;
bool movie_readonly = true;

char curMovieFilename[512] = {0};
MovieData currMovieData;
int currRerecordCount;

//--------------
#include "mednafen.h"

extern void DisplayMessage(char* str);

void MovieData::clearRecordRange(int start, int len)
{
	for(int i=0;i<len;i++)
		records[i+start].clear();
}

void MovieData::insertEmpty(int at, int frames)
{
	if(at == -1) 
	{
		int currcount = records.size();
		records.resize(records.size()+frames);
		clearRecordRange(currcount,frames);
	}
	else
	{
		records.insert(records.begin()+at,frames,MovieRecord());
		clearRecordRange(at,frames);
	}
}



void ResetFrameCount() {
	//for loading roms
	currFrameCounter = 0;
}

void MovieRecord::clear()
{ 
	for (int i = 0; i < 5; i++)
		pad[i] = 0;
	commands = 0;
	touch.padding = 0;
}
/*

Table 4.1 - Button Data
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
rdd rdl sel str ldu ldd ldl ldr rdr rdu lbb rbb b a 1 bat
rdx – Right DPad, where x is Up, Down, Left, Right
ldx – Left DPad, where x is Up, Down, Left, Right
sel – Select
str – Start
lbb, rbb – Left/Right Button on back of controller
bat – Battery low, may flicker so test multiple times.
*/

//const char MovieRecord::mnemonics[13] = {'R','L','D','U','T','S','B','A','Y','X','W','E','G'};
//const char MovieRecord::mnemonics[8] = {'U','D','L','R','1','2','N','S'};
//const char MovieRecord::mnemonics[14] = {'U', 'D', 'L', 'R', 'N', 'S', 'E', 'W', 'A', 'B', 'C', 'T', 'F', 'G'};


const char MovieRecord::mnemonics[14] = {'S', 'W', 'C', 'T', '^', 'v', '<', '>', 'E', 'N', 'L', 'R', 'B', 'A'};
void MovieRecord::dumpPad(std::ostream* os, uint16 pad)
{
	//these are mnemonics for each joystick bit.
	//since we usually use the regular joypad, these will be more helpful.
	//but any character other than ' ' or '.' should count as a set bit
	//maybe other input types will need to be encoded another way..
	for(int bit=0;bit<14;bit++)
	{
		int bitmask = (1<<(13-bit));
		char mnemonic = mnemonics[bit];
		//if the bit is set write the mnemonic
		if(pad & bitmask)
			os->put(mnemonic);
		else //otherwise write an unset bit
			os->put('.');
	}
}
#undef read
#undef open
#undef close
#define read read
#define open open
#define close close


void MovieRecord::parsePad(std::istream* is, uint16& pad)
{
	char buf[14];
	is->read(buf,14);
	pad = 0;
	for(int i=0;i<14;i++)
	{
		pad <<= 1;
		pad |= ((buf[i]=='.'||buf[i]==' ')?0:1);
	}
}


void MovieRecord::parse(MovieData* md, std::istream* is)
{
	//by the time we get in here, the initial pipe has already been extracted

	//extract the commands
	commands = u32DecFromIstream(is);
	
	is->get(); //eat the pipe

	for (int i = 0; i < md->ports; i++) {
		parsePad(is, pad[i]);
		is->get();
	}
//	touch.x = u32DecFromIstream(is);
//	touch.y = u32DecFromIstream(is);
//	touch.touch = u32DecFromIstream(is);
		
	is->get(); //eat the pipe

	//should be left at a newline
}


void MovieRecord::dump(MovieData* md, std::ostream* os, int index)
{
	//dump the misc commands
	//*os << '|' << setw(1) << (int)commands;
	os->put('|');
	putdec<uint8,1,true>(os,commands);

	os->put('|');
//	dumpPad(os, pad);
	for (int i = 0; i < md->ports; i++) 
	{
		dumpPad(os,pad[i]); 
		os->put('|');
	}
//	putdec<u8,3,true>(os,touch.x); os->put(' ');
//	putdec<u8,3,true>(os,touch.y); os->put(' ');
//	putdec<u8,1,true>(os,touch.touch);
//	os->put('|');
	
	//each frame is on a new line
	os->put('\n');
}

MovieData::MovieData()
	: version(MOVIE_VERSION)
	, emuVersion(DESMUME_VERSION_NUMERIC)
	, rerecordCount(1)
	, binaryFlag(false)
	//, greenZoneCount(0)
{
//	memset(&romChecksum,0,sizeof(MD5DATA));
}

void MovieData::truncateAt(int frame)
{
	records.resize(frame);
}


void MovieData::installValue(std::string& key, std::string& val)
{
	//todo - use another config system, or drive this from a little data structure. because this is gross
	if(key == "PCECD")
		installInt(val,pcecd);
	else if(key == "version")
		installInt(val,version);
	else if(key == "emuVersion")
		installInt(val,emuVersion);
	else if(key == "rerecordCount")
		installInt(val,rerecordCount);
	else if(key == "romFilename")
		romFilename = val;
	else if(key == "ports")
		installInt(val, ports);
//	else if(key == "romChecksum")
//		StringToBytes(val,&romChecksum,MD5DATA::size);
//	else if(key == "guid")
//		guid = Desmume_Guid::fromString(val);
	else if(key == "comment")
		comments.push_back(val);
	else if(key == "binary")
		installBool(val,binaryFlag);
	else if(key == "savestate")
	{
		int len = Base64StringToBytesLength(val);
		if(len == -1) len = HexStringToBytesLength(val); // wasn't base64, try hex
		if(len >= 1)
		{
			savestate.resize(len);
			StringToBytes(val,&savestate[0],len); // decodes either base64 or hex
		}
	}
}

char* RemovePath(char * input) {

	char* temp=(char*)malloc(1024);
	strcpy(temp, input);

	if (strrchr(temp, '/'))
        temp = 1 + strrchr(temp, '/');

	if (strrchr(temp, '\\'))
        temp = 1 + strrchr(temp, '\\');

	return temp;
}


int MovieData::dump(std::ostream *os, bool binary)
{
	int start = (int)os->tellp();
	*os << "version " << version << endl;
	*os << "emulator " << "VBjin" << endl;
	*os << "emuVersion " << emuVersion << endl;
	*os << "rerecordCount " << rerecordCount << endl;
	*os << "ports " << ports << endl;
/*	*os << "cdGameName " << cdip->gamename << endl;
	*os << "cdInfo " << cdip->cdinfo << endl;
	*os << "cdItemNum " << cdip->itemnum << endl;
	*os << "cdVersion " << cdip->version << endl;
	*os << "cdDate " << cdip->date << endl;
	*os << "cdRegion " << cdip->region << endl;
	*os << "emulatedBios " << yabsys.emulatebios << endl;
	*os << "isPal " << yabsys.IsPal << endl;
#ifdef WIN32
	*os << "sh2CoreType " << int(sh2coretype) << endl;
	*os << "sndCoreType " << int(sndcoretype) << endl;
	*os << "vidCoreType " << int(vidcoretype) << endl;
	*os << "cartType " << carttype << endl;

	*os << "cdRomPath " << RemovePath(cdrompath) << endl;
	*os << "biosFilename " << RemovePath(biosfilename) << endl;
	*os << "cartFilename " << RemovePath(cartfilename) << endl;
#endif
*/
//	fwrite("YMV", sizeof("YMV"), 1, fp);
//	fwrite(VERSION, sizeof(VERSION), 1, fp);

	////
//	*os << "romChecksum " << BytesToString(romChecksum.data,MD5DATA::size) << endl;
//	*os << "guid " << guid.toString() << endl;

	for(u32 i=0;i<comments.size();i++)
		*os << "comment " << comments[i] << endl;
	
	if(binary)
		*os << "binary 1" << endl;
		
	if(savestate.size() != 0)
		*os << "savestate " << BytesToString(&savestate[0],savestate.size()) << endl;
	if(binary)
	{
		//put one | to start the binary dump
		os->put('|');
		for(int i=0;i<(int)records.size();i++)
			records[i].dumpBinary(this,os,i);
	}
	else
		for(int i=0;i<(int)records.size();i++)
			records[i].dump(this,os,i);

	int end = (int)os->tellp();
	return end-start;
}

//yuck... another custom text parser.
bool LoadFM2(MovieData& movieData, std::istream* fp, int size, bool stopAfterHeader)
{
	//TODO - start with something different. like 'desmume movie version 1"
	std::ios::pos_type curr = fp->tellg();

	//movie must start with "version 1"
	char buf[9];
	curr = fp->tellg();
	fp->read(buf,9);
	fp->seekg(curr);
	if(fp->fail()) return false;
	if(memcmp(buf,"version 1",9)) 
		return false;

	std::string key,value;
	enum {
		NEWLINE, KEY, SEPARATOR, VALUE, RECORD, COMMENT
	} state = NEWLINE;
	bool bail = false;
	for(;;)
	{
		bool iswhitespace, isrecchar, isnewline;
		int c;
		if(size--<=0) goto bail;
		c = fp->get();
		if(c == -1)
			goto bail;
		iswhitespace = (c==' '||c=='\t');
		isrecchar = (c=='|');
		isnewline = (c==10||c==13);
		if(isrecchar && movieData.binaryFlag && !stopAfterHeader)
		{
			LoadFM2_binarychunk(movieData, fp, size);
			return true;
		}
		switch(state)
		{
		case NEWLINE:
			if(isnewline) goto done;
			if(iswhitespace) goto done;
			if(isrecchar) 
				goto dorecord;
			//must be a key
			key = "";
			value = "";
			goto dokey;
			break;
		case RECORD:
			{
				dorecord:
				if (stopAfterHeader) return true;
				int currcount = movieData.records.size();
				movieData.records.resize(currcount+1);
				int preparse = (int)fp->tellg();
				movieData.records[currcount].parse(&movieData, fp);
				int postparse = (int)fp->tellg();
				size -= (postparse-preparse);
				state = NEWLINE;
				break;
			}

		case KEY:
			dokey: //dookie
			state = KEY;
			if(iswhitespace) goto doseparator;
			if(isnewline) goto commit;
			key += c;
			break;
		case SEPARATOR:
			doseparator:
			state = SEPARATOR;
			if(isnewline) goto commit;
			if(!iswhitespace) goto dovalue;
			break;
		case VALUE:
			dovalue:
			state = VALUE;
			if(isnewline) goto commit;
			value += c;
			break;
//		case COMMENT:
		default:
			break;
		}
		goto done;

		bail:
		bail = true;
		if(state == VALUE) goto commit;
		goto done;
		commit:
		movieData.installValue(key,value);
		state = NEWLINE;
		done: ;
		if(bail) break;
	}

	return true;
}


static void closeRecordingMovie()
{
	if(osRecordingMovie)
	{
		delete osRecordingMovie;
		osRecordingMovie = 0;
	}
}

/// Stop movie playback.
static void StopPlayback()
{
	DisplayMessage("Movie playback stopped.");
	strcpy(MovieStatusM, "");
	movieMode = MOVIEMODE_INACTIVE;
}


/// Stop movie recording
static void StopRecording()
{
	DisplayMessage("Movie recording stopped.");
	strcpy(MovieStatusM, "");
	movieMode = MOVIEMODE_INACTIVE;
	
	closeRecordingMovie();
}



void FCEUI_StopMovie()
{
	if(movieMode == MOVIEMODE_PLAY)
		StopPlayback();
	else if(movieMode == MOVIEMODE_RECORD)
		StopRecording();

	curMovieFilename[0] = 0;
	freshMovie = false;
}
	extern void PCE_Power(void);

extern uint8 SaveRAM[2048];
#include "vb.h"

void ClearPCESRAM(void) {
//NEWTODO
	MDFN_IEN_VB::clearGPRAM();

//  memset(SaveRAM, 0x00, 2048);
//  memcpy(SaveRAM, "HUBM\x00\xa0\x10\x80", 8);  

}

//begin playing an existing movie
void FCEUI_LoadMovie(const char *fname, bool _read_only, bool tasedit, int _pauseframe)
{
	//if(!tasedit && !FCEU_IsValidUI(FCEUI_PLAYMOVIE))
	//	return;

	assert(fname);

	//mbg 6/10/08 - we used to call StopMovie here, but that cleared curMovieFilename and gave us crashes...
	if(movieMode == MOVIEMODE_PLAY)
		StopPlayback();
	else if(movieMode == MOVIEMODE_RECORD)
		StopRecording();
	//--------------

	currMovieData = MovieData();
	
	strcpy(curMovieFilename, fname);
	//FCEUFILE *fp = FCEU_fopen(fname,0,"rb",0);
	//if (!fp) return;
	//if(fp->isArchive() && !_read_only) {
	//	FCEU_PrintError("Cannot open a movie in read+write from an archive.");
	//	return;
	//}

	//LoadFM2(currMovieData, fp->stream, INT_MAX, false);

	currMovieData.ports = 1;	
	
	fstream fs (fname);
	LoadFM2(currMovieData, &fs, INT_MAX, false);
	fs.close();

	//TODO
	//fully reload the game to reinitialize everything before playing any movie
	//poweron(true);

//	extern bool _HACK_DONT_STOPMOVIE;
//	_HACK_DONT_STOPMOVIE = true;
//	extern void VB_Power(void);

	MDFN_IEN_VB::VB_Power();
//	_HACK_DONT_STOPMOVIE = false;
	////WE NEED TO LOAD A SAVESTATE
	//if(currMovieData.savestate.size() != 0)
	//{
	//	bool success = MovieData::loadSavestateFrom(&currMovieData.savestate);
	//	if(!success) return;
	//}
	
	pcejin.lagFrameCounter = 0;
	pcejin.isLagFrame = false;
	currFrameCounter = 0;
	pauseframe = _pauseframe;
	movie_readonly = _read_only;
	movieMode = MOVIEMODE_PLAY;
	currRerecordCount = currMovieData.rerecordCount;
	ClearPCESRAM();
//	InitMovieTime();
	freshMovie = true;
//	ClearAutoHold();

	if(movie_readonly)
		DisplayMessage("Replay started Read-Only.");
	//	driver->USR_InfoMessage("Replay started Read-Only.");
	else
		DisplayMessage("Replay started Read+Write.");
	//	driver->USR_InfoMessage("Replay started Read+Write.");
}

static void openRecordingMovie(const char* fname)
{
	//osRecordingMovie = FCEUD_UTF8_fstream(fname, "wb");
	osRecordingMovie = new fstream(fname,std::ios_base::out);
	/*if(!osRecordingMovie)
		FCEU_PrintError("Error opening movie output file: %s",fname);*/
	strcpy(curMovieFilename, fname);
}


//begin recording a new movie
//TODO - BUG - the record-from-another-savestate doesnt work.
 void FCEUI_SaveMovie(const char *fname, std::string author, int controllers)
{
	//if(!FCEU_IsValidUI(FCEUI_RECORDMOVIE))
	//	return;

	assert(fname);

	FCEUI_StopMovie();

	openRecordingMovie(fname);

	currFrameCounter = 0;
	//LagCounterReset();

	currMovieData = MovieData();
//	currMovieData.guid.newGuid();

	if(author != "") currMovieData.comments.push_back("author " + author);
	//currMovieData.romChecksum = GameInfo->MD5;
//	currMovieData.romFilename = cdip->gamename;//GetRomName();
	
//	extern bool _HACK_DONT_STOPMOVIE;
//	_HACK_DONT_STOPMOVIE = true;
	pcejin.lagFrameCounter = 0;
	pcejin.isLagFrame = false;
MDFN_IEN_VB::VB_Power();
//	_HACK_DONT_STOPMOVIE = false;
	currMovieData.ports = controllers;

	//todo ?
	//poweron(true);
	//else
	//	MovieData::dumpSavestateTo(&currMovieData.savestate,Z_BEST_COMPRESSION);

	//we are going to go ahead and dump the header. from now on we will only be appending frames
	currMovieData.dump(osRecordingMovie, false);

	movieMode = MOVIEMODE_RECORD;
	movie_readonly = false;
	currRerecordCount = 0;
//	InitMovieTime();
	ClearPCESRAM();
//	BupFormat(0);
	LagFrameCounter=0;

	DisplayMessage("Movie recording started.");
//	driver->USR_InfoMessage("Movie recording started.");
}

 void NDS_setTouchFromMovie(void) {

/*	 if(movieMode == MOVIEMODE_PLAY)
	 {

		 MovieRecord* mr = &currMovieData.records[currFrameCounter];
		 nds.touchX=mr->touch.x << 4;
		 nds.touchY=mr->touch.y << 4;

		 if(mr->touch.touch) {
			 nds.isTouch=mr->touch.touch;
			 MMU.ARM7_REG[0x136] &= 0xBF;
		 }
		 else {
			 nds.touchX=0;
			 nds.touchY=0;
			 nds.isTouch=0;

			 MMU.ARM7_REG[0x136] |= 0x40;
		 }
		 osd->addFixed(mr->touch.x, mr->touch.y, "%s", "X");
	 }*/
 }
#define MDFNNPCMD_RESET 	0x01
#define MDFNNPCMD_POWER 	0x02

uint16 pcepad;

void NDS_setPadFromMovie(uint16 pad[])
{
	for (int i = 0; i < currMovieData.ports; i++) {
	pcepad = 0;

	for (int j = 0; j < 14; j++) {
		if(pad[i] &(1 << j)) 
			pcepad |= (1 << j);//u
	}
/*
	if(pad[i] &(1 << 7)) 
		pcepad |= (1 << 4);//u
	if(pad[i] &(1 << 6)) pcepad |= (1 << 6);//d
	if(pad[i] &(1 << 5)) pcepad |= (1 << 7);//l
	if(pad[i] &(1 << 4)) pcepad |= (1 << 5);//r
	if(pad[i] &(1 << 3)) pcepad |= (1 << 0);//o
	if(pad[i] &(1 << 2)) pcepad |= (1 << 1);//t
	if(pad[i] &(1 << 0)) pcepad |= (1 << 2);//s
	if(pad[i] &(1 << 1)) pcepad |= (1 << 3);//n
*/
	pcejin.pads[i] = pcepad;
	}

}
 static int _currCommand = 0;
extern void *PortDataCache[16];
 //the main interaction point between the emulator and the movie system.
 //either dumps the current joystick state or loads one state from the movie
void FCEUMOV_AddInputState()
 {
//	 uint16 pad;
	 //todo - for tasedit, either dump or load depending on whether input recording is enabled
	 //or something like that
	 //(input recording is just like standard read+write movie recording with input taken from gamepad)
	 //otherwise, it will come from the tasedit data.

	 if(LagFrameFlag == 1)
		 LagFrameCounter++;
	 LagFrameFlag=1;

	 if(movieMode == MOVIEMODE_PLAY)
	 {
		 //stop when we run out of frames
		 if(currFrameCounter >= (int)currMovieData.records.size())
		 {
			 StopPlayback();
		 }
		 else
		 {

			 strcpy(MovieStatusM, "Playback");

			 MovieRecord* mr = &currMovieData.records[currFrameCounter];

			 //reset if necessary
			 if(mr->command_reset())
				 MDFN_IEN_VB::VB_Power();


			 if(mr->command_power())
				MDFN_IEN_VB::VB_Power();

			// {}
			 //ResetNES();
			 NDS_setPadFromMovie(mr->pad);
//			 NDS_setTouchFromMovie();
		 }

		 //if we are on the last frame, then pause the emulator if the player requested it
		 if(currFrameCounter == (int)currMovieData.records.size()-1)
		 {
			 /*if(FCEUD_PauseAfterPlayback())
			 {
			 FCEUI_ToggleEmulationPause();
			 }*/
		 }

		 //pause the movie at a specified frame 
		 //if(FCEUMOV_ShouldPause() && FCEUI_EmulationPaused()==0)
		 //{
		 //	FCEUI_ToggleEmulationPause();
		 //	FCEU_DispMessage("Paused at specified movie frame");
		 //}
//		 osd->addFixed(180, 176, "%s", "Playback");

	 }
	 else if(movieMode == MOVIEMODE_RECORD)
	 {
		 MovieRecord mr;
		mr.commands = _currCommand;
		_currCommand = 0;

	//	 mr.commands = 0;

		 strcpy(MovieStatusM, "Recording");

		 //int II, I, n, s, u, r, d, l;

	//	 for (int i = 0; i < currMovieData.ports; i++) {

			 pcepad = pcejin.pads[0];
			 mr.pad[0] = pcepad;

	//		 assert(false);

			 //wtf is this crap
/*
#define FIX(b) (b?1:0)
			 II = FIX(pcepad &(1 << 1));
			 I = FIX(pcepad & (1 << 0));
			 n = FIX(pcepad & (1 << 2));
			 s = FIX(pcepad & (1 << 3));
			 u = FIX(pcepad & (1 << 4));
			 r = FIX(pcepad & (1 << 7));
			 d = FIX(pcepad & (1 << 6));
			 l = FIX(pcepad & (1 << 5));

			 mr.pad[i] =
				 (FIX(r)<<5)|
				 (FIX(l)<<4)|
				 (FIX(u)<<7)|
				 (FIX(d)<<6)|
				 (FIX(I)<<3)|
				 (FIX(II)<<2)|
				 (FIX(s)<<1)|
				 (FIX(n)<<0);
				 */
	//	 }

		 mr.dump(&currMovieData, osRecordingMovie,currMovieData.records.size());
		 currMovieData.records.push_back(mr);
//		 osd->addFixed(180, 176, "%s", "Recording");
	 }

	 currFrameCounter++;

	 /*extern u8 joy[4];
	 memcpy(&cur_input_display,joy,4);*/
 }


//TODO 
static void FCEUMOV_AddCommand(int cmd)
{
	// do nothing if not recording a movie
	if(movieMode != MOVIEMODE_RECORD)
		return;
	
	//printf("%d\n",cmd);

	//MBG TODO BIG TODO TODO TODO
	//DoEncode((cmd>>3)&0x3,cmd&0x7,1);
}

//little endian 4-byte cookies
static const int kMOVI = 0x49564F4D;
static const int kNOMO = 0x4F4D4F4E;

#define LOCAL_LE

int read32lemov(int *Bufo, std::istream *is)
{
	uint32 buf;
	if(is->read((char*)&buf,4).gcount() != 4)
		return 0;
#ifdef LOCAL_LE
	*(uint32*)Bufo=buf;
#else
	*(u32*)Bufo=((buf&0xFF)<<24)|((buf&0xFF00)<<8)|((buf&0xFF0000)>>8)|((buf&0xFF000000)>>24);
#endif
	return 1;
}


int write32lemov(uint32 b, std::ostream* os)
{
	uint8 s[4];
	s[0]=b;
	s[1]=b>>8;
	s[2]=b>>16;
	s[3]=b>>24;
	os->write((char*)&s,4);
	return 4;
}

void mov_savestate(std::ostream *os)//std::ostream* os
{
	//we are supposed to dump the movie data into the savestate
	//if(movieMode == MOVIEMODE_RECORD || movieMode == MOVIEMODE_PLAY)
	//	return currMovieData.dump(os, true);
	//else return 0;
	if(movieMode == MOVIEMODE_RECORD || movieMode == MOVIEMODE_PLAY)
	{
		write32lemov(kMOVI,os);
		currMovieData.dump(os, true);
	}
	else
	{
		write32lemov(kNOMO,os);
	}
}

static bool load_successful;

bool mov_loadstate(std::istream* is, int size)//std::istream* is
{
	load_successful = false;

	int cookie;
	if(read32lemov(&cookie,is) != 1) return false;
	if(cookie == kNOMO)
		return true;
	else if(cookie != kMOVI)
		return false;

	size -= 4;

	if (!movie_readonly && autoMovieBackup && freshMovie) //If auto-backup is on, movie has not been altered this session and the movie is in read+write mode
	{
		FCEUI_MakeBackupMovie(false);	//Backup the movie before the contents get altered, but do not display messages						  
	}

	//a little rule: cant load states in read+write mode with a movie from an archive.
	//so we are going to switch it to readonly mode in that case
//	if(!movie_readonly 
//		//*&& FCEU_isFileInArchive(curMovieFilename)*/
//		) {
//		FCEU_PrintError("Cannot loadstate in Read+Write with movie from archive. Movie is now Read-Only.");
//		movie_readonly = true;
//	}

	MovieData tempMovieData = MovieData();
	std::ios::pos_type curr = is->tellg();
	if(!LoadFM2(tempMovieData, is, size, false)) {
		
	//	is->seekg((u32)curr+size);
	/*	extern bool FCEU_state_loading_old_format;
		if(FCEU_state_loading_old_format) {
			if(movieMode == MOVIEMODE_PLAY || movieMode == MOVIEMODE_RECORD) {
				FCEUI_StopMovie();			
				FCEU_PrintError("You have tried to use an old savestate while playing a movie. This is unsupported (since the old savestate has old-format movie data in it which can't be converted on the fly)");
			}
		}*/
		return false;
	}

	//complex TAS logic for when a savestate is loaded:
	//----------------
	//if we are playing or recording and toggled read-only:
	//  then, the movie we are playing must match the guid of the one stored in the savestate or else error.
	//  the savestate is assumed to be in the same timeline as the current movie.
	//  if the current movie is not long enough to get to the savestate's frame#, then it is an error. 
	//  the movie contained in the savestate will be discarded.
	//  the emulator will be put into play mode.
	//if we are playing or recording and toggled read+write
	//  then, the movie we are playing must match the guid of the one stored in the savestate or else error.
	//  the movie contained in the savestate will be loaded into memory
	//  the frames in the movie after the savestate frame will be discarded
	//  the in-memory movie will have its rerecord count incremented
	//  the in-memory movie will be dumped to disk as fcm.
	//  the emulator will be put into record mode.
	//if we are doing neither:
	//  then, we must discard this movie and just load the savestate


	if(movieMode == MOVIEMODE_PLAY || movieMode == MOVIEMODE_RECORD)
	{
		//handle moviefile mismatch
/*		if(tempMovieData.guid != currMovieData.guid)
		{
			//mbg 8/18/08 - this code  can be used to turn the error message into an OK/CANCEL
			#ifdef WIN32
				//std::string msg = "There is a mismatch between savestate's movie and current movie.\ncurrent: " + currMovieData.guid.toString() + "\nsavestate: " + tempMovieData.guid.toString() + "\n\nThis means that you have loaded a savestate belonging to a different movie than the one you are playing now.\n\nContinue loading this savestate anyway?";
				//extern HWND pwindow;
				//int result = MessageBox(pwindow,msg.c_str(),"Error loading savestate",MB_OKCANCEL);
				//if(result == IDCANCEL)
				//	return false;
			#else
				FCEU_PrintError("Mismatch between savestate's movie and current movie.\ncurrent: %s\nsavestate: %s\n",currMovieData.guid.toString().c_str(),tempMovieData.guid.toString().c_str());
				return false;
			#endif
		}*/

		closeRecordingMovie();

		if(movie_readonly)
		{
			//-------------------------------------------------------------
			//this code would reload the movie from disk. allegedly it is helpful to hexers, but
			//it is way too slow with dsm format. so it is only here as a reminder, and in case someone
			//wants to play with it
			//-------------------------------------------------------------
			//{
			//	fstream fs (curMovieFilename);
			//	if(!LoadFM2(tempMovieData, &fs, INT_MAX, false))
			//	{
			//		FCEU_PrintError("Failed to reload DSM after loading savestate");
			//	}
			//	fs.close();
			//	currMovieData = tempMovieData;
			//}
			//-------------------------------------------------------------

			//if the frame counter is longer than our current movie, then error
			if(currFrameCounter > (int)currMovieData.records.size())
			{
//				FCEU_PrintError("Savestate is from a frame (%d) after the final frame in the movie (%d). This is not permitted.", currFrameCounter, currMovieData.records.size()-1);
				return false;
			}

			movieMode = MOVIEMODE_PLAY;
		}
		else
		{
			//truncate before we copy, just to save some time
			tempMovieData.truncateAt(currFrameCounter);
			currMovieData = tempMovieData;
			
		//	#ifdef _S9XLUA_H
		//	if(!FCEU_LuaRerecordCountSkip())
				currRerecordCount++;
		//	#endif
			
			currMovieData.rerecordCount = currRerecordCount;

			openRecordingMovie(curMovieFilename);
			//printf("DUMPING MOVIE: %d FRAMES\n",currMovieData.records.size());
			currMovieData.dump(osRecordingMovie, false);
			movieMode = MOVIEMODE_RECORD;
		}
	}
	
	load_successful = true;
	freshMovie = false;

	//// Maximus: Show the last input combination entered from the
	//// movie within the state
	//if(current!=0) // <- mz: only if playing or recording a movie
	//	memcpy(&cur_input_display, joop, 4);

	return true;
}

void SaveStateMovie(std::string filename) {

	filename+="mov";
//	strcat (filename, "mov");
	filebuf fb;
	fb.open (filename.c_str(), ios::out | ios::binary);//ios::out
	ostream os(&fb);
	mov_savestate(&os);
	fb.close();
}

void LoadStateMovie(char* filename) {

	std::string fname = (std::string)filename + "mov";

    FILE * fp = fopen( fname.c_str(), "r" );
	if(!fp)
		return;
	
	fseek( fp, 0, SEEK_END );
	int size = ftell(fp);
	fclose(fp);

	filebuf fb;
	fb.open (fname.c_str(), ios::in | ios::binary);//ios::in
	istream is(&fb);
	mov_loadstate(&is, size);
	fb.close();
}

static void FCEUMOV_PreLoad(void)
{
	load_successful=0;
}

static bool FCEUMOV_PostLoad(void)
{
	if(movieMode == MOVIEMODE_INACTIVE)
		return true;
	else
		return load_successful;
}


bool FCEUI_MovieGetInfo(std::istream* fp, MOVIE_INFO& info, bool skipFrameCount)
{
	//MovieData md;
	//if(!LoadFM2(md, fp, INT_MAX, skipFrameCount))
	//	return false;
	//
	//info.movie_version = md.version;
	//info.poweron = md.savestate.size()==0;
	//info.pal = md.palFlag;
	//info.nosynchack = true;
	//info.num_frames = md.records.size();
	//info.md5_of_rom_used = md.romChecksum;
	//info.emu_version_used = md.emuVersion;
	//info.name_of_rom_used = md.romFilename;
	//info.rerecord_count = md.rerecordCount;
	//info.comments = md.comments;

	return true;
}

bool MovieRecord::parseBinary(MovieData* md, std::istream* is)
{
	commands=is->get();
		
	for (int i = 0; i < md->ports; i++)
	{
		is->read((char *) &pad[i], sizeof pad[i]);

	//	parseJoy(is,pad[0]); is->get(); //eat the pipe
	//	parseJoy(is,joysticks[1]); is->get(); //eat the pipe
//		parseJoy(is,joysticks[2]); is->get(); //eat the pipe
//		parseJoy(is,joysticks[3]); is->get(); //eat the pipe
	}
//	else
//		is->read((char *) &pad, sizeof pad);
//	is->read((char *) &touch.x, sizeof touch.x);
//	is->read((char *) &touch.y, sizeof touch.y);
//	is->read((char *) &touch.touch, sizeof touch.touch);
	return true;
}


void MovieRecord::dumpBinary(MovieData* md, std::ostream* os, int index)
{
	os->put(md->records[index].commands);
	for (int i = 0; i < md->ports; i++) {
	os->write((char *) &md->records[index].pad[i], sizeof md->records[index].pad[i]);
	}
//	os->write((char *) &md->records[index].touch.x, sizeof md->records[index].touch.x);
//	os->write((char *) &md->records[index].touch.y, sizeof md->records[index].touch.y);
//	os->write((char *) &md->records[index].touch.touch, sizeof md->records[index].touch.touch);
}

void LoadFM2_binarychunk(MovieData& movieData, std::istream* fp, int size)
{
	int recordsize = 1; //1 for the command

	recordsize = 3;

	assert(size%3==0);

	//find out how much remains in the file
	int curr = (int)fp->tellg();
	fp->seekg(0,std::ios::end);
	int end = (int)fp->tellg();
	int flen = end-curr;
	fp->seekg(curr,std::ios::beg);

#undef min
	//the amount todo is the min of the limiting size we received and the remaining contents of the file
	int todo = std::min(size, flen);

	int numRecords = todo/recordsize;
	//printf("LOADED MOVIE: %d records; currFrameCounter: %d\n",numRecords,currFrameCounter);
	movieData.records.resize(numRecords);
	for(int i=0;i<numRecords;i++)
	{
		movieData.records[i].parseBinary(&movieData,fp);
	}
}

#include <sstream>

static bool CheckFileExists(const char* filename)
{
	//This function simply checks to see if the given filename exists
	string checkFilename; 

	if (filename)
		checkFilename = filename;
			
	//Check if this filename exists
	fstream test;
	test.open(checkFilename.c_str(),fstream::in);
		
	if (test.fail())
	{
		test.close();
		return false; 
	}
	else
	{
		test.close();
		return true; 
	}
}

void FCEUI_MakeBackupMovie(bool dispMessage)
{
	//This function generates backup movie files
	string currentFn;					//Current movie fillename
	string backupFn;					//Target backup filename
	string tempFn;						//temp used in back filename creation
	stringstream stream;
	int x;								//Temp variable for string manip
	bool exist = false;					//Used to test if filename exists
	bool overflow = false;				//Used for special situation when backup numbering exceeds limit

	currentFn = curMovieFilename;		//Get current moviefilename
	backupFn = curMovieFilename;		//Make backup filename the same as current moviefilename
	x = backupFn.find_last_of(".");		 //Find file extension
	backupFn = backupFn.substr(0,x);	//Remove extension
	tempFn = backupFn;					//Store the filename at this point
	for (unsigned int backNum=0;backNum<999;backNum++) //999 = arbituary limit to backup files
	{
		stream.str("");					 //Clear stream
		if (backNum > 99)
			stream << "-" << backNum;	 //assign backNum to stream
		 else if (backNum <= 99 && backNum >= 10)
			stream << "-0" << backNum;      //Make it 010, etc if two digits
		else
			stream << "-00" << backNum;	 //Make it 001, etc if single digit
		backupFn.append(stream.str());	 //add number to bak filename
		backupFn.append(".bak");		 //add extension

		exist = CheckFileExists(backupFn.c_str());	//Check if file exists
		
		if (!exist) 
			break;						//Yeah yeah, I should use a do loop or something
		else
		{
			backupFn = tempFn;			//Before we loop again, reset the filename
			
			if (backNum == 999)			//If 999 exists, we have overflowed, let's handle that
			{
				backupFn.append("-001.bak"); //We are going to simply overwrite 001.bak
				overflow = true;		//Flag that we have exceeded limit
				break;					//Just in case
			}
		}
	}

	MovieData md = currMovieData;								//Get current movie data
	std::fstream* outf = new fstream(backupFn.c_str(),std::ios_base::out); //FCEUD_UTF8_fstream(backupFn, "wb");	//open/create file
	md.dump(outf,false);										//dump movie data
	delete outf;												//clean up, delete file object
	
	//TODO, decide if fstream successfully opened the file and print error message if it doesn't

	if (dispMessage)	//If we should inform the user 
	{
//		if (overflow)
//			FCEUI_DispMessage("Backup overflow, overwriting %s",backupFn.c_str()); //Inform user of overflow
//		else
//			FCEUI_DispMessage("%s created",backupFn.c_str()); //Inform user of backup filename
	}
}

void ToggleReadOnly() {

	movie_readonly ^=1;

	if(movie_readonly==1) DisplayMessage("Movie is now read only.");
	else DisplayMessage("Movie is now read+write.");

}

int MovieIsActive() {

	if(movieMode == MOVIEMODE_INACTIVE)
		return false;
	
	return true;
}

void MakeMovieStateName(const char *filename) {

	if(movieMode != MOVIEMODE_INACTIVE)
		strcat ((char *)filename, "movie");
}

void FCEUI_MoviePlayFromBeginning(void)
{
	if (movieMode != MOVIEMODE_INACTIVE)
	{
		char *fname = strdup(curMovieFilename);
		FCEUI_LoadMovie(fname, true, false, 0);
		MDFN_DispMessage("Movie is now Read-Only. Playing from beginning.");
		free(fname);
	}
}