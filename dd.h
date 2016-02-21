#if USE_DDRAW

#include <windows.h>
#include <ddraw.h>
#include "types.h"

#define _15BIT(r,g,b) (((r&248)<<7) + ((g&248)<<2) + (b>>3))
#define _16BIT(r,g,b) (((r&248)<<8) + ((g&252)<<3) + (b>>3))

typedef  u16 ushort ;

void DirectDrawInit (void);
extern LPDIRECTDRAWSURFACE7	lpPrimary;
extern LPDIRECTDRAWSURFACE7 lpBack;
void LockSurface (LPDIRECTDRAWSURFACE7 surface);
void UnlockSurface (LPDIRECTDRAWSURFACE7 surface);
void Pixel (int x, int y, ushort color);

extern DDSURFACEDESC2			ddsd;
extern ushort *lpscreen;

extern LPDIRECTDRAWCLIPPER		lpDDClipPrimary;
extern LPDIRECTDRAW7			lpdd7;

void ClearDirectDrawOutput();
int CreateDDrawBuffers();
void renderDD();

#endif // #if USE_DDRAW