/************************************************************************************

PublicHeader:   OVR
Filename    :   OVR_Win32_LatencyTest.h
Content     :   Sensor device header interfacing to Oculus Latency Tester through
                raw system I/O.
Created     :   February 6, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#ifndef OVR_Win32_LatencyTest_h
#define OVR_Win32_LatencyTest_h

#include "OVR_Win32_DeviceManager.h"

namespace OVR { namespace Win32 { 

struct LatencyTestSamplesMessage;
struct LatencyTestButtonMessage;
struct LatencyTestStartedMessage;
struct LatencyTestColorDetectedMessage;

//-------------------------------------------------------------------------------------
// LatencyTestDeviceFactory enumerates Oculus Latency Tester devices.
class LatencyTestDeviceFactory : public DeviceFactory
{
public:
    static LatencyTestDeviceFactory Instance;

    // Enumerates devices, creating and destroying relevant objects in manager.
    virtual void EnumerateDevices(EnumerateVisitor& visitor);

protected:
    DeviceManager* getManager() const { return (DeviceManager*) pManager; }   
};


// Describes a single a Oculus Latency Tester device and supports creating its instance.
class LatencyTestDeviceCreateDesc : public DeviceCreateDesc
{
public:
    LatencyTestDeviceCreateDesc(DeviceFactory* factory, const HIDDeviceDesc& hidDesc)
        : DeviceCreateDesc(factory, Device_LatencyTester), HIDDesc(hidDesc) { }
    LatencyTestDeviceCreateDesc(const LatencyTestDeviceCreateDesc& other)
        : DeviceCreateDesc(other.pFactory, Device_LatencyTester), HIDDesc(other.HIDDesc) { }

    HIDDeviceDesc HIDDesc;
    
    virtual DeviceCreateDesc* Clone() const
    {
        return new LatencyTestDeviceCreateDesc(*this);
    }

    virtual DeviceBase* NewDeviceInstance();

    virtual MatchResult MatchDevice(const DeviceCreateDesc& other,
                                    DeviceCreateDesc**) const
    {
        if ((other.Type == Device_LatencyTester) && (pFactory == other.pFactory))
        {            
            const LatencyTestDeviceCreateDesc& s2 = (const LatencyTestDeviceCreateDesc&) other;
            if ((HIDDesc.Path == s2.HIDDesc.Path) &&
                (HIDDesc.SerialNumber == s2.HIDDesc.SerialNumber))
                return Match_Found;
        }
        return Match_None;
    }

    virtual bool        GetDeviceInfo(DeviceInfo* info) const;
};


//-------------------------------------------------------------------------------------
// ***** OVR::Win32::LatencyTestDevice

// Oculus Latency Tester interface under Win32.

class LatencyTestDevice : public DeviceImpl<OVR::LatencyTestDevice>,
                            public DeviceManagerThread::Notifier
{
public:
     LatencyTestDevice(LatencyTestDeviceCreateDesc* createDesc);
    ~LatencyTestDevice();


    // DeviceCommon interface
    virtual bool Initialize(DeviceBase* parent);
    virtual void Shutdown();

    // DeviceManager::OverlappedNotify
    virtual void OnOverlappedEvent(HANDLE hevent);

    virtual bool OnDeviceMessage(DeviceMessageType messageType, const String& devicePath);

    // LatencyTesterDevice interface
    virtual bool SetConfiguration(const OVR::LatencyTestConfiguration& configuration, bool waitFlag);
    virtual bool SetCalibrate(const OVR::LatencyTestCalibrate& calibrate, bool waitFlag);
    virtual bool SetStartTest(const OVR::LatencyTestStartTest& start, bool waitFlag);
    virtual bool SetDisplay(const OVR::LatencyTestDisplay& display, bool waitFlag);

protected:
    bool    openDevice(const char** errorFormatString);
    void    closeDevice();
    void    closeDeviceOnIOError();

    bool    initializeRead();
    bool    processReadResult();

    struct WriteData
    {
        enum { BufferSize = 64 };
        UByte Buffer[64];
        UPInt Size;

        WriteData(UByte* data, UPInt size) : Size(size)
        {
            OVR_ASSERT(size <= BufferSize);
            memcpy(Buffer, data, size);
        }
    };

	bool    setConfiguration(const LatencyTestConfiguration& configuration);
    bool    setCalibrate(const LatencyTestCalibrate& calibrate);
    bool    setStartTest(const OVR::LatencyTestStartTest& start);
    bool    setDisplay(const OVR::LatencyTestDisplay& display);


    // Called for decoded messages
    void onLatencyTestSamplesMessage(LatencyTestSamplesMessage* message);
    void onLatencyTestButtonMessage(LatencyTestButtonMessage* message);
    void onLatencyTestStartedMessage(LatencyTestStartedMessage* message);
    void onLatencyTestColorDetectedMessage(LatencyTestColorDetectedMessage* message);

    // Helpers to reduce casting.
    LatencyTestDeviceCreateDesc* getCreateDesc() const
    { return (LatencyTestDeviceCreateDesc*)pCreateDesc.GetPtr(); }

    HIDDeviceDesc* getHIDDesc() const
    { return &getCreateDesc()->HIDDesc; }
    
    Win32::DeviceManager* getManagerImpl() const
    { return (DeviceManager*)DeviceImpl<OVR::LatencyTestDevice>::GetManager(); }

    enum { ReadBufferSize = 96 };

    // Handle to open device, or null.
    HANDLE      hDev;    

    // OVERLAPPED data structure servicing messages to be sent.
    OVERLAPPED  ReadOverlapped;
    bool        ReadRequested;
    UByte       ReadBuffer[ReadBufferSize];
};

}} // namespace OVR::Win32

#endif // OVR_Win32_LatencyTest_h
