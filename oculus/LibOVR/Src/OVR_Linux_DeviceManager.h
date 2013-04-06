/************************************************************************************

Filename    :   OVR_Linux_DeviceManager.h
Content     :   Linux-specific DeviceManager header.
Created     :   
Authors     :   

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#ifndef OVR_Linux_DeviceManager_h
#define OVR_Linux_DeviceManager_h

#include "OVR_DeviceImpl.h"
//#include "OVR_Linux_HID.h"

#include <unistd.h>
#include <sys/poll.h>

namespace OVR { namespace Linux {

class DeviceManagerThread;

//-------------------------------------------------------------------------------------
// ***** Linux DeviceManager

class DeviceManager : public DeviceManagerImpl
{
public:
    DeviceManager();
    ~DeviceManager();

    // Initialize/Shutdowncreate and shutdown manger thread.
    virtual bool Initialize(DeviceBase* parent);
    virtual void Shutdown();

    virtual ThreadCommandQueue* GetThreadQueue();

    virtual DeviceEnumerator<> EnumerateDevicesEx(const DeviceEnumerationArgs& args);    

    virtual bool  GetDeviceInfo(DeviceInfo* info) const;

    //LinuxHIDInterface        HIDInterface;
    Ptr<DeviceManagerThread> pThread;
};

//-------------------------------------------------------------------------------------
// ***** Device Manager Background Thread

class DeviceManagerThread : public Thread, public ThreadCommandQueue
{
    friend class DeviceManager;
    enum { ThreadStackSize = 32 * 1024 };
public:
    DeviceManagerThread();
    ~DeviceManagerThread();

    virtual int Run();

    // ThreadCommandQueue notifications for CommandEvent handling.
    virtual void OnPushNonEmpty_Locked() { write(CommandFd[1], this, 1); }
    virtual void OnPopEmpty_Locked()     { }

    struct FdNotify
    {
        virtual void OnEvent(int i, int fd) = 0;
    };

    bool AddSelectFd(FdNotify* notify, int fd);
    bool RemoveSelectFd(FdNotify* notify, int fd);

private:
    /*struct PollFdCons : public struct pollfd
    {
        PollFdCons() { events = POLLIN|POLLERR|POLLHUP; revents = 0; }
        };*/
    bool threadInitialized() { return CommandFd[0] != 0; }

    // pipe used to signal commands
    int CommandFd[2];

    Array<struct pollfd> PollFds;
    Array<FdNotify*>     FdNotifiers;
};

}} // namespace Linux::OVR

#endif // OVR_Linux_DeviceManager_h
