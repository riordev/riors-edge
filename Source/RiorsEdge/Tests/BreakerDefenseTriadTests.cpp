// The defense triad (owner ruling 2026-08-16/17): Physical Damage Reduction
// (bullets), Ailment Avoidance (a roll to refuse status inflictions), and
// Elemental Resistance (authored-but-ungated until elemental incoming exists).
// These tests pin the three properties the ruling names: the avoidance roll is
// deterministic and gated like dodge, the aggregation caps hold, and each leg
// has exactly the pool membership its honesty demands.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemTypes.h"

namespace
{
    FBreakerStatusApplicationSpec BreakerTriadMakeStatus(const TCHAR* TagName, float Duration = 3.0f)
    {
        FBreakerStatusApplicationSpec Spec;
        Spec.StatusTag = FGameplayTag::RequestGameplayTag(TagName, false);
        Spec.BaseDamagePerTick = 6.0f;
        Spec.Duration = Duration;
        Spec.TickInterval = 0.5f;
        return Spec;
    }

    FBreakerItemInstance BreakerTriadMakeItem(EBreakerEquipSlot Slot, std::initializer_list<TPair<const TCHAR*, float>> Lines)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.Slot = Slot;
        Item.Rarity = EBreakerItemRarity::Exceptional;
        Item.ItemLevel = 50;
        for (const TPair<const TCHAR*, float>& Line : Lines)
        {
            FBreakerRolledAffix Rolled;
            Rolled.AffixId = FName(Line.Key);
            Rolled.Tier = 6;
            Rolled.Value = Line.Value;
            Item.Affixes.Add(Rolled);
        }
        return Item;
    }

    // Runs one fixed application sequence against a fresh component at the
    // given baseline chance and returns the landed/avoided verdict pattern.
    FString BreakerTriadRollPattern(float BaselineChance, int32 Applications)
    {
        AActor* Target = NewObject<AActor>();
        UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
        Status->AilmentAvoidanceChance = BaselineChance;

        FString Pattern;
        for (int32 Index = 0; Index < Applications; ++Index)
        {
            // Distinct tags so every application is a fresh status rather
            // than a stack add — the verdict is then readable off the list.
            const int32 Before = Status->GetActiveStatuses().Num();
            Status->ApplyStatus(
                BreakerTriadMakeStatus(*FString::Printf(TEXT("Status.TriadProbe%d"), Index % 2 == 0 ? 0 : 1)),
                EBreakerDamageFamily::Physical, nullptr);
            Pattern += Status->GetActiveStatuses().Num() > Before ? TEXT("L") : TEXT("A");
            // Consume so the next same-tag application is a new status, not a
            // refresh — a refresh also rolls, but the list length would not
            // move and the pattern would misread a landed refresh as avoided.
            Status->ConsumeAllStatuses();
        }
        return Pattern;
    }
}

// ---------------------------------------------------------------------------
// Avoidance determinism: same component state, same application sequence,
// same verdicts — the dodge property, at the status door.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTriadAvoidanceDeterminismTest,
    "RiorsEdge.Combat.DefenseTriad.AvoidanceDeterminism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTriadAvoidanceDeterminismTest::RunTest(const FString& Parameters)
{
    const FString First = BreakerTriadRollPattern(0.5f, 24);
    const FString Second = BreakerTriadRollPattern(0.5f, 24);
    TestEqual(TEXT("An identical application sequence produces identical verdicts"), First, Second);

    // A seeded 50% roll over 24 applications must actually exercise both
    // verdicts — a stream that always lands (or always avoids) would be the
    // salt/seed silently degenerating, which determinism alone cannot catch.
    TestTrue(TEXT("The 50% stream lands at least once"), First.Contains(TEXT("L")));
    TestTrue(TEXT("The 50% stream avoids at least once"), First.Contains(TEXT("A")));

    // Ticks never re-roll: a landed status advanced through its whole life
    // keeps ticking even at the avoidance ceiling. Avoidance lives only at
    // the application door.
    AActor* Target = NewObject<AActor>();
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
    // The tick path refuses to run without a combat sink on the owner
    // (AdvanceStatuses' guard), and a NewObject rig never runs BeginPlay —
    // register a combat component so the lazy re-bind can find it.
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddOwnedComponent(Combat);
    Status->ApplyStatus(BreakerTriadMakeStatus(TEXT("Status.TriadBleed")), EBreakerDamageFamily::Physical, nullptr);
    if (!TestEqual(TEXT("The probe status landed"), Status->GetActiveStatuses().Num(), 1)) return false;
    Status->AilmentAvoidanceChance = 1.0f;
    Status->AdvanceStatuses(1.0f);
    TestEqual(TEXT("A landed status is untouched by avoidance on later ticks"), Status->GetActiveStatuses().Num(), 1);
    TestTrue(TEXT("The landed status kept delivering ticks"), Status->GetActiveStatuses()[0].TicksDelivered >= 1);
    return true;
}

// ---------------------------------------------------------------------------
// Avoidance gate: zero never refuses, the ceiling holds, the two statements
// of the cap agree, and the refusal is a full refusal with a visible tell.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTriadAvoidanceGateTest,
    "RiorsEdge.Combat.DefenseTriad.AvoidanceGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTriadAvoidanceGateTest::RunTest(const FString& Parameters)
{
    // Chance zero is a hard gate: every application in a long sequence lands.
    const FString ZeroPattern = BreakerTriadRollPattern(0.0f, 24);
    TestFalse(TEXT("Zero avoidance never refuses an application"), ZeroPattern.Contains(TEXT("A")));

    // The composed ceiling: a baseline of 1.0 clamps to the cap, and the
    // component's constant agrees with the gear cap it restates (the one
    // ceiling, stated twice because Items/ is not includable from the Combat
    // header — this is the pin that keeps them from drifting).
    AActor* Target = NewObject<AActor>();
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
    Status->AilmentAvoidanceChance = 1.0f;
    TestEqual(TEXT("The effective chance clamps at the ceiling"),
        Status->GetEffectiveAilmentAvoidanceChance(), UBreakerStatusComponent::MaxAilmentAvoidanceChance);
    TestEqual(TEXT("The Combat ceiling and the Items gear cap are the same number"),
        UBreakerStatusComponent::MaxAilmentAvoidanceChance,
        FBreakerEquipmentStats::AilmentAvoidanceCapPercent / 100.0f);
    TestTrue(TEXT("The ceiling is below certainty — immunity stays the immunity primitive's"),
        UBreakerStatusComponent::MaxAilmentAvoidanceChance < 1.0f);

    // A refused application refuses EVERYTHING (no status, no stacks) and
    // broadcasts the avoided tell exactly once per refusal; landed + avoided
    // always accounts for every application.
    int32 AvoidedCount = 0;
    // A capped component refusing at 75%: over 24 distinct applications the
    // seeded stream must refuse at least once, and every refusal must have
    // left no status behind.
    AActor* Capped = NewObject<AActor>();
    UBreakerStatusComponent* CappedStatus = NewObject<UBreakerStatusComponent>(Capped);
    CappedStatus->AilmentAvoidanceChance = 1.0f;   // clamps to the 0.75 ceiling
    int32 Landed = 0;
    for (int32 Index = 0; Index < 24; ++Index)
    {
        const int32 Before = CappedStatus->GetActiveStatuses().Num();
        CappedStatus->ApplyStatus(
            BreakerTriadMakeStatus(*FString::Printf(TEXT("Status.TriadGate%d"), Index)),
            EBreakerDamageFamily::Physical, nullptr);
        if (CappedStatus->GetActiveStatuses().Num() > Before) ++Landed;
        else ++AvoidedCount;
    }
    TestTrue(TEXT("The capped stream refuses applications"), AvoidedCount > 0);
    TestEqual(TEXT("Every application either landed or was avoided"), Landed + AvoidedCount, 24);
    TestEqual(TEXT("Refused applications left no status behind"), CappedStatus->GetActiveStatuses().Num(), Landed);

    // Avoidance rolls BEFORE immunity and neither weakens the other: with
    // zero avoidance and a live immunity window, the application is still
    // refused (by immunity), so the roll cannot have swallowed the primitive.
    AActor* Immune = NewObject<AActor>();
    UBreakerStatusComponent* ImmuneStatus = NewObject<UBreakerStatusComponent>(Immune);
    ImmuneStatus->GrantStatusImmunity(5.0f);
    ImmuneStatus->ApplyStatus(BreakerTriadMakeStatus(TEXT("Status.TriadImmune")), EBreakerDamageFamily::Physical, nullptr);
    TestEqual(TEXT("Immunity still refuses when avoidance is zero"), ImmuneStatus->GetActiveStatuses().Num(), 0);
    return true;
}

// ---------------------------------------------------------------------------
// Aggregation: the three legs fold, each against its own named cap, and the
// gear leg reaches the status component's composed chance.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTriadAggregationTest,
    "RiorsEdge.Items.DefenseTriad.Aggregation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTriadAggregationTest::RunTest(const FString& Parameters)
{
    // In-band values sum per leg.
    TArray<FBreakerItemInstance> Items;
    Items.Add(BreakerTriadMakeItem(EBreakerEquipSlot::Helmet,
        {{TEXT("Core.PhysicalDR"), 7.0f}, {TEXT("Core.AilmentAvoidance"), 9.0f}}));
    Items.Add(BreakerTriadMakeItem(EBreakerEquipSlot::BodyArmour,
        {{TEXT("Core.PhysicalDR"), 6.0f}, {TEXT("Core.ElementalResist"), 8.0f}}));
    Items.Add(BreakerTriadMakeItem(EBreakerEquipSlot::Waist,
        {{TEXT("Core.AilmentAvoidance"), 8.0f}, {TEXT("Core.ElementalResist"), 7.0f}}));

    const FBreakerEquipmentStats Stats = UBreakerEquipmentComponent::AggregateStats(Items);
    TestEqual(TEXT("Physical DR lines sum"), Stats.PhysicalDamageReductionPercent, 13.0f, 0.001f);
    TestEqual(TEXT("Ailment avoidance lines sum"), Stats.AilmentAvoidanceChancePercent, 17.0f, 0.001f);
    TestEqual(TEXT("Elemental resistance lines sum (via the FindAffix fallback)"), Stats.ElementalResistancePercent, 15.0f, 0.001f);

    // Stacked past the ceilings, each leg clamps at its own cap.
    TArray<FBreakerItemInstance> Stacked;
    Stacked.Add(BreakerTriadMakeItem(EBreakerEquipSlot::Helmet,
        {{TEXT("Core.PhysicalDR"), 40.0f}, {TEXT("Core.AilmentAvoidance"), 50.0f}, {TEXT("Core.ElementalResist"), 45.0f}}));
    Stacked.Add(BreakerTriadMakeItem(EBreakerEquipSlot::Boots,
        {{TEXT("Core.PhysicalDR"), 40.0f}, {TEXT("Core.AilmentAvoidance"), 50.0f}, {TEXT("Core.ElementalResist"), 45.0f}}));
    const FBreakerEquipmentStats Capped = UBreakerEquipmentComponent::AggregateStats(Stacked);
    TestEqual(TEXT("Physical DR clamps at its default cap"),
        Capped.PhysicalDamageReductionPercent, FBreakerEquipmentStats::DefaultPhysicalDamageReductionCap, 0.001f);
    TestEqual(TEXT("Ailment avoidance clamps at its cap"),
        Capped.AilmentAvoidanceChancePercent, FBreakerEquipmentStats::AilmentAvoidanceCapPercent, 0.001f);
    TestEqual(TEXT("Elemental resistance clamps at its cap"),
        Capped.ElementalResistancePercent, FBreakerEquipmentStats::ElementalResistanceCapPercent, 0.001f);

    // Gear avoidance reaches the composed chance the roll actually uses.
    AActor* Wearer = NewObject<AActor>();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Wearer);
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Wearer);
    Equipment->EquipItem(BreakerTriadMakeItem(EBreakerEquipSlot::Helmet, {{TEXT("Core.AilmentAvoidance"), 20.0f}}));
    Status->AilmentAvoidanceChance = 0.1f;
    TestEqual(TEXT("Baseline and gear compose into the effective chance"),
        Status->GetEffectiveAilmentAvoidanceChance(), 0.3f, 0.001f);

    // The Elemental read site consumes the resistance: an Elemental hit is
    // reduced by the gear percentage, and a TrueDamage hit answers to
    // neither leg.
    AActor* Defender = NewObject<AActor>();
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Defender);
    UBreakerEquipmentComponent* DefenderGear = NewObject<UBreakerEquipmentComponent>(Defender);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Defender);
    Combat->BindAttributes(Attributes);
    DefenderGear->EquipItem(BreakerTriadMakeItem(EBreakerEquipSlot::BodyArmour, {{TEXT("Core.ElementalResist"), 18.0f}}));

    FBreakerDamageRequest Elemental;
    Elemental.BaseDamage = 100.0f;
    Elemental.DamageFamily = EBreakerDamageFamily::Elemental;
    Elemental.bCanCritical = false;
    const FBreakerDamageResult ElementalResult = Combat->ReceiveDamage(Elemental);
    TestEqual(TEXT("Elemental resistance reduces an Elemental hit"), ElementalResult.MitigatedDamage, 82.0f, 0.001f);

    FBreakerDamageRequest True;
    True.BaseDamage = 100.0f;
    True.DamageFamily = EBreakerDamageFamily::TrueDamage;
    True.bCanCritical = false;
    const FBreakerDamageResult TrueResult = Combat->ReceiveDamage(True);
    TestEqual(TEXT("TrueDamage ignores the triad entirely"), TrueResult.MitigatedDamage, 100.0f, 0.001f);
    return true;
}

// ---------------------------------------------------------------------------
// Pool membership: the droppable legs are droppable, the ungated leg is
// resolvable but never rollable, and every leg's definition says what its
// consumer consumes.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTriadPoolMembershipTest,
    "RiorsEdge.Items.DefenseTriad.PoolMembership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTriadPoolMembershipTest::RunTest(const FString& Parameters)
{
    const TArray<FBreakerAffixDefinition>& Slice = UBreakerAffixLibrary::GetSliceAffixPool();
    auto SliceHas = [&Slice](const TCHAR* Id)
    {
        const FName AffixId(Id);
        return Slice.ContainsByPredicate([&AffixId](const FBreakerAffixDefinition& Affix) { return Affix.AffixId == AffixId; });
    };

    // The two live legs roll from the ordinary pool at every rarity (slice
    // membership IS rarity-universality: the generic loop draws Standard
    // through Anomalous from this one array, and both lines default to
    // MinimumRarity Standard).
    TestTrue(TEXT("Physical DR is droppable"), SliceHas(TEXT("Core.PhysicalDR")));
    TestTrue(TEXT("Ailment Avoidance is droppable"), SliceHas(TEXT("Core.AilmentAvoidance")));
    const FBreakerAffixDefinition* Avoidance = UBreakerAffixLibrary::FindAffix(Slice, TEXT("Core.AilmentAvoidance"));
    if (TestNotNull(TEXT("The avoidance definition resolves"), Avoidance))
    {
        TestEqual(TEXT("Avoidance rolls at the ordinary rarity floor"),
            Avoidance->MinimumRarity, EBreakerItemRarity::Standard);
        TestEqual(TEXT("Avoidance feeds the AilmentAvoidance target"),
            Avoidance->StatTarget, EBreakerStatTarget::AilmentAvoidance);
        TestFalse(TEXT("Avoidance is a defensive line, not offence"),
            UBreakerAffixLibrary::IsOffensiveTarget(Avoidance->StatTarget));
    }

    // The ungated leg: absent from EVERY droppable pool — slice, Aberrant,
    // Anomalous — yet resolvable through FindAffix so an item carrying it
    // aggregates truthfully. That combination is the stated choice: the
    // line is honest, and the pool refuses to sell it before it can pay.
    const FName ElementalId(TEXT("Core.ElementalResist"));
    auto PoolHas = [&ElementalId](const TArray<FBreakerAffixDefinition>& Pool)
    {
        return Pool.ContainsByPredicate([&ElementalId](const FBreakerAffixDefinition& Affix) { return Affix.AffixId == ElementalId; });
    };
    TestFalse(TEXT("Elemental Resistance is not in the slice pool"), PoolHas(Slice));
    TestFalse(TEXT("Elemental Resistance is not in the Aberrant pool"), PoolHas(UBreakerAffixLibrary::GetAberrantAffixPool()));
    TestFalse(TEXT("Elemental Resistance is not in the Anomalous pool"), PoolHas(UBreakerAffixLibrary::GetAnomalousAffixPool()));

    const FBreakerAffixDefinition* Resist = UBreakerAffixLibrary::FindAffix(Slice, ElementalId);
    if (TestNotNull(TEXT("Elemental Resistance still resolves through the fallback"), Resist))
    {
        TestEqual(TEXT("It feeds the reserved ElementalDamageReduction target"),
            Resist->StatTarget, EBreakerStatTarget::ElementalDamageReduction);
        TestEqual(TEXT("Its band mirrors the Physical DR reference floor"), Resist->ValueAtT12, 2.0f);
        TestEqual(TEXT("Its band mirrors the Physical DR reference ceiling"), Resist->ValueAtT1, 18.0f);
    }
    return true;
}

#endif
