#if USE_D3D

#pragma comment( lib, "oculus/LibOVR/Lib/Windows/Win32/Release/VS2013/libovr.lib" )

#include "aggdraw.h"
#include "GPU_osd.h"
#include "main.h"
#include "lua-engine.h"
#include "mednafen/src/mednafen.h"
#include "pcejin.h"
#include "vb.h"

#include "oculus/Win32_DirectXAppUtil.h"
#include "OVR_CAPI_D3D.h"

//------------------------------------------------------------
// ovrSwapTextureSet wrapper class that also maintains the render target views
// needed for D3D11 rendering.
struct OculusTexture
{
	ovrSession               Session;
	ovrTextureSwapChain      TextureChain;
	std::vector<ID3D11RenderTargetView*> TexRtv;

	OculusTexture() :
		Session( nullptr ),
		TextureChain( nullptr )
	{}

	bool Init( ovrSession session, int sizeW, int sizeH )
	{
		Session = session;

		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.Width = sizeW;
		desc.Height = sizeH;
		desc.MipLevels = 1;
		desc.SampleCount = 1;
		desc.MiscFlags = ovrTextureMisc_DX_Typeless;
		desc.BindFlags = ovrTextureBind_DX_RenderTarget;
		desc.StaticImage = ovrFalse;

		ovrResult result = ovr_CreateTextureSwapChainDX( session, DIRECTX.Device, &desc, &TextureChain );
		if ( !OVR_SUCCESS( result ) )
			return false;

		int textureCount = 0;
		ovr_GetTextureSwapChainLength( Session, TextureChain, &textureCount );
		for ( int i = 0; i < textureCount; ++i )
		{
			ID3D11Texture2D* tex = nullptr;
			ovr_GetTextureSwapChainBufferDX( Session, TextureChain, i, IID_PPV_ARGS( &tex ) );
			D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
			rtvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			ID3D11RenderTargetView* rtv;
			DIRECTX.Device->CreateRenderTargetView( tex, &rtvd, &rtv );
			TexRtv.push_back( rtv );
			tex->Release();
		}

		return true;
	}

	~OculusTexture()
	{
		for ( int i = 0; i < (int)TexRtv.size(); ++i )
		{
			Release( TexRtv[i] );
		}
		if ( TextureChain )
		{
			ovr_DestroyTextureSwapChain( Session, TextureChain );
		}
	}

	ID3D11RenderTargetView* GetRTV()
	{
		int index = 0;
		ovr_GetTextureSwapChainCurrentIndex( Session, TextureChain, &index );
		return TexRtv[index];
	}

	// Commit changes
	void Commit()
	{
		ovr_CommitTextureSwapChain( Session, TextureChain );
	}
};

static DepthBuffer* s_eyeDepthBuffers[2];
static ovrRecti s_eyeRenderViewports[2];
static OculusTexture* s_eyeTextures[2];
static long long s_frameIndex = 0;
static ovrHmdDesc s_hmdDesc;
static ovrGraphicsLuid s_luid;
static ovrMirrorTexture s_mirrorTexture = NULL;
static Scene s_immersiveScene;
static Scene s_scene;
static ovrSession s_session;
static Texture* s_vbEyeTexture;

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
	ovrResult result = ovr_Initialize( nullptr );
	assert( OVR_SUCCESS( result ) );
}

static bool SetupOculus()
{
	ovrResult result = ovr_Create( &s_session, &s_luid );

	if ( OVR_FAILURE( result ) )
	{
		MessageBoxA( NULL, "Oculus Rift not detected.", "", MB_OK );
		return false;
	}

	s_hmdDesc = ovr_GetHmdDesc( s_session );
	if ( !DIRECTX.InitDevice( s_hmdDesc.Resolution.w / 2, s_hmdDesc.Resolution.h / 2, reinterpret_cast<LUID*>( &s_luid ) ) )
	{
		MessageBoxA( NULL, "Failed to initialize graphics device.", "", MB_OK );
		return false;
	}

	for ( int eye = 0 ; eye < 2 ; ++eye )
	{
		ovrSizei idealSize = ovr_GetFovTextureSize( s_session, (ovrEyeType)eye, s_hmdDesc.DefaultEyeFov[eye], 2.0f );
		// Choose power-of-two sizes that will fit the ideal size
		ovrSizei textureSize =
		{
			pow( 2, ceil( log( idealSize.w ) / log( 2 ) ) ),
			pow( 2, ceil( log( idealSize.h ) / log( 2 ) ) )
		};
		s_eyeTextures[eye] = new OculusTexture();
		if ( !s_eyeTextures[eye]->Init( s_session, textureSize.w, textureSize.h ) )
		{
			MessageBoxA( NULL, "Failed to create eye texture.", "", MB_OK );
			return false;
		}
		s_eyeDepthBuffers[eye] = new DepthBuffer( DIRECTX.Device, textureSize.w, textureSize.h );
		s_eyeRenderViewports[eye].Pos.x = textureSize.w / 2 - idealSize.w / 2;
		s_eyeRenderViewports[eye].Pos.y = textureSize.h / 2 - idealSize.h / 2;
		s_eyeRenderViewports[eye].Size = idealSize;
		if ( !s_eyeTextures[eye]->TextureChain )
		{
			MessageBoxA( NULL, "Failed to create eye texture.", "", MB_OK );
			return false;
		}
	}

	// Create a mirror to see on the monitor.
	ovrMirrorTextureDesc mirrorDesc = {};
	mirrorDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	mirrorDesc.Width = DIRECTX.WinSizeW;
	mirrorDesc.Height = DIRECTX.WinSizeH;
	result = ovr_CreateMirrorTextureDX( s_session, DIRECTX.Device, &mirrorDesc, &s_mirrorTexture );
	if ( OVR_FAILURE( result ) )
	{
		MessageBoxA( NULL, "Failed to create mirror texture.", "", MB_OK );
		return false;
	}

	s_vbEyeTexture = new Texture( pcejin.width, pcejin.height, false );
	s_immersiveScene.Add( new Model( new Material( s_vbEyeTexture, Material::MAT_TRANS | Material::MAT_POINT_FILTER ), -0.5f, -0.5f, 0.5f, 0.5f ) );
	s_scene.Add( new Model( new Material( s_vbEyeTexture, Material::MAT_TRANS ), -0.5f, -0.5f, 0.5f, 0.5f ) );

	ovr_SetTrackingOriginType( s_session, ovrTrackingOrigin_EyeLevel );
}

bool D3DInit()
{
	DIRECTX.SetWindow( g_hWnd );

	if ( !SetupOculus() )
	{
		return false;
	}

	return true;
}

static void UpdateTexture( ovrEyeType eye )
{
	DIRECTX.Context->UpdateSubresource( s_vbEyeTexture->Tex, 0, NULL, convert_buffer, s_vbEyeTexture->SizeW * 4, s_vbEyeTexture->SizeW * s_vbEyeTexture->SizeH * 4 );
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

	// Call ovr_GetRenderDesc each frame to get the ovrEyeRenderDesc, as the returned values (e.g. HmdToEyeOffset) may change at runtime.
	ovrEyeRenderDesc eyeRenderDescs[2];
	eyeRenderDescs[0] = ovr_GetRenderDesc( s_session, ovrEye_Left, s_hmdDesc.DefaultEyeFov[0] );
	eyeRenderDescs[1] = ovr_GetRenderDesc( s_session, ovrEye_Right, s_hmdDesc.DefaultEyeFov[1] );

	// Get both eye poses simultaneously, with IPD offset already included. 
	ovrPosef eyeRenderPoses[2];
	ovrVector3f hmdToEyeOffsets[2] = { eyeRenderDescs[0].HmdToEyeOffset, eyeRenderDescs[1].HmdToEyeOffset };

	double sensorSampleTime; // sensorSampleTime is fed into the layer later
	ovr_GetEyePoses( s_session, s_frameIndex, ovrTrue, hmdToEyeOffsets, eyeRenderPoses, &sensorSampleTime );

	bool immersiveMode = MDFN_IEN_VB::GetSplitMode() == MDFN_IEN_VB::VB3DMODE_OVR_IMMERSIVE;
	for ( int eye = 0 ; eye < 2 ; ++eye )
	{
		// Clear and set up rendertarget
#ifdef _DEBUG
		DIRECTX.SetAndClearRenderTarget( s_eyeTextures[eye]->GetRTV(), s_eyeDepthBuffers[eye], 0.0f, 1.0f );
#else // #ifdef _DEBUG
		DIRECTX.SetAndClearRenderTarget( s_eyeTextures[eye]->GetRTV(), s_eyeDepthBuffers[eye] );
#endif // #else // #ifdef _DEBUG
		DIRECTX.SetViewport( (float)s_eyeRenderViewports[eye].Pos.x, (float)s_eyeRenderViewports[eye].Pos.y,
								(float)s_eyeRenderViewports[eye].Size.w, (float)s_eyeRenderViewports[eye].Size.h );

		// Copy virtual boy frame from CPU to GPU
		render( eyeRenderDescs[eye].Eye );

		if ( immersiveMode )
		{
			//Get the pose information in XM format
			XMVECTOR eyeQuat = XMVectorSet( eyeRenderPoses[eye].Orientation.x, eyeRenderPoses[eye].Orientation.y,
											eyeRenderPoses[eye].Orientation.z, eyeRenderPoses[eye].Orientation.w );
			XMVECTOR eyePos = XMVectorSet( eyeRenderPoses[eye].Position.x, eyeRenderPoses[eye].Position.y, eyeRenderPoses[eye].Position.z, 0 );
			Camera finalCam( &eyePos, &eyeQuat );
			XMMATRIX view = finalCam.GetViewMatrix();

			ovrMatrix4f p = ovrMatrix4f_Projection( eyeRenderDescs[eye].Fov, 0.01f, 10000.0f, ovrProjection_None );
			XMMATRIX proj = XMMatrixSet( p.M[0][0], p.M[1][0], p.M[2][0], p.M[3][0],
										 p.M[0][1], p.M[1][1], p.M[2][1], p.M[3][1],
										 p.M[0][2], p.M[1][2], p.M[2][2], p.M[3][2],
										 p.M[0][3], p.M[1][3], p.M[2][3], p.M[3][3] );
			XMMATRIX viewProj = XMMatrixMultiply( view, proj );
			float scaleFactor = 0.5f;
			float aspectRatio = (float)pcejin.width / pcejin.height;
			XMMATRIX worldMat = XMMatrixSet( scaleFactor * aspectRatio, 0.0f, 0.0f, 0.0f,
											 0.0f, scaleFactor, 0.0f, 0.0f,
											 0.0f, 0.0f, scaleFactor, 0.0f,
											 0.0f, 0.0f, -0.5f, 1.0f );
			XMMATRIX worldViewProj = XMMatrixMultiply( worldMat, viewProj );
			s_immersiveScene.Render( &worldViewProj, 1.0f, 1.0f, 1.0f, 1.0f, true );
		}
		else
		{
			float scaleFactor = 1.0f;
			float aspectRatio = (float)pcejin.width / pcejin.height / ( (float)s_eyeRenderViewports[eye].Size.w / s_eyeRenderViewports[eye].Size.h );
			XMMATRIX projMat = XMMatrixSet( scaleFactor, 0, 0, 0,
											0, scaleFactor / aspectRatio, 0, 0,
											0, 0, scaleFactor, 0,
											0, 0, 0, 1 );
			float ipdX = eyeRenderDescs[eye].HmdToEyeOffset.x * 5.0f;
			float ipdY = eyeRenderDescs[eye].HmdToEyeOffset.y * 5.0f;
			XMMATRIX projWithIPDMat = XMMatrixMultiply( projMat, XMMatrixTranslation( -ipdX, -ipdY, 0.0f ) );
			s_scene.Render( &projWithIPDMat, 1.0f, 1.0f, 1.0f, 1.0f, true );
		}

		// Commit rendering to the swap chain
		s_eyeTextures[eye]->Commit();
	}

	// Initialize our single full screen Fov layer.
	ovrLayerEyeFov ld = {};
	if ( immersiveMode )
	{
		ld.Header.Type = ovrLayerType_EyeFov;
		ld.Header.Flags = ovrLayerFlag_HighQuality;

		for ( int eye = 0 ; eye < 2 ; ++eye )
		{
			ld.ColorTexture[eye] = s_eyeTextures[eye]->TextureChain;
			ld.Viewport[eye] = s_eyeRenderViewports[eye];
			ld.Fov[eye] = s_hmdDesc.DefaultEyeFov[eye];
			ld.RenderPose[eye] = eyeRenderPoses[eye];
			ld.SensorSampleTime = sensorSampleTime;
		}
	}
	else
	{
		ld.Header.Type = ovrLayerType_EyeFov;
		ld.Header.Flags = ovrLayerFlag_HeadLocked | ovrLayerFlag_HighQuality;

		for ( int eye = 0 ; eye < 2 ; ++eye )
		{
			ld.ColorTexture[eye] = s_eyeTextures[eye]->TextureChain;
			ld.Viewport[eye] = s_eyeRenderViewports[eye];
			ld.Fov[eye] = s_hmdDesc.DefaultEyeFov[eye];
			ld.RenderPose[eye] = ovrPosef { ovrQuatf { 0.0f, 0.0f, 0.0f, 1.0f }, ovrVector3f { 0.0f, 0.0f, 0.0f } };
			ld.SensorSampleTime = sensorSampleTime;
		}
	}

	ovrLayerHeader* layers = &ld.Header;
	ovrResult frameResult = ovr_SubmitFrame( s_session, s_frameIndex, nullptr, &layers, 1 );
	assert( OVR_SUCCESS( frameResult ) );

	ovrSessionStatus sessionStatus;
	ovr_GetSessionStatus( s_session, &sessionStatus );
	if ( sessionStatus.ShouldRecenter )
	{
		ovr_RecenterTrackingOrigin( s_session );
	}

	// Render mirror
	ID3D11Texture2D* tex = NULL;
	ovr_GetMirrorTextureBufferDX( s_session, s_mirrorTexture, IID_PPV_ARGS( &tex ) );
	DIRECTX.Context->CopyResource( DIRECTX.BackBuffer, tex );
	tex->Release();
	DIRECTX.SwapChain->Present( 0, 0 );

	s_frameIndex++;
}

#endif // #if USE_D3D