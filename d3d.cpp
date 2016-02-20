#if USE_D3D

/*#ifdef _DEBUG
#pragma comment( lib, "oculus/LibOVR/Lib/Win32/libovrd.lib" )
#else
#pragma comment( lib, "oculus/LibOVR/Lib/Win32/libovr.lib" )
#endif*/

#ifdef _DEBUG
#pragma comment( lib, "oculus/LibOVR/Lib/Win32/VS2013/libovrd.lib" )
#else
#pragma comment( lib, "oculus/LibOVR/Lib/Win32/VS2013/libovr.lib" )
#endif

#include "aggdraw.h"
#include "GPU_osd.h"
#include "main.h"
#include "lua-engine.h"
#include "mednafen/src/mednafen.h"
#include "pcejin.h"
#include "vb.h"

#include "OVR_CAPI.h"
#include "OculusSDK\Samples\CommonSrc\Render\Render_D3D11_Device.h"
#define OVR_D3D_VERSION 11
#include "OVR_CAPI_D3D.h"
using namespace OVR;
ovrHmd             HMD;
ovrEyeRenderDesc   EyeRenderDesc[2];
ovrD3D11Texture    EyeTexture[2];

Render::D3D11::RenderDevice*      pRender = 0;
Render::Texture*           pRenderTargetTexture = 0;
Render::Texture*		   vbEyeTexture = 0;
Render::ShaderFill*		   vbShaderFill = 0;
ovrRecti           EyeRenderViewport[2];
Render::Buffer*			   QuadVertexBuffer;

static void __forceinline convert32( int eye ){
	uint8 *pb_ptr = (uint8*)convert_buffer;
	MDFN_Surface* eyeSurface = eye == 0 ? espec.surface : espec.surface2;

	for(int y = espec.DisplayRect.y; y < espec.DisplayRect.y + espec.DisplayRect.h; y++)
	{
		uint16 meow_width = (espec.LineWidths[0].w == ~0) ? espec.DisplayRect.w : espec.LineWidths[y].w;
		int meow_x = (espec.LineWidths[0].w == ~0) ? espec.DisplayRect.x : espec.LineWidths[y].x;
		uint32 *fb_line = eyeSurface->pixels + y * (MDFNGameInfo->pitch >> 2) + meow_x;

		for(int x = 0; x < meow_width; x++)
		{
			uint32 pixel = fb_line[x];

			int r, g, b;

			eyeSurface->DecodeColor(pixel, r, g, b);

			*pb_ptr++ = r;
			*pb_ptr++ = g;
			*pb_ptr++ = b;
			*pb_ptr++ = 255;
		}
	}
}

void OculusInit()
{
	ovr_Initialize();
}

void SetupOculus( bool warnIfNotFound )
{
	HMD = ovrHmd_Create( 0 );

	if ( warnIfNotFound )
	{
		if ( !HMD )
		{
			MessageBoxA( NULL, "Oculus Rift not detected.", "", MB_OK );
			return;
		}
		if ( HMD->ProductName[0] == '\0' )
		{
			MessageBoxA( NULL, "Rift detected, display not enabled.", "", MB_OK );
			return;
		}
	}
}

bool D3DInit()
{
	SetupOculus( MDFN_IEN_VB::GetSplitMode() == MDFN_IEN_VB::VB3DMODE_OVR );
	bool UseAppWindowFrame = ( HMD->HmdCaps & ovrHmdCap_ExtendDesktop ) ? false : true;
	RendererParams rendererParams;
	rendererParams.Resolution = HMD->Resolution;
	rendererParams.Multisample = 1;
	rendererParams.Fullscreen = true;
	pRender = RenderDevice::CreateDevice( rendererParams, (void*)g_hWnd );
	ovrHmd_AttachToWindow( HMD, g_hWnd, NULL, NULL );

	vbEyeTexture = pRender->CreateTexture( Texture_RGBA | 1, pcejin.width, pcejin.height, NULL );
	vbShaderFill = pRender->CreateTextureFill( vbEyeTexture );

	Sizei recommenedTex0Size = ovrHmd_GetFovTextureSize( HMD, ovrEye_Left, HMD->DefaultEyeFov[0], 1.0f );
	Sizei recommenedTex1Size = ovrHmd_GetFovTextureSize( HMD, ovrEye_Right, HMD->DefaultEyeFov[1], 1.0f );
	Sizei RenderTargetSize;
	RenderTargetSize.w = recommenedTex0Size.w + recommenedTex1Size.w;
	RenderTargetSize.h = max( recommenedTex0Size.h, recommenedTex1Size.h );
	pRenderTargetTexture = pRender->CreateTexture( Texture_RGBA | Texture_RenderTarget |
		1,
		RenderTargetSize.w, RenderTargetSize.h, NULL );
	RenderTargetSize.w = pRenderTargetTexture->GetWidth();
	RenderTargetSize.h = pRenderTargetTexture->GetHeight();

	ovrFovPort eyeFov[2] = { HMD->DefaultEyeFov[0], HMD->DefaultEyeFov[1] };
	EyeRenderViewport[0].Pos = Vector2i( 0, 0 );
	EyeRenderViewport[0].Size = Sizei( RenderTargetSize.w / 2, RenderTargetSize.h );
	EyeRenderViewport[1].Pos = Vector2i( ( RenderTargetSize.w + 1 ) / 2, 0 );
	EyeRenderViewport[1].Size = EyeRenderViewport[0].Size;

	EyeTexture[0].D3D11.Header.API = ovrRenderAPI_D3D11;
	EyeTexture[0].D3D11.Header.TextureSize = RenderTargetSize;
	EyeTexture[0].D3D11.Header.RenderViewport = EyeRenderViewport[0];
	EyeTexture[0].D3D11.pTexture = pRenderTargetTexture->Tex.GetPtr();
	EyeTexture[0].D3D11.pSRView = pRenderTargetTexture->TexSv.GetPtr();	

	// Right eye uses the same texture, but different rendering viewport.
	EyeTexture[1] = EyeTexture[0];
	EyeTexture[1].D3D11.Header.RenderViewport = EyeRenderViewport[1];

	ovrD3D11Config d3d11cfg;
	d3d11cfg.D3D11.Header.API = ovrRenderAPI_D3D11;
	d3d11cfg.D3D11.Header.RTSize = Sizei( HMD->Resolution.w, HMD->Resolution.h );
	d3d11cfg.D3D11.Header.Multisample = 1;
	d3d11cfg.D3D11.pDevice = pRender->Device;
	d3d11cfg.D3D11.pDeviceContext = pRender->Context;
	d3d11cfg.D3D11.pBackBufferRT = pRender->BackBufferRT;
	d3d11cfg.D3D11.pSwapChain = pRender->SwapChain;	

	ovrHmd_SetEnabledCaps( HMD, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction );

	if ( !ovrHmd_ConfigureRendering( HMD, &d3d11cfg.Config,
		ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette |
		/*ovrDistortionCap_TimeWarp |*/ ovrDistortionCap_Overdrive,
		eyeFov, EyeRenderDesc ) )
	{
		MessageBoxA( NULL, "ovrHmd_ConfigureRendering failed", "", MB_OK );
		return false;
	}

	ovrHmd_ConfigureTracking( HMD, ovrTrackingCap_Orientation |
		ovrTrackingCap_MagYawCorrection |
		ovrTrackingCap_Position, 0 );

	QuadVertexBuffer = pRender->CreateBuffer();
	const Vertex QuadVertices[] =
	{ Vertex( Vector3f( -0.5, 0.5, 0 ), Color( 255, 255, 255, 255 ), 0.0f, 0.0f ), Vertex( Vector3f( 0.5, 0.5, 0 ), Color( 255, 255, 255, 255 ), 1.0f, 0.0f ),
	Vertex( Vector3f( -0.5, -0.5, 0 ), Color( 255, 255, 255, 255 ), 0.0f, 1.0f ), Vertex( Vector3f( 0.5, -0.5, 0 ), Color( 255, 255, 255, 255 ), 1.0f, 1.0f ) };
	QuadVertexBuffer->Data( Buffer_Vertex, QuadVertices, sizeof( QuadVertices ) );

	return true;
}

static void UpdateTexture( ovrEyeType eye )
{
	pRender->Context->UpdateSubresource( vbEyeTexture->Tex, 0, NULL, convert_buffer, vbEyeTexture->Width * 4, vbEyeTexture->Width * vbEyeTexture->Height * 4 );
}

void render( ovrEyeType eye )
{
	convert32( eye == ovrEye_Left ? 1 : 0 );

	CallRegisteredLuaFunctions( LUACALL_AFTEREMULATIONGUI );

	osd->update();
	DrawHUD();
	osd->clear();

	aggDraw.hud->attach( convert_buffer, pcejin.width, pcejin.height, 4 * pcejin.width );
	UpdateTexture( eye );
}

void render()
{
	ovrPosef eyeRenderPose[2];

	if( !pcejin.romLoaded || espec.skip )
	{
		return;
	}

	if( ( pcejin.width != espec.DisplayRect.w ) || ( pcejin.height != espec.DisplayRect.h ) )
	{
		pcejin.width = espec.DisplayRect.w;
		pcejin.height = espec.DisplayRect.h;
		if( !pcejin.maximized )
		{
			ScaleScreen( (float)pcejin.windowSize );
		}
	}

	SetInputDisplayCharacters(pcejin.pads);

	if ( MDFN_IEN_VB::GetSplitMode() == MDFN_IEN_VB::VB3DMODE_OVR )
	{
		ovrHmd_BeginFrame( HMD, 0 );
		pRender->BeginScene();
		ovrHmd_GetTrackingState( HMD, 0 );
		for ( int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++ )
		{
			ovrEyeType eye = HMD->EyeRenderOrder[eyeIndex];
			render( eye );
			pRender->SetRenderTarget( pRenderTargetTexture );
			pRender->SetViewport( Recti( EyeRenderViewport[eye] ) );
			pRender->Clear();
			pRender->SetProjection( Matrix4f() );
			pRender->SetDepthMode( false, false );
			float aspectRatio = (float)pcejin.width / pcejin.height / ( (float)EyeRenderViewport[eye].Size.w / EyeRenderViewport[eye].Size.h );
			Matrix4f view = Matrix4f( 1, 0, 0, 0,
				0, 1 / aspectRatio, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1 );
			pRender->Render( vbShaderFill, QuadVertexBuffer, NULL, sizeof( Vertex ), view, 0, 4, Prim_TriangleStrip );
		}
		pRender->FinishScene();		
		ovrHmd_EndFrame( HMD, eyeRenderPose, &EyeTexture[0].Texture );
	}
	else
	{
		render( ovrEye_Right );
		pRender->SetDefaultRenderTarget();
		pRender->SetFullViewport();
		pRender->Clear();
		pRender->SetProjection( Matrix4f() );
		pRender->SetDepthMode( false, false );
		float aspectRatio = (float)pcejin.width / pcejin.height / ( (float)pRender->D3DViewport.Width / pRender->D3DViewport.Height );
		Matrix4f view = Matrix4f( 2, 0, 0, 0,
			0, 2 / aspectRatio, 0, 0,
			0, 0, 2, 0,
			0, 0, 0, 1 );
		pRender->Render( vbShaderFill, QuadVertexBuffer, NULL, sizeof( Vertex ), view, 0, 4, Prim_TriangleStrip );
		pRender->Present( true );
	}
}

void DismissHSWDisplay()
{
	ovrHmd_DismissHSWDisplay( HMD );
}

#endif // #if USE_D3D