#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
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
    Swift->StartingClassAbilityIds = {TEXT("Blink"), TEXT("KineticBurst")};
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
    Initial.ClassNodeRanks.Add({TEXT("KineticEntry"), 2});
    Progression->LoadProgressionState(Initial);

    FText Failure;
    TestFalse(TEXT("Respec away from Forge is rejected"), Progression->RespecAtForge(EBreakerPointCurrency::ClassPoints, false, Failure));
    TestEqual(TEXT("Rejected respec preserves allocation"), Progression->GetNodeRank(TEXT("KineticEntry"), EBreakerPointCurrency::ClassPoints), 2);
    TestTrue(TEXT("Respec at Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::ClassPoints, true, Failure));
    TestEqual(TEXT("Forge respec clears allocation"), Progression->GetNodeRank(TEXT("KineticEntry"), EBreakerPointCurrency::ClassPoints), 0);
    TestEqual(TEXT("Forge respec refunds ranks when definitions are unavailable"), Progression->GetProgressionState().UnspentClassPoints, 5);
    return true;
}

#endif
