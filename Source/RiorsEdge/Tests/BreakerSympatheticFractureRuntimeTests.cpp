#include "Tests/BreakerFractureTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerEntropy.h"
#include "Combat/BreakerZoneActor.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerStatusCycleComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "TimerManager.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerRocketProjectile.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSympatheticFractureRuntimeTest, "RiorsEdge.Abilities.SympatheticFractureRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSympatheticFractureRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated attunement world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto MakePlayer = [&](FVector Location, EBreakerClassId Class)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Player = World->SpawnActor<ABreakerCharacter>(Location, FRotator::ZeroRotator, Spawn);
        if (!Player) return Player;
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        Player->GetEquipment()->BindAttributes(Player->GetAttributes());
        TestTrue(TEXT("real permanent class choice"), Player->GetProgression()->ChoosePermanentClassById(Class));
        Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        Player->FindComponentByClass<UBreakerChargeComponent>()->BindAttributes(Player->GetAttributes());
        Player->GetAttributes()->ApplyClassResource(100);
        return Player;
    };
    auto* Source = MakePlayer(FVector(0, 200, 100), EBreakerClassId::Support);
    auto* Ally = MakePlayer(FVector(0, 0, 100), EBreakerClassId::Caster);
    if (!Source || !Ally) return false;
    auto Advance = [&](int32 Steps)
    {
        for (int32 I = 0; I < Steps; ++I)
        {
            TArray<UBreakerAbilityStateComponent*> States;
            for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
                if (auto* State = It->FindComponentByClass<UBreakerAbilityStateComponent>())
                { State->SetComponentTickEnabled(false); States.Add(State); }
            ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
            for (auto* State : States) State->AdvanceTime(.05f);
            if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
        }
    };
    auto Purchase = [&](ABreakerCharacter* Player)
    {
        auto* Progression = Player->GetProgression();
        // Restored authored campaign completion, never extra Doctrine points.
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, FBreakerExperienceCurve()));
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
            for (const auto& Beat : Mission.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
        FText Failure;
        // Attunement is a single-rank travel root (O272): one buy, no prerequisite.
        auto* Tree = UBreakerProgressionLibrary::GetSupportConductorTree();
        if (!TestTrue(TEXT("purchase actual Attunement"), Progression->PurchaseNode(Tree, TEXT("Support.Conductor.Attunement"), Failure))) return false;
        Player->FindComponentByClass<UBreakerChargeComponent>()->BindAttributes(Player->GetAttributes());
        return true;
    };
    auto CastBuff = [&](ABreakerCharacter* Player, TSubclassOf<UBreakerGameplayAbility> Ability)
    {
        Player->GetAttributes()->ApplyClassResource(100);
        auto* ASC = Player->GetAbilitySystemComponent();
        // Reuse the same isolated recipient rig across lifecycle cases. Clear
        // only this ability's cooldown; cooldown behavior is not this fixture.
        FGameplayTagContainer Cooldown;
        Cooldown.AddTag(Ability->GetDefaultObject<UBreakerGameplayAbility>()->GetAbilityDefinition()->CooldownTag);
        ASC->RemoveActiveEffectsWithGrantedTags(Cooldown);
        const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(Ability, 1));
        TestTrue(TEXT("native buff activation commits"), ASC->TryActivateAbility(Handle));
        BreakerResolvePendingCast(World, Player);
        return Handle;
    };
    auto FinishSympathetic = [&](ABreakerCharacter* Player)
    {
        FText Failure;
        auto* P = Player->GetProgression();
        auto* Tree = UBreakerProgressionLibrary::GetSupportConductorTree();
        // Sympathetic Resonance is the impactful half of Attunement's pair
        // (O272): it follows its travel and nothing else gates it.
        if (!TestTrue(TEXT("real Sympathetic purchase"), P->PurchaseNode(Tree, TEXT("Support.Conductor.SympatheticResonance"), Failure))) return false;
        TestEqual(TEXT("the pair spends two of the eight earned Doctrine"), P->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), UBreakerProgressionLibrary::DoctrinePointGrant - 2);
        return true;
    };
    if (!Purchase(Source)) return false;
    Advance(1);
    auto FirstCast = CastBuff(Source, UBreakerAbility_Cadence::StaticClass());
    auto* AllyState = Ally->FindComponentByClass<UBreakerAbilityStateComponent>();
    if (!TestNotNull(TEXT("actual ally state"), AllyState)) return false;
    TestFalse(TEXT("Attunement alone grants no Sympathetic"), AllyState->HasActiveSympatheticAttunement());
    if (!FinishSympathetic(Source)) return false;
    TestTrue(TEXT("purchased node consumes an actual maintained buff"), AllyState->HasActiveSympatheticAttunement());

    FBreakerItemInstance Rifle;
    for (int32 Seed = 1; Seed <= 4096; ++Seed)
    {
        Rifle = UBreakerLootLibrary::RollItem(TEXT("Sympathetic.Fracture"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
        if (Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle) break;
    }
    if (!TestTrue(TEXT("legal level-one rifle"), Rifle.IsValid() && Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle)) return false;
    for (auto& Affix : Rifle.Affixes) Affix.Value = 0;
    if (!TestTrue(TEXT("legal equip without additional offense"), Ally->GetEquipment()->EquipItem(Rifle))) return false;
    auto* Weapon = Ally->GetWeapon();
    Weapon->RegisterAllComponentTickFunctions(true); Weapon->SetComponentTickEnabled(true); Weapon->BeginPlay();
    Weapon->SyncArchetypesToEquipment(); Weapon->EquipSlot(1);
    Ally->GetMana()->BindAttributes(Ally->GetAttributes());
    Advance(30); // Ordinary initial equipment swap; no refill during delivery.
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!Controller) return false;
    Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->SetAsLocalPlayerController();
    Controller->Possess(Ally); Controller->SetViewTarget(Ally);
    Controller->SetInitialLocationAndRotation(Ally->GetActorLocation(), FRotator::ZeroRotator);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), FVector(1000, 0, 100), FRotator::ZeroRotator, Spawn);
    if (!Enemy) return false;
    Enemy->ConfigureCrowdProbe(); Enemy->SetAreaLevel(1); Enemy->SetMonsterRank(EBreakerMonsterRank::Boss);
    Enemy->DispatchBeginPlay(); Enemy->RegisterAllActorTickFunctions(true, true);
    Enemy->SetActorTickEnabled(false);
    if (auto* Movement = Enemy->GetMovementComponent()) Movement->SetComponentTickEnabled(false);
    auto* Status = Enemy->FindComponentByClass<UBreakerStatusComponent>();
    const auto* Health = Enemy->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>();
    if (!Status || !Health) return false;
    TestTrue(TEXT("real enemy status component has registered world ticking"), Status->PrimaryComponentTick.IsTickFunctionRegistered());
    FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye, Facing);
    Controller->SetControlRotation((Enemy->GetActorLocation() + FVector(0, 0, 25) - Eye).Rotation());
    if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(.05f);
    if (!TestTrue(TEXT("actual maintained Cadence still attunes the cast"), AllyState->HasActiveSympatheticAttunement())) return false;
    const float HealthBefore = Health->GetHealth();
    const float ManaBefore = Ally->GetMana()->GetMana();
    auto* AllyASC = Ally->GetAbilitySystemComponent();
    // Explicit delivery-isolation fixture: select the existing Entropy cycle
    // position through its public cursor API. This is not progression or a
    // sustained-resource claim; the measured cast still pays its real cost.
    auto* Cycle = UBreakerStatusCycleComponent::FindOrAdd(Ally);
    if (!Cycle) return false;
    for (int32 Position = 0; Position < Cycle->GetCycleLength()
        && Cycle->PeekNextEntry().Element != EBreakerElement::Entropy; ++Position) Cycle->AdvanceCycle();
    if (!TestEqual(TEXT("fixture selects the actual authored Entropy position"), Cycle->PeekNextEntry().Element, EBreakerElement::Entropy)) return false;
    const auto Fracture = AllyASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
    if (!TestTrue(TEXT("real Fracture cast commits"), AllyASC->TryActivateAbility(Fracture))) return false;
    BreakerResolvePendingCast(World, Ally);
    TestTrue(TEXT("real Fracture spends Mana"), Ally->GetMana()->GetMana() < ManaBefore);
    if (!BreakerWaitForFractureCast(World, AllyASC, Fracture)) return false;
    ABreakerProjectileBase* Projectile = nullptr;
    for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It)
        if (It->GetOwner() == Ally) { Projectile = *It; break; }
    if (!TestNotNull(TEXT("cast spawns actual Fracture projectile"), Projectile)) return false;
    const FBreakerDamageRequest CastSnapshot = Projectile->GetProjectileDamage();
    TestEqual(TEXT("real Fracture payload is Entropy"), CastSnapshot.Element, EBreakerElement::Entropy);
    TestEqual(TEXT("real payload snapshots flat Sympathetic buildup"), CastSnapshot.ElementBuildupFlat, BreakerEntropy::SympatheticFlatBuildup());
    const FVector LaunchAt = Projectile->GetActorLocation();
    if (!Projectile->HasActorBegunPlay()) Projectile->DispatchBeginPlay();
    Projectile->RegisterAllActorTickFunctions(true, true);
    Source->GetAbilitySystemComponent()->CancelAbilityHandle(FirstCast);
    TestFalse(TEXT("cancelled maintained buff no longer grants new-hit protection"), AllyState->HasActiveSympatheticAttunement());
    TestEqual(TEXT("spawn and cancellation do not apply instant damage"), Health->GetHealth(), HealthBefore);
    TWeakObjectPtr<ABreakerProjectileBase> Flying = Projectile;
    bool bTravelled = false;
    for (int32 Step = 0; Step < 40 && Health->GetHealth() == HealthBefore; ++Step)
    {
        Advance(1);
        if (auto* Live = Flying.Get()) bTravelled |= FVector::Dist(LaunchAt, Live->GetActorLocation()) > 1;
    }
    TestTrue(TEXT("Fracture travels through world simulation before collision"), bTravelled);
    const float LandedDamage = HealthBefore - Health->GetHealth();
    if (!TestTrue(TEXT("traveling Fracture actually damages shipped enemy"), LandedDamage > 0)) return false;
    const float Earned = Status->GetEntropyBuildup();
    TestTrue(TEXT("authored boss chassis naturally retains subthreshold contribution"), Earned < Status->GetEntropyThreshold());
    // Raw hit snapshot, not post-armour health loss, earns ordinary buildup.
    // The native Vestige's resistance reduces both that and the flat bonus.
    const float SnapshotRaw = UBreakerDamageLibrary::ResolveDamage(CastSnapshot, FBreakerDefenseState()).RawDamage;
    const float ExpectedBuildup = (SnapshotRaw * CastSnapshot.ElementalFraction + BreakerEntropy::SympatheticFlatBuildup())
        * FMath::Clamp(CastSnapshot.ProcCoefficient, 0.0f, 1.0f) * (1.0f - Status->GetEntropyResistancePercent() / 100.0f);
    TestEqual(TEXT("actual impact carries resistance-scaled raw hit and flat bonus captured before cancellation"), Earned, ExpectedBuildup, .01f);
    Advance(78); // 3.9 real seconds after the observed impact: inside own grace.
    TestEqual(TEXT("protected contribution survives ordinary grace"), Status->GetEntropyBuildup(), Earned, .01f);
    Advance(22); // 5.0 seconds: one second into the authored four-second fade.
    const float Expected = Earned * (1.0f - 1.0f / BreakerEntropy::SympatheticFadeSeconds());
    // Impact can occur before this frame's status tick, shifting its age by at
    // most one .05s simulation step. Bound that real ordering uncertainty.
    const float OneTickFade = Earned * .051f / BreakerEntropy::SympatheticFadeSeconds();
    TestEqual(TEXT("actual world time linearly fades protected buildup after grace"), Status->GetEntropyBuildup(), Expected, OneTickFade);
    Advance(62);
    TestEqual(TEXT("protected buildup expires without manual status advancement"), Status->GetEntropyBuildup(), 0.0f);
    return true;
}
#endif
