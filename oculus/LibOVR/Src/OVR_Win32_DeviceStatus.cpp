/************************************************************************************

Filename    :   OVR_Win32_DeviceStatus.cpp
Content     :   Win32 implementation of DeviceStatus.
Created     :   January 24, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#include "OVR_Win32_DeviceStatus.h"

#include "OVR_Win32_HID.h"

#include "Kernel/OVR_Log.h"

#include <dbt.h>

namespace OVR { namespace Win32 {

static TCHAR windowClassName[] = TEXT("LibOVR_DeviceStatus_WindowClass");

//-------------------------------------------------------------------------------------
DeviceStatus::DeviceStatus(Notifier* const pClient)
	: pNotificationClient(pClient)
{	
}

bool DeviceStatus::Initialize()
{

	WNDCLASS wndClass;
	wndClass.style         = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc   = WindowsMessageCallback;
	wndClass.cbClsExtra    = 0;
	wndClass.cbWndExtra    = 0;
	wndClass.hInstance     = 0;
	wndClass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wndClass.lpszMenuName  = NULL;
	wndClass.lpszClassName = windowClassName;

	if (!RegisterClass(&wndClass))
	{
		OVR_ASSERT_LOG(false, ("Failed to register window class."));
		return false;
	}

    // We're going to create a 'message-only' window. This will be hidden, can't be enumerated etc.
    // To do this we supply 'HWND_MESSAGE' as the hWndParent.
    // http://msdn.microsoft.com/en-us/library/ms632599%28VS.85%29.aspx#message_only
	hMessageWindow = CreateWindow(	windowClassName,
									windowClassName,
									WS_OVERLAPPEDWINDOW,
									CW_USEDEFAULT,
									CW_USEDEFAULT,
									CW_USEDEFAULT,
									CW_USEDEFAULT,
									HWND_MESSAGE,
									NULL,
									0,
									this);	// Pass this object via the CREATESTRUCT mechanism 
                                            // so that we can attach it to the window user data.

    if (hMessageWindow == NULL)
	{
		OVR_ASSERT_LOG(false, ("Failed to create window."));
		return false;
	}

	UpdateWindow(hMessageWindow);


	// Register notification for additional HID messages.
	GUID hidguid;
	Win32HIDInterface HIDInterface;
	HIDInterface.HidD_GetHidGuid(&hidguid);

	DEV_BROADCAST_DEVICEINTERFACE notificationFilter;

	ZeroMemory(&notificationFilter, sizeof(notificationFilter));
	notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	notificationFilter.dbcc_classguid = hidguid;

	hDeviceNotify = RegisterDeviceNotification(	hMessageWindow,
												&notificationFilter,
												DEVICE_NOTIFY_WINDOW_HANDLE);

	if (hDeviceNotify == NULL)
	{
		OVR_ASSERT_LOG(false, ("Failed to register for device notifications."));
		return false;
	}

	return true;
}

void DeviceStatus::ShutDown()
{
	OVR_ASSERT(hMessageWindow);

	if (!UnregisterDeviceNotification(hDeviceNotify))
	{
		OVR_ASSERT_LOG(false, ("Failed to unregister device notification."));
	}

	PostMessage(hMessageWindow, WM_CLOSE, 0, 0);

	while (hMessageWindow != NULL)
	{
		ProcessMessages();
		Sleep(1);
	}

    if (!UnregisterClass(windowClassName, NULL))
    {
        OVR_ASSERT_LOG(false, ("Failed to unregister window class."));
    }
}

DeviceStatus::~DeviceStatus()
{    
	OVR_ASSERT_LOG(hMessageWindow == NULL, ("Need to call 'ShutDown' from DeviceManagerThread."));
}

void DeviceStatus::ProcessMessages()
{
	OVR_ASSERT_LOG(hMessageWindow != NULL, ("Need to call 'Initialize' before first use."));

	MSG msg;

	// Note WM_DEVICECHANGED messages are dispatched but not retrieved by PeekMessage.
    // I think this is because they are pending, non-queued messages.
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void DeviceStatus::MessageCallback(WORD messageType, const String& devicePath)
{
	if (messageType == DBT_DEVICEARRIVAL)
	{
		pNotificationClient->OnMessage(Notifier::DeviceAdded, devicePath);
	}
	else if (messageType == DBT_DEVICEREMOVECOMPLETE)
	{
		pNotificationClient->OnMessage(Notifier::DeviceRemoved, devicePath);
	}
	else
	{
		OVR_ASSERT(0);
	}
}

LRESULT CALLBACK DeviceStatus::WindowsMessageCallback(  HWND hwnd, 
                                                        UINT message, 
                                                        WPARAM wParam, 
                                                        LPARAM lParam)
{

	switch (message)
	{
	case WM_CREATE:
		{
			// Setup window user data with device status object pointer.
			LPCREATESTRUCT create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
			void *lpCreateParam = create_struct->lpCreateParams;
			DeviceStatus *pDeviceStatus = reinterpret_cast<DeviceStatus*>(lpCreateParam);

			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pDeviceStatus));
		}
		return 0;	// Return 0 for successfully handled WM_CREATE.

	case WM_DEVICECHANGE:
		{
			WORD loword = LOWORD(wParam);

			if (loword != DBT_DEVICEARRIVAL &&
				loword != DBT_DEVICEREMOVECOMPLETE) 
			{
				// Ignore messages other than device arrive and remove complete 
                // (we're not handling intermediate ones).
				return TRUE;	// Grant WM_DEVICECHANGE request.
			}

			DEV_BROADCAST_DEVICEINTERFACE* hdr;
			hdr = (DEV_BROADCAST_DEVICEINTERFACE*) lParam;

			if (hdr->dbcc_devicetype != DBT_DEVTYP_DEVICEINTERFACE) 
			{
				// Ignore non interface device messages.
				return TRUE;	// Grant WM_DEVICECHANGE request.
			}

			LONG_PTR userData = GetWindowLongPtr(hwnd, GWLP_USERDATA);
			OVR_ASSERT(userData != NULL);

			// Call callback on device messages object with the device path.
			DeviceStatus* pDeviceStatus = (DeviceStatus*) userData;
			String devicePathStr(hdr->dbcc_name);
			pDeviceStatus->MessageCallback(loword, devicePathStr);
		}
		return TRUE;	// Grant WM_DEVICECHANGE request.

	case WM_CLOSE:
		{
			LONG_PTR userData = GetWindowLongPtr(hwnd, GWLP_USERDATA);
			OVR_ASSERT(userData != NULL);
			DeviceStatus* pDeviceStatus = (DeviceStatus*) userData;
			pDeviceStatus->hMessageWindow = NULL;

			DestroyWindow(hwnd);
		}
		return 0;	// We processed the WM_CLOSE message.

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;	// We processed the WM_DESTROY message.
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

}} // namespace OVR::Win32
