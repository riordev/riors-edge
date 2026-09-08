#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerExperience.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerTempoChannelRuntimeTest, "RiorsEdge.Abilities.TempoChannelRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerTempoChannelRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated ability world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Caster = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real caster without save-loading BeginPlay"), Caster)) return false;
    UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Caster, Caster); ASC->AddAttributeSetSubobject(Caster->GetAttributes());
    Caster->GetAttributes()->SetCriticalChance(0);
    Caster->GetCombat()->BindAttributes(Caster->GetAttributes());
    Caster->GetProgression()->BindAttributes(Caster->GetAttributes());
    if (!TestTrue(TEXT("actual permanent Caster selection"), Caster->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    Caster->GetMana()->BindAttributes(Caster->GetAttributes());
    Caster->GetBreakerMovement()->SetComponentTickEnabled(false);
    // Recover through the class's normal passive loop before the paid cast.
    Caster->GetMana()->AdvanceLoop(20.0f);
    ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0.0f);
    AActor* Target = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("real target"), Target)) return false;
    USphereComponent* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
    Body->SetSphereRadius(40); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    Body->RegisterComponent(); Target->SetActorLocation(FVector(500, 0, 0));
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    UBreakerAttributeSet* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); Combat->BindAttributes(Health);
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status); Status->RegisterComponent();
    auto Advance = [&](int32 Steps) { for (int32 I = 0; I < Steps; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); } };
    // Native ability grant isolates actual paid delivery, not campaign acquisition.
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Siphon::StaticClass(), 1));
    auto Channel = [&](bool bRespecDuring, int32& Hits, float& Cost)
    {
        Caster->GetMana()->AdvanceLoop(20);
        const float BeforeMana = Caster->GetAttributes()->GetClassResource();
        if (!TestTrue(TEXT("Native paid channel activates"), ASC->TryActivateAbility(Handle))) return false;
        Cost = BeforeMana - Caster->GetAttributes()->GetClassResource();
        Hits = 0;
        for (int32 Step = 0; Step < 106; ++Step)
        {
            const float Before = Health->GetHealth(); Advance(1);
            if (Health->GetHealth() < Before) ++Hits;
            if (Step == 20 && bRespecDuring)
            {
                FText Reason;
                if (!TestTrue(TEXT("Real mid-channel respec"), Caster->GetProgression()->RespecCore(Reason))) return false;
                TestEqual(TEXT("Live channel rate returns to baseline"), UBreakerGameplayAbility::AbilityChannelRateMultiplierFor(Caster), 1.0f);
            }
        }
        TestFalse(TEXT("Same five-second window naturally closes"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
        return true;
    };
    int32 BaselineHits = 0, FasterHits = 0; float BaselineCost = 0, FasterCost = 0;
    if (!Channel(false, BaselineHits, BaselineCost)) return false;
    // Separate schema asset; the twofold diagnostic makes cadence distinguishable.
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Caster->GetProgression()->ClassDefinition, Caster);
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition);
    Tree->TreeId = TEXT("Test.Core.ChannelRate"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = TEXT("Test.Core.ChannelRate.Rank");
    FBreakerNodeEffect Line; Line.StatTarget = EBreakerNodeStatTarget::AbilityChannelRate;
    Line.StatBucket = EBreakerNodeStatBucket::IncreasedPercent; Line.ValuePerRank = 100;
    Node->Effects.Add(Line); Tree->Nodes.Add(Node); Definition->BranchTrees.Add(Tree);
    Caster->GetProgression()->ClassDefinition = Definition;
    Caster->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2, Caster->GetProgression()->ExperienceCurve));
    FText Reason;
    if (!TestTrue(TEXT("Actual earned-point channel rate purchase"), Caster->GetProgression()->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    if (!Channel(true, FasterHits, FasterCost)) return false;
    TestEqual(TEXT("Faster channel keeps identical upfront cost"), FasterCost, BaselineCost);
    TestTrue(TEXT("Ordinary native channel produced ticks"), BaselineHits >= 9);
    TestTrue(TEXT("Snapshotted faster cadence survives mid-channel respec for the full window"), FasterHits >= BaselineHits * 2 - 1);
    return true;
}
#endif
