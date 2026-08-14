#include "Combat/BreakerEnemyModifiers.h"

namespace BreakerModifierDetail
{
    // File-local and distinctively prefixed: unity builds concatenate
    // translation units, and a bare `Pair` or `Clamp01` here would collide with
    // whatever the next file in the blob declares.
    struct FBreakerModifierPair
    {
        EBreakerEnemyModifier A;
        EBreakerEnemyModifier B;
    };

    // Encounter-Design §1.3's forbidden pairs, plus one EXTENDS entry.
    //
    //  * Warded + Reflective — the player cannot make progress and is punished
    //    for trying. The two together are a wall that bites.
    //  * Splitting + Volatile — a death detonation that spawns two more
    //    detonations is a chain the player cannot read.
    //  * Phasing + Cascading — constant relocation plus permanent arena
    //    pollution has no stable answer.
    //  * Wakeful + Splitting — EXTENDS §1.3. Not in the document because
    //    Wakeful is one of its optional entries and was never costed against
    //    the shipping list. It is forbidden for exactly the reason
    //    Splitting + Volatile is: dies, splits, revives is a three-step death
    //    chain, and a player who cannot predict what a corpse does next stops
    //    reading corpses at all.
    static const FBreakerModifierPair BreakerForbiddenPairs[] =
    {
        { EBreakerEnemyModifier::Warded,    EBreakerEnemyModifier::Reflective },
        { EBreakerEnemyModifier::Splitting, EBreakerEnemyModifier::Volatile   },
        { EBreakerEnemyModifier::Phasing,   EBreakerEnemyModifier::Cascading  },
        { EBreakerEnemyModifier::Wakeful,   EBreakerEnemyModifier::Splitting  },
    };
}

TArray<EBreakerEnemyModifier> UBreakerEnemyModifierLibrary::GetAllModifiers()
{
    TArray<EBreakerEnemyModifier> All;
    for (int32 Index = static_cast<int32>(EBreakerEnemyModifier::None) + 1;
         Index < static_cast<int32>(EBreakerEnemyModifier::Count); ++Index)
    {
        All.Add(static_cast<EBreakerEnemyModifier>(Index));
    }
    return All;
}

FString UBreakerEnemyModifierLibrary::GetModifierName(EBreakerEnemyModifier Modifier)
{
    switch (Modifier)
    {
    case EBreakerEnemyModifier::Warded:      return TEXT("WARDED");
    case EBreakerEnemyModifier::Volatile:    return TEXT("VOLATILE");
    case EBreakerEnemyModifier::Fleetfoot:   return TEXT("FLEETFOOT");
    case EBreakerEnemyModifier::Anchored:    return TEXT("ANCHORED");
    case EBreakerEnemyModifier::Splitting:   return TEXT("SPLITTING");
    case EBreakerEnemyModifier::WardingAura: return TEXT("WARDING AURA");
    case EBreakerEnemyModifier::Reflective:  return TEXT("REFLECTIVE");
    case EBreakerEnemyModifier::Phasing:     return TEXT("PHASING");
    case EBreakerEnemyModifier::Cascading:   return TEXT("CASCADING");
    case EBreakerEnemyModifier::Wakeful:     return TEXT("WAKEFUL");
    default:                                 return FString();
    }
}

FString UBreakerEnemyModifierLibrary::GetModifierCounterplay(EBreakerEnemyModifier Modifier)
{
    // Phrased as what the PLAYER should do, never as what the modifier does to
    // them. A readout that says "takes 35% less damage" teaches nothing; one
    // that says "kill the one with the tethers" teaches the fight.
    switch (Modifier)
    {
    case EBreakerEnemyModifier::Warded:      return TEXT("Burst it, or bleed it — the ward ignores nothing but damage over time");
    case EBreakerEnemyModifier::Volatile:    return TEXT("Kill it at range, or leave the ground it dies on");
    case EBreakerEnemyModifier::Fleetfoot:   return TEXT("Fight it where it cannot circle");
    case EBreakerEnemyModifier::Anchored:    return TEXT("Do not get hit — the slow is the punish, not the damage");
    case EBreakerEnemyModifier::Splitting:   return TEXT("Kill it away from the pack");
    case EBreakerEnemyModifier::WardingAura: return TEXT("Kill this one first");
    case EBreakerEnemyModifier::Reflective:  return TEXT("Stop holding the trigger");
    case EBreakerEnemyModifier::Phasing:     return TEXT("Never fight it with your back to geometry");
    case EBreakerEnemyModifier::Cascading:   return TEXT("The floor is running out — move first, shoot second");
    case EBreakerEnemyModifier::Wakeful:     return TEXT("Finish it on the weak point or it gets up");
    default:                                 return FString();
    }
}

FLinearColor UBreakerEnemyModifierLibrary::GetModifierColor(EBreakerEnemyModifier Modifier)
{
    // Deliberately NOT saturated teal for any entry: O19's object-chroma law
    // reserves saturated teal for rift objects and suppression hardware, and a
    // modifier halo is neither.
    switch (Modifier)
    {
    case EBreakerEnemyModifier::Warded:      return FLinearColor(0.35f, 0.62f, 1.00f);   // cold blue shell
    case EBreakerEnemyModifier::Volatile:    return FLinearColor(1.00f, 0.42f, 0.08f);   // hot orange fuse
    case EBreakerEnemyModifier::Fleetfoot:   return FLinearColor(0.55f, 1.00f, 0.42f);   // quick green
    case EBreakerEnemyModifier::Anchored:    return FLinearColor(0.62f, 0.55f, 0.40f);   // heavy earth
    case EBreakerEnemyModifier::Splitting:   return FLinearColor(0.85f, 0.85f, 0.30f);   // seam yellow
    case EBreakerEnemyModifier::WardingAura: return FLinearColor(0.30f, 0.90f, 0.72f);   // tether mint
    case EBreakerEnemyModifier::Reflective:  return FLinearColor(0.92f, 0.92f, 1.00f);   // faceted white
    case EBreakerEnemyModifier::Phasing:     return FLinearColor(0.72f, 0.30f, 1.00f);   // blink violet
    case EBreakerEnemyModifier::Cascading:   return FLinearColor(0.72f, 0.18f, 0.30f);   // hazard crimson
    case EBreakerEnemyModifier::Wakeful:     return FLinearColor(1.00f, 0.82f, 0.35f);   // amber second wind
    default:                                 return FLinearColor::White;
    }
}

EBreakerModifierPressure UBreakerEnemyModifierLibrary::GetModifierPressure(EBreakerEnemyModifier Modifier)
{
    switch (Modifier)
    {
    case EBreakerEnemyModifier::Warded:
    case EBreakerEnemyModifier::WardingAura:
    case EBreakerEnemyModifier::Reflective:
    // Wakeful is Durability and not Attrition on purpose: what it costs the
    // player is a second health bar, which is exactly what the other three
    // cost, and the diversity rule exists to stop three of those stacking.
    case EBreakerEnemyModifier::Wakeful:
        return EBreakerModifierPressure::Durability;

    case EBreakerEnemyModifier::Volatile:
    case EBreakerEnemyModifier::Cascading:
        return EBreakerModifierPressure::SpaceDenial;

    case EBreakerEnemyModifier::Fleetfoot:
    case EBreakerEnemyModifier::Phasing:
        return EBreakerModifierPressure::Mobility;

    case EBreakerEnemyModifier::Splitting:
    case EBreakerEnemyModifier::Anchored:
    default:
        return EBreakerModifierPressure::Attrition;
    }
}

EBreakerModifierWeightClass UBreakerEnemyModifierLibrary::GetModifierWeightClass(EBreakerEnemyModifier Modifier)
{
    switch (Modifier)
    {
    case EBreakerEnemyModifier::Warded:
    case EBreakerEnemyModifier::Volatile:
    case EBreakerEnemyModifier::Fleetfoot:
    case EBreakerEnemyModifier::Anchored:
        return EBreakerModifierWeightClass::Common;

    case EBreakerEnemyModifier::Splitting:
    case EBreakerEnemyModifier::WardingAura:
    case EBreakerEnemyModifier::Reflective:
        return EBreakerModifierWeightClass::Uncommon;

    case EBreakerEnemyModifier::Phasing:
    case EBreakerEnemyModifier::Cascading:
    case EBreakerEnemyModifier::Wakeful:
    default:
        return EBreakerModifierWeightClass::Rare;
    }
}

float UBreakerEnemyModifierLibrary::GetModifierSelectionWeight(EBreakerEnemyModifier Modifier, const FBreakerEnemyModifierParams& Params)
{
    switch (GetModifierWeightClass(Modifier))
    {
    case EBreakerModifierWeightClass::Common:   return FMath::Max(0.0f, Params.CommonWeight);
    case EBreakerModifierWeightClass::Uncommon: return FMath::Max(0.0f, Params.UncommonWeight);
    case EBreakerModifierWeightClass::Rare:
    default:                                    return FMath::Max(0.0f, Params.RareWeight);
    }
}

bool UBreakerEnemyModifierLibrary::AreModifiersForbiddenTogether(EBreakerEnemyModifier A, EBreakerEnemyModifier B)
{
    if (A == EBreakerEnemyModifier::None || B == EBreakerEnemyModifier::None) return false;
    if (A == B) return true;   // §1.3: "any modifier with itself (obviously)"
    for (const BreakerModifierDetail::FBreakerModifierPair& Pair : BreakerModifierDetail::BreakerForbiddenPairs)
    {
        if ((Pair.A == A && Pair.B == B) || (Pair.A == B && Pair.B == A)) return true;
    }
    return false;
}

int32 UBreakerEnemyModifierLibrary::GetDistinctPressureCount(const TArray<EBreakerEnemyModifier>& Modifiers)
{
    TSet<EBreakerModifierPressure> Kinds;
    for (const EBreakerEnemyModifier Modifier : Modifiers)
    {
        if (Modifier == EBreakerEnemyModifier::None) continue;
        Kinds.Add(GetModifierPressure(Modifier));
    }
    return Kinds.Num();
}

bool UBreakerEnemyModifierLibrary::IsLegalModifierSet(const TArray<EBreakerEnemyModifier>& Modifiers, FString& OutReason)
{
    OutReason.Reset();
    if (Modifiers.Num() > BreakerMaxModifiersPerEnemy)
    {
        OutReason = FString::Printf(TEXT("%d modifiers exceeds the cap of %d"), Modifiers.Num(), BreakerMaxModifiersPerEnemy);
        return false;
    }

    for (int32 I = 0; I < Modifiers.Num(); ++I)
    {
        if (Modifiers[I] == EBreakerEnemyModifier::None)
        {
            OutReason = TEXT("None is not a modifier");
            return false;
        }
        for (int32 J = I + 1; J < Modifiers.Num(); ++J)
        {
            if (AreModifiersForbiddenTogether(Modifiers[I], Modifiers[J]))
            {
                OutReason = FString::Printf(TEXT("%s + %s is a forbidden pair"),
                    *GetModifierName(Modifiers[I]), *GetModifierName(Modifiers[J]));
                return false;
            }
        }
    }

    // §1.3's required-diversity rule. A 2- or 3-modifier enemy must draw from
    // at least two pressure kinds, or it is one problem stated twice.
    if (Modifiers.Num() >= 2 && GetDistinctPressureCount(Modifiers) < 2)
    {
        OutReason = TEXT("A multi-modifier enemy must draw from at least two pressure kinds");
        return false;
    }

    // ...and a 3-modifier enemy may not take two Durability entries. §1.3:
    // "that is how sponges are born", which is also what O27 forbids.
    if (Modifiers.Num() >= 3)
    {
        int32 DurabilityCount = 0;
        for (const EBreakerEnemyModifier Modifier : Modifiers)
        {
            if (GetModifierPressure(Modifier) == EBreakerModifierPressure::Durability) ++DurabilityCount;
        }
        if (DurabilityCount >= 2)
        {
            OutReason = TEXT("A 3-modifier enemy may not draw two Durability modifiers");
            return false;
        }
    }
    return true;
}

EBreakerModifierFamilyScope UBreakerEnemyModifierLibrary::GetModifierFamilyScope(EBreakerEnemyModifier Modifier)
{
    switch (Modifier)
    {
    // Alien-body rules. A trained soldier does not segment into copies, does
    // not desaturate out of the world, and is not made of faceted shell.
    case EBreakerEnemyModifier::Splitting:
    case EBreakerEnemyModifier::Phasing:
    case EBreakerEnemyModifier::Reflective:
        return EBreakerModifierFamilyScope::VestigeOnly;

    // Tactical rules. Anchored plants a stance to hold ground; Warding Aura
    // protects the squad. Both are DECISIONS, and Story-Source §1.5 says a
    // Vestige has no tactics that resemble a military.
    case EBreakerEnemyModifier::Anchored:
    case EBreakerEnemyModifier::WardingAura:
        return EBreakerModifierFamilyScope::AlteredOnly;

    // Warded, Volatile, Fleetfoot, Cascading, Wakeful: things a body does
    // rather than things a mind chooses. Legal on either family.
    default:
        return EBreakerModifierFamilyScope::Any;
    }
}

bool UBreakerEnemyModifierLibrary::IsModifierAllowedOnFamily(EBreakerEnemyModifier Modifier, EBreakerEnemyFamily Family)
{
    switch (GetModifierFamilyScope(Modifier))
    {
    case EBreakerModifierFamilyScope::VestigeOnly: return Family == EBreakerEnemyFamily::Vestige;
    case EBreakerModifierFamilyScope::AlteredOnly: return Family == EBreakerEnemyFamily::Altered;
    case EBreakerModifierFamilyScope::Any:
    default:                                       return true;
    }
}

bool UBreakerEnemyModifierLibrary::IsLegalModifierSetForFamily(const TArray<EBreakerEnemyModifier>& Modifiers,
    EBreakerEnemyFamily Family, FString& OutReason)
{
    if (!IsLegalModifierSet(Modifiers, OutReason)) return false;
    for (const EBreakerEnemyModifier Modifier : Modifiers)
    {
        if (IsModifierAllowedOnFamily(Modifier, Family)) continue;
        OutReason = FString::Printf(TEXT("%s is not legal on a %s"),
            *GetModifierName(Modifier), *UBreakerEnemyFamilyLibrary::GetFamilyName(Family));
        return false;
    }
    return true;
}

int32 UBreakerEnemyModifierLibrary::RollModifierCount(int32 Seed, const FBreakerEnemyModifierParams& Params)
{
    const float W1 = FMath::Max(0.0f, Params.WeightOneModifier);
    const float W2 = FMath::Max(0.0f, Params.WeightTwoModifiers);
    const float W3 = FMath::Max(0.0f, Params.WeightThreeModifiers);
    const float Total = W1 + W2 + W3;
    // A parameter block that zeroes every count weight still has to produce a
    // modifier-bearing enemy: the caller already decided this one carries
    // modifiers, and returning zero here would silently downgrade it to trash.
    if (Total <= 0.0f) return 1;

    FRandomStream Stream(Seed);
    const float Roll = Stream.FRandRange(0.0f, Total);
    if (Roll < W1) return 1;
    if (Roll < W1 + W2) return 2;
    return 3;
}

TArray<EBreakerEnemyModifier> UBreakerEnemyModifierLibrary::RollModifierSet(int32 Seed, int32 DesiredCount,
    const FBreakerEnemyModifierParams& Params, EBreakerEnemyFamily Family)
{
    TArray<EBreakerEnemyModifier> Chosen;
    const int32 Target = FMath::Clamp(DesiredCount, 0, BreakerMaxModifiersPerEnemy);
    if (Target == 0) return Chosen;

    // Seed offset keeps the count roll and the set roll from sharing a stream
    // position, so two enemies whose counts happen to match do not also get
    // identical sets.
    FRandomStream Stream(HashCombine(static_cast<uint32>(Seed), 0x4D4F4446u /* 'MODF' */));
    const TArray<EBreakerEnemyModifier> Pool = GetAllModifiers();

    for (int32 Slot = 0; Slot < Target; ++Slot)
    {
        // §1.4: re-roll a conflicting draw up to three times, then give up on
        // this slot and let the count drop. Bounded, so selection always
        // terminates even with a pathological parameter block.
        bool bPlaced = false;
        for (int32 Attempt = 0; Attempt < 3 && !bPlaced; ++Attempt)
        {
            // Build the legal remainder for THIS draw, then weight it. Doing
            // the legality filter before the weighted pick rather than after is
            // what makes a rejected draw cost an attempt instead of a whole
            // slot, and it is why the sweep never emits an illegal set.
            TArray<EBreakerEnemyModifier> Legal;
            TArray<float> Weights;
            float Total = 0.0f;
            for (const EBreakerEnemyModifier Candidate : Pool)
            {
                if (Chosen.Contains(Candidate)) continue;
                // The family gate is applied at SELECTION, not as a filter over
                // a finished set, so a Vestige that rolls Anchored simply never
                // rolls it rather than losing a modifier slot to it.
                if (!IsModifierAllowedOnFamily(Candidate, Family)) continue;
                TArray<EBreakerEnemyModifier> Trial = Chosen;
                Trial.Add(Candidate);
                FString Reason;
                // A partial set of size 2 must not be rejected for failing a
                // rule that only applies at size 3, so legality is checked
                // against the trial set as it stands. The final set is checked
                // again by the caller-facing test sweep.
                if (!IsLegalModifierSet(Trial, Reason)) continue;
                const float Weight = GetModifierSelectionWeight(Candidate, Params);
                if (Weight <= 0.0f) continue;
                Legal.Add(Candidate);
                Weights.Add(Weight);
                Total += Weight;
            }
            if (Legal.IsEmpty() || Total <= 0.0f) break;

            float Roll = Stream.FRandRange(0.0f, Total);
            for (int32 Index = 0; Index < Legal.Num(); ++Index)
            {
                Roll -= Weights[Index];
                if (Roll <= 0.0f || Index == Legal.Num() - 1)
                {
                    Chosen.Add(Legal[Index]);
                    bPlaced = true;
                    break;
                }
            }
        }
        if (!bPlaced) break;
    }

    // A 2-set that ended up single-pressure cannot happen (the trial check
    // above rejects it), but a 3-set whose third slot failed can leave a legal
    // 2-set — which is exactly §1.4's "the count drops by one".
    return Chosen;
}

TArray<EBreakerEnemyModifier> UBreakerEnemyModifierLibrary::RollModifiers(int32 Seed,
    const FBreakerEnemyModifierParams& Params, EBreakerEnemyFamily Family)
{
    return RollModifierSet(Seed, RollModifierCount(Seed, Params), Params, Family);
}

float UBreakerEnemyModifierLibrary::GetModifierCountHealthMultiplier(int32 ModifierCount, const FBreakerEnemyModifierParams& Params)
{
    if (ModifierCount <= 1) return 1.0f;
    return 1.0f + FMath::Max(0.0f, Params.HealthStepPerExtraModifier) * static_cast<float>(ModifierCount - 1);
}

EBreakerMonsterRank UBreakerEnemyModifierLibrary::GetRankForModifierCount(int32 ModifierCount)
{
    return ModifierCount > 0 ? EBreakerMonsterRank::ModifierBearing : EBreakerMonsterRank::Trash;
}

float UBreakerEnemyModifierLibrary::GetWardShieldAmount(float MonsterMaxHealth, const FBreakerEnemyModifierParams& Params)
{
    return FMath::Max(0.0f, MonsterMaxHealth) * FMath::Max(0.0f, Params.WardShieldFractionOfHealth);
}

float UBreakerEnemyModifierLibrary::GetVolatileDetonationDamage(float MonsterAttackDamage, const FBreakerEnemyModifierParams& Params)
{
    return FMath::Max(0.0f, MonsterAttackDamage) * FMath::Max(0.0f, Params.VolatileDamageAsAttackMultiple);
}

float UBreakerEnemyModifierLibrary::GetVolatileFalloff(float DistanceCm, const FBreakerEnemyModifierParams& Params)
{
    const float Inner = FMath::Max(0.0f, Params.VolatileInnerRadiusCm);
    const float Outer = FMath::Max(Inner, Params.VolatileOuterRadiusCm);
    if (DistanceCm <= Inner) return 1.0f;
    if (DistanceCm >= Outer) return 0.0f;
    // Inner == Outer with a distance strictly between them is impossible, so
    // the divide below cannot be by zero.
    return 1.0f - (DistanceCm - Inner) / (Outer - Inner);
}

float UBreakerEnemyModifierLibrary::GetReflectDamage(float IncomingDamage, float MonsterMaxHealth, const FBreakerEnemyModifierParams& Params)
{
    const float Raw = FMath::Max(0.0f, IncomingDamage) * FMath::Clamp(Params.ReflectFraction, 0.0f, 1.0f);
    const float Cap = FMath::Max(0.0f, MonsterMaxHealth) * FMath::Clamp(Params.ReflectCapFractionOfMonsterHealth, 0.0f, 1.0f);
    // A zero cap means "no reflect", not "uncapped". The cap is the part of
    // this modifier that keeps it from being a tax on damage.
    return FMath::Min(Raw, Cap);
}

float UBreakerEnemyModifierLibrary::GetHazardTickDamage(float MonsterAttackDamage, const FBreakerEnemyModifierParams& Params)
{
    return FMath::Max(0.0f, MonsterAttackDamage) * FMath::Max(0.0f, Params.HazardTickDamageAsAttackFraction);
}

FVector UBreakerEnemyModifierLibrary::GetPhaseDestination(const FVector& Origin, const FVector& Target, const FBreakerEnemyModifierParams& Params)
{
    const FVector Flat = FVector(Target.X - Origin.X, Target.Y - Origin.Y, 0.0f);
    const float Distance = Flat.Size();
    const float Standoff = FMath::Max(0.0f, Params.PhaseMinimumStandoffCm);
    // Already inside the standoff: a blink toward a target it is on top of
    // would be a jitter, so it does not move at all.
    if (Distance <= Standoff || Distance <= KINDA_SMALL_NUMBER) return Origin;

    const float Step = FMath::Min(FMath::Max(0.0f, Params.PhaseDistanceCm), Distance - Standoff);
    return Origin + Flat / Distance * Step;
}

FString UBreakerEnemyModifierLibrary::GetModifierBanner(const TArray<EBreakerEnemyModifier>& Modifiers)
{
    TArray<FString> Names;
    for (const EBreakerEnemyModifier Modifier : Modifiers)
    {
        if (Modifier == EBreakerEnemyModifier::None) continue;
        Names.Add(GetModifierName(Modifier));
    }
    return FString::Join(Names, TEXT(" | "));
}
