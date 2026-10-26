// SPDX-License-Identifier: AGPL-3.0-only
// ---------------------------------------------------------------------------
// ISS (DEMO version, no network): the station on a world map, with the
// night creeping across, its past and upcoming track, the panel's city, and
// the distance between them. In landscape, a close-up view alternates every 600 frames.
//
// The orbit is computed for real (inclination 51.64 deg, period 92.9 min,
// drift of 360 deg per sidereal day), but it starts from an arbitrary position
// and runs 20 times faster than reality: without a network-set clock, the map
// cannot be synchronised with the real station.
// ---------------------------------------------------------------------------
#include "display.h"
#include "grid.h"
#include "iss_text.h"
#include "palette.h"
#include "screens/screens.h"
#include "i18n.h"
#include "settings.h"
#include "type.h"

namespace iss {
  const float INCLINATION = 51.64f * (float)PI / 180.0f;
  const float PERIOD = 92.9f, SIDEREAL_DAY = 1436.0f;  // minutes
  const float LAT0 = 20.0f, LON0 = -40.0f;             // DEMO starting point

  // 64x32 land map, one bit per pixel (generated from the simulator's table)
  static const uint64_t LAND_MASK[32] = {
    0x0000000000000000ULL, 0x000000001fff8000ULL, 0x0007061c1ffffc00ULL, 0xfffffffc1fdffff8ULL,
    0xffffffff0b9c7ffcULL, 0x1fffffef803c7ff8ULL, 0x18fffffec07f7f00ULL, 0x03ffffff807ffe00ULL,
    0x03fffc9fc00ffe00ULL, 0x01fffff4c007fc00ULL, 0x00bfffffc003f800ULL, 0x003ef7ffe0027000ULL,
    0x002ff7ffe00de000ULL, 0x002e63ffe0018000ULL, 0x002641ffe03f0000ULL, 0x003f00ff80fe0000ULL,
    0x07ba00fe03fe0000ULL, 0x071c00fc03fe0000ULL, 0x038001fc03fc0000ULL, 0x07e0017c01f80000ULL,
    0x0ff0003c00f80000ULL, 0x0ff0003800780000ULL, 0x8700000000380000ULL, 0xc200000000180000ULL,
    0x2000000000180000ULL, 0x0000000000180000ULL, 0x0000000000000000ULL, 0x0000000000700000ULL,
    0x1fffffffe01fffc0ULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL,
  };
  inline bool isLand(int x, int y) { return (LAND_MASK[y] >> x) & 1ULL; }

  inline float rad(float d) { return d * (float)PI / 180.0f; }
  inline float deg(float r) { return r * 180.0f / (float)PI; }
  inline float normLon(float lon) {
    lon = fmodf(fmodf(lon + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f;
    return lon;
  }

  struct Pos { float lat, lon; };

  // Position dt minutes after the start, circular orbit
  Pos position(float dt) {
    const float u = asinf(constrain(sinf(rad(LAT0)) / sinf(INCLINATION), -1.0f, 1.0f));
    const float node = rad(LON0) - atan2f(cosf(INCLINATION) * sinf(u), cosf(u));
    const float ut = u + 2.0f * (float)PI * dt / PERIOD;
    Pos p;
    p.lat = deg(asinf(sinf(INCLINATION) * sinf(ut)));
    p.lon = normLon(deg(node + atan2f(cosf(INCLINATION) * sinf(ut), cosf(ut)))
                    - 360.0f * dt / SIDEREAL_DAY);
    return p;
  }

  int distanceKm(const Pos &a, float blat, float blon) {
    const float d = sinf(rad(a.lat)) * sinf(rad(blat))
                  + cosf(rad(a.lat)) * cosf(rad(blat)) * cosf(rad(a.lon - blon));
    return (int)(6371.0f * acosf(constrain(d, -1.0f, 1.0f)));
  }
}

// A label-face line centred on cx over a black plate, so it reads on the map
static void plateText(int cx, int y, const char *txt, uint16_t color) {
  const int w = textWidth(txt, Face::LABEL);
  const int x = constrain(cx - w / 2, 1, W() - w - 1);
  display->fillRect(x - 1, y - 1, w + 2, 7, palette().off);
  drawText(*display, txt, x, y, Face::LABEL, color);
}

void screenIss(uint32_t f, bool init) {
  using namespace iss;
  (void)init;
  const bool landscape = W() >= 60;

  const float minutes = millis() / 60000.0f * 20.0f;   // DEMO: 20x real time
  const Pos here = position(minutes);
  const int  km     = distanceKm(here, settings.lat, settings.lon);
  const bool isNear = km < 2000;

  // Sun: season frozen, but the subsolar point moves back 15 deg per simulated
  // hour, so the day/night terminator really does sweep across the map.
  const float slat = 23.44f * sinf(2.0f * (float)PI * (172.0f - 81.0f) / 365.0f);
  const float slon = normLon(180.0f - 15.0f * (minutes / 60.0f));

  const Palette &pal = palette();
  display->clearScreen();

  const int mapH = landscape ? 32 : 16;
  const int mapW = landscape ? 64 : 32;
  const int step = landscape ? 1 : 2;
  const bool closeUp = landscape && (f / 600) % 2 == 1 && (f / 600) % 4 != 3;

  if (!closeUp) {
    // --- World map, day and night -------------------------------------------
    // The trigonometric terms depend only on the row or only on the column:
    // hoist them out of the loops, otherwise it is 6000 sines per frame.
    const float sinSlat = sinf(rad(slat)), cosSlat = cosf(rad(slat));
    float cosDelta[64];
    for (int x = 0; x < mapW; x++) {
      const float lon = (x * step + 0.5f) * 360.0f / 64.0f - 180.0f;
      cosDelta[x] = cosf(rad(lon - slon));
    }
    for (int y = 0; y < mapH; y++) {
      const int   my  = y * step;
      const float lat = 90.0f - (my + 0.5f) * 180.0f / 32.0f;
      const float sinLat = sinf(rad(lat)), cosLat = cosf(rad(lat));
      for (int x = 0; x < mapW; x++) {
        const int   mx = x * step;
        const float sun = sinLat * sinSlat + cosLat * cosSlat * cosDelta[x];
        const float k = sun > 0.1f ? 1.0f : (sun > -0.1f ? 0.6f : 0.3f);
        const bool  land = isLand(mx, my);
        uint16_t col = land ? display->color565(40 * k, 130 * k, 50 * k)
                            : display->color565(0, 30 * k, 90 * k);
        if (land && k < 0.5f && ((mx * 7 + my * 13) % 9 == 0))
          col = display->color565(160, 130, 50);      // city lights at night
        display->drawPixel(x, y, col);
      }
    }

    // Track: past as dark dots, upcoming as bright dots
    for (int i = -30; i <= 60; i++) {
      if (i % 2) continue;
      const float dt = i * 1.5f;
      const Pos p = position(minutes + dt);
      display->drawPixel((int)((p.lon + 180.0f) / 360.0f * mapW),
                         (int)((90.0f - p.lat) / 180.0f * mapH),
                         dt < 0 ? display->color565(90, 90, 120)
                                : display->color565(200, 200, 255));
    }

    if ((f / 20) % 2)
      display->drawPixel((int)((settings.lon + 180.0f) / 360.0f * mapW),
                         (int)((90.0f - settings.lat) / 180.0f * mapH),
                         display->color565(255, 40, 40));

    const int ix = (int)((here.lon + 180.0f) / 360.0f * mapW);
    const int iy = (int)((90.0f - here.lat) / 180.0f * mapH);
    const uint16_t c = (f / 10) % 2 ? WHITE : display->color565(255, 220, 0);
    display->drawPixel(ix, iy, c);
    display->drawPixel(ix - 1, iy, c); display->drawPixel(ix + 1, iy, c);
    display->drawPixel(ix, iy - 1, c); display->drawPixel(ix, iy + 1, c);
  } else {
    // --- Close-up view: the station above the curved horizon -----------------
    for (int i = 0; i < 20; i++)
      display->drawPixel((i * 37 + (int)(f / 8)) % W(), (i * 11) % 18,
                         display->color565(120, 120, 140));
    display->fillCircle(32, 110, 88, display->color565(20, 70, 160));
    display->drawCircle(32, 110, 89, display->color565(120, 190, 255));
    const int bx = 22, by = 9 + (int)(sinf(f * 0.05f) * 1.5f);
    const int offsets[2] = { -9, 13 };
    for (int dx : offsets) {                                  // solar panels
      display->fillRect(bx + dx, by - 2, 7, 5, display->color565(40, 60, 170));
      display->drawFastHLine(bx + dx, by, 7, display->color565(150, 150, 170));
    }
    display->drawFastHLine(bx - 2, by, 15, display->color565(200, 200, 210));
    display->fillRect(bx + 1, by - 1, 9, 3, display->color565(230, 230, 235));
    display->drawPixel(bx + 5, by - 2, display->color565(255, 60, 60));

    char txt[16];
    issSpeedText(txt, sizeof txt, (f / 200) % 2 == 0);
    plateText(W() / 2, 25, txt, pal.text);
  }

  if (landscape && !closeUp && (f / 600) % 4 == 3) {
    char txt[20];
    // The orbit is simulated: say so before giving a distance
    if ((f / 150) % 2 == 0) snprintf(txt, sizeof txt, "%s", ISS_DEMO_LABEL);
    else issDistanceText(txt, sizeof txt, isNear, km, tr());
    plateText(32, 25, txt, isNear && (f / 150) % 2 ? pal.accent : pal.text);
  }

  if (!landscape) {
    // --- Portrait: information below the map ---------------------------------
    char lat[8], lon[8];
    snprintf(lat, sizeof lat, "%d", (int)here.lat);
    snprintf(lon, sizeof lon, "%d", (int)here.lon);
    const int mapX = constrain((int)((here.lon + 180.0f) / 360.0f * 64), 0, 63);
    const int mapY = constrain((int)((90.0f - here.lat) / 180.0f * 32), 0, 31);
    const char *words[6] = { ISS_DEMO_LABEL, "LAT", lat, "LON", lon,
                             isLand(mapX, mapY) ? tr().land : tr().ocean };
    const Face faces[6]  = { Face::LABEL, Face::LABEL, Face::BODY, Face::LABEL, Face::BODY,
                             Face::LABEL };
    const uint16_t colors[6] = { pal.dim, pal.label, pal.text, pal.label, pal.text, pal.accent };
    for (int i = 0; i < 6; i++)
      drawText(*display, words[i], W() / 2, 17 + i * 8 + (faces[i] == Face::LABEL ? 1 : 0),
               faces[i], colors[i], Align::CENTER);
    if (isNear && (f / 15) % 2)
      display->drawRect(0, 16, W(), H() - 16, pal.accent);
  }
}
