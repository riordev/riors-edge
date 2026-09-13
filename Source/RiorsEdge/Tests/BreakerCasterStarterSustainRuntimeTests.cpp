#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Tests/BreakerCasterDamageSplitObserver.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Progression/BreakerExperience.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerZoneActor.h"
#include "Combat/BreakerZoneMath.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerMissionContent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCasterStarterSustainRuntimeTest, "RiorsEdge.Combat.CasterStarterSustainRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCasterStarterSustainRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated paid cast and real rifle world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    int32 Scenario = 0;
    for (const int32 Depth : {1, 20})
    for (int32 Mode = 0; Mode < (Depth == 20 ? 6 : 4); ++Mode)
    {
        const bool bFracture = Mode >= 4;
        const bool bCleave = !bFracture && (Mode % 2) == 1;
        const bool bCasting = bCleave || bFracture;
        const bool bRot = Mode == 2 || Mode == 3 || Mode == 5;
        const FString Label = FString::Printf(TEXT("level%d ilvl%d area%d %s%s"), Depth, Depth, Depth, bFracture ? TEXT("Fracture[AbilityPool]") : (bCleave ? TEXT("Cleave[WeaponPool]") : TEXT("Rifle[WeaponPool]")), bRot ? TEXT("+Rot") : TEXT(""));
        const FVector Origin(Scenario++ * 10000.0f, 0, 100);
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Player = World->SpawnActor<ABreakerCharacter>(ABreakerCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Spawn);
        auto* Controller = World->SpawnActor<APlayerController>();
        if (!Player || !Controller) return false;
        Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->SetAsLocalPlayerController();
        Controller->Possess(Player); Controller->SetViewTarget(Player);
        Controller->SetInitialLocationAndRotation(Origin, FRotator::ZeroRotator);
        Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        TestEqual(TEXT("shipped player capsule blocks enemy projectiles"), Player->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Block);
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        if (!TestTrue(*(Label + TEXT(" chooses Caster without extra points")), Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
        Player->bRefuseSavesForPendingCharacter = true;
        auto* Progression = Player->GetProgression();
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(Depth,Progression->ExperienceCurve));
        FText Reason; auto* Abilities = Player->GetAbilities();
        if (!TestTrue(TEXT("Native free starter Cleave equips"), Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityOne,TEXT("Caster.Cleave"),Reason))) return false;
        if (!TestTrue(TEXT("Native free starter Rot equips"), Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityTwo,TEXT("Caster.Rot"),Reason))) return false;
        if (bFracture)
        {
            const int32 TokensBefore = Progression->GetUnspentAbilityTokens();
            if (!TestTrue(TEXT("Level-earned token unlocks actual Fracture"), Progression->SpendAbilityToken(TEXT("Caster.Fracture"),Reason))) return false;
            TestEqual(TEXT("Fracture consumes one earned token"), Progression->GetUnspentAbilityTokens(), TokensBefore-1);
            if (!TestTrue(TEXT("Purchased Fracture equips through ordinary slot API"), Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityOne,TEXT("Caster.Fracture"),Reason))) return false;
        }
        Abilities->RefreshGrants();
        // Regional-level starter kit, deliberately no purchased Core/Doctrine:
        // this is delivery/economy evidence, not a representative midgame build.
        FBreakerItemInstance Rifle;
        for (int32 Seed = 1; Seed <= 4096; ++Seed)
        {
            Rifle = UBreakerLootLibrary::RollItem(TEXT("Caster.Burst"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, Depth, Seed);
            if (Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle) break;
        }
        if (!TestTrue(*(Label + TEXT(" rolls actual Rifle depth")), Rifle.IsValid() && Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle)) return false;
        Player->GetEquipment()->BindAttributes(Player->GetAttributes());
        if (!TestTrue(*(Label + TEXT(" equips actual Rifle")), Player->GetEquipment()->EquipItem(Rifle))) return false;
        auto* Weapon = Player->GetWeapon();
        Weapon->RegisterAllComponentTickFunctions(true); Weapon->SetComponentTickEnabled(true); Weapon->BeginPlay();
        Weapon->SyncArchetypesToEquipment(); Weapon->EquipSlot(1);
        auto* Mana = Player->GetMana();
        Mana->RegisterAllComponentTickFunctions(true); Mana->SetComponentTickEnabled(true); Mana->BeginPlay();
        Mana->BindAttributes(Player->GetAttributes());
        TestTrue(TEXT("isolated fixture registers actual Mana ticking"), Mana->PrimaryComponentTick.IsTickFunctionRegistered());
        TestTrue(TEXT("isolated fixture registers actual weapon ticking"), Weapon->PrimaryComponentTick.IsTickFunctionRegistered());
        Mana->AdvanceLoop(20.0f); // Ordinary pre-fight recovery, never replenished during the run.
        auto Advance = [&] { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); };
        for (int32 Step = 0; Step < 30; ++Step) Advance(); // Real equipment swap completes before timing.
        Weapon->ResetAmmunition(); // Normal starting ammunition, once before measurement.
        auto* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), Origin + FVector(300, 0, 0), FRotator::ZeroRotator, Spawn);
        if (!Enemy) return false;
        Enemy->ConfigureCrowdProbe(); Enemy->SetAreaLevel(Depth); Enemy->SetMonsterRank(EBreakerMonsterRank::Boss);
        Enemy->DispatchBeginPlay(); Enemy->RegisterAllActorTickFunctions(true, true); Enemy->SetActorTickEnabled(false);
        if (auto* Movement = Enemy->GetMovementComponent()) Movement->SetComponentTickEnabled(false);
        const auto* EnemyAttributes = Enemy->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>();
        if (!TestNotNull(TEXT("actual shipped boss-rank attributes"), EnemyAttributes)) return false;
        auto Aim = [&]
        {
            FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye, Facing);
            Controller->SetControlRotation((Enemy->GetActorLocation() + FVector(0, 0, 25) - Eye).Rotation());
            if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(.05f);
        };
        Aim();
        auto* DamageSplit=NewObject<UBreakerCasterDamageSplitObserver>(Enemy);
        TStrongObjectPtr<UBreakerCasterDamageSplitObserver> HeldDamageSplit(DamageSplit);
        auto* TargetCombat=Enemy->FindComponentByClass<UBreakerCombatComponent>();
        if(!TargetCombat)return false;
        TargetCombat->OnDamageTaken.AddDynamic(DamageSplit,&UBreakerCasterDamageSplitObserver::Observe);
        double SliceDirect=0,SlicePeriodic=0,SliceBleed=0,SliceEntropy=0,SliceVoid=0,SliceRift=0;
        int32 SliceDirectHits=0,SlicePeriodicHits=0,SliceCriticalHits=0;
        const float StartingHealth = EnemyAttributes->GetHealth();
        const float StartingMana = Mana->GetMana();
        const int32 StartingAmmo = Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo();
        int32 PaidCasts = 0, Reloads = 0, ProjectileSpawns = 0, RotTicks = 0;
        // Count actual commits, including queued wind-ups that start between
        // input presses. TryActivateSlot returns false when it only buffers.
        const FDelegateHandle CommitObserver = ASC->AbilityCommittedCallbacks.AddLambda([&](UGameplayAbility* Ability)
        {
            const auto* Spec = Ability ? ASC->FindAbilitySpecFromHandle(Ability->GetCurrentAbilitySpecHandle()) : nullptr;
            if (Spec && Spec->InputID == static_cast<int32>(EBreakerAbilitySlot::ClassAbilityOne)) ++PaidCasts;
        });
        ON_SCOPE_EXIT { ASC->AbilityCommittedCallbacks.Remove(CommitObserver); };
        bool bRotOccupied = false;
        // Retain only the UObject's bookkeeping after actual actor destruction:
        // a weak pointer disappears in the same world tick as the final zone tick.
        TStrongObjectPtr<ABreakerZoneActor> CastZone;
        float FirstDeath = -1;
        if (bRot && !TestTrue(*(Label + TEXT(" pays for one opening Rot")), Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo))) return false;
        BreakerResolvePendingCast(World, Player);
        if (bRot)
        {
            for (const auto& Held : ABreakerZoneActor::GetLiveZones())
                if (auto* Zone = Held.Get(); Zone && Zone->GetZoneInstigator() == Player) { CastZone.Reset(Zone); break; }
            if (!TestTrue(*(Label + TEXT(" real aimed cast creates a zone")), CastZone.IsValid())) return false;
            const auto& Spec = CastZone->GetSpec();
            TestTrue(*(Label + TEXT(" real weapon-query aim places Rot on the actual enemy")),
                UBreakerZoneMath::IsInsideZone(CastZone->GetActorLocation(), Spec.RadiusCm, Spec.HalfHeightCm, Enemy->GetActorLocation()));
        }
        if (!bCasting) Weapon->StartFire();
        bool bWasReloading = Weapon->IsReloading();
        TSet<TWeakObjectPtr<ABreakerProjectileBase>> Projectiles;
        auto BeginSpawnedDelivery = [&]
        {
            for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It)
                if (It->GetOwner() == Player && !Projectiles.Contains(*It))
                {
                    Projectiles.Add(*It); ++ProjectileSpawns;
                    if (!It->HasActorBegunPlay()) It->DispatchBeginPlay();
                    It->RegisterAllActorTickFunctions(true, true);
                    if (const auto* Collision = Cast<UPrimitiveComponent>(It->GetRootComponent()))
                        TestEqual(TEXT("actual traveling projectile uses its authored object channel"), Collision->GetCollisionObjectType(), ECC_GameTraceChannel1);
                    if (auto* Movement = It->FindComponentByClass<UProjectileMovementComponent>())
                    {
                        Movement->SetComponentTickEnabled(true);
                        TestTrue(TEXT("actual projectile movement registered for world simulation"), Movement->PrimaryComponentTick.IsTickFunctionRegistered());
                    }
                }
            // BeginPlay prunes/adds entries in the live registry; never iterate that mutable array directly.
            const auto Zones = ABreakerZoneActor::GetLiveZones();
            for (const auto& Held : Zones)
                if (auto* Zone = Held.Get(); Zone && Zone->GetZoneInstigator() == Player && !Zone->HasActorBegunPlay())
                {
                    Zone->DispatchBeginPlay(); Zone->RegisterAllActorTickFunctions(true, true); Zone->SetActorTickEnabled(true);
                }
        };
        // A declared five-presses/s input cadence, not a new ability cooldown.
        // The actual resource/cooldown gates decide how many casts are accepted.
        float SliceHealth = StartingHealth;
        int32 SliceCasts = 0, SliceRounds = 0;
        float OpeningDamage = 0;
        bool bOpeningObserved = false;
        int32 OpeningCasts = 0;
        int32 FinalSliceCasts = 0;
        float FinalSliceDamage = 0;
        for (int32 Step = 0; Step < 1200; ++Step)
        {
            Aim();
            if (bCasting && Step % 4 == 0)
            {
                const float Quoted = Abilities->GetResourceCostForSlot(EBreakerAbilitySlot::ClassAbilityOne);
                const float BeforeCast = Mana->GetMana();
                if (Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne))
                {
                    // Cleave immediately earns ordinary melee/status income;
                    // net Mana is not identical to the pre-hit debit. Record both
                    // without suppressing those legitimate native callbacks.
                    if (PaidCasts == 1) AddInfo(FString::Printf(TEXT("STARTER CAST %s quote%.3f bank%.3f->%.3f including immediate hit/status income"), *Label, Quoted, BeforeCast, Mana->GetMana()));
                }
            }
            BeginSpawnedDelivery(); // Begin only safe delivery actors/components, never the save-loading Character.
            const float ManaBeforeTick = Mana->GetMana();
            Advance();
            if (bCasting && Step == 0) TestTrue(*(Label + TEXT(" actual world time regenerates spent Mana")), Mana->GetMana() > ManaBeforeTick);
            if (const auto* Zone = CastZone.Get())
            {
                RotTicks = FMath::Max(RotTicks, Zone->GetTicksDelivered());
                bRotOccupied |= Zone->GetOccupantCount() > 0;
            }
            if (!bCasting)
            {
                const bool bReloading = Weapon->IsReloading();
                if (!bWasReloading && bReloading) ++Reloads;
                if (bWasReloading && !bReloading) Weapon->StartFire(); // Actual new press after normal reload completion.
                bWasReloading = bReloading;
            }
            if (EnemyAttributes->GetHealth() <= 0)
            {
                FirstDeath = (Step + 1) * .05f;
                AddInfo(FString::Printf(TEXT("STARTER KIT CENSORED %s target died at %.2fs; later fixed-window DPS and net-ammo comparisons are unavailable, native kill rewards may add ammo."), *Label, FirstDeath));
                break;
            }
            if ((Step + 1) % 200 == 0)
            {
                const float Damage = SliceHealth - EnemyAttributes->GetHealth();
                const int32 UsedRounds = StartingAmmo - Weapon->GetMagazineAmmo() - Weapon->GetReserveAmmo();
                AddInfo(FString::Printf(TEXT("STARTER KIT SUSTAIN %s seconds%d-%d damage%.3f dps%.3f casts%d rounds%d mana%.2f ammo%d dead%d"),
                    *Label, (Step + 1) / 20 - 10, (Step + 1) / 20, Damage, Damage / 10,
                    PaidCasts - SliceCasts, UsedRounds - SliceRounds, Mana->GetMana(),
                    Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo(), EnemyAttributes->GetHealth() <= 0));
                AddInfo(FString::Printf(TEXT("STARTER ACCEPTED SPLIT %s seconds%d-%d directHealth%.3f periodicHealth%.3f bleedHealth%.3f entropyHealth%.3f voidHealth%.3f riftHealth%.3f directHits%d periodicHits%d directCrits%d; element/status subtotals overlap direct/periodic, no requested or overkill damage"),
                    *Label,(Step+1)/20-10,(Step+1)/20,DamageSplit->Direct-SliceDirect,DamageSplit->Periodic-SlicePeriodic,
                    DamageSplit->Bleed-SliceBleed,DamageSplit->Entropy-SliceEntropy,DamageSplit->Void-SliceVoid,DamageSplit->Rift-SliceRift,
                    DamageSplit->DirectHits-SliceDirectHits,DamageSplit->PeriodicHits-SlicePeriodicHits,DamageSplit->DirectCriticalHits-SliceCriticalHits));
                SliceDirect=DamageSplit->Direct;SlicePeriodic=DamageSplit->Periodic;SliceBleed=DamageSplit->Bleed;
                SliceEntropy=DamageSplit->Entropy;SliceVoid=DamageSplit->Void;SliceRift=DamageSplit->Rift;
                SliceDirectHits=DamageSplit->DirectHits;SlicePeriodicHits=DamageSplit->PeriodicHits;SliceCriticalHits=DamageSplit->DirectCriticalHits;
                if (Step == 199) { OpeningDamage = Damage; OpeningCasts = PaidCasts; bOpeningObserved = true; }
                if (Step == 1199) { FinalSliceDamage = Damage; FinalSliceCasts = PaidCasts - SliceCasts; }
                SliceHealth = EnemyAttributes->GetHealth(); SliceCasts = PaidCasts; SliceRounds = UsedRounds;
            }
        }
        Weapon->StopFire();
        const float WindowDamage = StartingHealth - EnemyAttributes->GetHealth();
        const double WindowDirect=DamageSplit->Direct,WindowPeriodic=DamageSplit->Periodic;
        AddInfo(FString::Printf(TEXT("STARTER ACCEPTED WINDOW %s observedSeconds%.2f directHealth%.3f periodicHealth%.3f sum%.3f netHealthLoss%.3f bleedHealth%.3f directHits%d periodicHits%d directCrits%d"),
            *Label,FirstDeath<0?60.f:FirstDeath,WindowDirect,WindowPeriodic,WindowDirect+WindowPeriodic,WindowDamage,
            DamageSplit->Bleed,DamageSplit->DirectHits,DamageSplit->PeriodicHits,DamageSplit->DirectCriticalHits));
        const float EndingMana = Mana->GetMana();
        const int32 Rounds = StartingAmmo - Weapon->GetMagazineAmmo() - Weapon->GetReserveAmmo();
        BeginSpawnedDelivery(); // Include actors accepted during the final measurement tick.
        for (int32 Step = 0; Step < 100; ++Step)
        {
            Advance();
            BeginSpawnedDelivery(); // Drain native delayed deliveries; never issue another input.
        }
        const float TailDamage = StartingHealth - EnemyAttributes->GetHealth() - WindowDamage;
        AddInfo(FString::Printf(TEXT("STARTER ACCEPTED TAIL %s directHealth%.3f periodicHealth%.3f netHealthLoss%.3f"),
            *Label,DamageSplit->Direct-WindowDirect,DamageSplit->Periodic-WindowPeriodic,TailDamage));
        TestTrue(*(Label + TEXT(" actual attacks damage the real collider")), WindowDamage > 0);
        if (bCasting)
        {
            TestTrue(*(Label + TEXT(" real resource gates permit paid casts")), PaidCasts > 0);
            if (FirstDeath < 0)
            {
                TestTrue(*(Label + TEXT(" natural Mana recovery supports casts after the opening bank")), bOpeningObserved && PaidCasts > OpeningCasts);
                TestTrue(*(Label + TEXT(" normal Mana recovery still pays casts during seconds 50-60")), FinalSliceCasts > 0);
                TestTrue(*(Label + TEXT(" late paid delivery still reduces actual target health during seconds 50-60")), FinalSliceDamage > 0);
                TestEqual(*(Label + TEXT(" surviving-target casting row does not spend rifle ammo")), Rounds, 0);
            }
            if (bFracture) TestTrue(*(Label + TEXT(" actual Ability-pool Fracture produces traveling projectiles")), ProjectileSpawns > 0);
        }
        else if (FirstDeath < 0) TestTrue(*(Label + TEXT(" real trigger consumes ammunition")), Rounds > 0);
        // This is an unpinned measurement, not a requirement that ordinary crits
        // cannot kill the target. A kill ends observation; never invent health
        // or treat native kill rewards as negative ammunition consumption.
        if (bRot)
        {
            TestTrue(*(Label + TEXT(" actual aimed Rot reaches live combat occupancy")), bRotOccupied);
            const auto& Spec = CastZone->GetSpec();
            const int32 ExpectedTicks = FMath::FloorToInt((Spec.Duration + UE_KINDA_SMALL_NUMBER) / Spec.TickInterval);
            TestEqual(*(Label + TEXT(" actual world simulation delivers every Rot boundary tick")), RotTicks, ExpectedTicks);
            TestTrue(*(Label + TEXT(" actual zone releases its leases")), CastZone->IsReleased());
            TestTrue(*(Label + TEXT(" actual zone is destroyed after its final tick")), CastZone->IsActorBeingDestroyed());
        }
        AddInfo(FString::Printf(TEXT("STARTER KIT DELIVERY %s hp%.3f openingAvailable%d opening10s%.3f observedSeconds%.2f windowDamage%.3f tail5s%.3f casts%d projectiles%d netRounds%d reloads%d rotTicks%d mana%.2f->%.2f firstDeath%.2f; one initial bank/ammo supply, native kill rewards retained, stationary shipped probe target, unchanged legal rolled affixes, no allocations or build-parity claim"),
            *Label, StartingHealth, bOpeningObserved, OpeningDamage, FirstDeath < 0 ? 60.0f : FirstDeath, WindowDamage, TailDamage, PaidCasts, ProjectileSpawns, Rounds, Reloads, RotTicks, StartingMana, EndingMana, FirstDeath));
        ASC->CancelAllAbilities();
        for (const auto& Held : Projectiles) if (auto* Projectile = Held.Get()) Projectile->Destroy();
        Enemy->Destroy(); Controller->UnPossess(); Controller->Destroy(); Player->Destroy();
    }
    return true;
}
#endif
