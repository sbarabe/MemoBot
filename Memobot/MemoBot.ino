//=====================================================================
// MémoBot Firmware
//=====================================================================
//
// SPDX-License-Identifier: MIT
//
// Firmware for the MémoBot electronic memory game.
//
// Features:
//   - Memory game with 3 difficulty levels in Solo mode
//   - Two-player Duel mode
//   - LED attract mode and animations
//   - Sound effects and melodies
//   - Mute/unmute buzzer with long-press
//
// Hardware configuration options:
//   - Battery monitoring (internal or external using a resistor divider)
//   - Optional power-latch circuit for automatic inactivity and low-battery shutdown
//
// Target MCU:
//   ATmega328PB @ 8 MHz (internal oscillator)
//
// Project:
//   https://github.com/<your-github>/MemoBot
//
// Copyright (c) 2026 Samuel Barabé
//
// Licensed under the MIT License.
// See the LICENSE file in the project root for license information.
//
//=====================================================================

//=====================================================================
// Firmware Architecture
//=====================================================================
//
// Compile-time hardware configuration
//   ├── Power-hold configuration
//   ├── Battery sensing configuration
//   └── Buzzer output configuration
//
// Main loop
//   ├── Update inputs
//   ├── Update buzzer
//   ├── Update tune player
//   ├── Update battery manager
//   ├── Check automatic power-off
//   └── Run the active system state
//
// System states
//   BOOT
//     ↓
//   MENU
//     ↓
//   MEMORY GAME
//     ↓
//   VICTORY / GAME OVER
//     ↓
//   MENU
//
// Power management can interrupt any state and transition to
// LOW BATTERY or POWER DOWN.
//=====================================================================

//=====================================================================
// Includes
//=====================================================================

#include <Arduino.h>

#include <SBK_Button.h>
#include <SBK_AvrBuzzer.h>
#include <SBK_Tune.h>
#include <SBK_TunePlayer.h>
#include "MemoBotTunes.h"

//=====================================================================
// Compile-Time Hardware Configuration
//=====================================================================

// Power-hold feature
//
// POWER_HOLD_ENABLED
//   Use with a hardware power-latch circuit.
//   The Green button is active HIGH with an external pull-down.
//   Long-press and inactivity shutdown are enabled.
//
// POWER_HOLD_DISABLED
//   Use when the circuit is powered continuously.
//   The Green button is active LOW with the internal pull-up.
//   Software shutdown behavior is disabled.
#define POWER_HOLD_DISABLED 0
#define POWER_HOLD_ENABLED 1

// ******** Set HERE ********
#define MEMOBOT_POWER_HOLD POWER_HOLD_ENABLED

// Battery sensing method
//
// BATTERY_SENSE_DISABLED
//   No battery voltage measurement is performed.
//   All battery-related features are disabled, including the battery indicator LEDs and low-battery shutdown.
//
// BATTERY_SENSE_VCC
//   Measures the MCU supply voltage using the internal 1.1 V reference.
//   Use when the MCU is powered directly from the battery.
//
// BATTERY_SENSE_DIVIDER
//   Measures an external battery connected to PIN_BAT_ADC through
//   a resistor divider. Suitable when the MCU is powered through
//   a regulator, such as on an Arduino module.
#define BATTERY_SENSE_DISABLED 0
#define BATTERY_SENSE_VCC 1
#define BATTERY_SENSE_DIVIDER 2

// ******** Set HERE ********
#define MEMOBOT_BATTERY_SENSE BATTERY_SENSE_VCC

// Resistor divider values (ohms).
//
// Recommended universal values:
//   R_TOP    = 470 kΩ
//   R_BOTTOM = 100 kΩ
//
// These values are suitable for:
//   - 3.3 V and 5 V MCU boards
//   - 3×AAA batteries
//   - 9 V batteries
//   - Nominal 12 V batteries up to 15 V
//
// Recommended hardware:
//   Add a 100 nF capacitor from PIN_BAT_ADC to GND.
//   Use 1% tolerance resistors for better accuracy.
constexpr uint32_t BATTERY_DIVIDER_R_TOP_OHMS = 470000;
constexpr uint32_t BATTERY_DIVIDER_R_BOTTOM_OHMS = 100000;

// Divider design limits.
//
// The divider must safely measure up to 15 V, allowing for a nominal
// 12 V battery while charging.
//
// Limit the ADC input to 3.0 V instead of the full 3.3 V supply,
// providing measurement and resistor-tolerance margin.
constexpr uint32_t BATTERY_SENSE_MAX_INPUT_MV = 15000;
constexpr uint32_t BATTERY_SENSE_MAX_ADC_MV = 3000;

// Buzzer output configuration
//
// BUZZER_OUTPUT_SINGLE_ENDED
//   Uses one MCU pin to drive the buzzer.
//   Connect the buzzer between PIN_BUZZER1 and GND.
//   PIN_BUZZER2 is unused.
//
// BUZZER_OUTPUT_DIFFERENTIAL
//   Uses two MCU pins driven with opposite polarities.
//   Connect the buzzer between PIN_BUZZER1 and PIN_BUZZER2.
//   Provides greater voltage swing and increased sound volume.
#define BUZZER_OUTPUT_SINGLE_ENDED 0
#define BUZZER_OUTPUT_DIFFERENTIAL 1

// ******** Set HERE ********
#define MEMOBOT_BUZZER_OUTPUT BUZZER_OUTPUT_DIFFERENTIAL

// Validate compile-time settings.
// These checks prevent invalid configurations from compiling.

#if MEMOBOT_POWER_HOLD != POWER_HOLD_DISABLED && \
    MEMOBOT_POWER_HOLD != POWER_HOLD_ENABLED
#error "Invalid MEMOBOT_POWER_HOLD configuration"
#endif

#if MEMOBOT_BATTERY_SENSE != BATTERY_SENSE_DISABLED && \
    MEMOBOT_BATTERY_SENSE != BATTERY_SENSE_VCC &&      \
    MEMOBOT_BATTERY_SENSE != BATTERY_SENSE_DIVIDER
#error "Invalid MEMOBOT_BATTERY_SENSE configuration"
#endif

#if MEMOBOT_BATTERY_SENSE == BATTERY_SENSE_DIVIDER
static_assert(BATTERY_DIVIDER_R_TOP_OHMS > 0,
              "BATTERY_DIVIDER_R_TOP_OHMS must be greater than zero.");
static_assert(BATTERY_DIVIDER_R_BOTTOM_OHMS > 0,
              "BATTERY_DIVIDER_R_BOTTOM_OHMS must be greater than zero.");
constexpr uint32_t BATTERY_SENSE_ADC_AT_MAX_MV =
    BATTERY_SENSE_MAX_INPUT_MV *
    BATTERY_DIVIDER_R_BOTTOM_OHMS /
    (BATTERY_DIVIDER_R_TOP_OHMS +
     BATTERY_DIVIDER_R_BOTTOM_OHMS);
static_assert(BATTERY_SENSE_ADC_AT_MAX_MV <=
                  BATTERY_SENSE_MAX_ADC_MV,
              "Battery divider output is too high for a 3.3 V ADC.");
#endif

#if MEMOBOT_BUZZER_OUTPUT != BUZZER_OUTPUT_SINGLE_ENDED && \
    MEMOBOT_BUZZER_OUTPUT != BUZZER_OUTPUT_DIFFERENTIAL
#error "Invalid MEMOBOT_BUZZER_OUTPUT configuration"
#endif

//=====================================================================
// Constants
//=====================================================================

constexpr char FIRMWARE_VERSION[] = "1.0.0";
constexpr char FIRMWARE_NAME[] = "MemoBot";

// ----- Buttons and LEDs ------

// Button LED bit masks
constexpr uint8_t LED_OFF = 0;
constexpr uint8_t LED_RED = (1 << 0);
constexpr uint8_t LED_YELLOW = (1 << 1);
constexpr uint8_t LED_BLUE = (1 << 2);
constexpr uint8_t LED_GREEN = (1 << 3);
constexpr uint8_t LED_ALL = (LED_RED | LED_YELLOW | LED_BLUE | LED_GREEN);

// Buttons pitches grouped by color, from low to high: RED, YELLOW, BLUE, GREEN
constexpr uint16_t TONE_RED_PITCH = 440;    // A5
constexpr uint16_t TONE_YELLOW_PITCH = 587; // D5
constexpr uint16_t TONE_BLUE_PITCH = 784;   // G5
constexpr uint16_t TONE_GREEN_PITCH = 880;  // A6

// Button indexes.
//
// The order of this enum is intentionally shared by:
//   - buttonLeds[]
//   - buttonTones[]
//   - Button buttons[]
//   - Button bit masks
//
// Changing this order requires updating all matching lookup tables.
enum ButtonIndex : uint8_t
{
  BTN_RED,    // RED    (440 Hz)
  BTN_YELLOW, // YELLOW (587 Hz)
  BTN_BLUE,   // BLUE   (784 Hz)
  BTN_GREEN,  // GREEN  (880 Hz)
  BTN_COUNT,
  BTN_NONE = 255 // Special value indicating no button was pressed
};

// Button bit masks.
//
// These allow multiple button states to be packed into a single byte,
// making it easy to test combinations of pressed or released buttons.
constexpr uint8_t BTN_MASK_NONE = 0;
constexpr uint8_t BTN_MASK_RED = (1 << BTN_RED);
constexpr uint8_t BTN_MASK_YELLOW = (1 << BTN_YELLOW);
constexpr uint8_t BTN_MASK_BLUE = (1 << BTN_BLUE);
constexpr uint8_t BTN_MASK_GREEN = (1 << BTN_GREEN);
constexpr uint8_t BTN_MASK_ALL = BTN_MASK_RED | BTN_MASK_YELLOW | BTN_MASK_BLUE | BTN_MASK_GREEN;

// Array of LED bit masks for each button, matching the order of the buttonTones array
constexpr uint8_t buttonLeds[BTN_COUNT] =
    {
        LED_RED,
        LED_YELLOW,
        LED_BLUE,
        LED_GREEN};

// Array of button tones, matching the order of the buttonLeds array
constexpr uint16_t buttonTones[BTN_COUNT] =
    {
        TONE_RED_PITCH,
        TONE_YELLOW_PITCH,
        TONE_BLUE_PITCH,
        TONE_GREEN_PITCH};

// Button long press duration threshold in milliseconds
constexpr uint16_t LONG_PRESS_DELAY_MS = 2000;

// ----- Battery and Power Management -----
constexpr uint32_t LOW_BATTERY_TIMEOUT_MS = 20000;
constexpr uint32_t AUTO_POWER_OFF_MS = 5UL * 60UL * 1000UL; // 5 minutes in milliseconds
constexpr uint16_t BATTERY_FULL_MV = 4500;                  // 3 x 1.5 V
constexpr uint16_t BATTERY_GOOD_MV = 3900;                  // ~1.3 V/cell
constexpr uint16_t BATTERY_LOW_MV = 3300;                   // ~1.1 V/cell
constexpr uint16_t BATTERY_CRIT_MV = 3000;                  // ~1.0 V/cell
constexpr uint16_t BATTERY_UNKNOWN_MV = 0xFFFF;             // Invalid ADC reading sentinel
constexpr uint32_t ADC_MAX = 1023;                          // Maximum ADC value for 10-bit resolution
constexpr uint8_t BATTERY_SAMPLE_COUNT = 8;                 // Number of ADC samples to average for battery voltage measurement
// Battery LED indicators bit masks
constexpr uint8_t BATT_LED_OFF = 0;
constexpr uint8_t BATT_LED_RED = (1 << 0);
constexpr uint8_t BATT_LED_YELLOW = (1 << 1);
constexpr uint8_t BATT_LED_GREEN = (1 << 2);
constexpr uint8_t BATT_LED_ALL = (BATT_LED_RED | BATT_LED_YELLOW | BATT_LED_GREEN);

// ----- System machine states enum -----
enum SystemState : uint8_t
{
  STATE_POWER_DOWN,
  STATE_LOW_BATTERY,
  STATE_BOOT,
  STATE_MENU,
  STATE_MEMORY_GAME,
  STATE_FUTURE_GAME
};

// ----- Memory Game Mode, Phase enums and constant variables -----
enum MemoryGameMode : uint8_t
{
  MODE_SOLO,
  MODE_DUEL
};

enum MemoryGamePhase : uint8_t
{
  PHASE_INTRO,         // Start the game with a short animation
  PHASE_ADD_NEXT_MOVE, // Solo Memory game = add a random move to the sequence, Duel game = wait for player to add a move to the sequence
  PHASE_PLAYBACK,      // Play back the current sequence
  PHASE_WAIT_REPEAT,   // Wait for the player to repeat the sequence
  PHASE_ROUND_SUCCESS, // Player has successfully completed the current round
  PHASE_VICTORY,       // Player has successfully completed the game
  PHASE_GAMEOVER       // Player has failed to complete the game
};

enum MemoryGamePreset : uint8_t
{
  PRESET_EASY,
  PRESET_MEDIUM,
  PRESET_HARD,
  PRESET_DUEL
};

constexpr uint8_t MEMORY_GAME_MAX_GAME_LENGTH = 32; // Maximum number of moves in the memory game sequence
// constexpr uint16_t PLAYBACK_PAUSE_MS = 150;
constexpr uint16_t MEMORY_GAME_VICTORY_TIMEOUT_MS = 20000; // Timeout for the victory phase before returning to the menu

enum MemoryGameInputResult : uint8_t
{
  INPUT_WAITING,
  INPUT_ADDED,
  INPUT_TIMEOUT
};

// ---- Animation and sound effects ----
constexpr ButtonIndex ASCENDING_TONES[] =
    {
        BTN_RED,
        BTN_YELLOW,
        BTN_BLUE,
        BTN_GREEN};

constexpr ButtonIndex DESCENDING_TONES[] =
    {
        BTN_GREEN,
        BTN_BLUE,
        BTN_YELLOW,
        BTN_RED};

//=====================================================================
// ATmega328PB (TQFP-32) Pin Reference
//=====================================================================
//
// Physical  AVR Port  Arduino
// --------  --------  -------
//  1        PD3       D3
//  2        PD4       D4
//  3        GND
//  4        VCC
//  5        GND
//  6        XTAL2/PB7
//  7        XTAL1/PB6
//  8        PD5       D5
//  9        PD6       D6
// 10        PD7       D7
// 11        PB0       D8
// 12        PB1       D9
// 13        PB2       D10
// 14        PB3       D11 (MOSI)
// 15        PB4       D12 (MISO)
// 16        PB5       D13 (SCK)
// 17        AVCC
// 18        ADC6
// 19        AREF
// 20        ADC7
// 21        PE3
// 22        PE2
// 23        PC0       A0
// 24        PC1       A1
// 25        PC2       A2
// 26        PC3       A3
// 27        PC4       A4
// 28        PC5       A5
// 29        PC6       RESET
// 30        PD0       D0
// 31        PD1       D1
// 32        PD2       D2
//

//=====================================================================
// Pin Assignments
//=====================================================================

// ----- Button inputs  -----
#define PIN_BTN_BE A1 // PC1 - Blue Button, active LOW - internal pull-up
#define PIN_BTN_YW A2 // PC2 - Yellow Button, active LOW - internal pull-up
#define PIN_BTN_RD A3 // PC3 - Red Button, active LOW - internal pull-up
// Green button.
//
// This button also controls the hardware power latch.
// Unlike the other buttons, it is wired as active HIGH with an
// external pull-down resistor so it can wake the MCU from power-off.
#define PIN_BTN_GR A4 // PC4 - Green Button / Power-On Button, active HIGH - external pull-down

// ----- Button LEDs -----
#define PIN_BTN_LED_BE 2 // PD2 - Blue LED
#define PIN_BTN_LED_YW 3 // PD3 - Yellow LED
#define PIN_BTN_LED_GR 4 // PD4 - Green LED
#define PIN_BTN_LED_RD 5 // PD5 - Red LED

// ----- Battery Voltage Sense -----
// Reserved for future firmware versions.
#define PIN_BAT_ADC A5 // PC5

// ----- Battery Level LEDs -----
#define PIN_BAT_LED_GR 8  // PB0 - Green
#define PIN_BAT_LED_YW 9  // PB1 - Yellow
#define PIN_BAT_LED_RD 10 // PB2 - Red

// ----- Buzzer -----
#define PIN_BUZZER1 7 // PD7 - SINGLE_ENDED 1-pin mode / Positive
#define PIN_BUZZER2 6 // PD6 - DIFFERENTIAL 2-pin mode

// ----- Auto Power-Off Latch -----
#define PIN_POWER_HOLD A0 // PC0 - Keeps power enabled

//=====================================================================
// Global Variables
//=====================================================================

// ----- Buttons -----
//
// Button objects, indexed by ButtonIndex.
//
// The order of this array must match:
//   - ButtonIndex
//   - buttonLeds[]
//   - buttonTones[]
//
// This allows the firmware to use the same button index for all
// related lookup tables without additional mapping.
Button buttons[BTN_COUNT] = {
    {PIN_BTN_RD, ButtonWiring::INTERNAL_PULLUP}, // Active LOW - internal pull-up
    {PIN_BTN_YW, ButtonWiring::INTERNAL_PULLUP}, // Active LOW - internal pull-up
    {PIN_BTN_BE, ButtonWiring::INTERNAL_PULLUP}, // Active LOW - internal pull-up
#if MEMOBOT_POWER_HOLD == POWER_HOLD_ENABLED
    {PIN_BTN_GR, ButtonWiring::EXTERNAL_PULLDOWN} // Active HIGH - external pull-down
#else
    {PIN_BTN_GR, ButtonWiring::INTERNAL_PULLUP} // Active LOW - internal pull-up
#endif
};

// ----- Buzzer -----
//
// The buzzer output mode is configured in the
// Compile-Time Hardware Configuration section.
//
// The buzzer driver supports two output modes:
//
//   SINGLE_ENDED - One MCU pin drives the buzzer.
//                  The buzzer negative terminal is connected to GND.
//                  PIN_BUZZER2 is ignored.
//
//   DIFFERENTIAL - Both MCU pins drive the buzzer with opposite
//                  polarities, increasing the voltage swing and
//                  output volume.
//
// PIN_BUZZER1 is always the primary output.
// In differential mode, PIN_BUZZER2 provides the complementary output.
#if MEMOBOT_BUZZER_OUTPUT == BUZZER_OUTPUT_SINGLE_ENDED
constexpr OutputMode MEMOBOT_BUZZER_OUTPUT_MODE =
    OutputMode::SINGLE_ENDED;
#elif MEMOBOT_BUZZER_OUTPUT == BUZZER_OUTPUT_DIFFERENTIAL
constexpr OutputMode MEMOBOT_BUZZER_OUTPUT_MODE =
    OutputMode::DIFFERENTIAL;
#else
#error "Invalid MEMOBOT_BUZZER_OUTPUT configuration"
#endif

Buzzer buzzer = {PIN_BUZZER1, PIN_BUZZER2, MEMOBOT_BUZZER_OUTPUT_MODE};

// Tune player for melodies and sound effects.
//
// The TunePlayer class manages the playback of tunes using the Buzzer driver.
// It handles the timing of notes, durations, and transitions between notes,
// allowing for non-blocking playback of melodies and sound effects.
//
// The TunePlayer is updated in the main loop to ensure smooth playback.
//
// Note:
// the buzzer instance should be initialized before creating the TunePlayer instance.
TunePlayer tunePlayer(buzzer);

// ----- Battery and Power Management -----
//
// Shared runtime state used by the battery monitor and
// automatic power management.
uint16_t batteryMv = 0;
uint32_t lastUserActivityTime = 0;
uint32_t lowBatteryTime = 0;

// ----- Timing -----
//
// Shared system timing values updated every loop iteration.
uint32_t currentMillis = 0;
uint32_t stateStartTime = 0;

// ----- System Mode -----
SystemState state = STATE_BOOT; // Current system mode at startup is BOOT
// SystemState previousState = STATE_BOOT; // Previous system mode at startup is BOOT

// ----- Memory Game -----

// Current game mode and phase
MemoryGamePhase memoryGamePhase = PHASE_INTRO;

// Current game sequence.
uint8_t memoryGameSequence[MEMORY_GAME_MAX_GAME_LENGTH]; // Array to store the sequence of moves in the memory game
uint8_t memoryGameSequenceLength = 0;                    // Current stored sequence length (number of moves) in the memory game

// Runtime progress.
//
// Tracks playback position and the player's progress through
// the current sequence.
uint8_t memoryGamePlayerIndex = 0;
uint32_t memoryGameLastInputTime = 0;

// Game timing.
uint32_t memoryGameTimer = 0; // Timer for the memory game, used to track the time limit for player input

// Preset parameters for each game mode.
//
// Each preset defines:
//   - game mode (Solo or Duel)
//   - number of rounds required to win
//   - playback speed
//   - player input timeout
//
// The selected preset determines which settings-table entry is used
// while the game is running.
MemoryGamePreset gamePreset = PRESET_EASY;
struct MemoryGameSettings
{
  MemoryGameMode mode;
  uint8_t roundsToWin;
  uint16_t playbackSpeed;
  uint16_t entryTimeout;
};
constexpr MemoryGameSettings memoryGameSettings[] =
    {
        // mode, rounds, speed, timeout
        {MODE_SOLO, 6, 255, 5000},                          // Easy
        {MODE_SOLO, 13, 200, 3000},                         // Medium
        {MODE_SOLO, 20, 150, 3000},                         // Hard
        {MODE_DUEL, MEMORY_GAME_MAX_GAME_LENGTH, 200, 3000} // Duel
};

//=====================================================================
// Function Prototypes
//
// Functions are grouped by subsystem rather than alphabetically.
// This provides a quick overview of the firmware architecture.
//
// Function definitions follow the main loop in the same order.
//=====================================================================

// System State Functions
void startPowerDownState();
void updatePowerDownState();
void startLowBatteryState();
void updateLowBatteryState();
void startBootState();
void startMenuState();
void updateMenuState();
void startMemoryGameState(MemoryGamePreset preset);
void updateMemoryGameState();
void startFutureGameState();
void updateFutureGameState();

// Memory Game Phase functions
void memoryGameStartIntro();
void memoryGameStartAddNextMove();
void memoryGameUpdateAddNextMove();
void memoryGameStartPlayback();
void memoryGameStartWaitRepeat();
void memoryGameUpdateWaitRepeat();
void memoryGameStartRoundSuccess();
void memoryGameStartVictory();
void memoryGameUpdateVictory();
void memoryGameStartGameOver();

// Memory Game Helper functions
void seedRandomOnce();
void memoryGameAddRandomMove();
MemoryGameInputResult memoryGameAddPlayerMove();

// Sound effects and animations
void playToneAndWait(uint16_t tone, uint16_t duration);                         // Blocking function
void playChirpAndWait(uint16_t startTone, uint16_t endTone, uint16_t duration); // Blocking function
void playPowerDownAnimation();                                                  // Blocking function
void playBootAnimation();                                                       // Blocking function
void runMenuAnimation();
void playMemoryGameStartAnimation();                                                   // Blocking function
void playMemoryGameSequence();                                                         // Blocking function
void playVictoryAnimation();                                                           // Blocking function
void playGameOverAnimation();                                                          // Blocking function
void playToneSequence(const ButtonIndex sequence[], uint8_t count, uint16_t stepTime); // Blocking function
void runTuneAnimation();

// Input feedback functions
void playUnmuteFeedback();                                      // Blocking function
void playMuteFeedback();                                        // Blocking function
void playButtonFeedback(ButtonIndex button, uint16_t duration); // Blocking function
void playRoundCompletedFeedback();                              // Blocking function
void playTimeoutFeedback();                                     // Blocking function
void playWrongEntryFeedback();                                  // Blocking function
void playLowBatteryFeedback();                                  // Blocking function

// LEDs hardware control
void setLEDs(uint8_t leds);

// Button inputs functions
uint8_t getPressedButtonsMask();
uint8_t getReleasedButtonsMask();
uint8_t getJustPressedButtonsMask();
uint8_t getJustReleasedButtonsMask();
void updateButtons();
void updateInputsAndBuzzer();

// Battery and Power Management
bool waitForAdcConversion(uint32_t timeoutUs = 1000);
uint16_t readVccMillivolts();
uint16_t readBatteryMillivolts();
void setBatteryLEDs(uint8_t leds);
void displayUnknownBatteryLevel();
void updateBatteryLeds();
void updateBatteryManager();
void checkLowBatteryPowerOff();
void checkNoActivityPowerOff();

//=====================================================================
// Setup
//
// Runs once after reset or power-up.
//
// This function initializes the hardware, performs startup checks,
// and prepares the firmware before entering the main loop.
//=====================================================================
void setup()
{

  // Keep the hardware power latch enabled.
  //
  // The Green button initially powers the MCU.
  // Once running, the firmware drives this pin HIGH so the
  // system remains powered after the button is released.
  pinMode(PIN_POWER_HOLD, OUTPUT);
  digitalWrite(PIN_POWER_HOLD, HIGH);

  // Initialize the game button LEDs.
  pinMode(PIN_BTN_LED_GR, OUTPUT);
  pinMode(PIN_BTN_LED_RD, OUTPUT);
  pinMode(PIN_BTN_LED_BE, OUTPUT);
  pinMode(PIN_BTN_LED_YW, OUTPUT);
  setLEDs(LED_OFF);
  // Initialize the battery level indicator LEDs.
  pinMode(PIN_BAT_LED_RD, OUTPUT);
  pinMode(PIN_BAT_LED_YW, OUTPUT);
  pinMode(PIN_BAT_LED_GR, OUTPUT);
  setBatteryLEDs(BATT_LED_OFF);

  // Initialize shared timing variables.
  //
  // currentMillis is refreshed every loop iteration.
  // The activity timer starts now so the auto power-off
  // countdown begins from power-up.
  currentMillis = millis();
  lastUserActivityTime = currentMillis;

  // Initialize the buzzer driver.
  buzzer.begin();

  // Initialize all button inputs.
  //
  // Each Button object configures its GPIO pin and
  // initializes its debouncing state.
  for (uint8_t i = 0; i < BTN_COUNT; i++)
  {
    buttons[i].begin();
    buttons[i].setLongPressDelay(LONG_PRESS_DELAY_MS);
  }

#if MEMOBOT_BATTERY_SENSE != BATTERY_SENSE_DISABLED
  // Measure the battery voltage before entering the main loop.
  //
  // This allows the battery indicator to display the correct
  // level immediately after power-up.
  batteryMv = readBatteryMillivolts();
  // Update the battery indicator LEDs using the measured voltage.
  updateBatteryLeds();

  // If the battery is already critically low, skip the normal
  // boot sequence and enter the low-battery warning state.
  if (batteryMv != BATTERY_UNKNOWN_MV && batteryMv <= BATTERY_CRIT_MV)
  {
    startLowBatteryState();
    return;
  }
#endif

  // Start the firmware state machine.
  startBootState();
}

//=====================================================================
// Main Loop
//
// Runs continuously after setup() completes.
//
// Each iteration first updates the shared hardware and background
// services, then runs the logic associated with the active system state.
//=====================================================================
void loop()
{

  // Capture the current system time once for this loop iteration.
  //
  // Sharing one timestamp keeps timing comparisons consistent and avoids
  // calling millis() repeatedly throughout the state-machine functions.
  currentMillis = millis();

  // Update all button objects.
  //
  // This reads the GPIO pins, performs debouncing, and generates
  // one-cycle events such as justPressed() and justReleased().
  updateButtons();

  // Seed the pseudo-random number generator from the timing of the
  // user's first button press. This function has no effect after the
  // generator has been seeded once.
  seedRandomOnce();

  // Advance any active tone or chirp.
  //
  // The buzzer uses non-blocking timing internally, so update() must
  // be called regularly to stop tones and advance chirps correctly.
  buzzer.update();

  // Advance the active melody, if one is playing.
  tunePlayer.update();

  // Periodically measure the battery voltage and update the
  // battery-level indicator LEDs.
  updateBatteryManager();

  // Reset the inactivity timer when the user presses a button,
  // or request power-down after the configured inactivity period.
  checkNoActivityPowerOff();

  // Firmware state machine
  // Run the behavior associated with the current system state.
  //
  // State functions may remain in the current state or request a
  // transition by changing the global state variable.
  switch (state)
  {
  case STATE_POWER_DOWN:
    updatePowerDownState();
    break;

  case STATE_LOW_BATTERY:
    updateLowBatteryState();
    break;

  case STATE_BOOT:
    // Play the startup animation once when entering the boot state.
    playBootAnimation(); // Blocking call

    // The Green button may still be held after powering the circuit.
    // Wait for its release so the same press cannot immediately trigger
    // another menu action or begin a game.
    while (buttons[BTN_GREEN].isPressed())
    {
      // Blocking loops must continue updating inputs and the buzzer
      // so button debouncing and sound timing remain operational.
      updateInputsAndBuzzer();
    }

    startMenuState();
    break;

  case STATE_MENU:
    updateMenuState();
    break;

  case STATE_MEMORY_GAME:
    updateMemoryGameState();
    break;

  case STATE_FUTURE_GAME:
    // Reserved for an additional game mode.
    // updateFutureGameState();
    break;
  }
}

//=====================================================================
// Function Definitions
//
// Functions are implemented in the same subsystem order as their
// prototypes. Keeping related code together makes each subsystem
// easier to understand, maintain, and extend.
//=====================================================================

// ----- State machine functions for each system state -----
//
// Every system state provides a start<State>() function so state
// transitions are performed consistently throughout the firmware.
//
// Most states are also implemented with an update<State>() function:
//
//   start<State>()  - Performs one-time actions when entering
//                     the state.
//
//   update<State>() - Runs repeatedly while the state remains active.
//
// Simple states may perform their update logic directly in the main
// loop instead of using a dedicated update<State>() function.
//
// Separating state entry from continuous execution keeps state
// transitions explicit and prevents one-time actions from being
// repeated every loop iteration.
// ---------------------------------------------------------

// Request a transition to the power-down state.
// The actual shutdown sequence is performed by
// updatePowerDownState().
void startPowerDownState()
{
  state = STATE_POWER_DOWN;
}

// Perform the power-down sequence once.
// The hardware power latch is released after the shutdown
// animation has completed.
void updatePowerDownState()
{
  static bool powerDownDone = false;

  if (!powerDownDone)
  {
    playPowerDownAnimation();          // Blocking call
    digitalWrite(PIN_POWER_HOLD, LOW); // Turn off power latch
    delay(100);                        // Wait a moment to ensure the power latch is released
    powerDownDone = true;
    return; // Exit the loop to allow the power latch to turn off the system
  }

  // This is included only to allow the system to restart when powered
  // from an external supply. It is useful for testing and debugging but
  // is not part of normal battery-powered operation.
  if (buttons[BTN_GREEN].justLongPressed())
  {
    // If the power-on button is pressed, restart the system.
    powerDownDone = false; // Reset the power down flag for the next power cycle
    startBootState();
  }
}

// Record when the warning state began so it can time out
// before automatically powering down.
void startLowBatteryState()
{
  lowBatteryTime = currentMillis;
  state = STATE_LOW_BATTERY;
}

// Periodically repeat the warning until the shutdown timeout expires.
void updateLowBatteryState()
{
  static uint32_t lastWarningTime = 0;
  constexpr uint16_t WARNING_PERIOD_MS = 3000;

  if (currentMillis - lastWarningTime >= WARNING_PERIOD_MS)
  {
    lastWarningTime = currentMillis;
    playLowBatteryFeedback(); // Blocking call
  }

  if (currentMillis - lowBatteryTime >= LOW_BATTERY_TIMEOUT_MS)
  {
    startPowerDownState();
  }
}

// Enter the boot state with all button LEDs turned off.
void startBootState()
{
  setLEDs(LED_OFF);
  state = STATE_BOOT;
}

// Enter the menu state with all button LEDs turned off.
void startMenuState()
{
  setLEDs(LED_OFF);
  state = STATE_MENU;
}

// Handles menu animation, game selection, and long-press actions.
//
// Only one button transaction is tracked at a time. A short press
// performs its action when released, while a long press performs its
// action once during the hold and consumes the following release.
void updateMenuState()
{
  static uint8_t activeButton = BTN_NONE;
  static bool longPressActionDone = false;

  runMenuAnimation();

  // 1. Start tracking exactly one button press.
  if (activeButton == BTN_NONE)
  {
    for (uint8_t i = 0; i < BTN_COUNT; i++)
    {
      if (buttons[i].justPressed())
      {
        activeButton = i;
        longPressActionDone = false;
        return;
      }
    }

    return;
  }

  // 2. Long-press action, fired once during the hold.
  if (!longPressActionDone && buttons[activeButton].justLongPressed())
  {
    longPressActionDone = true;

    buzzer.playTone(buttonTones[activeButton], 100); // Play a short tone to indicate the long press action registered
    setLEDs(LED_OFF);                                // Turn OFF the red LED to indicate the long press action

    while (buttons[activeButton].isPressed())
    {
      // Wait for the user to release the button before proceeding.
      updateInputsAndBuzzer();
    }

    switch (activeButton)
    {

    case BTN_RED:
      // Directly enter the victory phase of the memory game for tunes testing purposes.

      // Release the button transaction before leaving the menu state.
      activeButton = BTN_NONE;

      state = STATE_MEMORY_GAME;
      memoryGameStartVictory();
      return;

    case BTN_YELLOW:
      // Reserved for future long-press actions.

      // Future long-press action.
      return;

    case BTN_BLUE:
      // Mute or unmute the buzzer when the Blue button is long-pressed.

      if (buzzer.isMuted())
      {
        buzzer.unmute();
        playUnmuteFeedback();
      }
      else
      {
        playMuteFeedback();
        buzzer.mute();
      }
      return;

    case BTN_GREEN:
      // Power down the system when the Green button is long-pressed.

#if MEMOBOT_POWER_HOLD == POWER_HOLD_ENABLED
      // Release the button transaction before leaving the menu state.
      activeButton = BTN_NONE;
      startPowerDownState();
#endif
      return;

    default:
      return;
    }
  }

  // Release ends the transaction.
  // isReleased() is intentional: blocking feedback may update buttons
  // and consume the one-frame justReleased() event before we return here.
  if (activeButton != BTN_NONE && (buttons[activeButton].justReleased() || buttons[activeButton].isReleased()))
  {
    uint8_t releasedButton = activeButton;
    bool wasLongPress = longPressActionDone;

    activeButton = BTN_NONE;
    longPressActionDone = false;

    // Long press consumes the release. No game start.
    if (wasLongPress)
    {
      return;
    }

    // Normal release actions.
    switch (releasedButton)
    {
    case BTN_GREEN:
      startMemoryGameState(PRESET_EASY);
      return;

    case BTN_BLUE:
      startMemoryGameState(PRESET_MEDIUM);
      return;

    case BTN_RED:
      startMemoryGameState(PRESET_HARD);
      return;

    case BTN_YELLOW:
      startMemoryGameState(PRESET_DUEL);
      return;

    default:
      return;
    }
  }
}

// Enters the memory game state using the selected preset.
//
// This clears the button LEDs, selects the game settings, resets the
// game phase, and prepares the memory-game state machine to begin.
void startMemoryGameState(MemoryGamePreset preset)
{
  setLEDs(LED_OFF);
  state = STATE_MEMORY_GAME;
  memoryGamePhase = PHASE_INTRO;
  gamePreset = preset;
  memoryGameStartIntro();
}

// Runs the phase state machine for the active memory game.
//
// Each phase either performs its current action or transitions to the
// next phase by calling the corresponding memoryGameStart... function.
void updateMemoryGameState()
{
  // Dispatch execution to the active game phase.
  switch (memoryGamePhase)
  {
  case PHASE_INTRO:
    playMemoryGameStartAnimation(); // Blocking call
    memoryGameStartAddNextMove();
    break;

  case PHASE_ADD_NEXT_MOVE:
    memoryGameUpdateAddNextMove();
    break;

  case PHASE_PLAYBACK:
    playMemoryGameSequence(); // Blocking call
    memoryGameStartWaitRepeat();
    break;

  case PHASE_WAIT_REPEAT:
    memoryGameUpdateWaitRepeat();
    break;

  case PHASE_ROUND_SUCCESS:
    memoryGameStartAddNextMove();
    break;

  case PHASE_VICTORY:
    memoryGameUpdateVictory();
    break;

  case PHASE_GAMEOVER:
    delay(300);              // Short delay before starting the game over animation
    playGameOverAnimation(); // Blocking call
    startMenuState();        // Return to the menu after the game over animation
    break;
  }
}

// Placeholder entry function for a future game mode.
void startFutureGameState()
{
  // state = STATE_FUTURE_GAME;
  // setLEDs(LED_OFF);
}

// Placeholder for future game mode logic.
void updateFutureGameState()
{
  // This function can be implemented when the new game mode is added.
}

// ----- Memory Game Phase Functions -----
//
// The Memory Game is implemented as a phase state machine.
//
// Each phase provides:
//
//   memoryGameStart<Phase>()  - Performs one-time actions when
//                               entering the phase.
//
//   memoryGameUpdate<Phase>() - Runs repeatedly while the phase
//                               remains active.
//
// Simple phases that only perform immediate actions do not require
// a dedicated update function.
// ---------------------------------------------------------

void memoryGameStartIntro()
{
  memoryGameSequenceLength = 0;    // Reset the stored sequence length for a new game
  memoryGameTimer = currentMillis; // Reset the game timer for a new game
  memoryGamePhase = PHASE_INTRO;
}

void memoryGameStartAddNextMove()
{
  memoryGamePhase = PHASE_ADD_NEXT_MOVE;
}

// Advance the game by adding the next move.
//
// Solo mode generates a random move.
// Duel mode waits for the current player to enter one.
void memoryGameUpdateAddNextMove()
{
  if (memoryGameSequenceLength >= memoryGameSettings[gamePreset].roundsToWin)
  {
    memoryGameStartVictory();
    return;
  }
  if (memoryGameSettings[gamePreset].mode == MODE_SOLO)
  {
    memoryGameAddRandomMove();
    delay(memoryGameSettings[gamePreset].playbackSpeed); // Short delay before starting playback
    memoryGameStartPlayback();
    return;
  }

  // Duel mode: wait for player input to add a new move to the sequence
  MemoryGameInputResult result = memoryGameAddPlayerMove();
  switch (result)
  {
  case INPUT_WAITING:
    // Still waiting for player input, do nothing.
    return;
  case INPUT_ADDED:
    // Player has added a new button to the sequence, proceed to next player repeat phase.
    playRoundCompletedFeedback(); // Blocking call
    memoryGameStartWaitRepeat();
    return;
  case INPUT_TIMEOUT:
    // Player took too long to input a button, game over.
    playTimeoutFeedback(); // Blocking call
    memoryGameStartGameOver();
    return;
  }
}

void memoryGameStartPlayback()
{
  memoryGamePhase = PHASE_PLAYBACK;
}

// Prepare for player inputs.
void memoryGameStartWaitRepeat()
{
  memoryGamePhase = PHASE_WAIT_REPEAT;
  memoryGameTimer = currentMillis; // Reset the game timer for a new wait repeat phase
  memoryGamePlayerIndex = 0;
  memoryGameLastInputTime = currentMillis;
}

// Wait for the player to repeat the sequence of moves.
//
// Verify each button press against the expected sequence and handle timeouts or incorrect inputs.
// Correct inputs advance through the sequence.
// An incorrect input or timeout immediately ends the game.
// When full sequence is repeated successfully, transition to the next round or victory phase.
void memoryGameUpdateWaitRepeat()
{
  static bool waitingForRelease = false;

  if (currentMillis - memoryGameLastInputTime >= memoryGameSettings[gamePreset].entryTimeout)
  {
    setLEDs(LED_OFF);
    memoryGamePlayerIndex = 0;
    memoryGameLastInputTime = 0;
    waitingForRelease = false;
    playTimeoutFeedback();                               // Blocking call
    delay(memoryGameSettings[gamePreset].playbackSpeed); // Short delay before starting game over
    memoryGameStartGameOver();
    return;
  }

  // Wait until all buttons are released before accepting another press.
  if (waitingForRelease)
  {
    if (getReleasedButtonsMask() == BTN_MASK_ALL)
    {
      waitingForRelease = false;
    }
    return;
  }

  for (uint8_t i = 0; i < BTN_COUNT; i++)
  {

    if (!buttons[i].justPressed())
    {
      continue;
    }

    ButtonIndex pressedButton = static_cast<ButtonIndex>(i);
    waitingForRelease = true;

    // Wrong button, immediately end the game and start the game over phase.
    if (pressedButton != memoryGameSequence[memoryGamePlayerIndex])
    {
      setLEDs(LED_OFF);
      memoryGamePlayerIndex = 0;
      memoryGameLastInputTime = 0;
      playWrongEntryFeedback();                            // Blocking call
      delay(memoryGameSettings[gamePreset].playbackSpeed); // Short delay before starting game over
      memoryGameStartGameOver();
      return;
    }

    // Immediate feedback
    playButtonFeedback(pressedButton, memoryGameSettings[gamePreset].playbackSpeed); // Blocking call

    // Correct button
    memoryGamePlayerIndex++;
    memoryGameLastInputTime = currentMillis; // reset timeout after each correct move

    // Full sequence repeated successfully
    if (memoryGamePlayerIndex >= memoryGameSequenceLength)
    {
      memoryGamePlayerIndex = 0;
      memoryGameLastInputTime = 0;
      waitingForRelease = false;
      memoryGameStartRoundSuccess();
    }
    return; // Wait for the next button press
  }
}

// Transition to the round success phase after the player has successfully completed the current round.
void memoryGameStartRoundSuccess()
{
  memoryGamePhase = PHASE_ROUND_SUCCESS;
  // Additional logic for starting the round success phase can be added here
}

void memoryGameStartVictory()
{
  memoryGamePhase = PHASE_VICTORY;
  playVictoryAnimation();         // Blocking call
  stateStartTime = currentMillis; // Record the time when the victory phase starts
  //tunePlayer.play(TUNE_MEMOBOT_VICTORY);  // Start playing the victory tune
  tunePlayer.playRandom(MEMOBOT_TUNES, MEMOBOT_TUNE_COUNT); // Start playing a random tune
}

// Keep the victory sequence running until the player exits or
// the timeout expires.
void memoryGameUpdateVictory()
{
  const bool timeout = (currentMillis - stateStartTime >= MEMORY_GAME_VICTORY_TIMEOUT_MS);
  const bool buttonReleased = (getJustReleasedButtonsMask() != BTN_MASK_NONE);

  // Exit to the menu when the player presses a button.
  if (buttonReleased)
  {
    tunePlayer.stop();
    startMenuState();
    return;
  }

  // Wait for the current tune to finish before timing out.
  if (!tunePlayer.isPlaying())
  {
    if (timeout)
    {
      startMenuState();
      return;
    }

    // Restart the victory tune.
    tunePlayer.repeat();
  }

  // Animate the LEDs while the tune is playing.
  runTuneAnimation();
}

void memoryGameStartGameOver()
{
  memoryGamePhase = PHASE_GAMEOVER;
  // Additional logic for starting the game over phase can be added here
}

// ----- Memory Game Helper Functions -----
//
// Utility functions used by the memory game to initialize randomness,
// add moves to the sequence, and collect a new move in Duel mode.
// ---------------------------------------------------------

// Seeds the pseudo-random number generator once using the timing of
// the player's first button press.
//
// Human reaction timing varies between power cycles, providing a less
// predictable seed than using a fixed startup value.
void seedRandomOnce()
{
  static bool randomSeeded = false;

  // Do nothing after the generator has already been seeded.
  if (randomSeeded)
  {
    return;
  }

  // Wait for the first button press so its timing can contribute
  // unpredictable information to the seed.
  uint8_t pressedButtons = getJustPressedButtonsMask();

  if (pressedButtons == BTN_MASK_NONE)
  {
    return; // Wait until the first button is pressed to seed the random number generator
  }

  // Combine several runtime values to produce the seed.
  uint32_t seed = currentMillis;
  seed ^= batteryMv;
  seed ^= pressedButtons;

  randomSeed(seed);
  randomSeeded = true;
}

// Generates one random button and appends it to the game sequence.
//
// memoryGameSequenceLength represents both the current sequence length and
// the index where the next move must be stored.
void memoryGameAddRandomMove()
{
  // random() includes the minimum value but excludes the maximum,
  // producing a valid button index from 0 through BTN_COUNT - 1.
  uint8_t newButton = random(0, BTN_COUNT); // min (included), max (exluded)

  // Store the new button at the end of the current sequence.
  memoryGameSequence[memoryGameSequenceLength] = newButton;

  // Increase the sequence length for the next round.
  memoryGameSequenceLength++;
}

// Collects one player-selected move for the Duel game sequence.
//
// This function is called repeatedly while the game remains in the
// add-next-move phase. Static variables preserve its progress between
// calls.
//
// Returns:
//   INPUT_WAITING - No completed input yet.
//   INPUT_ADDED   - A new move was added to the sequence.
//   INPUT_TIMEOUT - The player did not respond in time.
MemoryGameInputResult memoryGameAddPlayerMove()
{
  static bool initialized = false;
  static uint32_t startTime = 0;
  static ButtonIndex newButton = BTN_NONE;

  // Initialize this input transaction on its first call.
  if (!initialized)
  {
    initialized = true;
    startTime = currentMillis;
    newButton = BTN_NONE;
    setLEDs(LED_OFF);
  }

  // End the transaction if the player takes too long to respond.
  if ((currentMillis - startTime) >= memoryGameSettings[gamePreset].entryTimeout) // Loop until too much time has passed
  {
    initialized = false;
    setLEDs(LED_OFF);
    return INPUT_TIMEOUT;
  }

  // Capture the first newly pressed button.
  if (newButton == BTN_NONE)
  {
    for (uint8_t i = 0; i < BTN_COUNT; i++)
    {
      if (buttons[i].justPressed())
      {
        newButton = static_cast<ButtonIndex>(i);
        playButtonFeedback(newButton, memoryGameSettings[gamePreset].playbackSpeed); // Blocking call: play the button that was pressed by the user
        break;
      }
    }
  }

  // Store the move after its feedback has completed.
  if (newButton != BTN_NONE)
  {
    memoryGameSequence[memoryGameSequenceLength++] = newButton;
    setLEDs(LED_OFF);
    initialized = false;
    return INPUT_ADDED; // Player has added a new button to the sequence
  }

  return INPUT_WAITING; // Still waiting for player input
}

// ----- Animation Functions -----
//
// Animation functions follow two execution models:
//
//   play<Animation>() - Blocking.
//                      The function does not return until the entire
//                      animation, or the current animation phase,
//                      has completed.
//
//   run<Animation>()  - Non-blocking.
//                      Advances the animation one step each time it
//                      is called and returns immediately. Intended
//                      to be called repeatedly from the main loop.
//
// Some blocking animations continue servicing the buzzer and button
// inputs while waiting, allowing sound playback and button debouncing
// to remain responsive when required.
// ---------------------------------------------------------

// Plays a tone and waits for it to finish.
//
// While waiting, button inputs and the buzzer are continuously updated
// so input events and timing remain responsive.
void playToneAndWait(uint16_t tone, uint16_t duration) // Blocking function
{
  buzzer.playTone(tone, duration);

  while (buzzer.isPlaying())
  {
    updateInputsAndBuzzer();
  }
}

// Plays a chirp and waits for it to finish.
//
// While waiting, button inputs and the buzzer are continuously updated
// so input events and timing remain responsive.
void playChirpAndWait(uint16_t startTone, uint16_t endTone, uint16_t duration) // Blocking function
{
  buzzer.playChirp(startTone, endTone, duration);

  while (buzzer.isPlaying())
  {
    updateInputsAndBuzzer();
  }
}

void playPowerDownAnimation() // Blocking function
{
  setLEDs(LED_ALL);
  playToneAndWait(TONE_GREEN_PITCH, 70);

  setLEDs(LED_RED | LED_YELLOW | LED_BLUE);
  playToneAndWait(TONE_BLUE_PITCH, 70);

  setLEDs(LED_RED | LED_YELLOW);
  playToneAndWait(TONE_YELLOW_PITCH, 90);

  setLEDs(LED_RED);
  playToneAndWait(TONE_RED_PITCH, 120);

  setLEDs(LED_OFF);
}

void playBootAnimation() // Blocking function
{
  setLEDs(LED_RED);
  playToneAndWait(TONE_RED_PITCH, 120);

  setLEDs(LED_RED | LED_YELLOW);
  playToneAndWait(TONE_YELLOW_PITCH, 90);

  setLEDs(LED_RED | LED_YELLOW | LED_BLUE);
  playToneAndWait(TONE_BLUE_PITCH, 70);

  setLEDs(LED_ALL);
  playToneAndWait(TONE_GREEN_PITCH, 70);

  delay(100); // Short delay with all LEDS on.
  setLEDs(LED_OFF);
}

// Runs the menu attract animation.
//
// The animation pauses while a button is held so the pressed button
// can provide immediate visual and audio feedback.
void runMenuAnimation()
{

  for (uint8_t i = 0; i < BTN_COUNT; i++)
  {
    if (buttons[i].isPressed())
    {
      setLEDs(buttonLeds[i]);

      if (buttons[i].justPressed()) // Your naming: just pressed
      {
        buzzer.playTone(buttonTones[i], 100); // Play the corresponding tone for 100 ms
      }

      return; // Pause menu shuffle while a button is held
    }
  }

  static uint8_t activeCase = 0;
  static uint32_t lastUpdateTime = 0;
  constexpr uint16_t STEP_TIME = 250;

  // Crossed LEDs like animation...
  constexpr uint8_t menuShuffleLeds[] =
      {LED_RED,
       LED_BLUE,
       LED_GREEN,
       LED_YELLOW};

  constexpr uint8_t MENU_SHUFFLE_COUNT =
      sizeof(menuShuffleLeds) / sizeof(menuShuffleLeds[0]);

  if (currentMillis - lastUpdateTime >= STEP_TIME)
  {
    lastUpdateTime = currentMillis;
    activeCase++;
    if (activeCase >= MENU_SHUFFLE_COUNT)
    {
      activeCase = 0;
    }
  }

  setLEDs(menuShuffleLeds[activeCase]);
}

// Plays a short preset-specific animation before the first round.
void playMemoryGameStartAnimation() // Blocking function
{
  switch (gamePreset)
  {
  case PRESET_EASY:
    setLEDs(LED_ALL);
    delay(150);
    setLEDs(LED_RED | LED_YELLOW | LED_BLUE);
    delay(150);
    setLEDs(LED_ALL);
    delay(150);
    setLEDs(LED_RED | LED_YELLOW | LED_BLUE);
    delay(150);
    setLEDs(LED_ALL);
    delay(350);
    setLEDs(LED_OFF);
    delay(350);
    break;
  case PRESET_MEDIUM:
    setLEDs(LED_ALL);
    delay(150);
    setLEDs(LED_RED | LED_YELLOW | LED_GREEN);
    delay(150);
    setLEDs(LED_ALL);
    delay(150);
    setLEDs(LED_RED | LED_YELLOW | LED_GREEN);
    delay(150);
    setLEDs(LED_ALL);
    delay(350);
    setLEDs(LED_OFF);
    delay(350);
    break;
  case PRESET_HARD:
    setLEDs(LED_ALL);
    delay(150);
    setLEDs(LED_YELLOW | LED_BLUE | LED_GREEN);
    delay(150);
    setLEDs(LED_ALL);
    delay(150);
    setLEDs(LED_YELLOW | LED_BLUE | LED_GREEN);
    delay(150);
    setLEDs(LED_ALL);
    delay(350);
    setLEDs(LED_OFF);
    delay(350);
    break;
  case PRESET_DUEL:
    setLEDs(LED_ALL);
    delay(150);
    setLEDs(LED_RED | LED_BLUE | LED_GREEN);
    delay(150);
    setLEDs(LED_ALL);
    delay(150);
    setLEDs(LED_RED | LED_BLUE | LED_GREEN);
    delay(150);
    setLEDs(LED_ALL);
    delay(350);
    setLEDs(LED_OFF);
    delay(350);
  }
}

// Plays the complete memory sequence from beginning to end.
//
// Each stored button is replayed using the current playback speed.
void playMemoryGameSequence() // Blocking function
{
  for (uint8_t i = 0; i < memoryGameSequenceLength; i++)
  {
    // Play a pause between button feedbacks to make the sequence easier to follow.
    // delay(PLAYBACK_PAUSE_MS);
    delay(memoryGameSettings[gamePreset].playbackSpeed / 2); // Shorter pause based on playback speed

    // Play the feedback for the current button in the sequence.
    ButtonIndex currentButton = static_cast<ButtonIndex>(memoryGameSequence[i]);
    playButtonFeedback(currentButton, memoryGameSettings[gamePreset].playbackSpeed);
  };

  setLEDs(LED_OFF);
}

// Plays a sequence of button tones without LED feedback.
void playToneSequence(const ButtonIndex sequence[], uint8_t count, uint16_t stepTime) // Blocking function
{
  for (uint8_t i = 0; i < count; i++)
  {
    playToneAndWait(buttonTones[sequence[i]], stepTime);
  }
}

void playVictoryAnimation() // Blocking function
{
  constexpr uint16_t CHIRP_START = 2000;
  constexpr uint16_t CHIRP_END = 7000;
  constexpr uint16_t CHIRP_TIME = 175;
  constexpr uint16_t END_PAUSE = 350;

  setLEDs(LED_BLUE | LED_GREEN);
  playChirpAndWait(CHIRP_START, CHIRP_END, CHIRP_TIME);

  setLEDs(LED_RED | LED_YELLOW);
  playChirpAndWait(CHIRP_START, CHIRP_END, CHIRP_TIME);

  setLEDs(LED_BLUE | LED_GREEN);
  playChirpAndWait(CHIRP_START, CHIRP_END, CHIRP_TIME);

  setLEDs(LED_RED | LED_YELLOW);
  playChirpAndWait(CHIRP_START, CHIRP_END, CHIRP_TIME);

  setLEDs(LED_OFF);
  delay(END_PAUSE);
}

void playGameOverAnimation() // Blocking function
{
  constexpr uint16_t STEP_DUR = 115;

  setLEDs(LED_RED | LED_BLUE);
  playToneAndWait(NOTE_F3, STEP_DUR);
  delay(STEP_DUR);

  setLEDs(LED_YELLOW | LED_GREEN);
  playToneAndWait(NOTE_DS3, STEP_DUR);
  delay(STEP_DUR);

  setLEDs(LED_RED | LED_BLUE);
  playToneAndWait(NOTE_D3, STEP_DUR);
  delay(STEP_DUR);

  setLEDs(LED_YELLOW | LED_GREEN);
  playToneAndWait(NOTE_C3, STEP_DUR * 4);
  delay(STEP_DUR);

  setLEDs(LED_OFF);
}

// Synchronizes random LED flashes with the currently playing tune.
//
// The displayed LED changes only when a new note begins. Rest notes
// turn all LEDs off.
void runTuneAnimation()
{

  constexpr uint8_t INVALID_NOTE_INDEX = 255;

  static uint8_t lastNoteIndex = INVALID_NOTE_INDEX;
  static uint8_t lastLedIndex = 0;

  if (!tunePlayer.isPlaying())
  {
    lastNoteIndex = INVALID_NOTE_INDEX;
    setLEDs(LED_OFF);
    return;
  }

  uint8_t noteIndex = tunePlayer.getCurrentNoteIndex();

  if (noteIndex == lastNoteIndex)
  {
    return;
  }

  lastNoteIndex = noteIndex;

  if (tunePlayer.getCurrentNotePitch() == NOTE_REST)
  {
    setLEDs(LED_OFF);
    return;
  }

  // Pick a random button LED.
  uint8_t ledIndex = random(BTN_COUNT);

  // Optional: avoid repeating the same LED twice.
  if (BTN_COUNT > 1)
  {
    while (ledIndex == lastLedIndex)
    {
      ledIndex = random(BTN_COUNT);
    }
  }

  lastLedIndex = ledIndex;

  setLEDs(buttonLeds[ledIndex]);
}

// ----- Input Feedback Functions -----
//
// These functions provide immediate visual and audio feedback for
// user actions and game events.
//
// All feedback functions in this section are blocking: they return
// only after the complete feedback sequence has finished.
// ---------------------------------------------------------

// Plays an ascending tone sequence to confirm that sound is enabled.
void playUnmuteFeedback() // Blocking function
{
  constexpr uint16_t STEP_TIME = 120;

  playToneSequence(ASCENDING_TONES, sizeof(ASCENDING_TONES) / sizeof(ASCENDING_TONES[0]), STEP_TIME);
}

// Plays a descending tone sequence before sound is muted.
void playMuteFeedback() // Blocking function
{
  constexpr uint16_t STEP_TIME = 120;

  playToneSequence(DESCENDING_TONES, sizeof(DESCENDING_TONES) / sizeof(DESCENDING_TONES[0]), STEP_TIME);
}

// Provides visual and audio feedback for one button.
//
// The corresponding LED and tone remain active for the requested
// duration. Passing BTN_NONE produces a silent pause with all LEDs off.
void playButtonFeedback(ButtonIndex button, uint16_t duration) // Blocking function
{
  if (button == BTN_NONE)
  {
    setLEDs(LED_OFF);
    playToneAndWait(NOTE_REST, duration);
  }
  else
  {
    setLEDs(buttonLeds[button]);
    playToneAndWait(buttonTones[button], duration);
  }

  setLEDs(LED_OFF);
}

// Confirms that the current sequence was completed successfully.
void playRoundCompletedFeedback() // Blocking function
{
  constexpr uint16_t STEP_TIME = 75;

  setLEDs(LED_ALL);
  for (uint8_t i = 0; i < BTN_COUNT; i++)
  {
    playToneAndWait(buttonTones[i], STEP_TIME);
  }
  setLEDs(LED_OFF);
  delay(memoryGameSettings[gamePreset].playbackSpeed); // Short pause after the round completion feedback
}

// Signals an incorrect button entry with a low tone and flashing LEDs.
//
// The buzzer and LED flashing are updated together until the feedback
// duration has elapsed.
void playWrongEntryFeedback() // Blocking function
{
  constexpr uint16_t WRONG_TONE = 180;
  constexpr uint16_t BUZZ_TIME = 700;
  constexpr uint16_t FLASH_TIME = 100;

  uint32_t lastFlashTime = millis();
  bool ledsOn = false;

  buzzer.playTone(WRONG_TONE, BUZZ_TIME);

  while (buzzer.isPlaying())
  {
    updateInputsAndBuzzer();

    if (millis() - lastFlashTime >= FLASH_TIME)
    {
      lastFlashTime = millis();
      ledsOn = !ledsOn;

      setLEDs(ledsOn ? LED_ALL : LED_OFF);
    }
  }

  setLEDs(LED_OFF);
  delay(150); // Short pause after the wrong entry feedback
}

// Signals an input timeout with two short flashes and tones.
void playTimeoutFeedback() // Blocking function
{
  constexpr uint16_t TIMEOUT_TONE = 300;
  constexpr uint16_t TONE_TIME = 120;

  setLEDs(LED_ALL);
  playToneAndWait(TIMEOUT_TONE, TONE_TIME);

  setLEDs(LED_OFF);
  delay(TONE_TIME);

  setLEDs(LED_ALL);
  playToneAndWait(TIMEOUT_TONE, TONE_TIME);

  setLEDs(LED_OFF);
  delay(TONE_TIME);
}

// Signals a critically low battery with three red flashes and tones.
void playLowBatteryFeedback() // Blocking function
{
  constexpr uint16_t TONE = 220;
  constexpr uint16_t TONE_TIME = 120;
  constexpr uint16_t PAUSE_TIME = 120;

  for (uint8_t i = 0; i < 3; i++)
  {
    setBatteryLEDs(BATT_LED_RED);

    playToneAndWait(TONE, TONE_TIME);

    setBatteryLEDs(BATT_LED_OFF);

    delay(PAUSE_TIME);
  }
}

// ----- LED Control Functions -----
//
// These functions provide a hardware-independent interface for
// controlling the game button LEDs and battery indicator LEDs.
//
// LED states are specified using the corresponding LED bit masks.
// ---------------------------------------------------------

// Sets the game button LEDs using an LED bit mask.
//
// Multiple LEDs can be illuminated simultaneously by combining
// LED_RED, LED_YELLOW, LED_BLUE, and LED_GREEN with the bitwise OR
// operator.
void setLEDs(uint8_t leds)
{
  digitalWrite(PIN_BTN_LED_RD, (leds & LED_RED) ? HIGH : LOW);
  digitalWrite(PIN_BTN_LED_YW, (leds & LED_YELLOW) ? HIGH : LOW);
  digitalWrite(PIN_BTN_LED_BE, (leds & LED_BLUE) ? HIGH : LOW);
  digitalWrite(PIN_BTN_LED_GR, (leds & LED_GREEN) ? HIGH : LOW);
}

// Sets the battery indicator LEDs using a battery LED bit mask.
void setBatteryLEDs(uint8_t leds)
{
  digitalWrite(PIN_BAT_LED_GR, (leds & BATT_LED_GREEN) ? HIGH : LOW);
  digitalWrite(PIN_BAT_LED_YW, (leds & BATT_LED_YELLOW) ? HIGH : LOW);
  digitalWrite(PIN_BAT_LED_RD, (leds & BATT_LED_RED) ? HIGH : LOW);
}

// ----- Button Input Functions -----
//
// These helper functions provide a consistent interface for reading
// button states throughout the firmware.
//
// Individual button states are combined into bit masks when multiple
// buttons must be tested simultaneously.
// ---------------------------------------------------------

// Returns a bit mask representing all buttons currently pressed.
uint8_t getPressedButtonsMask()
{
  uint8_t mask = BTN_MASK_NONE;

  for (uint8_t i = 0; i < BTN_COUNT; i++)
  {
    if (buttons[i].isPressed())
    {
      mask |= (1 << i);
    }
  }

  return mask;
}

// Returns a bit mask representing all buttons currently released.
uint8_t getReleasedButtonsMask()
{
  uint8_t mask = BTN_MASK_NONE;

  for (uint8_t i = 0; i < BTN_COUNT; i++)
  {
    if (buttons[i].isReleased())
    {
      mask |= (1 << i);
    }
  }

  return mask;
}

// Returns a bit mask of buttons pressed during the current loop iteration.
uint8_t getJustPressedButtonsMask()
{
  uint8_t mask = BTN_MASK_NONE;

  for (uint8_t i = 0; i < BTN_COUNT; i++)
  {
    if (buttons[i].justPressed())
    {
      mask |= (1 << i);
    }
  }

  return mask;
}

// Returns a bit mask of buttons released during the current loop iteration.
uint8_t getJustReleasedButtonsMask()
{
  uint8_t mask = BTN_MASK_NONE;
  for (uint8_t i = 0; i < BTN_COUNT; i++)
  {
    if (buttons[i].justReleased())
    {
      mask |= (1 << i);
    }
  }
  return mask;
}

// Updates every button object.
//
// Reads the hardware inputs, performs debouncing, and generates
// one-shot events such as justPressed() and justReleased().
void updateButtons()
{
  for (uint8_t i = 0; i < BTN_COUNT; i++)
  {
    buttons[i].update();
  }
}

// Services time-critical background tasks during blocking operations.
//
// This helper keeps button events and buzzer timing responsive while
// waiting inside blocking loops.
void updateInputsAndBuzzer()
{
  currentMillis = millis();
  buzzer.update();

  updateButtons();
}

// ----- Battery and Power Management -----
//
// This subsystem periodically measures the battery voltage, updates
// the battery level indicator, monitors for low-battery conditions,
// and automatically powers the device down when required.
// ---------------------------------------------------------

// Reads the MCU supply voltage in millivolts.
//
// The ADC cannot measure VCC directly while also using VCC as its
// reference. Instead, this function measures the internal 1.1 V
// bandgap reference using VCC as the ADC reference.
//
// Since the bandgap voltage is approximately fixed, the ADC result
// can be used to calculate the MCU supply voltage.
//
// In BATTERY_SENSE_VCC mode, the MCU is powered directly from the
// battery, so the returned VCC voltage is also the battery voltage.
//
// In BATTERY_SENSE_DIVIDER mode, the measured VCC value is used as
// the ADC reference voltage when calculating the external battery
// voltage from PIN_BAT_ADC.
uint16_t readVccMillivolts()
{
  constexpr uint32_t BANDGAP_MV = 1100;

  // Configure the ADC to measure the internal 1.1 V bandgap reference
  // using VCC as the ADC reference.
  //
  // ADMUX register:
  //   REFS1:0 = 01   -> VCC used as ADC reference.
  //   MUX3:0  = 1110 -> Internal 1.1 V bandgap reference.
  //   ADMUX   = 0b01001110
  ADMUX =
      _BV(REFS0) | // Set REFS0 (REFS1 remains 0)
      _BV(MUX3) |  // Set MUX3
      _BV(MUX2) |  // Set MUX2
      _BV(MUX1);   // Set MUX1 (MUX0 remains 0)

  // Allow the reference voltage to settle after switching inputs.
  delay(2);

  // Discard the first conversion after changing the ADC input.
  ADCSRA |= _BV(ADSC);

  if (!waitForAdcConversion())
  {
    return BATTERY_UNKNOWN_MV;
  }

  // Read the ADC results from multiple samples.
  uint32_t adcTotal = 0;

  for (uint8_t i = 0; i < BATTERY_SAMPLE_COUNT; i++)
  {
    // Set the ADSC bit to start an ADC conversion.
    // The hardware clears this bit automatically when the conversion completes.
    ADCSRA |= _BV(ADSC);

    if (!waitForAdcConversion())
    {
      return BATTERY_UNKNOWN_MV;
    }

    adcTotal += ADC;
  }

  // Calculate the average ADC value from the total.
  uint16_t adcAverage = adcTotal / BATTERY_SAMPLE_COUNT; // ADC = BANDGAP_MV / VCC * ADC_MAX, where ADC_MAX = 1023

  if (adcAverage == 0)
  {
    return BATTERY_UNKNOWN_MV;
  }

  // Convert the ADC reading to the supply voltage in millivolts.
  //
  // ADC = BANDGAP_MV / VCC × ADC_MAX
  // Therefore:
  // VCC = BANDGAP_MV × ADC_MAX / ADC
  // VCC = 1100 mV × 1023 / ADC.
  uint32_t vccMv = (BANDGAP_MV * ADC_MAX) / adcAverage;

  return static_cast<uint16_t>(vccMv);
}

// Reads the battery voltage in millivolts.
//
// The measurement method is selected at compile time:
//
//   BATTERY_SENSE_DISABLED
//     Returns BATTERY_UNKNOWN_MV.
//
//   BATTERY_SENSE_VCC
//     The MCU is powered directly from the battery.
//     Returns the MCU supply voltage measured using the
//     internal 1.1 V bandgap reference.
//
//   BATTERY_SENSE_DIVIDER
//     Measures the battery voltage through an external resistor
//     divider connected to PIN_BAT_ADC.
//
// Returns BATTERY_UNKNOWN_MV if the measurement fails or battery
// sensing is disabled.
uint16_t readBatteryMillivolts()
{
#if MEMOBOT_BATTERY_SENSE == BATTERY_SENSE_DISABLED

  return BATTERY_UNKNOWN_MV;

#elif MEMOBOT_BATTERY_SENSE == BATTERY_SENSE_VCC

  return readVccMillivolts();

#elif MEMOBOT_BATTERY_SENSE == BATTERY_SENSE_DIVIDER

  uint16_t vccMv = readVccMillivolts();

  if (vccMv == BATTERY_UNKNOWN_MV)
  {
    return BATTERY_UNKNOWN_MV;
  }

  // Discard the first conversion after switching from the internal
  // bandgap input to the external ADC channel.
  delay(2);
  analogRead(PIN_BAT_ADC);

  uint32_t adcTotal = 0;

  for (uint8_t i = 0; i < BATTERY_SAMPLE_COUNT; i++)
  {
    adcTotal += analogRead(PIN_BAT_ADC);
  }

  uint16_t adcAverage = adcTotal / BATTERY_SAMPLE_COUNT;

  uint32_t adcPinMv =
      static_cast<uint32_t>(adcAverage) *
      vccMv /
      ADC_MAX;

  uint32_t measuredBatteryMv =
      adcPinMv *
      (BATTERY_DIVIDER_R_TOP_OHMS +
       BATTERY_DIVIDER_R_BOTTOM_OHMS) /
      BATTERY_DIVIDER_R_BOTTOM_OHMS;

  if (measuredBatteryMv >= BATTERY_UNKNOWN_MV)
  {
    return BATTERY_UNKNOWN_MV;
  }

  return static_cast<uint16_t>(measuredBatteryMv);

#else

#error "Unsupported battery sensing method"

#endif
}

// Waits for the current ADC conversion to complete.
//
// Returns false if the conversion does not complete before the
// specified timeout expires.
bool waitForAdcConversion(uint32_t timeoutUs)
{
  uint32_t startTime = micros();
  while (bit_is_set(ADCSRA, ADSC))
  {
    if (micros() - startTime >= timeoutUs)
    {
      return false;
    }
  }
  return true;
}

// Displays an animated pattern while a valid battery reading is unavailable.
void displayUnknownBatteryLevel()
{
  // Animate red -> yellow -> green -> yellow while battery reading is unknown.
  static uint8_t step = 0;
  static uint32_t lastStepTime = 0;
  constexpr uint16_t STEP_TIME_MS = 140;

  constexpr uint8_t readingPattern[] = {
      BATT_LED_RED,
      BATT_LED_YELLOW,
      BATT_LED_GREEN,
      BATT_LED_YELLOW};

  if (currentMillis - lastStepTime >= STEP_TIME_MS)
  {
    lastStepTime = currentMillis;
    step = (step + 1) % (sizeof(readingPattern) / sizeof(readingPattern[0]));
  }

  setBatteryLEDs(readingPattern[step]);
}

// Updates the battery indicator LEDs.
//
// A dedicated animation is shown while the battery voltage is unknown.
// During the low-battery state, LED control is left to the warning
// animation.
void updateBatteryLeds()
{
  if (state == STATE_LOW_BATTERY)
  {
    // Low-battery warning owns the battery LEDs while this state is active.
    // Do not overwrite its blinking pattern with the normal level display.
    return;
  }

  if (batteryMv == BATTERY_UNKNOWN_MV)
  {
    displayUnknownBatteryLevel();
    return;
  }

  if (batteryMv >= BATTERY_GOOD_MV)
  {
    setBatteryLEDs(BATT_LED_GREEN | BATT_LED_YELLOW | BATT_LED_RED);
  }
  else if (batteryMv >= BATTERY_LOW_MV)
  {
    setBatteryLEDs(BATT_LED_RED | BATT_LED_YELLOW);
  }
  else if (batteryMv >= BATTERY_CRIT_MV)
  {
    setBatteryLEDs(BATT_LED_RED);
  }
  else
  {
    setBatteryLEDs(BATT_LED_OFF);
  }
}

// Periodically services the battery management subsystem.
//
// This function:
//   - Updates the battery level indicator LEDs.
//   - Measures the battery voltage at fixed intervals.
//   - Checks whether a low-battery shutdown should be requested.
//
// The LEDs are refreshed every loop iteration, while the ADC is
// sampled less frequently to reduce processing overhead.
void updateBatteryManager()
{
#if MEMOBOT_BATTERY_SENSE == BATTERY_SENSE_DISABLED

  setBatteryLEDs(BATT_LED_OFF);
  return;

#else

  if (state == STATE_POWER_DOWN)
  {
    setBatteryLEDs(BATT_LED_OFF);
    return; // Skip battery management while powering down
  }

  static uint32_t lastBatteryCheckTime = 0;
  constexpr uint32_t BATTERY_CHECK_INTERVAL_MS = 10000;

  updateBatteryLeds();

  if (currentMillis - lastBatteryCheckTime < BATTERY_CHECK_INTERVAL_MS)
  {
    return;
  }

  lastBatteryCheckTime = currentMillis;

  batteryMv = readBatteryMillivolts();

  checkLowBatteryPowerOff();

#endif
}

// Requests the low-battery state after several consecutive critical
// voltage readings.
//
// Requiring multiple low readings prevents brief voltage dips from
// triggering an unnecessary shutdown.
void checkLowBatteryPowerOff()
{
#if MEMOBOT_POWER_HOLD == POWER_HOLD_DISABLED || \
    MEMOBOT_BATTERY_SENSE == BATTERY_SENSE_DISABLED
  return;
#endif

  static uint8_t lowBatteryCount = 0;
  constexpr uint8_t LOW_BATTERY_COUNT_MAX = 5;

  if (state == STATE_POWER_DOWN || state == STATE_LOW_BATTERY)
  {
    return;
  }

  if (batteryMv == BATTERY_UNKNOWN_MV)
  {
    lowBatteryCount = 0;
    return;
  }

  if (batteryMv <= BATTERY_CRIT_MV)
  {
    lowBatteryCount++;

    if (lowBatteryCount >= LOW_BATTERY_COUNT_MAX)
    {
      startLowBatteryState();
    }
  }
  else
  {
    lowBatteryCount = 0;
  }
}

// Resets the inactivity timer on user input and powers the device
// down after a prolonged period with no activity.
void checkNoActivityPowerOff()
{
#if MEMOBOT_POWER_HOLD == POWER_HOLD_DISABLED
  return;
#endif
  // Register user activity to reset the auto power-off timer
  if (getJustPressedButtonsMask())
  {
    lastUserActivityTime = currentMillis;
  }

  // Check for auto power-off
  if (state != STATE_POWER_DOWN && currentMillis - lastUserActivityTime >= AUTO_POWER_OFF_MS)
  {
    startPowerDownState();
    return;
  }
}
