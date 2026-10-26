// SPDX-License-Identifier: AGPL-3.0-only
// The city table on the panel: data/places.bin read in pieces with range
// requests (places_logic for what they hold), in the HTTPS task, which has
// the stack and the idle priority HTTPS needs (https_task.h). The table's head and
// last blocks stay in RAM while the owner searches, and are freed after
// PLACES_IDLE_MS without a search, or before a forecast (placesKeepTable):
// about 13 KB, never kept for nothing.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "places_logic.h"

// The web task waits this long for an answer, then says "busy" and the page
// asks again: one web server serves every route (the API, the hook, the
// pages), and another HTTPS session in progress (a forecast, up to 35 s)
// or the table's first
// pieces (a TLS session each) would hold it for seconds
static const uint32_t PLACES_WAIT_MS = 1000;

// The settings page's search, from the web task: the HTTPS task runs it.
// Up to PLACES_RESULTS cities, best first, as placesJson writes them into
// json; false when there is no answer yet (the page asks again: a search
// still running keeps its answer for the same query), or the table could
// not be reached (the network, a piece refused).
bool placesSearch(const char *query, char *json, size_t size);

// The HTTPS task's side: runs a search placesSearch posted, if any, and
// frees the table once unused for PLACES_IDLE_MS
void placesServe();
// The HTTPS task's too: frees the table unless placesKeepTable keeps it;
// httpsNext before a forecast
void placesTidy(bool httpsNext);

enum LocateResult : uint8_t {
  LOCATE_FOUND,
  LOCATE_NOT_FOUND,     // the table has no such city
  LOCATE_UNREACHABLE,   // its trouble or the network's: ask again later
};
// The HTTPS task's: the city chosen on the settings page (cid >= 0, its
// number in the table) or typed (the best match) -> hit and its POSIX zone
LocateResult placesLocate(const char *city, int32_t cid, PlaceHit &hit, char *zone,
                          size_t zoneSize);

// The suggestion the settings form posted with the city (cid), until the
// HTTPS task looked it up; -1 none. In RAM only: a restart before then
// takes the typed name's best match instead.
extern volatile int32_t placesChosen;
