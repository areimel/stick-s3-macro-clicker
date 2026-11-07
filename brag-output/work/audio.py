"""Soundtrack for the Stick-S3 brag video. Pure stdlib. 120 BPM, A minor, 22.0 s.
Every event sits on the beat grid counted from t=0 (beat = 0.5 s)."""
import math, random, wave, array

SR = 44100
DUR = 22.0
N = int(SR * DUR)
random.seed(7)
TAU = 2 * math.pi

def hz(note):  # MIDI -> Hz
    return 440.0 * 2 ** ((note - 69) / 12)

music = [0.0] * N
sfx = [0.0] * N
duck = [1.0] * N

def add(buf, start, length, fn):
    i0 = max(0, int(start * SR)); i1 = min(N, int((start + length) * SR))
    for i in range(i0, i1):
        buf[i] += fn((i - i0) / SR, i / SR)

# Chord per 2-second bar: Am F C G Am F C G Am F Am
A, F, Cc, G = (57, 60, 64), (53, 57, 60), (48, 52, 55), (55, 59, 62)
BARS = [A, F, Cc, G, A, F, Cc, G, A, F, A]
ROOTS = [45, 41, 48, 43, 45, 41, 48, 43, 45, 41, 45]

# --- kick (quarters from 4.0 to 21.0) + sidechain envelope
def kick(t, _):
    f = 45 + 80 * math.exp(-t * 28)
    ph = TAU * (45 * t + 80 * (1 - math.exp(-t * 28)) / 28)
    return 0.55 * math.sin(ph) * math.exp(-t * 9)
def soft_kick(t, a):
    return 0.55 * kick(t, a)
for k in range(1, int(21.0 / 0.5) + 1):
    t0 = k * 0.5
    add(music, t0, 0.35, kick if t0 >= 4.0 else soft_kick)
    i0 = int(t0 * SR)
    for i in range(i0, min(N, i0 + int(0.3 * SR))):
        x = (i - i0) / SR
        duck[i] = min(duck[i], 0.45 + 0.55 * min(1, x / 0.22))

# --- bass: 8th pulses from 4.0 (hook gets quarter pulses, quieter)
def bass_note(freq, amp, length):
    def fn(t, _):
        env = min(1, t / 0.006) * math.exp(-t * 5)
        s = math.sin(TAU * freq * t) + 0.35 * math.sin(TAU * 2 * freq * t)
        return amp * math.tanh(1.6 * s) * env
    return fn
for k in range(int(21.0 / 0.25)):
    t0 = k * 0.25
    bar = int(t0 // 2)
    if t0 < 4.0:
        amp = 0.13
    else:
        amp = 0.17
    add(music, t0, 0.24, bass_note(hz(ROOTS[bar]), amp, 0.24))

# --- pad (whole bars, soft, detuned)
def pad_fn(notes, amp):
    def fn(t, _):
        env = min(1, t / 0.35) * min(1, (2.05 - t) / 0.4) if t < 2.05 else 0
        s = 0.0
        for n in notes:
            f = hz(n)
            s += math.sin(TAU * f * t) + math.sin(TAU * f * 1.004 * t) + 0.25 * math.sin(TAU * 2 * f * t)
        return amp * env * s / len(notes)
    return fn
for b, ch in enumerate(BARS):
    add(music, b * 2.0, 2.05, pad_fn(ch, 0.035))
# final sustained Am tail to the end with fade
def tail(t, _):
    env = min(1, t / 0.05) * max(0, 1 - t / 1.0)
    s = sum(math.sin(TAU * hz(n) * t) + math.sin(TAU * hz(n) * 1.004 * t) for n in A) / 3
    return 0.09 * env * s
add(music, 21.0, 1.0, tail)

# --- arp: 16th plucks through chord tones, 0..21
PAT = [0, 1, 2, 1, 0, 2, 1, 2]
def pluck(freq, amp):
    def fn(t, _):
        env = min(1, t / 0.003) * math.exp(-t * 11)
        return amp * env * (math.sin(TAU * freq * t) + 0.3 * math.sin(TAU * 3 * freq * t) * math.exp(-t * 20))
    return fn
for k in range(int(21.0 / 0.125)):
    t0 = k * 0.125
    bar = int(t0 // 2)
    ch = BARS[bar]
    note = ch[PAT[k % 8]] + 12 + (12 if (k // 8) % 2 and k % 8 == 7 else 0)
    amp = 0.06
    if (k % 4) == 0: amp *= 1.3
    add(music, t0, 0.3, pluck(hz(note), amp))

# --- hats: offbeat 8ths from 0.5, snare on 2 & 4 from 7.0
noise = [random.uniform(-1, 1) for _ in range(SR)]
def hat(t, abs_t):
    i = int(abs_t * SR) % SR
    return 0.05 * (noise[i] - noise[i - 1]) * math.exp(-t * 70)
for k in range(1, int(21.0 / 0.5)):
    add(music, k * 0.5 + 0.25, 0.08, hat)
def snare(t, abs_t):
    i = int(abs_t * SR) % SR
    return (0.06 * noise[i] * math.exp(-t * 22) + 0.09 * math.sin(TAU * 190 * t) * math.exp(-t * 30))
for k in range(int(7.0 / 0.5), int(21.0 / 0.5)):
    if k % 2 == 1:
        add(music, k * 0.5, 0.25, snare)

# --- SFX (blended: in key, on the grid)
def click(freq, amp):
    def fn(t, abs_t):
        i = int(abs_t * SR) % SR
        return amp * (math.sin(TAU * freq * t) * math.exp(-t * 45) + 0.25 * noise[i] * math.exp(-t * 400))
    return fn
PRESS = [(0.5, 81), (7.5, 81), (8.5, 81), (8.75, 81), (9.5, 76), (11.5, 76), (12.0, 81),
         (12.5, 84), (13.0, 88), (13.5, 93), (19.0, 81)]
for t0, n in PRESS:
    add(sfx, t0, 0.15, click(hz(n), 0.09))
add(sfx, 11.5, 0.15, click(hz(81), 0.06))  # the chord: two buttons
# hold: soft glide A4 -> A5 while held, then a blip when it fires (10.0 grid, fires at 10.15 -> blip on 10.25 16th? keep 10.125 32nd)
def glide(t, _):
    f = hz(69) * 2 ** (t / 0.65)
    return 0.03 * math.sin(TAU * hz(69) * 0.65 / math.log(2) * (2 ** (t / 0.65) - 1)) * min(1, t / 0.05)
add(sfx, 9.5, 0.625, glide)
add(sfx, 10.125, 0.2, click(hz(93), 0.07))
# typing: 32nd-note ticks while the prompt and the YAML type
def tick(t, abs_t):
    i = int(abs_t * SR) % SR
    return 0.03 * (noise[i] - noise[i - 1]) * math.exp(-t * 260)
for a, b in [(0.5625, 3.375), (15.1875, 16.1875)]:
    k = 0
    while a + k * 0.0625 <= b:
        add(sfx, a + k * 0.0625, 0.02, tick); k += 1
# risers into each cut
def riser(length):
    state = {"y": 0.0}
    def fn(t, abs_t):
        i = int(abs_t * SR) % SR
        p = t / length
        a = 0.02 + 0.25 * p * p
        state["y"] += a * (noise[i] - state["y"])
        return 0.09 * p * p * state["y"]
    return fn
for cut in [4.0, 7.0, 11.0, 15.0, 18.0]:
    add(sfx, cut - 0.5, 0.5, riser(0.5))
# reveal impact at 4.0 (A1 boom)
def impact(t, abs_t):
    i = int(abs_t * SR) % SR
    return 0.17 * math.sin(TAU * hz(33) * t) * math.exp(-t * 3) + 0.04 * noise[i] * math.exp(-t * 18)
add(sfx, 4.0, 1.4, impact)
add(sfx, 21.0, 1.0, lambda t, a: 0.8 * impact(t, a))
# install complete chime on the beat at 20.5 (A C E)
def chime(t, _):
    return 0.022 * sum(math.sin(TAU * hz(n) * t) for n in (81, 84, 88)) * math.exp(-t * 2.5) * min(1, t / 0.004)
add(sfx, 20.5, 1.5, chime)

# --- mix
out = array.array("h")
mixed = []
for i in range(N):
    x = music[i] * duck[i] * 0.95 + sfx[i] * 0.75
    mixed.append(math.tanh(1.1 * x))
peak = max(abs(v) for v in mixed)
g = 10 ** (-1.5 / 20) / peak
fade0 = int((DUR - 0.5) * SR)
for i, v in enumerate(mixed):
    if i >= fade0: v *= 1 - (i - fade0) / (N - fade0)
    s = int(max(-1, min(1, v * g)) * 32767)
    out.append(s); out.append(s)
with wave.open("audio.wav", "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(out.tobytes())
print("peak before gain", round(peak, 3), "gain", round(g, 3))
