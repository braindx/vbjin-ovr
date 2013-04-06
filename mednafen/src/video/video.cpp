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

#include "video-common.h"

#include <sys/types.h>
#include <sys/stat.h>
//#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#include <trio/trio.h>

#include "png.h"
#include "assert.h"

void MDFNI_SaveSnapshot(const MDFN_Surface *src, const MDFN_Rect *DisplayRect)
{
 try
 {
  FILE *pp=NULL;
  std::string fn;
  int u;

  if(!(pp = fopen(MDFN_MakeFName(MDFNMKF_SNAP_DAT, 0, NULL).c_str(), "rb")))
   u = 0;
  else
  {
   if(trio_fscanf(pp, "%d", &u) == 0)
    u = 0;
   fclose(pp);
  }

  if(!(pp = fopen(MDFN_MakeFName(MDFNMKF_SNAP_DAT, 0, NULL).c_str(), "wb")))
   throw(0);

  fseek(pp, 0, SEEK_SET);
  trio_fprintf(pp, "%d\n", u + 1);
  fclose(pp);

  fn = MDFN_MakeFName(MDFNMKF_SNAP, u, "png");

  if(!MDFN_SavePNGSnapshot(fn.c_str(), src, DisplayRect))
   throw(0);

  MDFN_DispMessage("Screen snapshot %d saved.", u);
 }
 // Compiler says this is unreferenced.
 catch(int x)
 {
  MDFN_DispMessage("Error saving screen snapshot.", x);
 }
}
#include "GPU_osd.h"
void MDFN_DispMessage(const char *format, ...)
{
 va_list ap;
 va_start(ap,format);
 char *msg = NULL;

 trio_vasprintf(&msg, format,ap);
 va_end(ap);

 osd->addLine("%s", msg);

// MDFND_DispMessage(msg);//(UTF8*)
}

void MDFN_ResetMessages(void)
{
// MDFND_DispMessage(NULL);
}

MDFN_Surface::MDFN_Surface(uint32 *p_pixels, uint32 p_width, uint32 p_height, uint32 p_pitch32, unsigned int p_colorspace,
	uint8 p_rs, uint8 p_gs, uint8 p_bs, uint8 p_as)
{
 assert((p_rs + p_gs + p_bs + p_as) == 48);
 assert(!((p_rs | p_gs | p_bs | p_as) & 0x7));
 assert(p_pitch32 >= p_width);

 pixels_is_external = FALSE;

 if(p_pixels)
 {
  pixels = p_pixels;
  pixels_is_external = TRUE;
 }
 else
 {
  if(!(pixels = (uint32 *)calloc(1, p_pitch32 * p_height * sizeof(uint32))))
   throw(1);
 }

 w = p_width;
 h = p_height;

 pitch32 = p_pitch32;

 format.bpp = 32;
 format.colorspace = p_colorspace;
 format.Rshift = p_rs;
 format.Gshift = p_gs;
 format.Bshift = p_bs;
 format.Ashift = p_as;

 palette = NULL;
}

MDFN_Surface::MDFN_Surface(uint8 *p_pixels, uint32 p_width, uint32 p_height, uint32 p_pitch8)
{
 assert(p_pitch8 >= p_width);

 pixels_is_external = FALSE;

 if(p_pixels)
 {
  pixels8 = p_pixels;
  pixels_is_external = TRUE;
 }
 else
 {
  if(!(pixels8 = (uint8 *)calloc(1, p_pitch8 * p_height * sizeof(uint8))))
   throw(1);
 }

 if(!(palette = (uint32 *)calloc(1, 256 * sizeof(uint32))))
  throw(1);

 w = p_width;
 h = p_height;

 pitch8 = p_pitch8;

 format.bpp = 8;
 format.colorspace = MDFN_COLORSPACE_RGB;
 format.Rshift = 0;
 format.Gshift = 8;
 format.Bshift = 16;
 format.Ashift = 24;
}


// When we're converting, only convert the w*h area(AKA leave the last part of the line, pitch32 - w, alone),
// for places where we store auxillary information there(graphics viewer in the debugger), and it'll be faster
// to boot.
void MDFN_Surface::SetFormat(const MDFN_PixelFormat &nf, bool convert)
{
 assert((nf.Rshift + nf.Gshift + nf.Bshift + nf.Ashift) == 48);
 assert(!((nf.Rshift | nf.Gshift | nf.Bshift | nf.Ashift) & 0x7));
 assert(format.bpp == 32);
 assert(nf.bpp == 32);

 if(convert)
 {
  // We should assert that surface->pixels is non-NULL even if we don't need to convert the surface, to catch more insidious bugs.
  assert(pixels);

  if(memcmp(&format, &nf, sizeof(MDFN_PixelFormat)))
  {
   puts("Converting");
   for(int y = 0; y < h; y++)
   {
    uint32 *row = &pixels[y * pitch32];

    for(int x = 0; x < w; x++)
    {
     uint32 c = row[x];
     int r, g, b, a;

     DecodeColor(c, r, g, b, a);
     row[x] = nf.MakeColor(r, g, b, a);
    }
   }
  }
 }
 format = nf;
}

void MDFN_Surface::Fill(uint8 r, uint8 g, uint8 b, uint8 a)
{
 uint32 color = MakeColor(r, g, b, a);

 assert(pixels);

 for(uint32 i = 0; i < (uint32)pitch32 * h; i++)
 {
  pixels[i] = color;
 }
}

MDFN_Surface::~MDFN_Surface()
{
 if(!pixels_is_external && pixels)
  free(pixels);

 if(palette)
  free(palette);
}

