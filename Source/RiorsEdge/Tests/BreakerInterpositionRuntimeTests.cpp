#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDeployable.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerInterpositionRuntimeTest,
    "RiorsEdge.Abilities.InterpositionPaidAnchor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerInterpositionRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated Anchor world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Floor = World->SpawnActor<AActor>();
    auto* Surface = NewObject<UBoxComponent>(Floor);
    Floor->AddInstanceComponent(Surface); Floor->SetRootComponent(Surface);
    Surface->SetBoxExtent(FVector(3000, 3000, 20));
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Surface->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->RegisterComponent(); Floor->SetActorLocation(FVector(0, 0, -20));
    auto* Tank = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("native Tank"), Tank)) return false;
    Tank->SetActorTickEnabled(false); Tank->GetBreakerMovement()->SetComponentTickEnabled(false);
    const float HalfHeight = Tank->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Tank->SetActorLocation(FVector(0, 0, HalfHeight));
    auto* ASC = Tank->GetAbilitySystemComponent();
    auto* Attributes = Tank->GetAttributes();
    auto* Progression = Tank->GetProgression();
    ASC->InitAbilityActorInfo(Tank, Tank); ASC->AddAttributeSetSubobject(Attributes);
    Tank->GetCombat()->BindAttributes(Attributes); Progression->BindAttributes(Attributes);
    if (!TestTrue(TEXT("actual Tank class"), Progression->ChoosePermanentClassById(EBreakerClassId::Tank))) return false;
    // Restored benchmark entitlement fixture, not a claimed campaign run.
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("actual entitlement is eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
    const auto* Tree = UBreakerProgressionLibrary::GetTankBastionTree();
    FText Reason;
    // O272: Interposition is the impactful half of Bulk's pair; the travel
    // buys first and the pair costs two of the eight.
    for (const TCHAR* Node : { TEXT("Tank.Bastion.Bulk"), TEXT("Tank.Bastion.Interposition") })
    {
        const bool bPurchased = Progression->PurchaseNode(Tree, Node, Reason);
        if (!TestTrue(FString::Printf(TEXT("purchase %s: %s"), Node, *Reason.ToString()), bPurchased)) return false;
    }
    TestEqual(TEXT("Interposition pair leaves six of eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 6);
    if (!Progression->IsAbilityUnlocked(TEXT("Tank.AnchorPoint")))
        if (!TestTrue(TEXT("level-earned token unlocks Anchor Point"), Progression->SpendAbilityToken(TEXT("Tank.AnchorPoint"), Reason))) return false;
    auto* Grit = Tank->GetGrit(); Grit->BindAttributes(Attributes); Grit->BeginPlay(); Grit->SetComponentTickEnabled(false);
    Grit->SetInCombat(true);
    for (int32 Second = 0; Second < 70; ++Second) { Grit->SetEnemyInProximity(true); Grit->AdvanceLoop(1); }
    TestEqual(TEXT("ordinary combat entry and proximity fund cast"), Attributes->GetClassResource(), 100.0f);
    const float BaselineCap = Attributes->GetMaxShield();
    const float BaselineShield = Attributes->GetShield();
    const float Bonus = Attributes->GetMaxHealth() * .10f;
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_AnchorPoint::StaticClass(), 1));
    if (!TestTrue(TEXT("actual paid Anchor cast"), ASC->TryActivateAbility(Handle))) return false;
    TestEqual(TEXT("Anchor pays thirty Grit"), Attributes->GetClassResource(), 70.0f);
    ABreakerDeployable* Anchor = nullptr;
    for (const auto& Weak : ABreakerDeployable::GetLiveDeployables())
        if (auto* Candidate = Weak.Get(); Candidate && Candidate->GetOwningCharacter() == Tank && Candidate->GetDeployableType() == EBreakerDeployableType::AnchorPoint) Anchor = Candidate;
    if (!TestNotNull(TEXT("paid cast creates owned physical Anchor"), Anchor)) return false;
    const FVector Origin = Anchor->GetActorLocation();
    const FVector Forward = Anchor->GetActorForwardVector();
    auto CheckPosition = [&](float Distance, float ExpectedCap)
    {
        FVector Position = Origin + Forward * Distance; Position.Z = HalfHeight;
        Tank->SetActorLocation(Position);
        Grit->SetEnemyInProximity(true); Grit->AdvanceLoop(.05f);
        TestEqual(TEXT("live positional headroom"), Attributes->GetMaxShield(), ExpectedCap, .001f);
        TestEqual(TEXT("headroom never grants current shield"), Attributes->GetShield(), BaselineShield, .001f);
    };
    CheckPosition(-200, BaselineCap + Bonus);
    CheckPosition(200, BaselineCap);
    CheckPosition(-500, BaselineCap);
    CheckPosition(-200, BaselineCap + Bonus);
    CheckPosition(-200, BaselineCap + Bonus);
    auto* Core = NewObject<UBreakerProgressionTree>(); Core->TreeId = TEXT("Test.Anchor.CoreShield");
    Core->Currency = EBreakerPointCurrency::CorePoints;
    auto* Layer = NewObject<UBreakerProgressionNode>(Core); Layer->NodeId = TEXT("Test.Anchor.Layer");
    Layer->Currency = Core->Currency;
    FBreakerNodeEffect Capacity; Capacity.StatTarget = EBreakerNodeStatTarget::ShieldPercentMaxHealth;
    Capacity.StatBucket = EBreakerNodeStatBucket::Flat; Capacity.ValuePerRank = 40;
    Layer->Effects.Add(Capacity); Core->Nodes.Add(Layer);
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition, Progression);
    Definition->BranchTrees.Add(Core); Progression->ClassDefinition = Definition;
    if (!TestTrue(TEXT("Level-earned Core shield purchase beside paid Anchor"), Progression->PurchaseNode(Core, Layer->NodeId, Reason))) return false;
    auto* Gear = Tank->GetEquipment(); Gear->BindAttributes(Attributes);
    FBreakerItemInstance Body;
    for (int32 Seed = 1; Seed <= 100; ++Seed)
    { Body = UBreakerLootLibrary::RollItem(TEXT("Test.Anchor.Shield"), EBreakerEquipSlot::BodyArmour, EBreakerItemRarity::Standard, 1, Seed); if (Body.ArmourArchetype == EBreakerArmourArchetype::Shield) break; }
    if (!TestTrue(TEXT("Actual shield armour equips beside paid Anchor"), Body.ArmourArchetype == EBreakerArmourArchetype::Shield && Gear->EquipItem(Body))) return false;
    const float CombinedBase = Gear->GetStats().BaseShieldFromGear + Attributes->GetMaxHealth() * .4f;
    CheckPosition(-200, CombinedBase + Attributes->GetMaxHealth() * .1f);
    CheckPosition(200, CombinedBase);
    CheckPosition(-200, CombinedBase + Attributes->GetMaxHealth() * .1f);
    Anchor->Destroy();
    TestEqual(TEXT("actual destruction delegate immediately removes headroom"), Attributes->GetMaxShield(), CombinedBase, .001f);
    Grit->AdvanceLoop(.05f);
    TestEqual(TEXT("destroyed Anchor removes headroom"), Attributes->GetMaxShield(), CombinedBase, .001f);
    TestEqual(TEXT("destroyed Anchor never paid shield"), Attributes->GetShield(), BaselineShield, .001f);
    return true;
}
#endif
