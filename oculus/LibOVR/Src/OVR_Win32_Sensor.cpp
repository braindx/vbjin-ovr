/************************************************************************************

Filename    :   OVR_Win32_Sensor.cpp
Content     :   Oculus Sensor device implementation using direct Win32 system I/O.
Created     :   October 23, 2012
Authors     :   Michael Antonov

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#include "OVR_Win32_Sensor.h"
// HMDDeviceDesc can be created/updated through Sensor carrying DisplayInfo.
#include "OVR_Win32_HMDDevice.h"

#include "Kernel/OVR_Timer.h"


namespace OVR { namespace Win32 {

//-------------------------------------------------------------------------------------
// ***** Oculus Sensor-specific packet data structures

enum {    
    Sensor_VendorId  = 0x2833,
    Sensor_ProductId = 0x0001,

    // ST's VID used originally; should be removed in the future
    Sensor_OldVendorId  = 0x0483,
    Sensor_OldProductId = 0x5750
};

// Reported data is little-endian now
static UInt16 DecodeUInt16(const UByte* buffer)
{
    return (UInt16(buffer[1]) << 8) | UInt16(buffer[0]);
}

static SInt16 DecodeSInt16(const UByte* buffer)
{
    return (SInt16(buffer[1]) << 8) | SInt16(buffer[0]);
}

static UInt32 DecodeUInt32(const UByte* buffer)
{    
    return (buffer[0]) | UInt32(buffer[1] << 8) | UInt32(buffer[2] << 16) | UInt32(buffer[3] << 24);    
}

static float DecodeFloat(const UByte* buffer)
{
    union {
        UInt32 U;
        float  F;
    };

    U = DecodeUInt32(buffer);
    return F;
}



static void UnpackSensor(const UByte* buffer, SInt32* x, SInt32* y, SInt32* z)
{
    // Sign extending trick
    // from http://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend
    struct {SInt32 x:21;} s;

    *x = s.x = (buffer[0] << 13) | (buffer[1] << 5) | ((buffer[2] & 0xF8) >> 3);
    *y = s.x = ((buffer[2] & 0x07) << 18) | (buffer[3] << 10) | (buffer[4] << 2) |
               ((buffer[5] & 0xC0) >> 6);
    *z = s.x = ((buffer[5] & 0x3F) << 15) | (buffer[6] << 7) | (buffer[7] >> 1);
}

// Messages we care for
enum TrackerMessageType
{
    TrackerMessage_None              = 0,
    TrackerMessage_Sensors           = 1,
    TrackerMessage_Unknown           = 0x100,
    TrackerMessage_SizeError         = 0x101,
};

struct TrackerSample
{
    SInt32 AccelX, AccelY, AccelZ;
    SInt32 GyroX, GyroY, GyroZ;
};



struct TrackerSensors
{
    UByte	SampleCount;
    UInt16	Timestamp;
    UInt16	LastCommandID;
    SInt16	Temperature;

    TrackerSample Samples[3];

    SInt16	MagX, MagY, MagZ;

    TrackerMessageType Decode(const UByte* buffer, int size)
    {
        if (size < 62)
            return TrackerMessage_SizeError;

        SampleCount		= buffer[1];
        Timestamp		= DecodeUInt16(buffer + 2);
        LastCommandID	= DecodeUInt16(buffer + 4);
        Temperature		= DecodeSInt16(buffer + 6);
        
        //if (SampleCount > 2)        
        //    OVR_DEBUG_LOG_TEXT(("TackerSensor::Decode SampleCount=%d\n", SampleCount));        

        // Only unpack as many samples as there actually are
        UByte iterationCount = (SampleCount > 2) ? 3 : SampleCount;

        for (UByte i = 0; i < iterationCount; i++)
        {
            UnpackSensor(buffer + 8 + 16 * i,  &Samples[i].AccelX, &Samples[i].AccelY, &Samples[i].AccelZ);
            UnpackSensor(buffer + 16 + 16 * i, &Samples[i].GyroX,  &Samples[i].GyroY,  &Samples[i].GyroZ);
        }

        MagX = DecodeSInt16(buffer + 56);
        MagY = DecodeSInt16(buffer + 58);
        MagZ = DecodeSInt16(buffer + 60);

        return TrackerMessage_Sensors;
    }
};

struct TrackerMessage
{
    TrackerMessageType Type;
    TrackerSensors     Sensors;
};

bool DecodeTrackerMessage(TrackerMessage* message, UByte* buffer, int size)
{
    memset(message, 0, sizeof(TrackerMessage));

    if (size < 4)
    {
        message->Type = TrackerMessage_SizeError;
        return false;
    }

    switch (buffer[0])
    {
    case TrackerMessage_Sensors:
        message->Type = message->Sensors.Decode(buffer, size);
        break;

    default:
        message->Type = TrackerMessage_Unknown;
        break;
    }

    return (message->Type < TrackerMessage_Unknown) && (message->Type != TrackerMessage_None);
}


// ***** SensorScaleRange Implementation

// Sensor HW only accepts specific maximum range values, used to maximize
// the 16-bit sensor outputs. Use these ramps to specify and report appropriate values.
static const UInt16 AccelRangeRamp[] = { 2, 4, 8, 16 };
static const UInt16 GyroRangeRamp[]  = { 250, 500, 1000, 2000 };
static const UInt16 MagRangeRamp[]   = { 880, 1300, 1900, 2500 };

static UInt16 SelectSensorRampValue(const UInt16* ramp, unsigned count,
                                    float val, float factor, const char* label)
{    
    UInt16 threshold = (UInt16)(val * factor);

    for (unsigned i = 0; i<count; i++)
    {
        if (ramp[i] >= threshold)
            return ramp[i];
    }
    OVR_DEBUG_LOG(("SensorDevice::SetRange - %s clamped to %0.4f",
                   label, float(ramp[count-1]) / factor));
    OVR_UNUSED2(factor, label);
    return ramp[count-1];
}

// SensorScaleRange provides buffer packing logic for the Sensor Range
// record that can be applied to DK1 sensor through Get/SetFeature. We expose this
// through SensorRange class, which has different units.
struct SensorScaleRange
{
    enum  { PacketSize = 8 };
    UByte   Buffer[PacketSize];
    
    UInt16  CommandId;
    UInt16  AccelScale;
    UInt16  GyroScale;
    UInt16  MagScale;

    SensorScaleRange(const SensorRange& r, UInt16 commandId = 0)
    {
        SetSensorRange(r, commandId);
    }

    void SetSensorRange(const SensorRange& r, UInt16 commandId = 0)
    {
        CommandId  = commandId;
        AccelScale = SelectSensorRampValue(AccelRangeRamp, sizeof(AccelRangeRamp)/sizeof(AccelRangeRamp[0]),
                                           r.MaxAcceleration, (1.0f / 9.81f), "MaxAcceleration");
        GyroScale  = SelectSensorRampValue(GyroRangeRamp, sizeof(GyroRangeRamp)/sizeof(GyroRangeRamp[0]),
                                           r.MaxRotationRate, Math<float>::RadToDegreeFactor, "MaxRotationRate");
        MagScale   = SelectSensorRampValue(MagRangeRamp, sizeof(MagRangeRamp)/sizeof(MagRangeRamp[0]),
                                           r.MaxMagneticField, 1000.0f, "MaxMagneticField");
        Pack();
    }

    void GetSensorRange(SensorRange* r)
    {
        r->MaxAcceleration = AccelScale * 9.81f;
        r->MaxRotationRate = DegreeToRad((float)GyroScale);
        r->MaxMagneticField= MagScale * 0.001f;
    }

    static SensorRange GetMaxSensorRange()
    {
        return SensorRange(AccelRangeRamp[sizeof(AccelRangeRamp)/sizeof(AccelRangeRamp[0]) - 1] * 9.81f,
                           GyroRangeRamp[sizeof(GyroRangeRamp)/sizeof(GyroRangeRamp[0]) - 1] *
                                Math<float>::DegreeToRadFactor,
                           MagRangeRamp[sizeof(MagRangeRamp)/sizeof(MagRangeRamp[0]) - 1] * 0.001f);
    }

    void  Pack()
    {
        Buffer[0] = 4;
        Buffer[1] = UByte(CommandId & 0xFF);
        Buffer[2] = UByte(CommandId >> 8);
        Buffer[3] = UByte(AccelScale);
        Buffer[4] = UByte(GyroScale & 0xFF);
        Buffer[5] = UByte(GyroScale >> 8);
        Buffer[6] = UByte(MagScale & 0xFF);
        Buffer[7] = UByte(MagScale >> 8);
    }

    void Unpack()
    {
        CommandId = Buffer[1] | (UInt16(Buffer[2]) << 8);
        AccelScale= Buffer[3];
        GyroScale = Buffer[4] | (UInt16(Buffer[5]) << 8);
        MagScale  = Buffer[6] | (UInt16(Buffer[7]) << 8);
    }
};


// Sensor configuration command, ReportId == 2.

struct SensorConfig
{
    enum  { PacketSize = 7 };
    UByte   Buffer[PacketSize];

    // Flag values for Flags.
    enum {
        Flag_RawMode            = 0x01,
        Flag_CallibrationTest   = 0x02, // Internal test mode
        Flag_UseCallibration    = 0x04,
        Flag_AutoCallibration   = 0x08,
        Flag_MotionKeepAlive    = 0x10,
        Flag_CommandKeepAlive   = 0x20,
        Flag_SensorCoordinates  = 0x40
    };

    UInt16  CommandId;
    UByte   Flags;
    UInt16  PacketInterval;
    UInt16  KeepAliveIntervalMs;

    SensorConfig() : CommandId(0), Flags(0), PacketInterval(0), KeepAliveIntervalMs(0)
    {
        memset(Buffer, 0, PacketSize);
        Buffer[0] = 2;
    }

    void    SetSensorCoordinates(bool sensorCoordinates)
    { Flags = (Flags & ~Flag_SensorCoordinates) | (sensorCoordinates ? Flag_SensorCoordinates : 0); }
    bool    IsUsingSensorCoordinates() const
    { return (Flags & Flag_SensorCoordinates) != 0; }

    void Pack()
    {
        Buffer[0] = 2;
        Buffer[1] = UByte(CommandId & 0xFF);
        Buffer[2] = UByte(CommandId >> 8);
        Buffer[3] = Flags;
        Buffer[4] = UByte(PacketInterval);
        Buffer[5] = UByte(KeepAliveIntervalMs & 0xFF);
        Buffer[6] = UByte(KeepAliveIntervalMs >> 8);
    }

    void Unpack()
    {
        CommandId          = Buffer[1] | (UInt16(Buffer[2]) << 8);
        Flags              = Buffer[3];
        PacketInterval     = Buffer[4];
        KeepAliveIntervalMs= Buffer[5] | (UInt16(Buffer[6]) << 8);
    }
    
};


// SensorKeepAlive - feature report that needs to be sent at regular intervals for sensor
// to receive commands.
struct SensorKeepAlive
{
    enum  { PacketSize = 5 };
    UByte   Buffer[PacketSize];

    UInt16  CommandId;
    UInt16  KeepAliveIntervalMs;

    SensorKeepAlive(UInt16 interval = 0, UInt16 commandId = 0)
        : CommandId(commandId), KeepAliveIntervalMs(interval)
    {
        Pack();
    }

    void  Pack()
    {
        Buffer[0] = 8;
        Buffer[1] = UByte(CommandId & 0xFF);
        Buffer[2] = UByte(CommandId >> 8);
        Buffer[3] = UByte(KeepAliveIntervalMs & 0xFF);
        Buffer[4] = UByte(KeepAliveIntervalMs >> 8);
    }

    void Unpack()
    {
        CommandId          = Buffer[1] | (UInt16(Buffer[2]) << 8);
        KeepAliveIntervalMs= Buffer[3] | (UInt16(Buffer[4]) << 8);
    }
};



// DisplayInfo obtained from sensor; these values are used to report distortion
// settings and other coefficients.
// Older SensorDisplayInfo will have all zeros, causing the library to apply hard-coded defaults.
// Currently, only resolutions and sizes are used.

struct SensorDisplayInfo
{
    enum  { PacketSize = 56 };
    UByte   Buffer[PacketSize];

    enum
    {
        Mask_BaseFmt    = 0x0f,
        Mask_OptionFmts = 0xf0,
        Base_None       = 0,
        Base_ScreenOnly = 1,
        Base_Distortion = 2,
    };

    UInt16  CommandId;
    UByte   DistortionType;    
    UInt16  HResolution, VResolution;
    float   HScreenSize, VScreenSize;
    float   VCenter;
    float   LensSeparation;
    float   EyeToScreenDistance[2];
    float   DistortionK[6];

    SensorDisplayInfo() : CommandId(0)
    {
        memset(Buffer, 0, PacketSize);
        Buffer[0] = 9;
    }
    /*
    void Pack()
    {
        Buffer[0] = 9;
        Buffer[1] = UByte(CommandId & 0xFF);
        Buffer[2] = UByte(CommandId >> 8);
        Buffer[3] = DistortionType;               
    } */

    void Unpack()
    {
        CommandId               = Buffer[1] | (UInt16(Buffer[2]) << 8);
        DistortionType          = Buffer[3];
        HResolution             = DecodeUInt16(Buffer+4);
        VResolution             = DecodeUInt16(Buffer+6);
        HScreenSize             = DecodeUInt32(Buffer+8) *  (1/1000000.f);
        VScreenSize             = DecodeUInt32(Buffer+12) * (1/1000000.f);
        VCenter                 = DecodeUInt32(Buffer+16) * (1/1000000.f);
        LensSeparation          = DecodeUInt32(Buffer+20) * (1/1000000.f);
        EyeToScreenDistance[0]  = DecodeUInt32(Buffer+24) * (1/1000000.f);
        EyeToScreenDistance[1]  = DecodeUInt32(Buffer+28) * (1/1000000.f);
        DistortionK[0]          = DecodeFloat(Buffer+32);
        DistortionK[1]          = DecodeFloat(Buffer+36);
        DistortionK[2]          = DecodeFloat(Buffer+40);
        DistortionK[3]          = DecodeFloat(Buffer+44);
        DistortionK[4]          = DecodeFloat(Buffer+48);
        DistortionK[5]          = DecodeFloat(Buffer+52);
    }

};





//-------------------------------------------------------------------------------------
// ***** SensorDeviceFactory

SensorDeviceFactory SensorDeviceFactory::Instance;


void SensorDeviceFactory::EnumerateDevices(EnumerateVisitor& visitor)
{

    class SensorEnumerator : public HIDEnumerateVisitor
    {
        // Assign not supported; suppress MSVC warning.
        void operator = (const SensorEnumerator&) { }

        DeviceFactory*     pFactory;
        EnumerateVisitor&  ExternalVisitor;   
    public:
        SensorEnumerator(DeviceFactory* factory, EnumerateVisitor& externalVisitor)
            : pFactory(factory), ExternalVisitor(externalVisitor) { }

        virtual bool MatchVendorProduct(UInt16 vendorId, UInt16 productId)
        {
            return ((vendorId == Sensor_VendorId) && (productId == Sensor_ProductId)) ||
                   ((vendorId == Sensor_OldVendorId) && (productId == Sensor_OldProductId));
        }

        virtual void Visit(HANDLE hidDev, const HIDDeviceDesc& desc)
        {
            SensorDeviceCreateDesc createDesc(pFactory, desc);
            ExternalVisitor.Visit(createDesc);

            // Check if the sensor returns DisplayInfo. If so, try to use it to override potentially
            // mismatching monitor information (in case wrong EDID is reported by splitter),
            // or to create a new "virtualized" HMD Device.
            DeviceManager* manager = (Win32::DeviceManager*)pFactory->GetManagerImpl();
            Win32HIDInterface& hid = manager->HIDInterface;
            
            SensorDisplayInfo displayInfo;
            if (hid.HidD_GetFeature(hidDev, displayInfo.Buffer, SensorDisplayInfo::PacketSize))            
                displayInfo.Unpack();

            /*
            displayInfo.HResolution = 1280;
            displayInfo.VResolution = 800;
            displayInfo.HScreenSize = 0.14976f;
            displayInfo.VScreenSize = 0.0936f;
            */

            // If we got display info, try to match / create HMDDevice as well
            // so that sensor settings give preference.
            if (displayInfo.DistortionType & SensorDisplayInfo::Mask_BaseFmt)
            {
                HMDDeviceCreateDesc hmdCreateDesc(&HMDDeviceFactory::Instance, String(), String());
                hmdCreateDesc.SetScreenParameters(0, 0,
                                                  displayInfo.HResolution, displayInfo.VResolution,
                                                  displayInfo.HScreenSize, displayInfo.VScreenSize);

                if ((displayInfo.DistortionType & SensorDisplayInfo::Mask_BaseFmt) == SensorDisplayInfo::Base_Distortion)
                    hmdCreateDesc.SetDistortion(displayInfo.DistortionK);
                if (displayInfo.HScreenSize > 0.14f)
                    hmdCreateDesc.Set7Inch();

                ExternalVisitor.Visit(hmdCreateDesc);
            }                       
        }
    };

    //double start = Timer::GetProfileSeconds();

    SensorEnumerator sensorEnumerator(this, visitor);
    getManager()->HIDInterface.Enumerate(&sensorEnumerator);

    //double totalSeconds = Timer::GetProfileSeconds() - start; 
}


//-------------------------------------------------------------------------------------
// ***** SensorDeviceCreateDesc

DeviceBase* SensorDeviceCreateDesc::NewDeviceInstance()
{
    return new SensorDevice(this);
}

bool SensorDeviceCreateDesc::GetDeviceInfo(DeviceInfo* info) const
{
    if ((info->InfoClassType != Device_Sensor) &&
        (info->InfoClassType != Device_None))
        return false;

    OVR_strcpy(info->ProductName,  DeviceInfo::MaxNameLength, HIDDesc.Product.ToCStr());
    OVR_strcpy(info->Manufacturer, DeviceInfo::MaxNameLength, HIDDesc.Manufacturer.ToCStr());
    info->Type    = Device_Sensor;
    info->Version = 0;

    if (info->InfoClassType == Device_Sensor)
    {
        SensorInfo* sinfo = (SensorInfo*)info;
        sinfo->VendorId  = HIDDesc.VendorId;
        sinfo->ProductId = HIDDesc.ProductId;
        sinfo->MaxRanges = SensorScaleRange::GetMaxSensorRange();
        OVR_strcpy(sinfo->SerialNumber, sizeof(sinfo->SerialNumber),HIDDesc.SerialNumber.ToCStr());
    }
    return true;
}


//-------------------------------------------------------------------------------------
// ***** Freespace::SensorDevice

SensorDevice::SensorDevice(SensorDeviceCreateDesc* createDesc)
    : OVR::DeviceImpl<OVR::SensorDevice>(createDesc, 0),
      Coordinates(SensorDevice::Coord_Sensor),
      HWCoordinates(SensorDevice::Coord_HMD), // HW reports HMD coorinates by default.
      NextKeepAliveTicks(0),
      MaxValidRange(SensorScaleRange::GetMaxSensorRange()),
      hDev(NULL), ReadRequested(false)
{
    SequenceValid  = false;
    LastSampleCount= 0;
    LastTimestamp   = 0;

    OldCommandId = 0;

    memset(&ReadOverlapped, 0, sizeof(OVERLAPPED));
}

SensorDevice::~SensorDevice()
{
    // Check that Shutdown() was called.
    OVR_ASSERT(!pCreateDesc->pDevice);    
}

// Internal creation APIs.
bool SensorDevice::Initialize(DeviceBase* parent)
{
    HIDDeviceDesc& hidDesc = *getHIDDesc();

    if (ReadBufferSize < hidDesc.InputReportByteLength)
    {
        OVR_ASSERT(false);
        return false;
    }

    const char* errorFormatString = "";
    if (!openDevice(&errorFormatString))
    {
        LogText(errorFormatString, hidDesc.Path.ToCStr());
        return false;
    }

    getManagerImpl()->pThread->AddTicksNotifier(this);
	getManagerImpl()->pThread->AddMessageNotifier(this);

    LogText("OVR::SensorDevice - Opened '%s'\n"
            "                    Manufacturer:'%s'  Product:'%s'  Serial#:'%s'\n",
            hidDesc.Path.ToCStr(),
            hidDesc.Manufacturer.ToCStr(), hidDesc.Product.ToCStr(),
            hidDesc.SerialNumber.ToCStr());
   
    // AddRef() to parent, forcing chain to stay alive.
    pParent = parent;
    return true;
}


void SensorDevice::Shutdown()
{   
    // Remove the handler, if any.
    HandlerRef.SetHandler(0);
    getManagerImpl()->pThread->RemoveTicksNotifier(this);
	getManagerImpl()->pThread->RemoveMessageNotifier(this);

    closeDevice();
    LogText("OVR::SensorDevice - Closed '%s'\n", getHIDDesc()->Path.ToCStr());

    pParent.Clear();
}


bool SensorDevice::openDevice(const char ** errorFormatString)
{
    HIDDeviceDesc&     hidDesc = *getHIDDesc();
    DeviceManager*     manager = getManagerImpl();
    Win32HIDInterface& hid     = manager->HIDInterface;

    hDev = hid.CreateHIDFile(hidDesc.Path.ToCStr());
    if (hDev == INVALID_HANDLE_VALUE)
    {
        *errorFormatString = "OVR::SensorDevice - Failed to open '%s'\n";
        hDev = 0;
        return false;
    }

    *errorFormatString = "OVR::SensorDevice - Failed to initialize '%s' during open\n";

    if (!hid.HidD_SetNumInputBuffers(hDev, 128))
    {
        ::CloseHandle(hDev);
        hDev = 0;
        return false;
    }

    // Create a manual-reset non-signaled event
    memset(&ReadOverlapped, 0, sizeof(OVERLAPPED)); 
    ReadOverlapped.hEvent = ::CreateEvent(0, TRUE, FALSE, 0);

    if (!ReadOverlapped.hEvent)
    {
        ::CloseHandle(hDev);
        hDev = 0;
        return false;
    }

    // Read the currently configured range from sensor.
    SensorScaleRange ssr(SensorRange(), 0);
    if (hid.HidD_GetFeature(hDev, ssr.Buffer, SensorScaleRange::PacketSize))
    {
        ssr.Unpack();
        ssr.GetSensorRange(&CurrentRange);
    }

    // If the sensor has "DisplayInfo" data, use HMD coordinate frame by default.
    SensorDisplayInfo displayInfo;
    if (hid.HidD_GetFeature(hDev, displayInfo.Buffer, SensorDisplayInfo::PacketSize))
    {
        displayInfo.Unpack();
        Coordinates = (displayInfo.DistortionType & SensorDisplayInfo::Mask_BaseFmt) ?
                      Coord_HMD : Coord_Sensor;
    }

    // Read/Apply sensor config.
    setCoordinateFrame(Coordinates);

    // Set Keep-alive at 10 seconds.
    SensorKeepAlive skeepAlive(10 * 1000);
    hid.HidD_SetFeature(hDev, skeepAlive.Buffer, SensorKeepAlive::PacketSize);

    if (!initializeRead())
    {
        hDev = 0;
        return false;
    }

    *errorFormatString = "";
    return true;
}


void SensorDevice::closeDevice()
{
    if (ReadRequested)
    {
        getManagerImpl()->pThread->RemoveOverlappedEvent(this, ReadOverlapped.hEvent);
        ReadRequested = false;
        // Must call this to avoid Win32 assertion; CloseHandle is not enough.
        ::CancelIo(hDev);
    }

    ::CloseHandle(ReadOverlapped.hEvent);
    memset(&ReadOverlapped, 0, sizeof(OVERLAPPED));
    ::CloseHandle(hDev);
    hDev = 0;
}

void SensorDevice::closeDeviceOnIOError()
{
    LogText("OVR::SensorDevice - Lost connection to '%s'\n", getHIDDesc()->Path.ToCStr());
    closeDevice();
    NextKeepAliveTicks = 0;
}


bool SensorDevice::initializeRead()
{
    DeviceManager* manager = getManagerImpl();

    if (!ReadRequested)
    {        
        manager->pThread->AddOverlappedEvent(this, ReadOverlapped.hEvent);
        ReadRequested = true;
    }
   
    // Read resets the event...
    while(::ReadFile(hDev, ReadBuffer, getHIDDesc()->InputReportByteLength, 0, &ReadOverlapped))
    {
        processReadResult();
    }
    
    if (GetLastError() != ERROR_IO_PENDING)
    {
        // Some other error (such as unplugged).
        closeDeviceOnIOError();
        return false;
    }

    return true;
}


bool SensorDevice::processReadResult()
{
    OVR_ASSERT(ReadRequested);

    DWORD bytesRead = 0;

    if (GetOverlappedResult(hDev, &ReadOverlapped, &bytesRead, FALSE))
    {
        // We got data.
        TrackerMessage message;
        if (DecodeTrackerMessage(&message, ReadBuffer, bytesRead))     
            onTrackerMessage(&message);

        // TBD: Not needed?
        // Event should be reset by Read call...
        ReadOverlapped.Pointer = 0;
        ReadOverlapped.Internal = 0;
        ReadOverlapped.InternalHigh = 0;
        return true;
    }
    else
    {
        if (GetLastError() != ERROR_IO_PENDING)
        {
            closeDeviceOnIOError();
            return false;
        }
    }

    return false;
}


void SensorDevice::OnOverlappedEvent(HANDLE hevent)
{
    OVR_UNUSED(hevent);
    OVR_ASSERT(hevent == ReadOverlapped.hEvent);

    if (processReadResult()) 
	{
        // Proceed to read further.
        initializeRead();
    }
}

UInt64 SensorDevice::OnTicks(UInt64 ticksMks)
{
    if (ticksMks >= NextKeepAliveTicks)
    {
        // Use 3-seconds keep alive by default.
        UInt64 keepAliveDelta = Timer::MksPerSecond * 3;

        if (hDev)
        {
            DeviceManager*     manager = getManagerImpl();
            Win32HIDInterface& hid     = manager->HIDInterface;
            // Set Keep-alive at 10 seconds.
            SensorKeepAlive skeepAlive(10 * 1000);
            hid.HidD_SetFeature(hDev, skeepAlive.Buffer, SensorKeepAlive::PacketSize);
        }

		// Emit keep-alive every few seconds.
        NextKeepAliveTicks = ticksMks + keepAliveDelta;
    }
    return NextKeepAliveTicks - ticksMks;
}

bool SensorDevice::OnDeviceMessage(DeviceMessageType messageType, const String& devicePath)
{
	// Is this the correct device?
	SensorDeviceCreateDesc* pCreateDesc = getCreateDesc();
	if (pCreateDesc->HIDDesc.Path.CompareNoCase(devicePath) != 0)
	{
		return false;
	}

	if (messageType == DeviceMessage_DeviceAdded && !hDev)
	{
		// A device has been re-added that is currently closed. Try to reopen.
		const char *errorFormatString = "";
		if (!openDevice(&errorFormatString))
		{
			LogError("OVR::SendorDevice - Failed to reopen a device '%s' that was re-added. Error: %s.\n", devicePath.ToCStr(), errorFormatString);
			return true;
		}

		LogText("OVR::SensorDevice - Reopened device '%s'\n", devicePath.ToCStr());
	}

	MessageType handlerMessageType = handlerMessageType = Message_DeviceAdded;
	if (messageType == DeviceMessage_DeviceAdded)
	{
	}
	else if (messageType == DeviceMessage_DeviceRemoved)
	{
		handlerMessageType = Message_DeviceRemoved;
	}
	else
	{
		OVR_ASSERT(0);		
	}

	
	// Do Sensor notification.
    {
        Lock::Locker scopeLock(HandlerRef.GetLock());

        if (HandlerRef.GetHandler())
        {
            Message msg(handlerMessageType, this);
            HandlerRef.GetHandler()->OnMessage(msg);
        }
    }


	// Do device manager notification.
	DeviceManager*   manager = getManagerImpl();

	if (handlerMessageType == Message_DeviceAdded)
	{
		manager->CallOnDeviceAdded(pCreateDesc);
	}
	else if (handlerMessageType == Message_DeviceRemoved)
	{
		manager->CallOnDeviceRemoved(pCreateDesc);
	}

	return true;
}

bool SensorDevice::SetRange(const SensorRange& range, bool waitFlag)
{
    bool                 setRangeResult = 0;
    ThreadCommandQueue * threadQueue = getManagerImpl()->GetThreadQueue();

    if (!waitFlag)
        return threadQueue->PushCall(this, &SensorDevice::setRange, range);
    
    if (!threadQueue->PushCallAndWaitResult(this, &SensorDevice::setRange,
                                            &setRangeResult, range))
        return false;

    return setRangeResult;
}

void SensorDevice::GetRange(SensorRange* range) const
{
    Lock::Locker lockScope(GetLock());
    *range = CurrentRange;
}

bool SensorDevice::setRange(const SensorRange& range)
{
    if (!ReadRequested)
        return false;

    SensorScaleRange ssr(range);
    DeviceManager*   manager = getManagerImpl();
    if (manager->HIDInterface.HidD_SetFeature(hDev, (void*)ssr.Buffer,
                                              SensorScaleRange::PacketSize))
    {
        Lock::Locker lockScope(GetLock());
        ssr.GetSensorRange(&CurrentRange);
        return true;
    }
    return false;
}


void SensorDevice::SetCoordinateFrame(CoordinateFrame coordframe)
{ 
    // Push call with wait.
    getManagerImpl()->GetThreadQueue()->
        PushCall(this, &SensorDevice::setCoordinateFrame, coordframe, true);
}

SensorDevice::CoordinateFrame SensorDevice::GetCoordinateFrame() const
{
    return Coordinates;
}

Void SensorDevice::setCoordinateFrame(CoordinateFrame coordframe)
{
    DeviceManager*     manager = getManagerImpl();
    Win32HIDInterface& hid     = manager->HIDInterface;

    Coordinates = coordframe;

    // Read the original coordinate frame, then try to change it.
    SensorConfig scfg;
    if (hid.HidD_GetFeature(hDev, scfg.Buffer, SensorConfig::PacketSize))
    {
        scfg.Unpack();
    }

    scfg.SetSensorCoordinates(coordframe == Coord_Sensor);
    scfg.Pack();
    hid.HidD_SetFeature(hDev, scfg.Buffer, SensorConfig::PacketSize);
    
    // Re-read the state, in case of older firmware that doesn't support Sensor coordinates.
    if (hid.HidD_GetFeature(hDev, scfg.Buffer, SensorConfig::PacketSize))
    {
        scfg.Unpack();
        HWCoordinates = scfg.IsUsingSensorCoordinates() ? Coord_Sensor : Coord_HMD;
    }
    else
    {
        HWCoordinates = Coord_HMD;
    }
    return 0;
}



bool SensorDevice::SetFeature(UByte* data, UPInt size, bool waitFlag)
{
    if (size > WriteData::BufferSize)
    {
        OVR_DEBUG_LOG(("SensorDevice::SetFeature failed - Max size == %d",
                       WriteData::BufferSize));
        return 0;
    }

    // Right now ReadRequested == false means something failed during IO;
    // in that case fail the command as well..
    if (!ReadRequested)
        return false;

    bool                 setFeatureResult = 0;
    ThreadCommandQueue * threadQueue = getManagerImpl()->GetThreadQueue();
    WriteData            writeData(data, size);

    if (!waitFlag)
        return threadQueue->PushCall(this, &SensorDevice::setFeature, writeData);
    
    if (!threadQueue->PushCallAndWaitResult(this, &SensorDevice::setFeature,
                                            &setFeatureResult, writeData))
        return false;

    return setFeatureResult;
}

bool SensorDevice::GetFeature(UByte* data, UPInt size)
{    
    if (!ReadRequested)
        return false;
    bool                 getFeatureResult = false;
    ThreadCommandQueue * threadQueue = getManagerImpl()->GetThreadQueue();

    if (!threadQueue->PushCallAndWaitResult(this, &SensorDevice::getFeature,
                                            &getFeatureResult, data, size))
        return false;
    return getFeatureResult;
}


bool SensorDevice::setFeature(const WriteData& data)
{
    if (!ReadRequested)
        return false;
    DeviceManager* manager = getManagerImpl();
    return manager->HIDInterface.HidD_SetFeature(hDev, (void*)data.Buffer, (ULONG)data.Size) != FALSE;
}

bool SensorDevice::getFeature(UByte* data, UPInt size)
{
    if (!ReadRequested)
        return false;
    DeviceManager* manager = getManagerImpl();
    return manager->HIDInterface.HidD_GetFeature(hDev, data, (ULONG)size) != FALSE;
}



/*
UPInt SensorDevice::WriteCommand(UByte* data, UPInt size, bool waitFlag)
{
    if (size > WriteData::BufferSize)
    {
        OVR_DEBUG_LOG(("SensorDevice::WriteCommand failed - Max size == %d",
                       WriteData::BufferSize));
        return 0;
    }

    // Right now ReadRequested == false means something failed during IO;
    // in that case fail the command as well..
    if (!ReadRequested)
        return 0;

    UPInt                bytesWritten = 0;
    ThreadCommandQueue * threadQueue = getManagerImpl()->GetThreadQueue();
    WriteData            writeData(data, size);

    if (!waitFlag)
    {
        if (threadQueue->PushCall(this, &SensorDevice::writeCommand, writeData))
            return size;
        return 0;
    }

    if (!threadQueue->PushCallAndWaitResult(this, &SensorDevice::writeCommand,
                                            &bytesWritten, writeData))
    {
        return 0;
    }    
    return bytesWritten;    
}

UPInt SensorDevice::writeCommand(const WriteData& data)
{
    if (!ReadRequested)
        return 0;
    DeviceManager* manager = getManagerImpl();
    return manager->HIDInterface.Write(hDev, data.Buffer, data.Size);
}
*/

void SensorDevice::SetMessageHandler(MessageHandler* handler)
{
    if (handler)
    {
        SequenceValid = false;
        DeviceBase::SetMessageHandler(handler);
    }
    else
    {       
        DeviceBase::SetMessageHandler(handler);
    }    
}




// Sensor reports data in the following coordinate system:
// Accelerometer: 10^-4 m/s^2; X forward, Y right, Z Down.
// Gyro:          10^-4 rad/s; X positive roll right, Y positive pitch up; Z positive yaw right.


// We need to convert it to the following RHS coordinate system:
// X right, Y Up, Z Back (out of screen)
//
Vector3f AccelFromBodyFrameUpdate(const TrackerSensors& update, UByte sampleNumber,
                                  bool convertHMDToSensor = false)
{
    const TrackerSample& sample = update.Samples[sampleNumber];
    float                ax = (float)sample.AccelX;
    float                ay = (float)sample.AccelY;
    float                az = (float)sample.AccelZ;

    Vector3f val = convertHMDToSensor ? Vector3f(ax, az, -ay) :  Vector3f(ax, ay, az);
    return val * 0.0001f;
}


Vector3f MagFromBodyFrameUpdate(const TrackerSensors& update,
                                bool convertHMDToSensor = false)
{    
    if (!convertHMDToSensor)
    {
        return Vector3f( (float)update.MagX,
                         (float)update.MagY,
                         (float)update.MagZ) * 0.0001f;
    }    

    return Vector3f( (float)update.MagX,
                     (float)update.MagZ,
                    -(float)update.MagY) * 0.0001f;
}

Vector3f EulerFromBodyFrameUpdate(const TrackerSensors& update, UByte sampleNumber,
                                  bool convertHMDToSensor = false)
{
    const TrackerSample& sample = update.Samples[sampleNumber];
    float                gx = (float)sample.GyroX;
    float                gy = (float)sample.GyroY;
    float                gz = (float)sample.GyroZ;

    Vector3f val = convertHMDToSensor ? Vector3f(gx, gz, -gy) :  Vector3f(gx, gy, gz);
    return val * 0.0001f;
}


void SensorDevice::onTrackerMessage(TrackerMessage* message)
{
    if (message->Type != TrackerMessage_Sensors)
        return;
    
    const float     timeUnit   = (1.0f / 1000.f);
    TrackerSensors& s = message->Sensors;
    

    // Call OnMessage() within a lock to avoid conflicts with handlers.
    Lock::Locker scopeLock(HandlerRef.GetLock());


    if (SequenceValid)
    {
        unsigned timestampDelta;

        if (s.Timestamp < LastTimestamp)
            timestampDelta = ((((int)s.Timestamp) + 0x10000) - (int)LastTimestamp);
        else
            timestampDelta = (s.Timestamp - LastTimestamp);

        // If we missed a small number of samples, replicate the last sample.
        if ((timestampDelta > LastSampleCount) && (timestampDelta <= 254))
        {
            if (HandlerRef.GetHandler())
            {
                MessageBodyFrame sensors(this);
                sensors.TimeDelta     = (timestampDelta - LastSampleCount) * timeUnit;
                sensors.Acceleration  = LastAcceleration;
                sensors.RotationRate  = LastRotationRate;
                sensors.MagneticField = LastMagneticField;
                sensors.Temperature   = LastTemperature;

                HandlerRef.GetHandler()->OnMessage(sensors);
            }
        }
    }
    else
    {
        LastAcceleration = Vector3f(0);
        LastRotationRate = Vector3f(0);
        LastMagneticField= Vector3f(0);
        LastTemperature  = 0;
        SequenceValid    = true;
    }

    LastSampleCount = s.SampleCount;
    LastTimestamp   = s.Timestamp;

    bool convertHMDToSensor = (Coordinates == Coord_Sensor) && (HWCoordinates == Coord_HMD);

    if (HandlerRef.GetHandler())
    {
        MessageBodyFrame sensors(this);                
        UByte            iterations = s.SampleCount;

        if (s.SampleCount > 3)
        {
            iterations        = 3;
            sensors.TimeDelta = (s.SampleCount - 2) * timeUnit;
        }
        else
        {
            sensors.TimeDelta = timeUnit;
        }

        for (UByte i = 0; i < iterations; i++)
        {            
            sensors.Acceleration = AccelFromBodyFrameUpdate(s, i, convertHMDToSensor);
            sensors.RotationRate = EulerFromBodyFrameUpdate(s, i, convertHMDToSensor);
            sensors.MagneticField= MagFromBodyFrameUpdate(s, convertHMDToSensor);
            sensors.Temperature  = s.Temperature * 0.01f;
            HandlerRef.GetHandler()->OnMessage(sensors);
            // TimeDelta for the last two sample is always fixed.
            sensors.TimeDelta = timeUnit;
        }

        LastAcceleration = sensors.Acceleration;
        LastRotationRate = sensors.RotationRate;
        LastMagneticField= sensors.MagneticField;
        LastTemperature  = sensors.Temperature;
    }
    else
    {
        UByte i = (s.SampleCount > 3) ? 2 : (s.SampleCount - 1);
        LastAcceleration  = AccelFromBodyFrameUpdate(s, i, convertHMDToSensor);
        LastRotationRate  = EulerFromBodyFrameUpdate(s, i, convertHMDToSensor);
        LastMagneticField = MagFromBodyFrameUpdate(s, convertHMDToSensor);
        LastTemperature   = s.Temperature * 0.01f;
    }
}


}} // namespace OVR::Win32


