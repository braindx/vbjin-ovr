#ifndef __MDFN_VIDEO_H
#define __MDFN_VIDEO_H

#include "types.h"

//#include "video/primitives.h"
//#include "video/text.h"

void MDFN_ResetMessages(void);
void MDFN_InitFontData(void);
void MDFN_DispMessage(const char *format, ...);// { }//__attribute__ ((format (printf, 1, 2)));

int MDFN_InitVirtualVideo(void);
void MDFN_KillVirtualVideo(void);

enum
{
 MDFN_COLORSPACE_RGB = 0,
 MDFN_COLORSPACE_YCbCr = 1,
 //MDFN_COLORSPACE_YUV = 2, // TODO
};

class MDFN_PixelFormat //typedef struct
{
 public:

 unsigned int bpp;	// 8 or 32 only.
 unsigned int colorspace;

 union
 {
  uint8 Rshift;  // Bit position of the lowest bit of the red component
  uint8 Yshift;
 };

 union
 {
  uint8 Gshift;  // [...] green component
  uint8 Ushift;
  uint8 Cbshift;
 };

 union
 {
  uint8 Bshift;  // [...] blue component
  uint8 Vshift;
  uint8 Crshift;
 };

 uint8 Ashift;  // [...] alpha component.

 // Creates a 32-bit value for the surface corresponding to the R/G/B/A color passed.
 INLINE uint32 MakeColor(uint8 r, uint8 g, uint8 b, uint8 a = 0) const
 {
  if(colorspace == MDFN_COLORSPACE_YCbCr)
  {
   uint32 y, u, v;

   y = 16 + ((r * 16842 + g * 33030 + b * 6422) >> 16);
   u = 128 + ((r * -9699 + g * -19071 + b * 28770) >> 16);
   v = 128 + ((r * 28770 + g * -24117 + b * -4653) >> 16);

   return((y << Yshift) | (u << Ushift) | (v << Vshift) | (a << Ashift));
  }
  else
   return((r << Rshift) | (g << Gshift) | (b << Bshift) | (a << Ashift));
 }

 // Gets the R/G/B/A values for the passed 32-bit surface pixel value
 INLINE void DecodeColor(uint32 value, int &r, int &g, int &b, int &a) const
 {
  if(colorspace == MDFN_COLORSPACE_YCbCr)
  {
   int32 y = (value >> Yshift) & 0xFF;
   int32 cb = (value >> Cbshift) & 0xFF;
   int32 cr = (value >> Crshift) & 0xFF;

   int32 r_tmp, g_tmp, b_tmp;

   r_tmp = g_tmp = b_tmp = 76284 * (y - 16);
   
   r_tmp = r_tmp + 104595 * (cr - 128);
   g_tmp = g_tmp - 53281 * (cr - 128) - 25690 * (cb - 128);
   b_tmp = b_tmp + 132186 * (cb - 128);

   r_tmp >>= 16;
   g_tmp >>= 16;
   b_tmp >>= 16;

   if(r_tmp < 0) r_tmp = 0;
   if(r_tmp > 255) r_tmp = 255;

   if(g_tmp < 0) g_tmp = 0;
   if(g_tmp > 255) g_tmp = 255;

   if(b_tmp < 0) b_tmp = 0;
   if(b_tmp > 255) b_tmp = 255;

   r = r_tmp;
   g = g_tmp;
   b = b_tmp;

   a = (value >> Ashift) & 0xFF;
  }
  else
  {
   r = (value >> Rshift) & 0xFF;
   g = (value >> Gshift) & 0xFF;
   b = (value >> Bshift) & 0xFF;
   a = (value >> Ashift) & 0xFF;
  }
 }

}; // MDFN_PixelFormat;

// Only supports 32-bit RGBA
class MDFN_Surface //typedef struct
{
 public:

 MDFN_Surface(uint32 *pixels, uint32 width, uint32 height, uint32 pitch32, unsigned int colorspace,
	uint8 rs, uint8 gs, uint8 bs, uint8 as);

 MDFN_Surface(uint8 *pixels, uint32 width, uint32 height, uint32 pitch8);

 ~MDFN_Surface();


 union 
 {
  uint32 *pixels;
  uint8 *pixels8;	// For 8bpp mode
 };

 uint32 *palette;	// [256]

 bool pixels_is_external;

 // w, h, and pitch32 should always be > 0
 int32 w;
 int32 h;

 union
 {
  int32 pitch8;	// for 8bpp mode
  int32 pitch32; // In pixels, not in bytes.
 };

 MDFN_PixelFormat format;

 void Fill(uint8 r, uint8 g, uint8 b, uint8 a);
 void SetFormat(const MDFN_PixelFormat &new_format, bool convert);

 // Creates a 32-bit value for the surface corresponding to the R/G/B/A color passed.
 INLINE uint32 MakeColor(uint8 r, uint8 g, uint8 b, uint8 a = 0) const
 {
  return(format.MakeColor(r, g, b, a));
 }

 // Gets the R/G/B/A values for the passed 32-bit surface pixel value
 INLINE void DecodeColor(uint32 value, int &r, int &g, int &b, int &a) const
 {
  format.DecodeColor(value, r, g, b, a);
 }

 INLINE void DecodeColor(uint32 value, int &r, int &g, int &b) const
 {
  int dummy_a;

  DecodeColor(value, r, g, b, dummy_a);
 }
};

#endif
