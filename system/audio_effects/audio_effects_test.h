/*
 * Copyright (C) 2025 The Android Open Source Project
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

#pragma once

#include <system/audio_effect.h>
#include <utils/Errors.h>

namespace android::effect::utils {

// --- Review of structures and methods used for effects ----
//
// effect_param_s structure describes the format of the pCmdData argument of EFFECT_CMD_SET_PARAM
// command and pCmdData and pReplyData of EFFECT_CMD_GET_PARAM command.
// psize and vsize represent the actual size of parameter and value.
//
// NOTE: the start of value field inside the data field is always on a 32 bit boundary:
//
//  +-----------+
//  | status    | sizeof(int)
//  +-----------+
//  | psize     | sizeof(int)
//  +-----------+
//  | vsize     | sizeof(int)
//  +-----------+
//  |           |   |           |
//  ~ parameter ~   > psize     |
//  |           |   |           >  ((psize - 1)/sizeof(int) + 1) * sizeof(int)
//  +-----------+               |
//  | padding   |               |
//  +-----------+
//  |           |   |
//  ~ value     ~   > vsize
//  |           |   |
//  +-----------+
//
// typedef struct effect_param_s {
//     int32_t     status;     // Transaction status (unused for command, used for reply)
//     uint32_t    psize;      // Parameter size
//     uint32_t    vsize;      // Value size
//     char        data[];     // Start of Parameter + Value data
// } effect_param_t;
//
// From (*handle) can access:
//
// int32_t (*process)(effect_handle_t self,
//         audio_buffer_t *inBuffer,
//         audio_buffer_t *outBuffer);
//
// int32_t (*command)(effect_handle_t self,
//         uint32_t cmdCode,
//         uint32_t cmdSize,
//         void *pCmdData,
//         uint32_t *replySize,
//         void *pReplyData);

/**
 * Invoke an effect command with no data and status reply.
 */
inline status_t effect_command_with_status(effect_handle_t handle, uint32_t command) {
    int32_t reply = 0;
    uint32_t replySize = sizeof(reply);
    const int32_t status = (*handle)->command(
            handle, command, 0 /* cmdSize */, nullptr /* pCmdData */, &replySize, &reply);
    if (status) return status;
    return reply;
}

/**
 * Invoke an effect command with param data and status reply.
 */
template <typename P>
requires (std::is_trivially_copyable_v<P>)
status_t effect_command_with_status(effect_handle_t handle, uint32_t command, const P& p) {
    int32_t reply = 0;
    uint32_t replySize = sizeof(reply);

    // We use a copy of p in this method to guarantee that p is never changed.
    P copyP;
    memcpy(&copyP, &p, sizeof(p));  // memcpy ensures hidden padding is identical.

    const int32_t status = (*handle)->command(
            handle, command, sizeof(copyP), &copyP, &replySize, &reply);

    // Could also check if copyP has been modified after calling the (*handle)->command function.
    // The command interface method permits it by void* parameter, but effects shouldn't do so.

    if (status) return status;
    return reply;
}

/**
 * Return the padding size of a parameter type.
 */
template <typename P>
inline constexpr size_t effect_padding_size_v = 4 - (int)sizeof(P) & 3;

/**
 * Return the size of a variable length tuple of value types.
 */
template <typename... Vs>
inline constexpr size_t effect_value_size_v =  (sizeof(Vs) + ...);

/**
 * Invoke an effect command with a parameter and a sequence of values, with status reply.
 */
template <typename P, typename... Vs>
requires (std::is_trivially_copyable_v<P> && sizeof...(Vs) > 0)
status_t effect_command_with_status(
        effect_handle_t handle, uint32_t command, const P& p, const Vs&... vs) {
    constexpr size_t psize = sizeof(p);
    constexpr size_t padding = effect_padding_size_v<P>;
    constexpr size_t vsize = effect_value_size_v<Vs...>;
    constexpr size_t dsize = sizeof(effect_param_t) + psize + padding + vsize;
    uint8_t paramData[dsize];  // to avoid pointer aliasing, only access through param.
    auto param = reinterpret_cast<effect_param_t*>(paramData);

    // Write param sizes, parameter, and padding.
    param->psize = psize;
    param->vsize = vsize;
    memcpy(&param->data[0], &p, psize);
    if constexpr (padding) memset(&param->data[psize], 0, padding);

    // Write each value in turn to param.
    const size_t offset = psize + padding;
    size_t argsize = 0;
    auto copyArg = [&](auto& v) {
        // we only allow trivially copyable values at the moment.
        // allowing special containers requires changing this and the vsize computation above.
        static_assert(std::is_trivially_copyable_v<std::decay_t<decltype(v)>>);
        memcpy(&param->data[offset + argsize], &v, sizeof(v));
        argsize += sizeof(v);
    };
    (copyArg(vs), ...);

    // Invoke command
    int32_t reply = 0;
    uint32_t replySize = sizeof(reply);
    const int32_t status = (*handle)->command(handle, command, dsize, param, &replySize, &reply);
    if (status) return status;
    return reply;
}

/**
 * Enable the effect.
 */
//==================================================================================================
// command: EFFECT_CMD_ENABLE
//--------------------------------------------------------------------------------------------------
// description:
//  Enable the process. Called by the framework before the first call to process()
//--------------------------------------------------------------------------------------------------
// command format:
//  size: 0
//  data: N/A
//--------------------------------------------------------------------------------------------------
// reply format:
//  size: sizeof(int)
//  data: status
//==================================================================================================
inline status_t effect_enable(effect_handle_t handle) {
    return effect_command_with_status(handle, EFFECT_CMD_ENABLE);
}

/**
 * Disable the effect.
 */
//==================================================================================================
// command: EFFECT_CMD_DISABLE
//--------------------------------------------------------------------------------------------------
// description:
//  Disable the process. Called by the framework after the last call to process()
//--------------------------------------------------------------------------------------------------
// command format:
//  size: 0
//  data: N/A
//--------------------------------------------------------------------------------------------------
// reply format:
//  size: sizeof(int)
//  data: status
//==================================================================================================
inline status_t effect_disable(effect_handle_t handle) {
    return effect_command_with_status(handle, EFFECT_CMD_DISABLE);
}

/**
 * Sets an effect parameter.
 */
//==================================================================================================
// command: EFFECT_CMD_SET_PARAM
//--------------------------------------------------------------------------------------------------
// description:
//  Set a parameter and apply it immediately
//--------------------------------------------------------------------------------------------------
// command format:
//  size: sizeof(effect_param_t) + size of param and value
//  data: effect_param_t + param + value. See effect_param_t definition below for value offset
//--------------------------------------------------------------------------------------------------
// reply format:
//  size: sizeof(int)
//  data: status
//==================================================================================================
template <typename P, typename... Vs>
requires (std::is_trivially_copyable_v<P>)
status_t effect_set_param(effect_handle_t handle, const P& p, const Vs&... vs) {
    return effect_command_with_status(handle, EFFECT_CMD_SET_PARAM, p, vs...);
}

/**
 * Sets the effect configuration
 */
//==================================================================================================
// command: EFFECT_CMD_SET_CONFIG
//--------------------------------------------------------------------------------------------------
// description:
//  Apply new audio parameters configurations for input and output buffers
//--------------------------------------------------------------------------------------------------
// command format:
//  size: sizeof(effect_config_t)
//  data: effect_config_t
//--------------------------------------------------------------------------------------------------
// reply format:
//  size: sizeof(int)
//  data: status
status_t effect_set_config(effect_handle_t handle, const effect_config_t& config) {
    return effect_command_with_status(handle, EFFECT_CMD_SET_CONFIG, config);
}

/**
 * Sets the effect configuration for a pass-through insert effect.
 */
status_t effect_set_config(effect_handle_t handle,
        uint32_t sample_rate, audio_channel_mask_t channel_mask, bool accumulate = false) {
    const effect_config_t config = {
            .inputCfg = {
                    .buffer = {
                            .frameCount = 0,
                            .raw = nullptr,
                    },
                    .samplingRate = sample_rate,
                    .channels = channel_mask,
                    .bufferProvider = {
                            .getBuffer = nullptr,
                            .releaseBuffer = nullptr,
                            .cookie = nullptr,
                    },
                    .format = AUDIO_FORMAT_PCM_FLOAT,
                    .accessMode = EFFECT_BUFFER_ACCESS_READ,
                    .mask = EFFECT_CONFIG_ALL,
            },
            .outputCfg = {
                    .buffer = {
                            .frameCount = 0,
                            .raw = nullptr,
                    },
                    .samplingRate = sample_rate,
                    .channels = channel_mask,
                    .bufferProvider = {
                            .getBuffer = nullptr,
                            .releaseBuffer = nullptr,
                            .cookie = nullptr,
                    },
                    .format = AUDIO_FORMAT_PCM_FLOAT,
                    .accessMode = static_cast<uint8_t>(
                            accumulate ? EFFECT_BUFFER_ACCESS_ACCUMULATE
                            : EFFECT_BUFFER_ACCESS_WRITE),
                    .mask = EFFECT_CONFIG_ALL,
            },
    };
    return effect_set_config(handle, config);
}

/**
 * Process data
 */
inline status_t effect_process(effect_handle_t handle, audio_buffer_t* in, audio_buffer_t* out) {
    return (*handle)->process(handle, in, out);
}

}  // namespace android::effect::utils
