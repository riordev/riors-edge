#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreEnduranceRuntimeTest, "RiorsEdge.Progression.CoreEnduranceRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreEnduranceRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    auto* Attacker = World->SpawnActor<AActor>();
    if (!Player || !Attacker) return false;
    Player->SetActorTickEnabled(false);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    World->SpawnActor<APlayerController>()->Possess(Player);
    Player->SetPlayerState(World->SpawnActor<APlayerState>());
    auto* Attr = Player->GetAttributes();
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    auto* Progression = Player->GetProgression();
    auto* Combat = Player->GetCombat();
    Progression->BindAttributes(Attr); Combat->BeginPlay(); Combat->SetComponentTickEnabled(false);

    auto* Tree = NewObject<UBreakerProgressionTree>();
    Tree->TreeId = TEXT("Test.Core.Endurance"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        auto* Node = NewObject<UBreakerProgressionNode>(Tree);
        Node->NodeId = FName(*FString::Printf(TEXT("Test.Core.Endurance.%d"), Index));
        Node->Currency = Tree->Currency; Tree->Nodes.Add(Node);
    }
    auto AddStat = [&](int32 Index, EBreakerNodeStatTarget Target, float Value)
    {
        FBreakerNodeEffect Effect; Effect.StatTarget = Target;
        Effect.StatBucket = EBreakerNodeStatBucket::Flat; Effect.ValuePerRank = Value;
        Tree->Nodes[Index]->Effects.Add(Effect);
    };
    Tree->Nodes[0]->MaxRank = 3;
    AddStat(0, EBreakerNodeStatTarget::ShieldPercentMaxHealth, 4);
    AddStat(0, EBreakerNodeStatTarget::FrontShieldPercentMaxHealth, 6);
    AddStat(1, EBreakerNodeStatTarget::FrontShieldPercentMaxHealth, 8);
    Tree->Nodes[2]->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Endurance")));
    auto* Class = NewObject<UBreakerClassDefinition>();
    Class->ClassId = EBreakerClassId::Caster; Class->BranchTrees.Add(Tree);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(6, Progression->ExperienceCurve));
    FText Reason;
    for (int32 Index : {0, 0, 0, 1})
        if (!TestTrue(TEXT("Level-earned defensive capacity purchase"), Progression->PurchaseNode(Tree, Tree->Nodes[Index]->NodeId, Reason))) return false;
    auto Advance = [&]()
    {
        for (int32 Frame = 0; Frame < 100; ++Frame) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
        Combat->TickComponent(5, LEVELTICK_All, nullptr);
    };
    Advance();
    const float PhysicalMaximum = Attr->GetMaxHealth();
    TestEqual(TEXT("Recovery fills earned ward"), Attr->GetShield(), PhysicalMaximum * .12f, .001f);
    TestEqual(TEXT("Recovery fills earned front pool"), Combat->GetFrontShield(), PhysicalMaximum * .26f, .001f);
    Attr->ApplyHealth(PhysicalMaximum * .8f);
    TestEqual(TEXT("Without Endurance owner view is physical health"), Attr->GetOwnerHealthView().Current, Attr->GetHealth());
    TestFalse(TEXT("Physical eighty percent is below the unchanged high threshold"),
        FBreakerBuildConditionState::EvaluateForActor(Player).IsActive(EBreakerBuildCondition::HealthHigh));
    if (!TestTrue(TEXT("Endurance uses the remaining earned point"), Progression->PurchaseNode(Tree, Tree->Nodes[2]->NodeId, Reason))) return false;
    const auto FullLayers = Attr->GetOwnerHealthView();
    TestEqual(TEXT("Combined view sums standing pools once"), FullLayers.Current, PhysicalMaximum * 1.18f, .001f);
    TestEqual(TEXT("Combined view sums capacities once"), FullLayers.Maximum, PhysicalMaximum * 1.38f, .001f);
    TestTrue(TEXT("Standing layers make eighty percent physical health HealthHigh"),
        FBreakerBuildConditionState::EvaluateForActor(Player).IsActive(EBreakerBuildCondition::HealthHigh));
    TestEqual(TEXT("Endurance does not rewrite physical maximum or capacity inputs"), Attr->GetMaxHealth(), PhysicalMaximum);

    Attr->ApplyHealth(PhysicalMaximum);
    FBreakerDamageRequest Hit; Hit.SetInstigator(Attacker); Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Hit.bCanCritical = false; Hit.bCanBeAvoided = false; Hit.bHasSourceLocation = true;
    Hit.SourceLocation = Player->GetActorLocation() - Player->GetActorForwardVector() * 100;
    Hit.BaseDamage = Attr->GetShield();
    Combat->ReceiveDamage(Hit);
    TestEqual(TEXT("Rear hit spends ward, not physical health"), Attr->GetShield(), 0.0f, .001f);
    TestEqual(TEXT("Rear hit retains front pool"), Combat->GetFrontShield(), PhysicalMaximum * .26f, .001f);
    Hit.SourceLocation = Player->GetActorLocation() + Player->GetActorForwardVector() * 100;
    Hit.BaseDamage = Combat->GetFrontShield();
    Combat->ReceiveDamage(Hit);
    TestEqual(TEXT("Frontal hit spends front pool"), Combat->GetFrontShield(), 0.0f, .001f);
    TestEqual(TEXT("Both actual hits leave physical health full"), Attr->GetHealth(), PhysicalMaximum, .001f);
    TestFalse(TEXT("Empty layers make full physical health insufficient for HealthHigh"),
        FBreakerBuildConditionState::EvaluateForActor(Player).IsActive(EBreakerBuildCondition::HealthHigh));
    TestEqual(TEXT("Spent pools remain in denominator"), Attr->GetOwnerHealthView().Maximum, FullLayers.Maximum, .001f);
    Combat->ApplyHealingAmount(PhysicalMaximum, Player, FGameplayTag());
    TestEqual(TEXT("Healing does not treat spent defensive layers as missing physical health"), Attr->GetHealth(), PhysicalMaximum, .001f);
    TestEqual(TEXT("Health healing cannot refill Endurance's combined view"), Attr->GetOwnerHealthView().Current, PhysicalMaximum, .001f);

    Advance();
    Attr->ApplyHealth(PhysicalMaximum * .33f);
    TestFalse(TEXT("Standing layers prevent HealthLow at thirty-three percent physical health"),
        FBreakerBuildConditionState::EvaluateForActor(Player).IsActive(EBreakerBuildCondition::HealthLow));
    FBreakerBuildConditionState EnemyView;
    EnemyView.SupplyTargetState(Player, Attacker);
    TestTrue(TEXT("Enemy-facing low-health threshold still reads physical health"), EnemyView.IsActive(EBreakerBuildCondition::TargetLowHealth));
    if (!TestTrue(TEXT("Native Core respec removes Endurance"), Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec restores physical owner maximum"), Attr->GetOwnerHealthView().Maximum, Attr->GetMaxHealth());
    TestEqual(TEXT("Respec restores physical owner current"), Attr->GetOwnerHealthView().Current, Attr->GetHealth());
    TestTrue(TEXT("Respec restores HealthLow at the same physical health"),
        FBreakerBuildConditionState::EvaluateForActor(Player).IsActive(EBreakerBuildCondition::HealthLow));
    return true;
}
#endif
