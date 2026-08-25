/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef ANDROID_AUDIO_POLICY_CORE_H
#define ANDROID_AUDIO_POLICY_CORE_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

/* The enums were moved here mostly from
 * frameworks/base/include/media/AudioSystem.h
 */

#define AUDIO_ENUM_QUOTE(x) #x
#define AUDIO_ENUM_STRINGIFY(x) AUDIO_ENUM_QUOTE(x)
#define AUDIO_DEFINE_ENUM_SYMBOL(symbol) symbol,
#define AUDIO_DEFINE_STRINGIFY_CASE(symbol) case symbol: return AUDIO_ENUM_STRINGIFY(symbol);

/* device categories used for audio_policy->set_force_use()
 * These must match the values in AudioSystem.java
 */
#define AUDIO_POLICY_FORCE_LIST_DEF(V)                       \
    V(AUDIO_POLICY_FORCE_NONE)                               \
    V(AUDIO_POLICY_FORCE_SPEAKER)                            \
    V(AUDIO_POLICY_FORCE_HEADPHONES)                         \
    V(AUDIO_POLICY_FORCE_BT_SCO)                             \
    V(AUDIO_POLICY_FORCE_BT_A2DP)                            \
    V(AUDIO_POLICY_FORCE_WIRED_ACCESSORY)                    \
    V(AUDIO_POLICY_FORCE_BT_CAR_DOCK)                        \
    V(AUDIO_POLICY_FORCE_BT_DESK_DOCK)                       \
    V(AUDIO_POLICY_FORCE_ANALOG_DOCK)                        \
    V(AUDIO_POLICY_FORCE_DIGITAL_DOCK)                       \
    V(AUDIO_POLICY_FORCE_NO_BT_A2DP)                         \
    V(AUDIO_POLICY_FORCE_SYSTEM_ENFORCED)                    \
    V(AUDIO_POLICY_FORCE_HDMI_SYSTEM_AUDIO_ENFORCED)         \
    V(AUDIO_POLICY_FORCE_ENCODED_SURROUND_NEVER)             \
    V(AUDIO_POLICY_FORCE_ENCODED_SURROUND_ALWAYS)            \
    V(AUDIO_POLICY_FORCE_ENCODED_SURROUND_MANUAL)            \
    V(AUDIO_POLICY_FORCE_BT_BLE)

typedef enum {
    AUDIO_POLICY_FORCE_LIST_DEF(AUDIO_DEFINE_ENUM_SYMBOL)

    AUDIO_POLICY_FORCE_CFG_CNT,
    AUDIO_POLICY_FORCE_CFG_MAX = AUDIO_POLICY_FORCE_CFG_CNT - 1,

    AUDIO_POLICY_FORCE_DEFAULT = AUDIO_POLICY_FORCE_NONE,
} audio_policy_forced_cfg_t;

inline const char* audio_policy_forced_cfg_to_string(audio_policy_forced_cfg_t t) {
    switch (t) {
    AUDIO_POLICY_FORCE_LIST_DEF(AUDIO_DEFINE_STRINGIFY_CASE)
    default:
        return "";
    }
}

#undef AUDIO_POLICY_FORCE_LIST_DEF

/* usages used for audio_policy->set_force_use()
 * These must match the values in AudioSystem.java
 */
typedef enum {
    AUDIO_POLICY_FORCE_FOR_COMMUNICATION,
    AUDIO_POLICY_FORCE_FOR_MEDIA,
    AUDIO_POLICY_FORCE_FOR_RECORD,
    AUDIO_POLICY_FORCE_FOR_DOCK,
    AUDIO_POLICY_FORCE_FOR_SYSTEM,
    AUDIO_POLICY_FORCE_FOR_HDMI_SYSTEM_AUDIO,
    AUDIO_POLICY_FORCE_FOR_ENCODED_SURROUND,
    AUDIO_POLICY_FORCE_FOR_VIBRATE_RINGING,

    AUDIO_POLICY_FORCE_USE_CNT,
    AUDIO_POLICY_FORCE_USE_MAX = AUDIO_POLICY_FORCE_USE_CNT - 1,
} audio_policy_force_use_t;

/* device connection states used for audio_policy->set_device_connection_state()
 */
typedef enum {
    AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE,
    AUDIO_POLICY_DEVICE_STATE_AVAILABLE,

    AUDIO_POLICY_DEVICE_STATE_CNT,
    AUDIO_POLICY_DEVICE_STATE_MAX = AUDIO_POLICY_DEVICE_STATE_CNT - 1,
} audio_policy_dev_state_t;

typedef enum {
    /* Used to generate a tone to notify the user of a
     * notification/alarm/ringtone while they are in a call. */
    AUDIO_POLICY_TONE_IN_CALL_NOTIFICATION = 0,

    AUDIO_POLICY_TONE_CNT,
    AUDIO_POLICY_TONE_MAX                  = AUDIO_POLICY_TONE_CNT - 1,
} audio_policy_tone_t;

/* AudioRecord client UID state derived from ActivityManager::PROCESS_STATE_XXX
 * and used for concurrent capture policy.
 */
typedef enum {
    APP_STATE_IDLE = 0,   /* client is idle: cannot capture */
    APP_STATE_FOREGROUND, /* client has a foreground service: can capture */
    APP_STATE_TOP, /* client has a visible UI: can capture and select use case */
} app_state_t;

/* The role indicates how the audio policy manager should consider particular
 * device(s) when making routing decisions for a particular strategy or audio
 * source. It is primarily used to override the default routing rules.
 */
typedef enum {
    DEVICE_ROLE_NONE = 0, /* default routing rules and priority apply */
    DEVICE_ROLE_PREFERRED = 1, /* devices are specified as preferred devices */
    DEVICE_ROLE_DISABLED = 2, /* devices cannot be used */
} device_role_t;

#undef AUDIO_DEFINE_STRINGIFY_CASE
#undef AUDIO_DEFINE_ENUM_SYMBOL
#undef AUDIO_ENUM_STRINGIFY
#undef AUDIO_ENUM_QUOTE

__END_DECLS

#endif  // ANDROID_AUDIO_POLICY_CORE_H
