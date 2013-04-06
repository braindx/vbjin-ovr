#if USE_D3D

#ifdef _DEBUG
#pragma comment( lib, "oculus/LibOVR/Lib/Win32/libovrd.lib" )
#else
#pragma comment( lib, "oculus/LibOVR/Lib/Win32/libovr.lib" )
#endif

#include "aggdraw.h"
#include "GPU_osd.h"
#include "main.h"
#include "lua-engine.h"
#include "mednafen/src/mednafen.h"
#include "pcejin.h"
#include "vb.h"
#include "oculus/OculusRoomTiny/RenderTiny_D3D1X_Device.h"
#include "oculus/OculusRoomTiny/RenderTiny_Device.h"
#include "oculus/LibOVR/Src/OVR_SensorFusion.h"
#include "oculus/LibOVR/Src/Kernel/OVR_System.h"

using namespace OVR;
using namespace RenderTiny;

RendererParams		RenderParams;
StereoConfig        SConfig;
Ptr<DeviceManager>	pManager = NULL;
Ptr<SensorDevice>   pSensor;
Ptr<HMDDevice>      pHMD;
SensorFusion        SFusion;
OVR::HMDInfo        hmdInfo;
Ptr<RenderDevice>   pRender;
int					Width = 1280, Height = 800;
RenderTiny::Scene   scene;
Matrix4f            View;
Texture*			ScreenTexture;

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
	// *** Oculus HMD & Sensor Initialization

	// Create DeviceManager and first available HMDDevice from it.
	// Sensor object is created from the HMD, to ensure that it is on the
	// correct device.

	OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));
	pManager = *DeviceManager::Create();	
}

void SetupOculus( bool warnIfNotFound )
{
	if ( pManager == NULL )
	{
		return;
	}

	int         detectionResult = IDCONTINUE;
	const char* detectionMessage;

	do 
	{
		// Release Sensor/HMD in case this is a retry.
		pSensor.Clear();
		pHMD.Clear();
		RenderParams.MonitorName.Clear();

		if ( pManager )
		{
			pHMD  = *pManager->EnumerateDevices<HMDDevice>().CreateDevice();
			if (pHMD)
			{
				pSensor = *pHMD->GetSensor();

				// This will initialize HMDInfo with information about configured IPD,
				// screen size and other variables needed for correct projection.
				// We pass HMD DisplayDeviceName into the renderer to select the
				// correct monitor in full-screen mode.
				if (pHMD->GetDeviceInfo(&hmdInfo))
				{            
					RenderParams.MonitorName = hmdInfo.DisplayDeviceName;
					RenderParams.DisplayId = hmdInfo.DisplayId;
					SConfig.SetHMDInfo(hmdInfo);
				}
			}
			else
			{            
				// If we didn't detect an HMD, try to create the sensor directly.
				// This is useful for debugging sensor interaction; it is not needed in
				// a shipping app.
				pSensor = *pManager->EnumerateDevices<SensorDevice>().CreateDevice();
			}
		}


		// If there was a problem detecting the Rift, display appropriate message.
		detectionResult  = IDCANCEL;

		if (!pHMD && !pSensor)
			detectionMessage = "Oculus Rift not detected.";
		else if (!pHMD)
			detectionMessage = "Oculus Sensor detected; HMD Display not detected.";
		else if (!pSensor)
			detectionMessage = "Oculus HMD Display detected; Sensor not detected.";
		else if (hmdInfo.DisplayDeviceName[0] == '\0')
			detectionMessage = "Oculus Sensor detected; HMD display EDID not detected.";
		else
			detectionMessage = 0;

		if (detectionMessage && warnIfNotFound)
		{
			String messageText(detectionMessage);
			messageText += "\n\n"
				"Press 'Try Again' to run retry detection.\n"
				"Press 'Continue' to run full-screen anyway.";

			detectionResult = ::MessageBoxA(0, messageText.ToCStr(), "Oculus Rift Detection",
				MB_RETRYCANCEL|MB_ICONWARNING);
		}

	} while (detectionResult != IDCANCEL);


	if (hmdInfo.HResolution > 0)
	{
		Width  = hmdInfo.HResolution;
		Height = hmdInfo.VResolution;
	}

	SConfig.SetFullViewport(Viewport(0, 0, Width, Height));
	SConfig.SetStereoMode(Stereo_LeftRight_Multipass);

	if (hmdInfo.HScreenSize > 0.0f)
	{
		if (hmdInfo.HScreenSize > 0.140f)  // 7"
			SConfig.SetDistortionFitPointVP(-1.0f, 0.0f);        
		else        
			SConfig.SetDistortionFitPointVP(0.0f, 1.0f);        
	}
}

bool D3DInit()
{
	OculusInit();
	SetupOculus( MDFN_IEN_VB::GetSplitMode() == MDFN_IEN_VB::VB3DMODE_OVR );

	pRender = *RenderTiny::D3D10::RenderDevice::CreateDevice(RenderParams, (void*)g_hWnd);
	if (!pRender)
	{
		return false;
	}	

	pRender->SetSceneRenderScale(SConfig.GetDistortionScale());
	SConfig.Set2DAreaFov(DegreeToRad(105.0f));

	ScreenTexture = pRender->CreateTexture( Texture_RGBA, pcejin.width, pcejin.height, NULL );

	return true;
}

void render( const StereoEyeParams &stereo )
{
	convert32( stereo.Eye == StereoEye_Left ? 1 : 0 );

	CallRegisteredLuaFunctions(LUACALL_AFTEREMULATIONGUI);

	osd->update();
	DrawHUD();
	osd->clear();

	aggDraw.hud->attach(convert_buffer, pcejin.width, pcejin.height, 4*pcejin.width);

	pRender->UpdateTexture( ScreenTexture, convert_buffer );

	PostProcessType postProcess = MDFN_IEN_VB::GetSplitMode() == MDFN_IEN_VB::VB3DMODE_OVR ? PostProcess_Distortion : PostProcess_None;
	pRender->BeginScene(postProcess);

	// Apply Viewport/Projection for the eye.
	pRender->ApplyStereoParams2D(stereo);
	pRender->Clear();
	pRender->SetDepthMode(false, false);

	if ( MDFN_IEN_VB::GetSplitMode() == MDFN_IEN_VB::VB3DMODE_OVR )
	{
		float width = 1.0f;
		float height = pcejin.height * width / pcejin.width;
		pRender->FillTexture( -width / 2.0f, -height / 2.0f, width / 2.0f, height / 2.0f, ScreenTexture );
	}
	else
	{
		pRender->SetProjection( Matrix4f::Ortho2D( 1.0f, 1.0f ) );
		pRender->FillTexture( 0.0f, 0.0f, 1.0f, 1.0f, ScreenTexture );
	}

	pRender->FinishScene();
}

void render()
{
	if( !pcejin.romLoaded || espec.skip )
	{
		return;
	}

	if( ( pcejin.width != espec.DisplayRect.w ) || ( pcejin.height != espec.DisplayRect.h ) )
	{
		pcejin.width = espec.DisplayRect.w;
		pcejin.height = espec.DisplayRect.h;
		ScreenTexture->Release();
		ScreenTexture = pRender->CreateTexture( Texture_RGBA, pcejin.width, pcejin.height, NULL );
		if( !pcejin.maximized )
		{
			ScaleScreen( (float)pcejin.windowSize );
		}
	}

	SetInputDisplayCharacters(pcejin.pads);

	if ( MDFN_IEN_VB::GetSplitMode() == MDFN_IEN_VB::VB3DMODE_OVR )
	{
		SConfig.SetStereoMode(Stereo_LeftRight_Multipass);
	}
	else
	{
		SConfig.SetStereoMode(Stereo_None);
	}

	switch(SConfig.GetStereoMode())
	{
	case Stereo_None:
	render(SConfig.GetEyeRenderParams(StereoEye_Center));
	break;

	case Stereo_LeftRight_Multipass:
	render(SConfig.GetEyeRenderParams(StereoEye_Left));
	render(SConfig.GetEyeRenderParams(StereoEye_Right));
	break;
	}	

	pRender->Present();
}

#endif // #if USE_D3D