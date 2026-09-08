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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerElementSourceRuntimeTest, "RiorsEdge.Combat.ElementSourceRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerElementSourceRuntimeTest::RunTest(const FString& Parameters)
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
    // Separate schema fixture while the replacement Core roster is pending.
    // Its source lines are purchased with actual level-earned points; no shipped ID is changed.
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Caster->GetProgression()->ClassDefinition, Caster);
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition);
    Tree->TreeId = TEXT("Test.Core.ElementSource");
    Tree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Node = NewObject<UBreakerProgressionNode>(Tree);
    Node->NodeId = TEXT("Test.Core.ElementSourceSnapshot");
    for (const auto TargetStat : { EBreakerNodeStatTarget::ElementalBuildup, EBreakerNodeStatTarget::VoidBuildup,
        EBreakerNodeStatTarget::ElementalBuildupPenetration, EBreakerNodeStatTarget::ElementalThresholdReduction })
    {
        FBreakerNodeEffect Line;
        Line.StatTarget = TargetStat;
        Line.StatBucket = (TargetStat == EBreakerNodeStatTarget::ElementalBuildup || TargetStat == EBreakerNodeStatTarget::VoidBuildup)
            ? EBreakerNodeStatBucket::IncreasedPercent : EBreakerNodeStatBucket::Flat;
        Line.ValuePerRank = TargetStat == EBreakerNodeStatTarget::ElementalThresholdReduction ? 15 : 25;
        Node->Effects.Add(Line);
    }    Tree->Nodes.Add(Node);
    Definition->BranchTrees.Add(Tree);
    Caster->GetProgression()->ClassDefinition = Definition;
    Caster->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2, Caster->GetProgression()->ExperienceCurve));
    FText Reason;
    if (!TestTrue(TEXT("Schema source lines actually purchased"), Caster->GetProgression()->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    const float ScaledBase = GetDefault<UBreakerAbility_Siphon>()->DamagePerTick * UBreakerGameplayAbility::AbilityDamageScalarFor(Caster);
    FBreakerDamageRequest Expected;
    Expected.SetInstigator(Caster);
    Expected.BaseDamage = ScaledBase;
    UBreakerDamageLibrary::FillSourcePools(Caster->GetAttributes(), EBreakerDamageDelivery::Ability, Expected);
    Caster->GetCombat()->ApplyOutgoingModifiers(Expected);
    ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0.0f);
    Status->VoidResistancePercent = 50;
    TestEqual(TEXT("Source threshold captured"), Expected.ElementSource.ElementalThresholdMultiplier, .85f);
    const float Before = Health->GetHealth();
    const float ManaBefore = Caster->GetAttributes()->GetClassResource();
    if (!TestTrue(TEXT("Actual paid Siphon activates"), ASC->TryActivateAbility(Handle))) return false;
    for (int32 Step = 0; Step < 20 && Health->GetHealth() == Before; ++Step) Advance(1);
    TestTrue(TEXT("Native channel pays Mana"), Caster->GetAttributes()->GetClassResource() < ManaBefore);
    TestTrue(TEXT("Buildup stats do not increase actual damage"), FMath::IsNearlyEqual(Before - Health->GetHealth(), Expected.BaseDamage * Expected.SourceDamageMultiplier, .01f));
    const float Raw = Expected.BaseDamage * Expected.SourceDamageMultiplier;
    TestTrue(TEXT("Native Void tick combines generic and specific buildup then penetrates resistance"),
        FMath::IsNearlyEqual(Status->GetVoidBuildup(), Raw * 1.5f * .75f, .01f));
    ASC->CancelAbilityHandle(Handle);
    const float AfterCancel = Health->GetHealth();
    Advance(12);
    TestEqual(TEXT("Cancellation stops native ticks"), Health->GetHealth(), AfterCancel);
    // A request already emitted retains its source bonuses across an actual respec.
    Expected.Element = EBreakerElement::Void;
    Expected.ElementalFraction = 1;
    Expected.bCanCritical = false;
    if (!TestTrue(TEXT("Actual low-level Core respec"), Caster->GetProgression()->RespecCore(Reason))) return false;
    FBreakerDamageRequest Fresh;
    UBreakerDamageLibrary::FillSourcePools(Caster->GetAttributes(), EBreakerDamageDelivery::Ability, Fresh);
    TestEqual(TEXT("Fresh source has no removed buildup bonus"), Fresh.ElementSource.ElementalBuildupIncreasedPercent, 0.0f);
    TestEqual(TEXT("Emitted source retains purchased buildup"), Expected.ElementSource.ElementalBuildupIncreasedPercent, 25.0f);
    TestEqual(TEXT("Emitted source retains threshold snapshot"), Expected.ElementSource.ElementalThresholdMultiplier, .85f);

    TestTrue(TEXT("Initial channel buildup is below the threshold margin"), Status->GetVoidBuildup() < 100);
    Expected.BaseDamage = 800;
    Expected.SourceDamageMultiplier = 1;
    Expected.bHasSourceSplit = false;
    const auto Trigger = Combat->ReceiveDamage(Expected);
    TestEqual(TEXT("Snapshot test hit keeps its raw damage"), Trigger.RawDamage, 800.0f);
    bool bFoundErased = false;
    for (const auto& Active : Status->GetActiveStatuses())
    {
        if (Active.Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased")))
        {
            bFoundErased = true;
            TestEqual(TEXT("Earlier source threshold creates only the earned raw damage budget"), Active.UnpaidDamageBudget, 400.0f);
        }
    }
    TestTrue(TEXT("Captured .85 threshold triggers at 900 buildup, below the normal 1000 threshold"), bFoundErased);
    // One critical result is allocated once, leaving the physical remainder explicit.
    FBreakerDamageRequest Split;
    Split.BaseDamage = 100;
    Split.SourceDamageMultiplier = 2;
    Split.CriticalChance = 1;
    Split.CriticalMultiplier = 2;
    Split.ElementShares = { { EBreakerElement::Entropy, .25f }, { EBreakerElement::Void, .5f } };
    FBreakerDefenseState Defense;
    Defense.Health = 10000;
    const auto Result = UBreakerDamageLibrary::ResolveDamage(Split, Defense);
    TestEqual(TEXT("Single critical raw result"), Result.RawDamage, 400.0f);
    if (TestEqual(TEXT("Two normalized raw allocations"), Result.ElementRawDamage.Num(), 2))
    {
        TestEqual(TEXT("Entropy raw allocation"), Result.ElementRawDamage[0].RawDamage, 100.0f);
        TestEqual(TEXT("Void raw allocation"), Result.ElementRawDamage[1].RawDamage, 200.0f);
    }
    TestEqual(TEXT("Unconverted raw remainder"), Result.UnconvertedRawDamage, 100.0f);
    // The next native cast exercises the actual new damage scopes, rather than
    // only checking a hypothetical source projection.
    auto* DamageNode = NewObject<UBreakerProgressionNode>(Tree);
    DamageNode->NodeId = TEXT("Test.Core.ElementDamage"); DamageNode->Currency = Tree->Currency;
    for (const auto Pair : { TPair<EBreakerNodeStatTarget, float>(EBreakerNodeStatTarget::Damage, 100),
        { EBreakerNodeStatTarget::ElementalDamage, 50 } })
    {
        FBreakerNodeEffect Effect; Effect.StatTarget = Pair.Key; Effect.ValuePerRank = Pair.Value;
        Effect.StatBucket = EBreakerNodeStatBucket::IncreasedPercent; DamageNode->Effects.Add(Effect);
    }
    FBreakerNodeEffect VoidMore; VoidMore.StatTarget = EBreakerNodeStatTarget::VoidDamage;
    VoidMore.StatBucket = EBreakerNodeStatBucket::MorePercent; VoidMore.ValuePerRank = 18;
    DamageNode->Effects.Add(VoidMore); Tree->Nodes.Add(DamageNode);
    if (!TestTrue(TEXT("Earned damage schema purchase"), Caster->GetProgression()->PurchaseNode(Tree, DamageNode->NodeId, Reason))) return false;
    Status->ConsumeAllStatuses(); Caster->GetMana()->AdvanceLoop(20);
    const float DamageBefore = Health->GetHealth();
    const float CostBefore = Caster->GetAttributes()->GetClassResource();
    if (!TestTrue(TEXT("Native Siphon with purchased damage scopes activates"), ASC->TryActivateAbility(Handle))) return false;
    for (int32 Step = 0; Step < 20 && Health->GetHealth() == DamageBefore; ++Step) Advance(1);
    TestTrue(TEXT("Scoped cast pays actual Mana"), Caster->GetAttributes()->GetClassResource() < CostBefore);
    TestEqual(TEXT("Native Void delivery adds Increased once then pays selected Void More"),
        DamageBefore - Health->GetHealth(), ScaledBase * 2.5025f * 1.18f, .02f); // Includes the existing 0.25% spent-point floor.
    ASC->CancelAbilityHandle(Handle);
    return true;
}
#endif
