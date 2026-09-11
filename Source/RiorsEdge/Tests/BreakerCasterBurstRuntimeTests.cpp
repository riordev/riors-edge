#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Abilities/BreakerAbility_Rot.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCasterBurstRuntimeTest, "RiorsEdge.Combat.CasterBurstRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCasterBurstRuntimeTest::RunTest(const FString& Parameters)
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
    TMap<FIntPoint, int32> BaselineSustainedCasts;
    for (const int32 Depth : {1, 50})
    for (int32 Mode = 0; Mode < 6; ++Mode)
    {
        const bool bPatience = Mode >= 4;
        const bool bFracture = (Mode % 2) == 1 || bPatience;
        const bool bRot = Mode == 2 || Mode == 3 || Mode == 5;
        const FString Label = FString::Printf(TEXT("ilvl%d area%d %s%s%s"), Depth, Depth, bFracture ? TEXT("Fracture") : TEXT("Rifle"), bRot ? TEXT("+Rot") : TEXT(""), bPatience ? TEXT("+Patience2") : TEXT(""));
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
        if (bPatience)
        {
            // Explicit first-benchmark entitlement fixture, not a campaign-playthrough claim.
            // Settle shipped completion flags, then buy through the actual node gates.
            FBreakerQuestFlagSet Flags;
            const auto& Missions = UBreakerMissionLibrary::GetMissions();
            if (Missions.IsEmpty()) return false;
            for (const auto& Beat : Missions[0].Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
            Player->GetProgression()->SettleDoctrineEntitlement(Flags);
            TestEqual(TEXT("first benchmark supplies exactly two Doctrine points"), Player->GetProgression()->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 2);
            FText Reason;
            for (int32 Rank = 0; Rank < 2; ++Rank)
                if (!TestTrue(TEXT("purchases authored Patience rank through normal gates"), Player->GetProgression()->PurchaseNode(
                    UBreakerProgressionLibrary::GetCasterVoidWhispererTree(), TEXT("Caster.VoidWhisperer.Patience"), Reason))) return false;
        }
        FBreakerItemInstance Rifle;
        for (int32 Seed = 1; Seed <= 4096; ++Seed)
        {
            Rifle = UBreakerLootLibrary::RollItem(TEXT("Caster.Burst"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, Depth, Seed);
            if (Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle) break;
        }
        if (!TestTrue(*(Label + TEXT(" rolls actual Rifle depth")), Rifle.IsValid() && Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle)) return false;
        for (auto& Affix : Rifle.Affixes) Affix.Value = 0; // Same neutral depth fixture in every row.
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
        auto* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), Origin + FVector(600, 0, 0), FRotator::ZeroRotator, Spawn);
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
        const auto Fracture = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
        const auto Rot = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Rot::StaticClass(), 1));
        const float StartingHealth = EnemyAttributes->GetHealth();
        const float StartingMana = Mana->GetMana();
        const int32 StartingAmmo = Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo();
        int32 PaidCasts = 0, Reloads = 0, ProjectileSpawns = 0, RotTicks = 0;
        bool bRotOccupied = false;
        // Retain only the UObject's bookkeeping after actual actor destruction:
        // a weak pointer disappears in the same world tick as the final zone tick.
        TStrongObjectPtr<ABreakerZoneActor> CastZone;
        float FirstDeath = -1;
        if (bRot && !TestTrue(*(Label + TEXT(" pays for one opening Rot")), ASC->TryActivateAbility(Rot))) return false;
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
        if (!bFracture) Weapon->StartFire();
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
        int32 OpeningCasts = 0;
        int32 FinalSliceCasts = 0;
        float FinalSliceDamage = 0;
        for (int32 Step = 0; Step < 1200; ++Step)
        {
            Aim();
            if (bFracture && Step % 4 == 0 && ASC->TryActivateAbility(Fracture)) ++PaidCasts;
            BreakerResolvePendingCast(World, Player);
            BeginSpawnedDelivery(); // Begin only safe delivery actors/components, never the save-loading Character.
            const float ManaBeforeTick = Mana->GetMana();
            Advance();
            if (bFracture && Step == 0) TestTrue(*(Label + TEXT(" actual world time regenerates spent Mana")), Mana->GetMana() > ManaBeforeTick);
            if (const auto* Zone = CastZone.Get())
            {
                RotTicks = FMath::Max(RotTicks, Zone->GetTicksDelivered());
                bRotOccupied |= Zone->GetOccupantCount() > 0;
            }
            if (!bFracture)
            {
                const bool bReloading = Weapon->IsReloading();
                if (!bWasReloading && bReloading) ++Reloads;
                if (bWasReloading && !bReloading) Weapon->StartFire(); // Actual new press after normal reload completion.
                bWasReloading = bReloading;
            }
            if (FirstDeath < 0 && EnemyAttributes->GetHealth() <= 0) FirstDeath = (Step + 1) * .05f;
            if ((Step + 1) % 200 == 0)
            {
                const float Damage = SliceHealth - EnemyAttributes->GetHealth();
                const int32 UsedRounds = StartingAmmo - Weapon->GetMagazineAmmo() - Weapon->GetReserveAmmo();
                AddInfo(FString::Printf(TEXT("CASTER SUSTAIN %s seconds%d-%d damage%.3f dps%.3f casts%d rounds%d mana%.2f ammo%d dead%d"),
                    *Label, (Step + 1) / 20 - 10, (Step + 1) / 20, Damage, Damage / 10,
                    PaidCasts - SliceCasts, UsedRounds - SliceRounds, Mana->GetMana(),
                    Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo(), EnemyAttributes->GetHealth() <= 0));
                if (Step == 199) { OpeningDamage = Damage; OpeningCasts = PaidCasts; }
                if (Step == 1199) { FinalSliceDamage = Damage; FinalSliceCasts = PaidCasts - SliceCasts; }
                SliceHealth = EnemyAttributes->GetHealth(); SliceCasts = PaidCasts; SliceRounds = UsedRounds;
            }
        }
        Weapon->StopFire();
        const float WindowDamage = StartingHealth - EnemyAttributes->GetHealth();
        const float EndingMana = Mana->GetMana();
        const int32 Rounds = StartingAmmo - Weapon->GetMagazineAmmo() - Weapon->GetReserveAmmo();
        for (int32 Step = 0; Step < 100; ++Step) Advance(); // Existing projectiles/statuses finish; no new inputs.
        const float TailDamage = StartingHealth - EnemyAttributes->GetHealth() - WindowDamage;
        TestTrue(*(Label + TEXT(" actual attacks damage the real collider")), WindowDamage > 0);
        if (bFracture)
        {
            TestTrue(*(Label + TEXT(" real resource gates permit paid casts")), PaidCasts > 0);
            TestTrue(*(Label + TEXT(" natural Mana recovery supports casts after the opening bank")), PaidCasts > OpeningCasts);
            TestTrue(*(Label + TEXT(" normal Mana recovery still pays casts during seconds 50-60")), FinalSliceCasts > 0);
            TestTrue(*(Label + TEXT(" late paid delivery still reduces actual target health during seconds 50-60")), FinalSliceDamage > 0);
            TestTrue(*(Label + TEXT(" shipped target survives the measurement so sustain is not limited by target death")), FirstDeath < 0);
            const FIntPoint SustainKey(Depth, bRot ? 1 : 0);
            const int32 SustainedCasts = PaidCasts - OpeningCasts;
            if (!bPatience) BaselineSustainedCasts.Add(SustainKey, SustainedCasts);
            else
            {
                // Identical level/gear/input and optional opening Rot. The
                // existing fixture buys only Patience with two earned Doctrine;
                // compare resource-paid casts after the initial bank, not DPS
                // parity, a damage multiplier, or a claimed human clear time.
                const int32* Baseline = BaselineSustainedCasts.Find(SustainKey);
                if (TestNotNull(*(Label + TEXT(" has its matching unpurchased sustain control")), Baseline))
                    TestTrue(*(Label + TEXT(" purchased Patience pays more actual casts during seconds 10-60")), SustainedCasts > *Baseline);
            }
            TestTrue(*(Label + TEXT(" paid casts create actual traveling projectiles")), ProjectileSpawns > 0);
            TestEqual(*(Label + TEXT(" spell row does not spend rifle ammo")), Rounds, 0);
        }
        else TestTrue(*(Label + TEXT(" real trigger consumes ammunition")), Rounds > 0);
        if (bRot)
        {
            TestTrue(*(Label + TEXT(" actual aimed Rot reaches live combat occupancy")), bRotOccupied);
            const auto& Spec = CastZone->GetSpec();
            const int32 ExpectedTicks = FMath::FloorToInt((Spec.Duration + UE_KINDA_SMALL_NUMBER) / Spec.TickInterval);
            TestEqual(*(Label + TEXT(" actual world simulation delivers every Rot boundary tick")), RotTicks, ExpectedTicks);
            TestTrue(*(Label + TEXT(" actual zone releases its leases")), CastZone->IsReleased());
            TestTrue(*(Label + TEXT(" actual zone is destroyed after its final tick")), CastZone->IsActorBeingDestroyed());
        }
        AddInfo(FString::Printf(TEXT("CASTER DELIVERY %s hp%.3f opening10s%.3f total60s%.3f tail5s%.3f casts%d projectiles%d rounds%d reloads%d rotTicks%d mana%.2f->%.2f firstDeath%.2f; one initial bank/ammo supply, stationary neutral target, no build-parity claim"),
            *Label, StartingHealth, OpeningDamage, WindowDamage, TailDamage, PaidCasts, ProjectileSpawns, Rounds, Reloads, RotTicks, StartingMana, EndingMana, FirstDeath));
        ASC->CancelAllAbilities();
        for (const auto& Held : Projectiles) if (auto* Projectile = Held.Get()) Projectile->Destroy();
        Enemy->Destroy(); Controller->UnPossess(); Controller->Destroy(); Player->Destroy();
    }
    return true;
}

// A recast on a live puddle carries the caster's CURRENT footprint: a refresh
// that dropped the radius made an area passive worth zero on recast, and one
// that assigned it would shrink a Lingering-grown puddle back to authored.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerZoneRefreshFootprintRuntimeTest, "RiorsEdge.Combat.ZoneRefreshFootprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerZoneRefreshFootprintRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated zone world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Zone = World->SpawnActor<ABreakerZoneActor>();
    if (!TestNotNull(TEXT("actual zone"), Zone)) return false;
    FBreakerZoneSpec Spec;
    Spec.Duration = 6; Spec.TickInterval = 1; Spec.RadiusCm = 400;
    Zone->ConfigureZone(Spec, nullptr);
    TestEqual(TEXT("authored footprint arms"), Zone->GetSpec().RadiusCm, 400.0f, 0.0001f);
    Spec.RadiusCm = 460; // The caster bought an area node between casts.
    Zone->RefreshPaidPayload(Spec);
    TestEqual(TEXT("a recast adopts the wider footprint"), Zone->GetSpec().RadiusCm, 460.0f, 0.0001f);
    Spec.RadiusCm = 400; // The caster respecced; the live puddle keeps what it has.
    Zone->RefreshPaidPayload(Spec);
    TestEqual(TEXT("a recast never shrinks a live puddle"), Zone->GetSpec().RadiusCm, 460.0f, 0.0001f);
    return true;
}
#endif
