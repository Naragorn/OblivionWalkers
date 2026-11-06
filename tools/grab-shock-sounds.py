#!/usr/bin/env python3
"""Synthesises the grab's shock sounds (docs/walkers-spec.md 4.20.6, the grab
shock sound; Nadi, 2026-11-06: "default #1, alle drei in LINK und oder ini"):
three stingers of about two seconds, mono 16-bit 44.1 kHz, made here from
saws, a pitched sine and noise - no samples, so they are the project's own
(GPL-3.0 with the rest). [Grab] ShockSound picks one.

    python3 tools/grab-shock-sounds.py sounds/fx/OblivionWalkers

writes GrabShock1.wav (a screech over a low hit), GrabShock2.wav (a boom
first, the screech under it) and GrabShock3.wav (a dissonant brass stab)."""
import sys
import wave

import numpy as np  # numpy only

SR = 44100
rng = np.random.default_rng(7)


def t(sec):
    return np.arange(int(SR * sec)) / SR


def env(n, attack, decay):
    x = np.arange(n) / SR
    a = np.clip(x / attack, 0, 1)
    return a * np.exp(-x / decay)


def saw(freq, tt, vib=0.0):
    phase = np.cumsum(freq * (1 + vib * np.sin(2 * np.pi * 5.5 * tt)) / SR)
    return 2 * (phase % 1) - 1


def lowpass(x, cutoff):
    spec = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    spec *= 1 / (1 + (f / cutoff) ** 4)
    return np.fft.irfft(spec, len(x))


def highpass(x, cutoff):
    spec = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    spec *= (f / cutoff) ** 4 / (1 + (f / cutoff) ** 4)
    return np.fft.irfft(spec, len(x))


def reverb(x, seconds=1.6, wet=0.35):
    n = int(SR * seconds)
    ir = rng.standard_normal(n) * np.exp(-np.arange(n) / SR / (seconds / 5))
    ir = lowpass(ir, 6000)
    ir /= np.sqrt(np.sum(ir ** 2))
    size = len(x) + n
    y = np.fft.irfft(np.fft.rfft(x, size) * np.fft.rfft(ir, size), size)
    out = np.zeros(size)
    out[: len(x)] += x * (1 - wet)
    return out + y * wet


def cluster(tt, base, cents, vib):
    s = np.zeros(len(tt))
    for c in cents:
        s += saw(base * 2 ** (c / 1200) * (1 + rng.uniform(-0.002, 0.002)), tt, vib)
    return s / len(cents)


def boom(sec, f0=55, f1=32, decay=0.5):
    tt = t(sec)
    freq = f1 + (f0 * 2 - f1) * np.exp(-tt / 0.06)
    s = np.sin(2 * np.pi * np.cumsum(freq) / SR)
    return s * env(len(tt), 0.002, decay)


def screech(sec, base=740, decay=0.45):
    tt = t(sec)
    s = cluster(tt, base, [0, 100, 600, 700, 1100], 0.012)
    s = highpass(lowpass(s, 7000), 500)
    return s * env(len(tt), 0.004, decay)


def stab(sec, base=110, decay=0.35):
    tt = t(sec)
    s = cluster(tt, base, [0, 300, 600, 1300, 1900], 0.0)
    s = lowpass(s, 2500) + 0.3 * highpass(rng.standard_normal(len(tt)), 3000) * env(len(tt), 0.001, 0.03)
    return s * env(len(tt), 0.003, decay)


def mix(*parts):
    n = max(len(p) for p, _ in parts)
    out = np.zeros(n)
    for p, g in parts:
        out[: len(p)] += p * g
    return out


def finish(x, sec):
    x = x[: int(SR * sec)]
    fade = int(SR * 0.25)
    x[-fade:] *= np.linspace(1, 0, fade)
    x = np.tanh(1.6 * x / np.max(np.abs(x)))
    return x / np.max(np.abs(x)) * 0.89


def save(path, x):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((x * 32767).astype("<i2").tobytes())


out = sys.argv[1]
variants = {
    "1": finish(reverb(mix((screech(2.0), 1.0), (boom(1.5), 0.6)), 1.4, 0.3), 2.0),
    "2": finish(reverb(mix((boom(2.0, decay=0.8), 1.0), (screech(1.6, 620, 0.35), 0.55)), 1.8, 0.35), 2.2),
    "3": finish(reverb(mix((stab(1.8), 1.0), (boom(1.5, decay=0.4), 0.7)), 1.5, 0.3), 2.0),
}
for name, x in variants.items():
    save(f"{out}/GrabShock{name}.wav", x)
    print(name, f"{len(x) / SR:.2f} s")
