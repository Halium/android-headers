#ifndef HYBRIS_CONFIG_H_
#define HYBRIS_CONFIG_H_

/* When android is built for a specific device the build is
   modified by BoardConfig.mk and possibly other mechanisms.
   eg
   device/samsung/i9305/BoardConfig.mk:
       COMMON_GLOBAL_CFLAGS += -DCAMERA_WITH_CITYID_PARAM
   device/samsung/smdk4412-common/BoardCommonConfig.mk:
       COMMON_GLOBAL_CFLAGS += -DEXYNOS4_ENHANCEMENTS

   This file allows those global configurations, which are not
   otherwise defined in the build headers, to be available in
   hybris builds.

   Typically it is generated at hardware adaptation time.

   The CONFIG GOES... line below can be used by automation to modify
   this file.
*/

#include "android-version.h"

/* CONFIG GOES HERE */

#endif

/* Compatibility for clang-specific annotations when building with GCC */
#ifndef _Nonnull
#define _Nonnull
#endif
#ifndef _Nullable
#define _Nullable
#endif

/* Neutralize __INTRODUCED_IN() API level annotations (clang-only) */
#ifdef __INTRODUCED_IN
#undef __INTRODUCED_IN
#endif
#define __INTRODUCED_IN(x)

#ifdef __INTRODUCED_IN_NO_GUARD_FOR_NDK
#undef __INTRODUCED_IN_NO_GUARD_FOR_NDK
#endif
#define __INTRODUCED_IN_NO_GUARD_FOR_NDK(x)

#ifdef __DEPRECATED_IN
#undef __DEPRECATED_IN
#endif
#define __DEPRECATED_IN(x, ...)
