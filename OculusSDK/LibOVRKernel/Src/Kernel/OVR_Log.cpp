/************************************************************************************

Filename    :   OVR_Log.cpp
Content     :   Logging support
Created     :   September 19, 2012
Notes       :

Copyright   :   Copyright 2014 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "OVR_Log.h"
#include "OVR_Std.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "OVR_System.h"
#include "OVR_DebugHelp.h"
#include <Util/Util_SystemGUI.h>
#include <Tracing/Tracing.h>

#if defined(OVR_OS_MS) && !defined(OVR_OS_MS_MOBILE)
#include "OVR_Win32_IncludeWindows.h"
#elif defined(OVR_OS_ANDROID)
#include <android/log.h>
#elif defined(OVR_OS_LINUX) || defined(OVR_OS_MAC) || defined(OVR_OS_UNIX)
#include <syslog.h>
#include <errno.h>

typedef int errno_t;
errno_t gmtime_s(struct tm* tm, const time_t* timer)
{
    if (nullptr == gmtime_r(timer, tm)) {
        return errno;
    }

    return 0;
}
#endif

//-----------------------------------------------------------------------------------
// ***** LogSubject

static bool LogSubject_IsReady = false;

class LogSubject : public OVR::SystemSingletonBase<LogSubject>
{
    OVR_DECLARE_SINGLETON(LogSubject);

public:
    void AddListener(OVR::CallbackListener<OVR::Log::LogHandler> *listener)
    {
        OVR::Lock::Locker locker(&SubjectLock);
        Subject.AddListener(listener);
    }

    void Call(const char* message, OVR::LogMessageType type)
    {
        OVR::Lock::Locker locker(&SubjectLock);
        Subject.Call(message, type);
    }

protected:
    virtual void OnThreadDestroy(); // Listen to thread shutdown event

    OVR::CallbackEmitter<OVR::Log::LogHandler> Subject;

    // This lock is needed because AddListener() and Call() can happen on different
    // threads but CallbackEmitter is not thread-safe.
    OVR::Lock SubjectLock;
};

LogSubject::LogSubject()
{
    LogSubject_IsReady = true;

    // Must be at end of function
    PushDestroyCallbacks();
}

LogSubject::~LogSubject()
{
}

void LogSubject::OnThreadDestroy()
{
    LogSubject_IsReady = false;
}

void LogSubject::OnSystemDestroy()
{
    delete this;
}

OVR_DEFINE_SINGLETON(LogSubject);


namespace OVR {


// Global Log pointer.
Log* OVR_GlobalLog = nullptr;
Log::CAPICallback OVR_CAPICallback = nullptr;
uintptr_t OVR_CAPICallbackUserData = 0;


//-----------------------------------------------------------------------------------
// ***** Log Implementation

Log::Log(unsigned logMask, bool defaultLogEnabled) :
    LoggingMask(logMask),
    DefaultLogEnabled(defaultLogEnabled)
{
#ifdef OVR_OS_WIN32
    hEventSource = ::RegisterEventSourceW(nullptr, OVR_SYSLOG_NAME);
    OVR_ASSERT(hEventSource != nullptr);
#endif
}

Log::~Log()
{
#ifdef OVR_OS_WIN32
    if (hEventSource)
    {
        ::DeregisterEventSource(hEventSource);
    }
#endif

    // Clear out global log
    if (this == OVR_GlobalLog)
    {
        // TBD: perhaps we should ASSERT if this happens before system shutdown?
        OVR_GlobalLog = 0;
    }
}

void Log::SetCAPICallback(CAPICallback callback, uintptr_t userData)
{
    if (!OVR::System::IsInitialized())
    {
        OVR_CAPICallback = callback;
        OVR_CAPICallbackUserData = userData;
    }
}

void Log::AddLogObserver(CallbackListener<LogHandler>* listener)
{
    if (OVR::System::IsInitialized() && LogSubject_IsReady)
    {
        LogSubject::GetInstance()->AddListener(listener);
    }
}

static void RouteLogOutput(const char* message, LogMessageType messageType)
{
    int level = int(LogLevel_Debug);
    if (Log::IsDebugMessage(messageType))
    {
        TraceLogDebug(message);
    }
    else if (messageType == OVR::Log_Error)
    {
        level = int(LogLevel_Error);
        TraceLogError(message);
    }
    else
    {
        level = int(LogLevel_Info);
        TraceLogInfo(message);
    }

    if (OVR_CAPICallback)
        OVR_CAPICallback(OVR_CAPICallbackUserData, level, message);

    LogSubject::GetInstance()->Call(message, messageType);
}

void Log::LogMessageVargInt(LogMessageType messageType, const char* fmt, va_list argList)
{
    if (OVR::System::IsInitialized())
    {
        // Invoke subject
        char  buffer[MaxLogBufferMessageSize];
        char* pBuffer = buffer;
        char* pAllocated = NULL;

        #if !defined(OVR_CC_MSVC) // Non-Microsoft compilers require you to save a copy of the va_list.
            va_list argListSaved;
            va_copy(argListSaved, argList);
        #endif

        int result = FormatLog(pBuffer, MaxLogBufferMessageSize, Log_Text, fmt, argList);

        if(result >= MaxLogBufferMessageSize) // If there was insufficient capacity...
        {
            // We assume C++ exceptions are disabled.
            // FormatLog will handle the case that pAllocated is NULL.
            pAllocated = new char [result + 1];
            // We cannot use OVR_ALLOC() for this allocation because the logging subsystem exists
            // outside of the rest of LibOVR so that it can be used to log events from anywhere.
            pBuffer = pAllocated;

            #if !defined(OVR_CC_MSVC)
                va_end(argList); // The caller owns argList and will call va_end on it.
                va_copy(argList, argListSaved);
            #endif

            FormatLog(pBuffer, (size_t)result + 1, Log_Text, fmt, argList);
        }

        RouteLogOutput(pBuffer, messageType);

        delete[] pAllocated;
    }
}

void Log::LogMessageVarg(LogMessageType messageType, const char* fmt, va_list argList)
{
    if (!DefaultLogEnabled || ((messageType & LoggingMask) == 0))
        return;
#ifndef OVR_BUILD_DEBUG
    if (IsDebugMessage(messageType))
        return;
#endif

    char  buffer[MaxLogBufferMessageSize];
    char* pBuffer = buffer;
    char* pAllocated = nullptr;

    #if !defined(OVR_CC_MSVC) // Non-Microsoft compilers require you to save a copy of the va_list.
        va_list argListSaved;
        va_copy(argListSaved, argList);
    #endif

    int result = FormatLog(pBuffer, MaxLogBufferMessageSize, messageType, fmt, argList);

    if(result >= MaxLogBufferMessageSize) // If there was insufficient capacity...
    {
        // We assume C++ exceptions are disabled.
        // FormatLog will handle the case that pAllocated is NULL.
        pAllocated = (char*)malloc(result + 1);
        // We cannot use OVR_ALLOC() for this allocation because the logging subsystem exists
        // outside of the rest of LibOVR so that it can be used to log events from anywhere.
        // We cannot use new/delete either because we wrap that.
        pBuffer = pAllocated;

        #if !defined(OVR_CC_MSVC)
            va_end(argList); // The caller owns argList and will call va_end on it.
            va_copy(argList, argListSaved);
        #endif

        FormatLog(pBuffer, (size_t)result + 1, messageType, fmt, argList);
    }

    DefaultLogOutput(pBuffer, messageType, result);
    free(pAllocated);
}

void OVR::Log::LogMessage(LogMessageType messageType, const char* pfmt, ...)
{
    va_list argList;
    va_start(argList, pfmt);
    LogMessageVarg(messageType, pfmt, argList);
    va_end(argList);
}


// Return behavior is the same as ISO C vsnprintf: returns the required strlen of buffer (which will
// be >= bufferSize if bufferSize is insufficient) or returns a negative value because the input was bad.
int Log::FormatLog(char* buffer, size_t bufferSize, LogMessageType messageType,
                    const char* fmt, va_list argList)
{
    const char bareMinString[] = "01/01/15_08:00:00: Assert: \n";
    OVR_UNUSED(bareMinString);
    OVR_ASSERT(buffer && (bufferSize >= sizeof(bareMinString)));
    if(!buffer || (bufferSize < sizeof(bareMinString)))
        return -1;

    int addNewline = 1;
    int prefixLength = 0;

    // Prepend short timestamp to the message
    time_t timer;
    time(&timer);
    if (timer != -1)
    {
        tm timeData;
        errno_t err = gmtime_s(&timeData, &timer);
        if (err == 0)
        {
            // We're guaranteed to have enough space in the buffer because of
            // the minimum size check at the top of this function (so any
            // bytesWritten > 0 is a success case here).
            int bytesWritten = OVR_snprintf(buffer, bufferSize, "%02i/%02i/%02i %02i:%02i:%02i: ",
                                            timeData.tm_mon+1, timeData.tm_mday,
                                            timeData.tm_year % 100,
                                            timeData.tm_hour, timeData.tm_min, timeData.tm_sec);
            if (bytesWritten > 0)
            {
                prefixLength = bytesWritten;
            }
        }
    }

    switch(messageType)
    {
    case Log_Error:      OVR_strcpy(buffer+prefixLength, bufferSize-prefixLength, "Error: ");  prefixLength += 7; break;
    case Log_Debug:      OVR_strcpy(buffer+prefixLength, bufferSize-prefixLength, "Debug: ");  prefixLength += 7; break;
    case Log_Assert:     OVR_strcpy(buffer+prefixLength, bufferSize-prefixLength, "Assert: "); prefixLength += 8; break;
    case Log_Text:       buffer[prefixLength] = 0; addNewline = 0; break;
    case Log_DebugText:  buffer[prefixLength] = 0; addNewline = 0; break;
    default:             buffer[prefixLength] = 0; addNewline = 0; break;
    }

    char*  buffer2       = buffer + prefixLength;
    size_t size2         = bufferSize - (size_t)prefixLength;
    int    messageLength = OVR_vsnprintf(buffer2, size2, fmt, argList);

    if (addNewline)
    {
        if (messageLength < 0) // If there was a format error...
        {
            // To consider: append <format error> to the buffer here.
            buffer2[0] = '\n'; // We are guaranteed to have capacity for this.
            buffer2[1] = '\0';
        }
        else
        {
            size_t bufferUsed = messageLength;
            if (bufferUsed > size2)
                bufferUsed = size2;

            if ((bufferUsed == 0) || (buffer2[bufferUsed - 1] != '\n')) // If there isn't already a trailing '\n' ...
            {
                // If the printed string used all of the capacity or required more than the capacity,
                // Chop the output by one character so we can append the \n safely.
                int messageEnd = (messageLength >= (int)(size2 - 1)) ? (int)(size2 - 2) : messageLength;
                buffer2[messageEnd + 0] = '\n';
                buffer2[messageEnd + 1] = '\0';
            }
        }
    }

    if (messageLength >= 0) // If the format was OK...
        return prefixLength + messageLength + addNewline; // Return the required strlen of buffer.

    return messageLength; // Else we cannot know what the required strlen is and return the error to the caller.
}

void Log::DefaultLogOutput(const char* formattedText, LogMessageType messageType, int bufferSize)
{
    std::wstring wideText = UTF8StringToUCSString(formattedText, bufferSize);
    const wchar_t* wideBuff = wideText.c_str();
    OVR_UNUSED(wideBuff);

#if defined(OVR_OS_WIN32)

    auto endOfDatestamp = wideText.find(L": ", 11);
    ::OutputDebugStringW(wideBuff + (endOfDatestamp != std::wstring::npos ? (endOfDatestamp + 2) : 0));

    fputs(formattedText, stdout);

#elif defined(OVR_OS_MS) // Any other Microsoft OSs

    ::OutputDebugStringW(wideBuff);

#elif defined(OVR_OS_ANDROID)
    // To do: use bufferSize to deal with the case that Android has a limited output length.
    __android_log_write(ANDROID_LOG_INFO, "OVR", formattedText);

#elif defined(OVR_OS_MAC)
    syslog(LOG_INFO, "%s", formattedText);
    fputs(formattedText, stdout);
#else
    fputs(formattedText, stdout);
#endif

    if (messageType == Log_Error)
    {
#if defined(OVR_OS_WIN32)
        if (!ReportEventW(hEventSource, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 1, 0, &wideBuff, NULL))
        {
            OVR_ASSERT(false);
        }
#elif defined(OVR_OS_MS) // Any other Microsoft OSs
        // TBD
#elif defined(OVR_OS_ANDROID)
        // TBD
#elif defined(OVR_OS_MAC) || defined(OVR_OS_LINUX)
        syslog(LOG_ERR, "%s", formattedText);
#else
        // TBD
#endif
    }
}

//static
void Log::SetGlobalLog(Log *log)
{
    OVR_GlobalLog = log;
}
//static
Log* Log::GetGlobalLog()
{
// No global log by default?
//    if (!OVR_GlobalLog)
//        OVR_GlobalLog = GetDefaultLog();
    return OVR_GlobalLog;
}

//static
Log* Log::GetDefaultLog()
{
    // Create default log pointer statically so that it can be used
    // even during startup.
    static Log defaultLog;
    return &defaultLog;
}


//-----------------------------------------------------------------------------------
// ***** Global Logging functions

#if !defined(OVR_CC_MSVC)
// The reason for va_copy is because you can't use va_start twice on Linux
#define OVR_LOG_FUNCTION_IMPL(Name)  \
    void Log##Name(const char* fmt, ...) \
    {                                                                    \
        if (OVR_GlobalLog)                                               \
        {                                                                \
            va_list argList1;                                             \
            va_start(argList1, fmt);                                     \
            va_list argList2;                                             \
            va_copy(argList2, argList1);                                 \
            OVR_GlobalLog->LogMessageVargInt(Log_##Name, fmt, argList2); \
            va_end(argList2);                                             \
            OVR_GlobalLog->LogMessageVarg(Log_##Name, fmt, argList1);    \
            va_end(argList1);                                            \
        }                                                                \
    }
#else
#define OVR_LOG_FUNCTION_IMPL(Name)  \
    void Log##Name(const char* fmt, ...) \
    {                                                                    \
        if (OVR_GlobalLog)                                               \
        {                                                                \
            va_list argList1;                                             \
            va_start(argList1, fmt);                                     \
            OVR_GlobalLog->LogMessageVargInt(Log_##Name, fmt, argList1); \
            OVR_GlobalLog->LogMessageVarg(Log_##Name, fmt, argList1);    \
            va_end(argList1);                                            \
        }                                                                \
    }
#endif // #if !defined(OVR_OS_WIN32)

OVR_LOG_FUNCTION_IMPL(Text)
OVR_LOG_FUNCTION_IMPL(Error)

#ifdef OVR_BUILD_DEBUG
OVR_LOG_FUNCTION_IMPL(DebugText)
OVR_LOG_FUNCTION_IMPL(Debug)
OVR_LOG_FUNCTION_IMPL(Assert)
#endif



void OVR_Fail_F(const char* format, ...)
{
    char message[512];
    va_list argList;
    va_start(argList, format);
    OVR_vsnprintf(message, sizeof(message), format, argList);
    va_end(argList);
    OVR_FAIL_M(message);
}


// Assertion handler support
// To consider: Move this to an OVR_Types.cpp or OVR_Assert.cpp source file.

static OVRAssertionHandler sOVRAssertionHandler = OVR::DefaultAssertionHandler;
static intptr_t sOVRAssertionHandlerUserParameter = 0;

OVRAssertionHandler GetAssertionHandler(intptr_t* userParameter)
{
    if(userParameter)
        *userParameter = sOVRAssertionHandlerUserParameter;
    return sOVRAssertionHandler;
}

void SetAssertionHandler(OVRAssertionHandler assertionHandler, intptr_t userParameter)
{
    sOVRAssertionHandler = assertionHandler;
    sOVRAssertionHandlerUserParameter = userParameter;
}

intptr_t DefaultAssertionHandler(intptr_t /*userParameter*/, const char* title, const char* message)
{
    if(OVRIsDebuggerPresent())
    {
        OVR_DEBUG_BREAK;
    }
    else
    {
        OVR_UNUSED(title);
        OVR_UNUSED(message);

        #if defined(OVR_BUILD_DEBUG)
            if(Allocator::GetInstance()) // The code below currently depends on having a valid Allocator.
            {
                // Print a stack trace of all threads.
                OVR::String s;
                OVR::String threadListOutput;
                static OVR::SymbolLookup symbolLookup;

                s = "Failure: ";
                s += message;

                if(symbolLookup.Initialize() && symbolLookup.ReportThreadCallstack(threadListOutput, 4)) // This '4' is there to skip our internal handling and retrieve starting at the assertion location (our caller) only.
                {
                    // threadListOutput has newlines that are merely \n, whereas Windows MessageBox wants \r\n newlines. So we insert \r in front of all \n.
                    for(size_t i = 0, iEnd = threadListOutput.GetSize(); i < iEnd; i++)
                    {
                        if(threadListOutput[i] == '\n')
                        {
                            threadListOutput.Insert("\r", i, 1);
                            ++i;
                            ++iEnd;
                        }
                    }

                    s += "\r\n\r\n";
                    s += threadListOutput;
                }

                OVR::Util::DisplayMessageBox(title, s.ToCStr());
            }
            else
            {
                // See above.
                OVR::Util::DisplayMessageBox(title, message);
            }
        #else
            OVR::Util::DisplayMessageBox(title, message);
        #endif
    }

    return 0;
}

bool IsAutomationRunning()
{
    #if defined(OVR_OS_WIN32)
        // We use the OS GetEnvironmentVariable function as opposed to getenv, as that
        // is process-wide as opposed to being tied to the current C runtime library.
        return GetEnvironmentVariableW(L"OvrAutomationRunning", NULL, 0) > 0;
    #else
        return getenv("OvrAutomationRunning") != nullptr;
    #endif
}

} // OVR
