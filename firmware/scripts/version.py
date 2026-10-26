# SPDX-License-Identifier: AGPL-3.0-only
# PlatformIO pre-script: bakes `git describe` into the firmware as
# FIRMWARE_VERSION, shown at boot and on /id. The FIRMWARE_VERSION
# environment variable wins: the release workflow sets it to the tag.
import os
import subprocess

Import("env")  # noqa: F821 - provided by PlatformIO

version = os.environ.get("FIRMWARE_VERSION")
try:
    version = version or subprocess.check_output(
        ["git", "describe", "--always", "--dirty"],
        cwd=env.subst("$PROJECT_DIR"), text=True).strip()  # noqa: F821
except Exception:
    version = "dev"
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])  # noqa: F821
