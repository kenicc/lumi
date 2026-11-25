"""Wydanie nowej wersji oprogramowania dla LUMI.

1. Podbij LUMI_FW_BUILD (i LUMI_FW_VERSION) w src/Version.h.
2. pio run -e <środowisko>
3. SUPABASE_URL=... SUPABASE_SERVICE_ROLE_KEY=... python3 tools/release.py <środowisko> [--notes "co nowego"]

Skrypt liczy SHA-256 obrazu, podpisuje go kluczem z keys/ota_private.pem, wysyła plik do
magazynu "firmware" i dodaje wpis do tabeli firmware_releases. Urządzenia danego wariantu
zainstalują go w najbliższą noc. --dry-run tylko zapisuje manifest obok obrazu.
"""
import argparse
import base64
import hashlib
import json
import os
import re
import sys
import urllib.request

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

ROOT = os.path.join(os.path.dirname(__file__), "..")


def version():
    src = open(os.path.join(ROOT, "src", "Version.h")).read()
    v = re.search(r'LUMI_FW_VERSION "([^"]+)"', src).group(1)
    b = int(re.search(r"LUMI_FW_BUILD (\d+)", src).group(1))
    return v, b


def sign(digest: bytes, key_path: str) -> str:
    key = serialization.load_pem_private_key(open(key_path, "rb").read(), password=None)
    sig = key.sign(digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))  # DER, jak oczekuje mbedtls_pk_verify
    return base64.b64encode(sig).decode()


def manifest(env: str, key_path: str, notes: str):
    ver, build = version()
    path = os.path.join(ROOT, ".pio", "build", env, "firmware.bin")
    data = open(path, "rb").read()
    digest = hashlib.sha256(data).digest()
    return data, {
        "variant": env,
        "build": build,
        "version": ver,
        "size": len(data),
        "sha256": digest.hex(),
        "signature": sign(digest, key_path),
        "notes": notes,
    }


def call(method, url, key, body, ctype):
    req = urllib.request.Request(url, data=body, method=method)
    req.add_header("Authorization", f"Bearer {key}")
    req.add_header("apikey", key)
    req.add_header("Content-Type", ctype)
    req.add_header("Prefer", "return=minimal")
    with urllib.request.urlopen(req) as r:
        return r.status


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("env", help="środowisko PlatformIO = wariant urządzenia (allegro, proto, series)")
    ap.add_argument("--notes", default="")
    ap.add_argument("--key", default=os.path.join(ROOT, "keys", "ota_private.pem"))
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    data, m = manifest(a.env, a.key, a.notes)
    if a.dry_run:
        out = os.path.join(ROOT, ".pio", "build", a.env, "release.json")
        json.dump(m, open(out, "w"), indent=2)
        print("manifest:", out)
        return
    url, key = os.environ.get("SUPABASE_URL", "").rstrip("/"), os.environ.get("SUPABASE_SERVICE_ROLE_KEY", "")
    if not url or not key:
        sys.exit("ustaw SUPABASE_URL i SUPABASE_SERVICE_ROLE_KEY")
    obj = f"{a.env}/{m['build']}-{m['sha256'][:12]}.bin"
    call("POST", f"{url}/storage/v1/object/firmware/{obj}", key, data, "application/octet-stream")
    m["url"] = f"{url}/storage/v1/object/public/firmware/{obj}"
    call("POST", f"{url}/rest/v1/firmware_releases", key, json.dumps(m).encode(), "application/json")
    print(f"wydano {m['version']} (build {m['build']}) dla {a.env}: {m['url']}")


if __name__ == "__main__":
    main()
