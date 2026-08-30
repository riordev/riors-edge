#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerItemTypes.h"
#include "Items/BreakerLootLibrary.h"

// ---------------------------------------------------------------------------
// THE UNIVERSAL-SHARE MEASUREMENT — the affix brief's instrument.
// ---------------------------------------------------------------------------
// Two questions about the slice pool's per-slot identity, MEASURED rather than
// argued, so the brief's pre-committed thresholds (a 40% absolute per-slot
// universal-share ceiling, the below-baseline ratchet, 50% pairwise overlap
// excluding the Necklace wildcard) can be judged against numbers:
//
//   1. What share of the affix lines a slot actually rolls comes from
//      ALL-EIGHT-SLOTS (universal) lines? Reported two ways per slot:
//      FIRST-DRAW-WEIGHT — the analytic single-draw probability, straight off
//      RollWeight, leans ignored — and ROLLED — the realized share over the
//      live pipeline, which folds in the rarity mix at the item level, the
//      without-replacement candidate walk, the archetype leans on rolled
//      weapons, and the special seats' prefix/suffix pressure. The two
//      disagreeing is information: it is the distance between the pool as
//      authored and the pool as experienced.
//
//   2. How alike are two slots? PAIRWISE WEIGHTED OVERLAP: normalise each
//      slot's legal slice-pool lines into a weight distribution and sum the
//      per-line minima — 1.0 is "these two slots offer the same hunt",
//      0.0 is fully disjoint. Analytic, leans ignored, so it measures the
//      AUTHORED identity table and not a sampling accident.
//
// MEASUREMENT BASIS, stated because a basis nobody states is a basis nobody
// can re-run: item levels 10 / 50 / 120; rarity from the live gated table
// (RollGatedRarity, rank Elite so every rank gate is open and the ilvl gates
// alone shape the mix, zero Drop Chance bonus, default FBreakerDropTableParams);
// slot fixed per cell; items rolled by the live RollItem. SLICE-POOL LINES
// ONLY: a rolled line that does not resolve in the slice pool (Aberrant /
// Anomalous seats, their bills, legendary extras) is excluded from both counts
// — the special seats are a rarity's identity, not the pool's slot identity.
// N = 2000 items per (slot, item level); seeds are
// HashCombine(HashCombine(i, Slot * 8009), ItemLevel * 104729), i in
// [0, 2000), deterministic so any run reproduces these figures exactly.
//
// THIS TEST ASSERTS ONLY ITS OWN INSTRUMENT (samples landed, shares are
// probabilities, distributions normalise). The thresholds are the brief's to
// judge and the seat's to promote to enumerated reds — pre-committed numbers
// get cited in the report, not smuggled into assertions the same cycle they
// are first measured.
// ---------------------------------------------------------------------------

namespace BreakerAffixShareTest
{
    constexpr int32 ShareSampleCount = 2000;

    bool ShareIsUniversal(const FBreakerAffixDefinition& Definition)
    {
        for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
        {
            if (!Definition.AllowsSlot(static_cast<EBreakerEquipSlot>(SlotIndex))) return false;
        }
        return true;
    }

    FString ShareSlotName(int32 SlotIndex)
    {
        const FString Full = UEnum::GetValueAsString(static_cast<EBreakerEquipSlot>(SlotIndex));
        FString Left, Right;
        return Full.Split(TEXT("::"), &Left, &Right) ? Right : Full;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAffixUniversalShareTest,
    "RiorsEdge.Items.Affixes.UniversalShare",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAffixUniversalShareTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAffixShareTest;
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    const int32 SlotCount = static_cast<int32>(EBreakerEquipSlot::Count);

    AddInfo(FString::Printf(TEXT("AFFIX SHARE basis: live pipeline, rank Elite, bonus 0, default params; slice-pool lines only; N=%d per slot per ilvl; seeds HashCombine(HashCombine(i, slot*8009), ilvl*104729), i in [0,%d)"),
        ShareSampleCount, ShareSampleCount));

    // ---- The analytic halves: first-draw weight share and pairwise overlap.
    float FirstDrawShare[static_cast<int32>(EBreakerEquipSlot::Count)] = {};
    TArray<TArray<float>> SlotDistributions;   // per slot, per pool index: normalised weight
    SlotDistributions.SetNum(SlotCount);
    for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
    {
        const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
        SlotDistributions[SlotIndex].SetNumZeroed(Pool.Num());
        float TotalWeight = 0.0f;
        float UniversalWeight = 0.0f;
        for (int32 PoolIndex = 0; PoolIndex < Pool.Num(); ++PoolIndex)
        {
            const FBreakerAffixDefinition& Affix = Pool[PoolIndex];
            if (!Affix.AllowsSlot(Slot)) continue;
            SlotDistributions[SlotIndex][PoolIndex] = Affix.RollWeight;
            TotalWeight += Affix.RollWeight;
            if (ShareIsUniversal(Affix)) UniversalWeight += Affix.RollWeight;
        }
        if (!TestTrue(TEXT("every slot offers weighted lines"), TotalWeight > 0.0f)) return false;
        FirstDrawShare[SlotIndex] = UniversalWeight / TotalWeight;
        float Normalised = 0.0f;
        for (float& Weight : SlotDistributions[SlotIndex]) { Weight /= TotalWeight; Normalised += Weight; }
        TestEqual(TEXT("a slot's weight distribution normalises"), Normalised, 1.0f, 0.0001f);
    }

    // ---- The rolled half, per item level, over the live pipeline.
    const int32 ItemLevels[] = {10, 50, 120};
    for (const int32 ItemLevel : ItemLevels)
    {
        for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
        {
            const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
            int32 SliceLines = 0;
            int32 UniversalLines = 0;
            for (int32 Sample = 0; Sample < ShareSampleCount; ++Sample)
            {
                const int32 Seed = static_cast<int32>(HashCombine(
                    HashCombine(static_cast<uint32>(Sample), static_cast<uint32>(SlotIndex * 8009)),
                    static_cast<uint32>(ItemLevel * 104729)));
                const EBreakerItemRarity Rarity = UBreakerDropTableLibrary::RollGatedRarity(
                    Seed, ItemLevel, EBreakerMonsterRank::Elite, 0.0f, FBreakerDropTableParams());
                const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(
                    TEXT("ShareProbe"), Slot, Rarity, ItemLevel, Seed);
                for (const FBreakerRolledAffix& Rolled : Item.Affixes)
                {
                    const FBreakerAffixDefinition* Definition = Pool.FindByPredicate(
                        [&Rolled](const FBreakerAffixDefinition& Affix) { return Affix.AffixId == Rolled.AffixId; });
                    if (!Definition) continue;   // special seat / bill / non-slice extra
                    ++SliceLines;
                    if (ShareIsUniversal(*Definition)) ++UniversalLines;
                }
            }
            if (!TestTrue(*FString::Printf(TEXT("ilvl %d %s rolled lines at all"), ItemLevel, *ShareSlotName(SlotIndex)),
                SliceLines > 0)) return false;
            const float RolledShare = static_cast<float>(UniversalLines) / static_cast<float>(SliceLines);
            TestTrue(TEXT("a share is a probability"), RolledShare >= 0.0f && RolledShare <= 1.0f);
            AddInfo(FString::Printf(TEXT("AFFIX SHARE ilvl %3d  %-10s rolled %.3f (%d of %d lines) | first-draw-weight %.3f"),
                ItemLevel, *ShareSlotName(SlotIndex), RolledShare, UniversalLines, SliceLines, FirstDrawShare[SlotIndex]));
        }
    }

    // ---- Pairwise weighted overlap, every pair once, Necklace called out.
    float MaxOverlap = 0.0f, MaxOverlapExNecklace = 0.0f;
    FString MaxPair, MaxPairExNecklace;
    for (int32 A = 0; A < SlotCount; ++A)
    {
        for (int32 B = A + 1; B < SlotCount; ++B)
        {
            float Overlap = 0.0f;
            for (int32 PoolIndex = 0; PoolIndex < Pool.Num(); ++PoolIndex)
            {
                Overlap += FMath::Min(SlotDistributions[A][PoolIndex], SlotDistributions[B][PoolIndex]);
            }
            const FString Pair = ShareSlotName(A) + TEXT("~") + ShareSlotName(B);
            AddInfo(FString::Printf(TEXT("AFFIX OVERLAP %-22s %.3f"), *Pair, Overlap));
            if (Overlap > MaxOverlap) { MaxOverlap = Overlap; MaxPair = Pair; }
            const bool bTouchesNecklace = A == static_cast<int32>(EBreakerEquipSlot::Necklace)
                || B == static_cast<int32>(EBreakerEquipSlot::Necklace);
            if (!bTouchesNecklace && Overlap > MaxOverlapExNecklace) { MaxOverlapExNecklace = Overlap; MaxPairExNecklace = Pair; }
        }
    }
    AddInfo(FString::Printf(TEXT("AFFIX OVERLAP max %.3f (%s) | max ex-Necklace %.3f (%s) | brief thresholds: 40%% universal share ceiling, 50%% overlap ex-Necklace"),
        MaxOverlap, *MaxPair, MaxOverlapExNecklace, *MaxPairExNecklace));

    // Structural sanity the shares depend on: the pool splits into both kinds.
    int32 UniversalCount = 0;
    for (const FBreakerAffixDefinition& Affix : Pool) { if (ShareIsUniversal(Affix)) ++UniversalCount; }
    TestTrue(TEXT("universal lines exist"), UniversalCount > 0);
    TestTrue(TEXT("slot-limited lines exist"), UniversalCount < Pool.Num());
    return true;
}

#endif
