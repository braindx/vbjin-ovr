#if USE_D3D

/*#ifdef _DEBUG
#pragma comment( lib, "oculus/LibOVR/Lib/Win32/libovrd.lib" )
#else
#pragma comment( lib, "oculus/LibOVR/Lib/Win32/libovr.lib" )
#endif*/

#ifdef _DEBUG
#pragma comment( lib, "OculusSDK/LibOVR/Lib/Windows/Win32/Debug/VS2013/LibOVR.lib" )
#else
#pragma comment( lib, "OculusSDK/LibOVR/Lib/Windows/Win32/Debug/VS2013/LibOVR.lib" )
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
ovrHmdDesc			HmdDesc;
ovrGraphicsLuid  pLuid;
ovrEyeRenderDesc   EyeRenderDesc[2];
//ovrD3D11Texture    EyeTexture[2];

ovrTexture     * mirrorTexture = nullptr;
Render::D3D11::RenderDevice*      pRender = 0;
Render::D3D11::Texture*           pRenderTargetTexture[2] = { 0 };
 

Render::D3D11::Texture*		   vbEyeTexture = 0;
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
	ovrResult error = ovr_Initialize(NULL);
	if (!OVR_SUCCESS(error))
	{ 
		char c[256] = { 0 };
		sprintf(c, "ovr_Initialize Failed. error code:%d", error);
		MessageBoxA(NULL, c, "", MB_OK); 
		return;
	}
	else
	{
		printf("ovr_Initialize success.\n");
	}
}

void SetupOculus( bool warnIfNotFound )
{
	
	//HMD = ovrHmd_Create( 0 );
	ovrResult res =  ovr_Create(&HMD, &pLuid); 
		if (!OVR_SUCCESS(res))
		{
			MessageBoxA( NULL, "Oculus Rift not detected.", "", MB_OK );
			return;
		}  
		else
		{

			printf("ovr_Create success.\n");
		}
}

bool D3DInit()
{
	SetupOculus( MDFN_IEN_VB::GetSplitMode() == MDFN_IEN_VB::VB3DMODE_OVR );
	HmdDesc = ovr_GetHmdDesc(HMD);
	//bool UseAppWindowFrame = ( HMD->HmdCaps & ovrHmdCap_ExtendDesktop ) ? false : true;
	Render::RendererParams rendererParams;
	rendererParams.Resolution = HmdDesc.Resolution;
	pRender = (Render::D3D11::RenderDevice*)pRender->CreateDevice(HMD, rendererParams, (void*)g_hWnd, pLuid);
	if (!pRender) 
		MessageBoxA(NULL, "Render::D3D11::RenderDevice Init Failed.", "", MB_OK); 

	vbEyeTexture = pRender->CreateTexture(Render::Texture_RGBA | 1, pcejin.width, pcejin.height, NULL);
	if (!vbEyeTexture)
		MessageBoxA(NULL, "vbEyeTexture Init Failed.", "", MB_OK);
	vbShaderFill = (Render::ShaderFill*)pRender->CreateTextureFill(vbEyeTexture);
	if (!vbShaderFill)
		MessageBoxA(NULL, "vbEyeTexture Init Failed.", "", MB_OK);

	Sizei recommenedTex0Size = ovr_GetFovTextureSize( HMD, ovrEye_Left, HmdDesc.DefaultEyeFov[0], 1.0f );
	Sizei recommenedTex1Size = ovr_GetFovTextureSize(HMD, ovrEye_Right, HmdDesc.DefaultEyeFov[1], 1.0f);
	Sizei RenderTargetSize;
	RenderTargetSize.w = recommenedTex0Size.w + recommenedTex1Size.w;
	RenderTargetSize.h = max( recommenedTex0Size.h, recommenedTex1Size.h );

	for (size_t i = 0; i < 2; i++)
	{
		pRenderTargetTexture[i] = pRender->CreateTexture(Render::Texture_RGBA | Render::Texture_RenderTarget | Render::Texture_SwapTextureSet,
			RenderTargetSize.w, RenderTargetSize.h, NULL);
		if (!pRenderTargetTexture[i])
			MessageBoxA(NULL, "pRenderTargetTexture Init Failed.", "", MB_OK);
		if (!pRenderTargetTexture[i]->TextureSet)
			MessageBoxA(NULL, "pRenderTargetTexture TextureSet Init Failed.", "", MB_OK);

	}



		/*ovrResult result = ovr_CreateSwapTextureSetD3D11(HMD, pRender->Device, &dsDesc, ovrSwapTextureSetD3D11_Typeless, &pRenderTargetTexture->TextureSet);
	if (!OVR_SUCCESS(result))
	{
		MessageBoxA(NULL, "ovr_CreateSwapTextureSetD3D11 Init Failed.", "", MB_OK);
	}*/





	/*D3D11_TEXTURE2D_DESC dsDesc;
	dsDesc.Width = pcejin.width;
	dsDesc.Height = pcejin.height;
	dsDesc.MipLevels = 1;
	dsDesc.ArraySize = 1;
	dsDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	dsDesc.SampleDesc.Count = 1;   // No multi-sampling allowed
	dsDesc.SampleDesc.Quality = 0;
	dsDesc.Usage = D3D11_USAGE_DEFAULT;
	dsDesc.CPUAccessFlags = 0;
	dsDesc.MiscFlags = 0;
	dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;*/

	RenderTargetSize.w = pRenderTargetTexture[0]->GetWidth();
	RenderTargetSize.h = pRenderTargetTexture[0]->GetHeight();

	ovrFovPort eyeFov[2] = { HmdDesc.DefaultEyeFov[0], HmdDesc.DefaultEyeFov[1] };
	EyeRenderViewport[0].Pos = Vector2i( 0, 0 );
	EyeRenderViewport[0].Size = Sizei( RenderTargetSize.w / 2, RenderTargetSize.h );
	EyeRenderViewport[1].Pos = Vector2i( ( RenderTargetSize.w + 1 ) / 2, 0 );
	EyeRenderViewport[1].Size = EyeRenderViewport[0].Size;

/*	EyeTexture[0].D3D11.Header.API = ovrRenderAPI_D3D11;
	EyeTexture[0].D3D11.Header.TextureSize = RenderTargetSize;
	//EyeTexture[0].D3D11.Header.RenderViewport = EyeRenderViewport[0];
	EyeTexture[0].D3D11.pTexture = pRenderTargetTexture->Tex.GetPtr(); //((ovrD3D11TextureData*)(pRenderTargetTexture->Get_ovrTexture().PlatformData))->pTexture;
	EyeTexture[0].D3D11.pSRView = pRenderTargetTexture->TexSv[0].GetPtr();

	// Right eye uses the same texture, but different rendering viewport.
	EyeTexture[1] = EyeTexture[0];
	//EyeTexture[1].D3D11.Header.RenderViewport = EyeRenderViewport[1];
	*/

	/*ovrD3D11Config d3d11cfg;
	d3d11cfg.D3D11.Header.API = ovrRenderAPI_D3D11;
	d3d11cfg.D3D11.Header.RTSize = Sizei( HMD->Resolution.w, HMD->Resolution.h );
	d3d11cfg.D3D11.Header.Multisample = 1;
	d3d11cfg.D3D11.pDevice = pRender->Device;
	d3d11cfg.D3D11.pDeviceContext = pRender->Context;
	d3d11cfg.D3D11.pBackBufferRT = pRender->BackBufferRT;
	d3d11cfg.D3D11.pSwapChain = pRender->SwapChain;	 
	ovrHmd_SetEnabledCaps( HMD, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction );*/

	

	/*if ( !ovr_ConfigureRendering( HMD, &d3d11cfg.Config,
		ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette |
		 ovrDistortionCap_TimeWarp | ovrDistortionCap_Overdrive,
		eyeFov, EyeRenderDesc ) )
	{
		MessageBoxA( NULL, "ovrHmd_ConfigureRendering failed", "", MB_OK );
		return false;
	}
	*/
	for (int i = 0; i < ovrEye_Count; ++i)
		EyeRenderDesc[i] = ovr_GetRenderDesc(HMD, (ovrEyeType)i, eyeFov[i]);

	ovr_ConfigureTracking( HMD, ovrTrackingCap_Orientation |
		ovrTrackingCap_MagYawCorrection |
		ovrTrackingCap_Position, 0 );

	QuadVertexBuffer = pRender->CreateBuffer();
	const Render::Vertex QuadVertices[] =
	{ Render::Vertex(Vector3f(-0.5, 0.5, 0), Color(255, 255, 255, 255), 0.0f, 0.0f), Render::Vertex(Vector3f(0.5, 0.5, 0), Color(255, 255, 255, 255), 1.0f, 0.0f),
	Render::Vertex(Vector3f(-0.5, -0.5, 0), Color(255, 255, 255, 255), 0.0f, 1.0f), Render::Vertex(Vector3f(0.5, -0.5, 0), Color(255, 255, 255, 255), 1.0f, 1.0f) };
	QuadVertexBuffer->Data(Render::Buffer_Vertex, QuadVertices, sizeof(QuadVertices));


	/*D3D11_TEXTURE2D_DESC td = {}; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	td.Width = pcejin.width;
	td.Height = pcejin.height;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.SampleDesc.Count = 1;
	td.MipLevels = 1;
	result = ovr_CreateMirrorTextureD3D11(HMD, pRender->Device, &td, 0, &mirrorTexture);
	if (!OVR_SUCCESS(result))
	{
		MessageBoxA(NULL, "Failed to create mirror texture.", "", MB_OK); 
	}*/

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
		//ovrHmd_BeginFrame( HMD, 0 );
		pRender->BeginScene();
		ovr_GetTrackingState( HMD, 0,false );

		// Get both eye poses simultaneously, with IPD offset already included. 
		ovrPosef         EyeRenderPose[2];
		ovrVector3f      HmdToEyeViewOffset[2] = { EyeRenderDesc[0].HmdToEyeViewOffset,
			EyeRenderDesc[1].HmdToEyeViewOffset };
		double frameTime = ovr_GetPredictedDisplayTime(HMD, 0);
		// Keeping sensorSampleTime as close to ovr_GetTrackingState as possible - fed into the layer
		double           sensorSampleTime = ovr_GetTimeInSeconds();
		ovrTrackingState hmdState = ovr_GetTrackingState(HMD, frameTime, ovrTrue);
		ovr_CalcEyePoses(hmdState.HeadPose.ThePose, HmdToEyeViewOffset, EyeRenderPose);


		for ( int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++ )
		{
			ovrEyeType eye = (ovrEyeType)eyeIndex;// HMD->EyeRenderOrder[eyeIndex];
			render( eye ); 
			pRender->SetRenderTarget(pRenderTargetTexture[eyeIndex]);
			pRender->SetViewport(Recti(EyeRenderViewport[eye]));
			pRender->Clear();
			pRender->SetProjection( Matrix4f() );
			pRender->SetDepthMode( false, false );
			float aspectRatio = (float)pcejin.width / pcejin.height / ( (float)EyeRenderViewport[eye].Size.w / EyeRenderViewport[eye].Size.h );
			Matrix4f view = Matrix4f( 
				1, 0, 0, 0,
				0, 1 / aspectRatio, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1 );
			  pRender->Render( vbShaderFill, QuadVertexBuffer, NULL, view, 0, 4, Render::Prim_TriangleStrip);
		}
		pRender->FinishScene();	 
		ovrLayerEyeFov ld = {};
		ld.Header.Type = ovrLayerType_Direct;
		ld.Header.Flags = 0;
		 
		for (int eye = 0; eye < 2; eye++)
		{
			ld.ColorTexture[eye] = pRenderTargetTexture[eye]->TextureSet;
			ld.Viewport[eye] = EyeRenderViewport[eye];
			ld.Fov[eye] = HmdDesc.DefaultEyeFov[eye];
			ld.RenderPose[eye] = eyeRenderPose[eye];
		}
		ovrLayerHeader* layers = &ld.Header;
		ovrViewScaleDesc viewScaleDesc; 
		
		ovrResult result = ovr_SubmitFrame(HMD, 0, nullptr, &layers, 1);
		if (!OVR_SUCCESS(result))
		{
			char c[256] = { 0 };
			sprintf(c,"ovr_SubmitFrame Failed. error code:%d\n",result);
			printf(c);
			//MessageBoxA(NULL, c, "", MB_OK);
			return;
		}

		// Render mirror
		/*ovrD3D11Texture* tex = (ovrD3D11Texture*)mirrorTexture;
		pRender->Context->CopyResource(pRender->BackBuffer, tex->D3D11.pTexture);
		pRender->SwapChain->Present(0, 0);*/

		//ovrHmd_EndFrame( HMD, eyeRenderPose, &EyeTexture[0].Texture );
	}
	else
	{
		render( ovrEye_Right );
		pRender->SetDefaultRenderTarget();
		//pRender->SetFullViewport();
		pRender->Clear();
		pRender->SetProjection( Matrix4f() );
		pRender->SetDepthMode( false, false );
		float aspectRatio = (float)pcejin.width / pcejin.height / ( (float)pRender->D3DViewport.Width / pRender->D3DViewport.Height );
		Matrix4f view = Matrix4f( 
			2, 0, 0, 0,
			0, 2 / aspectRatio, 0, 0,
			0, 0, 2, 0,
			0, 0, 0, 1 );
		//pRender->Render( vbShaderFill, QuadVertexBuffer, NULL, sizeof( Vertex ), view, 0, 4, Prim_TriangleStrip );

		pRender->Render(vbShaderFill, QuadVertexBuffer, NULL, view, 0, 4, Render::Prim_TriangleStrip);
		pRender->Present( true );
	}
}

void DismissHSWDisplay()
{
	//ovrHmd_DismissHSWDisplay( HMD );
}

#endif // #if USE_D3D