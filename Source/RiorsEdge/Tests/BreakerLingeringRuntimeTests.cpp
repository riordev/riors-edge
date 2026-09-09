#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerZoneActor.h"
#include "Components/BoxComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerLingeringRuntimeTest,
    "RiorsEdge.Abilities.LingeringPaidRadius", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerLingeringRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 SavedFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = SavedFrame; };
    auto* Floor = World->SpawnActor<AActor>();
    auto* Shape = NewObject<UBoxComponent>(Floor); Floor->AddInstanceComponent(Shape); Floor->SetRootComponent(Shape);
    Shape->SetBoxExtent(FVector(5000, 5000, 20)); Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Shape->SetCollisionResponseToAllChannels(ECR_Block); Shape->RegisterComponent(); Floor->SetActorLocation(FVector(0, 0, -20));
    auto* Player = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 200), FRotator::ZeroRotator);
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!Player || !Controller) return false;
    Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->SetAsLocalPlayerController();
    Controller->Possess(Player); Controller->SetViewTarget(Player);
    Controller->SetInitialLocationAndRotation(Player->GetActorLocation(), FRotator::ZeroRotator);
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attributes = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attributes);
    Player->GetCombat()->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attributes);
    if (!TestTrue(TEXT("actual Caster"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
    // Explicit restored benchmark fixture; spend only the four-point reachable path.
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
    const int32 Wallet = Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints);
    const auto* Tree = UBreakerProgressionLibrary::GetCasterVoidWhispererTree(); FText Reason;
    for (const TCHAR* Node : {TEXT("Caster.VoidWhisperer.StandingWater"), TEXT("Caster.VoidWhisperer.StandingWater"), TEXT("Caster.VoidWhisperer.Lingering")})
        if (!TestTrue(Node, Progression->PurchaseNode(Tree, Node, Reason))) return false;
    if (!Progression->IsAbilityUnlocked(TEXT("Caster.Rot")))
        if (!TestTrue(TEXT("earned token unlocks Rot"), Progression->SpendAbilityToken(TEXT("Caster.Rot"), Reason))) return false;
    auto* Mana = Player->GetMana(); Mana->BindAttributes(Attributes); Mana->SetComponentTickEnabled(false); Mana->AdvanceLoop(30);
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Rot::StaticClass(), 1));
    ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); // Camera cache must have positive world time.
    if (!Controller->PlayerCameraManager) return false;
    auto Cast = [&](FVector Aim) -> ABreakerZoneActor*
    {
        for (int32 Step = 0; Step < 3; ++Step)
        {
            FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye, Facing);
            Controller->SetControlRotation((Aim - Eye).Rotation()); Controller->PlayerCameraManager->UpdateCamera(.05f);
        }
        FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye, Facing);
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(BreakerLingeringAim), false, Player);
        if (!TestTrue(TEXT("real paid aim reaches floor"), World->LineTraceSingleByChannel(Hit, Eye, Eye + Facing.Vector() * 2500,
            ECC_GameTraceChannel2, Query) && Hit.GetActor() == Floor && FVector::Dist(Hit.ImpactPoint, Aim) < 1)) return nullptr;
        const float Before = Mana->GetMana();
        if (!TestTrue(TEXT("actual paid Rot"), ASC->TryActivateAbility(Handle))) return nullptr;
        BreakerResolvePendingCast(World, Player);
        TestTrue(TEXT("cast pays ordinary Mana"), Mana->GetMana() < Before);
        ABreakerZoneActor* Found = nullptr;
        for (const auto& Weak : ABreakerZoneActor::GetLiveZones())
            if (auto* Zone = Weak.Get(); Zone && Zone->GetZoneInstigator() == Player && !Zone->IsReleased()
                && FVector::Dist(Zone->GetActorLocation(), Aim) < 1) Found = Zone;
        if (Found) Found->SetActorTickEnabled(false);
        return Found;
    };
    const FVector Aim(800, 0, 0);
    auto* Zone = Cast(Aim);
    if (!Zone) return false;
    const float InitialRadius = Zone->GetSpec().RadiusCm;
    TestEqual(TEXT("rank one overlap reuses zone"), Cast(Aim), Zone);
    TestEqual(TEXT("rank one cannot grow radius"), Zone->GetSpec().RadiusCm, InitialRadius);
    if (!TestTrue(TEXT("actual rank two purchase"), Progression->PurchaseNode(Tree, TEXT("Caster.VoidWhisperer.Lingering"), Reason))) return false;
    TestEqual(TEXT("path spends exactly four Doctrine"), Wallet - Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 4);
    Mana->AdvanceLoop(30); // Ordinary recovery between the rank-one and rank-two observations.
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(800, InitialRadius + 50, 100), FRotator::ZeroRotator);
    if (!Enemy) return false;
    Enemy->SetActorTickEnabled(false);
    auto* EnemyHealth = FindObject<UBreakerAttributeSet>(Enemy, TEXT("Attributes"));
    auto* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
    if (!EnemyHealth || !EnemyCombat) return false;
    auto* EnemyASC = Enemy->GetAbilitySystemComponent(); EnemyASC->InitAbilityActorInfo(Enemy, Enemy); EnemyASC->AddAttributeSetSubobject(EnemyHealth);
    EnemyHealth->ApplyMaxHealth(1000); EnemyHealth->ApplyHealth(1000); EnemyCombat->BindAttributes(EnemyHealth);
    Zone->AdvanceZone(.5f);
    TestEqual(TEXT("enemy beyond original radius takes no damage"), EnemyHealth->GetHealth(), 1000.0f);
    TestEqual(TEXT("rank two refresh keeps original zone"), Cast(Aim), Zone);
    TestEqual(TEXT("rank two adds exactly one metre"), Zone->GetSpec().RadiusCm, InitialRadius + 100);
    TestEqual(TEXT("growth immediately admits real enemy"), Zone->GetOccupantCount(), 1);
    Zone->AdvanceZone(.5f);
    TestTrue(TEXT("expanded ring delivers real damage"), EnemyHealth->GetHealth() < 1000);
    TestEqual(TEXT("repeated paid refresh reuses grown zone"), Cast(Aim), Zone);
    TestEqual(TEXT("repeated refresh cannot grow twice"), Zone->GetSpec().RadiusCm, InitialRadius + 100);
    auto* Fresh = Cast(FVector(800, -1400, 0));
    if (!Fresh) return false;
    TestTrue(TEXT("separate paid placement creates new zone"), Fresh != Zone);
    TestEqual(TEXT("fresh zone starts unexpanded"), Fresh->GetSpec().RadiusCm, InitialRadius);
    return true;
}
#endif
