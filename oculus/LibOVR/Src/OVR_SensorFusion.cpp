/************************************************************************************

Filename    :   OVR_SensorFusion.cpp
Content     :   Sensor fusion implementation
Created     :   October 9, 2012
Authors     :   Michael Antonov

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#include "OVR_SensorFusion.h"
#include "Kernel/OVR_Log.h"
#include "Kernel/OVR_System.h"

namespace OVR {

//-------------------------------------------------------------------------------------
// ***** Sensor Fusion


bool SensorFusion::AttachToSensor(SensorDevice* sensor)
{
    
    if (sensor != NULL)
    {
        MessageHandler* pCurrentHandler = sensor->GetMessageHandler();

        if (pCurrentHandler == &Handler)
        {
            Reset();
            return true;
        }

        if (pCurrentHandler != NULL)
        {
            OVR_DEBUG_LOG(
                ("SensorFusion::AttachToSensor failed - sensor %p already has handler", sensor));
            return false;
        }
    }

    if (Handler.IsHandlerInstalled())
    {
        Handler.RemoveHandlerFromDevices();
    }

    if (sensor != NULL)
    {
        sensor->SetMessageHandler(&Handler);
    }

    Reset();
    return true;
}

void SensorFusion::handleMessage(const MessageBodyFrame& msg)
{
    if (msg.Type != Message_BodyFrame)
        return;

    AngV = msg.RotationRate;
    AngV.y *= YawMult;
    A = msg.Acceleration * msg.TimeDelta;

    /*
    // Mike's original integration approach. Subdivision to reduce error.
    Quatf q = AngVToYawPitchRollQuatf(msg.AngV * msg.TimeDelta * (1.0f / 16.0f));
    Quatf q2 = q * q;
    Quatf q4 = q2 * q2;
    Quatf q8 = q4 * q4;
    Q = q8 * q8 * Q;
    */

    // Integration based on exact movement on 4D unit quaternion sphere.
    // Developed by Steve & Anna, this technique is based on Lie algebra
    // and exponential map.
    Vector3f    dV    = AngV * msg.TimeDelta;
    const float angle = dV.Length();  // Magnitude of angular velocity.

    if (angle > 0.0f)
    {
        float halfa = angle * 0.5f;
        float sina  = sin(halfa) / angle;
        Quatf dQ(dV.x*sina, dV.y*sina, dV.z*sina, cos(halfa));
        Q =  Q * dQ;

        if (EnablePrediction)
        {
			Vector3f AngVF;
			GetAngVFilterVal(AngV, AngVF);
            float angSpeed = AngVF.Length();
            if (angSpeed > 0.001f)
            {
                Vector3f    axis = AngVF / angSpeed;
                float       halfaP = angSpeed * (msg.TimeDelta + PredictionDT) * 0.5f;
                Quatf       dQP(0, 0, 0, 1);
                float       sinaP  = sin(halfaP);  
                dQP = Quatf(axis.x*sinaP, axis.y*sinaP, axis.z*sinaP, cos(halfaP));
                QP =  Q * dQP;
            }
            else
            {
                QP = Q;
            }
        }
        else
        {
            QP = Q;
        }
    }    
    

    // This introduces gravity drift adjustment based on gain
    float        accelMagnitude = msg.Acceleration.Length();
    float        angVMagnitude  = AngV.Length();   
    const float  gravityEpsilon = 0.4f;
    const float  angVEpsilon    = 3.0f; // Relatively slow rotation
    
    if (EnableGravity &&
        (fabs(accelMagnitude - 9.81f) < gravityEpsilon) &&
        (angVMagnitude < angVEpsilon))
    {
        // TBD: Additional conditions:
        //  - Angular velocity < epsilon, or
        //  - Angle of transformed Acceleration < epsilon

        Vector3f yUp(0,1,0);
        Vector3f aw = Q.Rotate(A);
        Quatf    qfeedback(-aw.z * Gain, 0, aw.x * Gain, 1);

        Quatf    q1 = (qfeedback * Q).Normalized();
        float    angle0 = yUp.Angle(aw);
        float    angle1 = yUp.Angle(q1.Rotate(A));

        if (angle1 < angle0)
        {
            Q = q1;
        }
        else
        {
            Quatf    qfeedback2(aw.z * Gain, 0, -aw.x * Gain, 1);
            Quatf    q2 = (qfeedback2 * Q).Normalized();
            float    angle2 = yUp.Angle(q2.Rotate(A));

            if (angle2 < angle0)
            {
                Q = q2;
            }
        }
    }    
}


SensorFusion::BodyFrameHandler::~BodyFrameHandler()
{
    RemoveHandlerFromDevices();
}

void SensorFusion::BodyFrameHandler::OnMessage(const Message& msg)
{
    if (msg.Type == Message_BodyFrame)
        pFusion->handleMessage(static_cast<const MessageBodyFrame&>(msg));
    if (pFusion->pDelegate)
        pFusion->pDelegate->OnMessage(msg);
}
bool SensorFusion::BodyFrameHandler::SupportsMessageType(MessageType type) const
{
    return (type == Message_BodyFrame);
}

void SensorFusion::ResetAngVFilter()
{
	for (int i = 0; i < 8; i++)
		AngVFilterHistory[i] = Vector3f(0,0,0);
}

void SensorFusion::GetAngVFilterVal(const Vector3f &in, Vector3f &out)
{
	if(FilterPrediction == false)
	{
		out = in;
		return;
	}

	// rotate history and add latest value
	for (int i = 6; i >= 0 ; i--)
		AngVFilterHistory[i+1] = AngVFilterHistory[i];	
	
	AngVFilterHistory[0] = in;

	// Filtered value (Savitsky-Golay derivative)
	out = 
	(AngVFilterHistory[0] *  0.41667f) +
	(AngVFilterHistory[1] *  0.33333f) +
	(AngVFilterHistory[2] *  0.025f) +
	(AngVFilterHistory[3] *  0.16667f) +
	(AngVFilterHistory[4] *  0.08333f) +
	(AngVFilterHistory[5] *  0.0f) +
	(AngVFilterHistory[6] * -0.08333f) +
	(AngVFilterHistory[7] * -0.16667f);
}

} // namespace OVR

