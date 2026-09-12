#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFallRecoveryRuntimeTest, "RiorsEdge.Movement.FallRecoveryRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFallRecoveryRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated physical fall world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    AActor* Floor = World->SpawnActor<AActor>();
    UBoxComponent* Shape = NewObject<UBoxComponent>(Floor);
    Floor->AddInstanceComponent(Shape); Floor->SetRootComponent(Shape);
    Shape->SetBoxExtent(FVector(10000, 10000, 10));
    Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Shape->SetCollisionResponseToAllChannels(ECR_Block); Shape->RegisterComponent();
    Floor->SetActorLocation(FVector(0, 0, -10));
    auto Clock = [&](float Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, Step); };
    enum class ECase { Ordinary, Safe, Shielded, Owned, Foreign, Teleported, Late, Unpurchased, Respecced };
    for (ECase Case : {ECase::Ordinary, ECase::Safe, ECase::Shielded, ECase::Owned, ECase::Foreign,
        ECase::Teleported, ECase::Late, ECase::Unpurchased, ECase::Respecced})
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Player = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 1400), FRotator::ZeroRotator, Spawn);
        if (!TestNotNull(TEXT("actual player"), Player)) return false;
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        auto* Combat = Player->GetCombat(); Combat->BindAttributes(Player->GetAttributes()); Combat->SetComponentTickEnabled(false);
        auto* Progression = Player->GetProgression(); Progression->BindAttributes(Player->GetAttributes());
        // Restored completed-campaign budget, not a claim that this fixture earns the story.
        FBreakerProgressionState State; State.PermanentClass = EBreakerClassId::Tank; State.UnspentDoctrinePoints = 8;
        Progression->LoadProgressionState(State);
        if (Case != ECase::Unpurchased)
        {
            FText Failure;
            // O272: Kinetic Recovery hangs from Bootstraps alone; the pair is two points.
            for (const TCHAR* Id : {TEXT("Tank.Demolitionist.Bootstraps"), TEXT("Tank.Demolitionist.KineticRecovery")})
                if (!TestTrue(Id, Progression->PurchaseNode(UBreakerProgressionLibrary::GetTankDemolitionistTree(), Id, Failure))) return false;
        }
        auto* Movement = Player->GetBreakerMovement();
        Movement->SetComponentTickEnabled(false); Movement->bRunPhysicsWithNoController = true;
        TestEqual(TEXT("shipped safe fall distance"), Movement->SafeFallDistanceCm, 600.0f);
        TestEqual(TEXT("shipped health fraction per metre"), Movement->FallDamageHealthFractionPerMeter, .05f);
        const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float Height = Case == ECase::Safe ? 400.0f : 1200.0f;
        Player->TeleportTo(FVector(0, 0, Height + HalfHeight), FRotator::ZeroRotator, false, true);
        Movement->SetMovementMode(MOVE_Falling); Movement->Velocity = FVector::ZeroVector;
        if (Case == ECase::Shielded)
        {
            FBreakerItemInstance ShieldItem;
            for (int32 Seed = 1; Seed <= 256; ++Seed)
            {
                ShieldItem = UBreakerLootLibrary::RollItem(TEXT("Fall.Shield"), EBreakerEquipSlot::BodyArmour, EBreakerItemRarity::Standard, 1, Seed);
                if (ShieldItem.ArmourArchetype == EBreakerArmourArchetype::Shield) break;
            }
            // Exercise the real level-one base without unrelated affix effects.
            for (auto& Affix : ShieldItem.Affixes) Affix.Value = 0;
            Player->GetEquipment()->BindAttributes(Player->GetAttributes());
            if (!TestTrue(TEXT("actual rolled Shield body equips"), Player->GetEquipment()->EquipItem(ShieldItem))) return false;
            if (!TestTrue(TEXT("equipped body provides a real shield pool"), Player->GetAttributes()->GetMaxShield() > 0)) return false;
        }
        Player->GetAttributes()->ApplyHealth(Player->GetAttributes()->GetMaxHealth());
        Player->GetAttributes()->ApplyShield(Case == ECase::Shielded ? Player->GetAttributes()->GetMaxShield() : 0);
        const float Health = Player->GetAttributes()->GetHealth();
        const float Shield = Player->GetAttributes()->GetShield();
        Combat->DodgeChance = 1.0f; Combat->BlockChance = 1.0f;
        const bool bLaunchCase = Case != ECase::Ordinary && Case != ECase::Safe && Case != ECase::Shielded;
        if (bLaunchCase)
        {
            Player->LaunchCharacter(FVector(0, 0, 100), true, true);
            Movement->NotifyOwnBlastLaunch(); // Source delivery is separately tested through actual Breach.
            if (Case == ECase::Foreign) Player->LaunchCharacter(FVector(0, 0, 100), true, true);
            if (Case == ECase::Teleported) Player->TeleportTo(Player->GetActorLocation() + FVector(100, 0, 0), FRotator::ZeroRotator, false, true);
            if (Case == ECase::Late) for (int32 I = 0; I < 160; ++I) Clock(.02f);
            if (Case == ECase::Respecced)
            {
                FText Failure;
                if (!TestTrue(TEXT("actual Forge respec before landing"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Failure))) return false;
            }
        }
        for (int32 I = 0; I < 300 && !Movement->IsMovingOnGround(); ++I)
        { Clock(.02f); Movement->PerformMovement(.02f); }
        TestTrue(TEXT("real swept physics lands on the floor"), Movement->IsMovingOnGround());
        const bool bProtected = Case == ECase::Owned;
        TestEqual(TEXT("only valid owned launch grants landing immunity"), Combat->IsStaggerImmune(), bProtected);
        if (bProtected || Case == ECase::Safe)
            TestEqual(TEXT("safe or protected landing preserves health"), Player->GetAttributes()->GetHealth(), Health);
        else if (Case == ECase::Shielded)
        {
            TestEqual(TEXT("shield receives fall harm before health"), Player->GetAttributes()->GetHealth(), Health - FMath::Max(0.0f, Health * .30f - Shield), .25f);
            TestTrue(TEXT("actual shield absorbs harmful fall"), Player->GetAttributes()->GetShield() < Shield);
        }
        else
            TestTrue(TEXT("unprotected high fall hurts despite full dodge and block"), Player->GetAttributes()->GetHealth() < Health - 1.0f);
        if (Case == ECase::Ordinary)
            TestEqual(TEXT("twelve metre fall pays six excess metres"), Player->GetAttributes()->GetHealth(), Health * .70f, .25f);
        if (bProtected)
        {
            TestFalse(TEXT("landing immunity refuses real stagger"), Combat->ApplyStagger(.5f));
            for (int32 I = 0; I < 80; ++I) Clock(.02f);
            TestTrue(TEXT("stagger works after authored immunity expires"), Combat->ApplyStagger(.5f));
            TestEqual(TEXT("stagger prevents locomotion"), Movement->GetMaxSpeed(), 0.0f);
            TestFalse(TEXT("stagger refuses actual jump"), Movement->DoJump(false, .02f));
            Player->GetWeapon()->ResetAmmunition();
            const int32 Ammo = Player->GetWeapon()->GetMagazineAmmo();
            Player->GetWeapon()->StartFire();
            TestEqual(TEXT("stagger refuses actual trigger"), Player->GetWeapon()->GetMagazineAmmo(), Ammo);
            TestFalse(TEXT("refused trigger is not queued for later"), Player->GetWeapon()->IsTriggerHeld());
            for (int32 I = 0; I < 30; ++I) Clock(.02f);
            TestTrue(TEXT("locomotion recovers after stagger"), Movement->GetMaxSpeed() > 0.0f);
        }
        Player->Destroy();
    }
    return true;
}
#endif
