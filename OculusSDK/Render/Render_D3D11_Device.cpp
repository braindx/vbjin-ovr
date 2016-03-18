/************************************************************************************

Filename    :   Renderer_D3D1x.cpp
Content     :   RenderDevice implementation  for D3D11.
Created     :   September 10, 2012
Authors     :   Andrew Reisse

Copyright   :   Copyright 2012 Oculus VR, LLC. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#define GPU_PROFILING 1

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "Kernel/OVR_Log.h"
#include "Kernel/OVR_Std.h"

#include "Util/Util_Direct3D.h"
#include <comdef.h>

#include "Render_D3D11_Device.h"
#include "Util/Util_ImageWindow.h"
#include "Kernel/OVR_Log.h"

namespace OVR { namespace Render { namespace D3D11 {

using namespace D3DUtil;

static D3D11_INPUT_ELEMENT_DESC ModelVertexDesc[] =
{
    { "Position",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "Color",      0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(Vertex, C),     D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TexCoord",   0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, U),     D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TexCoord",   1, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, U2),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "Normal",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Norm),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

#pragma region Scene shaders
static const char* StdVertexShaderSrc =
    "float4x4 Proj;\n"
    "float4x4 View;\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "   float2 TexCoord1 : TEXCOORD1;\n"
    "   float3 Normal   : NORMAL;\n"
    "   float3 VPos     : TEXCOORD4;\n"
    "};\n"
    "void main(in float4 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord : TEXCOORD0, in float2 TexCoord1 : TEXCOORD1, in float3 Normal : NORMAL,\n"
    "          out Varyings ov)\n"
    "{\n"
    "   ov.Position = mul(Proj, mul(View, Position));\n"
    "   ov.Normal = mul(View, Normal);\n"
    "   ov.VPos = mul(View, Position);\n"
    "   ov.TexCoord = TexCoord;\n"
    "   ov.TexCoord1 = TexCoord1;\n"
    "   ov.Color = Color;\n"
    "}\n";

static const char* DirectVertexShaderSrc =
    "float4x4 View : register(c4);\n"
    "void main(in float4 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord : TEXCOORD0, in float2 TexCoord1 : TEXCOORD1, in float3 Normal : NORMAL,\n"
    "          out float4 oPosition : SV_Position, out float4 oColor : COLOR, out float2 oTexCoord : TEXCOORD0, out float2 oTexCoord1 : TEXCOORD1, out float3 oNormal : NORMAL)\n"
    "{\n"
    "   oPosition = mul(View, Position);\n"
    "   oTexCoord = TexCoord;\n"
    "   oTexCoord1 = TexCoord1;\n"
    "   oColor = Color;\n"
    "   oNormal = mul(View, Normal);\n"
    "}\n";

static const char* SolidPixelShaderSrc =
    "float4 Color;\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "   float4 finalColor = ov.Color;"
    // blend state expects premultiplied alpha
    "	finalColor.rgb *= finalColor.a;\n"
    "   return finalColor;\n"
    "}\n";

static const char* GouraudPixelShaderSrc =
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "   float4 finalColor = ov.Color;"
    // blend state expects premultiplied alpha
    "	finalColor.rgb *= finalColor.a;\n"
    "   return finalColor;\n"
    "}\n";

static const char* TexturePixelShaderSrc =
    "Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "	float4 color2 = ov.Color * Texture.Sample(Linear, ov.TexCoord);\n"
    "   if (color2.a <= 0.4)\n"
    "		discard;\n"
    "   return color2;\n"
    "}\n";

static const char* MultiTexturePixelShaderSrc =
    "Texture2D Texture[2] : register(t0);\n"
    "SamplerState Linear[2] : register(s0);\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "   float2 TexCoord1 : TEXCOORD1;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "   float4 color1 = Texture[0].Sample(Linear[0], ov.TexCoord);\n"
    "   float4 color2 = Texture[1].Sample(Linear[1], ov.TexCoord1);\n"
    "	color2.rgb = sqrt(color2.rgb);\n"
    "	color2.rgb = color2.rgb * lerp(0.2, 1.2, saturate(length(color2.rgb)));\n"
    "	color2 = color1 * color2;\n"
    "   if (color2.a <= 0.6)\n"
    "		discard;\n"
    "	return float4(color2.rgb / color2.a, 1);\n"
    "}\n";

#define LIGHTING_COMMON                 \
    "cbuffer Lighting : register(b1)\n" \
    "{\n"                               \
    "    float3 Ambient;\n"             \
    "    float3 LightPos[8];\n"         \
    "    float4 LightColor[8];\n"       \
    "    float  LightCount;\n"          \
    "};\n"                              \
    "struct Varyings\n"                 \
    "{\n"                                       \
    "   float4 Position : SV_Position;\n"       \
    "   float4 Color    : COLOR0;\n"            \
    "   float2 TexCoord : TEXCOORD0;\n"         \
    "   float3 Normal   : NORMAL;\n"            \
    "   float3 VPos     : TEXCOORD4;\n"         \
    "};\n"                                      \
    "float4 DoLight(Varyings v)\n"              \
    "{\n"                                       \
    "   float3 norm = normalize(v.Normal);\n"   \
    "   float3 light = Ambient;\n"              \
    "   for (uint i = 0; i < LightCount; i++)\n"\
    "   {\n"                                        \
    "       float3 ltp = (LightPos[i] - v.VPos);\n" \
    "       float  ldist = dot(ltp,ltp);\n"         \
    "       ltp = normalize(ltp);\n"                \
    "       light += saturate(LightColor[i] * v.Color.rgb * dot(norm, ltp) / sqrt(ldist));\n"\
    "   }\n"                                        \
    "   return float4(light, v.Color.a);\n"         \
    "}\n"

static const char* LitSolidPixelShaderSrc =
    LIGHTING_COMMON
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "   return DoLight(ov) * ov.Color;\n"
    "}\n";

static const char* LitTexturePixelShaderSrc =
    "Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    LIGHTING_COMMON
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "   return DoLight(ov) * Texture.Sample(Linear, ov.TexCoord);\n"
    "}\n";

static const char* AlphaTexturePixelShaderSrc =
    "Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "	float4 finalColor = ov.Color;\n"
    "	finalColor.a *= Texture.Sample(Linear, ov.TexCoord).r;\n"
    // blend state expects premultiplied alpha
    "	finalColor.rgb *= finalColor.a;\n"
    "	return finalColor;\n"
    "}\n";

static const char* AlphaBlendedTexturePixelShaderSrc =
    "Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "	float4 finalColor = ov.Color;\n"
    "	finalColor *= Texture.Sample(Linear, ov.TexCoord);\n"
    // blend state expects premultiplied alpha
    "	finalColor.rgb *= finalColor.a;\n"
    "	return finalColor;\n"
    "}\n";

static const char* AlphaPremultTexturePixelShaderSrc =
    "Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "	float4 finalColor = ov.Color;\n"
    "	finalColor *= Texture.Sample(Linear, ov.TexCoord);\n"
    // texture should already be in premultiplied alpha
    "	return finalColor;\n"
    "}\n";

#pragma endregion

#pragma region Distortion shaders
// ***** PostProcess Shader

static const char* PostProcessVertexShaderSrc =
    "float4x4 View : register(c4);\n"
    "float4x4 Texm : register(c8);\n"
    "void main(in float4 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord : TEXCOORD0, in float2 TexCoord1 : TEXCOORD1,\n"
    "          out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0)\n"
    "{\n"
    "   oPosition = mul(View, Position);\n"
    "   oTexCoord = mul(Texm, float4(TexCoord,0,1));\n"
    "}\n";


// Shader with lens distortion and chromatic aberration correction.
static const char* PostProcessPixelShaderWithChromAbSrc =
    "Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    "float3 DistortionClearColor;\n"
    "float EdgeFadeScale;\n"
    "float2 EyeToSourceUVScale;\n"
    "float2 EyeToSourceUVOffset;\n"
    "float2 EyeToSourceNDCScale;\n"
    "float2 EyeToSourceNDCOffset;\n"
    "float2 TanEyeAngleScale;\n"
    "float2 TanEyeAngleOffset;\n"
    "float4 HmdWarpParam;\n"
    "float4 ChromAbParam;\n"
    "\n"

    "float4 main(in float4 oPosition : SV_Position,\n"
    "            in float2 oTexCoord : TEXCOORD0) : SV_Target\n"
    "{\n"
    // Input oTexCoord is [-1,1] across the half of the screen used for a single eye.
    "   float2 TanEyeAngleDistorted = oTexCoord * TanEyeAngleScale + TanEyeAngleOffset;\n" // Scales to tan(thetaX),tan(thetaY), but still distorted (i.e. only the center is correct)
    "   float  RadiusSq = TanEyeAngleDistorted.x * TanEyeAngleDistorted.x + TanEyeAngleDistorted.y * TanEyeAngleDistorted.y;\n"
    "   float Distort = rcp ( 1.0 + RadiusSq * ( HmdWarpParam.y + RadiusSq * ( HmdWarpParam.z + RadiusSq * ( HmdWarpParam.w ) ) ) );\n"
    "   float DistortR = Distort * ( ChromAbParam.x + RadiusSq * ChromAbParam.y );\n"
    "   float DistortG = Distort;\n"
    "   float DistortB = Distort * ( ChromAbParam.z + RadiusSq * ChromAbParam.w );\n"
    "   float2 TanEyeAngleR = DistortR * TanEyeAngleDistorted;\n"
    "   float2 TanEyeAngleG = DistortG * TanEyeAngleDistorted;\n"
    "   float2 TanEyeAngleB = DistortB * TanEyeAngleDistorted;\n"

    // These are now in "TanEyeAngle" space.
    // The vectors (TanEyeAngleRGB.x, TanEyeAngleRGB.y, 1.0) are real-world vectors pointing from the eye to where the components of the pixel appear to be.
    // If you had a raytracer, you could just use them directly.

    // Scale them into ([0,0.5],[0,1]) or ([0.5,0],[0,1]) UV lookup space (depending on eye)
    "   float2 SourceCoordR = TanEyeAngleR * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   float2 SourceCoordG = TanEyeAngleG * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   float2 SourceCoordB = TanEyeAngleB * EyeToSourceUVScale + EyeToSourceUVOffset;\n"

    // Find the distance to the nearest edge.
    "   float2 NDCCoord = TanEyeAngleG * EyeToSourceNDCScale + EyeToSourceNDCOffset;\n"
    "   float EdgeFadeIn = EdgeFadeScale * ( 1.0 - max ( abs ( NDCCoord.x ), abs ( NDCCoord.y ) ) );\n"
    "   if ( EdgeFadeIn < 0.0 )\n"
    "   {\n"
    "       return float4(DistortionClearColor.r, DistortionClearColor.g, DistortionClearColor.b, 1.0);\n"
    "   }\n"
    "   EdgeFadeIn = saturate ( EdgeFadeIn );\n"

    // Actually do the lookups.
    "   float4 Result = float4(0,0,0,1);\n"
    "   Result.r = Texture.Sample(Linear, SourceCoordR).r;\n"
    "   Result.g = Texture.Sample(Linear, SourceCoordG).g;\n"
    "   Result.b = Texture.Sample(Linear, SourceCoordB).b;\n"
    "   Result.rgb *= EdgeFadeIn;\n"
    "   return Result;\n"
    "}\n";

//----------------------------------------------------------------------------

// A vertex format used for mesh-based distortion.
/*
struct DistortionVertex
{
Vector2f Pos;
Vector2f TexR;
Vector2f TexG;
Vector2f TexB;
Color Col;
};
*/

static D3D11_INPUT_ELEMENT_DESC DistortionVertexDesc[] =
{
    { "Position", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,             D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 8,             D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TexCoord", 1, DXGI_FORMAT_R32G32_FLOAT,   0, 8 + 8,         D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TexCoord", 2, DXGI_FORMAT_R32G32_FLOAT,   0, 8 + 8 + 8,     D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "Color",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 8 + 8 + 8 + 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

//----------------------------------------------------------------------------
// Simple distortion shader that does three texture reads.
// Used for mesh-based distortion without timewarp.

static const char* PostProcessMeshVertexShaderSrc =
    "float2 EyeToSourceUVScale;\n"
    "float2 EyeToSourceUVOffset;\n"
    "void main(in float2 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord0 : TEXCOORD0, in float2 TexCoord1 : TEXCOORD1, in float2 TexCoord2 : TEXCOORD2,\n"
    "          out float4 oPosition : SV_Position, out float4 oColor : COLOR, out float2 oTexCoord0 : TEXCOORD0, out float2 oTexCoord1 : TEXCOORD1, out float2 oTexCoord2 : TEXCOORD2)\n"
    "{\n"
    "   oPosition.x = Position.x;\n"
    "   oPosition.y = Position.y;\n"
    "   oPosition.z = 0.5;\n"
    "   oPosition.w = 1.0;\n"
    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // Scale them into the correct [0-1],[0-1] UV lookup space (depending on eye)
    "   oTexCoord0 = TexCoord0 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord1 = TexCoord1 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord2 = TexCoord2 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oColor = Color;\n"              // Used for vignette fade.
    "}\n";

static const char* PostProcessMeshPixelShaderSrc =
    "Texture2D HmdSpcTexture : register(t0);\n"
    "Texture2D OverlayTexture : register(t1);\n"
    "SamplerState Linear : register(s0);\n"
    "float  UseOverlay = 1;\n"
    "\n"
    "float4 main(in float4 oPosition : SV_Position, in float4 oColor : COLOR,\n"
    "            in float2 oTexCoord0 : TEXCOORD0, in float2 oTexCoord1 : TEXCOORD1, in float2 oTexCoord2 : TEXCOORD2) : SV_Target\n"
    "{\n"
    "   float4 finalColor = float4(0,0,0,1);\n"
    "   finalColor.r = HmdSpcTexture.Sample(Linear, oTexCoord0).r;\n"
    "   finalColor.g = HmdSpcTexture.Sample(Linear, oTexCoord1).g;\n"
    "   finalColor.b = HmdSpcTexture.Sample(Linear, oTexCoord2).b;\n"

    "   if(UseOverlay > 0)\n"
    "   {\n"
    "       float2 overlayColorR = OverlayTexture.Sample(Linear, oTexCoord0).ra;\n"
    "       float2 overlayColorG = OverlayTexture.Sample(Linear, oTexCoord1).ga;\n"
    "       float2 overlayColorB = OverlayTexture.Sample(Linear, oTexCoord2).ba;\n"

    // do premultiplied alpha blending - overlayColorX.x is color, overlayColorX.y is alpha
    "       finalColor.r = finalColor.r * saturate(1-overlayColorR.y) + overlayColorR.x;\n"
    "       finalColor.g = finalColor.g * saturate(1-overlayColorG.y) + overlayColorG.x;\n"
    "       finalColor.b = finalColor.b * saturate(1-overlayColorB.y) + overlayColorB.x;\n"
    "   }\n"

    "   finalColor.rgb = saturate(finalColor.rgb * oColor.rgb);\n"
    "   return finalColor;\n"
    "}\n";


//----------------------------------------------------------------------------
// Pixel shader is very simple - does three texture reads.
// Vertex shader does all the hard work.
// Used for mesh-based distortion with timewarp.

static const char* PostProcessMeshTimewarpVertexShaderSrc =
    "float2 EyeToSourceUVScale;\n"
    "float2 EyeToSourceUVOffset;\n"
    "float3x3 EyeRotationStart;\n"
    "float3x3 EyeRotationEnd;\n"
    "void main(in float2 Position : POSITION, in float4 Color : COLOR0,\n"
    "          in float2 TexCoord0 : TEXCOORD0, in float2 TexCoord1 : TEXCOORD1, in float2 TexCoord2 : TEXCOORD2,\n"
    "          out float4 oPosition : SV_Position, out float4 oColor : COLOR,\n"
    "          out float2 oHmdSpcTexCoordR : TEXCOORD0, out float2 oHmdSpcTexCoordG : TEXCOORD1, out float2 oHmdSpcTexCoordB : TEXCOORD2,"
    "          out float2 oOverlayTexCoordR : TEXCOORD3, out float2 oOverlayTexCoordG : TEXCOORD4, out float2 oOverlayTexCoordB : TEXCOORD5)\n"
    "{\n"
    "   oPosition.x = Position.x;\n"
    "   oPosition.y = Position.y;\n"
    "   oPosition.z = 0.5;\n"
    "   oPosition.w = 1.0;\n"

    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // These are now "real world" vectors in direction (x,y,1) relative to the eye of the HMD.
    "   float3 TanEyeAngleR = float3 ( TexCoord0.x, TexCoord0.y, 1.0 );\n"
    "   float3 TanEyeAngleG = float3 ( TexCoord1.x, TexCoord1.y, 1.0 );\n"
    "   float3 TanEyeAngleB = float3 ( TexCoord2.x, TexCoord2.y, 1.0 );\n"

    // Accurate time warp lerp vs. faster
#if 1
    // Apply the two 3x3 timewarp rotations to these vectors.
    "   float3 TransformedRStart = mul ( TanEyeAngleR, EyeRotationStart );\n"
    "   float3 TransformedGStart = mul ( TanEyeAngleG, EyeRotationStart );\n"
    "   float3 TransformedBStart = mul ( TanEyeAngleB, EyeRotationStart );\n"
    "   float3 TransformedREnd   = mul ( TanEyeAngleR, EyeRotationEnd );\n"
    "   float3 TransformedGEnd   = mul ( TanEyeAngleG, EyeRotationEnd );\n"
    "   float3 TransformedBEnd   = mul ( TanEyeAngleB, EyeRotationEnd );\n"
    // And blend between them.
    "   float3 TransformedR = lerp ( TransformedRStart, TransformedREnd, Color.a );\n"
    "   float3 TransformedG = lerp ( TransformedGStart, TransformedGEnd, Color.a );\n"
    "   float3 TransformedB = lerp ( TransformedBStart, TransformedBEnd, Color.a );\n"
#else
    "   float3x3 EyeRotation = lerp ( EyeRotationStart, EyeRotationEnd, Color.a );\n"
    "   float3 TransformedR   = mul ( TanEyeAngleR, EyeRotation );\n"
    "   float3 TransformedG   = mul ( TanEyeAngleG, EyeRotation );\n"
    "   float3 TransformedB   = mul ( TanEyeAngleB, EyeRotation );\n"
#endif

    // Project them back onto the Z=1 plane of the rendered images.
    "   float RecipZR = rcp ( TransformedR.z );\n"
    "   float RecipZG = rcp ( TransformedG.z );\n"
    "   float RecipZB = rcp ( TransformedB.z );\n"
    "   float2 FlattenedR = float2 ( TransformedR.x * RecipZR, TransformedR.y * RecipZR );\n"
    "   float2 FlattenedG = float2 ( TransformedG.x * RecipZG, TransformedG.y * RecipZG );\n"
    "   float2 FlattenedB = float2 ( TransformedB.x * RecipZB, TransformedB.y * RecipZB );\n"

    // These are now still in TanEyeAngle space.
    // Scale them into the correct [0-1],[0-1] UV lookup space (depending on eye)
    "   oHmdSpcTexCoordR = FlattenedR * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oHmdSpcTexCoordG = FlattenedG * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oHmdSpcTexCoordB = FlattenedB * EyeToSourceUVScale + EyeToSourceUVOffset;\n"

    // Static layer texcoords don't get any time warp offset
    "   oOverlayTexCoordR = TexCoord0 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oOverlayTexCoordG = TexCoord1 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oOverlayTexCoordB = TexCoord2 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"

    "   oColor = Color.r;\n"              // Used for vignette fade.
    "}\n";

static const char* PostProcessMeshTimewarpPixelShaderSrc =
    "Texture2D HmdSpcTexture : register(t0);\n"
    "Texture2D OverlayTexture : register(t1);\n"
    "SamplerState Linear : register(s0);\n"
    "float  UseOverlay = 1;\n"
    "\n"
    "float4 main(in float4 oPosition : SV_Position, in float4 oColor : COLOR,\n"
    "          in float2 oHmdSpcTexCoordR : TEXCOORD0, in float2 oHmdSpcTexCoordG : TEXCOORD1, in float2 oHmdSpcTexCoordB : TEXCOORD2,"
    "          in float2 oOverlayTexCoordR : TEXCOORD3, in float2 oOverlayTexCoordG : TEXCOORD4, in float2 oOverlayTexCoordB : TEXCOORD5) : SV_Target\n"
    "{\n"
    "   float4 finalColor = float4(0,0,0,1);\n"
    "   finalColor.r = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordR).r;\n"
    "   finalColor.g = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordG).g;\n"
    "   finalColor.b = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordB).b;\n"

    "   if(UseOverlay > 0)\n"
    "   {\n"
    "       float2 overlayColorR = OverlayTexture.Sample(Linear, oOverlayTexCoordR).ra;\n"
    "       float2 overlayColorG = OverlayTexture.Sample(Linear, oOverlayTexCoordG).ga;\n"
    "       float2 overlayColorB = OverlayTexture.Sample(Linear, oOverlayTexCoordB).ba;\n"

    // do premultiplied alpha blending - overlayColorX.x is color, overlayColorX.y is alpha
    "       finalColor.r = finalColor.r * saturate(1-overlayColorR.y) + overlayColorR.x;\n"
    "       finalColor.g = finalColor.g * saturate(1-overlayColorG.y) + overlayColorG.x;\n"
    "       finalColor.b = finalColor.b * saturate(1-overlayColorB.y) + overlayColorB.x;\n"
    "   }\n"

    "   finalColor.rgb = saturate(finalColor.rgb * oColor.rgb);\n"
    "   return finalColor;\n"
    "}\n";


//----------------------------------------------------------------------------
// Pixel shader is very simple - does three texture reads.
// Vertex shader does all the hard work.
// Used for mesh-based distortion with positional timewarp.

static const char* PostProcessMeshPositionalTimewarpVertexShaderSrc =
    "Texture2DMS<float,4> DepthTexture : register(t0);\n"
    // Padding because we are uploading "standard uniform buffer" constants
    "float4x4 Padding1;\n"
    "float4x4 Padding2;\n"
    "float2 EyeToSourceUVScale;\n"
    "float2 EyeToSourceUVOffset;\n"
    "float2 DepthProjector;\n"
    "float2 DepthDimSize;\n"
    "float4x4 EyeRotationStart;\n"
    "float4x4 EyeRotationEnd;\n"

    "float4 PositionFromDepth(float2 inTexCoord)\n"
    "{\n"
    "   float2 eyeToSourceTexCoord = inTexCoord * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   float depth = DepthTexture.Load(int2(eyeToSourceTexCoord * DepthDimSize), 0).x;\n"
    "   float linearDepth = DepthProjector.y / (depth - DepthProjector.x);\n"
    "   float4 retVal = float4(inTexCoord, 1, 1);\n"
    "   retVal.xyz *= linearDepth;\n"
    "   return retVal;\n"
    "}\n"

    "float2 TimewarpTexCoordToWarpedPos(float2 inTexCoord, float4x4 rotMat)\n"
    "{\n"
    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // These are now "real world" vectors in direction (x,y,1) relative to the eye of the HMD.	
    // Apply the 4x4 timewarp rotation to these vectors.
    "   float4 inputPos = PositionFromDepth(inTexCoord);\n"
    "   float3 transformed = float3( mul ( rotMat, inputPos ).xyz);\n"
    // Project them back onto the Z=1 plane of the rendered images.
    "   float2 flattened = transformed.xy / transformed.z;\n"
    // Scale them into ([0,0.5],[0,1]) or ([0.5,0],[0,1]) UV lookup space (depending on eye)
    "   float2 noDepthUV = flattened * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    //"   float depth = DepthTexture.SampleLevel(Linear, noDepthUV, 0).r;\n"
    "   return noDepthUV.xy;\n"
    "}\n"

    "void main(in float2 Position    : POSITION,    in float4 Color       : COLOR0,    in float2 TexCoord0 : TEXCOORD0,\n"
    "          in float2 TexCoord1   : TEXCOORD1,   in float2 TexCoord2   : TEXCOORD2,\n"
    "          out float4 oPosition  : SV_Position, out float4 oColor     : COLOR,\n"
    "          out float2 oHmdSpcTexCoordR : TEXCOORD0, out float2 oHmdSpcTexCoordG : TEXCOORD1, out float2 oHmdSpcTexCoordB : TEXCOORD2,"
    "          out float2 oOverlayTexCoordR : TEXCOORD3, out float2 oOverlayTexCoordG : TEXCOORD4, out float2 oOverlayTexCoordB : TEXCOORD5)\n"
    "{\n"
    "   oPosition.x = Position.x;\n"
    "   oPosition.y = Position.y;\n"
    "   oPosition.z = 0.5;\n"
    "   oPosition.w = 1.0;\n"

    "   float timewarpLerpFactor = Color.a;\n"
    "   float4x4 lerpedEyeRot = lerp(EyeRotationStart, EyeRotationEnd, timewarpLerpFactor);\n"
    //"	float4x4 lerpedEyeRot = EyeRotationStart;\n"

    // warped positions are a bit more involved, hence a separate function
    "   oHmdSpcTexCoordR = TimewarpTexCoordToWarpedPos(TexCoord0, lerpedEyeRot);\n"
    "   oHmdSpcTexCoordG = TimewarpTexCoordToWarpedPos(TexCoord1, lerpedEyeRot);\n"
    "   oHmdSpcTexCoordB = TimewarpTexCoordToWarpedPos(TexCoord2, lerpedEyeRot);\n"

    "   oOverlayTexCoordR = TexCoord0 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oOverlayTexCoordG = TexCoord1 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oOverlayTexCoordB = TexCoord2 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"

    "   oColor = Color.r;              // Used for vignette fade.\n"
    "}\n";

static const char* PostProcessMeshPositionalTimewarpPixelShaderSrc =
    "Texture2D HmdSpcTexture : register(t0);\n"
    "Texture2D OverlayTexture : register(t1);\n"
    "SamplerState Linear : register(s0);\n"
    "float2 DepthDimSize;\n"
    "float  UseOverlay = 1;\n"
    "\n"
    "float4 main(in float4 oPosition : SV_Position, in float4 oColor : COLOR,\n"
    "            in float2 oHmdSpcTexCoordR : TEXCOORD0, in float2 oHmdSpcTexCoordG : TEXCOORD1, in float2 oHmdSpcTexCoordB : TEXCOORD2,"
    "            in float2 oOverlayTexCoordR : TEXCOORD3, in float2 oOverlayTexCoordG : TEXCOORD4, in float2 oOverlayTexCoordB : TEXCOORD5) : SV_Target\n"
    "{\n"
    "   float4 finalColor = float4(0,0,0,1);\n"
    "   finalColor.r = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordR).r;\n"
    "   finalColor.g = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordG).g;\n"
    "   finalColor.b = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordB).b;\n"

    "   if(UseOverlay > 0)\n"
    "   {\n"
    "       float2 overlayColorR = OverlayTexture.Sample(Linear, oOverlayTexCoordR).ra;\n"
    "       float2 overlayColorG = OverlayTexture.Sample(Linear, oOverlayTexCoordG).ga;\n"
    "       float2 overlayColorB = OverlayTexture.Sample(Linear, oOverlayTexCoordB).ba;\n"

    // do premultiplied alpha blending - overlayColorX.x is color, overlayColorX.y is alpha
    "       finalColor.r = finalColor.r * saturate(1-overlayColorR.y) + overlayColorR.x;\n"
    "       finalColor.g = finalColor.g * saturate(1-overlayColorG.y) + overlayColorG.x;\n"
    "       finalColor.b = finalColor.b * saturate(1-overlayColorB.y) + overlayColorB.x;\n"
    "   }\n"

    "   finalColor.rgb = saturate(finalColor.rgb * oColor.rgb);\n"
    "   return finalColor;\n"
    "}\n";

//----------------------------------------------------------------------------
// Pixel shader is very simple - does three texture reads.
// Vertex shader does all the hard work.
// Used for mesh-based heightmap reprojection for positional timewarp.

static D3D11_INPUT_ELEMENT_DESC HeightmapVertexDesc[] =
{
    { "Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

static const char* PostProcessHeightmapTimewarpVertexShaderSrc =
    "Texture2DMS<float,4> DepthTexture : register(t0);\n"
    // Padding because we are uploading "standard uniform buffer" constants
    "float4x4 Padding1;\n"
    "float4x4 Padding2;\n"
    "float2 EyeToSourceUVScale;\n"
    "float2 EyeToSourceUVOffset;\n"
    "float2 DepthDimSize;\n"
    "float4x4 EyeXformStart;\n"
    "float4x4 EyeXformEnd;\n"
    //"float4x4 Projection;\n"
    "float4x4 InvProjection;\n"

    "float4 PositionFromDepth(float2 position, float2 inTexCoord)\n"
    "{\n"
    "   float depth = DepthTexture.Load(int2(inTexCoord * DepthDimSize), 0).x;\n"
    "   float4 retVal = float4(position, depth, 1);\n"
    "   return retVal;\n"
    "}\n"

    "float4 TimewarpPos(float2 position, float2 inTexCoord, float4x4 rotMat)\n"
    "{\n"
    // Apply the 4x4 timewarp rotation to these vectors.
    "   float4 transformed = PositionFromDepth(position, inTexCoord);\n"
    // TODO: Precombining InvProjection in rotMat causes loss of precision flickering
    "   transformed = mul ( InvProjection, transformed );\n"
    "   transformed = mul ( rotMat, transformed );\n"
    // Commented out as Projection is currently contained in rotMat
    //"   transformed = mul ( Projection, transformed );\n"
    "   return transformed;\n"
    "}\n"

    "void main( in float2 Position    : POSITION,    in float3 TexCoord0    : TEXCOORD0,\n"
    "           out float4 oPosition  : SV_Position, out float2 oTexCoord0  : TEXCOORD0)\n"
    "{\n"
    "   float2 eyeToSrcTexCoord = TexCoord0.xy * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord0 = eyeToSrcTexCoord;\n"

    "   float timewarpLerpFactor = TexCoord0.z;\n"
    "   float4x4 lerpedEyeRot = lerp(EyeXformStart, EyeXformEnd, timewarpLerpFactor);\n"
    //"	float4x4 lerpedEyeRot = EyeXformStart;\n"

    "   oPosition = TimewarpPos(Position.xy, oTexCoord0, lerpedEyeRot);\n"
    "}\n";

static const char* PostProcessHeightmapTimewarpPixelShaderSrc =
    "Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    "\n"
    "float4 main(in float4 oPosition : SV_Position, in float2 oTexCoord0 : TEXCOORD0) : SV_Target\n"
    "{\n"
    "   float3 result;\n"
    "   result = Texture.Sample(Linear, oTexCoord0);\n"
    "   return float4(result, 1.0);\n"
    "}\n";

#pragma endregion

//----------------------------------------------------------------------------

struct ShaderSource
{
    const char* ShaderModel;
    const char* SourceStr;
};

static ShaderSource VShaderSrcs[VShader_Count] =
{
    { "vs_4_0", DirectVertexShaderSrc },
    { "vs_4_0", StdVertexShaderSrc },
    { "vs_4_0", PostProcessVertexShaderSrc },
    { "vs_4_0", PostProcessMeshVertexShaderSrc },
    { "vs_4_0", PostProcessMeshTimewarpVertexShaderSrc },
    { "vs_4_1", PostProcessMeshPositionalTimewarpVertexShaderSrc },
    { "vs_4_1", PostProcessHeightmapTimewarpVertexShaderSrc },
};
static ShaderSource FShaderSrcs[FShader_Count] =
{
    { "ps_4_0", SolidPixelShaderSrc },
    { "ps_4_0", GouraudPixelShaderSrc },
    { "ps_4_0", TexturePixelShaderSrc },
    { "ps_4_0", AlphaTexturePixelShaderSrc },
    { "ps_4_0", AlphaBlendedTexturePixelShaderSrc },
    { "ps_4_0", AlphaPremultTexturePixelShaderSrc },
    { "ps_4_0", PostProcessPixelShaderWithChromAbSrc },
    { "ps_4_0", LitSolidPixelShaderSrc },
    { "ps_4_0", LitTexturePixelShaderSrc },
    { "ps_4_0", MultiTexturePixelShaderSrc },
    { "ps_4_0", PostProcessMeshPixelShaderSrc },
    { "ps_4_0", PostProcessMeshTimewarpPixelShaderSrc },
    { "ps_4_0", PostProcessMeshPositionalTimewarpPixelShaderSrc },
    { "ps_4_0", PostProcessHeightmapTimewarpPixelShaderSrc },
};


RenderDevice::RenderDevice(ovrHmd hmd, const RendererParams& p, HWND window, ovrGraphicsLuid luid) :
    Render::RenderDevice(hmd),
    DXGIFactory(),
    Window(window),
    Device(),
    Context(),
    SwapChain(),
    Adapter(),
    FullscreenOutput(),
    FSDesktopX(-1),
    FSDesktopY(-1),
    PreFullscreenX(0),
    PreFullscreenY(0),
    PreFullscreenW(0),
    PreFullscreenH(0),
    BackBuffer(),
    BackBufferRT(),
    CurRenderTarget(),
    CurDepthBuffer(),
    RasterizerCullOff(),
    RasterizerCullBack(),
    RasterizerCullFront(),
    BlendState(),
    //DepthStates[]
    CurDepthState(),
    ModelVertexIL(),
    DistortionVertexIL(),
    HeightmapVertexIL(),
    //SamplerStates[]
    StdUniforms(),
    //UniformBuffers[];
    //MaxTextureSet[];
    //VertexShaders[];
    //PixelShaders[];
    //pStereoShaders[];
    //CommonUniforms[];
    ExtraShaders(),
    DefaultFill(),
    DefaultTextureFill(),
    DefaultTextureFillAlpha(),
    DefaultTextureFillPremult(),
    QuadVertexBuffer(),
    DepthBuffers()
{
    memset(&D3DViewport, 0, sizeof(D3DViewport));
    memset(MaxTextureSet, 0, sizeof(MaxTextureSet));

    HRESULT hr;

    RECT rc;
    if (p.Resolution == Sizei(0))
    {
        GetClientRect(window, &rc);
        UINT width = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;
        ::OVR::Render::RenderDevice::SetWindowSize(width, height);
    }
    else
    {
        // TBD: This should be renamed to not be tied to window for App mode.
        ::OVR::Render::RenderDevice::SetWindowSize(p.Resolution.w, p.Resolution.h);
    }

    Params = p;
    DXGIFactory = NULL;
    hr = CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)(&DXGIFactory.GetRawRef()));
    OVR_D3D_CHECK_RET(hr);
    UINT adapterNum = 0;
    bool adapterFound = false;

    LUID* pLuid = (LUID*)&luid;

    if ((pLuid->HighPart | pLuid->LowPart) == 0)
    {
        // Allow to use null/default adapter for applications that may render
        // a window without an HMD.
        Adapter = nullptr;
    }
    else
    {
        do
        {
            Adapter = nullptr;

            hr = DXGIFactory->EnumAdapters(adapterNum, &Adapter.GetRawRef());

            if (SUCCEEDED(hr) && Adapter)
            {
                DXGI_ADAPTER_DESC adapterDesc;

                Adapter->GetDesc(&adapterDesc);
                if (adapterDesc.AdapterLuid.HighPart == pLuid->HighPart &&
                    adapterDesc.AdapterLuid.LowPart == pLuid->LowPart)
                {
                    adapterFound = true;
                    break;
                }
            }

            ++adapterNum;
        } while (SUCCEEDED(hr));

        OVR_ASSERT(adapterFound);
        if (!adapterFound)
        {
            // This is unfortunate. The HMD's adapter disappeared during creating our
            // adapter.
            throw AdapterNotFoundException();
        }
    }
    

    int flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    // FIXME: Disable debug device creation while
    // we find the source of the debug slowdown.
#if 0
    if (p.DebugEnabled)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    Device  = NULL;
    Context = NULL;
    D3D_FEATURE_LEVEL featureLevel; // TODO: Limit certain features based on D3D feature level
    hr = D3D11CreateDevice(Adapter, Adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                           NULL, flags, NULL, 0, D3D11_SDK_VERSION,
                           &Device.GetRawRef(), &featureLevel, &Context.GetRawRef());
    OVR_D3D_CHECK_RET(hr);

    if (!RecreateSwapChain())
        throw SwapChainCreationFailedException();

    CurRenderTarget = NULL;
    for (int i = 0; i < Shader_Count; i++)
    {
        UniformBuffers[i] = *CreateBuffer();
        MaxTextureSet[i] = 0;
    }

    ID3D10Blob* vsData = CompileShader(VShaderSrcs[0].ShaderModel, VShaderSrcs[0].SourceStr);

    VertexShaders[VShader_MV] = *new VertexShader(this, vsData);
    for (int i = 1; i < VShader_Count; i++)
    {
        OVR_ASSERT(VShaderSrcs[i].SourceStr != NULL);      // You forgot a shader!
        ID3D10Blob *pShader = CompileShader(VShaderSrcs[i].ShaderModel, VShaderSrcs[i].SourceStr);

        VertexShaders[i] = NULL;
        if (pShader != NULL)
        {
            VertexShaders[i] = *new VertexShader(this, pShader);
        }
    }

    for (int i = 0; i < FShader_Count; i++)
    {
        OVR_ASSERT(FShaderSrcs[i].SourceStr != NULL);      // You forgot a shader!
        ID3D10Blob *pShader = CompileShader(FShaderSrcs[i].ShaderModel, FShaderSrcs[i].SourceStr);

        PixelShaders[i] = NULL;
        if (pShader != NULL)
        {
            PixelShaders[i] = *new PixelShader(this, pShader);
        }
    }

    intptr_t bufferSize = vsData->GetBufferSize();
    const void* buffer = vsData->GetBufferPointer();
    ModelVertexIL = NULL;
    ID3D11InputLayout** objRef = &ModelVertexIL.GetRawRef();
    hr = Device->CreateInputLayout(ModelVertexDesc, sizeof(ModelVertexDesc) / sizeof(ModelVertexDesc[0]), buffer, bufferSize, objRef);
    OVR_D3D_CHECK_RET(hr);

    {
        ID3D10Blob* vsData2 = CompileShader("vs_4_1", PostProcessMeshVertexShaderSrc);
        intptr_t bufferSize2 = vsData2->GetBufferSize();
        const void* buffer2 = vsData2->GetBufferPointer();
        DistortionVertexIL = NULL;
        ID3D11InputLayout** objRef2 = &DistortionVertexIL.GetRawRef();
        hr = Device->CreateInputLayout(DistortionVertexDesc, sizeof(DistortionVertexDesc) / sizeof(DistortionVertexDesc[0]), buffer2, bufferSize2, objRef2);
        OVR_D3D_CHECK_RET(hr);
    }

    {
        ID3D10Blob* vsData2 = CompileShader("vs_4_1", PostProcessHeightmapTimewarpVertexShaderSrc);
        intptr_t bufferSize2 = vsData2->GetBufferSize();
        const void* buffer2 = vsData2->GetBufferPointer();
        HeightmapVertexIL = NULL;
        ID3D11InputLayout** objRef2 = &HeightmapVertexIL.GetRawRef();
        hr = Device->CreateInputLayout(HeightmapVertexDesc, sizeof(HeightmapVertexDesc) / sizeof(HeightmapVertexDesc[0]), buffer2, bufferSize2, objRef2);
        OVR_D3D_CHECK_RET(hr);
    }

    Ptr<ShaderSet> gouraudShaders = *new ShaderSet();
    gouraudShaders->SetShader(VertexShaders[VShader_MVP]);
    gouraudShaders->SetShader(PixelShaders[FShader_Gouraud]);
    DefaultFill = *new ShaderFill(gouraudShaders);

    DefaultTextureFill        = CreateTextureFill ( NULL, false, false );
    DefaultTextureFillAlpha   = CreateTextureFill ( NULL, true, false );
    DefaultTextureFillPremult = CreateTextureFill ( NULL, false, true );
    // One day, I will understand smart pointers. Today is not that day...
    DefaultTextureFill->Release();
    DefaultTextureFillAlpha->Release();
    DefaultTextureFillPremult->Release();


    D3D11_BLEND_DESC bm;
    memset(&bm, 0, sizeof(bm));
    bm.RenderTarget[0].BlendEnable = true;
    bm.RenderTarget[0].BlendOp = bm.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bm.RenderTarget[0].SrcBlend = bm.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; //premultiplied alpha
    bm.RenderTarget[0].DestBlend = bm.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bm.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    BlendState = NULL;
    hr = Device->CreateBlendState(&bm, &BlendState.GetRawRef());
    OVR_D3D_CHECK_RET(hr);

    D3D11_RASTERIZER_DESC rs;
    memset(&rs, 0, sizeof(rs));
    rs.AntialiasedLineEnable = false;   // You can't just turn this on - it needs alpha modes etc setting up and doesn't work with Z buffers.
    rs.CullMode = D3D11_CULL_BACK;      // Don't use D3D11_CULL_NONE as it will cause z-fighting on certain double-sided thin meshes (e.g. leaves)
    rs.DepthClipEnable = true;
    rs.FillMode = D3D11_FILL_SOLID;
    RasterizerCullBack = NULL;
    hr = Device->CreateRasterizerState(&rs, &RasterizerCullBack.GetRawRef());
    OVR_D3D_CHECK_RET(hr);

    rs.CullMode = D3D11_CULL_FRONT;
    RasterizerCullFront = NULL;
    hr = Device->CreateRasterizerState(&rs, &RasterizerCullFront.GetRawRef());
    OVR_D3D_CHECK_RET(hr);

    rs.CullMode = D3D11_CULL_NONE;
    RasterizerCullOff = NULL;
    hr = Device->CreateRasterizerState(&rs, &RasterizerCullOff.GetRawRef());
    OVR_D3D_CHECK_RET(hr);    

    QuadVertexBuffer = *CreateBuffer();
    const Render::Vertex QuadVertices[] =
    { Vertex(Vector3f(0, 1, 0)), Vertex(Vector3f(1, 1, 0)),
    Vertex(Vector3f(0, 0, 0)), Vertex(Vector3f(1, 0, 0)) };
    if (!QuadVertexBuffer->Data(Buffer_Vertex | Buffer_ReadOnly, QuadVertices, sizeof(QuadVertices)))
    {
        OVR_ASSERT(false);
    }

    SetDepthMode(0, 0);

    Blitter = *new D3DUtil::Blitter(Device);
    if (!Blitter->Initialize())
    {
        OVR_ASSERT(false);
    }

    Context->QueryInterface(IID_PPV_ARGS(&UserAnnotation.GetRawRef()));
}

RenderDevice::~RenderDevice()
{
    DeleteFills();
}

void RenderDevice::DeleteFills()
{
    DefaultTextureFill.Clear();
    DefaultTextureFillAlpha.Clear();
    DefaultTextureFillPremult.Clear();
}

// Implement static initializer function to create this class.
Render::RenderDevice* RenderDevice::CreateDevice(ovrHmd hmd, const RendererParams& rp, void* oswnd, ovrGraphicsLuid luid)
{
    Render::D3D11::RenderDevice* render = new RenderDevice(hmd, rp, (HWND)oswnd, luid);

    // Sanity check to make sure our resources were created.
    // This should stop a lot of driver related crashes we have experienced
    if ((render->DXGIFactory == NULL) || (render->Device == NULL) || (render->SwapChain == NULL))
    {
        OVR_ASSERT(false);
        // TBD: Probabaly other things like shader creation should be verified as well
        render->Shutdown();
        render->Release();
        return NULL;
    }

    return render;
}


bool RenderDevice::RecreateSwapChain()
{
    HRESULT hr;

    DXGI_SWAP_CHAIN_DESC scDesc;
    memset(&scDesc, 0, sizeof(scDesc));
    scDesc.BufferCount = 1;
    scDesc.BufferDesc.Width = WindowWidth;
    scDesc.BufferDesc.Height = WindowHeight;
    scDesc.BufferDesc.Format = Params.SrgbBackBuffer ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    // Use default refresh rate; switching rate on CC prototype can cause screen lockup.
    scDesc.BufferDesc.RefreshRate.Numerator = 0;
    scDesc.BufferDesc.RefreshRate.Denominator = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.OutputWindow = Window;
    scDesc.SampleDesc.Count = 1;
    OVR_ASSERT ( scDesc.SampleDesc.Count >= 1 );        // 0 is no longer valid.
    scDesc.SampleDesc.Quality = 0;
    scDesc.Windowed = TRUE;

    if (SwapChain)
    {
        SwapChain = NULL;
    }

    Ptr<IDXGISwapChain> newSC;
    hr = DXGIFactory->CreateSwapChain(Device, &scDesc, &newSC.GetRawRef());
    OVR_D3D_CHECK_RET_FALSE(hr);
    SwapChain = newSC;

    BackBuffer = NULL;
    BackBufferRT = NULL;
    hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer.GetRawRef());
    OVR_D3D_CHECK_RET_FALSE(hr);

    hr = Device->CreateRenderTargetView(BackBuffer, NULL, &BackBufferRT.GetRawRef());
    OVR_D3D_CHECK_RET_FALSE(hr);

    Texture* depthBuffer = GetDepthBuffer(WindowWidth, WindowHeight, 1, Texture_Depth32f);
    CurDepthBuffer = depthBuffer;
    if (CurRenderTarget == NULL && depthBuffer != NULL)
    {
        Context->OMSetRenderTargets(1, &BackBufferRT.GetRawRef(), depthBuffer->GetDsv());
    }

    return true;
}

bool RenderDevice::SetParams(const RendererParams& newParams)
{
    Params = newParams;
    return RecreateSwapChain();
}

ovrTexture Texture::Get_ovrTexture()
{
    ovrTexture tex;

    OVR::Sizei newRTSize(Width, Height);

    ovrD3D11TextureData* texData = (ovrD3D11TextureData*)&tex;
    texData->Header.API = ovrRenderAPI_D3D11;
    texData->Header.TextureSize = newRTSize;
    texData->pTexture = GetTex();

    return tex;
}

void RenderDevice::SetViewport(const Recti& vp)
{
    D3DViewport.Width = (float)vp.w;
    D3DViewport.Height = (float)vp.h;
    D3DViewport.MinDepth = 0;
    D3DViewport.MaxDepth = 1;
    D3DViewport.TopLeftX = (float)vp.x;
    D3DViewport.TopLeftY = (float)vp.y;
    Context->RSSetViewports(1, &D3DViewport);
}

static int GetDepthStateIndex(bool enable, bool write, RenderDevice::CompareFunc func)
{
    if (!enable)
    {
        return 0;
    }
    return 1 + int(func) * 2 + write;
}

void RenderDevice::SetDepthMode(bool enable, bool write, CompareFunc func)
{
    HRESULT hr;

    int index = GetDepthStateIndex(enable, write, func);
    if (DepthStates[index])
    {
        CurDepthState = DepthStates[index];
        Context->OMSetDepthStencilState(DepthStates[index], 0);
        return;
    }

    D3D11_DEPTH_STENCIL_DESC dss;
    memset(&dss, 0, sizeof(dss));
    dss.DepthEnable = enable;
    switch (func)
    {
    case Compare_Always:  dss.DepthFunc = D3D11_COMPARISON_ALWAYS;  break;
    case Compare_Less:    dss.DepthFunc = D3D11_COMPARISON_LESS;    break;
    case Compare_Greater: dss.DepthFunc = D3D11_COMPARISON_GREATER; break;
    default:
        OVR_ASSERT(0);
    }
    dss.DepthWriteMask = write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    hr = Device->CreateDepthStencilState(&dss, &DepthStates[index].GetRawRef());
    OVR_D3D_CHECK_RET(hr);
    Context->OMSetDepthStencilState(DepthStates[index], 0);
    CurDepthState = DepthStates[index];
}

Texture* RenderDevice::GetDepthBuffer(int w, int h, int ms, TextureFormat depthFormat)
{
    for (unsigned i = 0; i < DepthBuffers.GetSize(); i++)
    {
        if (w == DepthBuffers[i]->Width && h == DepthBuffers[i]->Height &&
            ms == DepthBuffers[i]->Samples)
            return DepthBuffers[i];
    }

    OVR_ASSERT_M(
        depthFormat == Texture_Depth32f ||
        depthFormat == Texture_Depth24Stencil8 ||
        depthFormat == Texture_Depth32fStencil8 ||
        depthFormat == Texture_Depth16, "Unknown depth buffer format");

    Ptr<Texture> newDepth = *CreateTexture(depthFormat | ms, w, h, NULL);
    if (newDepth == NULL)
    {
        OVR_DEBUG_LOG(("Failed to get depth buffer."));
        return NULL;
    }

    DepthBuffers.PushBack(newDepth);
    return newDepth.GetPtr();
}

void RenderDevice::Clear(float r /*= 0*/, float g /*= 0*/, float b /*= 0*/, float a /*= 1*/,
    float depth /*= 1*/,
    bool clearColor /*= true*/, bool clearDepth /*= true*/)
{
    if (clearColor)
    {
        const float color[] = { r, g, b, a };
        if (CurRenderTarget == NULL)
        {
            Context->ClearRenderTargetView(BackBufferRT.GetRawRef(), color);
        }
        else
        {
            Context->ClearRenderTargetView(CurRenderTarget->GetRtv(), color);
        }
    }

    if (clearDepth)
    {
        Context->ClearDepthStencilView(CurDepthBuffer->GetDsv(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, 0);
    }
}

// Buffers

Buffer* RenderDevice::CreateBuffer()
{
    return new Buffer(this);
}

Buffer::~Buffer()
{
}

bool Buffer::Data(int use, const void *buffer, size_t size)
{
    HRESULT hr;

    if (D3DBuffer && Size >= size)
    {
        if (Dynamic)
        {
            if (!buffer)
                return true;

            void* v = Map(0, size, Map_Discard);
            if (v)
            {
                memcpy(v, buffer, size);
                Unmap(v);
                return true;
            }
        }
        else
        {
            OVR_ASSERT(!(use & Buffer_ReadOnly));
            Ren->Context->UpdateSubresource(D3DBuffer, 0, NULL, buffer, 0, 0);
            return true;
        }
    }
    if (D3DBuffer)
    {
        D3DBuffer = NULL;
        Size = 0;
        Use = 0;
        Dynamic = false;
    }

    D3D11_BUFFER_DESC desc;
    memset(&desc, 0, sizeof(desc));
    if (use & Buffer_ReadOnly)
    {
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.CPUAccessFlags = 0;
    }
    else
    {
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Dynamic = true;
    }

    switch (use & Buffer_TypeMask)
    {
    case Buffer_Vertex:  desc.BindFlags = D3D11_BIND_VERTEX_BUFFER; break;
    case Buffer_Index:   desc.BindFlags = D3D11_BIND_INDEX_BUFFER;  break;
    case Buffer_Uniform:
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        size = ((size + 15) & ~15);
        break;
    case Buffer_Feedback:
        desc.BindFlags = D3D11_BIND_STREAM_OUTPUT;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.CPUAccessFlags = 0;
        size = ((size + 15) & ~15);
        break;
    case Buffer_Compute:
        // There's actually a bunch of options for buffers bound to a CS.
        // Right now this is the most appropriate general-purpose one. Add more as needed.

        // NOTE - if you want D3D11_CPU_ACCESS_WRITE, it MUST be either D3D11_USAGE_DYNAMIC or D3D11_USAGE_STAGING.
        // TODO: we want a resource that is rarely written to, in which case we'd need two surfaces - one a STAGING
        // that the CPU writes to, and one a DEFAULT, and we CopyResource from one to the other. Hassle!
        // Setting it as D3D11_USAGE_DYNAMIC will get the job done for now.
        // Also for fun - you can't have a D3D11_USAGE_DYNAMIC buffer that is also a D3D11_BIND_UNORDERED_ACCESS.
        OVR_ASSERT(!(use & Buffer_ReadOnly));
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        // SUPERHACKYFIXME
        desc.StructureByteStride = sizeof(DistortionComputePin);

        Dynamic = true;
        size = ((size + 15) & ~15);
        break;
    default:
        OVR_ASSERT(!"unknown buffer type");
        break;
    }

    desc.ByteWidth = (unsigned)size;

    D3D11_SUBRESOURCE_DATA sr;
    sr.pSysMem = buffer;
    sr.SysMemPitch = 0;
    sr.SysMemSlicePitch = 0;

    D3DBuffer = NULL;
    hr = Ren->Device->CreateBuffer(&desc, buffer ? &sr : NULL, &D3DBuffer.GetRawRef());
    OVR_D3D_CHECK_RET_FALSE(hr);

    Use = 0;
    Size = 0;

    if ((use & Buffer_TypeMask) == Buffer_Compute)
    {
        hr = Ren->Device->CreateShaderResourceView(D3DBuffer, NULL, &D3DSrv.GetRawRef());
        OVR_D3D_CHECK_RET_FALSE(hr);
    }

    Use = use;
    Size = desc.ByteWidth;

    return true;
}

void* Buffer::Map(size_t start, size_t size, int flags)
{
    OVR_UNUSED(size);

    D3D11_MAP mapFlags = D3D11_MAP_WRITE;
    if (flags & Map_Discard)
        mapFlags = D3D11_MAP_WRITE_DISCARD;
    if (flags & Map_Unsynchronized)
        mapFlags = D3D11_MAP_WRITE_NO_OVERWRITE;

    D3D11_MAPPED_SUBRESOURCE map;
    if (SUCCEEDED(Ren->Context->Map(D3DBuffer, 0, mapFlags, 0, &map)))
        return ((char*)map.pData) + start;
    else
        return NULL;
}

bool Buffer::Unmap(void* m)
{
    OVR_UNUSED(m);

    Ren->Context->Unmap(D3DBuffer, 0);
    return true;
}


// Shaders

template<> bool Shader<Render::Shader_Vertex, ID3D11VertexShader>::Load(void* shader, size_t size)
{
    HRESULT hr = Ren->Device->CreateVertexShader(shader, size, NULL, &D3DShader);
    OVR_D3D_CHECK_RET_FALSE(hr);
    return true;
}
template<> bool Shader<Render::Shader_Pixel, ID3D11PixelShader>::Load(void* shader, size_t size)
{
    HRESULT hr = Ren->Device->CreatePixelShader(shader, size, NULL, &D3DShader);
    OVR_D3D_CHECK_RET_FALSE(hr);
    return true;
}
template<> bool Shader<Render::Shader_Geometry, ID3D11GeometryShader>::Load(void* shader, size_t size)
{
    HRESULT hr = Ren->Device->CreateGeometryShader(shader, size, NULL, &D3DShader);
    OVR_D3D_CHECK_RET_FALSE(hr);
    return true;
}

template<> void Shader<Render::Shader_Vertex, ID3D11VertexShader>::Set(PrimitiveType) const
{
    Ren->Context->VSSetShader(D3DShader, NULL, 0);
}
template<> void Shader<Render::Shader_Pixel, ID3D11PixelShader>::Set(PrimitiveType) const
{
    Ren->Context->PSSetShader(D3DShader, NULL, 0);
}
template<> void Shader<Render::Shader_Geometry, ID3D11GeometryShader>::Set(PrimitiveType) const
{
    Ren->Context->GSSetShader(D3DShader, NULL, 0);
}

template<> void Shader<Render::Shader_Vertex, ID3D11VertexShader>::SetUniformBuffer(Render::Buffer* buffer, int i)
{
    Ren->Context->VSSetConstantBuffers(i, 1, &((Buffer*)buffer)->D3DBuffer.GetRawRef());
}
template<> void Shader<Render::Shader_Pixel, ID3D11PixelShader>::SetUniformBuffer(Render::Buffer* buffer, int i)
{
    Ren->Context->PSSetConstantBuffers(i, 1, &((Buffer*)buffer)->D3DBuffer.GetRawRef());
}
template<> void Shader<Render::Shader_Geometry, ID3D11GeometryShader>::SetUniformBuffer(Render::Buffer* buffer, int i)
{
    Ren->Context->GSSetConstantBuffers(i, 1, &((Buffer*)buffer)->D3DBuffer.GetRawRef());
}

ID3D10Blob* RenderDevice::CompileShader(const char* profile, const char* src, const char* mainName)
{
    Ptr<ID3D10Blob> shader;
    Ptr<ID3D10Blob> errors;
    HRESULT hr = D3DCompile(src, strlen(src), NULL, NULL, NULL, mainName, profile,
        0, 0, &shader.GetRawRef(), &errors.GetRawRef());
    LogD3DCompileError(hr, errors);
    OVR_D3D_CHECK_RET_NULL(hr);

    shader->AddRef();
    return shader;
}


ShaderBase::ShaderBase(RenderDevice* r, ShaderStage stage) :
    Render::Shader(stage),
    Ren(r),
    UniformData(NULL),
    UniformsSize(-1)
{
}

ShaderBase::~ShaderBase()
{
    if (UniformData)
    {
        OVR_FREE(UniformData);
        UniformData = NULL;
    }
}

bool ShaderBase::SetUniform(const char* name, int n, const float* v)
{
    for (int i = 0; i < UniformInfo.GetSizeI(); i++)
    {
        if (UniformInfo[i].Name == name)
        {
            memcpy(UniformData + UniformInfo[i].Offset, v, n * sizeof(float));
            return true;
        }
    }

    return false;
}

void ShaderBase::InitUniforms(ID3D10Blob* s)
{
    HRESULT hr;

    UniformsSize = 0;
    if (UniformData)
    {
        OVR_FREE(UniformData);
        UniformData = 0;
    }

    Ptr<ID3D11ShaderReflection> ref;
    hr = D3DReflect(s->GetBufferPointer(), s->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&ref.GetRawRef());
    OVR_D3D_CHECK_RET(hr);

    ID3D11ShaderReflectionConstantBuffer* buf = ref->GetConstantBufferByIndex(0);
    D3D11_SHADER_BUFFER_DESC bufd = {};
    hr = buf->GetDesc(&bufd);
    if (FAILED(hr))
    {
        // This failure is normal - it means there are no constants in this shader.
        return;
    }

    for (unsigned i = 0; i < bufd.Variables; i++)
    {
        ID3D11ShaderReflectionVariable* var = buf->GetVariableByIndex(i);
        if (var)
        {
            D3D11_SHADER_VARIABLE_DESC vd;
            hr = var->GetDesc(&vd);
            OVR_D3D_CHECK_RET(hr);

            Uniform u;
            u.Name = vd.Name;
            u.Offset = vd.StartOffset;
            u.Size = vd.Size;
            UniformInfo.PushBack(u);
        }
    }

    UniformsSize = bufd.Size;
    UniformData = (unsigned char*)OVR_ALLOC(bufd.Size);
}

void ShaderBase::UpdateBuffer(Buffer* buf)
{
    if (UniformsSize)
    {
        if (!buf->Data(Buffer_Uniform, UniformData, UniformsSize))
        {
            OVR_ASSERT(false);
        }
    }
}

void RenderDevice::SetCommonUniformBuffer(int i, Render::Buffer* buffer)
{
    CommonUniforms[i] = (Buffer*)buffer;

    Context->PSSetConstantBuffers(1, 1, &CommonUniforms[1]->D3DBuffer.GetRawRef());
    Context->VSSetConstantBuffers(1, 1, &CommonUniforms[1]->D3DBuffer.GetRawRef());
}

Render::Shader *RenderDevice::LoadBuiltinShader(ShaderStage stage, int shader)
{
    switch (stage)
    {
    case Shader_Vertex:
        return VertexShaders[shader];
    case Shader_Pixel:
        return PixelShaders[shader];
    default:
        OVR_ASSERT(false);
        return NULL;
    }
}

Fill* RenderDevice::GetSimpleFill(int flags)
{
    OVR_UNUSED(flags);
    return DefaultFill;
}

Fill* RenderDevice::GetTextureFill(Render::Texture* t, bool useAlpha, bool usePremult)
{
    Fill *f = DefaultTextureFill;
    if ( usePremult )
    {
        f = DefaultTextureFillPremult;
    }
    else if ( useAlpha )
    {
        f = DefaultTextureFillAlpha;
    }
	f->SetTexture(0, t);
	return f;
}



// Textures

ID3D11SamplerState* RenderDevice::GetSamplerState(int sm)
{
    HRESULT hr;

    if (SamplerStates[sm])
        return SamplerStates[sm];

    D3D11_SAMPLER_DESC ss;
    memset(&ss, 0, sizeof(ss));
    if (sm & Sample_Clamp)
        ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    else if (sm & Sample_ClampBorder)
        ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    else
        ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

    if (sm & Sample_Nearest)
    {
        ss.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    }
    else if (sm & Sample_Anisotropic)
    {
        ss.Filter = D3D11_FILTER_ANISOTROPIC;
        ss.MaxAnisotropy = 4;
    }
    else
    {
        ss.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    }
    ss.MaxLOD = 15;
    hr = Device->CreateSamplerState(&ss, &SamplerStates[sm].GetRawRef());
    OVR_D3D_CHECK_RET_NULL(hr);
    return SamplerStates[sm];
}

Texture::Texture(ovrHmd hmd, RenderDevice* ren, int fmt, int w, int h) :
    Hmd(hmd),
    Ren(ren),
    TextureSet(NULL),
    MirrorTex(NULL),
    TexSv(NULL),
    TexRtv(NULL),
    TexDsv(NULL),
    TexStaging(NULL),
    Sampler(NULL),
    Format(fmt),
    Width(w),
    Height(h),
    Samples(0)
{
    Sampler = Ren->GetSamplerState(0);
}

Texture::~Texture()
{
    if (TextureSet)
    {
        ovr_DestroySwapTextureSet(Hmd, TextureSet);
        TextureSet = nullptr;
    }

    if (MirrorTex)
    {
        ovr_DestroyMirrorTexture(Hmd, MirrorTex);
        MirrorTex = nullptr;
    }
}

void Texture::Set(int slot, Render::ShaderStage stage) const
{
    Ren->SetTexture(stage, slot, this);
}

void Texture::SetSampleMode(int sm)
{
    Sampler = Ren->GetSamplerState(sm);
}

void RenderDevice::SetTexture(Render::ShaderStage stage, int slot, const Texture* t)
{
    if (MaxTextureSet[stage] <= slot)
        MaxTextureSet[stage] = slot + 1;

    ID3D11ShaderResourceView* sv = t ? t->GetSv() : NULL;
    switch (stage)
    {
    case Shader_Pixel:
        Context->PSSetShaderResources(slot, 1, &sv);
        if (t)
        {
            Context->PSSetSamplers(slot, 1, &t->Sampler.GetRawRef());
        }
        break;

    case Shader_Vertex:
        Context->VSSetShaderResources(slot, 1, &sv);
        if (t)
        {
            Context->VSSetSamplers(slot, 1, &t->Sampler.GetRawRef());
        }
        break;

    case Shader_Compute:
        Context->CSSetShaderResources(slot, 1, &sv);
        if (t)
        {
            Context->CSSetSamplers(slot, 1, &t->Sampler.GetRawRef());
        }
        break;

    default:
        OVR_ASSERT(false);
        break;

    }
}

void RenderDevice::GenerateSubresourceData(
    unsigned imageWidth, unsigned imageHeight, int format, unsigned imageDimUpperLimit,
    const void* rawBytes, D3D11_SUBRESOURCE_DATA* subresData,
    unsigned& largestMipWidth, unsigned& largestMipHeight, unsigned& byteSize, unsigned& effectiveMipCount)
{
    largestMipWidth = 0;
    largestMipHeight = 0;

    unsigned sliceLen = 0;
    unsigned rowLen = 0;
    unsigned numRows = 0;
    const byte* mipBytes = static_cast<const byte*>(rawBytes);

    unsigned index = 0;
    unsigned subresWidth = imageWidth;
    unsigned subresHeight = imageHeight;
    unsigned numMips = effectiveMipCount;

    unsigned bytesPerBlock = 0;
    switch (format)
    {
    case DXGI_FORMAT_BC1_UNORM_SRGB: // fall thru
    case DXGI_FORMAT_BC1_UNORM: bytesPerBlock = 8;  break;

    case DXGI_FORMAT_BC2_UNORM_SRGB: // fall thru
    case DXGI_FORMAT_BC2_UNORM: bytesPerBlock = 16;  break;

    case DXGI_FORMAT_BC3_UNORM_SRGB: // fall thru
    case DXGI_FORMAT_BC3_UNORM: bytesPerBlock = 16;  break;

    default:    OVR_ASSERT(false);
    }

    for (unsigned i = 0; i < numMips; i++)
    {

        unsigned blockWidth = 0;
        blockWidth = (subresWidth + 3) / 4;
        if (blockWidth < 1)
        {
            blockWidth = 1;
        }

        unsigned blockHeight = 0;
        blockHeight = (subresHeight + 3) / 4;
        if (blockHeight < 1)
        {
            blockHeight = 1;
        }

        rowLen = blockWidth * bytesPerBlock;
        numRows = blockHeight;
        sliceLen = rowLen * numRows;

        if (imageDimUpperLimit == 0 || (effectiveMipCount == 1) ||
            (subresWidth <= imageDimUpperLimit && subresHeight <= imageDimUpperLimit))
        {
            if (!largestMipWidth)
            {
                largestMipWidth = subresWidth;
                largestMipHeight = subresHeight;
            }

            subresData[index].pSysMem = (const void*)mipBytes;
            subresData[index].SysMemPitch = static_cast<UINT>(rowLen);
            subresData[index].SysMemSlicePitch = static_cast<UINT>(sliceLen);
            byteSize += sliceLen;
            ++index;
        }
        else
        {
            effectiveMipCount--;
        }

        mipBytes += sliceLen;

        subresWidth = subresWidth >> 1;
        subresHeight = subresHeight >> 1;
        if (subresWidth <= 0)
        {
            subresWidth = 1;
        }
        if (subresHeight <= 0)
        {
            subresHeight = 1;
        }
    }
}

static DXGI_FORMAT ConvertDXGIFormatToSrgb(DXGI_FORMAT inFormat)
{
    switch (inFormat)
    {
        // only a handful of types actually have sRGB versions
    case DXGI_FORMAT_R8G8B8A8_UNORM:            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_BC1_UNORM:                 return DXGI_FORMAT_BC1_UNORM_SRGB;     
    case DXGI_FORMAT_BC2_UNORM:                 return DXGI_FORMAT_BC2_UNORM_SRGB;     
    case DXGI_FORMAT_BC3_UNORM:                 return DXGI_FORMAT_BC3_UNORM_SRGB;     
    case DXGI_FORMAT_B8G8R8A8_UNORM:            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8X8_UNORM:            return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
    case DXGI_FORMAT_BC7_UNORM:                 return DXGI_FORMAT_BC7_UNORM_SRGB;     

    // The rest will just return the same format back
    default:                                    return inFormat;
    }
}

#define _256Megabytes 268435456
#define _512Megabytes 536870912

Texture* RenderDevice::CreateTexture(int format, int width, int height, const void* data, int mipcount, ovrResult* error)
{
    HRESULT hr;

    if(error != nullptr)
        *error = ovrSuccess;

    OVR_ASSERT(Device != NULL);

    size_t gpuMemorySize = 0;
    {
        IDXGIDevice* pDXGIDevice;
        hr = Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
        OVR_D3D_CHECK_RET_NULL(hr);
        IDXGIAdapter * pDXGIAdapter;
        hr = pDXGIDevice->GetAdapter(&pDXGIAdapter);
        OVR_D3D_CHECK_RET_NULL(hr);
        DXGI_ADAPTER_DESC adapterDesc;
        hr = pDXGIAdapter->GetDesc(&adapterDesc);
        OVR_D3D_CHECK_RET_NULL(hr);
        gpuMemorySize = adapterDesc.DedicatedVideoMemory;
        pDXGIAdapter->Release();
        pDXGIDevice->Release();
    }

    unsigned imageDimUpperLimit = 0;
    if (gpuMemorySize <= _256Megabytes)
    {
        imageDimUpperLimit = 512;
    }
    else if (gpuMemorySize <= _512Megabytes)
    {
        imageDimUpperLimit = 1024;
    }

    bool isDepth = ((format & Texture_DepthMask) != 0);

    if ((format & Texture_TypeMask) == Texture_DXT1 ||
        (format & Texture_TypeMask) == Texture_DXT3 ||
        (format & Texture_TypeMask) == Texture_DXT5)
    {
        int convertedFormat;
        if ((format & Texture_TypeMask) == Texture_DXT1)
        {
            convertedFormat = (format & Texture_SRGB) ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;
        }
        else if ((format & Texture_TypeMask) == Texture_DXT3)
        {
            convertedFormat = (format & Texture_SRGB) ? DXGI_FORMAT_BC2_UNORM_SRGB : DXGI_FORMAT_BC2_UNORM;
        }
        else if ((format & Texture_TypeMask) == Texture_DXT5)
        {
            convertedFormat = (format & Texture_SRGB) ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
        }
        else
        {
            OVR_ASSERT(false);  return NULL;
        }

        unsigned largestMipWidth = 0;
        unsigned largestMipHeight = 0;
        unsigned effectiveMipCount = mipcount;
        unsigned textureSize = 0;

        D3D11_SUBRESOURCE_DATA* subresData = (D3D11_SUBRESOURCE_DATA*)
            OVR_ALLOC(sizeof(D3D11_SUBRESOURCE_DATA) * mipcount);
        GenerateSubresourceData(width, height, convertedFormat, imageDimUpperLimit, data, subresData, largestMipWidth,
            largestMipHeight, textureSize, effectiveMipCount);
        TotalTextureMemoryUsage += textureSize;

        if (!Device || !subresData)
        {
            return NULL;
        }

        Ptr<Texture> NewTex = *new Texture(Hmd, this, format, largestMipWidth, largestMipHeight);
        // BCn/DXTn - no AA.
        NewTex->Samples = 1;

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = largestMipWidth;
        desc.Height = largestMipHeight;
        desc.MipLevels = effectiveMipCount;
        desc.ArraySize = 1;
        desc.Format = static_cast<DXGI_FORMAT>(convertedFormat);
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        NewTex->Tex = NULL;
        hr = Device->CreateTexture2D(&desc, static_cast<D3D11_SUBRESOURCE_DATA*>(subresData),
            &NewTex->Tex.GetRawRef());
        OVR_FREE(subresData);
        OVR_D3D_CHECK_RET_NULL(hr);

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        memset(&SRVDesc, 0, sizeof(SRVDesc));
        SRVDesc.Format = static_cast<DXGI_FORMAT>(convertedFormat);
        SRVDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = desc.MipLevels;

        Ptr<ID3D11ShaderResourceView> srv;
        hr = Device->CreateShaderResourceView(NewTex->Tex, NULL, &srv.GetRawRef());
        OVR_D3D_CHECK_RET_NULL(hr);

        NewTex->TexSv.PushBack(srv);

        NewTex->AddRef();
        return NewTex;
    }
    else
    {
        int samples = (format & Texture_SamplesMask);
        if (samples < 1)
        {
            samples = 1;
        }

        bool createDepthSrv = (format & Texture_SampleDepth) > 0;

        DXGI_FORMAT d3dformat;
        DXGI_FORMAT srvFormat;
        int         bpp;
        switch (format & Texture_TypeMask)
        {
        case Texture_BGRA:
            bpp = 4;
            d3dformat = (format & Texture_SRGB) ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
            srvFormat = d3dformat;
            break;
        case Texture_RGBA:
            bpp = 4;
            d3dformat = (format & Texture_SRGB) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
            srvFormat = d3dformat;
            break;
        case Texture_R:
            bpp = 1;
            d3dformat = DXGI_FORMAT_R8_UNORM;
            srvFormat = d3dformat;
            break;
        case Texture_A:
            bpp = 1;
            d3dformat = DXGI_FORMAT_A8_UNORM;
            srvFormat = d3dformat;
            break;
        case Texture_Depth32f:
            bpp = 0;
            d3dformat = createDepthSrv ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_D32_FLOAT;
            srvFormat = DXGI_FORMAT_R32_FLOAT;
            break;
        case Texture_Depth24Stencil8:
            bpp = 0;
            d3dformat = createDepthSrv ? DXGI_FORMAT_R24G8_TYPELESS: DXGI_FORMAT_D24_UNORM_S8_UINT;
            srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            break;
        case Texture_Depth16:
            bpp = 0;
            d3dformat = createDepthSrv ? DXGI_FORMAT_R16_TYPELESS : DXGI_FORMAT_D16_UNORM;
            srvFormat = DXGI_FORMAT_R16_UNORM;
            break;
        case Texture_Depth32fStencil8:
            bpp = 0;
            d3dformat = createDepthSrv ? DXGI_FORMAT_R32G8X24_TYPELESS : DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
            srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            break;
        default:
            OVR_ASSERT(false);
            return NULL;
        }

        Ptr<Texture> NewTex = *new Texture(Hmd, this, format, width, height);
        NewTex->Samples = samples;

        D3D11_TEXTURE2D_DESC dsDesc;
        dsDesc.Width = width;
        dsDesc.Height = height;
        dsDesc.MipLevels = (((format & Texture_GenMipmaps)!=0) && data) ? GetNumMipLevels(width, height) : 1;
        dsDesc.ArraySize = 1;
        dsDesc.Format = d3dformat;
        dsDesc.SampleDesc.Count = samples;
        dsDesc.SampleDesc.Quality = 0;
        dsDesc.Usage = D3D11_USAGE_DEFAULT;
        dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        dsDesc.CPUAccessFlags = 0;
        dsDesc.MiscFlags = 0;
        
        if (isDepth)
        {
            dsDesc.BindFlags = createDepthSrv ? (dsDesc.BindFlags | D3D11_BIND_DEPTH_STENCIL) : D3D11_BIND_DEPTH_STENCIL;
        }
        else if (format & Texture_RenderTarget)
        {
            dsDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        }

        int count = 1;
        
        // Only need to create full texture set for render targets
        if (format & Texture_Mirror)
        {
            D3D11_TEXTURE2D_DESC sdkTextureDesc = dsDesc;
            // Override the format to be sRGB so that the compositor always treats eye buffers
            // as if they're sRGB even if we are sending in linear format textures
            sdkTextureDesc.Format = ConvertDXGIFormatToSrgb(srvFormat);

            // Create typeless when we are rendering as non-sRGB since we will override the texture format in the RTV
            bool reinterpretSrgbAsLinear = (format & Texture_SRGB) == 0;
            unsigned compositorTextureFlags = 0;
            if (reinterpretSrgbAsLinear)
                compositorTextureFlags |= ovrSwapTextureSetD3D11_Typeless;

            ovrResult result = ovr_CreateMirrorTextureD3D11(Hmd, Device, &sdkTextureDesc, compositorTextureFlags, &NewTex->MirrorTex);
            if (error != nullptr)
                *error = result;

            if (result == ovrError_DisplayLost || !NewTex->MirrorTex)
            {
                OVR_ASSERT(false);
                return NULL;
            }

            ovrD3D11Texture* tex = (ovrD3D11Texture*)NewTex->MirrorTex;
            NewTex->Tex = tex->D3D11.pTexture;

            // If we are overriding the texture format
            // then ignore the SRV the SDK returns us and create our own.
            if (reinterpretSrgbAsLinear)
            {
                D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
                srvd.Format = dsDesc.Format;
                srvd.ViewDimension = dsDesc.SampleDesc.Count > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
                srvd.Texture2D.MostDetailedMip = 0;
                srvd.Texture2D.MipLevels = dsDesc.MipLevels;

                Ptr<ID3D11ShaderResourceView> srv;
                hr = Device->CreateShaderResourceView(NewTex->Tex, &srvd, &srv.GetRawRef());
                OVR_D3D_CHECK_RET_NULL(hr);

                NewTex->TexSv.PushBack(srv);
            }
            else
            {
                NewTex->TexSv.PushBack(tex->D3D11.pSRView);
            }

            NewTex->AddRef();
            return NewTex;
        }
        else if (format & Texture_SwapTextureSet)
        {
            D3D11_TEXTURE2D_DESC sdkTextureDesc = dsDesc;
            // Override the format to be sRGB so that the compositor always treats eye buffers
            // as if they're sRGB even if we are sending in linear formatted textures
            sdkTextureDesc.Format = ConvertDXGIFormatToSrgb(srvFormat);

            // Can do this with rendertargets, depth buffers, or normal textures, but *not* MSAA color swap buffers.
            OVR_ASSERT((samples == 1) || isDepth);

            // Create typeless when we are rendering as non-sRGB since we will override the texture format in the RTV
            unsigned compositorTextureFlags = 0;
            if ((format & Texture_SRGB) == 0)
                compositorTextureFlags |= ovrSwapTextureSetD3D11_Typeless;

            ovrResult result = ovr_CreateSwapTextureSetD3D11(Hmd, Device.GetPtr(), &sdkTextureDesc, compositorTextureFlags, &NewTex->TextureSet);
            if (error != nullptr)
                *error = result;
            
            if (result == ovrError_DisplayLost || !NewTex->TextureSet)
            {
                OVR_ASSERT(false);
                return NULL;
            }

            // Sanity-check - find out what type of resource we actually got back.
            Ptr<ID3D11Texture2D> tex = ((ovrD3D11Texture*)&NewTex->TextureSet->Textures[0])->D3D11.pTexture;
            D3D11_TEXTURE2D_DESC desc;
            tex->GetDesc ( &desc );

            count = NewTex->TextureSet->TextureCount;
        }
        else
        {
            NewTex->Tex = NULL;
            hr = Device->CreateTexture2D(&dsDesc, nullptr, &NewTex->Tex.GetRawRef());
            OVR_D3D_CHECK_RET_NULL(hr);
        }

        for (int i = 0; i < count; ++i)
        {
            Ptr<ID3D11Texture2D> tex = NewTex->Tex ? NewTex->Tex : ((ovrD3D11Texture*)&NewTex->TextureSet->Textures[i])->D3D11.pTexture;

            if (dsDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
            {
                Ptr<ID3D11ShaderResourceView> srv;

                if (dsDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC depthSrv;
                    depthSrv.Format = srvFormat;
                    switch (d3dformat)
                    {
                    case DXGI_FORMAT_R32_TYPELESS:      depthSrv.Format = DXGI_FORMAT_R32_FLOAT;    break;
                    case DXGI_FORMAT_R24G8_TYPELESS:    depthSrv.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;    break;
                    case DXGI_FORMAT_R16_TYPELESS:      depthSrv.Format = DXGI_FORMAT_R16_UNORM;    break;
                    case DXGI_FORMAT_R32G8X24_TYPELESS: depthSrv.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS; break;

                    default:    OVR_ASSERT(false);      depthSrv.Format = DXGI_FORMAT_R32_FLOAT;
                    }
                    depthSrv.ViewDimension = samples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
                    depthSrv.Texture2D.MostDetailedMip = 0;
                    depthSrv.Texture2D.MipLevels = dsDesc.MipLevels;

                    hr = Device->CreateShaderResourceView(tex, &depthSrv, &srv.GetRawRef());
                    OVR_D3D_CHECK_RET_NULL(hr);
                }
                else
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
                    srvd.Format = dsDesc.Format;
                    srvd.ViewDimension = samples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
                    srvd.Texture2D.MostDetailedMip = 0;
                    srvd.Texture2D.MipLevels = dsDesc.MipLevels;

                    hr = Device->CreateShaderResourceView(tex, &srvd, &srv.GetRawRef());
                    OVR_D3D_CHECK_RET_NULL(hr);
                }

                NewTex->TexSv.PushBack(srv);
            }

            if (data)
            {
                Context->UpdateSubresource(tex, 0, NULL, data, width * bpp, width * height * bpp);
                if (format & Texture_GenMipmaps)
                {
                    OVR_ASSERT ( ( format & Texture_TypeMask ) == Texture_RGBA );
                    int srcw = width, srch = height;
                    int level = 0;
                    uint8_t* mipmaps = NULL;
                    do
                    {
                        level++;
                        int mipw = srcw >> 1;
                        if (mipw < 1)
                        {
                            mipw = 1;
                        }
                        int miph = srch >> 1;
                        if (miph < 1)
                        {
                            miph = 1;
                        }
                        if (mipmaps == NULL)
                        {
                            mipmaps = (uint8_t*)OVR_ALLOC(mipw * miph * 4);
                        }
                        FilterRgba2x2(level == 1 ? (const uint8_t*)data : mipmaps, srcw, srch, mipmaps);
                        Context->UpdateSubresource(tex, level, NULL, mipmaps, mipw * bpp, miph * bpp);
                        srcw = mipw;
                        srch = miph;
                    } while (srcw > 1 || srch > 1);

                    if (mipmaps != NULL)
                    {
                        OVR_FREE(mipmaps);
                    }
                }
            }

            if (isDepth)
            {
                D3D11_DEPTH_STENCIL_VIEW_DESC depthDsv;
                ZeroMemory(&depthDsv, sizeof(depthDsv));
                switch (format & Texture_DepthMask)
                {
                case Texture_Depth32f:          depthDsv.Format = DXGI_FORMAT_D32_FLOAT;            break;
                case Texture_Depth24Stencil8:   depthDsv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;    break;
                case Texture_Depth16:           depthDsv.Format = DXGI_FORMAT_D16_UNORM;            break;
                case Texture_Depth32fStencil8:  depthDsv.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT; break;

                default:    OVR_ASSERT(false);  depthDsv.Format = DXGI_FORMAT_D32_FLOAT;
                }
                depthDsv.ViewDimension = samples > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
                depthDsv.Texture2D.MipSlice = 0;

                Ptr<ID3D11DepthStencilView> dsv;
                hr = Device->CreateDepthStencilView(tex, &depthDsv, &dsv.GetRawRef());
                OVR_D3D_CHECK_RET_NULL(hr);
                NewTex->TexDsv.PushBack(dsv);
            }
            else if (format & Texture_RenderTarget)
            {
                D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
                rtvd.Format = dsDesc.Format;
                rtvd.Texture2D.MipSlice = 0;
                rtvd.ViewDimension = dsDesc.SampleDesc.Count > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;

                Ptr<ID3D11RenderTargetView> rtv;
                hr = Device->CreateRenderTargetView(tex, &rtvd, &rtv.GetRawRef());
                OVR_D3D_CHECK_RET_NULL(hr);
                NewTex->TexRtv.PushBack(rtv);
            }
        }

        NewTex->AddRef();
        return NewTex;
    }
}

// Rendering

void RenderDevice::ResolveMsaa(OVR::Render::Texture* msaaTex, OVR::Render::Texture* outputTex)
{
    OVR_ASSERT_M(!(((Texture*)msaaTex)->Format & Texture_DepthMask), "Resolving depth buffers not supported.");

    DXGI_FORMAT resolveFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (((Texture*)msaaTex)->Format & Texture_SRGB)
    {
        resolveFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }

    Context->ResolveSubresource(((Texture*)outputTex)->GetTex(), 0, ((Texture*)msaaTex)->GetTex(), 0, resolveFormat);
}

void RenderDevice::SetCullMode(CullMode cullMode)
{
    switch (cullMode)
    {
    case OVR::Render::RenderDevice::Cull_Off:
        Context->RSSetState(RasterizerCullOff);
        break;
    case OVR::Render::RenderDevice::Cull_Back:
        Context->RSSetState(RasterizerCullBack);
        break;
    case OVR::Render::RenderDevice::Cull_Front:
        Context->RSSetState(RasterizerCullFront);
        break;
    default:
        break;
    }
}

void RenderDevice::BeginRendering()
{
    Context->RSSetState(RasterizerCullBack);
}

void RenderDevice::SetRenderTarget(Render::Texture* color, Render::Texture* depth, Render::Texture* stencil)
{
    OVR_UNUSED(stencil);

    CurRenderTarget = (Texture*)color;
    if (color == NULL)
    {
        Texture* newDepthBuffer = GetDepthBuffer(WindowWidth, WindowHeight, 1, Texture_Depth32f);
        if (newDepthBuffer == NULL)
        {
            OVR_DEBUG_LOG(("New depth buffer creation failed."));
        }
        if (newDepthBuffer != NULL)
        {
            CurDepthBuffer = GetDepthBuffer(WindowWidth, WindowHeight, 1, Texture_Depth32f);
            Context->OMSetRenderTargets(1, &BackBufferRT.GetRawRef(), CurDepthBuffer->GetDsv());
        }
        return;
    }
    if (depth == NULL)
    {
        depth = GetDepthBuffer(color->GetWidth(), color->GetHeight(), CurRenderTarget->Samples, Texture_Depth32f);
    }

    ID3D11ShaderResourceView* sv[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    if (MaxTextureSet[Shader_Fragment])
    {
        Context->PSSetShaderResources(0, MaxTextureSet[Shader_Fragment], sv);
    }
    memset(MaxTextureSet, 0, sizeof(MaxTextureSet));

    CurDepthBuffer = (Texture*)depth;
	
    Context->OMSetRenderTargets(1, &((Texture*)color)->GetRtv().GetRawRef(), ((Texture*)depth)->GetDsv());
}

void RenderDevice::SetWorldUniforms(const Matrix4f& proj)
{
    StdUniforms.Proj = proj.Transposed();
    // Shader constant buffers cannot be partially updated.
}

void RenderDevice::Blt(Render::Texture* texture)
{
    Texture* tex = (Texture*)texture;
    Blitter->Blt(BackBufferRT.GetPtr(), tex->GetSv().GetPtr());
}

void RenderDevice::Render(const Matrix4f& matrix, Model* model)
{
    // Store data in buffers if not already
    if (!model->VertexBuffer)
    {
        Ptr<Buffer> vb = *CreateBuffer();
        if (!vb->Data(Buffer_Vertex | Buffer_ReadOnly, &model->Vertices[0], model->Vertices.GetSize() * sizeof(Vertex)))
        {
            OVR_ASSERT(false);
        }
        model->VertexBuffer = vb;
    }
    if (!model->IndexBuffer)
    {
        Ptr<Buffer> ib = *CreateBuffer();
        if (!ib->Data(Buffer_Index | Buffer_ReadOnly, &model->Indices[0], model->Indices.GetSize() * 2))
        {
            OVR_ASSERT(false);
        }
        model->IndexBuffer = ib;
    }

    Render(model->Fill ? model->Fill : DefaultFill,
        model->VertexBuffer, model->IndexBuffer,
        matrix, 0, (unsigned)model->Indices.GetSize(), model->GetPrimType());
}

void RenderDevice::RenderWithAlpha(const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
    const Matrix4f& matrix, int offset, int count, PrimitiveType rprim)
{
    Context->OMSetBlendState(BlendState, NULL, 0xffffffff);
    Render(fill, vertices, indices, matrix, offset, count, rprim);
    Context->OMSetBlendState(NULL, NULL, 0xffffffff);
}

void RenderDevice::Render(const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
    const Matrix4f& matrix, int offset, int count, PrimitiveType rprim, MeshType meshType/* = Mesh_Scene*/)
{
    ID3D11Buffer* vertexBuffer = ((Buffer*)vertices)->GetBuffer();
    UINT vertexOffset = offset;
    UINT vertexStride = sizeof(Vertex);
    switch (meshType)
    {
    case Mesh_Scene:
        Context->IASetInputLayout(ModelVertexIL);
        vertexStride = sizeof(Vertex);
        break;
    case Mesh_Distortion:
        Context->IASetInputLayout(DistortionVertexIL);
        vertexStride = sizeof(DistortionVertex);
        break;
    case Mesh_Heightmap:
        Context->IASetInputLayout(HeightmapVertexIL);
        vertexStride = sizeof(HeightmapVertex);
        break;
    default: OVR_ASSERT(false);
    }

    Context->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);

    if (indices)
    {
        Context->IASetIndexBuffer(((Buffer*)indices)->GetBuffer(), DXGI_FORMAT_R16_UINT, 0);
    }

    ShaderSet* shaders = ((ShaderFill*)fill)->GetShaders();

    ShaderBase* vshader = ((ShaderBase*)shaders->GetShader(Shader_Vertex));
    unsigned char* vertexData = vshader->UniformData;
    if (vertexData != NULL)
    {
        // TODO: some VSes don't start with StandardUniformData!
        if (vshader->UniformsSize >= sizeof(StandardUniformData))
        {
            StandardUniformData* stdUniforms = (StandardUniformData*)vertexData;
            stdUniforms->View = matrix.Transposed();
            stdUniforms->Proj = StdUniforms.Proj;
        }

        if (!UniformBuffers[Shader_Vertex]->Data(Buffer_Uniform, vertexData, vshader->UniformsSize))
        {
            OVR_ASSERT(false);
        }
        vshader->SetUniformBuffer(UniformBuffers[Shader_Vertex]);
    }

    for (int i = Shader_Vertex + 1; i < Shader_Count; i++)
    {
        if (shaders->GetShader(i))
        {
            ((ShaderBase*)shaders->GetShader(i))->UpdateBuffer(UniformBuffers[i]);
            ((ShaderBase*)shaders->GetShader(i))->SetUniformBuffer(UniformBuffers[i]);
        }
    }

    D3D11_PRIMITIVE_TOPOLOGY prim;
    switch (rprim)
    {
    case Prim_Triangles:
        prim = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        break;
    case Prim_Lines:
        prim = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        break;
    case Prim_TriangleStrip:
        prim = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        break;
    default:
        OVR_ASSERT(0);
        return;
    }
    Context->IASetPrimitiveTopology(prim);

    fill->Set(rprim);
    if (ExtraShaders)
    {
        ExtraShaders->Set(rprim);
    }

    if (indices)
    {
        Context->DrawIndexed(count, 0, 0);
    }
    else
    {
        Context->Draw(count, 0);
    }
}

size_t RenderDevice::QueryGPUMemorySize()
{
    HRESULT hr;

    Ptr<IDXGIDevice> pDXGIDevice;
    hr = Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice.GetRawRef());
    OVR_D3D_CHECK_RET_VAL(hr, 0);

    Ptr<IDXGIAdapter> pDXGIAdapter;
    hr = pDXGIDevice->GetAdapter(&pDXGIAdapter.GetRawRef());
    OVR_D3D_CHECK_RET_VAL(hr, 0);

    DXGI_ADAPTER_DESC adapterDesc;
    hr = pDXGIAdapter->GetDesc(&adapterDesc);
    OVR_D3D_CHECK_RET_VAL(hr, 0);

    return adapterDesc.DedicatedVideoMemory;
}


void RenderDevice::Present(bool withVsync)
{
    for (int i = 0; i < 4; ++i)
    {
        if (Util::ImageWindow::GlobalWindow(i))
        {
            Util::ImageWindow::GlobalWindow(i)->Process();
        }
    }

    HRESULT hr = SwapChain->Present((withVsync ? 1 : 0), 0);
    OVR_D3D_CHECK_RET(hr);
}

void RenderDevice::Flush()
{
    Context->Flush();
}

void RenderDevice::FillRect(float left, float top, float right, float bottom, Color c, const Matrix4f* view)
{
    Context->OMSetBlendState(BlendState, NULL, 0xffffffff);
    OVR::Render::RenderDevice::FillRect(left, top, right, bottom, c, view);
    Context->OMSetBlendState(NULL, NULL, 0xffffffff);
}

void RenderDevice::RenderText(const struct Font* font, const char* str, float x, float y, float size, Color c, const Matrix4f* view)
{
    Context->OMSetBlendState(BlendState, NULL, 0xffffffff);
    OVR::Render::RenderDevice::RenderText(font, str, x, y, size, c, view);
    Context->OMSetBlendState(NULL, NULL, 0xffffffff);
}

void RenderDevice::FillGradientRect(float left, float top, float right, float bottom, Color col_top, Color col_btm, const Matrix4f* view)
{
    Context->OMSetBlendState(BlendState, NULL, 0xffffffff);
    OVR::Render::RenderDevice::FillGradientRect(left, top, right, bottom, col_top, col_btm, view);
    Context->OMSetBlendState(NULL, NULL, 0xffffffff);
}

void RenderDevice::RenderImage(float left, float top, float right, float bottom, ShaderFill* image, unsigned char alpha, const Matrix4f* view)
{
    Context->OMSetBlendState(BlendState, NULL, 0xffffffff);
    OVR::Render::RenderDevice::RenderImage(left, top, right, bottom, image, alpha, view);
    Context->OMSetBlendState(NULL, NULL, 0xffffffff);
}

void RenderDevice::BeginGpuEvent(const char* markerText, uint32_t markerColor)
{
#if GPU_PROFILING
    OVR_UNUSED(markerColor);

    WCHAR wStr[255];
    size_t newStrLen = 0;
    mbstowcs_s(&newStrLen, wStr, markerText, 255);
    LPCWSTR pwStr = wStr;

    if (UserAnnotation)
        UserAnnotation->BeginEvent(pwStr);

#else
    OVR_UNUSED(markerText);
    OVR_UNUSED(markerColor);
#endif
}

void RenderDevice::EndGpuEvent()
{
#if GPU_PROFILING
    if (UserAnnotation)
        UserAnnotation->EndEvent();
#endif
}


}}} // namespace OVR::Render::D3D11
