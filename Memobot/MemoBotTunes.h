#pragma once

#include <Arduino.h>

#include "SBK_Tune.h"

//=====================================================================
// SBK_AvrBuzzer Example Tunes
//=====================================================================
//
// SPDX-License-Identifier: MIT
//
// Original example melodies demonstrating tune playback with the
// SBK_AvrBuzzer library.
//
// These melodies are provided as examples and may be copied, modified
// or replaced in user projects.
//
// Copyright (c) 2026 Samuel Barabe
// Licensed under the MIT License.
//=====================================================================

//=====================================================================
// Example Tune Definitions
//=====================================================================

SBK_DEFINE_TUNE(TUNE_MEMOBOT_VICTORY, 156,
                NOTE_C5, D_16,
                NOTE_E5, D_16,
                NOTE_G5, D_16,
                NOTE_C6, D_8,

                NOTE_G5, D_16,
                NOTE_E5, D_16,
                NOTE_G5, D_16,
                NOTE_C6, D_8,

                NOTE_D6, D_16,
                NOTE_E6, D_16,
                NOTE_G6, D_16,
                NOTE_C7, D_8,

                NOTE_G6, D_16,
                NOTE_E6, D_16,
                NOTE_C7, D_4
            );

SBK_DEFINE_TUNE(TUNE_MEMOBOT_BOUNCE, 172,
                NOTE_C5, D_16,
                NOTE_G5, D_16,
                NOTE_E5, D_16,
                NOTE_G5, D_16,

                NOTE_D5, D_16,
                NOTE_A5, D_16,
                NOTE_FS5, D_16,
                NOTE_A5, D_16,

                NOTE_E5, D_16,
                NOTE_G5, D_16,
                NOTE_C6, D_16,
                NOTE_G5, D_16,

                NOTE_E5, D_16,
                NOTE_C5, D_16,
                NOTE_G4, D_8,
                NOTE_C5, D_8
            );

SBK_DEFINE_TUNE(TUNE_MEMOBOT_DANCE, 165,
                NOTE_C5, D_16,
                NOTE_REST, D_16,
                NOTE_E5, D_16,
                NOTE_G5, D_16,

                NOTE_E5, D_16,
                NOTE_REST, D_16,
                NOTE_C6, D_16,
                NOTE_G5, D_16,

                NOTE_D5, D_16,
                NOTE_REST, D_16,
                NOTE_F5, D_16,
                NOTE_A5, D_16,

                NOTE_G5, D_16,
                NOTE_E5, D_16,
                NOTE_C5, D_8,
                NOTE_REST, D_8
            );

SBK_DEFINE_TUNE(TUNE_MEMOBOT_GRAND, 150,
                NOTE_C5, D_16,
                NOTE_D5, D_16,
                NOTE_E5, D_16,
                NOTE_G5, D_16,

                NOTE_C6, D_16,
                NOTE_B5, D_16,
                NOTE_A5, D_16,
                NOTE_G5, D_16,

                NOTE_E6, D_16,
                NOTE_G6, D_16,
                NOTE_C7, D_8,

                NOTE_G6, D_16,
                NOTE_E6, D_16,
                NOTE_C7, D_4
            );

//=====================================================================
// Example Tune Table
//=====================================================================
//
// This table can be passed to TunePlayer::playRandom().

const Tune *const MEMOBOT_TUNES[] =
    {
        &TUNE_MEMOBOT_VICTORY,
        &TUNE_MEMOBOT_BOUNCE,
        &TUNE_MEMOBOT_DANCE,
        &TUNE_MEMOBOT_GRAND
    };

constexpr uint8_t MEMOBOT_TUNE_COUNT =
    static_cast<uint8_t>(
        sizeof(MEMOBOT_TUNES) / sizeof(MEMOBOT_TUNES[0]));
