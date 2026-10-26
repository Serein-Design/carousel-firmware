# SPDX-License-Identifier: AGPL-3.0-only
# PlatformIO pre-script for OTA uploads: reads OTA_PASSWORD from
# include/secrets.h, so the password lives in one git-ignored place.
import re
from pathlib import Path

Import("env")  # noqa: F821 - provided by PlatformIO

secrets = Path(env.subst("$PROJECT_INCLUDE_DIR"), "secrets.h")  # noqa: F821
match = secrets.exists() and re.search(r'#define\s+OTA_PASSWORD\s+"([^"]*)"', secrets.read_text())
if match and match.group(1):
    env.Append(UPLOAD_FLAGS=["--auth=" + match.group(1)])  # noqa: F821
else:
    print("ota_auth: no OTA_PASSWORD in include/secrets.h, the board will refuse the upload")
