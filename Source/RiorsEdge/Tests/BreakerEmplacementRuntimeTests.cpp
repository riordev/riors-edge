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
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEmplacementRuntimeTest,
    "RiorsEdge.Weapons.EmplacementPaidAnchor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEmplacementRuntimeTest::RunTest(const FString& Parameters)
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
    // O272: Emplacement is the impactful half of Answering Fire's pair; the
    // travel buys first, alone.
    {
        const bool bPurchased = Progression->PurchaseNode(Tree, TEXT("Tank.Bastion.AnsweringFire"), Reason);
        if (!TestTrue(FString::Printf(TEXT("purchase Answering Fire: %s"), *Reason.ToString()), bPurchased)) return false;
    }
    if (!Progression->IsAbilityUnlocked(TEXT("Tank.AnchorPoint")))
        if (!TestTrue(TEXT("earned token unlocks Anchor"), Progression->SpendAbilityToken(TEXT("Tank.AnchorPoint"), Reason))) return false;
    auto* Grit = Tank->GetGrit(); Grit->BindAttributes(Attributes); Grit->SetComponentTickEnabled(false);
    Grit->SetInCombat(true);
    for (int32 Second = 0; Second < 70; ++Second) { Grit->SetEnemyInProximity(true); Grit->AdvanceLoop(1); }
    TestEqual(TEXT("ordinary combat proximity funds Anchor"), Attributes->GetClassResource(), 100.0f);
    auto* Weapon = Tank->GetWeapon(); Weapon->BeginPlay(); Weapon->EquipArchetype(EBreakerWeaponArchetype::Rifle);
    auto* Movement = Tank->GetBreakerMovement(); Movement->SetMovementMode(MOVE_Walking);
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_AnchorPoint::StaticClass(), 1));
    if (!TestTrue(TEXT("actual paid Anchor cast"), ASC->TryActivateAbility(Handle))) return false;
    TestEqual(TEXT("Anchor spends thirty Grit"), Attributes->GetClassResource(), 70.0f);
    ABreakerDeployable* Anchor = nullptr;
    for (const auto& Weak : ABreakerDeployable::GetLiveDeployables())
        if (auto* Candidate = Weak.Get(); Candidate && Candidate->GetOwningCharacter() == Tank
            && Candidate->GetDeployableType() == EBreakerDeployableType::AnchorPoint) Anchor = Candidate;
    if (!TestNotNull(TEXT("paid Anchor exists"), Anchor)) return false;
    const FVector Origin = Anchor->GetActorLocation();
    auto Position = [&](const FVector& Offset)
    {
        FVector Point = Origin + Offset; Point.Z = HalfHeight;
        Tank->SetActorLocation(Point);
        Movement->Velocity = FVector(200, 0, 0);
    };
    Position(-Anchor->GetActorForwardVector() * 200);
    const float OrdinaryMovingSpread = Weapon->GetNextShotSpreadDegrees();
    TestTrue(TEXT("actual moving weapon has a positive cone"), OrdinaryMovingSpread > 0);
    TestFalse(TEXT("paid own panel alone cannot grant an unowned node"), Weapon->IsSpreadReadingStationary());
    const bool bPurchased = Progression->PurchaseNode(Tree, TEXT("Tank.Bastion.Emplacement"), Reason);
    if (!TestTrue(FString::Printf(TEXT("purchase Emplacement: %s"), *Reason.ToString()), bPurchased)) return false;
    TestEqual(TEXT("legal two-point pair leaves six of eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 6);
    Movement->Velocity = FVector::ZeroVector;
    const float ActualStationarySpread = Weapon->GetNextShotSpreadDegrees();
    TestTrue(TEXT("fixture distinguishes moving from stationary spread"), OrdinaryMovingSpread > ActualStationarySpread);
    auto Check = [&](const FVector& Offset, bool bExpected, const TCHAR* Label)
    {
        Position(Offset);
        TestEqual(FString::Printf(TEXT("%s posture"), Label), Weapon->IsSpreadReadingStationary(), bExpected);
        TestEqual(FString::Printf(TEXT("%s actual moving cone"), Label), Weapon->GetNextShotSpreadDegrees(),
            bExpected ? ActualStationarySpread : OrdinaryMovingSpread, .001f);
    };
    for (float Yaw : { 0.0f, 90.0f })
    {
        Anchor->SetActorRotation(FRotator(0, Yaw, 0));
        const FVector Forward = Anchor->GetActorForwardVector(), Right = Anchor->GetActorRightVector();
        Check(-Forward * 200, true, TEXT("behind live panel"));
        Check(Forward * 200, false, TEXT("in front of live panel"));
        Check(Right * 200, false, TEXT("beside live panel"));
        Check(-Forward * 400, false, TEXT("behind but outside radius"));
    }
    Position(-Anchor->GetActorForwardVector() * 200);
    // Fan must also forfeit a doctrine's stationary-spread substitution.
    // Keep the real paid Anchor and purchased Emplacement, adding only the
    // temporary Core schema through the already-earned Core budget.
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition, Tank);
    auto* FanTree = NewObject<UBreakerProgressionTree>(Definition);
    FanTree->TreeId = TEXT("Test.Core.Fan.Emplacement"); FanTree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Fan = NewObject<UBreakerProgressionNode>(FanTree);
    Fan->NodeId = TEXT("Test.Core.Fan.Emplacement.Rule"); Fan->Currency = FanTree->Currency;
    Fan->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Fan")));
    FanTree->Nodes.Add(Fan); Definition->BranchTrees.Add(FanTree); Progression->ClassDefinition = Definition;
    if (!TestTrue(TEXT("Earned Core point purchases Fan beside real Emplacement"), Progression->PurchaseNode(FanTree, Fan->NodeId, Reason))) return false;
    const float FanMovingSpread = Weapon->GetNextShotSpreadDegrees();
    Movement->Velocity = FVector::ZeroVector;
    const float FanStationarySpread = Weapon->GetNextShotSpreadDegrees();
    TestTrue(TEXT("Fan restores movement spread despite owned live Emplacement"), FanMovingSpread > FanStationarySpread);
    Movement->Velocity = FVector(200, 0, 0);
    Anchor->Destroy();
    TestFalse(TEXT("destroyed panel immediately stops posture rewrite"), Weapon->IsSpreadReadingStationary());
    TestEqual(TEXT("destroying panel cannot change Fan's already-unreduced moving spread"), Weapon->GetNextShotSpreadDegrees(), FanMovingSpread, .001f);
    return true;
}
#endif
