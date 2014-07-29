//#include <sys/types.h>
#include <stdint.h>

#ifndef TYPES_H
#define TYPES_H

/*typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef __int64 int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;*/
typedef int64_t int64;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32; 

typedef uint8_t uint8;  
typedef uint16_t uint16;
typedef uint32_t uint32;

#define MDFN_FASTCALL 
#define INLINE __forceinline

#pragma warning( disable: 4996 ) //disable "The POSIX name for this item is deprecated"
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define snprintf _snprintf
#define fstat _fstat
#define close _close
#define open _open
#define lseek _lseek
#define read _read

//WTF? Just who in the FUCK would do this?
//Prevents VC2008 from compiling VBJin. Fix involved stripping _() tags from several items.
//#define _ 

#define MDFN_printf printf

typedef struct
{
 int x, y, w, h;
} MDFN_Rect;

#endif

#define TRUE 1
#define FALSE 0

#define MDFNNPCMD_RESET 	0x01
#define MDFNNPCMD_POWER 	0x02

#define dup _dup
#define lseek _lseek
#define stat _stat
#define strtoull _strtoui64
#define strtoll _strtoi64
#define S_ISREG(x) 0

//extern uint16 PadData;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned __int64 u64;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))