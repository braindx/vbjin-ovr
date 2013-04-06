/************************************************************************************

Filename    :   OVR_Win32_Sensor.h
Content     :   Sensor device header interfacing to Oculus sensor through
                raw system I/O.
Created     :   September 21, 2012
Authors     :   Michael Antonov

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#ifndef OVR_Win32_Sensor_h
#define OVR_Win32_Sensor_h

#include "OVR_Win32_DeviceManager.h"

namespace OVR { namespace Win32 { 

struct TrackerMessage;

//-------------------------------------------------------------------------------------
// SensorDeviceFactory enumerates Oculus Sensor devices.
class SensorDeviceFactory : public DeviceFactory
{
public:
    static SensorDeviceFactory Instance;

    // Enumerates devices, creating and destroying relevant objects in manager.
    virtual void EnumerateDevices(EnumerateVisitor& visitor);

protected:
    DeviceManager* getManager() const { return (DeviceManager*) pManager; }   
};


// Describes a single a Oculus Sensor device and supports creating its instance.
class SensorDeviceCreateDesc : public DeviceCreateDesc
{
public:
    SensorDeviceCreateDesc(DeviceFactory* factory, const HIDDeviceDesc& hidDesc)
        : DeviceCreateDesc(factory, Device_Sensor), HIDDesc(hidDesc) { }
    SensorDeviceCreateDesc(const SensorDeviceCreateDesc& other)
        : DeviceCreateDesc(other.pFactory, Device_Sensor), HIDDesc(other.HIDDesc) { }

    HIDDeviceDesc HIDDesc;
    
    virtual DeviceCreateDesc* Clone() const
    {
        return new SensorDeviceCreateDesc(*this);
    }

    virtual DeviceBase* NewDeviceInstance();

    virtual MatchResult MatchDevice(const DeviceCreateDesc& other,
                                    DeviceCreateDesc**) const
    {
        if ((other.Type == Device_Sensor) && (pFactory == other.pFactory))
        {
            const SensorDeviceCreateDesc& s2 = (const SensorDeviceCreateDesc&) other;
            if ((HIDDesc.Path == s2.HIDDesc.Path) &&
                (HIDDesc.SerialNumber == s2.HIDDesc.SerialNumber))
                return Match_Found;
        }
        return Match_None;
    }

    virtual bool        GetDeviceInfo(DeviceInfo* info) const;
};


//-------------------------------------------------------------------------------------
// ***** OVR::Win32::SensorDevice

// Oculus Sensor interface under Win32.

class SensorDevice : public DeviceImpl<OVR::SensorDevice>,
                     public DeviceManagerThread::Notifier
{
public:
     SensorDevice(SensorDeviceCreateDesc* createDesc);
    ~SensorDevice();


    // DeviceCommaon interface
    virtual bool Initialize(DeviceBase* parent);
    virtual void Shutdown();

    virtual void SetMessageHandler(MessageHandler* handler);

    // DeviceManager::OverlappedNotify
    virtual void OnOverlappedEvent(HANDLE hevent);

    virtual UInt64 OnTicks(UInt64 ticksMks);

	virtual bool OnDeviceMessage(DeviceMessageType messageType, const String& devicePath);

    // HMD-Mounted sensor has a different coordinate frame.
    virtual void SetCoordinateFrame(CoordinateFrame coordframe);    
    virtual CoordinateFrame GetCoordinateFrame() const;    

    // SensorDevice interface
    virtual bool SetRange(const SensorRange& range, bool waitFlag);
    virtual void GetRange(SensorRange* range) const;

    virtual bool  SetFeature(UByte* data, UPInt size, bool waitFlag);
    virtual bool  GetFeature(UByte* data, UPInt size);
    //virtual UPInt WriteCommand(UByte* data, UPInt size, bool waitFlag);


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

    Void    setCoordinateFrame(CoordinateFrame coordframe);
    bool    setRange(const SensorRange& range);
    bool    setFeature(const WriteData& data);
    bool    getFeature(UByte* data, UPInt size);

    //UPInt writeCommand(const WriteData& data);    

    // Called for decoded messages
    void        onTrackerMessage(TrackerMessage* message);

    // Helpers to reduce casting.
    SensorDeviceCreateDesc* getCreateDesc() const
    { return (SensorDeviceCreateDesc*)pCreateDesc.GetPtr(); }

    HIDDeviceDesc* getHIDDesc() const
    { return &getCreateDesc()->HIDDesc; }
    
    Win32::DeviceManager* getManagerImpl() const
    { return (DeviceManager*)DeviceImpl<OVR::SensorDevice>::GetManager(); }


    enum { ReadBufferSize = 96 };

    // Set if the sensor is located on the HMD.
    // Older prototype firmware doesn't support changing HW coordinates,
    // so we track its state.
    CoordinateFrame Coordinates;
    CoordinateFrame HWCoordinates;
    UInt64      NextKeepAliveTicks;

    bool        SequenceValid;
    SInt16      LastTimestamp;
    UByte       LastSampleCount;
    float       LastTemperature;
    Vector3f    LastAcceleration;
    Vector3f    LastRotationRate;
    Vector3f    LastMagneticField;

    // Current sensor range obtained from device. 
    SensorRange MaxValidRange;
    SensorRange CurrentRange;
    
    // Handle to open device, or null.
    HANDLE      hDev;    

    UInt16      OldCommandId;

    // OVERLAPPED data structure servicing messages to be sent.
    OVERLAPPED  ReadOverlapped;
    bool        ReadRequested;
    UByte       ReadBuffer[ReadBufferSize];
};


}} // namespace OVR::Win32

#endif // OVR_Win32_Sensor_h
