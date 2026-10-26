# SPDX-License-Identifier: AGPL-3.0-only
# Builds certs/x509_crt_bundle.bin, the root certificates the panel trusts
# for HTTPS (WiFiClientSecure::setCACertBundle), from a PEM list such as
# Mozilla's, published by curl: https://curl.se/ca/cacert.pem
#
#   python3 firmware/scripts/gen_crt_bundle.py cacert.pem firmware/certs/x509_crt_bundle.bin
#
# Format read by the Arduino core (WiFiClientSecure/src/esp_crt_bundle.c):
# certificate count on 2 bytes, then per certificate the subject length and
# the public key length on 2 bytes each, the subject (DER) and the public key
# (DER SubjectPublicKeyInfo). Sorted by subject: the core binary-searches it.
import struct
import sys

from cryptography import x509
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat


def main(src, dst):
    with open(src, "rb") as f:
        certs = x509.load_pem_x509_certificates(f.read())
    entries = sorted(
        (c.subject.public_bytes(),
         c.public_key().public_bytes(Encoding.DER, PublicFormat.SubjectPublicKeyInfo))
        for c in certs)
    out = bytearray(struct.pack(">H", len(entries)))
    for name, key in entries:
        out += struct.pack(">HH", len(name), len(key)) + name + key
    with open(dst, "wb") as f:
        f.write(out)
    print(f"{len(entries)} certificates, {len(out)} bytes -> {dst}")


if __name__ == "__main__":
    main(*sys.argv[1:3])
