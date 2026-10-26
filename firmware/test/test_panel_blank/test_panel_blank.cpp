// SPDX-License-Identifier: AGPL-3.0-only
// The panel made dark without the DMA: against a model of a HUB75 column
// driver (one shift register and one latch per colour line, OE low = lit),
// whatever it latched before, power-on noise or the last row shown, is
// replaced by zeros, and the output stays off from the first write on.
//
//   pio test -e native -f test_panel_blank
#include <unity.h>

#include <cstring>

#include "../../src/board.h"
#include "../../src/panel_blank_logic.h"

static const uint16_t COLUMNS = PANEL_RES_X * PANEL_CHAIN;
static const PanelBlankPins PINS = {OE_PIN, LAT_PIN, CLK_PIN,
                                    {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN}};

struct PanelModel {
  bool level[40];          // what each GPIO drives (its output register)
  bool taken[40];          // the pin is a plain output, out of the DMA's hands
  bool shift[6][COLUMNS];  // one chain per colour line
  bool latch[6][COLUMNS];  // what the LEDs show while OE is low
  int  takes = 0, oeTakenAt = -1, firstOtherTakeAt = -1;
  int  rising = 0;         // CLK rising edges
  bool litDuring = false;  // a latched 1 on show while OE was driven low

  // The worst start: every LED latched on, OE floating low
  PanelModel() {
    memset(level, 0, sizeof(level));
    memset(taken, 0, sizeof(taken));
    memset(shift, 1, sizeof(shift));
    memset(latch, 1, sizeof(latch));
  }

  bool driven(int8_t pin) const { return taken[pin]; }
  bool oeOn() const { return driven(PINS.oe) && !level[PINS.oe]; }
  bool anyLatched() const {
    for (auto &line : latch)
      for (bool on : line) if (on) return true;
    return false;
  }

  void take(int8_t pin) {
    if (pin == PINS.oe) oeTakenAt = takes;
    else if (firstOtherTakeAt < 0) firstOtherTakeAt = takes;
    takes++;
    taken[pin] = true;
    if (oeOn() && anyLatched()) litDuring = true;
  }

  void set(int8_t pin, bool high) {
    const bool was = level[pin];
    level[pin] = high;
    if (!driven(pin)) return;
    if (pin == PINS.clk && high && !was) {   // shift in on the rising edge
      rising++;
      for (int d = 0; d < 6; d++) {
        memmove(&shift[d][1], &shift[d][0], COLUMNS - 1);
        shift[d][0] = driven(PINS.data[d]) && level[PINS.data[d]];
      }
    }
    if (pin == PINS.lat && high) memcpy(latch, shift, sizeof(latch));
    if (oeOn() && anyLatched()) litDuring = true;
  }
};

struct ModelOut {
  PanelModel &m;
  void level(int8_t pin, bool high) const { m.set(pin, high); }
  void take(int8_t pin) const { m.take(pin); }
};

static PanelModel *model;

void setUp() { model = new PanelModel(); }
void tearDown() { delete model; }

static void blank() {
  ModelOut out{*model};
  panelLatchBlank(PINS, COLUMNS, out);
}

void test_every_latch_holds_zero() {
  blank();
  TEST_ASSERT_FALSE(model->anyLatched());
}

void test_a_full_row_is_shifted_before_the_latch() {
  blank();
  TEST_ASSERT_EQUAL(COLUMNS, model->rising);
}

void test_oe_is_off_before_it_is_driven_and_stays_off() {
  blank();
  TEST_ASSERT_FALSE(model->litDuring);
  TEST_ASSERT_TRUE(model->taken[PINS.oe]);
  TEST_ASSERT_TRUE(model->level[PINS.oe]);
}

void test_oe_is_taken_before_any_other_pin() {
  blank();
  TEST_ASSERT_EQUAL(0, model->oeTakenAt);
  TEST_ASSERT_EQUAL(1, model->firstOtherTakeAt);
}

void test_every_panel_pin_is_taken() {
  blank();
  TEST_ASSERT_TRUE(model->taken[PINS.lat]);
  TEST_ASSERT_TRUE(model->taken[PINS.clk]);
  for (int8_t pin : PINS.data) TEST_ASSERT_TRUE(model->taken[pin]);
  TEST_ASSERT_EQUAL(9, model->takes);
}

void test_lat_and_clk_end_low() {
  blank();
  TEST_ASSERT_FALSE(model->level[PINS.lat]);
  TEST_ASSERT_FALSE(model->level[PINS.clk]);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_every_latch_holds_zero);
  RUN_TEST(test_a_full_row_is_shifted_before_the_latch);
  RUN_TEST(test_oe_is_off_before_it_is_driven_and_stays_off);
  RUN_TEST(test_oe_is_taken_before_any_other_pin);
  RUN_TEST(test_every_panel_pin_is_taken);
  RUN_TEST(test_lat_and_clk_end_low);
  return UNITY_END();
}
