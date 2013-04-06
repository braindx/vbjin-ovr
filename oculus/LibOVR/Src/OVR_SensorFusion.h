/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_SensorFusion.h
Content     :   Implementation of Sensor fusion
Created     :   October 9, 2012
Authors     :   Michael Antonov

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#ifndef OVR_SensorFusion_h
#define OVR_SensorFusion_h

#include "OVR_Device.h"

namespace OVR {


//-------------------------------------------------------------------------------------
// ***** SensorFusion

// SensorFusion class accumulates Sensor notification messages to keep track of
// orientation, which involves integrating the gyro and doing correction with gravity.
// Orientation is reported as a quaternion, from which users can obtain either the
// rotation matrix or Euler angles.
//
// The class can operate in two ways:
//  - By user manually passing MessageBodyFrame messages to the OnMessage() function. 
//  - By attaching SensorFusion to a SensorDevice, in which case it will
//    automatically handle notifications from that device.

class SensorFusion : public NewOverrideBase
{
public:
    SensorFusion(SensorDevice* sensor = 0)
        : Handler(getThis()), pDelegate(0),
          Gain(0.05f), YawMult(1), EnableGravity(true), 
		  EnablePrediction(false), FilterPrediction(false), PredictionDT(0)
    {
        if (sensor)
            AttachToSensor(sensor);

		ResetAngVFilter();
    }
    ~SensorFusion()
    {
    }
    
    // Attaches this SensorFusion to a sensor device, from which it will receive
    // notification messages. If a sensor is attached, manual message notification
    // is not necessary. Calling this function also resets SensorFusion state.
    bool        AttachToSensor(SensorDevice* sensor);

    // Returns true if this Sensor fusion object is attached to a sensor.
    bool        IsAttachedToSensor() const
    { return Handler.IsHandlerInstalled(); }

    void        SetGravityEnabled(bool enableGravity)
    { EnableGravity = enableGravity; }
   
	bool        IsGravityEnabled() const
    { return EnableGravity;}

    // Notifies SensorFusion object about a new BodyFrame message from a sensor.
    // Should be called by user if not attaching to a sensor.
    void        OnMessage(const MessageBodyFrame& msg)
    {
        OVR_ASSERT(!IsAttachedToSensor());
        handleMessage(msg);
    }

    // Obtain the current accumulated orientation.
    Quatf       GetOrientation() const
    {
        Lock::Locker lockScope(Handler.GetHandlerLock());
        return Q;
    }    
    Quatf       GetPredictedOrientation() const
    {
        Lock::Locker lockScope(Handler.GetHandlerLock());
        return QP;
    }    
    // Obtain the last absolute acceleration reading, in m/s^2.
    Vector3f    GetAcceleration() const
    {
        Lock::Locker lockScope(Handler.GetHandlerLock());
        return A;
    }
    
    // Obtain the last angular velocity reading, in rad/s.
    Vector3f    GetAngularVelocity() const
    {
        Lock::Locker lockScope(Handler.GetHandlerLock());
        return AngV;
    }

    // For later
    //Vector3f    GetGravity() const;

    // Resets the current orientation and acceleration.
    void        Reset()
    {
        Lock::Locker lockScope(Handler.GetHandlerLock());
        Q = Quatf();
        QP = Quatf();
        A = Vector3f();

		ResetAngVFilter();
    }

    // Configuration

    // Gain used to correct gyro with accel. Default value is appropriate for typical use.
    float       GetAccelGain() const   { return Gain; }
    void        SetAccelGain(float ag) { Gain = ag; }

    // Multiplier for yaw rotation (turning); setting this higher than 1 (the default) can allow the game
    // to be played without auxillary rotation controls, possibly making it more immersive. Whether this is more
    // or less likely to cause motion sickness is unknown.
    float       GetYawMultiplier() const  { return YawMult; }
    void        SetYawMultiplier(float y) { YawMult = y; }

    void        SetDelegateMessageHandler(MessageHandler* handler)
    { pDelegate = handler; }

	// Prediction functions.
    // Prediction delta specifes how much prediction should be applied in seconds; it should in
    // general be under the average rendering latency. Call GetPredictedOrientation() to get
    // predicted orientation.
    float       GetPredictionDelta() const                  { return PredictionDT; }
    void        SetPrediction(float dt, bool enable = true) { PredictionDT = dt; EnablePrediction = enable; }
	void		SetPredictionFilter(bool enable = true)     {FilterPrediction = enable;}

private:
    SensorFusion* getThis()  { return this; }

    // Internal handler for messages; bypasses error checking.
    void handleMessage(const MessageBodyFrame& msg);

    class BodyFrameHandler : public MessageHandler
    {
        SensorFusion* pFusion;
    public:
        BodyFrameHandler(SensorFusion* fusion) : pFusion(fusion) { }
        ~BodyFrameHandler();

        virtual void OnMessage(const Message& msg);
        virtual bool SupportsMessageType(MessageType type) const;
    };   

    Quatf             Q;
    Vector3f          A;    
    Vector3f          AngV;
    BodyFrameHandler  Handler;
    MessageHandler*   pDelegate;
    float             Gain;
    float             YawMult;
    volatile bool     EnableGravity;

    bool              EnablePrediction;
	bool			  FilterPrediction;
    float             PredictionDT;
    Quatf             QP;

	// Testing AngV filtering suggested by Steve
	Vector3f		  AngVFilterHistory[8];
	void			  ResetAngVFilter();
	void			  GetAngVFilterVal(const Vector3f &in, Vector3f &out);
};


} // namespace OVR

#endif
