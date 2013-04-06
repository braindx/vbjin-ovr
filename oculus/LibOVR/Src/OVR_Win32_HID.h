/************************************************************************************

Filename    :   OVR_Win32_HID.h
Content     :   Win32 HID interface helpers
Created     :   September 21, 2012
Authors     :   Michael Antonov

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#ifndef OVR_Win32_HID_h
#define OVR_Win32_HID_h

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_String.h"

#include <windows.h>
#include <setupapi.h>

//-------------------------------------------------------------------------------------
// Define needed "hidsdi.h" functionality to avoid requiring DDK installation.
// #include "hidsdi.h"

#ifndef _HIDSDI_H
#include <pshpack4.h>

#define HIDP_STATUS_SUCCESS (0x11 << 16)
struct HIDP_PREPARSED_DATA;

struct HIDD_ATTRIBUTES
{
    ULONG   Size; // = sizeof (struct _HIDD_ATTRIBUTES)
    USHORT  VendorID;
    USHORT  ProductID;
    USHORT  VersionNumber;
};

struct HIDP_CAPS
{
    USHORT   Usage;
    USHORT   UsagePage;
    USHORT   InputReportByteLength;
    USHORT   OutputReportByteLength;
    USHORT   FeatureReportByteLength;
    USHORT   Reserved[17];

    USHORT   NumberLinkCollectionNodes;
    USHORT   NumberInputButtonCaps;
    USHORT   NumberInputValueCaps;
    USHORT   NumberInputDataIndices;
    USHORT   NumberOutputButtonCaps;
    USHORT   NumberOutputValueCaps;
    USHORT   NumberOutputDataIndices;
    USHORT   NumberFeatureButtonCaps;
    USHORT   NumberFeatureValueCaps;
    USHORT   NumberFeatureDataIndices;
};

#include <poppack.h>
#endif


//-------------------------------------------------------------------------------------

namespace OVR {


// HIDDeviceDesc contains interesting attributes of a HID device, including a Path
// that can be used to create it.
struct HIDDeviceDesc
{
    UInt16  VendorId;
    UInt16  ProductId;
    UInt16  VersionNumber;
    UInt16  Usage;
    UInt16  UsagePage;
    UInt16  InputReportByteLength;
    UInt16  OutputReportByteLength;
    UInt16  FeatureReportByteLength;
    String  Path;
    String  Manufacturer;
    String  Product;
    String  SerialNumber;
};

// HIDEnumerateVisitor exposes a Visit interface called for every detected device
// by Win32HIDInterface::Enumerate. 
class HIDEnumerateVisitor
{
public:

    // Should return true if we are interested in supporting
    // this HID VendorId and ProductId pair.
    virtual bool MatchVendorProduct(UInt16 vendorId, UInt16 productId)
    { OVR_UNUSED2(vendorId, productId); return true; }

    // Override to get notified about available device. Will only be called for
    // devices that matched MatchVendorProduct.
    virtual void Visit(HANDLE hidDev, const HIDDeviceDesc&) { OVR_UNUSED(hidDev); }
};


// Win32HIDInterface is a wrapper around Win32 HID API that simplifies its use.
class Win32HIDInterface
{
public:
    HMODULE hHidLib;
    GUID    HidGuid;

    // Macros to declare and resolve needed functions from library.
#define OVR_DECLARE_HIDFUNC(func, rettype, args)   \
    typedef rettype (__stdcall *PFn_##func) args;  \
    PFn_##func      func;
#define OVR_RESOLVE_HIDFUNC(func) \
    func = (PFn_##func)::GetProcAddress(hHidLib, #func)

    OVR_DECLARE_HIDFUNC(HidD_GetHidGuid,            void,    (GUID *hidGuid));
    OVR_DECLARE_HIDFUNC(HidD_SetNumInputBuffers,    BOOLEAN, (HANDLE hidDev, ULONG numberBuffers));
    OVR_DECLARE_HIDFUNC(HidD_GetFeature,            BOOLEAN, (HANDLE hidDev, PVOID buffer, ULONG bufferLength));
    OVR_DECLARE_HIDFUNC(HidD_SetFeature,            BOOLEAN, (HANDLE hidDev, PVOID buffer, ULONG bufferLength));
    OVR_DECLARE_HIDFUNC(HidD_GetAttributes,         BOOLEAN, (HANDLE hidDev, HIDD_ATTRIBUTES *attributes));
    OVR_DECLARE_HIDFUNC(HidD_GetManufacturerString, BOOLEAN, (HANDLE hidDev, PVOID buffer, ULONG bufferLength));
    OVR_DECLARE_HIDFUNC(HidD_GetProductString,      BOOLEAN, (HANDLE hidDev, PVOID buffer, ULONG bufferLength));
    OVR_DECLARE_HIDFUNC(HidD_GetSerialNumberString, BOOLEAN, (HANDLE hidDev, PVOID buffer, ULONG bufferLength));
    OVR_DECLARE_HIDFUNC(HidD_GetPreparsedData,      BOOLEAN, (HANDLE hidDev, HIDP_PREPARSED_DATA **preparsedData));
    OVR_DECLARE_HIDFUNC(HidD_FreePreparsedData,     BOOLEAN, (HIDP_PREPARSED_DATA *preparsedData));
    OVR_DECLARE_HIDFUNC(HidP_GetCaps,               NTSTATUS,(HIDP_PREPARSED_DATA *preparsedData, HIDP_CAPS* caps));
    

    Win32HIDInterface();    
    ~Win32HIDInterface();    

    // Use enumerator to simplify matching devices.
    bool Enumerate(HIDEnumerateVisitor* enumVisitor);

    HANDLE CreateHIDFile(const char* path)
    {
        return ::CreateFileA(path, GENERIC_WRITE|GENERIC_READ,
                             0x0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    }

    // Return number of bytes written, 0 for failure.
    UPInt Write(HANDLE hidDev, const UByte* data, UPInt size);


    // Helper functions to fill in HIDDeviceDesc from open device handle.
    bool InitVendorProductVersion(HANDLE hidDev, HIDDeviceDesc* desc);
    bool InitUsageAndIOLength(HANDLE hidDev, HIDDeviceDesc* desc);
    void InitStrings(HANDLE hidDev, HIDDeviceDesc* desc);
};


} // namespace OVR

#endif // OVR_Win32_HID_h
