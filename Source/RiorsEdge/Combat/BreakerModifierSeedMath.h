#pragma once

#include "CoreMinimal.h"

// THE MODIFIER SEED, as pure world-free maths (O281). A modifier roll takes
// one int32 seed; this is where that seed comes from.
//
// THE RULE. Three numbers go in: the authored base, the session salt and the
// site. The base is the design seat's knob (ABreakerGameMode::ModifierSeedBase).
// The salt is drawn once per session and is what stops the same body wearing
// the same set two runs running. The site is which body: the wave and index,
// the pocket and slot, the carrier's lateral. Within a session the roll for a
// site repeats — a wave 8 Champion killed and met again on retry is the same
// Champion — and between two sessions it differs.
//
// THE GUARANTEE, and it is exact rather than probabilistic: for a fixed base
// and salt, two different sites never share a seed; for a fixed base and site,
// two different salts never share a seed. Every step below is a bijection on
// uint32 — xor with a constant, multiply by an odd constant, an invertible
// xorshift finaliser — so the last fold is injective in whichever argument
// varied. Two sessions differing in BOTH salt and site can collide, which is
// the birthday bound and is fine: nothing reads a seed as an identity.
//
// A salt of ZERO is the reproducible session: automation and the capture
// harness pass it so a suite assertion and a photograph read the same roll
// every run. Zero is folded like any other value, so a zero-salt seed is
// still mixed, not the raw base.
namespace BreakerModifierSeed
{
    // The reproducible session. Automation and the harness use this salt.
    inline constexpr int32 ReproducibleSalt = 0;   // O2 PLACEHOLDER

    // FNV-1a offset basis and prime, 32-bit. Odd prime: the multiply is a
    // bijection mod 2^32.
    inline constexpr uint32 FoldBasis = 0x811C9DC5u;   // O2 PLACEHOLDER
    inline constexpr uint32 FoldPrime = 0x01000193u;   // O2 PLACEHOLDER

    // Invertible 32-bit finaliser (xorshift-multiply, lowbias32 shape). Each
    // xorshift by a positive amount is a bijection; each multiplier is odd.
    constexpr uint32 Scramble(uint32 X)
    {
        X ^= X >> 16;
        X *= 0x7FEB352Du;   // O2 PLACEHOLDER
        X ^= X >> 15;
        X *= 0x846CA68Bu;   // O2 PLACEHOLDER
        X ^= X >> 16;
        return X;
    }

    // One fold of a value into the running hash. Bijective in Value for a
    // fixed H, and bijective in H for a fixed Value.
    constexpr uint32 Fold(uint32 H, int32 Value)
    {
        return Scramble((H ^ static_cast<uint32>(Value)) * FoldPrime);
    }

    // The seed a body rolls its modifiers from. Base is the authored knob,
    // SessionSalt is the session's draw (ReproducibleSalt under automation
    // and the harness), Site is the body's place in the encounter.
    constexpr int32 Mix(int32 Base, int32 SessionSalt, int32 Site)
    {
        uint32 H = FoldBasis;
        H = Fold(H, Base);
        H = Fold(H, SessionSalt);
        H = Fold(H, Site);
        return static_cast<int32>(H);
    }

    // The rule, proved at compile time against the shipped base.
    namespace Proof
    {
        inline constexpr int32 ShippedBase = 20260814;   // O2 PLACEHOLDER
        // A session repeats its own roll.
        static_assert(Mix(ShippedBase, 7, 3) == Mix(ShippedBase, 7, 3), "a site rolls the same seed twice in one session");
        // Two sessions differ at the same site.
        static_assert(Mix(ShippedBase, 1, 0) != Mix(ShippedBase, 2, 0), "two salts must not share a seed at one site");
        static_assert(Mix(ShippedBase, ReproducibleSalt, 0) != Mix(ShippedBase, 1, 0), "the reproducible session must differ from a live one");
        // Two sites differ within one session.
        static_assert(Mix(ShippedBase, 0, 0) != Mix(ShippedBase, 0, 1), "two sites must not share a seed in one session");
        static_assert(Mix(ShippedBase, 0, 8 * 7919 + 0) != Mix(ShippedBase, 0, 8 * 7919 + 1), "wave elite and carrier slots must not share a seed");
        static_assert(Mix(ShippedBase, 0, 500) != Mix(ShippedBase, 0, 0), "gym carrier and gym elite must not share a seed");
        // The reproducible session is mixed, not the raw base.
        static_assert(Mix(ShippedBase, ReproducibleSalt, 0) != ShippedBase, "salt zero is still a mix");
        // Negative salts (GetTypeHash cast to int32 can be negative) fold cleanly.
        static_assert(Mix(ShippedBase, -1, 0) != Mix(ShippedBase, 1, 0), "a negative salt is its own session");
    }
}
