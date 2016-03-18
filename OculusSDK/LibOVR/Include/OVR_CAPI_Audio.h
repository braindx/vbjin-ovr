/********************************************************************************//**
\file      OVR_CAPI_Audio.h
\brief     CAPI audio functions.
\copyright Copyright 2015 Oculus VR, LLC. All Rights reserved.
************************************************************************************/


#ifndef OVR_CAPI_Audio_h
#define OVR_CAPI_Audio_h

#include "OVR_CAPI.h"
#include <Windows.h>

#define OVR_AUDIO_MAX_DEVICE_STR_SIZE 128

/// Gets the ID of the preferred VR audio device.
///
/// Multiple calls to ovr_CreateSwapTextureSetD3D11 for the same ovrHmd is supported, but applications
/// cannot rely on switching between ovrSwapTextureSets at runtime without a performance penalty.
///
/// \param[out] deviceOutId The ID of the user's preferred VR audio device to use, which will be valid upon a successful return value, else it will be NULL.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutWaveId(UINT* deviceOutId);


/// Gets the GUID of the preferred VR audio device as a string.
///
/// \param[out] deviceOutStrBuffer A buffer where the GUID string for the device will copied to.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutGuidStr(WCHAR deviceOutStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE]);


/// Gets the GUID of the preferred VR audio device.
///
/// \param[out] deviceOutGuid The GUID of the user's preferred VR audio device to use, which will be valid upon a successful return value, else it will be NULL.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutGuid(GUID* deviceOutGuid);

#endif    // OVR_CAPI_Audio_h
