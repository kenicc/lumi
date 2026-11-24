"""Generuje src/net/ca_bundle.c - główne certyfikaty CA (z pakietu certifi / Mozilla) w formacie,
który czyta WiFiClientSecure::setCACertBundle w arduino-esp32 2.x:
  [u16 liczba] potem dla każdego: [u16 dł. nazwy][u16 dł. klucza][nazwa DER][klucz publiczny DER],
posortowane po nazwie (biblioteka szuka binarnie).
Dzięki temu LUMI sprawdza certyfikat każdego zwykłego serwera HTTPS (Vercel, Supabase, Open-Meteo).
Uruchom: pip install certifi cryptography && python3 tools/gen_ca_bundle.py
"""
import os
import struct

import certifi
from cryptography import x509
from cryptography.hazmat.primitives import serialization

pem = open(certifi.where(), "rb").read()
certs = x509.load_pem_x509_certificates(pem)
entries = {}
for c in certs:
    name = c.subject.public_bytes()
    key = c.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
    entries[name] = key
blob = struct.pack(">H", len(entries))
for name in sorted(entries):
    key = entries[name]
    blob += struct.pack(">HH", len(name), len(key)) + name + key

out = os.path.join(os.path.dirname(__file__), "..", "src", "net", "ca_bundle.c")
with open(out, "w") as f:
    f.write("// Wygenerowane przez tools/gen_ca_bundle.py - nie edytować ręcznie\n#include <stdint.h>\n")
    f.write(f"// {len(entries)} certyfikatów, {len(blob)} B\nconst uint8_t lumi_ca_bundle[] = {{\n")
    for i in range(0, len(blob), 20):
        f.write("  " + ", ".join(f"0x{b:02x}" for b in blob[i:i + 20]) + ",\n")
    f.write("};\n")
print(len(entries), "certyfikatów,", len(blob), "B")
