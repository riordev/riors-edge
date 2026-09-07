#include "Misc/AutomationTest.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAttunementRuntimeTest, "RiorsEdge.Abilities.AttunementRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerAttunementRuntimeTest::RunTest(const FString& Parameters)
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
    auto* Other = MakePlayer(FVector(0, -200, 100), EBreakerClassId::Support);
    auto* Ally = MakePlayer(FVector(0, 0, 100), EBreakerClassId::Swift);
    if (!Source || !Other || !Ally) return false;
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
    auto Purchase = [&](ABreakerCharacter* Player, int32 Rank)
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
        auto* Tree = UBreakerProgressionLibrary::GetSupportConductorTree();
        for (int32 I = 0; I < 2; ++I)
            if (!TestTrue(TEXT("purchase real Section prerequisite and tier investment"), Progression->PurchaseNode(Tree, TEXT("Support.Conductor.Section"), Failure))) return false;
        for (int32 I = 0; I < Rank; ++I)
            if (!TestTrue(TEXT("purchase actual Attunement rank"), Progression->PurchaseNode(Tree, TEXT("Support.Conductor.Attunement"), Failure))) return false;
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
        return Handle;
    };
    auto Conversion = [&](ABreakerCharacter* Player)
    {
        auto* State = Player->FindComponentByClass<UBreakerAbilityStateComponent>();
        return State ? State->GetWeaponEntropyConversionFraction() : 0.0f;
    };
    Advance(1);
    const auto PlainBuff = CastBuff(Source, UBreakerAbility_Cadence::StaticClass());
    TestEqual(TEXT("unpurchased buff cannot convert"), Conversion(Ally), 0.0f);
    Source->GetAbilitySystemComponent()->CancelAbilityHandle(PlainBuff);
    if (!Purchase(Source, 1) || !Purchase(Other, 2)) return false;
    const auto FirstBuff = CastBuff(Source, UBreakerAbility_Cadence::StaticClass());
    TestEqual(TEXT("Conductor attunes self first"), Conversion(Source), 1.0f);
    TestEqual(TEXT("actual nearby ally receives conversion"), Conversion(Ally), 1.0f);
    Ally->SetActorLocation(FVector(0, 3000, 100)); Advance(2);
    TestEqual(TEXT("rank one aura exit has no tail"), Conversion(Ally), 0.0f);
    Ally->SetActorLocation(FVector(0, 0, 100)); Advance(2);

    auto* Target = World->SpawnActor<AActor>();
    if (!Target) return false;
    auto* Body = NewObject<USphereComponent>(Target); Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
    Body->SetSphereRadius(80); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); Body->RegisterComponent();
    auto* Combat = NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(1000000); Health->ApplyHealth(1000000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Target); Target->AddInstanceComponent(Status); Status->RegisterComponent(); Status->BeginPlay(); Status->SetComponentTickEnabled(false);
    auto* Weapon = Ally->GetWeapon();
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = 0; Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    Weapon->WeaponDefinition->Recoil.BloomPerShotDegrees = 0; Weapon->WeaponDefinition->BleedChance = 0;
    auto Fire = [&]
    {
        const auto* Active = Weapon->GetActiveDefinition();
        const float ReadyAfter = FMath::Max(Active->SwapInDuration / Weapon->GetSwapSpeedMultiplier(),
            60.0f / FMath::Max(1.0f, Weapon->GetEffectiveRoundsPerMinute(Active)));
        Advance(FMath::CeilToInt((ReadyAfter + .1f) / .05f));
        FVector Eye; FRotator Aim; Ally->GetActorEyesViewPoint(Eye, Aim); Target->SetActorLocation(Eye + Aim.Vector() * 500);
        Ally->GetAbilitySystemComponent()->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0);
        Weapon->ResetAmmunition(); const int32 Before = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("actual trigger spends ammunition"), Weapon->GetMagazineAmmo(), Before - 1);
    };
    const float BeforeHit = Health->GetHealth(); Fire();
    const float ConvertedDamage = BeforeHit - Health->GetHealth();
    TestTrue(TEXT("attuned hitscan damages actual collision target"), ConvertedDamage > 0);
    TestTrue(TEXT("attuned hitscan produces real Entropy buildup"), Status->GetEntropyBuildup() > 0);
    Source->GetAbilitySystemComponent()->CancelAbilityHandle(FirstBuff);
    TestEqual(TEXT("rank one cancellation removes conversion immediately"), Conversion(Ally), 0.0f);
    const float BeforePlain = Health->GetHealth(), BeforePlainBuildup = Status->GetEntropyBuildup(); Fire();
    TestEqual(TEXT("conversion adds no base damage"), BeforePlain - Health->GetHealth(), ConvertedDamage);
    TestEqual(TEXT("unbuffed actual shot adds no buildup"), Status->GetEntropyBuildup(), BeforePlainBuildup);

    const auto TailBuff = CastBuff(Other, UBreakerAbility_Cadence::StaticClass());
    Ally->SetActorLocation(FVector(0, 3000, 100)); Advance(2);
    TestEqual(TEXT("rank two lingers after actual aura exit"), Conversion(Ally), 1.0f);
    Advance(50);
    TestEqual(TEXT("rank two tail expires"), Conversion(Ally), 0.0f);
    Ally->SetActorLocation(FVector(0, 0, 100)); Advance(2);
    TestEqual(TEXT("actual aura reentry restores conversion"), Conversion(Ally), 1.0f);
    const auto Overlap = CastBuff(Source, UBreakerAbility_Metronome::StaticClass());
    Other->GetAbilitySystemComponent()->CancelAbilityHandle(TailBuff);
    TestEqual(TEXT("one canceled source cannot erase another"), Conversion(Ally), 1.0f);
    Source->GetAbilitySystemComponent()->CancelAbilityHandle(Overlap);
    TestEqual(TEXT("last canceled source removes conversion"), Conversion(Ally), 0.0f);

    const auto Metro = CastBuff(Other, UBreakerAbility_Metronome::StaticClass());
    Weapon->EquipSlot(2); const float BeforeSecondary = Status->GetEntropyBuildup(); Fire();
    TestTrue(TEXT("buff attunes actual Secondary too"), Status->GetEntropyBuildup() > BeforeSecondary);
    FBreakerItemInstance RocketItem;
    bool bRolledRocket = false;
    for (int32 Seed = 1; Seed <= 1000 && !bRolledRocket; ++Seed)
    {
        RocketItem = UBreakerLootLibrary::RollItem(TEXT("Attunement.Runtime"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
        bRolledRocket = RocketItem.WeaponArchetype == EBreakerWeaponArchetype::Rocket;
    }
    if (!TestTrue(TEXT("genuine level-one rocket roll"), bRolledRocket)) return false;
    for (auto& Affix : RocketItem.Affixes) Affix.Value = 0; // Isolate buff conversion from gear effects.
    TestTrue(TEXT("actual rocket equipment"), Ally->GetEquipment()->EquipItem(RocketItem));
    Weapon->WeaponDefinition = nullptr; Weapon->SyncArchetypesToEquipment(); Weapon->EquipSlot(1); Fire();
    ABreakerRocketProjectile* Rocket = nullptr;
    for (TActorIterator<ABreakerRocketProjectile> It(World); It; ++It) if (!It->HasExploded()) { Rocket = *It; break; }
    if (!TestNotNull(TEXT("actual attuned projectile launch"), Rocket)) return false;
    if (auto* Movement = Rocket->FindComponentByClass<UProjectileMovementComponent>()) { Movement->StopMovementImmediately(); Movement->Deactivate(); }
    Rocket->SetActorEnableCollision(false);
    Other->GetAbilitySystemComponent()->CancelAbilityHandle(Metro);
    TestEqual(TEXT("rank two cancellation refuses a tail"), Conversion(Ally), 0.0f);
    const float BeforeRocket = Status->GetEntropyBuildup(); Rocket->Explode(Target->GetActorLocation());
    TestTrue(TEXT("projectile keeps fire-time attunement after cancellation"), Status->GetEntropyBuildup() > BeforeRocket);

    CastBuff(Other, UBreakerAbility_Metronome::StaticClass());
    auto* AllyState = Ally->FindComponentByClass<UBreakerAbilityStateComponent>();
    const float WindowDuration = GetDefault<UBreakerAbility_Metronome>()->GetAbilityDefinition()->WindowDuration;
    AllyState->ExtendWindow(UBreakerAbility_Metronome::WindowKey(), 1.0f);
    Advance(FMath::CeilToInt((WindowDuration + .5f) / .05f));
    TestEqual(TEXT("extended real buff retains attunement beyond original expiry"), Conversion(Ally), 1.0f);
    Advance(15);
    TestFalse(TEXT("actual extended Metronome has expired"), AllyState->IsWindowActive(UBreakerAbility_Metronome::WindowKey()));
    TestEqual(TEXT("natural expiry starts rank two tail"), Conversion(Ally), 1.0f);
    Advance(45);
    TestEqual(TEXT("natural expiry tail ends without a new cast"), Conversion(Ally), 0.0f);

    CastBuff(Other, UBreakerAbility_Cadence::StaticClass());
    FText Failure;
    TestTrue(TEXT("real Forge respec"), Other->GetProgression()->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Failure));
    TestEqual(TEXT("respec immediately refuses stale conversion"), Conversion(Ally), 0.0f);
    if (!Purchase(Other, 2)) return false;
    CastBuff(Other, UBreakerAbility_Metronome::StaticClass());
    FBreakerDamageRequest Lethal; Lethal.BaseDamage = 100000; Lethal.bCanCritical = false;
    Other->GetCombat()->ReceiveDamage(Lethal);
    TestTrue(TEXT("real source death"), Other->GetCombat()->IsDead());
    TestEqual(TEXT("source death clears allied conversion immediately"), Conversion(Ally), 0.0f);
    Other->GetCombat()->RestoreVitals();
    TestEqual(TEXT("source revive does not resurrect old conversion"), Conversion(Ally), 0.0f);
    CastBuff(Source, UBreakerAbility_Cadence::StaticClass());
    TestEqual(TEXT("fresh living source can buff revived party"), Conversion(Ally), 1.0f);
    Ally->GetCombat()->ReceiveDamage(Lethal);
    TestTrue(TEXT("real recipient death"), Ally->GetCombat()->IsDead());
    TestEqual(TEXT("recipient death removes incoming conversion"), Conversion(Ally), 0.0f);
    Ally->GetCombat()->RestoreVitals();
    TestEqual(TEXT("revival alone cannot resurrect the old incoming lease"), Conversion(Ally), 0.0f);
    return true;
}
#endif
