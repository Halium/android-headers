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

   The CONFIG GOES HERE line can be used by automation to modify
   this file.
*/

#include <android-version.h>

/* CONFIG GOES HERE */

#endif
