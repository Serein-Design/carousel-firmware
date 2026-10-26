// SPDX-License-Identifier: AGPL-3.0-only
/*
 * Reference board: pin map (KiCad net names in comments) and panel geometry.
 *
 * POWER WARNING:
 *   The panel must be powered by an external 5 V supply (4 A minimum).
 *   NEVER power it from the ESP32's 5V/VIN pin or from USB.
 *   The 5 V supply ground and the ESP32 ground must be tied together.
 */
#pragma once

// ---------------------------------------------------------------------------
// HUB75 pins  (KiCad net -> GPIO)
// ---------------------------------------------------------------------------
#define R1_PIN  25   // R0
#define G1_PIN  26   // G0
#define B1_PIN  27   // B0
#define R2_PIN  21   // R1
#define G2_PIN  22   // G1
#define B2_PIN  23   // B1

#define A_PIN   12   // A
#define B_PIN   16   // B
#define C_PIN   17   // C
#define D_PIN   18   // D
#define E_PIN   -1   // -1 for a 64x32 panel (1/16 scan)

#define CLK_PIN 15   // CLK
#define LAT_PIN 32   // LAT
#define OE_PIN  33   // OE

// ---------------------------------------------------------------------------
// Input pins  (KiCad net -> GPIO)
// ---------------------------------------------------------------------------
#define ENC_CLK_PIN 14   // ACLK - encoder channel A
#define ENC_DT_PIN  19   // DT   - encoder channel B
#define ENC_SW_PIN  13   // BT   - encoder push button
#define TILT_PIN    34   // TILT SWITCH

// GPIO34 is input-only and has NO internal pull-up: the external 10k
// resistor to +3.3V on the schematic is mandatory.

#define BUTTON_DEBOUNCE_MS 25
#define TILT_DEBOUNCE_MS   80

// ---------------------------------------------------------------------------
// Physical panel
// ---------------------------------------------------------------------------
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

#define PIXEL_COUNT (PANEL_RES_X * PANEL_RES_Y)   // 2048, whatever the orientation
#define MAX_DIM     64                            // largest logical dimension
