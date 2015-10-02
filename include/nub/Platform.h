/*  <nub/NUBplatform.h> -- Platform/Compiler defines and core types
    Copyright (c) 2005-09 by Gerald Lindsly

    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program (License.txt); if not, go to http://www.gnu.org/copyleft/lesser.txt
    or write to:
            Free Software Foundation, Inc.
            59 Temple Place - Suite 330
            Boston, MA 02111-1307

    The original source for this file is from OGRE (OgrePlatform.h), See http://www.ogre3d.org/
    Copyright (c) 2000-2006 Torus Knot Software Ltd

    Not much of this is actually used in NUB, but I have left it largely intact for future
    enhancements.
*/

#ifndef __NUB_PLATFORM_H__
#define __NUB_PLATFORM_H__

#include <cstdint>

namespace nub {

// platform / compiler

#define NUB_PLATFORM_WIN32 1
#define NUB_PLATFORM_LINUX 2
#define NUB_PLATFORM_APPLE 3

#define NUB_COMPILER_MSVC 1
#define NUB_COMPILER_GNUC 2
#define NUB_COMPILER_BORL 3

#define NUB_ENDIAN_LITTLE 1
#define NUB_ENDIAN_BIG 2

#define NUB_ARCHITECTURE_32 1
#define NUB_ARCHITECTURE_64 2

/* Finds the compiler type and version.
*/
#if defined(_MSC_VER)
#   define NUB_COMPILER NUB_COMPILER_MSVC
#   define NUB_COMP_VER _MSC_VER
#elif defined(__GNUC__)
#   define NUB_COMPILER NUB_COMPILER_GNUC
#   define NUB_COMP_VER (((__GNUC__)*100) + \
        (__GNUC_MINOR__*10) + \
        __GNUC_PATCHLEVEL__)
#elif defined(__BORLANDC__)
#   define NUB_COMPILER NUB_COMPILER_BORL
#   define NUB_COMP_VER __BCPLUSPLUS__
#else
#   pragma error "No known compiler. Abort! Abort!"
#endif

/* See if we can use __forceinline or if we need to use __inline instead */
#if NUB_COMPILER == NUB_COMPILER_MSVC
#   if NUB_COMP_VER >= 1200
#       define FORCEINLINE __forceinline
#   endif
#endif
#if !defined(FORCEINLINE)
#   define FORCEINLINE inline
#endif

/* Finds the current platform */

#if defined(__WIN32__) || defined(_WIN32)
#   define NUB_PLATFORM NUB_PLATFORM_WIN32
#elif defined( __APPLE_CC__)
#   define NUB_PLATFORM NUB_PLATFORM_APPLE
#else
#   define NUB_PLATFORM NUB_PLATFORM_LINUX
#endif

    /* Find the arch type */
#if defined(__x86_64__) || defined(_M_X64) || defined(__powerpc64__) || defined(__alpha__) || defined(__ia64__) || defined(__s390__) || defined(__s390x__)
#   define NUB_ARCH_TYPE NUB_ARCHITECTURE_64
#else
#   define NUB_ARCH_TYPE NUB_ARCHITECTURE_32
#endif

// For generating compiler warnings - should work on any compiler
// As a side note, if you start your message with 'Warning: ', the MSVC
// IDE actually does catch a warning :)
#define NUB_QUOTE_INPLACE(x) #x
#define NUB_QUOTE(x) NUB_QUOTE_INPLACE(x)
#define NUB_WARN(x)  message(__FILE__ "(" NUB_QUOTE(__LINE__) ") : " x "\n")

//----------------------------------------------------------------------------
// Windows Settings
#if NUB_PLATFORM == NUB_PLATFORM_WIN32

// If we're not including this from a client build, specify that the stuff
// should get exported. Otherwise, import it.
#	if defined(NUB_STATIC_LIB)
		// Linux compilers don't have symbol import/export directives.
#   	define _NubExport
#   	define _NubPrivate
#   else
#   	if defined(NUB_NONCLIENT_BUILD)
#       	define _NubExport __declspec(dllexport)
#   	else
#           if defined(__MINGW32__)
#               define _NubExport
#           else
#       	    define _NubExport __declspec(dllimport)
#           endif
#   	endif
#   	define _NubPrivate
#	endif
// Win32 compilers use _DEBUG for specifying debug builds.
#   ifdef _DEBUG
#       define NUB_DEBUG_MODE 1
#   else
#       define NUB_DEBUG_MODE 0
#   endif

// Disable unicode support on MingW at the moment, poorly supported in stdlibc++
// STLPORT fixes this though so allow if found
// MinGW C++ Toolkit supports unicode and sets the define __MINGW32_TOOLKIT_UNICODE__ in _mingw.h
#if defined(__MINGW32__) && !defined(_STLPORT_VERSION)
#   include <_mingw.h>
#   if defined(__MINGW32_TOOLBOX_UNICODE__)
#	    define NUB_UNICODE_SUPPORT 1
#   else
#       define NUB_UNICODE_SUPPORT 0
#   endif
#else
#	define NUB_UNICODE_SUPPORT 1
#endif

#endif

//----------------------------------------------------------------------------
// Linux/Apple Settings
#if NUB_PLATFORM == NUB_PLATFORM_LINUX || NUB_PLATFORM == NUB_PLATFORM_APPLE

// Enable GCC symbol visibility
#   if defined(NUB_GCC_VISIBILITY)
#       define _NubExport  __attribute__ ((visibility("default")))
#       define _NubPrivate __attribute__ ((visibility("hidden")))
#   else
#       define _NubExport
#       define _NubPrivate
#   endif

// A quick define to overcome different names for the same function
#   define stricmp strcasecmp

// Unlike the Win32 compilers, Linux compilers seem to use DEBUG for when
// specifying a debug build.
// (??? this is wrong, on Linux debug builds aren't marked in any way unless
// you mark it yourself any way you like it -- zap ???)
#   ifdef DEBUG
#       define NUB_DEBUG_MODE 1
#   else
#       define NUB_DEBUG_MODE 0
#   endif

#if NUB_PLATFORM == NUB_PLATFORM_APPLE
    #define NUB_PLATFORM_LIB "NUBPlatform.bundle"
#else
    //NUB_PLATFORM_LINUX
    #define NUB_PLATFORM_LIB "libNUBPlatform.so"
#endif

// Always enable unicode support for the moment
// Perhaps disable in old versions of gcc if necessary
#define NUB_UNICODE_SUPPORT 1

#endif

//----------------------------------------------------------------------------
// Endian Settings
// check for BIG_ENDIAN config flag, set NUB_ENDIAN correctly
#ifdef NUB_CONFIG_BIG_ENDIAN
#    define NUB_ENDIAN NUB_ENDIAN_BIG
#else
#    define NUB_ENDIAN NUB_ENDIAN_LITTLE
#endif
// Note: No code in NUB currently checks for endian


//----------------------------------------------------------------------------
// Core Types
// (These should probably be dependent on NUB_ARCHITECTURE_x)
typedef int            int32;
typedef short          int16;
typedef char           int8;
typedef unsigned int   uint32;
typedef unsigned short uint16;
typedef unsigned char  uint8;
typedef unsigned char  byte;
typedef std::int64_t   int64;
// typedef long long   tFilePos;

typedef int64          tFilePos;

typedef char           tChar;  // for later foreign language support (UTF-8)
//----------------------------------------------------------------------------
// Safe Deletion
#define NUB_DELETE(x)       { if (x) { delete x; x = 0; } }
#define NUB_DELETE_ARRAY(x) { if (x) { delete[] x; x = 0; } }

} // namespace nub

#endif // __NUB_PLATFORM_H__

