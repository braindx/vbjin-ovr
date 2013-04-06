/* Ripped from FCEUX, and altered to work correctly with stereo settings */
#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "waveout.h"

static FILE *soundlog=0;
static long wsize;

/* Checking whether the file exists before wiping it out is left up to the 
   reader..err...I mean, the driver code, if it feels so inclined(I don't feel
   so).
*/
void DRV_WriteWaveData(uint32 *Buffer, int Count)
{
 if(!soundlog) return;

uint32 *temp = (uint32*)alloca(Count*sizeof(uint32));

 uint32 *dest;
 int x;

 dest=temp;
 x=Count;

 //mbg 7/28/06 - we appear to be guaranteeing little endian
 while(x--)
 {
  uint32 tmp=*Buffer;

  *(uint8 *)dest=(((uint32)tmp)&255);
  *(((uint8 *)dest)+1)=((((uint32)tmp)>>8)&255);
  *(((uint8 *)dest)+2)=((((uint32)tmp)>>16)&255); // For single channel, cast input to (uint16*)
  *(((uint8 *)dest)+3)=(((uint32)tmp)>>24);		  // comment out these lines, edit BeginWaveRecord
  dest++;
  Buffer++;
 }
 if(soundlog)
	 wsize+=fwrite(temp,1,Count*sizeof(uint32),soundlog);
}

int DRV_EndWaveRecord()
{
 long s;

 if(!soundlog) return 0;
 s=ftell(soundlog)-8;
 fseek(soundlog,4,SEEK_SET);
 fputc(s&0xFF,soundlog);
 fputc((s>>8)&0xFF,soundlog);
 fputc((s>>16)&0xFF,soundlog);
 fputc((s>>24)&0xFF,soundlog);
 
 fseek(soundlog,0x28,SEEK_SET);
 s=wsize;
 fputc(s&0xFF,soundlog);
 fputc((s>>8)&0xFF,soundlog);
 fputc((s>>16)&0xFF,soundlog);
 fputc((s>>24)&0xFF,soundlog);
 
 fclose(soundlog);
 soundlog=0;
 return 1;
}


bool DRV_BeginWaveRecord(const char *fn)
{
 int r;

 if(!(soundlog=fopen(fn,"wb")))
  return false;
 wsize=0;
 
 /* Write the header. */
 fputs("RIFF",soundlog);
 fseek(soundlog,4,SEEK_CUR);  // Skip size
 fputs("WAVEfmt ",soundlog);

 fputc(0x10,soundlog);
 fputc(0,soundlog);
 fputc(0,soundlog);
 fputc(0,soundlog);

 fputc(1,soundlog);     // PCM
 fputc(0,soundlog);

 fputc(2,soundlog);     // 1 = Monophonic, 2 = Stereo
 fputc(0,soundlog);

 r=44100; // Playback rate
 fputc(r&0xFF,soundlog);
 fputc((r>>8)&0xFF,soundlog);
 fputc((r>>16)&0xFF,soundlog);
 fputc((r>>24)&0xFF,soundlog);
 r*=4; // 2 = Mono, 4 = Stereo. Average Byte Rate.
 fputc(r&0xFF,soundlog);
 fputc((r>>8)&0xFF,soundlog);
 fputc((r>>16)&0xFF,soundlog);
 fputc((r>>24)&0xFF,soundlog);
 fputc(4,soundlog); // 2 = Mono, 4 = Stereo. BlockAlign: (NumChannels * SignificantBitsPerSample / 8)
 fputc(0,soundlog);
 fputc(16,soundlog); //SignificantBitsPerSample
 fputc(0,soundlog);
 
 fputs("data",soundlog);
 fseek(soundlog,4,SEEK_CUR); // Chunk data size skipped

 return true;
}

bool DRV_WaveRecordActive()
{
	return (bool)(soundlog);
}