/************************************************************************************

Filename    :   Util_SystemInfo.h
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


#ifndef OVR_Util_SystemInfo_h
#define OVR_Util_SystemInfo_h

#include "../Kernel/OVR_String.h"
#include "../Kernel/OVR_Types.h"
#include "../Kernel/OVR_Array.h"

namespace OVR { namespace Util {
bool Is64BitWindows();
const char * OSAsString();
String OSVersionAsString();
uint64_t GetGuidInt();
String GetGuidString();
const char * GetProcessInfo();
String GetDisplayDriverVersion();
String GetCameraDriverVersion();
void GetGraphicsCardList(OVR::Array< OVR::String > &gpus);
String GetProcessorInfo();

//Retrives the root of the Oculus install directory
bool GetOVRRuntimePath(OVR::String &runtimePath);

//Retrives the root of the Oculus install directory and from there finds the firmware bundle relative.
//On Windows this is defined as a registry key. On Mac and Linux this is set as an environment variable.
//For Development builds with no key set, we at 10 directories for (DIR)/Firmware/FirmwareBundle.json .
//Iterating DIR as "..//" * maxSearchDirs concatenated.
bool GetFirmwarePathFromService(OVR::String &runtimePath, int numSearchDirs = 10);

#ifdef OVR_OS_MS

//Retrives the root of the Oculus install directory
bool GetOVRRuntimePathW(wchar_t out[MAX_PATH]);

// Returns true if a string-type registry key of the given name is present, else sets out to empty and returns false.
// The output string will always be 0-terminated, but may be empty.
// If wow64value is true then KEY_WOW64_32KEY is used in the registry lookup.
// If currentUser is true then HKEY_CURRENT_USER root is used instead of HKEY_LOCAL_MACHINE
bool GetRegistryStringW(const wchar_t* pSubKey, const wchar_t* stringName, wchar_t out[MAX_PATH], bool wow64value = false, bool currentUser = false);

// Returns true if a DWORD-type registry key of the given name is present, else sets out to 0 and returns false.
// If wow64value is true then KEY_WOW64_32KEY is used in the registry lookup.
// If currentUser is true then HKEY_CURRENT_USER root is used instead of HKEY_LOCAL_MACHINE
bool GetRegistryDwordW(const wchar_t* pSubKey, const wchar_t* stringName, DWORD& out, bool wow64value = false, bool currentUser = false);

// Returns true if a BINARY-type registry key of the given name is present, else sets out to 0 and returns false.
// Size must be set to max size of out buffer on way in. Will be set to size actually read into the buffer on way out.
// If wow64value is true then KEY_WOW64_32KEY is used in the registry lookup.
// If currentUser is true then HKEY_CURRENT_USER root is used instead of HKEY_LOCAL_MACHINE
bool GetRegistryBinaryW(const wchar_t* pSubKey, const wchar_t* stringName, LPBYTE out, DWORD* size, bool wow64value = false, bool currentUser = false);

// Returns true if a registry key of the given type is present and can be interpreted as a boolean, otherwise
// returns defaultValue. It's not possible to tell from a single call to this function if the given registry key
// was present. For Strings, boolean means (atoi(str) != 0). For DWORDs, boolean means (dw != 0).
// If currentUser is true then HKEY_CURRENT_USER root is used instead of HKEY_LOCAL_MACHINE
bool GetRegistryBoolW(const wchar_t* pSubKey, const wchar_t* stringName, bool defaultValue, bool wow64value = false, bool currentUser = false);

// Returns true if the value could be successfully written to the registry.
// If wow64value is true then KEY_WOW64_32KEY is used in the registry write.
// If currentUser is true then HKEY_CURRENT_USER root is used instead of HKEY_LOCAL_MACHINE
bool SetRegistryBinaryW(const wchar_t* pSubKey, const wchar_t* stringName, const LPBYTE value, DWORD size, bool wow64value = false, bool currentUser = false);

// Returns true if the value could be successfully deleted from the registry.
// If wow64value is true then KEY_WOW64_32KEY is used.
// If currentUser is true then HKEY_CURRENT_USER root is used instead of HKEY_LOCAL_MACHINE
bool DeleteRegistryValue(const wchar_t* pSubKey, const wchar_t* stringName, bool wow64value = false, bool currentUser = false);

//Mac + Linux equivelants are not implemented
String GetFileVersionStringW(wchar_t filePath[MAX_PATH]);
String GetSystemFileVersionStringW(wchar_t filePath[MAX_PATH]);
#endif // OVR_OS_MS


//-----------------------------------------------------------------------------
// Get the path for local app data.

String GetBaseOVRPath(bool create_dir);


} } // namespace OVR { namespace Util {

#endif // OVR_Util_SystemInfo_h
