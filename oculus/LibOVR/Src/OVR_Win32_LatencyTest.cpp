/************************************************************************************

Filename    :   OVR_Win32_LatencyTester.cpp
Content     :   Oculus Latency Tester device implementation using direct Win32 system I/O.
Created     :   February 8, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#include "OVR_Win32_LatencyTest.h"

namespace OVR { namespace Win32 {

//-------------------------------------------------------------------------------------
// ***** Oculus Latency Tester specific packet data structures

enum {    
    LatencyTester_VendorId  = 0x2833,
    LatencyTester_ProductId = 0x0101,
};

// Reported data is little-endian now
static UInt16 DecodeUInt16(const UByte* buffer)
{
    return (UInt16(buffer[1]) << 8) | UInt16(buffer[0]);
}
/* Unreferenced
static SInt16 DecodeSInt16(const UByte* buffer)
{
    return (SInt16(buffer[1]) << 8) | SInt16(buffer[0]);
}*/

static void UnpackSamples(const UByte* buffer, UByte* r, UByte* g, UByte* b)
{
    *r = buffer[0];
    *g = buffer[1];
    *b = buffer[2];
}

// Messages we handle.
enum LatencyTestMessageType
{
    LatencyTestMessage_None                 = 0,
    LatencyTestMessage_Samples              = 1,
    LatencyTestMessage_ColorDetected        = 2,
    LatencyTestMessage_TestStarted          = 3,
    LatencyTestMessage_Button               = 4,
    LatencyTestMessage_Unknown              = 0x100,
    LatencyTestMessage_SizeError            = 0x101,
};

struct LatencyTestSample
{
    UByte Value[3];
};

struct LatencyTestSamples
{
    UByte	SampleCount;
    UInt16	Timestamp;

    LatencyTestSample Samples[20];

    LatencyTestMessageType Decode(const UByte* buffer, int size)
    {
        if (size < 64)
        {
            return LatencyTestMessage_SizeError;
        }

        SampleCount		= buffer[1];
        Timestamp		= DecodeUInt16(buffer + 2);
        
        for (UByte i = 0; i < SampleCount; i++)
        {
            UnpackSamples(buffer + 4 + (3 * i),  &Samples[i].Value[0], &Samples[i].Value[1], &Samples[i].Value[2]);
        }

        return LatencyTestMessage_Samples;
    }
};

struct LatencyTestSamplesMessage
{
    LatencyTestMessageType      Type;
    LatencyTestSamples        Samples;
};

bool DecodeLatencyTestSamplesMessage(LatencyTestSamplesMessage* message, UByte* buffer, int size)
{
    memset(message, 0, sizeof(LatencyTestSamplesMessage));

    if (size < 64)
    {
        message->Type = LatencyTestMessage_SizeError;
        return false;
    }

    switch (buffer[0])
    {
    case LatencyTestMessage_Samples:
        message->Type = message->Samples.Decode(buffer, size);
        break;

    default:
        message->Type = LatencyTestMessage_Unknown;
        break;
    }

    return (message->Type < LatencyTestMessage_Unknown) && (message->Type != LatencyTestMessage_None);
}

struct LatencyTestColorDetected
{
    UInt16	CommandID;
    UInt16	Timestamp;
    UInt16  Elapsed;
    UByte   TriggerValue[3];
    UByte   TargetValue[3];

    LatencyTestMessageType Decode(const UByte* buffer, int size)
    {
        if (size < 13)
            return LatencyTestMessage_SizeError;

        CommandID = DecodeUInt16(buffer + 1);
        Timestamp = DecodeUInt16(buffer + 3);
        Elapsed = DecodeUInt16(buffer + 5);
        memcpy(TriggerValue, buffer + 7, 3);
        memcpy(TargetValue, buffer + 10, 3);

        return LatencyTestMessage_ColorDetected;
    }
};

struct LatencyTestColorDetectedMessage
{
    LatencyTestMessageType    Type;
    LatencyTestColorDetected  ColorDetected;
};

bool DecodeLatencyTestColorDetectedMessage(LatencyTestColorDetectedMessage* message, UByte* buffer, int size)
{
    memset(message, 0, sizeof(LatencyTestColorDetectedMessage));

    if (size < 13)
    {
        message->Type = LatencyTestMessage_SizeError;
        return false;
    }

    switch (buffer[0])
    {
    case LatencyTestMessage_ColorDetected:
        message->Type = message->ColorDetected.Decode(buffer, size);
        break;

    default:
        message->Type = LatencyTestMessage_Unknown;
        break;
    }

    return (message->Type < LatencyTestMessage_Unknown) && (message->Type != LatencyTestMessage_None);
}

struct LatencyTestStarted
{
    UInt16	CommandID;
    UInt16	Timestamp;
    UByte   TargetValue[3];

    LatencyTestMessageType Decode(const UByte* buffer, int size)
    {
        if (size < 8)
            return LatencyTestMessage_SizeError;

        CommandID = DecodeUInt16(buffer + 1);
        Timestamp = DecodeUInt16(buffer + 3);
        memcpy(TargetValue, buffer + 5, 3);

        return LatencyTestMessage_TestStarted;
    }
};

struct LatencyTestStartedMessage
{
    LatencyTestMessageType  Type;
    LatencyTestStarted      TestStarted;
};

bool DecodeLatencyTestStartedMessage(LatencyTestStartedMessage* message, UByte* buffer, int size)
{
    memset(message, 0, sizeof(LatencyTestStartedMessage));

    if (size < 8)
    {
        message->Type = LatencyTestMessage_SizeError;
        return false;
    }

    switch (buffer[0])
    {
    case LatencyTestMessage_TestStarted:
        message->Type = message->TestStarted.Decode(buffer, size);
        break;

    default:
        message->Type = LatencyTestMessage_Unknown;
        break;
    }

    return (message->Type < LatencyTestMessage_Unknown) && (message->Type != LatencyTestMessage_None);
}

struct LatencyTestButton
{
    UInt16	CommandID;
    UInt16	Timestamp;

    LatencyTestMessageType Decode(const UByte* buffer, int size)
    {
        if (size < 5)
            return LatencyTestMessage_SizeError;

        CommandID = DecodeUInt16(buffer + 1);
        Timestamp = DecodeUInt16(buffer + 3);

        return LatencyTestMessage_Button;
    }
};

struct LatencyTestButtonMessage
{
    LatencyTestMessageType    Type;
    LatencyTestButton         Button;
};

bool DecodeLatencyTestButtonMessage(LatencyTestButtonMessage* message, UByte* buffer, int size)
{
    memset(message, 0, sizeof(LatencyTestButtonMessage));

    if (size < 5)
    {
        message->Type = LatencyTestMessage_SizeError;
        return false;
    }

    switch (buffer[0])
    {
    case LatencyTestMessage_Button:
        message->Type = message->Button.Decode(buffer, size);
        break;

    default:
        message->Type = LatencyTestMessage_Unknown;
        break;
    }

    return (message->Type < LatencyTestMessage_Unknown) && (message->Type != LatencyTestMessage_None);
}

struct LatencyTestConfiguration
{
    enum  { PacketSize = 5 };
    UByte   Buffer[PacketSize];

    OVR::LatencyTestConfiguration  Configuration;

    LatencyTestConfiguration(const OVR::LatencyTestConfiguration& configuration)
        : Configuration(configuration)
    {
        Pack();
    }

    void Pack()
    {
        Buffer[0] = 5;
		Buffer[1] = UByte(Configuration.SendSamples);
		Buffer[2] = Configuration.Threshold.R;
        Buffer[3] = Configuration.Threshold.G;
        Buffer[4] = Configuration.Threshold.B;
    }

    void Unpack()
    {
		Configuration.SendSamples = Buffer[1] != 0 ? true : false;
        Configuration.Threshold.R = Buffer[2];
        Configuration.Threshold.G = Buffer[3];
        Configuration.Threshold.B = Buffer[4];
    }
};

struct LatencyTestCalibrate
{
    enum  { PacketSize = 4 };
    UByte   Buffer[PacketSize];

    OVR::LatencyTestCalibrate  Calibrate;

    LatencyTestCalibrate(const OVR::LatencyTestCalibrate& calibrate)
        : Calibrate(calibrate)
    {
        Pack();
    }

    void Pack()
    {
        Buffer[0] = 7;
		Buffer[1] = Calibrate.Value.R;
		Buffer[2] = Calibrate.Value.G;
		Buffer[3] = Calibrate.Value.B;
    }

    void Unpack()
    {
        Calibrate.Value.R = Buffer[1];
        Calibrate.Value.G = Buffer[2];
        Calibrate.Value.B = Buffer[3];
    }
};

struct LatencyTestStartTest
{
    enum  { PacketSize = 6 };
    UByte   Buffer[PacketSize];

    OVR::LatencyTestStartTest  StartTest;

    LatencyTestStartTest(const OVR::LatencyTestStartTest& startTest)
        : StartTest(startTest)
    {
        Pack();
    }

    void Pack()
    {
        UInt16 commandID = 1;

        Buffer[0] = 8;
		Buffer[1] = UByte(commandID  & 0xFF);
		Buffer[2] = UByte(commandID >> 8);
		Buffer[3] = StartTest.TargetValue.R;
		Buffer[4] = StartTest.TargetValue.G;
		Buffer[5] = StartTest.TargetValue.B;
    }

    void Unpack()
    {
        UInt16 commandID = Buffer[1] | (UInt16(Buffer[2]) << 8); commandID;
        StartTest.TargetValue.R = Buffer[3];
        StartTest.TargetValue.G = Buffer[4];
        StartTest.TargetValue.B = Buffer[5];
    }
};

struct LatencyTestDisplay
{
    enum  { PacketSize = 6 };
    UByte   Buffer[PacketSize];

    OVR::LatencyTestDisplay  Display;

    LatencyTestDisplay(const OVR::LatencyTestDisplay& display)
        : Display(display)
    {
        Pack();
    }

    void Pack()
    {
        Buffer[0] = 9;
        Buffer[1] = Display.Mode;
        Buffer[2] = UByte(Display.Value & 0xFF);
        Buffer[3] = UByte((Display.Value >> 8) & 0xFF);
        Buffer[4] = UByte((Display.Value >> 16) & 0xFF);
        Buffer[5] = UByte((Display.Value >> 24) & 0xFF);
    }

    void Unpack()
    {
        Display.Mode = Buffer[1];
        Display.Value = UInt32(Buffer[2]) |
                        (UInt32(Buffer[3]) << 8) |
                        (UInt32(Buffer[4]) << 16) |
                        (UInt32(Buffer[5]) << 24);
    }
};

//-------------------------------------------------------------------------------------
// ***** LatencyTestDeviceFactory

LatencyTestDeviceFactory LatencyTestDeviceFactory::Instance;

void LatencyTestDeviceFactory::EnumerateDevices(EnumerateVisitor& visitor)
{

    class LatencyTestEnumerator : public HIDEnumerateVisitor
    {
        // Assign not supported; suppress MSVC warning.
        void operator = (const LatencyTestEnumerator&) { }

        DeviceFactory*     pFactory;
        EnumerateVisitor&  ExternalVisitor;   
    public:
        LatencyTestEnumerator(DeviceFactory* factory, EnumerateVisitor& externalVisitor)
            : pFactory(factory), ExternalVisitor(externalVisitor) { }

        virtual bool MatchVendorProduct(UInt16 vendorId, UInt16 productId)
        {
            return ((vendorId == LatencyTester_VendorId) && (productId == LatencyTester_ProductId));                
        }

        virtual void Visit(HANDLE, const HIDDeviceDesc& desc)
        {
            LatencyTestDeviceCreateDesc createDesc(pFactory, desc);
            ExternalVisitor.Visit(createDesc);
        }
    };

    LatencyTestEnumerator latencyTestEnumerator(this, visitor);
    getManager()->HIDInterface.Enumerate(&latencyTestEnumerator);
}


//-------------------------------------------------------------------------------------
// ***** LatencyTestDeviceCreateDesc

DeviceBase* LatencyTestDeviceCreateDesc::NewDeviceInstance()
{
    return new LatencyTestDevice(this);
}

bool LatencyTestDeviceCreateDesc::GetDeviceInfo(DeviceInfo* info) const
{
    if ((info->InfoClassType != Device_LatencyTester) &&
        (info->InfoClassType != Device_None))
        return false;

    OVR_strcpy(info->ProductName,  DeviceInfo::MaxNameLength, HIDDesc.Product.ToCStr());
    OVR_strcpy(info->Manufacturer, DeviceInfo::MaxNameLength, HIDDesc.Manufacturer.ToCStr());
    info->Type    = Device_LatencyTester;
    info->Version = 0;

    if (info->InfoClassType == Device_LatencyTester)
    {
        SensorInfo* sinfo = (SensorInfo*)info;
        sinfo->VendorId  = HIDDesc.VendorId;
        sinfo->ProductId = HIDDesc.ProductId;
        OVR_strcpy(sinfo->SerialNumber, sizeof(sinfo->SerialNumber),HIDDesc.SerialNumber.ToCStr());
    }
    return true;
}

//-------------------------------------------------------------------------------------
// ***** LatencyTestDevice

LatencyTestDevice::LatencyTestDevice(LatencyTestDeviceCreateDesc* createDesc)
    : OVR::DeviceImpl<OVR::LatencyTestDevice>(createDesc, 0),
      hDev(NULL), ReadRequested(false)
{
    memset(&ReadOverlapped, 0, sizeof(OVERLAPPED));
}

LatencyTestDevice::~LatencyTestDevice()
{
    // Check that Shutdown() was called.
    OVR_ASSERT(!pCreateDesc->pDevice);    
}

// Internal creation APIs.
bool LatencyTestDevice::Initialize(DeviceBase* parent)
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

    getManagerImpl()->pThread->AddMessageNotifier(this);

    LogText("OVR::LatencyTestDevice - Opened '%s'\n"
            "                    Manufacturer:'%s'  Product:'%s'  Serial#:'%s'\n",
            hidDesc.Path.ToCStr(),
            hidDesc.Manufacturer.ToCStr(), hidDesc.Product.ToCStr(),
            hidDesc.SerialNumber.ToCStr());
   
    // AddRef() to parent, forcing chain to stay alive.
    pParent = parent;
    return true;
}

void LatencyTestDevice::Shutdown()
{   
    // Remove the handler, if any.
    HandlerRef.SetHandler(0);
    getManagerImpl()->pThread->RemoveMessageNotifier(this);

    closeDevice();
    LogText("OVR::LatencyTestDevice - Closed '%s'\n", getHIDDesc()->Path.ToCStr());

    pParent.Clear();
}

bool LatencyTestDevice::openDevice(const char ** errorFormatString)
{
    HIDDeviceDesc&     hidDesc = *getHIDDesc();
    DeviceManager*     manager = getManagerImpl();
    Win32HIDInterface& hid     = manager->HIDInterface;

    hDev = hid.CreateHIDFile(hidDesc.Path.ToCStr());
    if (hDev == INVALID_HANDLE_VALUE)
    {
        *errorFormatString = "OVR::LatencyTestDevice - Failed to open '%s'\n";
        hDev = 0;
        return false;
    }

    *errorFormatString = "OVR::LatencyTestDevice - Failed to initialize '%s' during open\n";

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


    if (!initializeRead())
    {
        hDev = 0;
        return false;
    }

    *errorFormatString = "";
    return true;
}

void LatencyTestDevice::closeDevice()
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

void LatencyTestDevice::closeDeviceOnIOError()
{
    LogText("OVR::LatencyTesterDevice - Lost connection to '%s'\n", getHIDDesc()->Path.ToCStr());
    closeDevice();
}

bool LatencyTestDevice::initializeRead()
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


bool LatencyTestDevice::processReadResult()
{
    OVR_ASSERT(ReadRequested);

    DWORD bytesRead = 0;

    if (GetOverlappedResult(hDev, &ReadOverlapped, &bytesRead, FALSE))
    {
        // We got data.
        bool processed = false;
        if (!processed)
        {
            LatencyTestSamplesMessage message; 
            if (DecodeLatencyTestSamplesMessage(&message, ReadBuffer, bytesRead))     
            {
                processed = true;
                onLatencyTestSamplesMessage(&message);
            }
        }

        if (!processed)
        {
            LatencyTestColorDetectedMessage message; 
            if (DecodeLatencyTestColorDetectedMessage(&message, ReadBuffer, bytesRead))     
            {
                processed = true;
                onLatencyTestColorDetectedMessage(&message);
            }
        }

        if (!processed)
        {
            LatencyTestStartedMessage message;
            if (DecodeLatencyTestStartedMessage(&message, ReadBuffer, bytesRead))     
            {
                processed = true;
                onLatencyTestStartedMessage(&message);
            }
        }

        if (!processed)
        {
            LatencyTestButtonMessage message; 
            if (DecodeLatencyTestButtonMessage(&message, ReadBuffer, bytesRead))     
            {
                processed = true;
                onLatencyTestButtonMessage(&message);
            }
        }

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

void LatencyTestDevice::OnOverlappedEvent(HANDLE hevent)
{
    OVR_UNUSED(hevent);
    OVR_ASSERT(hevent == ReadOverlapped.hEvent);

    if (processReadResult()) 
	{
        // Proceed to read further.
        initializeRead();
    }
}

bool LatencyTestDevice::OnDeviceMessage(DeviceMessageType messageType, const String& devicePath)
{
    // Is this the correct device?
    LatencyTestDeviceCreateDesc* pCreateDesc = getCreateDesc();
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
            LogError("OVR::LatencyTestDevice - Failed to reopen a device '%s' that was re-added. Error: %s.\n", devicePath.ToCStr(), errorFormatString);
            return true;
        }

        LogText("OVR::LatencyTestDevice - Reopened device '%s'\n", devicePath.ToCStr());
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


    // Do device notification.
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

bool LatencyTestDevice::SetConfiguration(const OVR::LatencyTestConfiguration& configuration, bool waitFlag)
{
    bool                 result = 0;
    ThreadCommandQueue * threadQueue = getManagerImpl()->GetThreadQueue();

    if (!waitFlag)
        return threadQueue->PushCall(this, &LatencyTestDevice::setConfiguration, configuration);

    if (!threadQueue->PushCallAndWaitResult(this, &LatencyTestDevice::setConfiguration,
        &result, configuration))
        return false;

    return result;
}

bool LatencyTestDevice::setConfiguration(const OVR::LatencyTestConfiguration& configuration)
{
    if (!ReadRequested)
        return false;

    LatencyTestConfiguration ltc(configuration);
    DeviceManager*   manager = getManagerImpl();
    if (manager->HIDInterface.HidD_SetFeature(hDev, (void*)ltc.Buffer,
        LatencyTestConfiguration::PacketSize))
    {
        return true;
    }

    return false;
}

bool LatencyTestDevice::SetCalibrate(const OVR::LatencyTestCalibrate& calibrate, bool waitFlag)
{
    bool                 result = 0;
    ThreadCommandQueue * threadQueue = getManagerImpl()->GetThreadQueue();

    if (!waitFlag)
        return threadQueue->PushCall(this, &LatencyTestDevice::setCalibrate, calibrate);

    if (!threadQueue->PushCallAndWaitResult(this, &LatencyTestDevice::setCalibrate,
        &result, calibrate))
        return false;

    return result;
}

bool LatencyTestDevice::setCalibrate(const OVR::LatencyTestCalibrate& calibrate)
{
    if (!ReadRequested)
        return false;

    LatencyTestCalibrate ltc(calibrate);
    DeviceManager*   manager = getManagerImpl();
    if (manager->HIDInterface.HidD_SetFeature(hDev, (void*)ltc.Buffer,
        LatencyTestCalibrate::PacketSize))
    {
        return true;
    }

    return false;
}

bool LatencyTestDevice::SetStartTest(const OVR::LatencyTestStartTest& start, bool waitFlag)
{
    bool                 result = 0;
    ThreadCommandQueue * threadQueue = getManagerImpl()->GetThreadQueue();

    if (!waitFlag)
        return threadQueue->PushCall(this, &LatencyTestDevice::setStartTest, start);

    if (!threadQueue->PushCallAndWaitResult(this, &LatencyTestDevice::setStartTest,
        &result, start))
        return false;

    return result;
}

bool LatencyTestDevice::setStartTest(const OVR::LatencyTestStartTest& start)
{
    if (!ReadRequested)
        return false;

    LatencyTestStartTest ltst(start);
    DeviceManager*   manager = getManagerImpl();
    if (manager->HIDInterface.HidD_SetFeature(hDev, (void*)ltst.Buffer,
        LatencyTestStartTest::PacketSize))
    {
        return true;
    }

    return false;
}

bool LatencyTestDevice::SetDisplay(const OVR::LatencyTestDisplay& display, bool waitFlag)
{
    bool                 result = 0;
    ThreadCommandQueue * threadQueue = getManagerImpl()->GetThreadQueue();

    if (!waitFlag)
        return threadQueue->PushCall(this, &LatencyTestDevice::setDisplay, display);

    if (!threadQueue->PushCallAndWaitResult(this, &LatencyTestDevice::setDisplay,
        &result, display))
        return false;

    return result;
}

bool LatencyTestDevice::setDisplay(const OVR::LatencyTestDisplay& display)
{
    if (!ReadRequested)
        return false;

    LatencyTestDisplay ltd(display);
    DeviceManager*   manager = getManagerImpl();
    if (manager->HIDInterface.HidD_SetFeature(hDev, (void*)ltd.Buffer,
        LatencyTestDisplay::PacketSize))
    {
        return true;
    }

    return false;
}

void LatencyTestDevice::onLatencyTestSamplesMessage(LatencyTestSamplesMessage* message)
{
    if (message->Type != LatencyTestMessage_Samples)
        return;

    LatencyTestSamples& s = message->Samples;

    // Call OnMessage() within a lock to avoid conflicts with handlers.
    Lock::Locker scopeLock(HandlerRef.GetLock());
  
    if (HandlerRef.GetHandler())
    {
        MessageLatencyTestSamples samples(this);
        for (UByte i = 0; i < s.SampleCount; i++)
        {            
            samples.Samples.PushBack(Color(s.Samples[i].Value[0], s.Samples[i].Value[1], s.Samples[i].Value[2]));
        }

        HandlerRef.GetHandler()->OnMessage(samples);
    }
}

void LatencyTestDevice::onLatencyTestColorDetectedMessage(LatencyTestColorDetectedMessage* message)
{
    if (message->Type != LatencyTestMessage_ColorDetected)
        return;

    LatencyTestColorDetected& s = message->ColorDetected;

    // Call OnMessage() within a lock to avoid conflicts with handlers.
    Lock::Locker scopeLock(HandlerRef.GetLock());

    if (HandlerRef.GetHandler())
    {
        MessageLatencyTestColorDetected detected(this);
        detected.Elapsed = s.Elapsed;
        detected.DetectedValue = Color(s.TriggerValue[0], s.TriggerValue[1], s.TriggerValue[2]);
        detected.TargetValue = Color(s.TargetValue[0], s.TargetValue[1], s.TargetValue[2]);

        HandlerRef.GetHandler()->OnMessage(detected);
    }
}

void LatencyTestDevice::onLatencyTestStartedMessage(LatencyTestStartedMessage* message)
{
    if (message->Type != LatencyTestMessage_TestStarted)
        return;

    LatencyTestStarted& ts = message->TestStarted;

    // Call OnMessage() within a lock to avoid conflicts with handlers.
    Lock::Locker scopeLock(HandlerRef.GetLock());

    if (HandlerRef.GetHandler())
    {
        MessageLatencyTestStarted started(this);
        started.TargetValue = Color(ts.TargetValue[0], ts.TargetValue[1], ts.TargetValue[2]);

        HandlerRef.GetHandler()->OnMessage(started);
    }
}

void LatencyTestDevice::onLatencyTestButtonMessage(LatencyTestButtonMessage* message)
{
    if (message->Type != LatencyTestMessage_Button)
        return;

    LatencyTestButton& s = message->Button; s;

    // Call OnMessage() within a lock to avoid conflicts with handlers.
    Lock::Locker scopeLock(HandlerRef.GetLock());

    if (HandlerRef.GetHandler())
    {
        MessageLatencyTestButton button(this);

        HandlerRef.GetHandler()->OnMessage(button);
    }
}

}} // namespace OVR::Win32
