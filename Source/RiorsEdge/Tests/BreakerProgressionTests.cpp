#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPermanentClassTest,
    "RiorsEdge.Progression.PermanentClassAndLoadout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPermanentClassTest::RunTest(const FString& Parameters)
{
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    UBreakerClassDefinition* Swift = NewObject<UBreakerClassDefinition>();
    Swift->ClassAssetId = TEXT("Swift");
    Swift->ClassId = EBreakerClassId::Swift;
    Swift->StarterAbilityIds = {TEXT("Blink"), TEXT("KineticBurst")};
    Swift->BaseUltimateId = TEXT("Overdrive");

    UBreakerClassDefinition* Caster = NewObject<UBreakerClassDefinition>();
    Caster->ClassAssetId = TEXT("Caster");
    Caster->ClassId = EBreakerClassId::Caster;

    TestTrue(TEXT("First class selection succeeds"), Progression->ChoosePermanentClass(Swift));
    TestFalse(TEXT("Permanent class cannot be replaced"), Progression->ChoosePermanentClass(Caster));
    TestEqual(TEXT("Permanent class remains Swift"), Progression->GetProgressionState().PermanentClass, EBreakerClassId::Swift);
    TestEqual(TEXT("First class ability is equipped"), Progression->GetProgressionState().AbilityLoadout.ClassAbilityOne, FName(TEXT("Blink")));
    TestEqual(TEXT("Second class ability is equipped"), Progression->GetProgressionState().AbilityLoadout.ClassAbilityTwo, FName(TEXT("KineticBurst")));
    TestEqual(TEXT("Ultimate occupies its dedicated slot"), Progression->GetProgressionState().AbilityLoadout.Ultimate, FName(TEXT("Overdrive")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeRespecTest,
    "RiorsEdge.Progression.ForgeGatedRespec",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeRespecTest::RunTest(const FString& Parameters)
{
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    FBreakerProgressionState Initial;
    Initial.PermanentClass = EBreakerClassId::Swift;
    Initial.UnspentClassPoints = 3;
    // A REAL doctrine node (MaxRank 2), because a hand-made id no longer
    // survives a load: the drop-and-credit sweep (Part One-U item 20) treats
    // any unresolvable id as removed content and converts it to points,
    // which is the ruled behaviour and not what this test is about.
    Initial.DoctrineNodeRanks.Add({TEXT("Swift.Kinetic.ReadTheRoom"), 2});
    Progression->LoadProgressionState(Initial);

    FText Failure;
    TestFalse(TEXT("Respec away from Forge is rejected"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, false, Failure));
    TestEqual(TEXT("Rejected respec preserves allocation"), Progression->GetNodeRank(TEXT("Swift.Kinetic.ReadTheRoom"), EBreakerPointCurrency::DoctrinePoints), 2);
    TestTrue(TEXT("Respec at Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Failure));
    TestEqual(TEXT("Forge respec clears allocation"), Progression->GetNodeRank(TEXT("Swift.Kinetic.ReadTheRoom"), EBreakerPointCurrency::DoctrinePoints), 0);
    // O111: A DOCTRINE RESPEC REFUNDS, AND IT USED TO ZERO. Zeroing was right
    // while the pool arrived whole at commitment -- the points belonged to the
    // commitment being cleared. They are earned at four benchmarks now, so
    // zeroing would delete progression the character was paid for levelling to
    // reach, with nothing left able to hand it back. The ranks come back as
    // points; the retired class wallet is still never credited.
    TestEqual(TEXT("The doctrine wallet is refunded, not zeroed"), Progression->GetProgressionState().UnspentDoctrinePoints, 2);
    TestEqual(TEXT("The retired wallet is never credited"), Progression->GetProgressionState().UnspentClassPoints, 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRespecRestoresAttributesTest,
    "RiorsEdge.Progression.RespecRestoresAttributes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRespecRestoresAttributesTest::RunTest(const FString& Parameters)
{
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(NewObject<AActor>());
    Progression->BindAttributes(Attributes);
    Progression->ApplySliceDefaultsIfFresh();

    const float BaseCritChance = Attributes->GetCriticalChance();
    const float BaseHealth = Attributes->GetMaxHealth();

    // Buying through the real purchase path, not a hand-written rank list.
    UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    FText Failure;
    TestTrue(TEXT("Gateway node purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Sightline"), Failure));
    // Atlas shape: Sightline is a +2 crit rim (O2 PLACEHOLDER).
    TestEqual(TEXT("A purchased node reaches the attribute"), Attributes->GetCriticalChance(), BaseCritChance + 0.02f, 0.0001f);

    TestTrue(TEXT("Respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure));
    TestEqual(TEXT("Respec restores the pre-purchase crit chance exactly"), Attributes->GetCriticalChance(), BaseCritChance);
    TestEqual(TEXT("Respec leaves unrelated attributes at their base"), Attributes->GetMaxHealth(), BaseHealth);
    TestEqual(TEXT("The base value is never overwritten"), Attributes->GetAttributeBase(EBreakerAggregatedAttribute::CriticalChance), BaseCritChance);

    // Buy/respec cycles must not drift.
    for (int32 Cycle = 0; Cycle < 25; ++Cycle)
    {
        Progression->PurchaseNode(Core, TEXT("Core.Precision.Sightline"), Failure);
        Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure);
    }
    TestEqual(TEXT("Twenty-five buy/respec cycles do not drift crit chance"), Attributes->GetCriticalChance(), BaseCritChance);
    TestEqual(TEXT("Twenty-five buy/respec cycles do not drift the point pool"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), UBreakerProgressionLibrary::SliceCorePointGrant);
    return true;
}

#endif
