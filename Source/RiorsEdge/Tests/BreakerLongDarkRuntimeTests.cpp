#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Components/BoxComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerLongDarkRuntimeTest,
    "RiorsEdge.Abilities.LongDarkPurchasedRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLongDarkRuntimeTest::RunTest(const FString& Parameters)
{
    // Restored final-benchmark/XP fixture proves actual purchase and delivery;
    // it is not a campaign acquisition or encounter-balance measurement.
    for (int32 Scenario = 0; Scenario < 4; ++Scenario)
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!TestNotNull(TEXT("isolated Long Dark world"), World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        const uint64 SavedFrame = GFrameCounter;
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = SavedFrame; };
        auto* Floor = World->SpawnActor<AActor>();
        auto* Surface = NewObject<UBoxComponent>(Floor);
        Floor->AddInstanceComponent(Surface); Floor->SetRootComponent(Surface);
        Surface->SetBoxExtent(FVector(5000, 5000, 20));
        Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Surface->SetCollisionResponseToAllChannels(ECR_Block);
        Surface->RegisterComponent(); Floor->SetActorLocation(FVector(0, 0, -20));
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Caster = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 200), FRotator::ZeroRotator, Spawn);
        auto* Controller = World->SpawnActor<APlayerController>();
        if (!Caster || !Controller) return false;
        Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->SetAsLocalPlayerController();
        Controller->Possess(Caster); Controller->SetViewTarget(Caster);
        Controller->SetInitialLocationAndRotation(Caster->GetActorLocation(), FRotator::ZeroRotator);
        Caster->SetActorTickEnabled(false); Caster->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC = Caster->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Caster, Caster); ASC->AddAttributeSetSubobject(Caster->GetAttributes());
        Caster->GetCombat()->BindAttributes(Caster->GetAttributes());
        auto* Progression = Caster->GetProgression();
        Progression->BindAttributes(Caster->GetAttributes());
        if (!TestTrue(TEXT("actual permanent Caster"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
            for (const auto& Beat : Mission.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
        TestEqual(TEXT("authored entitlement is eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
        FText Reason;
        const auto* Tree = UBreakerProgressionLibrary::GetCasterVoidWhispererTree();
        // O272: the keystone gates on six invested points, and every other
        // node is a one-point single, so six distinct singles reach it. Every
        // lifetime read below is relative to a value read live, so Lingering's
        // longer puddle changes none of them.
        for (const TCHAR* Node : { TEXT("Caster.VoidWhisperer.Patience"), TEXT("Caster.VoidWhisperer.Seep"),
            TEXT("Caster.VoidWhisperer.StandingWater"), TEXT("Caster.VoidWhisperer.Lingering"),
            TEXT("Caster.VoidWhisperer.Attrition"), TEXT("Caster.VoidWhisperer.Drain") })
            if (!TestTrue(Node, Progression->PurchaseNode(Tree, Node, Reason))) return false;
        if (!TestTrue(TEXT("actual Void Whisperer commitment"), Progression->CommitToBranch(Tree->TreeId, Reason))) return false;
        if (!TestTrue(TEXT("actual Long Dark purchase"), Progression->PurchaseNode(Tree, TEXT("Caster.VoidWhisperer.LongDark"), Reason))) return false;
        TestEqual(TEXT("six singles and the one-point keystone leave one of eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 1);
        if (!Progression->IsAbilityUnlocked(TEXT("Caster.Rot")))
            if (!TestTrue(TEXT("level-earned token unlocks Rot"), Progression->SpendAbilityToken(TEXT("Caster.Rot"), Reason))) return false;
        if (!TestTrue(TEXT("class ultimate is actually unlocked"), Progression->IsAbilityUnlocked(TEXT("Caster.Unmake")))) return false;
        if (!TestTrue(TEXT("purchased keystone reaches GAS"), ASC->HasMatchingGameplayTag(BreakerAbilityTags::Keystone_Caster_LongDark.GetTag()))) return false;
        auto* Mana = Caster->GetMana(); Mana->BindAttributes(Caster->GetAttributes());
        Mana->SetComponentTickEnabled(false); Mana->AdvanceLoop(30);
        auto* State = UBreakerAbilityStateComponent::FindOrAdd(Caster); State->SetComponentTickEnabled(false);
        const auto Rot = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Rot::StaticClass(), 1));
        const auto Unmake = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Unmake::StaticClass(), 1));
        auto Advance = [&](int32 Steps)
        {
            for (int32 Step = 0; Step < Steps; ++Step)
            {
                ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
                if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
                State->AdvanceTime(.05f);
                const auto Zones = ABreakerZoneActor::GetLiveZones();
                for (const auto& Held : Zones)
                    if (auto* Zone = Held.Get(); Zone && Zone->GetWorld() == World && !Zone->IsReleased()) Zone->AdvanceZone(.05f);
            }
        };
        auto CastRot = [&](const FVector& Aim) -> ABreakerZoneActor*
        {
            // Aim from the live camera, as the paid encounter fixture does.
            // Initial spawn location/rotation is not a per-cast camera reset.
            for (int32 Step = 0; Step < 3; ++Step)
            {
                FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye, Facing);
                Controller->SetControlRotation((Aim - Eye).Rotation());
                if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(.05f);
            }
            FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye, Facing);
            FHitResult FloorHit;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(BreakerLongDarkFixtureAim), false, Caster);
            const bool bHitFloor = World->LineTraceSingleByChannel(FloorHit, Eye, Eye + Facing.Vector() * GetDefault<UBreakerAbility_Rot>()->MaximumRangeCm,
                ECC_GameTraceChannel2, Query) && FloorHit.GetActor() == Floor;
            if (!TestTrue(*FString::Printf(TEXT("live paid-cast aim hits floor: eye=%s facing=%s actor=%s component=%s impact=%s desired=%s"),
                *Eye.ToString(), *Facing.ToString(), *GetNameSafe(FloorHit.GetActor()), *GetNameSafe(FloorHit.GetComponent()),
                *FloorHit.ImpactPoint.ToString(), *Aim.ToString()), bHitFloor)) return nullptr;
            if (!TestTrue(TEXT("live floor aim reaches intended refresh/placement point"), FVector::Dist(FloorHit.ImpactPoint, Aim) < 1)) return nullptr;
            const float Before = Mana->GetMana();
            if (!TestTrue(TEXT("real Rot activation"), ASC->TryActivateAbility(Rot))) return nullptr;
            BreakerResolvePendingCast(World, Caster);
            TestTrue(TEXT("Rot pays its actual current-window cost"), Mana->GetMana() < Before);
            ABreakerZoneActor* Nearest = nullptr;
            for (const auto& Held : ABreakerZoneActor::GetLiveZones())
                if (auto* Zone = Held.Get(); Zone && Zone->GetZoneInstigator() == Caster && !Zone->IsReleased()
                    && (!Nearest || FVector::DistSquared(Zone->GetActorLocation(), Aim) < FVector::DistSquared(Nearest->GetActorLocation(), Aim))) Nearest = Zone;
            if (Nearest)
            {
                Nearest->SetActorTickEnabled(false);
                TestTrue(TEXT("paid zone actually lands on the traced floor"), FMath::Abs(Nearest->GetActorLocation().Z) < 1);
            }
            return Nearest;
        };
        const FVector ControlAim(800, 0, 0), NewAim(800, 1600, 0);
        // UE GetPlayerViewPoint ignores the evaluated camera while its cache
        // timestamp is zero and returns the view target actor transform instead.
        // Advance the real world once before the first aimed cast, matching the
        // encounter fixture's pre-fight world ticks without running BeginPlay.
        Advance(1);
        if (!TestNotNull(TEXT("native player camera manager"), Controller->PlayerCameraManager.Get())) return false;
        Controller->PlayerCameraManager->UpdateCamera(.05f);
        if (!TestTrue(TEXT("aim reads an evaluated nonzero-time camera cache"), Controller->PlayerCameraManager->GetCameraCacheTime() > 0)) return false;
        TStrongObjectPtr<ABreakerZoneActor> Control(CastRot(ControlAim));
        if (!TestNotNull(TEXT("ordinary pre-window zone"), Control.Get())) return false;
        TestFalse(TEXT("before-window placement is not paused"), Control->IsExpiryPaused());
        const float ControlBirth = Control->GetRemainingDuration(); Advance(10);
        TestEqual(TEXT("ordinary zone ages normally"), Control->GetRemainingDuration(), ControlBirth - .5f, .001f);
        Mana->AdvanceLoop(30); // Ordinary pre-ultimate recovery; no combat-period refills.
        const float BeforeUnmake = Mana->GetMana();
        if (!TestTrue(TEXT("paid Long Dark Unmake"), ASC->TryActivateAbility(Unmake))) return false;
        BreakerResolvePendingCast(World, Caster);
        TestTrue(TEXT("ultimate pays Mana"), Mana->GetMana() < BeforeUnmake);
        TestEqual(TEXT("real Long Dark window is twelve seconds"), State->GetWindowRemaining(UBreakerCasterAbility::UnmakeWindowKey()), 12.0f);
        TestEqual(TEXT("real Long Dark cost scalar is half"), State->GetWindowPayload(UBreakerCasterAbility::UnmakeWindowKey()), .5f);
        if (!TestEqual(TEXT("paid overlap refresh reuses ordinary zone"), CastRot(ControlAim), Control.Get())) return false;
        TestFalse(TEXT("refresh cannot retroactively pause older placement"), Control->IsExpiryPaused());
        TStrongObjectPtr<ABreakerZoneActor> Paused(CastRot(NewAim));
        if (!TestNotNull(TEXT("new paid Long Dark zone"), Paused.Get())) return false;
        if (!TestTrue(TEXT("distinct new placement pauses expiry"), Paused.Get() != Control.Get() && Paused->IsExpiryPaused())) return false;
        const float FrozenLifetime = Paused->GetRemainingDuration();
        auto* Enemy = World->SpawnActor<ABreakerEnemy>(Paused->GetActorLocation() + FVector(0, 0, 90), FRotator::ZeroRotator, Spawn);
        if (!Enemy) return false;
        Enemy->SetActorTickEnabled(false);
        auto* Health = Cast<UBreakerAttributeSet>(Enemy->GetDefaultSubobjectByName(TEXT("Attributes")));
        if (!Health) return false;
        Enemy->GetAbilitySystemComponent()->InitAbilityActorInfo(Enemy, Enemy);
        Enemy->GetAbilitySystemComponent()->AddAttributeSetSubobject(Health);
        Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); // Durable observation target; player damage unchanged.
        Enemy->FindComponentByClass<UBreakerCombatComponent>()->BindAttributes(Health);
        Enemy->FindComponentByClass<UBreakerStatusComponent>()->SetComponentTickEnabled(false);
        const float HealthBefore = Health->GetHealth();
        Advance(20);
        TestEqual(TEXT("paused zone keeps its whole remaining lifetime"), Paused->GetRemainingDuration(), FrozenLifetime, .001f);
        TestEqual(TEXT("paused damage cadence still delivers two ticks"), Paused->GetTicksDelivered(), 2);
        TestTrue(TEXT("actual living enemy takes zone damage"), Health->GetHealth() < HealthBefore);
        TestTrue(TEXT("refreshed older zone continues aging"), Control->GetRemainingDuration() < ControlBirth);
        Enemy->SetActorLocation(Paused->GetActorLocation() + FVector(0, 1200, 90));
        const float OutsideHealth = Health->GetHealth(); Advance(10);
        TestEqual(TEXT("paused footprint still processes exit"), Paused->GetOccupantCount(), 0);
        TestEqual(TEXT("outside target receives no zone damage"), Health->GetHealth(), OutsideHealth);
        Enemy->SetActorLocation(Paused->GetActorLocation() + FVector(0, 0, 90)); Advance(10);
        TestTrue(TEXT("paused footprint still processes reentry and damage"), Paused->GetOccupantCount() > 0 && Health->GetHealth() < OutsideHealth);

        if (Scenario == 0) Advance(FMath::CeilToInt((State->GetWindowRemaining(UBreakerCasterAbility::UnmakeWindowKey()) + .1f) / .05f));
        if (Scenario == 1) ASC->CancelAbilityHandle(Unmake);
        if (Scenario == 2)
        {
            FBreakerDamageRequest Lethal; Lethal.BaseDamage = 100000; Lethal.bCanCritical = false;
            Lethal.DamageFamily = EBreakerDamageFamily::TrueDamage;
            Caster->GetCombat()->ReceiveDamage(Lethal);
            TestTrue(TEXT("owner actually dies"), Caster->GetCombat()->IsDead());
        }
        if (Scenario == 3)
        {
            TestTrue(TEXT("actual Doctrine respec"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Reason));
            TestFalse(TEXT("actual Long Dark tag removed"), ASC->HasMatchingGameplayTag(BreakerAbilityTags::Keystone_Caster_LongDark.GetTag()));
        }
        TestFalse(TEXT("original closure, cancel, death or respec releases lifetime pause"), Paused->IsExpiryPaused());
        if (Scenario == 1)
        {
            Mana->AdvanceLoop(30); // Natural recovery after cancellation, before a separate ultimate.
            if (!TestTrue(TEXT("later actual paid Unmake"), ASC->TryActivateAbility(Unmake))) return false;
            BreakerResolvePendingCast(World, Caster);
            TestFalse(TEXT("new ultimate never rearms old placement"), Paused->IsExpiryPaused());
        }
        const float ReleasedLifetime = Paused->GetRemainingDuration(); Advance(10);
        TestEqual(TEXT("released lifetime resumes aging"), Paused->GetRemainingDuration(), ReleasedLifetime - .5f, .001f);
        Advance(FMath::CeilToInt((Paused->GetRemainingDuration() + .1f) / .05f));
        TestTrue(TEXT("resumed zone reaches actual release/destruction"), Paused->IsReleased() && Paused->IsActorBeingDestroyed());
        TestEqual(TEXT("expiry releases all occupants"), Paused->GetOccupantCount(), 0);
        ASC->CancelAbilityHandle(Unmake);
    }
    return true;
}
#endif
