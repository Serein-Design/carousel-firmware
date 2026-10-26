// SPDX-License-Identifier: AGPL-3.0-only
#include "boot_task.h"

#include <Arduino.h>

#include "boot_logic.h"
#include "display.h"
#include "screens/screens.h"

static BootBaton baton;

// About 60 frames a second, as loop()'s
static const TickType_t FRAME_TICKS = pdMS_TO_TICKS(16);

// The sequence as loop() would draw it, frame after frame, until loop() asks
// for the frame or the opening is due (setup() past 15 s): the first screen
// then rises in loop(), which has it
static void bootTask(void *) {
  while (baton.taskMayDraw() && drawBootScreen(millis())) {
    displayPush();
    vTaskDelay(FRAME_TICKS);
  }
  baton.taskLeft();
  vTaskDelete(nullptr);
}

void bootTaskBegin() {
  baton.taskTakes();
  // On loop()'s core, above setup()'s priority (1): the fade stays even while
  // setup() works; each frame takes a few ms of the 16
  if (xTaskCreatePinnedToCore(bootTask, "boot", 4096, nullptr, 2, nullptr, 1) != pdPASS) {
    baton.taskLeft();
    Serial.println("[BOOT] no task for the mark: it lights with loop()");
  }
}

void bootTaskEnd() {
  baton.loopAsks();
  while (!baton.loopMayDraw()) delay(1);
}
