// SPDX-License-Identifier: AGPL-3.0-only
// Stand-in for ESP-IDF's esp_err.h: the error type and the two name lookups
// src/error_codes.cpp replaces.
#pragma once

#include <stddef.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1

extern "C" const char *esp_err_to_name(esp_err_t code);
extern "C" const char *esp_err_to_name_r(esp_err_t code, char *buf, size_t buflen);
