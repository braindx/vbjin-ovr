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
#include "dd.h"

#include "OVR_CAPI.h"
#include "OculusSDK\Render\Render_D3D11_Device.h"
#define OVR_D3D_VERSION 11
#include "OVR_CAPI_D3D.h"


float IDPFactor = 0;
using namespace OVR;
ovrHmd             HMD;
ovrHmdDesc			HmdDesc;
ovrGraphicsLuid  pLuid;
ovrEyeRenderDesc   EyeRenderDesc[2];
bool isOculusExists = false;
//ovrD3D11Texture    EyeTexture[2];

Recti fullViewport;// (0, 0, WindowWidth, WindowHeight);
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
 
void updateIPDFactor(bool isIncrease)
{
	if (isIncrease)
	{
		IDPFactor += 0.02;

		osd->addLine("IPD Increased to  %.3f \n", IDPFactor);
		printf("IPD Increase to  %.3f \n", IDPFactor);
	}
	else
	{
		IDPFactor -= 0.02;
		osd->addLine("IPD Decreased to  %.3f \n", IDPFactor);
		printf("IPD Decrease to  %.3f \n", IDPFactor);
	} 
	char c[8];
	sprintf(c, "%.3f", IDPFactor);
	WritePrivateProfileString("Display", "IPDFactor", c, IniName);
}
void OculusInit()
{
	ovrResult error = ovr_Initialize(NULL);
	if (!OVR_SUCCESS(error))
	{ 
		char c[256] = { 0 };
		sprintf(c, "ovr_Initialize Failed. error code:%d", error); 
		return;
	}
	else
	{
		printf("ovr_Initialize success.\n");

		char retval[256];
		 GetPrivateProfileString("Display", "IPDFactor", "-0.230",retval,256, IniName);
		 IDPFactor = atof(retval);

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
			isOculusExists = true;
			printf("ovr_Create success.\n");
		}
}

bool D3DInit()
{
	SetupOculus( MDFN_IEN_VB::GetSplitMode() == MDFN_IEN_VB::VB3DMODE_OVR );
	if (isOculusExists == false) return false;
	HmdDesc = ovr_GetHmdDesc(HMD); 
	Render::RendererParams rendererParams;
	rendererParams.Resolution = HmdDesc.Resolution;
	Recti  fvp(0, 0, rendererParams.Resolution.w, rendererParams.Resolution.h);
	fullViewport = fvp;

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
	 
	RenderTargetSize.w = pRenderTargetTexture[0]->GetWidth(); 
	RenderTargetSize.h = pRenderTargetTexture[0]->GetHeight();

	ovrFovPort eyeFov[2] = { HmdDesc.DefaultEyeFov[0], HmdDesc.DefaultEyeFov[1] };
	EyeRenderViewport[0].Pos = Vector2i( 0, 0 );
	EyeRenderViewport[0].Size = Sizei( RenderTargetSize.w / 2, RenderTargetSize.h );
	EyeRenderViewport[1].Pos = Vector2i( ( RenderTargetSize.w + 1 ) / 2, 0 );
	EyeRenderViewport[1].Size = EyeRenderViewport[0].Size;
	 
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
unsigned long long idx = 0;
void render()
{
	if (isOculusExists == false)
	{
		renderDD();
		return;
	}
	ovrPosef eyeRenderPose[2];
	/*eyeRenderPose[0].Position.x = offset;
	eyeRenderPose[1].Position.x = -offset;

	double ftiming = ovr_GetPredictedDisplayTime(HMD, 0);
	ovrTrackingState trackingState = ovr_GetTrackingState(HMD, ftiming, ovrTrue);
	ovrVector3f useHmdToEyeViewOffset[2] = { EyeRenderDesc[0].HmdToEyeViewOffset,
		EyeRenderDesc[1].HmdToEyeViewOffset };
	ovr_CalcEyePoses(trackingState.HeadPose.ThePose, useHmdToEyeViewOffset, eyeRenderPose);
	*/
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
		pRender->BeginScene(); 

		ovrLayerEyeFov ld = {};
		ld.Header.Type = ovrLayerType_Direct;
		ld.Header.Flags = 0;

		for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
		{
			ovrEyeType eye = (ovrEyeType)eyeIndex;// HMD->EyeRenderOrder[eyeIndex];
			render( eye ); 
			pRender->SetRenderTarget(pRenderTargetTexture[eyeIndex]);
			pRender->SetViewport(Recti(EyeRenderViewport[eye]));
			pRender->Clear();
			pRender->SetProjection( Matrix4f() );
			pRender->SetDepthMode( false, false );
			float scaleFactor = 2;
			float aspectRatio = (float)pcejin.width / pcejin.height / ( (float)EyeRenderViewport[eye].Size.w / EyeRenderViewport[eye].Size.h ) ;
			Matrix4f view = Matrix4f( 
				1 * aspectRatio, 0, 0, 0,
				0, 1 / aspectRatio* aspectRatio, 0, 0,
				0, 0, 1 * aspectRatio, 0,
				0, 0, 0, 1 * aspectRatio);  
			if (eyeIndex == 1)
				view.SetTranslation(Vector3f(IDPFactor, 0, 0));
			else
				view.SetTranslation(Vector3f(-IDPFactor, 0, 0));
			//view = view.Scaling(0.8)*view;
			  pRender->Render( vbShaderFill, QuadVertexBuffer, NULL, view, 0, 4, Render::Prim_TriangleStrip);

			  ld.ColorTexture[eyeIndex] = pRenderTargetTexture[eyeIndex]->TextureSet;
			  ld.Viewport[eyeIndex] = EyeRenderViewport[eyeIndex];
			  ld.Fov[eyeIndex] = HmdDesc.DefaultEyeFov[eyeIndex];
			  ld.RenderPose[eyeIndex] = eyeRenderPose[eyeIndex];
		}
		pRender->FinishScene();	  
		ovrLayerHeader* layers = &ld.Header;
		ovrResult result = ovr_SubmitFrame(HMD, idx, nullptr, &layers, 1);
		idx++;
		if (!OVR_SUCCESS(result))
		{
			char c[256] = { 0 };
			sprintf(c,"ovr_SubmitFrame Failed. error code:%d\n",result);
			printf(c); 
			return;
		}
		 
	} 
	else
	{ 
			render( ovrEye_Right );
		pRender->SetDefaultRenderTarget(); 

		RECT rc;
		GetClientRect(g_hWnd, &rc);
		int WindowWidth = rc.right - rc.left;
		int WindowHeight = rc.bottom - rc.top; 
		pRender->SetViewport(fullViewport);

		pRender->Clear();
		pRender->SetProjection(Matrix4f()); 
		pRender->SetDepthMode( false, false );
		float aspectRatio =  (float)pcejin.width / pcejin.height / ((float)pRender->D3DViewport.Width / pRender->D3DViewport.Height);
		Matrix4f view = Matrix4f( 
			2, 0, 0, 0,
			0, 2 / aspectRatio, 0, 0,
			0, 0, 2, 0,
			0, 0, 0, 1 ); 
		pRender->Render(vbShaderFill, QuadVertexBuffer, NULL, view, 0, 4, Render::Prim_TriangleStrip);
		pRender->Present( true );
	}
}

void DismissHSWDisplay()
{
	//ovrHmd_DismissHSWDisplay( HMD );
}

#endif // #if USE_D3D