/************************************************************************************

Filename    :   OVR_Win32_HID.cpp
Content     :   Win32 HID interface helpers
Created     :   September 21, 2012
Authors     :   Michael Antonov

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#include "OVR_Win32_HID.h"

namespace OVR {


// HIDDevicePathWrapper is a simple class used to extract HID device file path
// through SetupDiGetDeviceInterfaceDetail. We use a class since this is a bit messy.
class HIDDevicePathWrapper
{
    SP_INTERFACE_DEVICE_DETAIL_DATA_A* pData;
public:
    HIDDevicePathWrapper() : pData(0) { }
    ~HIDDevicePathWrapper() { if (pData) OVR_FREE(pData); }

    const char* GetPath() const { return pData ? pData->DevicePath : 0; }

    bool InitPathFromInterfaceData(HDEVINFO hdevInfoSet, SP_DEVICE_INTERFACE_DATA* pidata);
};

bool HIDDevicePathWrapper::InitPathFromInterfaceData(HDEVINFO hdevInfoSet, SP_DEVICE_INTERFACE_DATA* pidata)
{
    DWORD detailSize = 0;
    // SetupDiGetDeviceInterfaceDetailA returns "not enough buffer error code"
    // doe size request. Just check valid size.
    SetupDiGetDeviceInterfaceDetailA(hdevInfoSet, pidata, NULL, 0, &detailSize, NULL);
    if (!detailSize ||
        ((pData = (SP_INTERFACE_DEVICE_DETAIL_DATA_A*)OVR_ALLOC(detailSize)) == 0))
        return false;
    pData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA_A);

    if (!SetupDiGetDeviceInterfaceDetailA(hdevInfoSet, pidata, pData, detailSize, NULL, NULL))
        return false;
    return true;
}


//-------------------------------------------------------------------------------------
// ***** Win32HIDInterface

Win32HIDInterface::Win32HIDInterface()
{
    hHidLib = ::LoadLibraryA("hid.dll");
    OVR_RESOLVE_HIDFUNC(HidD_GetHidGuid);
    OVR_RESOLVE_HIDFUNC(HidD_SetNumInputBuffers);
    OVR_RESOLVE_HIDFUNC(HidD_GetFeature);
    OVR_RESOLVE_HIDFUNC(HidD_SetFeature);
    OVR_RESOLVE_HIDFUNC(HidD_GetAttributes);
    OVR_RESOLVE_HIDFUNC(HidD_GetManufacturerString);
    OVR_RESOLVE_HIDFUNC(HidD_GetProductString);
    OVR_RESOLVE_HIDFUNC(HidD_GetSerialNumberString);
    OVR_RESOLVE_HIDFUNC(HidD_GetPreparsedData);   
    OVR_RESOLVE_HIDFUNC(HidD_FreePreparsedData);  
    OVR_RESOLVE_HIDFUNC(HidP_GetCaps);    

    if (HidD_GetHidGuid)
        HidD_GetHidGuid(&HidGuid);
}

Win32HIDInterface::~Win32HIDInterface()
{
    ::FreeLibrary(hHidLib);
}


bool Win32HIDInterface::Enumerate(HIDEnumerateVisitor* enumVisitor)
{
    HDEVINFO                 hdevInfoSet;
    SP_DEVICE_INTERFACE_DATA interfaceData;
    interfaceData.cbSize = sizeof(interfaceData);
  
    // Get handle to info data set describing all available HIDs.
    hdevInfoSet = SetupDiGetClassDevsA(&HidGuid, NULL, NULL, DIGCF_INTERFACEDEVICE | DIGCF_PRESENT);
    if (hdevInfoSet == INVALID_HANDLE_VALUE)
        return false;

    for(int deviceIndex = 0;
        SetupDiEnumDeviceInterfaces(hdevInfoSet, NULL, &HidGuid, deviceIndex, &interfaceData);
        deviceIndex++)
    {
        // For each device, we extract its file path and open it to get attributes,
        // such as vendor and product id. If anything goes wrong, we move onto next device.
        HIDDevicePathWrapper pathWrapper;
        if (!pathWrapper.InitPathFromInterfaceData(hdevInfoSet, &interfaceData))
            continue;

        HANDLE hidDev = CreateHIDFile(pathWrapper.GetPath());
        if (hidDev == INVALID_HANDLE_VALUE)
            continue;

        HIDDeviceDesc devDesc;
        devDesc.Path = pathWrapper.GetPath();
        if (InitVendorProductVersion(hidDev, &devDesc) &&
            enumVisitor->MatchVendorProduct(devDesc.VendorId, devDesc.ProductId) &&
            InitUsageAndIOLength(hidDev, &devDesc))
        {
            InitStrings(hidDev, &devDesc);
            enumVisitor->Visit(hidDev, devDesc);
        }
        
        ::CloseHandle(hidDev);
    }

    SetupDiDestroyDeviceInfoList(hdevInfoSet);
    return true;
}


bool Win32HIDInterface::InitVendorProductVersion(HANDLE hidDev, HIDDeviceDesc* desc)
{
    HIDD_ATTRIBUTES attr;
    attr.Size = sizeof(attr);
    if (!HidD_GetAttributes(hidDev, &attr))
        return false;
    desc->VendorId      = attr.VendorID;
    desc->ProductId     = attr.ProductID;
    desc->VersionNumber = attr.VersionNumber;
    return true;
}

bool Win32HIDInterface::InitUsageAndIOLength(HANDLE hidDev, HIDDeviceDesc* desc)
{
    bool                 result = false;
    HIDP_CAPS            caps;
    HIDP_PREPARSED_DATA* preparsedData = 0;

    if (!HidD_GetPreparsedData(hidDev, &preparsedData))
        return false;

    if (HidP_GetCaps(preparsedData, &caps) == HIDP_STATUS_SUCCESS)
    {
        desc->Usage                  = caps.Usage;
        desc->UsagePage              = caps.UsagePage;
        desc->InputReportByteLength  = caps.InputReportByteLength;
        desc->OutputReportByteLength = caps.OutputReportByteLength;
        desc->FeatureReportByteLength= caps.FeatureReportByteLength;
        result = true;
    }
    HidD_FreePreparsedData(preparsedData);
    return result;
}

void Win32HIDInterface::InitStrings(HANDLE hidDev, HIDDeviceDesc* desc)
{
    // Documentation mentions 126 as being the max for USB.
    wchar_t strBuffer[196];
   
    // HidD_Get*String functions return nothing in buffer on failure,
    // so it's ok to do this without further error checking.
    strBuffer[0] = 0;
    HidD_GetManufacturerString(hidDev, strBuffer, sizeof(strBuffer));
    desc->Manufacturer = strBuffer;

    strBuffer[0] = 0;
    HidD_GetProductString(hidDev, strBuffer, sizeof(strBuffer));
    desc->Product = strBuffer;

    strBuffer[0] = 0;
    HidD_GetSerialNumberString(hidDev, strBuffer, sizeof(strBuffer));
    desc->SerialNumber = strBuffer;
}


// Return number of bytes written, 0 for failure.
UPInt Win32HIDInterface::Write(HANDLE hidDev, const UByte* buffer, UPInt size)
{
    DWORD      bytesWritten = 0;
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(OVERLAPPED));

    if (!::WriteFile(hidDev, buffer, (DWORD)size, NULL, &overlapped))
    {
        // ERROR_INVALID_USER_BUFFER ?

        DWORD lastError = GetLastError();
        if (lastError != ERROR_IO_PENDING)
        {
            // Write error. Should we handle this as unplug?
            return 0;
        }
    }

    // Wait if done
    if (!::GetOverlappedResult(hidDev, &overlapped, &bytesWritten, TRUE))
    {
        return 0;
    }
    
    return bytesWritten;
}


} // namespace OVR
