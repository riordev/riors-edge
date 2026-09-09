#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerLiteralAbilityDamageTest, "RiorsEdge.Abilities.LiteralAddedDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerLiteralAbilityDamageTest::RunTest(const FString& Parameters)
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
    // Its literal line is purchased with actual level-earned points; no shipped ID is changed.
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Caster->GetProgression()->ClassDefinition, Caster);
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition);
    Tree->TreeId = TEXT("Test.Core.LiteralDamage");
    Tree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Node = NewObject<UBreakerProgressionNode>(Tree);
    Node->NodeId = TEXT("Test.Core.AddedAbilityPower");
    FBreakerNodeEffect Line;
    Line.StatTarget = EBreakerNodeStatTarget::AddedAbilityPower;
    Line.ValuePerRank = 4;
    Node->Effects.Add(Line);
    Tree->Nodes.Add(Node);
    Definition->BranchTrees.Add(Tree);
    Caster->GetProgression()->ClassDefinition = Definition;
    Caster->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2, Caster->GetProgression()->ExperienceCurve));
    FText Reason;
    if (!TestTrue(TEXT("Schema literal line actually purchased"), Caster->GetProgression()->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    TestEqual(TEXT("Literal ability aggregation"), Caster->GetProgression()->GetNodeStats().AddedAbilityPower, 4.0f);
    TestEqual(TEXT("No weapon-lane spill"), Caster->GetProgression()->GetNodeStats().AddedWeaponDamage, 0.0f);
    TestEqual(TEXT("Zero utility base stays zero"), UBreakerGameplayAbility::AbilityBaseDamageFor(Caster, 0), 0.0f);
    const float ScaledBase = GetDefault<UBreakerAbility_Siphon>()->DamagePerTick * UBreakerGameplayAbility::AbilityDamageScalarFor(Caster);
    TestEqual(TEXT("Flat bonus follows authored item-level scalar"), UBreakerGameplayAbility::AbilityBaseDamageFor(Caster, ScaledBase), ScaledBase + 4);
    FBreakerDamageRequest Expected;
    Expected.BaseDamage = ScaledBase + 4;
    UBreakerDamageLibrary::FillSourcePools(Caster->GetAttributes(), EBreakerDamageDelivery::Ability, Expected);
    Caster->GetCombat()->ApplyOutgoingModifiers(Expected);
    ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0.0f);
    const float Before = Health->GetHealth();
    const float ManaBefore = Caster->GetAttributes()->GetClassResource();
    if (!TestTrue(TEXT("Actual paid Siphon activates"), ASC->TryActivateAbility(Handle))) return false;
    BreakerResolvePendingCast(World, Caster);
    for (int32 Step = 0; Step < 20 && Health->GetHealth() == Before; ++Step) Advance(1);
    TestTrue(TEXT("Native channel pays Mana"), Caster->GetAttributes()->GetClassResource() < ManaBefore);
    TestTrue(TEXT("Native tick applies flat before Increased exactly once"), FMath::IsNearlyEqual(Before - Health->GetHealth(), Expected.BaseDamage * Expected.SourceDamageMultiplier, .01f));
    ASC->CancelAbilityHandle(Handle);
    const float AfterCancel = Health->GetHealth();
    Advance(12);
    TestEqual(TEXT("Cancellation stops native ticks"), Health->GetHealth(), AfterCancel);
    return true;
}
#endif
