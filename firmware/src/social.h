// SPDX-License-Identifier: AGPL-3.0-only
// Follower counters from YouTube and GitHub, fetched in the background.
#pragma once

#include <Arduino.h>

struct SocialStats {
  uint32_t ytSubscribers = 0, ytViews = 0, ytVideos = 0;
  uint32_t ghFollowers = 0, ghRepos = 0;
  bool     ytOk = false, ghOk = false;
  uint32_t updatedAt = 0;
};
extern SocialStats socialStats;

// The HTTPS task's turn (https_task.cpp): fetches the counters; returns the
// ms until the next turn
uint32_t socialStep();
