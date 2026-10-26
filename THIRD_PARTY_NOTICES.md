# Third-party notices

Copyright 2026 Serein Design. Carousel is licensed under the [GNU AGPL v3.0 only](LICENSE). It
builds on the following components, each under its own license.

Serein Design, the se/de mark, Carousel and Nimbus are trademarks of Serein Design; the AGPL does
not grant rights to them.

## Linked into the firmware

| Component | Version | License |
| --- | --- | --- |
| [ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA) | 3.0.14 | MIT, © 2018 Faptastic |
| [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library) | 1.12.6 | BSD 2-Clause, © 2012 Adafruit Industries |
| [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO) | 1.17.4 | MIT, © 2017 Adafruit Industries |
| [Arduino core for ESP32](https://github.com/espressif/arduino-esp32) | 2.0.17 (PlatformIO `espressif32@6.10.0`) | LGPL-2.1 |
| [ESP-IDF](https://github.com/espressif/esp-idf) (through the Arduino core) | 4.4.7 | Apache-2.0, © Espressif Systems (Shanghai) Co., Ltd. |
| Mozilla's CA certificate list, as published by [curl](https://curl.se/docs/caextract.html) (`firmware/certs/x509_crt_bundle.bin`) | as of 2026-09-25 | MPL-2.0 |
| [tz database](https://www.iana.org/time-zones) (tzdata), each zone name and its POSIX TZ string (`firmware/src/tz_names.cpp`) | 2026e | public domain |

### ESP-IDF components and libraries in the binary

ESP-IDF 4.4.7 bundles components that keep their own license. The table lists those the
firmware links (read from the linker map of the `carousel` build). The full texts, with each
copyright notice, are in [`LICENSES/`](LICENSES/); a binary distribution of the firmware
must carry them.

| Component | License | Text |
| --- | --- | --- |
| [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) 2.28 (`mbedtls`, `mbedcrypto`, `mbedx509`) | Apache-2.0 (Mbed TLS is dual-licensed Apache-2.0 or GPL-2.0-or-later; Carousel uses it under Apache-2.0), © The Mbed TLS Contributors | [Apache-2.0](LICENSES/Apache-2.0.txt) |
| [esp-mqtt](https://github.com/espressif/esp-mqtt) (`mqtt`) | Apache-2.0, © Espressif Systems; `mqtt_msg.c` is BSD 3-Clause, © 2014 Stephen Robinson | [Apache-2.0](LICENSES/Apache-2.0.txt), [BSD-3-Clause](LICENSES/BSD-3-Clause.txt) |
| [lwIP](https://savannah.nongnu.org/projects/lwip/) (`lwip`) | BSD 3-Clause, © 2001, 2002 Swedish Institute of Computer Science | [BSD-3-Clause](LICENSES/BSD-3-Clause.txt) |
| [FreeRTOS kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) 10.4.3 (`freertos`) | MIT, © 2020 Amazon.com, Inc. or its affiliates; Espressif's changes are Apache-2.0 | [MIT](LICENSES/MIT.txt), [Apache-2.0](LICENSES/Apache-2.0.txt) |
| wpa_supplicant / hostapd (`wpa_supplicant`) | BSD 3-Clause, © 2002-2019 Jouni Malinen and contributors | [BSD-3-Clause](LICENSES/BSD-3-Clause.txt) |
| [LittleFS](https://github.com/littlefs-project/littlefs) (`esp_littlefs`) | BSD 3-Clause, © 2022 The littlefs authors, © 2017 Arm Limited | [BSD-3-Clause](LICENSES/BSD-3-Clause.txt) |
| [esp_littlefs](https://github.com/joltwallet/esp_littlefs) (the port to ESP-IDF) | MIT, © 2020 Brian Pugh | [MIT](LICENSES/MIT.txt) |
| [cJSON](https://github.com/DaveGamble/cJSON) (`json`) | MIT, © 2009-2017 Dave Gamble and cJSON contributors | [MIT](LICENSES/MIT.txt) |
| [nghttp2](https://github.com/nghttp2/nghttp2) (`nghttp`) | MIT, © Tatsuhiro Tsujikawa and nghttp2 contributors | [MIT](LICENSES/MIT.txt) |
| [protobuf-c](https://github.com/protobuf-c/protobuf-c) (`protobuf-c`) | BSD 2-Clause, © Dave Benson and the protobuf-c authors | [BSD-2-Clause](LICENSES/BSD-2-Clause.txt) |
| [QR Code generator library](https://www.nayuki.io/page/qr-code-generator-library) by Project Nayuki (`qrcode`) | MIT, © Project Nayuki; Espressif's wrapper is Apache-2.0 | [MIT](LICENSES/MIT.txt), [Apache-2.0](LICENSES/Apache-2.0.txt) |
| Newlib, the C library (`libc`, `libm`, `newlib`) | a set of BSD-style licenses | [Newlib](LICENSES/Newlib.txt) |
| `libgcc`, `libstdc++` | GPL-3.0-or-later with the GCC Runtime Library Exception | [GCC-exception-3.1](LICENSES/GCC-exception-3.1.txt) |
| Other ESP-IDF components (`esp_wifi`, `esp_netif`, `esp-tls`, `tcp_transport`, `mdns`, `nvs_flash`, `spi_flash`, `driver`, `heap`, `esp_system`, `esp_timer`, `app_update`, `vfs`, `wifi_provisioning`, `protocomm`, `esp_ipc`, `esp_event`, `esp_eth`, `soc`, `hal`, ...) | Apache-2.0, © Espressif Systems | [Apache-2.0](LICENSES/Apache-2.0.txt) |
| Espressif's precompiled radio libraries (`net80211`, `pp`, `phy`, `coexist`, `mesh`, `smartconfig`, `btdm_app`) | Apache-2.0, © Espressif Systems | [Apache-2.0](LICENSES/Apache-2.0.txt) |

The Arduino core also carries `libb64` (public domain) for base64.

## Data the firmware fetches

The panel downloads this data at run time; none of it ships in the binary. The pages that
show the weather (Stage and Running order) credit both sources in their footer.

| Data | Source | License |
| --- | --- | --- |
| Weather forecast | [MET Norway](https://api.met.no/), Locationforecast 2.0 | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (and NLOD 2.0) |
| Place names, coordinates, time zones | [GeoNames](https://www.geonames.org/) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |

`data/places.bin`, the city table the panel fetches block by block from this repository, is
built from the GeoNames dumps `cities500`, `countryInfo`, `admin1CodesASCII` and
`alternateNamesV2` (CC BY 4.0, GeoNames): places selected, nearby ones merged, names folded
and re-encoded by `tools/places/build.py` (format: `tools/places/FORMAT.md`). Its time zone
strings come from tzdata (public domain).

`firmware/test/test_met/compact.json`, a test fixture, is a trimmed MET Norway forecast for
Paris (2026-10-25), CC BY 4.0, MET Norway.

## Embedded in the simulator

| Component | Where | License |
| --- | --- | --- |
| 5x7 font from Adafruit GFX (`glcdfont.c`) | `tools/simulator/simulator.html`, `FONT` | BSD 2-Clause, © 2012 Adafruit Industries |

## Used for development only (not distributed)

| Component | Use | License |
| --- | --- | --- |
| [Unity](https://github.com/ThrowTheSwitch/Unity) | host unit tests | MIT |
| [Pillow](https://python-pillow.org) | `tools/pixelart/pixelart.py` | MIT-CMU (HPND) |
| [Playwright](https://playwright.dev) 1.64.0 | `tools/webcheck/webcheck.mjs`, headless Chromium | Apache-2.0 |
| [axe-core](https://github.com/dequelabs/axe-core) 4.14.0 | `tools/webcheck/webcheck.mjs` | MPL-2.0 |
| [cryptography](https://cryptography.io) | `firmware/scripts/gen_crt_bundle.py` | Apache-2.0 or BSD-3-Clause |
| [IBM Plex](https://github.com/IBM/plex) | loaded by `docs/wiring.html` from Google Fonts | SIL OFL 1.1 |

## Typeface behind the boot wordmark

The word "Carousel" that the panel shows at boot (`firmware/src/wordmark_data.cpp`) is a bitmap
of that one word, set in **Chillax** by the [Indian Type Foundry](https://www.fontshare.com)
(Fontshare) and rasterised by `tools/brand/wordmark.mjs`. Chillax is licensed under the
[ITF Free Font License](https://www.fontshare.com/licenses/itf-ffl) (FFL) 2.0, which allows
logos and wordmarks made with the font. The font file is not part of this repository or of the
firmware; only the bitmap is. The script can also draw the word in Switzer, the foundry's other
face under the same license, which is not distributed either.

## Characters, logos and trademarks

This repository ships no third-party character, meme, brand or logo: the screens and the
simulator examples are the product's own or generic. Fan art lives outside it, in a separate
community pack that the simulator loads from a file you pick (Open...). That pack is not
covered by the AGPL and is not licensed by Serein Design; every character and name in it remains
the property of its owner.
