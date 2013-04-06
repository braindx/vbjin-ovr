#ifndef __D3D_H_
#define __D3D_H_

#if USE_D3D

bool D3DInit();
void render();
void SetupOculus( bool warnIfNotFound );

#endif // #if USE_D3D

#endif // #ifndef __D3D_H_