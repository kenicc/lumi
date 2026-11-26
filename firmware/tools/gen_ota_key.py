"""Tworzy parę kluczy do podpisywania aktualizacji (ECDSA P-256).

- keys/ota_private.pem  - PRYWATNY, zostaje u twórcy (jest w .gitignore). Zgubiony = nowa para
  i jednorazowe wgranie kablem; wykradziony = ktoś może podpisać własne oprogramowanie.
- src/net/ota_pubkey.h  - publiczny, wkompilowany w firmware.

Uruchom raz: python3 tools/gen_ota_key.py
"""
import os
import sys

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec

root = os.path.join(os.path.dirname(__file__), "..")
priv_path = os.path.join(root, "keys", "ota_private.pem")
if os.path.exists(priv_path) and "--force" not in sys.argv:
    sys.exit(f"{priv_path} już istnieje (nadpisanie: --force)")
key = ec.generate_private_key(ec.SECP256R1())
os.makedirs(os.path.dirname(priv_path), exist_ok=True)
with open(priv_path, "wb") as f:
    f.write(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
os.chmod(priv_path, 0o600)
pub = key.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo).decode()
lines = "".join(f'    "{l}\\n"\n' for l in pub.strip().splitlines())
with open(os.path.join(root, "src", "net", "ota_pubkey.h"), "w") as f:
    f.write("#pragma once\n// Klucz publiczny do sprawdzania podpisu aktualizacji (tools/gen_ota_key.py)\n")
    f.write(f"static const char LUMI_OTA_PUBKEY[] =\n{lines};\n")
print("zapisano", priv_path, "i src/net/ota_pubkey.h")
