#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerChargeComponent.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerScrapComponent.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

// ---------------------------------------------------------------------------
// THE THREE FORMERLY-UNBUILT CLASSES, BUILT (owner authorization 2026-08-16:
// "feel free to do all 5 classes").
//
// Tests/BreakerUnbuiltClassTests.cpp keeps the loop arithmetic and the class
// gate; this file owns the BUILT state: each resource generates from its wired
// event entry points on a live rig, every ability row resolves to an executing
// class, the default loadout table answers for all five classes, and the
// progression-side class definitions mirror the ability registry exactly —
// the drift between those two registries being precisely how the Caster kit
// once shipped unreachable.
// ---------------------------------------------------------------------------

namespace BreakerBuiltKitTest
{
    // Prefixed rig, per the unity-build house rule. A freshly constructed
    // actor is ROLE_Authority; NewObject components with an actor outer
    // register into OwnedComponents, so FindComponentByClass works.
    struct FBreakerBuiltKitRig
    {
        AActor* Owner = nullptr;
        UBreakerProgressionComponent* Progression = nullptr;
        UBreakerAttributeSet* Attributes = nullptr;
    };

    static FBreakerBuiltKitRig BreakerMakeBuiltKitRig(EBreakerClassId ClassId)
    {
        FBreakerBuiltKitRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Progression = NewObject<UBreakerProgressionComponent>(Rig.Owner);
        Rig.Attributes = NewObject<UBreakerAttributeSet>();
        Rig.Progression->DevForceClass(ClassId);
        return Rig;
    }
}

// ---------------------------------------------------------------------------
// Scrap generates from its wired events (kills, reloads, magazine dumps).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuiltScrapGenerationTest,
    "RiorsEdge.Classes.Built.ScrapGeneratesFromEvents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuiltScrapGenerationTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBuiltKitTest;
    FBreakerBuiltKitRig Rig = BreakerMakeBuiltKitRig(EBreakerClassId::Gunsmith);
    UBreakerScrapComponent* Scrap = NewObject<UBreakerScrapComponent>(Rig.Owner);
    Scrap->BindAttributes(Rig.Attributes);
    TestTrue(TEXT("The Scrap loop runs for a Gunsmith"), Scrap->IsActiveForOwner());
    TestEqual(TEXT("A fresh ledger is empty"), Scrap->GetScrap(), 0.0f);

    // A kill at full coefficient pays the kill grant through the metered loop.
    Scrap->NotifyKill(1.0f);
    Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("A kill pays the kill grant"), Scrap->GetScrap(), Scrap->KillGrant);

    // The reload clause: only a reload that actually loaded pays.
    const float BeforeReload = Scrap->GetScrap();
    Scrap->NotifyReloadCompleted(false);
    Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("A top-off of an unfired magazine pays nothing"), Scrap->GetScrap(), BeforeReload);
    Scrap->NotifyReloadCompleted(true);
    Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("A real reload pays the reload grant"), Scrap->GetScrap(), BeforeReload + Scrap->ReloadGrant);

    // The magazine-dump clause: only a cycle that started full re-arms it.
    const float BeforeDump = Scrap->GetScrap();
    Scrap->NotifyMagazineEmptied(false);
    Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("Emptying a partial magazine pays nothing"), Scrap->GetScrap(), BeforeDump);
    Scrap->NotifyMagazineEmptied(true);
    Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("A full dump pays the dump grant"), Scrap->GetScrap(), BeforeDump + Scrap->MagazineDumpGrant);

    // The deployable half of the economy: damage credits, destruction refunds
    // at half — never profit.
    const float BeforeDeployable = Scrap->GetScrap();
    Scrap->NotifyDeployableDamageDealt(500.0f);
    Scrap->NotifyDeployableDestroyed(40.0f);
    Scrap->AdvanceLoop(5.0f);
    TestEqual(TEXT("Deployable damage and the destruction refund both land"),
        Scrap->GetScrap(), BeforeDeployable + 1.0f + 20.0f);

    // Spending is the deployable clock: refused one short, exact spend allowed.
    TestTrue(TEXT("An affordable placement spends"), Scrap->TrySpendScrap(20.0f));
    TestFalse(TEXT("An unaffordable placement is refused outright"), Scrap->TrySpendScrap(1000.0f));
    return true;
}

// ---------------------------------------------------------------------------
// Grit generates from its wired events (combat entry, damage taken, block
// procs, melee kills) and dies with the Tank.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuiltGritGenerationTest,
    "RiorsEdge.Classes.Built.GritGeneratesFromEvents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuiltGritGenerationTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBuiltKitTest;
    FBreakerBuiltKitRig Rig = BreakerMakeBuiltKitRig(EBreakerClassId::Tank);
    UBreakerGritComponent* Grit = NewObject<UBreakerGritComponent>(Rig.Owner);
    Rig.Attributes->ApplyMaxHealth(1000.0f);
    Grit->BindAttributes(Rig.Attributes);
    TestTrue(TEXT("The Grit loop runs for a Tank"), Grit->IsActiveForOwner());

    // Entering combat pays the one-off Winded softener, immediately.
    Grit->SetInCombat(true);
    TestEqual(TEXT("Combat entry grants the entry credit"), Grit->GetGrit(), Grit->CombatEntryGrant);
    const float AfterEntry = Grit->GetGrit();

    // Refill the per-source token buckets, then take a hit: 20% of maximum
    // health at the shipped 1-per-2% rate is 10 Grit, inside the 10/s cap.
    Grit->AdvanceLoop(1.0f);
    Grit->NotifyDamageTaken(200.0f, 0.0f, /*bSelfInflicted=*/false, 1.0f);
    Grit->AdvanceLoop(1.0f);
    TestEqual(TEXT("Post-mitigation damage taken generates"), Grit->GetGrit(), AfterEntry + 10.0f);

    // The aggression source and the proc lane.
    const float BeforeKill = Grit->GetGrit();
    Grit->NotifyMeleeKill();
    Grit->AdvanceLoop(1.0f);
    TestEqual(TEXT("A melee kill pays the kill grant"), Grit->GetGrit(), BeforeKill + Grit->MeleeKillGrant);
    const float BeforeBlock = Grit->GetGrit();
    Grit->NotifyBlockProc();
    Grit->AdvanceLoop(1.0f);
    TestEqual(TEXT("A block proc pays its grant"), Grit->GetGrit(), BeforeBlock + Grit->BlockProcGrant);

    // Death zeroes the bar: no banking through a death, no free Hold.
    Grit->NotifyDeath();
    TestEqual(TEXT("Death sets Grit to zero"), Grit->GetGrit(), 0.0f);
    return true;
}

// ---------------------------------------------------------------------------
// Charge generates from its wired events (healing, shielding, marked damage,
// cleanses) and pays overheal nothing.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuiltChargeGenerationTest,
    "RiorsEdge.Classes.Built.ChargeGeneratesFromEvents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuiltChargeGenerationTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBuiltKitTest;
    FBreakerBuiltKitRig Rig = BreakerMakeBuiltKitRig(EBreakerClassId::Support);
    UBreakerChargeComponent* Charge = NewObject<UBreakerChargeComponent>(Rig.Owner);
    Charge->BindAttributes(Rig.Attributes);
    TestTrue(TEXT("The Charge loop runs for a Support"), Charge->IsActiveForOwner());
    Charge->SetInCombat(true);
    Charge->AdvanceLoop(1.0f);   // fill the self-heal sub-cap bucket

    // 30% of the target's maximum health at 1-per-3% is 10 Charge; overheal
    // travels separately and pays nothing — the loop's most load-bearing rule.
    Charge->NotifyHealingDone(300.0f, 0.0f, 1000.0f, /*bSelfTargeted=*/false, 1.0f);
    Charge->AdvanceLoop(1.0f);
    TestEqual(TEXT("Effective healing generates"), Charge->GetCharge(), 10.0f);
    Charge->NotifyHealingDone(0.0f, 500.0f, 1000.0f, false, 1.0f);
    Charge->AdvanceLoop(1.0f);
    TestEqual(TEXT("Pure overheal generates NOTHING"), Charge->GetCharge(), 10.0f);

    // The offensive conversion path and the cleanse lane.
    Charge->NotifyMarkedTargetDamage(200.0f, 1000.0f);
    Charge->AdvanceLoop(1.0f);
    TestEqual(TEXT("Marked-target damage generates"), Charge->GetCharge(), 20.0f);
    Charge->NotifyStatusCleansed(2);
    Charge->AdvanceLoop(1.0f);
    TestEqual(TEXT("A cleanse pays per status removed"), Charge->GetCharge(), 20.0f + 2.0f * Charge->CleansePerStatusGrant);

    // The buff-uptime source is a BOOL and pays only in combat.
    const float BeforeUptime = Charge->GetCharge();
    Charge->SetAnyBuffActive(true);
    Charge->AdvanceLoop(1.0f);
    TestEqual(TEXT("A live buff pays the uptime rate per second"), Charge->GetCharge(), BeforeUptime + Charge->BuffUptimeRate);
    return true;
}

// ---------------------------------------------------------------------------
// Every ability row of the three kits resolves, executes, and defaults answer.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuiltKitResolutionTest,
    "RiorsEdge.Classes.Built.KitResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuiltKitResolutionTest::RunTest(const FString& Parameters)
{
    // ClassHasImplementedKit is O39's derived gate, and the whole point of the
    // pass: all five flip true with no list edited anywhere.
    for (const EBreakerClassId ClassId : { EBreakerClassId::Swift, EBreakerClassId::Caster,
        EBreakerClassId::Gunsmith, EBreakerClassId::Tank, EBreakerClassId::Support })
    {
        TestTrue(*FString::Printf(TEXT("Class %d has an implemented kit"), static_cast<int32>(ClassId)),
            UBreakerAbilityDefinition::ClassHasImplementedKit(ClassId));
    }

    // Every registered row of the three new kits executes and derives from the
    // shared Breaker base (cost/cooldown plumbing lives there).
    for (const UBreakerAbilityDefinition* Definition : UBreakerAbilityDefinition::GetFallbackRegistry())
    {
        if (!Definition) continue;
        if (Definition->ClassId != EBreakerClassId::Gunsmith
            && Definition->ClassId != EBreakerClassId::Tank
            && Definition->ClassId != EBreakerClassId::Support) continue;
        TestTrue(*FString::Printf(TEXT("%s is implemented"), *Definition->AbilityId.ToString()), Definition->IsImplemented());
        if (Definition->IsImplemented())
        {
            TestTrue(*FString::Printf(TEXT("%s derives from the Breaker ability base"), *Definition->AbilityId.ToString()),
                Definition->AbilityClass->IsChildOf(UBreakerGameplayAbility::StaticClass()));
        }
    }

    // Every slot of every new class resolves through the same path the E/T/G
    // keys use, with an empty loadout — the whole reachability chain.
    struct FExpectedDefault { EBreakerClassId ClassId; EBreakerAbilitySlot Slot; const TCHAR* Id; };
    static const FExpectedDefault Defaults[] =
    {
        { EBreakerClassId::Gunsmith, EBreakerAbilitySlot::ClassAbilityOne, TEXT("Gunsmith.SidearmRig") },
        { EBreakerClassId::Gunsmith, EBreakerAbilitySlot::ClassAbilityTwo, TEXT("Gunsmith.Turret") },
        { EBreakerClassId::Gunsmith, EBreakerAbilitySlot::Ultimate,        TEXT("Gunsmith.FieldAssembly") },
        { EBreakerClassId::Tank,     EBreakerAbilitySlot::ClassAbilityOne, TEXT("Tank.Rend") },
        { EBreakerClassId::Tank,     EBreakerAbilitySlot::ClassAbilityTwo, TEXT("Tank.AnchorPoint") },
        { EBreakerClassId::Tank,     EBreakerAbilitySlot::Ultimate,        TEXT("Tank.Hold") },
        { EBreakerClassId::Support,  EBreakerAbilitySlot::ClassAbilityOne, TEXT("Support.Patch") },
        { EBreakerClassId::Support,  EBreakerAbilitySlot::ClassAbilityTwo, TEXT("Support.Mark") },
        { EBreakerClassId::Support,  EBreakerAbilitySlot::Ultimate,        TEXT("Support.Conduit") },
    };
    for (const FExpectedDefault& Expected : Defaults)
    {
        TestEqual(*FString::Printf(TEXT("DefaultAbilityIdForSlot answers %s"), Expected.Id),
            UBreakerAbilityDefinition::DefaultAbilityIdForSlot(Expected.ClassId, Expected.Slot), FName(Expected.Id));
        const UBreakerAbilityDefinition* Resolved = UBreakerAbilityComponent::ResolveDefinition(Expected.ClassId, Expected.Slot, NAME_None);
        TestNotNull(*FString::Printf(TEXT("An empty slot resolves to %s"), Expected.Id), Resolved);
        if (Resolved)
        {
            TestEqual(TEXT("The resolved id is the default"), Resolved->AbilityId, FName(Expected.Id));
            TestTrue(TEXT("And it executes"), Resolved->IsImplemented());
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// The progression-side class definitions mirror the ability registry EXACTLY.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuiltClassDefinitionMirrorTest,
    "RiorsEdge.Classes.Built.ClassDefinitionMirror",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuiltClassDefinitionMirrorTest::RunTest(const FString& Parameters)
{
    // Two registries answer "what does this class grant": the ability fallback
    // registry and GetFallbackClassDefinition's catalogue. The Caster bug was
    // exactly these two disagreeing, so the mirror is asserted id by id.
    for (const EBreakerClassId ClassId : { EBreakerClassId::Gunsmith, EBreakerClassId::Tank, EBreakerClassId::Support })
    {
        const UBreakerClassDefinition* Definition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
        if (!TestNotNull(*FString::Printf(TEXT("Class %d has a definition"), static_cast<int32>(ClassId)), Definition)) return false;

        TestEqual(TEXT("The definition describes its own class"), Definition->ClassId, ClassId);
        // O100: the kit is a partition now — two free starters, four bought
        // unlockables, one free ultimate. Seven ids catalogued as before, in
        // three fields instead of one.
        TArray<FName> Catalogued = Definition->StarterAbilityIds;
        Catalogued.Append(Definition->UnlockableAbilityIds);
        Catalogued.Add(Definition->BaseUltimateId);
        TestEqual(TEXT("All seven ids are catalogued"), Catalogued.Num(), 7);
        for (const FName AbilityId : Catalogued)
        {
            const UBreakerAbilityDefinition* Ability = UBreakerAbilityDefinition::FindFallback(AbilityId);
            TestNotNull(*FString::Printf(TEXT("%s exists in the ability registry"), *AbilityId.ToString()), Ability);
            if (Ability)
            {
                TestEqual(*FString::Printf(TEXT("%s belongs to the class that catalogues it"), *AbilityId.ToString()), Ability->ClassId, ClassId);
                TestTrue(*FString::Printf(TEXT("%s executes"), *AbilityId.ToString()), Ability->IsImplemented());
            }
        }
        TestTrue(TEXT("The catalogued ultimate is real"),
            UBreakerAbilityDefinition::FindFallback(Definition->BaseUltimateId) != nullptr);
        // The starters sit first, so ChoosePermanentClassById's [0]/[1]
        // seeding (D11) agrees with DefaultAbilityIdForSlot.
        TestEqual(TEXT("Starter one matches the default table"),
            Definition->StarterAbilityIds[0],
            UBreakerAbilityDefinition::DefaultAbilityIdForSlot(ClassId, EBreakerAbilitySlot::ClassAbilityOne));
        TestEqual(TEXT("Starter two matches the default table"),
            Definition->StarterAbilityIds[1],
            UBreakerAbilityDefinition::DefaultAbilityIdForSlot(ClassId, EBreakerAbilitySlot::ClassAbilityTwo));
        TestEqual(TEXT("The ultimate matches the default table"),
            Definition->BaseUltimateId,
            UBreakerAbilityDefinition::DefaultAbilityIdForSlot(ClassId, EBreakerAbilitySlot::Ultimate));
    }
    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
