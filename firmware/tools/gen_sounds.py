"""Generuje dźwięki interfejsu do data/sounds (WAV 16 kHz, mono, 16 bit).

Wszystkie tony leżą w paśmie ok. 400-1000 Hz: z wiekiem najpierw słabnie słyszenie
wysokich częstotliwości, więc typowe "piski" telefonów (2-4 kHz) senior słyszy gorzej.
Uruchom: python3 tools/gen_sounds.py, potem pio run -t uploadfs
"""
import math
import os
import struct
import wave

RATE = 16000
OUT = os.path.join(os.path.dirname(__file__), "..", "data", "sounds")


def tone(freq, ms, vol=0.8, fade=8):
    n = int(RATE * ms / 1000)
    f = int(RATE * fade / 1000)
    out = []
    for i in range(n):
        env = min(1.0, i / f if f else 1, (n - i) / f if f else 1)
        # ton podstawowy + delikatna druga harmoniczna: wyraźniejszy w małym głośniku
        s = math.sin(2 * math.pi * freq * i / RATE) + 0.3 * math.sin(4 * math.pi * freq * i / RATE)
        out.append(vol * env * s / 1.3)
    return out


def silence(ms):
    return [0.0] * int(RATE * ms / 1000)


def save(name, samples):
    os.makedirs(OUT, exist_ok=True)
    with wave.open(os.path.join(OUT, name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s)) * 32000)) for s in samples))


save("key.wav", tone(880, 35, 0.6, 4))
save("ok.wav", tone(660, 90) + tone(880, 140))
save("error.wav", tone(440, 120) + silence(60) + tone(392, 200))
save("notice.wav", tone(660, 120) + silence(40) + tone(784, 120) + silence(40) + tone(988, 220))
# automatyczne odbieranie: trzy wyraźne sygnały "zaraz rozmowa"
save("autoanswer.wav", sum([tone(784, 180) + silence(120) for _ in range(3)], []))
# dzwonek: dwa tony na zmianę, potem przerwa; odtwarzany w pętli coraz głośniej
ring = []
for _ in range(4):
    ring += tone(523, 110) + tone(659, 110)
ring += silence(1100)
save("ring.wav", ring)
print("zapisano w", os.path.abspath(OUT))
