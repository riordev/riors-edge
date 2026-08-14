#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"

// ---------------------------------------------------------------------------
// CLASS SWAP IDENTITY — the front end must describe the class you are IN
// ---------------------------------------------------------------------------
// Owner playtest, 2026-08-14: "when selecting a different class i can only see
// the swift nodes" and "i dont see proper ability selection based on what
// character im at". One cause for both. DevForceClass set State.PermanentClass
// and deliberately KEPT whatever UBreakerClassDefinition was already held, and
// two readers trust that object directly rather than the state:
// GetAvailableTrees unioned ClassDefinition->BranchTrees, and IsAbilityUnlocked
// answered from ClassDefinition->StartingClassAbilityIds/BaseUltimateId.
//
// The reason it survived so long is the instructive part: RecalculateStats
// already re-derived everything from State.PermanentClass, so every number in
// the game was CORRECT while the entire front end was describing the previous
// class. No test that asserts an attribute value could ever have seen it. These
// tests assert IDENTITY — which trees and which abilities a class is offered —
// which is the axis the numbers tests structurally cannot cover.
// ---------------------------------------------------------------------------

namespace
{
    struct FClassRig
    {
        AActor* Owner = nullptr;
        UBreakerProgressionComponent* Progression = nullptr;

        // No RegisterComponent: these tests run without a UWorld, and
        // registering a component on a worldless actor trips an engine ensure
        // (ActorComponent.cpp's MyOwnerWorld check) which fails whichever test
        // happens to run first. Every rig in this suite constructs the same way
        // — see BreakerManaTests.cpp's MakeRig — and nothing under test here
        // needs registration: DevForceClass, GetAvailableTrees and
        // IsAbilityUnlocked all read State and the class definition only.
        FClassRig()
        {
            Owner = NewObject<AActor>();
            Progression = NewObject<UBreakerProgressionComponent>(Owner);
        }
    };

    bool AnyTreeRequiresClass(const TArray<UBreakerProgressionTree*>& Trees, EBreakerClassId ClassId)
    {
        for (const UBreakerProgressionTree* Tree : Trees)
        {
            if (Tree && Tree->RequiredClass == ClassId) return true;
        }
        return false;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerClassSwapShowsOwnTreesTest,
    "RiorsEdge.Progression.ClassSwapShowsOwnTrees",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerClassSwapShowsOwnTreesTest::RunTest(const FString& Parameters)
{
    FClassRig Rig;
    if (!Rig.Progression)
    {
        AddError(TEXT("Progression component failed to construct"));
        return false;
    }

    // Swift first, because Swift is the class whose trees leaked.
    Rig.Progression->DevForceClass(EBreakerClassId::Swift);
    const TArray<UBreakerProgressionTree*> SwiftTrees = Rig.Progression->GetAvailableTrees();
    TestTrue(TEXT("A Swift character is offered Swift branch trees"),
        AnyTreeRequiresClass(SwiftTrees, EBreakerClassId::Swift));

    // THE REGRESSION. Before the fix this still returned Swift's three branch
    // trees, because the held ClassDefinition was still Swift's.
    Rig.Progression->DevForceClass(EBreakerClassId::Caster);
    const TArray<UBreakerProgressionTree*> CasterTrees = Rig.Progression->GetAvailableTrees();
    TestFalse(TEXT("A Caster is NOT offered Swift's branch trees after a dev swap"),
        AnyTreeRequiresClass(CasterTrees, EBreakerClassId::Swift));

    // The Core tree is class-agnostic (RequiredClass == None) and must survive
    // the swap — an empty board would be a different bug, not a fix.
    bool bHasCoreTree = false;
    for (const UBreakerProgressionTree* Tree : CasterTrees)
    {
        if (Tree && Tree->RequiredClass == EBreakerClassId::None) bHasCoreTree = true;
    }
    TestTrue(TEXT("The class-agnostic Core tree survives a class swap"), bHasCoreTree);

    // And the swap is reversible: swapping back restores Swift's trees rather
    // than leaving the character stranded on Core alone.
    Rig.Progression->DevForceClass(EBreakerClassId::Swift);
    TestTrue(TEXT("Swapping back to Swift restores Swift's branch trees"),
        AnyTreeRequiresClass(Rig.Progression->GetAvailableTrees(), EBreakerClassId::Swift));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerClassSwapOffersOwnAbilitiesTest,
    "RiorsEdge.Progression.ClassSwapOffersOwnAbilities",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerClassSwapOffersOwnAbilitiesTest::RunTest(const FString& Parameters)
{
    FClassRig Rig;
    if (!Rig.Progression)
    {
        AddError(TEXT("Progression component failed to construct"));
        return false;
    }

    // Ability ids are quoted from the shipped fallback registry rather than
    // invented, so this test fails if the registry is renamed out from under it
    // instead of passing vacuously against ids nothing implements.
    const FName SwiftUltimate = TEXT("Swift.Overdrive");
    const FName CasterUltimate = TEXT("Caster.Unmake");
    const FName CasterStarter = TEXT("Caster.Cleave");

    Rig.Progression->DevForceClass(EBreakerClassId::Swift);
    TestTrue(TEXT("A Swift character can select Swift's ultimate"),
        Rig.Progression->IsAbilityUnlocked(SwiftUltimate));
    TestFalse(TEXT("A Swift character is not offered a Caster ability"),
        Rig.Progression->IsAbilityUnlocked(CasterUltimate));

    // THE REGRESSION, ability side.
    Rig.Progression->DevForceClass(EBreakerClassId::Caster);
    TestTrue(TEXT("A Caster can select Caster's ultimate after a dev swap"),
        Rig.Progression->IsAbilityUnlocked(CasterUltimate));
    TestTrue(TEXT("A Caster can select a Caster starter ability after a dev swap"),
        Rig.Progression->IsAbilityUnlocked(CasterStarter));
    TestFalse(TEXT("A Caster is NOT still offered Swift's ultimate"),
        Rig.Progression->IsAbilityUnlocked(SwiftUltimate));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerClassSwapKitlessIsEmptyNotStaleTest,
    "RiorsEdge.Progression.ClassSwapKitlessIsEmptyNotStale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerClassSwapKitlessIsEmptyNotStaleTest::RunTest(const FString& Parameters)
{
    FClassRig Rig;
    if (!Rig.Progression) { AddError(TEXT("Progression component failed to construct")); return false; }

    // O39: Gunsmith, Tank and Support have no implemented kit, so
    // GetFallbackClassDefinition returns nullptr for them. DevForceClass is the
    // sanctioned dev path into an unbuilt class, and the REQUIREMENT is that it
    // shows nothing rather than showing the previous class's kit. An empty
    // board is honest; a board full of Swift is a lie.
    Rig.Progression->DevForceClass(EBreakerClassId::Swift);
    Rig.Progression->DevForceClass(EBreakerClassId::Gunsmith);

    TestFalse(TEXT("A kitless class is not offered Swift's branch trees"),
        AnyTreeRequiresClass(Rig.Progression->GetAvailableTrees(), EBreakerClassId::Swift));
    TestFalse(TEXT("A kitless class is not offered Swift's ultimate"),
        Rig.Progression->IsAbilityUnlocked(TEXT("Swift.Overdrive")));

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
