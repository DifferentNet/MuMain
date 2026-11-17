// stdafx.h : include file for standard system include files,
#pragma once

//warning
#pragma warning( disable : 4067 )
#pragma warning( disable : 4786 )
#pragma warning( disable : 4800 )
#pragma warning( disable : 4996 )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4237 )
#pragma warning( disable : 4305 )
#pragma warning( disable : 4503 )
#pragma warning( disable : 4267 )
#pragma warning( disable : 4091 )
#pragma warning( disable : 4819 )
#pragma warning( disable : 4505 )
#pragma warning( disable : 4100 )
#pragma warning( disable : 4127 )
#pragma warning( disable : 4702 )
#pragma warning( disable : 4838 )
#pragma warning( disable : 5208 )
//#pragma warning( disable : 4482 )
//#pragma warning( disable : 4700 )
//#pragma warning( disable : 4748 )
//#pragma warning( disable : 4786 )
#pragma warning( disable : 28159 )
#pragma warning( disable : 26812 )

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

// Use 32-bit time_t only for 32-bit MSVC builds. This avoids conflicts
// with 64-bit toolchains such as x86_64-w64-mingw32, which require
// 64-bit time_t.
#if defined(_MSC_VER) && !defined(_WIN64)
#ifndef _USE_32BIT_TIME_T
#define _USE_32BIT_TIME_T
#endif //_USE_32BIT_TIME_T
#endif

#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE

// Cross-platform debug breakpoint helper.
#ifdef _MSC_VER
#define MU_DEBUG_BREAK() __debugbreak()
#else
#define MU_DEBUG_BREAK() __builtin_trap()
#endif

#pragma warning( push, 3 )

// Win32-only system headers. Guarded behind _WIN32 so the PCH can be parsed by
// non-Windows toolchains. On Windows _WIN32 is always defined (for both 32-bit
// and 64-bit targets), so every include below is taken and the build stays
// unchanged. Win32 types/macros still referenced by portable code past the PCH
// (e.g. the project headers further down) are a separate, case-by-case follow-up.
#ifdef _WIN32
#include <windows.h>

// MinGW workaround: Allow swprintf usage
#if defined(__MINGW32__) || defined(__MINGW64__)
#undef swprintf
#endif

#include <winsock2.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <tchar.h>
#include <mbstring.h>
#include <conio.h>
#else // !_WIN32
// Provide the Win32 scalar/handle types and the MSVC secure-CRT (_s) functions
// the engine uses, so the ~350 PCH consumers parse on non-Windows toolchains
// (issue #462, Phase 1).
#include "Core/Platform/WinCompat.h"
#include "Core/Platform/SecureCrt.h"
#include "Core/Platform/WinApiShims.h"
#include "Core/Platform/WinUser.h"
#include "Core/Platform/WinNls.h"
#include "Core/Platform/WinGdi.h"
#include "Core/Platform/WinFile.h"
#include "Core/Platform/WinFileFind.h"
#include "Core/Platform/Imm.h"
#endif // _WIN32

//c runtime
#include <stdio.h>
#include <stdlib.h>
#if !defined(__APPLE__)
#include <malloc.h>
#endif
#include <memory.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>

#include <string>
#include <list>
#include <map>
#include <deque>
#include <algorithm>
#include <type_traits>
#include <vector>
#include <queue>

#pragma warning( pop )

// mu_swprintf / mu_swprintf_s live in their own header so tests can use them without the rest of this file.
#include "Core/Text/MuSwprintf.h"

//opengl
#include <gl/glew.h>
#include <gl/GL.h>
#include "Render/Renderer/GLCompatShim.h"

//patch
//winmain
#include "App/Platform/Windows/Winmain.h"
#include "Core/Globals/Defined_Global.h"

//client
#include "Core/Globals/_define.h"
#include "Core/Globals/_enum.h"
#include "Core/Globals/_types.h"
#include "Core/Globals/_struct.h"
#include "Dotnet/PacketFunctions_CommonEnums.h"
#include "Dotnet/PacketFunctions_ClientToServer_Enums.h"
#include "Core/Utilities/_GlobalFunctions.h"
#include "Core/Globals/_TextureIndex.h"
#include "UI/Core/WindowCommon.h"
#include "Core/Math/ZzzMathLib.h"

// Reference resolution -- all UI coordinates and screen-space math use this as the base.
// Must be declared BEFORE ZzzOpenglUtil.h because BeginOpengl() uses them as default args.
// g_fScreenRate_x/y (declared further below) scale from reference to actual window size.
inline constexpr int REFERENCE_WIDTH = 640;
inline constexpr int REFERENCE_HEIGHT = 480;

#include "Render/Textures/ZzzOpenglUtil.h"

#include "Data/Translation/MultiLanguage.h"


#include "Core/Utilities/Log/muConsoleDebug.h"
#include "Core/Utilities/Log/ErrorReport.h"
#include "Core/Utilities/Log/WindowsConsole.h"

// Redirect console output to ImGui when editor is enabled
#ifdef _EDITOR
#include "../MuEditor/UI/Console/MuEditorConsoleRedirectUI.h"
#endif

#include "World/MapInfra/w_MapHeaders.h"

#include "Core/Globals/_crypt.h"

inline std::wstring g_strSelectedML = L"";
// Active logical-to-window transform. Screen overlays use full-window scale;
// panel UI uses centered, capped scale plus offsets.
inline float g_fScreenRate_x = 0;
inline float g_fScreenRate_y = 0;
inline float g_fScreenOffset_x = 0;
inline float g_fScreenOffset_y = 0;
inline float g_fTypographyScale = 1.0f;
inline float g_fWindowMouseX = 0;
inline float g_fWindowMouseY = 0;
