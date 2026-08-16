#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbility_Overdrive.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerProjectileBase.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Weapons/BreakerWeaponArchetype.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

// ---------------------------------------------------------------------------
// O34: THE MULTIPLIER CANON AND THE SINGLE CEILING
// ---------------------------------------------------------------------------
// Before this ruling two independent More budgets multiplied: the attribute
// aggregator clamped the tree/gear More product at 1.30^3 == 2.197, and the
// combat component's outgoing chain clamped ITS product at a separate
// hardcoded 2.20 — so a build at the tree ceiling plus an Overdrive window
// composed to ~2.75x against an authored budget of ~2.20. O34 rules ONE
// ceiling, derived from the aggregator, with temporary ability windows
// counting inside it. These tests pin every clause of that.
// ---------------------------------------------------------------------------

namespace BreakerCombatCeilingTest
{
    // Distinctively named for the unity build, per the project's twice-bitten
    // rule about anonymous-namespace collisions.

    UBreakerAttributeSet* CeilingTestMakeAttributes()
    {
        return NewObject<UBreakerAttributeSet>(GetTransientPackage());
    }

    UBreakerCombatComponent* CeilingTestMakeCombat(UBreakerAttributeSet* Attributes)
    {
        UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(GetTransientPackage());
        if (Attributes) Combat->BindAttributes(Attributes);
        return Combat;
    }

    // A progression offer holding Count tree Mores at the per-source ceiling.
    FBreakerAttributeContribution CeilingTestTreeMores(int32 Count)
    {
        FBreakerAttributeContribution Offer;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Offer.ComposeMore(EBreakerAggregatedAttribute::DamageMultiplier, FBreakerAttributeAggregator::SingleMoreCeiling);
        }
        return Offer;
    }
}

// (a) Three tree Mores near the ceiling plus an Overdrive window compose to at
// most the ONE ceiling — the window competes for headroom instead of stacking
// a second budget on top.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCeilingOneBudgetTest,
    "RiorsEdge.Combat.Ceiling.OneBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCeilingOneBudgetTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCombatCeilingTest;
    const float Ceiling = FBreakerAttributeAggregator::ComposedMoreCeiling();

    // ---- Partial budget first (no clamp, no warnings expected) ----------
    // Two tree Mores leave exactly one single-More ceiling of headroom, so
    // Overdrive's window (authored below that) must fit UNCLAMPED.
    {
        UBreakerAttributeSet* Attributes = CeilingTestMakeAttributes();
        Attributes->ApplyAttributeContribution(EBreakerAttributeContributor::Progression, CeilingTestTreeMores(2));
        UBreakerCombatComponent* Combat = CeilingTestMakeCombat(Attributes);
        Combat->PushOutgoingModifier(UBreakerAbility_Overdrive::OutgoingModifierKey(), 0.0f,
            UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.0f);

        TestEqual(TEXT("Under budget, the window applies at full authored value"),
            Combat->GetComposedMoreMultiplier(), UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.0001f);
        TestTrue(TEXT("Two tree Mores plus a window stay at or under the one ceiling"),
            Combat->GetAttributeSideMoreProduct() * Combat->GetComposedMoreMultiplier() <= Ceiling + UE_KINDA_SMALL_NUMBER);
    }

    // ---- Full budget: the window is squeezed to what remains -------------
    AddExpectedError(TEXT("exceeds the"), EAutomationExpectedErrorFlags::Contains, 0);
    {
        UBreakerAttributeSet* Attributes = CeilingTestMakeAttributes();
        Attributes->ApplyAttributeContribution(EBreakerAttributeContributor::Progression, CeilingTestTreeMores(3));
        UBreakerCombatComponent* Combat = CeilingTestMakeCombat(Attributes);

        // The attribute side alone sits AT the ceiling.
        TestEqual(TEXT("Three tree Mores compose to exactly the aggregator ceiling"),
            Combat->GetAttributeSideMoreProduct(), Ceiling, 0.0001f);

        Combat->PushOutgoingModifier(UBreakerAbility_Overdrive::OutgoingModifierKey(), 0.0f,
            UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.0f);

        const float ChainProduct = Combat->GetComposedMoreMultiplier();
        const float Total = Combat->GetAttributeSideMoreProduct() * ChainProduct;
        TestTrue(TEXT("O34: total effective More never exceeds the one ceiling"),
            Total <= Ceiling + UE_KINDA_SMALL_NUMBER);
        TestEqual(TEXT("At the tree ceiling the window buys nothing (the ruling's intent)"),
            ChainProduct, 1.0f, 0.001f);

        // And through the request path the weapon actually uses: the composed
        // DamageMultiplier attribute (whose More factor is already clamped)
        // times the chain must still respect the total.
        FBreakerDamageRequest Request;
        Request.BaseDamage = 100.0f;
        Request.SourceDamageMultiplier = Attributes->GetDamageMultiplier();
        Combat->ApplyOutgoingModifiers(Request);
        TestEqual(TEXT("A request leaving the chain carries at most the ceiling as its More total"),
            Request.SourceDamageMultiplier, Ceiling, 0.001f);
    }
    return true;
}

// (b) A single pushed modifier is clamped at the SAME per-source ceiling the
// aggregator's budget derives from — cited, never restated.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCeilingPerModifierClampTest,
    "RiorsEdge.Combat.Ceiling.PerModifierClamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCeilingPerModifierClampTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCombatCeilingTest;
    UBreakerCombatComponent* Combat = CeilingTestMakeCombat(nullptr);

    // A push exactly AT the ceiling is legal and silent.
    Combat->PushOutgoingModifier(TEXT("Test.AtCeiling"), 0.0f, FBreakerAttributeAggregator::SingleMoreCeiling, 0.0f);
    TestEqual(TEXT("A More at the per-source ceiling passes through untouched"),
        Combat->GetComposedMoreMultiplier(), FBreakerAttributeAggregator::SingleMoreCeiling, 0.0001f);
    Combat->RemoveOutgoingModifier(TEXT("Test.AtCeiling"));

    // A push above it is clamped to it, loudly.
    AddExpectedError(TEXT("exceeds the"), EAutomationExpectedErrorFlags::Contains, 0);
    Combat->PushOutgoingModifier(TEXT("Test.Overreach"), 0.0f, 1.60f, 0.0f);
    TestEqual(TEXT("A single modifier may never exceed the shared single-More ceiling"),
        Combat->GetComposedMoreMultiplier(), FBreakerAttributeAggregator::SingleMoreCeiling, 0.0001f);
    return true;
}

// (c) Nothing restates the deleted 2.20 ceiling: the chain's effective ceiling
// IS the aggregator's (constant identity), and no source file under
// RiorsEdge carries the old literal token (grep-style scan).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCeilingNoRestatedLiteralTest,
    "RiorsEdge.Combat.Ceiling.NoRestatedLiteral",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCeilingNoRestatedLiteralTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCombatCeilingTest;

    // Identity: the ceiling is DERIVED (1.30^3 == 2.197...), not the rounded
    // 2.20 the deleted constant held. If someone reintroduces a separate
    // rounded constant into the chain, the derivation check catches the drift.
    const float Ceiling = FBreakerAttributeAggregator::ComposedMoreCeiling();
    TestEqual(TEXT("The ceiling is derived from the two numbers that define it"),
        Ceiling,
        FMath::Pow(FBreakerAttributeAggregator::SingleMoreCeiling,
            static_cast<float>(FBreakerAttributeAggregator::MaxComposedMoreSources)),
        UE_KINDA_SMALL_NUMBER);
    TestTrue(TEXT("The derived ceiling is not the old rounded 2.20"),
        FMath::Abs(Ceiling - 2.2f) > 0.001f);

    // Behavioural identity: an over-full chain with no attribute side clamps
    // to EXACTLY the aggregator ceiling — same function, same value.
    AddExpectedError(TEXT("exceeds the"), EAutomationExpectedErrorFlags::Contains, 0);
    UBreakerCombatComponent* Combat = CeilingTestMakeCombat(nullptr);
    for (const TCHAR* Key : { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D") })
    {
        Combat->PushOutgoingModifier(Key, 0.0f, FBreakerAttributeAggregator::SingleMoreCeiling, 0.0f);
    }
    TestEqual(TEXT("The chain's ceiling IS the aggregator's ceiling"),
        Combat->GetComposedMoreMultiplier(), Ceiling, 0.0001f);

    // Grep-style scan: the old constant was written `2.20f`; assert the token
    // is gone from every RiorsEdge source file. The needle is assembled so
    // this file cannot match itself, and this file is skipped by name anyway
    // for the day someone quotes the pattern in a comment here.
    const FString SourceRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("RiorsEdge"));
    if (!IFileManager::Get().DirectoryExists(*SourceRoot))
    {
        AddInfo(TEXT("Source tree not present (packaged build?); literal scan skipped."));
        return true;
    }
    const FString Needle = FString::Printf(TEXT("2.2%sf"), TEXT("0"));
    TArray<FString> Files;
    // bClearFileNames explicitly false on the second call, or it would erase
    // the headers gathered by the first.
    IFileManager::Get().FindFilesRecursive(Files, *SourceRoot, TEXT("*.h"), /*Files*/true, /*Directories*/false, /*bClearFileNames*/true);
    IFileManager::Get().FindFilesRecursive(Files, *SourceRoot, TEXT("*.cpp"), /*Files*/true, /*Directories*/false, /*bClearFileNames*/false);
    int32 Scanned = 0;
    for (const FString& File : Files)
    {
        if (File.Contains(TEXT("BreakerCombatCeilingTests"))) continue;
        FString Contents;
        if (!FFileHelper::LoadFileToString(Contents, *File)) continue;
        ++Scanned;
        if (Contents.Contains(Needle, ESearchCase::CaseSensitive))
        {
            AddError(FString::Printf(TEXT("O34 violation: '%s' restates the deleted %s More-ceiling literal. Derive from FBreakerAttributeAggregator instead."),
                *File, *Needle));
        }
    }
    TestTrue(TEXT("The scan actually read the source tree"), Scanned > 100);
    return true;
}

// O34's weak-point clause: the aim-skill lane is outside the O3 budget and is
// bounded [1.0, 2.0] per archetype instead.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCeilingWeakPointBoundsTest,
    "RiorsEdge.Combat.Ceiling.WeakPointBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCeilingWeakPointBoundsTest::RunTest(const FString& Parameters)
{
    // Every archetype's authored multiplier sits inside the ruled bounds.
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>(GetTransientPackage());
    for (int32 Index = 0; Index < static_cast<int32>(EBreakerWeaponArchetype::Count); ++Index)
    {
        const EBreakerWeaponArchetype Archetype = static_cast<EBreakerWeaponArchetype>(Index);
        Weapon->EquipArchetype(Archetype);
        const UBreakerWeaponDefinition* Definition = Weapon->GetActiveDefinition();
        if (!Definition)
        {
            AddError(FString::Printf(TEXT("Archetype %s resolves no definition"), *BreakerWeaponArchetypeNames::Display(Archetype)));
            continue;
        }
        TestTrue(*FString::Printf(TEXT("%s weak point %.2fx sits inside the O34 bounds"),
                *BreakerWeaponArchetypeNames::Display(Archetype), Definition->WeakPointMultiplier),
            Definition->WeakPointMultiplier >= UBreakerDamageLibrary::WeakPointMultiplierFloor
            && Definition->WeakPointMultiplier <= UBreakerDamageLibrary::WeakPointMultiplierCeiling);
    }

    // And the resolve site guards the same pair, so an out-of-bounds request
    // (a future author, a mod, a bad merge) cannot ship past them.
    FBreakerDamageRequest Request;
    Request.BaseDamage = 100.0f;
    Request.bCanCritical = false;
    Request.bWeakPointHit = true;
    Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
    FBreakerDefenseState Defense;
    Defense.Health = 10000.0f;

    Request.WeakPointMultiplier = 5.0f;
    TestEqual(TEXT("An over-bounds weak point clamps to the ceiling at the multiply site"),
        UBreakerDamageLibrary::ResolveDamage(Request, Defense).RawDamage,
        100.0f * UBreakerDamageLibrary::WeakPointMultiplierCeiling, 0.001f);

    Request.WeakPointMultiplier = 0.5f;
    TestEqual(TEXT("A sub-floor weak point can never REDUCE a hit"),
        UBreakerDamageLibrary::ResolveDamage(Request, Defense).RawDamage,
        100.0f * UBreakerDamageLibrary::WeakPointMultiplierFloor, 0.001f);
    return true;
}

// DoT snapshot completeness: the outgoing chain's product is part of the
// application-time snapshot. A bleed applied inside a window ticks stronger
// for its whole life; one applied outside doesn't; a window opened after
// application changes nothing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCeilingDotWindowSnapshotTest,
    "RiorsEdge.Combat.Ceiling.DotWindowSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCeilingDotWindowSnapshotTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCombatCeilingTest;
    UBreakerAttributeSet* Attributes = CeilingTestMakeAttributes();
    UBreakerCombatComponent* Combat = CeilingTestMakeCombat(Attributes);

    FBreakerDefenseState Defense;
    Defense.Health = 100000.0f;

    auto TickDamageFor = [&Defense](float SnapshotSourcePower)
    {
        FBreakerStatusApplicationSpec Spec;
        Spec.BaseDamagePerTick = 10.0f;
        Spec.Snapshot.SourcePower = SnapshotSourcePower;
        FBreakerDamageRequest Tick = UBreakerDamageLibrary::MakeSnapshotDotTick(Spec, EBreakerDamageFamily::TrueDamage, 1, nullptr, FVector::ZeroVector, false);
        return UBreakerDamageLibrary::ResolveDamage(Tick, Defense).RawDamage;
    };

    // Applied OUTSIDE any window: the snapshot is the plain attribute side.
    const float PowerOutside = UBreakerCombatComponent::ComposeDotSourcePower(Attributes, Combat);
    TestEqual(TEXT("Outside a window the snapshot is the attribute side alone"), PowerOutside, 1.0f, 0.0001f);

    // Applied INSIDE a window: the snapshot carries the window's More.
    Combat->PushOutgoingModifier(UBreakerAbility_Overdrive::OutgoingModifierKey(), 0.0f,
        UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.0f);
    const float PowerInside = UBreakerCombatComponent::ComposeDotSourcePower(Attributes, Combat);
    TestEqual(TEXT("Inside a window the snapshot carries the window's More"),
        PowerInside, UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.0001f);

    TestTrue(TEXT("A bleed applied inside the window ticks stronger"),
        TickDamageFor(PowerInside) > TickDamageFor(PowerOutside));
    TestEqual(TEXT("The inside tick is stronger by exactly the window's More"),
        TickDamageFor(PowerInside), TickDamageFor(PowerOutside) * UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.001f);

    // The window closing does NOT weaken an application snapshotted inside it
    // (snapshot semantics: the spec's captured value is the whole truth) ...
    Combat->RemoveOutgoingModifier(UBreakerAbility_Overdrive::OutgoingModifierKey());
    TestEqual(TEXT("A DoT applied inside the window keeps its strength after the window closes"),
        TickDamageFor(PowerInside), 10.0f * UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.001f);

    // ... and a window opened AFTER application changes nothing about a spec
    // snapshotted before it: the captured value never re-reads the chain.
    Combat->PushOutgoingModifier(UBreakerAbility_Overdrive::OutgoingModifierKey(), 0.0f,
        UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.0f);
    TestEqual(TEXT("A window opened after application changes nothing already applied"),
        TickDamageFor(PowerOutside), 10.0f, 0.001f);

    // The fold respects the one O34 budget: at the tree ceiling the window
    // contributes nothing to a fresh snapshot either.
    AddExpectedError(TEXT("exceeds the"), EAutomationExpectedErrorFlags::Contains, 0);
    Attributes->ApplyAttributeContribution(EBreakerAttributeContributor::Progression, CeilingTestTreeMores(3));
    const float PowerAtCeiling = UBreakerCombatComponent::ComposeDotSourcePower(Attributes, Combat);
    TestEqual(TEXT("A snapshot at the tree ceiling gains nothing from the window (one budget)"),
        PowerAtCeiling, Attributes->GetDamageMultiplier(), 0.001f);
    return true;
}

// Fracture parity, worldless half 1: the CONTRACT. A request composed by the
// chain at cast and handed to ABreakerProjectileBase reaches the impact
// verbatim — window product, carried-status snapshot and all. (The fill site,
// Fracture's activation, needs a world and GAS; the conformance scan below
// pins that its file actually calls the chain.)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCeilingProjectileCarriesChainTest,
    "RiorsEdge.Combat.Ceiling.ProjectileCarriesTheChain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCeilingProjectileCarriesChainTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCombatCeilingTest;
    UBreakerAttributeSet* Attributes = CeilingTestMakeAttributes();
    UBreakerCombatComponent* Combat = CeilingTestMakeCombat(Attributes);
    Combat->PushOutgoingModifier(UBreakerAbility_Overdrive::OutgoingModifierKey(), 0.0f,
        UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.0f);

    // Exactly Fracture's cast-time sequence: attribute multiplier in, chain
    // composed on top, THEN the projectile is armed.
    FBreakerDamageRequest Damage;
    Damage.BaseDamage = 30.0f;
    Damage.SourceDamageMultiplier = Attributes->GetDamageMultiplier();
    Combat->ApplyOutgoingModifiers(Damage);
    TestEqual(TEXT("The cast-time composition carries the window product"),
        Damage.SourceDamageMultiplier, UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.0001f);

    FBreakerCarriedStatus Carried;
    Carried.Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false);
    Carried.Spec.BaseDamagePerTick = 5.0f;
    Carried.Spec.Duration = 4.0f;
    Carried.Spec.Snapshot.SourcePower = UBreakerCombatComponent::ComposeDotSourcePower(Attributes, Combat);

    ABreakerProjectileBase* Projectile = NewObject<ABreakerProjectileBase>(GetTransientPackage());
    if (!Projectile)
    {
        AddError(TEXT("Could not construct a projectile"));
        return false;
    }
    Projectile->AddImpactStatus(Carried);
    Projectile->InitializeProjectile(Damage, FVector::ForwardVector, 4000.0f);

    // The projectile's stored request and carried snapshot are the composed
    // values, verbatim — the "modifiers active at the moment of casting are
    // the ones that count" rule made checkable.
    TestEqual(TEXT("The projectile carries the composed request verbatim"),
        Projectile->GetProjectileDamage().SourceDamageMultiplier,
        UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.0001f);
    if (TestEqual(TEXT("The projectile carries exactly one status"), Projectile->GetImpactStatuses().Num(), 1))
    {
        TestEqual(TEXT("The carried status snapshot holds the window product"),
            Projectile->GetImpactStatuses()[0].Spec.Snapshot.SourcePower,
            UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.0001f);
    }

    // Closing the window after arming changes nothing the projectile carries.
    Combat->RemoveOutgoingModifier(UBreakerAbility_Overdrive::OutgoingModifierKey());
    TestEqual(TEXT("A window closing after the cast does not disarm the projectile"),
        Projectile->GetProjectileDamage().SourceDamageMultiplier,
        UBreakerAbility_Overdrive::OutgoingMoreMultiplier, 0.0001f);
    return true;
}

// Fracture parity, worldless half 2: the CONFORMANCE (O34's canon clause —
// every lane that submits outgoing damage must pass through the chain). Any
// ability translation unit that submits damage — directly via ReceiveDamage
// or by arming a projectile — must compose the outgoing chain first. Fracture
// shipped without this call and windows silently never applied to it; this
// scan is what makes the NEXT forgotten call a red test instead of a silent
// hole. Deliverer-routed payloads (Rot's zone spec) contain neither submit
// token and are rightly outside the rule — the zone composes at submission.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCeilingSubmissionConformanceTest,
    "RiorsEdge.Combat.Ceiling.AbilitySubmissionConformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCeilingSubmissionConformanceTest::RunTest(const FString& Parameters)
{
    const FString AbilitiesRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("RiorsEdge"), TEXT("Abilities"));
    if (!IFileManager::Get().DirectoryExists(*AbilitiesRoot))
    {
        AddInfo(TEXT("Source tree not present (packaged build?); conformance scan skipped."));
        return true;
    }
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *AbilitiesRoot, TEXT("*.cpp"), true, false, true);
    int32 Submitters = 0;
    for (const FString& File : Files)
    {
        FString Contents;
        if (!FFileHelper::LoadFileToString(Contents, *File)) continue;
        const bool bSubmits = Contents.Contains(TEXT("ReceiveDamage(")) || Contents.Contains(TEXT("InitializeProjectile("));
        if (!bSubmits) continue;
        ++Submitters;
        if (!Contents.Contains(TEXT("ApplyOutgoingModifiers")))
        {
            AddError(FString::Printf(
                TEXT("O34 conformance: '%s' submits outgoing damage without composing the outgoing modifier chain. Route the request through ApplyOutgoingModifiers before it leaves the ability (the Fracture bug, again)."),
                *File));
        }
    }
    // Cleave, Siphon, Resonance and Fracture all submit today; if this ever
    // reads zero the scan is looking at the wrong tree, not a clean one.
    TestTrue(TEXT("The scan found the known submitting abilities"), Submitters >= 4);
    return true;
}

#endif
