/************************************************************************************

Filename    :   Util_SystemInfo.cpp
Content     :   Various operations to get information about the system
Created     :   September 26, 2014
Author      :   Kevin Jenkins

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

#include "Util_SystemInfo.h"
#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_Threads.h"
#include "Kernel/OVR_Log.h"
#include "Kernel/OVR_Array.h"

#if defined(OVR_OS_LINUX)
#include <sys/utsname.h>
#endif

// Includes used for GetBaseOVRPath()
#ifdef OVR_OS_WIN32
    #include "Kernel/OVR_Win32_IncludeWindows.h"
    #include <Shlobj.h>
    #include <Shlwapi.h>

    #pragma comment(lib, "Shlwapi")
#elif defined(OVR_OS_MS) // Other Microsoft OSs
    // Nothing, thanks.
#else
    #include <dirent.h>
    #include <sys/stat.h>

    #ifdef OVR_OS_LINUX
        #include <unistd.h>
        #include <pwd.h>
    #endif
#endif


namespace OVR { namespace Util {

// From http://blogs.msdn.com/b/oldnewthing/archive/2005/02/01/364563.aspx
#if defined (OVR_OS_WIN64) || defined (OVR_OS_WIN32)

#pragma comment(lib, "version.lib")

typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
bool Is64BitWindows()
{
#if defined(_WIN64)
    return TRUE;  // 64-bit programs run only on Win64
#elif defined(_WIN32)
    // 32-bit programs run on both 32-bit and 64-bit Windows
    // so must sniff
    BOOL f64 = FALSE;
    LPFN_ISWOW64PROCESS fnIsWow64Process;

    fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandleW(L"kernel32"), "IsWow64Process");
    if (NULL != fnIsWow64Process)
    {
        return fnIsWow64Process(GetCurrentProcess(), &f64) && f64;
    }
    return FALSE;
#else
    return FALSE; // Win64 does not support Win16
#endif
}
#endif

const char * OSAsString()
{
#if defined (OVR_OS_IPHONE)
    return "IPhone";
#elif defined (OVR_OS_DARWIN)
    return "Darwin";
#elif defined (OVR_OS_MAC)
    return "Mac";
#elif defined (OVR_OS_BSD)
    return "BSD";
#elif defined (OVR_OS_WIN64) || defined (OVR_OS_WIN32)
    if (Is64BitWindows())
        return "Win64";
    else
        return "Win32";
#elif defined (OVR_OS_ANDROID)
    return "Android";
#elif defined (OVR_OS_LINUX)
    return "Linux";
#elif defined (OVR_OS_BSD)
    return "BSD";
#else
    return "Other";
#endif
}

uint64_t GetGuidInt()
{
    uint64_t g = Timer::GetTicksNanos();

    uint64_t lastTime, thisTime;
    int j;
    // Sleep a small random time, then use the last 4 bits as a source of randomness
    for (j = 0; j < 8; j++)
    {
        lastTime = Timer::GetTicksNanos();
        Thread::MSleep(1);
        // Note this does not actually sleep for "only" 1 millisecond
        // necessarily.  Since we do not call timeBeginPeriod(1) explicitly
        // before invoking this function it may be sleeping for 10+ milliseconds.
        thisTime = Timer::GetTicksNanos();
        uint64_t diff = thisTime - lastTime;
        unsigned int diff4Bits = (unsigned int)(diff & 15);
        diff4Bits <<= 32 - 4;
        diff4Bits >>= j * 4;
        ((char*)&g)[j] ^= diff4Bits;
    }

    return g;
}

String GetGuidString()
{
    uint64_t guid = GetGuidInt();

    char buff[64];
#if defined(OVR_CC_MSVC)
    OVR_sprintf(buff, sizeof(buff), "%I64u", guid);
#else
    OVR_sprintf(buff, sizeof(buff), "%llu", (unsigned long long) guid);
#endif
    return String(buff);
}

const char * GetProcessInfo()
{
#if defined (OVR_CPU_X86_64	)
    return "64 bit";
#elif defined (OVR_CPU_X86)
    return "32 bit";
#else
    return "TODO";
#endif
}
#ifdef OVR_OS_WIN32

String OSVersionAsString()
{
    return GetSystemFileVersionStringW(L"\\kernel32.dll");
}
String GetSystemFileVersionStringW(wchar_t filePath[MAX_PATH])
{
    wchar_t strFilePath[MAX_PATH]; // Local variable
    UINT sysDirLen = GetSystemDirectoryW(strFilePath, ARRAYSIZE(strFilePath));
    if (sysDirLen != 0)
    {
        OVR_wcscat(strFilePath, MAX_PATH, filePath);
        return GetFileVersionStringW(strFilePath);
    }
    else
    {
        return "GetSystemDirectoryW failed";
    }
}

// See http://stackoverflow.com/questions/940707/how-do-i-programatically-get-the-version-of-a-dll-or-exe-file
String GetFileVersionStringW(wchar_t filePath[MAX_PATH])
{
    String result;

    DWORD dwSize = GetFileVersionInfoSizeW(filePath, NULL);
    if (dwSize == 0)
    {
        OVR_DEBUG_LOG(("Error in GetFileVersionInfoSizeW: %d (for %s)", GetLastError(), String(filePath).ToCStr()));
        result = String(filePath) + " not found";
    }
    else
    {
        BYTE* pVersionInfo = new BYTE[dwSize];
        if (!pVersionInfo)
        {
            OVR_DEBUG_LOG(("Out of memory allocating %d bytes (for %s)", dwSize, filePath));
            result = "Out of memory";
        }
        else
        {
            if (!GetFileVersionInfoW(filePath, 0, dwSize, pVersionInfo))
            {
                OVR_DEBUG_LOG(("Error in GetFileVersionInfo: %d (for %s)", GetLastError(), String(filePath).ToCStr()));
                result = "Cannot get version info";
            }
            else
            {
                VS_FIXEDFILEINFO* pFileInfo = NULL;
                UINT              pLenFileInfo = 0;
                if (!VerQueryValueW(pVersionInfo, L"\\", (LPVOID*)&pFileInfo, &pLenFileInfo))
                {
                    OVR_DEBUG_LOG(("Error in VerQueryValueW: %d (for %s)", GetLastError(), String(filePath).ToCStr()));
                    result = "File has no version info";
                }
                else
                {
                    int major = (pFileInfo->dwFileVersionMS >> 16) & 0xffff;
                    int minor = (pFileInfo->dwFileVersionMS) & 0xffff;
                    int hotfix = (pFileInfo->dwFileVersionLS >> 16) & 0xffff;
                    int other = (pFileInfo->dwFileVersionLS) & 0xffff;

                    char str[128];
                    OVR::OVR_sprintf(str, 128, "%d.%d.%d.%d", major, minor, hotfix, other);

                    result = str;
                }
            }

            delete[] pVersionInfo;
        }
    }

    return result;
}

String GetDisplayDriverVersion()
{
    if (Is64BitWindows())
    {
        return GetSystemFileVersionStringW(L"\\OVRDisplay64.dll");
    }
    else
    {
        return GetSystemFileVersionStringW(L"\\OVRDisplay32.dll");
    }
}

String GetCameraDriverVersion()
{
    return GetSystemFileVersionStringW(L"\\drivers\\OCUSBVID.sys");
}

// From http://stackoverflow.com/questions/9524309/enumdisplaydevices-function-not-working-for-me
void GetGraphicsCardList( Array< String > &gpus)
{
    gpus.Clear();
    DISPLAY_DEVICEW dd;
    dd.cb = sizeof(dd);

    DWORD deviceNum = 0;
    while( EnumDisplayDevicesW(NULL, deviceNum, &dd, 0) )
    {
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
            gpus.PushBack(String(dd.DeviceString));
        deviceNum++;
    }
}

String GetProcessorInfo()
{
    char brand[0x40] = {};
    int cpui[4] = { -1 };

    __cpuidex(cpui, 0x80000002, 0);

    //unsigned int blocks = cpui[0];
    for (int i = 0; i <= 2; ++i)
    {
        __cpuidex(cpui, 0x80000002 + i, 0);
        *reinterpret_cast<int*>(brand + i * 16) = cpui[0];
        *reinterpret_cast<int*>(brand + 4 + i * 16) = cpui[1];
        *reinterpret_cast<int*>(brand + 8 + i * 16) = cpui[2];
        *reinterpret_cast<int*>(brand + 12 + i * 16) = cpui[3];

    }
    return String(brand, 0x40);
}

#else

#ifdef OVR_OS_MAC
//use objective c source

// used for driver files
String GetFileVersionString(String /*filePath*/)
{
    return String();
}

String GetSystemFileVersionString(String /*filePath*/)
{
    return String();
}

String GetDisplayDriverVersion()
{
    return String();
}

String GetCameraDriverVersion()
{
    return String();
}

#else

String GetDisplayDriverVersion()
{
    char info[256] = { 0 };
    FILE *file = popen("/usr/bin/glxinfo", "r");
    if (file)
    {
        int status = 0;
        while (status == 0)
        {
            status = fscanf(file, "%*[^\n]\n"); // Read up till the end of the current line, leaving the file pointer at the beginning of the next line (skipping any leading whitespace).
            OVR_UNUSED(status);                 // Prevent GCC compiler warning: "ignoring return value of ‘int fscanf(FILE*, const char*, ...)"

            status = fscanf(file, "OpenGL version string: %255[^\n]", info);
        }
        pclose(file);
        if (status == 1)
        {
            return String(info);
        }
    }
    return String("No graphics driver details found.");
}

String GetCameraDriverVersion()
{
    struct utsname kver;
    if (uname(&kver))
    {
        return String();
    }
    return String(kver.release);
}

void GetGraphicsCardList(OVR::Array< OVR::String > &gpus)
{
    gpus.Clear();

    char info[256] = { 0 };
    FILE *file = popen("/usr/bin/lspci", "r");
    if (file)
    {
        int status = 0;
        while (status >= 0)
        {
            status = fscanf(file, "%*[^\n]\n"); // Read up till the end of the current line, leaving the file pointer at the beginning of the next line (skipping any leading whitespace).
            OVR_UNUSED(status);                 // Prevent GCC compiler warning: "ignoring return value of ‘int fscanf(FILE*, const char*, ...)"

            status = fscanf(file, "%*[^ ] VGA compatible controller: %255[^\n]", info);
            if (status == 1)
            {
                gpus.PushBack(String(info));
            }
        }
        pclose(file);
    }
    if (gpus.GetSizeI() <= 0)
    {
        gpus.PushBack(String("No video card details found."));
    }
}

String OSVersionAsString()
{
    char info[256] = { 0 };
    FILE *file = fopen("/etc/issue", "r");
    if (file)
    {
        int status = fscanf(file, "%255[^\n\\]", info);
        fclose(file);
        if (status == 1)
        {
            return String(info);
        }
    }
    return String("No OS version details found.");
}

String GetProcessorInfo()
{
    char info[256] = { 0 };
    FILE *file = fopen("/proc/cpuinfo", "r");
    if (file)
    {
        int status = 0;
        while (status == 0)
        {
            status = fscanf(file, "%*[^\n]\n"); // Read up till the end of the current line, leaving the file pointer at the beginning of the next line (skipping any leading whitespace).
            OVR_UNUSED(status);                 // Prevent GCC compiler warning: "ignoring return value of ‘int fscanf(FILE*, const char*, ...)"

            status = fscanf(file, "model name : %255[^\n]", info);
        }
        fclose(file);
        if (status == 1)
        {
            return String(info);
        }
    }
    return String("No processor details found.");
}
#endif //OVR_OS_MAC
#endif // WIN32


//-----------------------------------------------------------------------------
// Get the path for local app data.

String GetBaseOVRPath(bool create_dir)
{
#if defined(OVR_OS_WIN32)

    wchar_t path[MAX_PATH];
    ::SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA, NULL, 0, path);

    OVR_wcscat(path, MAX_PATH, L"\\Oculus");

    if (create_dir)
    {   // Create the Oculus directory if it doesn't exist
        DWORD attrib = ::GetFileAttributesW(path);
        bool exists = attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY);
        if (!exists)
        {   
            ::CreateDirectoryW(path, NULL);
        }
    }

#elif defined(OVR_OS_MS) // Other Microsoft OSs

    // TODO: figure this out.
    OVR_UNUSED ( create_dir );
    path = "";
        
#elif defined(OVR_OS_MAC)

    const char* home = getenv("HOME");
    path = home;
    path += "/Library/Preferences/Oculus";

    if (create_dir)
    {   // Create the Oculus directory if it doesn't exist
        DIR* dir = opendir(path);
        if (dir == NULL)
        {
            mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
        }
        else
        {
            closedir(dir);
        }
    }

#else

    const char* home = getenv("HOME");
    String path = home;
    path += "/.config/Oculus";

    if (create_dir)
    {   // Create the Oculus directory if it doesn't exist
        DIR* dir = opendir(path);
        if (dir == NULL)
        {
            mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
        }
        else
        {
            closedir(dir);
        }
    }

#endif

    return String(path);
}

#ifdef OVR_OS_MS
//widechar functions are Windows only for now
bool GetRegistryStringW(const wchar_t* pSubKey, const wchar_t* stringName, wchar_t out[MAX_PATH], bool wow64value, bool currentUser)
{
    HKEY root = currentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    DWORD dwType = REG_SZ;
    HKEY hKey = 0;
    wchar_t value[MAX_PATH + 1]; // +1 because RegQueryValueEx doesn't necessarily 0-terminate.
    DWORD value_length = MAX_PATH;

    if ((RegOpenKeyExW(root, pSubKey, 0, KEY_QUERY_VALUE | (wow64value ? KEY_WOW64_32KEY : 0), &hKey) != ERROR_SUCCESS) ||
        (RegQueryValueExW(hKey, stringName, NULL, &dwType, (LPBYTE)&value, &value_length) != ERROR_SUCCESS) || (dwType != REG_SZ))
    {
        out[0] = L'\0';
        RegCloseKey(hKey);
        return false;
    }
    RegCloseKey(hKey);

    value[value_length] = L'\0';
    wcscpy_s(out, MAX_PATH, value);
    return true;
}


bool GetRegistryDwordW(const wchar_t* pSubKey, const wchar_t* stringName, DWORD& out, bool wow64value, bool currentUser)
{
    HKEY root = currentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    DWORD dwType = REG_DWORD;
    HKEY hKey = 0;
    DWORD value_length = sizeof(DWORD);

    if ((RegOpenKeyExW(root, pSubKey, 0, KEY_QUERY_VALUE | (wow64value ? KEY_WOW64_32KEY : 0), &hKey) != ERROR_SUCCESS) ||
        (RegQueryValueExW(hKey, stringName, NULL, &dwType, (LPBYTE)&out, &value_length) != ERROR_SUCCESS) || (dwType != REG_DWORD))
    {
        out = 0;
        RegCloseKey(hKey);
        return false;
    }
    RegCloseKey(hKey);

    return true;
}

bool GetRegistryBinaryW(const wchar_t* pSubKey, const wchar_t* stringName, LPBYTE out, DWORD* size, bool wow64value, bool currentUser)
{
    HKEY root = currentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    DWORD dwType = REG_BINARY;
    HKEY hKey = 0;

    if ((RegOpenKeyExW(root, pSubKey, 0, KEY_QUERY_VALUE | (wow64value ? KEY_WOW64_32KEY : 0), &hKey) != ERROR_SUCCESS) ||
        (RegQueryValueExW(hKey, stringName, NULL, &dwType, out, size) != ERROR_SUCCESS) || (dwType != REG_BINARY))
    {
        *out = 0;
        RegCloseKey(hKey);
        return false;
    }
    RegCloseKey(hKey);

    return true;
}


// When reading Oculus registry keys, we recognize that the user may have inconsistently 
// used a DWORD 1 vs. a string "1", and so we support either when checking booleans.
bool GetRegistryBoolW(const wchar_t* pSubKey, const wchar_t* stringName, bool defaultValue, bool wow64value, bool currentUser)
{
    wchar_t out[MAX_PATH];
    if (GetRegistryStringW(pSubKey, stringName, out, wow64value, currentUser))
    {
        return (_wtoi64(out) != 0);
    }

    DWORD dw;
    if (GetRegistryDwordW(pSubKey, stringName, dw, wow64value, currentUser))
    {
        return (dw != 0);
    }

    return defaultValue;
}

bool SetRegistryBinaryW(const wchar_t* pSubKey, const wchar_t* stringName, LPBYTE value, DWORD size, bool wow64value, bool currentUser)
{
    HKEY root = currentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    HKEY hKey = 0;

    if ((RegCreateKeyExW(root, pSubKey, 0, nullptr, 0, KEY_CREATE_SUB_KEY | KEY_SET_VALUE | (wow64value ? KEY_WOW64_32KEY : 0), nullptr, &hKey, nullptr) != ERROR_SUCCESS) ||
        (RegSetValueExW(hKey, stringName, 0, REG_BINARY, value, size) != ERROR_SUCCESS))
    {
        RegCloseKey(hKey);
        return false;
    }
    RegCloseKey(hKey);
    return true;
}

bool DeleteRegistryValue(const wchar_t* pSubKey, const wchar_t* stringName, bool wow64value, bool currentUser)
{
    HKEY root = currentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    HKEY hKey = 0;

    if (RegOpenKeyExW(root, pSubKey, 0, KEY_ALL_ACCESS | (wow64value ? KEY_WOW64_32KEY : 0), &hKey) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return false;
    }
    bool result = (RegDeleteValueW(hKey, stringName) == ERROR_SUCCESS);

    RegCloseKey(hKey);
    return result;
}



bool GetOVRRuntimePathW(wchar_t out[MAX_PATH])
{
    if (Is64BitWindows())
    {
        if (!GetRegistryStringW(L"Software\\Wow6432Node\\Oculus VR, LLC\\Oculus Runtime", L"Location", out))
        {
            return false;
        }
    }
    else if (!GetRegistryStringW(L"Software\\Oculus VR, LLC\\Oculus Runtime", L"Location", out))
    {
        return false;
    }
    return true;
}
#endif // OVR_OS_MS

bool GetOVRRuntimePath(String& runtimePath)
{
    runtimePath = "";
#ifdef OVR_OS_MS
    wchar_t path[MAX_PATH];
    if (GetOVRRuntimePathW(path))
    {
        runtimePath = String(path);
        return true;
    }
#endif // OVR_OS_MS
    //mac/linux uses environment variables
    return false;
}

bool GetDefaultFirmwarePath(String& firmwarePath)
{
    if (!GetOVRRuntimePath(firmwarePath))
    {
        return false;
    }
    else
    {
        firmwarePath + "\\Tools\\FirmwareBundle.json";
        return true;
    }
}

bool GetFirmwarePathFromService(OVR::String& firmwarePath, int numSearchDirs)
{
#ifdef OVR_OS_MS
    firmwarePath = "";

    //Try relative file locations
    wchar_t path[MAX_PATH]; // FIXME: This is Windows-specific.
    // Get full path to our module.
    int pathlen = ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    OVR_ASSERT_AND_UNUSED(pathlen, pathlen);

    //try the registry default
    if (GetOVRRuntimePath(firmwarePath) && !firmwarePath.IsEmpty())
    {
        if (OVR_strncmp(OVR::String(path), firmwarePath, firmwarePath.GetSize()) == 0)
        {
            //we are in the default runtime directory.
            firmwarePath += "/Tools/Firmware/";
            return true;
        }
        else
        {
            firmwarePath = "";
        }
    }

    if (firmwarePath.IsEmpty())
    {
        //try internal path.
        wchar_t relpath[MAX_PATH];
        relpath[0] = L'\0';
        for (int i = 0; i < numSearchDirs; i++)
        {
            wchar_t* trailingSlash = wcsrchr(path, L'\\');
            if (trailingSlash == nullptr)
            {
                break; // no more paths to traverse
            }
            *(trailingSlash + 1) = L'\0'; //delete after the last trailing slash
            //Then attach this prefix
            OVR_wcscat(path, MAX_PATH, L"Firmware\\");
            //And attempt to find the path
            if (::PathFileExistsW(path))
            {
                firmwarePath = String(path);
                return true;
            }
            else
            {
                *trailingSlash = L'\0'; //remove trailing slash, traversing up 1 directory
            }
        }
    }

    firmwarePath = L"";

#else
    OVR_UNUSED2(firmwarePath, numSearchDirs);
    //#error "FIXME"
#endif

    return false;
}

}} // namespace OVR::Util
