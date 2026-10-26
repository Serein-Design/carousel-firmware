// SPDX-License-Identifier: AGPL-3.0-only
// The boot sequence's first frames, from a task of their own: the mark
// lights right after the panel starts and fades while setup() starts the
// content, the gallery, the network, the web server and the updates; loop()
// then takes the frame back where the task left the sequence. One owner of
// the frame at a time (BootBaton, boot_logic.h).
#pragma once

// After displayBegin() and galleryPanelBegin(): the frame's rotation and the
// panel's brightness are set, nothing else in setup() draws until bootTaskEnd()
void bootTaskBegin();

// Stops the task and waits for its last frame to be pushed: the frame is the
// caller's from then on. At the end of setup(), and before setup() draws
// itself (the button held at boot). At once without a task, or a second time.
void bootTaskEnd();
