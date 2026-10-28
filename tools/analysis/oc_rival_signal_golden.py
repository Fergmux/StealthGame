#!/usr/bin/env python3
"""Independent re-derivation of the Objectives OC-12 rival-signal golden vectors (story oc-014, AC3).

This script shares no code with the C++ build: it reimplements SipHash-2-4 from the reference paper, the ADR-0006 sub-seed
derivation (FStealthServerSeeds::GetServerSubSeed), FSecretStream's sequential mode and Lemire's UniformBelow, and prints the
StageSignal delay and release for ordinals k = 1..3 at the fixed test secret used by StealthGame.Session.SeedDerivationGolden
(ServerSecret = bytes 0x10..0x1f, MatchNonce = bytes 0x20..0x27 little-endian, RoundIndex 0).

    python tools/analysis/oc_rival_signal_golden.py            # prints the vectors
    python tools/analysis/oc_rival_signal_golden.py --self-test  # SipHash reference vector check

The C++ test StealthGame.Objectives.RivalSignalGolden pins the printed values; a disagreement means one side is wrong.
"""
import sys

MASK = (1 << 64) - 1


def rotl(x, b):
    return ((x << b) | (x >> (64 - b))) & MASK


def siphash24(k0, k1, msg):
    v0 = k0 ^ 0x736F6D6570736575
    v1 = k1 ^ 0x646F72616E646F6D
    v2 = k0 ^ 0x6C7967656E657261
    v3 = k1 ^ 0x7465646279746573

    def rnd():
        nonlocal v0, v1, v2, v3
        v0 = (v0 + v1) & MASK; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32)
        v2 = (v2 + v3) & MASK; v3 = rotl(v3, 16); v3 ^= v2
        v0 = (v0 + v3) & MASK; v3 = rotl(v3, 21); v3 ^= v0
        v2 = (v2 + v1) & MASK; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32)

    length = len(msg)
    end = length - (length % 8)
    for i in range(0, end, 8):
        m = int.from_bytes(msg[i:i + 8], 'little')
        v3 ^= m
        rnd(); rnd()
        v0 ^= m
    tail = bytes(msg[end:]) + bytes(7 - (length % 8))
    b = (length & 0xFF) << 56 | int.from_bytes(tail[:7] + b'\x00', 'little')
    v3 ^= b
    rnd(); rnd()
    v0 ^= b
    v2 ^= 0xFF
    rnd(); rnd(); rnd(); rnd()
    return (v0 ^ v1 ^ v2 ^ v3) & MASK


def tag(text):
    assert len(text) == 8
    return int.from_bytes(text.encode('ascii'), 'little')


def le(value, n):
    return value.to_bytes(n, 'little')


K_STREAM_CONST = tag('SGSTREAM')
OC_STAGE = tag('OC_STAGE')
STAGE_SIGNAL_DOMAIN = 10


def server_sub_seed(secret, nonce, round_index, domain, attempt):
    k0 = int.from_bytes(secret[0:8], 'little')
    k1 = int.from_bytes(secret[8:16], 'little')
    msg = bytes([0x02]) + le(nonce, 8) + le(round_index, 4) + le(domain, 4) + le(attempt, 4)
    return siphash24(k0, k1, msg)


class SecretStream:
    def __init__(self, sub_seed, stream_tag):
        self.sub_seed = sub_seed
        self.stream_tag = stream_tag
        self.counter = 0

    def next_u32(self):
        msg = bytes([0x01]) + le(self.stream_tag, 8) + le(self.counter, 8)
        self.counter += 1
        return siphash24(self.sub_seed, K_STREAM_CONST, msg) & 0xFFFFFFFF


def uniform_below(stream, n):
    x = stream.next_u32()
    m = x * n
    low = m & 0xFFFFFFFF
    if low < n:
        threshold = ((1 << 32) - n) % n
        while low < threshold:
            x = stream.next_u32()
            m = x * n
            low = m & 0xFFFFFFFF
    return m >> 32


def uniform_in_range(stream, lo, hi):
    return lo + uniform_below(stream, hi - lo + 1)


def release_ms(t0, tc, delta, q):
    num = tc + delta - t0
    assert num > 0 and q > 0
    return t0 + q * ((num + q - 1) // q)


def self_test():
    # SipHash-2-4 reference: key 00..0f, message 00..0e (15 bytes) -> 0xa129ca6149be45e5 (Aumasson and Bernstein, appendix A).
    key = bytes(range(16))
    k0 = int.from_bytes(key[0:8], 'little')
    k1 = int.from_bytes(key[8:16], 'little')
    got = siphash24(k0, k1, bytes(range(15)))
    ok = got == 0xA129CA6149BE45E5
    print('siphash reference vector', 'ok' if ok else 'FAILED %016x' % got)
    return 0 if ok else 1


def main():
    if '--self-test' in sys.argv:
        return self_test()
    secret = bytes(range(0x10, 0x20))
    nonce = int.from_bytes(bytes(range(0x20, 0x28)), 'little')
    min_ms, max_ms, q_ms = 30000, 165000, 30000
    t0_ms, tc_ms = 100000, 200000  # Active at 100 s, the completion 100 s later
    for k in (1, 2, 3):
        sub = server_sub_seed(secret, nonce, 0, STAGE_SIGNAL_DOMAIN, k)
        delta = uniform_in_range(SecretStream(sub, OC_STAGE), min_ms, max_ms)
        rel = release_ms(t0_ms, tc_ms, delta, q_ms)
        print('k=%d subseed=0x%016X delta_ms=%d release_ms=%d' % (k, sub, delta, rel))
    return 0


if __name__ == '__main__':
    sys.exit(main())
