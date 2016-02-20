/************************************************************************************
Filename    :   Win32_BasicVR.h
Content     :   Core components for achieving basic VR, shared amongst samples
Created     :   October 20th, 2014
Author      :   Tom Heath
Copyright   :   Copyright 2014 Oculus, Inc. All Rights reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/
#ifndef OVR_Win32_BasicVR_h
#define OVR_Win32_BasicVR_h

// Include the OculusVR SDK
#include "OVR_CAPI_D3D.h"                  

#include <cstdlib>

//------------------------------------------------------------
struct OculusTexture
{
    ovrHmd                   hmd;
	ovrSwapTextureSet      * TextureSet;
    static const int         TextureCount = 2;
	ID3D11RenderTargetView * TexRtv[TextureCount];
    int                      SizeW, SizeH;

    OculusTexture() :
        hmd(nullptr),
        TextureSet(nullptr),
        SizeW(0),
        SizeH(0)
    {
        TexRtv[0] = TexRtv[1] = nullptr;
    }

	bool Init(ovrHmd _hmd, int sizeW, int sizeH, ID3D11Device *device = DIRECTX.Device, ovrSwapTextureSet  *tex = nullptr)
	{
        hmd = _hmd;
        SizeW = sizeW;
        SizeH = sizeH;

		D3D11_TEXTURE2D_DESC dsDesc;
		dsDesc.Width = sizeW;
		dsDesc.Height = sizeH;
		dsDesc.MipLevels = 1;
		dsDesc.ArraySize = 1;
        dsDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		dsDesc.SampleDesc.Count = 1;   // No multi-sampling allowed
		dsDesc.SampleDesc.Quality = 0;
		dsDesc.Usage = D3D11_USAGE_DEFAULT;
		dsDesc.CPUAccessFlags = 0;
		dsDesc.MiscFlags = 0;
		dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		if (!tex)tex = TextureSet;
		ovrResult result = ovr_CreateSwapTextureSetD3D11(hmd, DIRECTX.Device, &dsDesc, ovrSwapTextureSetD3D11_Typeless, &tex);
        if (!OVR_SUCCESS(result))
            return false;

        VALIDATE(TextureSet->TextureCount == TextureCount, "TextureCount mismatch.");

		for (int i = 0; i < TextureCount; ++i)
		{
			ovrD3D11Texture* tex = (ovrD3D11Texture*)&TextureSet->Textures[i];
			D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
			rtvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			DIRECTX.Device->CreateRenderTargetView(tex->D3D11.pTexture, &rtvd, &TexRtv[i]);
		}

        return true;
    }

	~OculusTexture()
	{
		for (int i = 0; i < TextureCount; ++i)
        {
            Release(TexRtv[i]);
        }
		if (TextureSet)
        {
            ovr_DestroySwapTextureSet(hmd, TextureSet);
            TextureSet = nullptr;
        }
	}

	void Increment()
	{
		TextureSet->CurrentIndex = (TextureSet->CurrentIndex + 1) % TextureSet->TextureCount;
	}
};

//------------------------------------------------------------------------------------
//Helper functions to convert from Oculus types to XM types - consider to add to SDK
static XMVECTOR ConvertToXM(ovrQuatf q)    { return(XMVectorSet(q.x, q.y, q.z, q.w)); }
static XMFLOAT4 ConvertToXMF(ovrQuatf q)    { return(XMFLOAT4(q.x, q.y, q.z, q.w)); }
static XMVECTOR ConvertToXM(ovrVector3f v) { return(XMVectorSet(v.x, v.y, v.z, 0)); }
static XMFLOAT3 ConvertToXMF(ovrVector3f v) { return(XMFLOAT3(v.x, v.y, v.z)); }
static XMMATRIX ConvertToXM(ovrMatrix4f p) {
    return(XMMatrixSet(p.M[0][0], p.M[1][0], p.M[2][0], p.M[3][0],
        p.M[0][1], p.M[1][1], p.M[2][1], p.M[3][1],
        p.M[0][2], p.M[1][2], p.M[2][2], p.M[3][2],
        p.M[0][3], p.M[1][3], p.M[2][3], p.M[3][3]));
}
//----------------------------------------------------------------------
struct VRLayer
{
    ovrHmd                      HMD;
    ovrEyeRenderDesc            EyeRenderDesc[2];        // Description of the VR.
    ovrRecti                    EyeRenderViewport[2];    // Useful to remember when varying resolution
    OculusTexture             * pEyeRenderTexture[2];    // Where the eye buffers will be rendered
    DepthBuffer               * pEyeDepthBuffer[2];      // For the eye buffers to use when rendered
    ovrPosef                    EyeRenderPose[2];
    ovrLayerEyeFov              ovrLayer;

    //---------------------------------------------------------------
    VRLayer(ovrHmd argHMD, const ovrFovPort * fov = 0, float pixelsPerDisplayPixel = 1.0f)
    {
        HMD = argHMD;
        MakeEyeBuffers(pixelsPerDisplayPixel);
        ConfigureRendering(fov);
    }
    ~VRLayer()
    {
        for (int eye = 0; eye < 2; ++eye)
        {
            delete pEyeRenderTexture[eye];
            pEyeRenderTexture[eye] = nullptr;
            delete pEyeDepthBuffer[eye];
            pEyeDepthBuffer[eye] = nullptr;
        }
    }

    //-----------------------------------------------------------------------
    void MakeEyeBuffers(float pixelsPerDisplayPixel = 1.0f)
    {
        for (int eye = 0; eye < 2; ++eye)
        {
            ovrSizei idealSize = ovr_GetFovTextureSize(HMD, (ovrEyeType)eye, ovr_GetHmdDesc(HMD).DefaultEyeFov[eye], pixelsPerDisplayPixel);
            pEyeRenderTexture[eye] = new OculusTexture();
            if (!pEyeRenderTexture[eye]->Init(HMD, idealSize.w, idealSize.h))
                return;
            pEyeDepthBuffer[eye] = new DepthBuffer(DIRECTX.Device, idealSize.w, idealSize.h);
            EyeRenderViewport[eye].Pos.x = 0;
            EyeRenderViewport[eye].Pos.y = 0;
            EyeRenderViewport[eye].Size = idealSize;
        }
    }

    //--------------------------------------------------------
    void ConfigureRendering(const ovrFovPort * fov = 0)
    {
        // If any values are passed as NULL, then we use the default basic case
        if (!fov) fov = ovr_GetHmdDesc(HMD).DefaultEyeFov;
        EyeRenderDesc[0] = ovr_GetRenderDesc(HMD, ovrEye_Left, fov[0]);
        EyeRenderDesc[1] = ovr_GetRenderDesc(HMD, ovrEye_Right, fov[1]);
    }

    //------------------------------------------------------------
    ovrTrackingState GetEyePoses(ovrPosef * useEyeRenderPose = 0, float * scaleIPD = 0, float * newIPD = 0)
    {
        // Get both eye poses simultaneously, with IPD offset already included. 
        ovrVector3f useHmdToEyeViewOffset[2] = { EyeRenderDesc[0].HmdToEyeViewOffset,
            EyeRenderDesc[1].HmdToEyeViewOffset };

        // If any values are passed as NULL, then we use the default basic case
        if (!useEyeRenderPose) useEyeRenderPose = EyeRenderPose;
        if (scaleIPD)
        {
            useHmdToEyeViewOffset[0].x *= *scaleIPD;
            useHmdToEyeViewOffset[1].x *= *scaleIPD;
        }
        if (newIPD)
        {
            useHmdToEyeViewOffset[0].x = -(*newIPD * 0.5f);
            useHmdToEyeViewOffset[1].x = +(*newIPD * 0.5f);
        }

        double ftiming = ovr_GetPredictedDisplayTime(HMD, 0);
        ovrTrackingState trackingState = ovr_GetTrackingState(HMD, ftiming, ovrTrue);

        ovr_CalcEyePoses(trackingState.HeadPose.ThePose, useHmdToEyeViewOffset, useEyeRenderPose);

        return(trackingState);
    }

    //-----------------------------------------------------------
    XMMATRIX RenderSceneToEyeBuffer(Camera * player, Scene * sceneToRender, int eye, ID3D11RenderTargetView * rtv = 0,
        ovrPosef * eyeRenderPose = 0, int timesToRenderRoom = 1,
        float alpha = 1, float red = 1, float green = 1, float blue = 1, float nearZ = 0.2f, float farZ = 1000.0f,
        bool doWeSetupRender = true, DepthBuffer * depthBuffer = 0,
        float backRed = 0.0f, float backGre = 0.0f, float backBlu = 0.0f)
    {
        // If any values are passed as NULL, then we use the default basic case
        if (!depthBuffer)    depthBuffer = pEyeDepthBuffer[eye];
        if (!eyeRenderPose)  eyeRenderPose = &EyeRenderPose[eye];

        if (doWeSetupRender)
        {
            // If none specified, then using special, and default, Oculus eye buffer render target
            if (rtv)
                DIRECTX.SetAndClearRenderTarget(rtv, depthBuffer, backRed, backGre, backBlu);
            else
            {
                // We increment which texture we are using, to the next one, just before writing
                pEyeRenderTexture[eye]->Increment();
                int texIndex = pEyeRenderTexture[eye]->TextureSet->CurrentIndex;
                DIRECTX.SetAndClearRenderTarget(pEyeRenderTexture[eye]->TexRtv[texIndex], depthBuffer, backRed, backGre, backBlu);
            }

            DIRECTX.SetViewport((float)EyeRenderViewport[eye].Pos.x, (float)EyeRenderViewport[eye].Pos.y,
                (float)EyeRenderViewport[eye].Size.w, (float)EyeRenderViewport[eye].Size.h);
        }

        // Get view and projection matrices for the Rift camera
        XMVECTOR CombinedPos = XMVectorAdd(player->Pos, XMVector3Rotate(ConvertToXM(eyeRenderPose->Position), player->Rot));
        Camera finalCam(&CombinedPos, &(XMQuaternionMultiply(ConvertToXM(eyeRenderPose->Orientation), player->Rot)));
        XMMATRIX view = finalCam.GetViewMatrix();
        ovrMatrix4f p = ovrMatrix4f_Projection(EyeRenderDesc[eye].Fov, nearZ, farZ, ovrProjection_RightHanded);
        XMMATRIX prod = XMMatrixMultiply(view, ConvertToXM(p));

        // Render the scene
        for (int n = 0; n < timesToRenderRoom; n++)
            sceneToRender->Render(&prod, red, green, blue, alpha, true);

        return(prod);
    }

    //------------------------------------------------------------
    void PrepareLayerHeader(OculusTexture * leftEyeTexture = 0, ovrPosef * leftEyePose = 0, XMVECTOR * extraQuat = 0)
    {
        // Use defaults where none specified
        OculusTexture *   useEyeTexture[2] = { pEyeRenderTexture[0], pEyeRenderTexture[1] };
        ovrPosef    useEyeRenderPose[2] = { EyeRenderPose[0], EyeRenderPose[1] };
        if (leftEyeTexture) useEyeTexture[0] = leftEyeTexture;
        if (leftEyePose)    useEyeRenderPose[0] = *leftEyePose;

        // If we need to fold in extra rotations to the timewarp, per eye
        // We make the changes to the temporary copy, rather than 
        // the global one.
        if (extraQuat)
        {
            for (int i = 0; i < 2; i++)
            {
                XMVECTOR localPoseQuat = XMVectorSet(useEyeRenderPose[i].Orientation.x, useEyeRenderPose[i].Orientation.y,
                    useEyeRenderPose[i].Orientation.z, useEyeRenderPose[i].Orientation.w);
                XMVECTOR temp = XMQuaternionMultiply(localPoseQuat, XMQuaternionInverse(extraQuat[i]));
                useEyeRenderPose[i].Orientation.w = XMVectorGetW(temp);
                useEyeRenderPose[i].Orientation.x = XMVectorGetX(temp);
                useEyeRenderPose[i].Orientation.y = XMVectorGetY(temp);
                useEyeRenderPose[i].Orientation.z = XMVectorGetZ(temp);
            }
        }

        ovrLayer.Header.Type = ovrLayerType_EyeFov;
        ovrLayer.Header.Flags = 0;
        ovrLayer.ColorTexture[0] = useEyeTexture[0]->TextureSet;
        ovrLayer.ColorTexture[1] = useEyeTexture[1]->TextureSet;
        ovrLayer.RenderPose[0] = useEyeRenderPose[0];
        ovrLayer.RenderPose[1] = useEyeRenderPose[1];
        ovrLayer.Fov[0] = EyeRenderDesc[0].Fov;
        ovrLayer.Fov[1] = EyeRenderDesc[1].Fov;
        ovrLayer.Viewport[0] = EyeRenderViewport[0];
        ovrLayer.Viewport[1] = EyeRenderViewport[1];
    }
};

//----------------------------------------------------------------------------------------
struct BasicVR
{
    ovrHmd       HMD;
    ovrHmdDesc   HmdDesc;
    ovrResult    presentResult;
    bool         initialized;
    bool         restart;
    static const int MAX_LAYERS = 32;
    VRLayer    * Layer[MAX_LAYERS];
    Camera     * MainCam;
    Scene      * RoomScene;
    ovrTexture * mirrorTexture;
    HINSTANCE    hinst;
    float        scaleWindowW;
    float        scaleWindowH;
    float        scaleMirrorW;
    float        scaleMirrorH;
    bool         windowed;

    //------------------------------------------------------
    BasicVR(HINSTANCE hinst, LPCWSTR title = L"BasicVR") :
        HMD(nullptr),
        presentResult(ovrSuccess),
        initialized(false),
        restart(false),
        MainCam(nullptr),
        RoomScene(nullptr),
        mirrorTexture(nullptr),
        hinst(hinst),
        scaleWindowW(0.5f),
        scaleWindowH(0.5f),
        scaleMirrorW(1.0f),
        scaleMirrorH(1.0f),
        windowed(true)
    {
        // Set all layers to zero
        for (int i = 0; i < MAX_LAYERS; ++i)
            Layer[i] = nullptr;

        // Initializes LibOVR, and the Rift
        ovrResult result = ovr_Initialize(nullptr);
        VALIDATE(OVR_SUCCESS(result), "Failed to initialize libOVR.");

        VALIDATE(DIRECTX.InitWindow(hinst, title), "Failed to open window.");
    }

    ~BasicVR()
    {
        Release();
	    ovr_Shutdown();
        DIRECTX.CloseWindow();
    }

    bool Init(bool retryCreate)
    {
        ovrGraphicsLuid luid;
        ovrResult result = ovr_Create(&HMD, &luid);
        if (!OVR_SUCCESS(result))
            return retryCreate;

        HmdDesc = ovr_GetHmdDesc(HMD);

        // Setup Device and Graphics
        if (!DIRECTX.InitDevice(int(scaleWindowW * HmdDesc.Resolution.w), int(scaleWindowH * HmdDesc.Resolution.h), reinterpret_cast<LUID*>(&luid), windowed))
            return retryCreate;

        // Create a mirror, to see Rift output on a monitor
        mirrorTexture = nullptr;
        D3D11_TEXTURE2D_DESC td = {};
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        td.Width = int(scaleMirrorW * DIRECTX.WinSizeW);
        td.Height = int(scaleMirrorH * DIRECTX.WinSizeH);
        td.Usage = D3D11_USAGE_DEFAULT;
        td.SampleDesc.Count = 1;
        td.MipLevels = 1;
        result = ovr_CreateMirrorTextureD3D11(HMD, DIRECTX.Device, &td, 0, &mirrorTexture);
        if (!OVR_SUCCESS(result))
        {
            if (retryCreate) return true;
            VALIDATE(false, "Failed to create mirror texture.");
        }

        // Create the room model in-place
        RoomScene = new Scene(false);

        // Create camera in-place
        __pragma(warning(push))
        __pragma(warning(disable:4316)) // Win32: object allocated on the heap may not be aligned 16
        MainCam = new Camera(&XMVectorSet(0.0f, 1.6f, +5.0f, 0), &XMQuaternionIdentity());
        __pragma(warning(pop))

        initialized = true;
        return initialized;
    }

    bool HandleMessages()
    {
        // continue MainLoop while SubmitFrame is successful,
        // the use hasn't quit and we're not restarting...
        return OVR_SUCCESS(presentResult) && DIRECTX.HandleMessages() && !restart;
    }

    // Implement this in subclasses
    virtual void MainLoop() = 0;

    int Run()
    {
        // false => exit on any failure
        VALIDATE(Init(false), "Oculus Rift not detected.");

        while (DIRECTX.HandleMessages())
        {
            if (initialized)
            {
                presentResult = ovrSuccess;
                MainLoop();
                Release();
            }

            if (!restart)
            {
                if (presentResult != ovrError_DisplayLost)
                    break;
                Sleep(10);
            }
            else restart = false;

            // true => retry on ovrError_DisplayLost
            if (!Init(true))
                break;
        }

        OutputDebugStringA("Run: Done\n");
        return Release();
    }

    // Called to reinitialize under the app's control
    void Restart()
    {
        restart = true;
    }

    //-------------------------------------------------------
    void ActionFromInput(float speed = 1.0f, bool updateYaw = true, bool verticalControl = false)
    {
        // Keyboard inputs to adjust player orientation, unaffected by speed
        if (updateYaw)
        {
            static float Yaw = 0;
            if (DIRECTX.Key[VK_LEFT])  MainCam->Rot = XMQuaternionRotationRollPitchYaw(0, Yaw += 0.02f, 0);
            if (DIRECTX.Key[VK_RIGHT]) MainCam->Rot = XMQuaternionRotationRollPitchYaw(0, Yaw -= 0.02f, 0);
        }
        // Keyboard inputs to adjust player position
        XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, -0.05f*speed, 0), MainCam->Rot);
        XMVECTOR right = XMVector3Rotate(XMVectorSet(0.05f*speed, 0, 0, 0), MainCam->Rot);
        if (DIRECTX.Key['W'] || DIRECTX.Key[VK_UP])	  MainCam->Pos = XMVectorAdd(MainCam->Pos, forward);
        if (DIRECTX.Key['S'] || DIRECTX.Key[VK_DOWN]) MainCam->Pos = XMVectorSubtract(MainCam->Pos, forward);
        if (DIRECTX.Key['D'])                         MainCam->Pos = XMVectorAdd(MainCam->Pos, right);
        if (DIRECTX.Key['A'])                         MainCam->Pos = XMVectorSubtract(MainCam->Pos, right);
        if (verticalControl)
        {
            if (DIRECTX.Key[VK_CONTROL])              MainCam->Pos = XMVectorAdd(MainCam->Pos, XMVectorSet(0, +0.05f*speed, 0, 0));
            if (DIRECTX.Key[VK_SHIFT])                MainCam->Pos = XMVectorAdd(MainCam->Pos, XMVectorSet(0, -0.05f*speed, 0, 0));
        }
        else
        {
            // Set camera height as fixed at this point - might be scaled later
            // It no longer seems to find the preset user height.
            MainCam->Pos = XMVectorSet(XMVectorGetX(MainCam->Pos), ovr_GetFloat(HMD, OVR_KEY_EYE_HEIGHT, 0), XMVectorGetZ(MainCam->Pos), 0);
        }

        // Animate the cube
        static float cubeClock = 0;
        RoomScene->Models[0]->Pos = XMFLOAT3(9 * sin(cubeClock), 3, 9 * cos(cubeClock += 0.015f));
    }

    //------------------------------------------------------------
    void DistortAndPresent(int numLayersToRender, D3D11_BOX * optionalBoxForMirrorWindow = 0, ovrTexture * texForMirror = NULL, bool mirror = true)
    {
        ovrLayerHeader* layerHeaders[MAX_LAYERS];
        for (int i = 0; i < MAX_LAYERS; i++)
        {
            if (Layer[i]) layerHeaders[i] = &Layer[i]->ovrLayer.Header;
        }

        presentResult = ovr_SubmitFrame(HMD, 0, nullptr, layerHeaders, numLayersToRender);
        if (!OVR_SUCCESS(presentResult))
            return;

        if (mirror)
        {
            // Render mirror
            ovrD3D11Texture* tex;
            if (texForMirror == 0) tex = (ovrD3D11Texture*)mirrorTexture;
            else tex = (ovrD3D11Texture*)texForMirror;

            DIRECTX.Context->CopySubresourceRegion(DIRECTX.BackBuffer, 0, 0, 0, 0, tex->D3D11.pTexture, 0, optionalBoxForMirrorWindow);
            DIRECTX.SwapChain->Present(0, 0);
        }
    }

    //------------------------------------------------------------
    int Release()
    {
        initialized = false;

        if (mirrorTexture)
        {
            ovr_DestroyMirrorTexture(HMD, mirrorTexture);
            mirrorTexture = nullptr;
        }

        for (int i = 0; i < MAX_LAYERS; ++i)
        {
            delete Layer[i];
            Layer[i] = nullptr;
        }

        RoomScene->Release();

        DIRECTX.ReleaseDevice();

        if (HMD)
        {
            ovr_Destroy(HMD);
            HMD = nullptr;
        }

        if (DIRECTX.Key['Q'] && DIRECTX.Key[VK_CONTROL]) return(99);  // Special return code for quitting sample 99 
        return(0);
    }
};

#endif // OVR_Win32_BasicVR_h
