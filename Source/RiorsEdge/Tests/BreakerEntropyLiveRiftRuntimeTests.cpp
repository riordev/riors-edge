#include "Misc/AutomationTest.h"
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
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerZoneBuilder.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerAccountSave.h"
#include "UI/BreakerPlaytestHUD.h"
#include "UObject/Package.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "AI/BreakerNavBounds.h"
#include "NavigationSystem.h"
#include "NavigationData.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEntropyLiveRiftRuntimeTest, "RiorsEdge.Combat.EntropyLiveRiftRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerEntropyLiveRiftRuntimeTest::RunTest(const FString& Parameters)
{
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true; UBreakerAccountSave::InjectForTesting(Account);
    ON_SCOPE_EXIT { UBreakerAccountSave::ResetCacheForTesting(); };
    // A real ordinary roll, with its authored conversion tier/value preserved.
    // Other affixes are neutralized identically in both diagnostic rows.
    FBreakerItemInstance Rifle;
    const FName ConversionId(TEXT("Weapon.EntropyConversion"));
    bool bFound = false;
    for (int32 Seed = 1; Seed <= 16384; ++Seed)
    {
        Rifle = UBreakerLootLibrary::RollItem(TEXT("Entropy.Runtime"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
        if (Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle
            && Rifle.Affixes.ContainsByPredicate([&](const auto& Affix) { return Affix.AffixId == ConversionId; }))
        { bFound = true; break; }
    }
    if (!TestTrue(TEXT("legal level-one converted rifle actually rolls"), bFound)) return false;
    for (auto& Affix : Rifle.Affixes) if (Affix.AffixId != ConversionId) Affix.Value = 0;
    // Preserve both Entropy rows and include the guaranteed unmodified kit.
    // A converted drop is attainable, but is not guaranteed before entry.
    for (int32 Row = 0; Row < 3; ++Row)
    {
        const bool bAbility = Row == 1;
        const bool bStarter = Row == 2;
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(true).CreateAISystem(true);
        UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/EntropyLive_%s/Lvl_Fernhall"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        Package->SetFlags(RF_Transient);
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("Lvl_Fernhall")), Package, true, ERHIFeatureLevel::Num, &Init);
        if (!World) return false;
        auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
        const uint64 InitialFrame = GFrameCounter;
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
        auto* Session = NewObject<UBreakerGameInstance>(); World->SetGameInstance(Session); Context.OwningGameInstance = Session;
        Session->PendingRift = UBreakerZoneBuilder::FernhallRiftFor(NAME_None);
        World->GetWorldSettings()->DefaultGameMode = ABreakerGameMode::StaticClass();
        if (!World->SetGameMode(FURL())) return false;
        World->InitializeActorsForPlay(FURL());
        auto* Mode = World->GetAuthGameMode<ABreakerGameMode>();
        if (!Mode) return false;
        Mode->DispatchBeginPlay();
        auto* Player = World->SpawnActor<ABreakerCharacter>();
        auto* Controller = World->SpawnActor<APlayerController>();
        if (!Player || !Controller) return false;
        Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->SetAsLocalPlayerController();
        Controller->Possess(Player); Controller->SetViewTarget(Player);
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        Player->GetEquipment()->BindAttributes(Player->GetAttributes());
        if (!Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster)
            || !Player->GetEquipment()->EquipItem(bStarter ? UBreakerEquipmentComponent::MakeStarterRifle() : Rifle)) return false;
        auto* Weapon = Player->GetWeapon();
        Weapon->RegisterAllComponentTickFunctions(true); Weapon->SetComponentTickEnabled(true); Weapon->BeginPlay();
        Weapon->SyncArchetypesToEquipment(); Weapon->EquipSlot(1);
        auto* Mana = Player->GetMana();
        Mana->RegisterAllComponentTickFunctions(true); Mana->SetComponentTickEnabled(true); Mana->BeginPlay();
        Mana->BindAttributes(Player->GetAttributes());
        // Only the native components begin. Character BeginPlay owns persistent
        // save loading and must never run in this isolated diagnostic.
        auto* Movement = Player->GetBreakerMovement();
        Movement->RegisterAllComponentTickFunctions(true); Movement->SetComponentTickEnabled(true); Movement->BeginPlay();
        Player->GetCombat()->RegisterAllComponentTickFunctions(true);
        Player->GetCombat()->BeginPlay();
        auto* PlayerStatus = Player->FindComponentByClass<UBreakerStatusComponent>();
        if (!PlayerStatus) return false;
        PlayerStatus->RegisterAllComponentTickFunctions(true);
        PlayerStatus->SetComponentTickEnabled(true);
        PlayerStatus->BeginPlay();
        TestTrue(TEXT("incoming player statuses tick through the real world"), PlayerStatus->PrimaryComponentTick.IsTickFunctionRegistered());
        Mode->HandleStartingNewPlayer_Implementation(Controller);
        if (!TestTrue(TEXT("production startup enters first Rift wave"), Mode->IsRiftInstance() && Mode->GetWaveEnemiesAlive() > 0)) return false;
        TArray<TWeakObjectPtr<ABreakerEnemy>> Enemies;
        TMap<TWeakObjectPtr<ABreakerEnemy>, FVector> Starts;
        float InitialEnemyHealth = 0;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            Enemies.Add(*It); Starts.Add(*It, It->GetActorLocation());
            if (!It->HasActorBegunPlay()) It->DispatchBeginPlay();
            It->RegisterAllActorTickFunctions(true, true);
            const auto* Attributes = It->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>();
            if (Attributes) InitialEnemyHealth += Attributes->GetHealth();
        }
        // This is a live-threat test in shipped obstructed geometry. Supply
        // the production navigation system; failed-path wall pushing must
        // never be what brings its enemies into attack range. Complete the
        // initial build before advancing simulation faster than wall time.
        auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        if (!TestNotNull(TEXT("live encounter has navigation"), Nav)) return false;
        BreakerNavBounds::EnsureCoverage(World);
        Nav->OnWorldInitDone(FNavigationSystemRunMode::GameMode);
        Nav->Build();
        auto* NavData = Nav->GetDefaultNavDataInstance(FNavigationSystem::Create);
        if (!TestNotNull(TEXT("live encounter has built navigation data"), NavData)) return false;
        NavData->EnsureBuildCompletion();
        auto* HUD = World->SpawnActor<ABreakerPlaytestHUD>();
        if (!HUD) return false;
        FScriptDelegate Feed; Feed.BindUFunction(HUD, TEXT("HandlePlayerHitDealt"));
        Player->GetCombat()->OnHitDealt.Add(Feed);
        const auto Fracture = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
        const auto Rot = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Rot::StaticClass(), 1));
        const FGameplayTag RotTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
        const float InitialMana = Mana->GetMana(), InitialHealth = Player->GetAttributes()->GetHealth();
        auto AmmoStock = [&] { return Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo(); };
        int32 Rounds = 0;
        auto PressFire = [&]
        {
            const int32 Before = AmmoStock();
            Weapon->StartFire();
            Rounds += FMath::Max(0, Before - AmmoStock());
        };
        int32 Casts = 0, RotCasts = 0, Reloads = 0, MovedEnemies = 0, Killed = 0;
        float MaxBuildup = 0, PeakRotNumber = 0, Elapsed = 0, LostPlayerHealth = 0;
        bool bRotActivated = false, bAttackObserved = false, bWasReloading = false, bRifleStarted = false;
        TSet<TWeakObjectPtr<ABreakerEnemy>> Moved;
        TSet<FString> ObservedEnemyStates;
        TSet<TWeakObjectPtr<AActor>> BegunDelivery;
        for (int32 Step = 0; Step < 1200; ++Step)
        {
            ABreakerEnemy* Target = nullptr;
            float Best = MAX_flt;
            int32 Alive = 0;
            for (const auto& Held : Enemies)
                if (auto* Enemy = Held.Get(); Enemy && !Enemy->IsDeadEnemy())
                {
                    ++Alive;
                    const float Distance = FVector::DistSquared2D(Player->GetActorLocation(), Enemy->GetActorLocation());
                    if (Distance < Best) { Best = Distance; Target = Enemy; }
                }
            if (Alive == 0 || Player->GetCombat()->IsDead()) break;
            if (!Target) break;
            FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye, Facing);
            Controller->SetControlRotation((Target->GetActorLocation() + FVector(0, 0, 25) - Eye).Rotation());
            if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(.05f);
            // Walk the actual arrival route into detection/ability range. This
            // baseline driver neither teleports nor grants defensive evasion.
            if (Best > FMath::Square(1400.0f)) Player->AddMovementInput((Target->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D());
            if (Best <= FMath::Square(2000.0f) && Step >= 30)
            {
                if (bAbility)
                {
                    if ((RotCasts == 0 || Step % 120 == 0) && ASC->TryActivateAbility(Rot)) ++RotCasts;
                    if (Step % 4 == 0 && ASC->TryActivateAbility(Fracture)) ++Casts;
                }
                else if (!bRifleStarted && !Weapon->IsReloading()) { PressFire(); bRifleStarted = true; }
            }
            // Safely begin newly spawned native delivery, including ENEMY
            // projectiles. Omitting them would fake an encounter with no threat.
            TArray<AActor*> Delivery;
            for (TActorIterator<AActor> It(World); It; ++It)
                if ((It->IsA<ABreakerProjectileBase>() || It->IsA<ABreakerZoneActor>()) && !BegunDelivery.Contains(*It)) Delivery.Add(*It);
            for (AActor* Actor : Delivery)
            {
                BegunDelivery.Add(Actor);
                if (!Actor->HasActorBegunPlay()) Actor->DispatchBeginPlay();
                Actor->RegisterAllActorTickFunctions(true, true);
            }
            const float BeforeHealth = Player->GetAttributes()->GetHealth();
            // Initial equip can finish during the live arrival and add its
            // ordinary starting magazine. Count actual stock debits around
            // fire/timer delivery; a starting fill is not negative shots.
            // Reload merely transfers reserve into magazine, conserving stock.
            const int32 AmmoBeforeTick = AmmoStock();
            ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); Elapsed += .05f;
            Rounds += FMath::Max(0, AmmoBeforeTick - AmmoStock());
            LostPlayerHealth += FMath::Max(0.0f, BeforeHealth - Player->GetAttributes()->GetHealth());
            const bool bReloading = Weapon->IsReloading();
            if (bReloading && !bWasReloading) ++Reloads;
            if (bWasReloading && !bReloading && !bAbility) { PressFire(); bRifleStarted = true; }
            bWasReloading = bReloading;
            for (const auto& Held : Enemies)
                if (auto* Enemy = Held.Get())
                {
                    if (FVector::Dist2D(Starts[Held], Enemy->GetActorLocation()) > 50) Moved.Add(Held);
                    const FString State = Enemy->GetEnemyStateLabel();
                    ObservedEnemyStates.Add(State);
                    // The shipped melee tell is spelled WIND-UP.
                    bAttackObserved |= State == TEXT("WIND-UP") || State.Contains(TEXT("LUNGE")) || State.Contains(TEXT("FIRE")) || State.Contains(TEXT("ATTACK"));
                    if (auto* Status = Enemy->FindComponentByClass<UBreakerStatusComponent>())
                    { MaxBuildup = FMath::Max(MaxBuildup, Status->GetEntropyBuildup()); bRotActivated |= Status->HasStatus(RotTag); }
                }
            for (const auto& Number : HUD->GetDamageNumbers())
                if (Number.bFromDoT && Number.Element == EBreakerElement::Entropy && Number.DamageTypeTag == RotTag)
                    PeakRotNumber = FMath::Max(PeakRotNumber, Number.Value);
        }
        Weapon->StopFire(); Player->GetCombat()->OnHitDealt.Remove(Feed);
        float RemainingEnemyHealth = 0;
        for (const auto& Held : Enemies)
            if (auto* Enemy = Held.Get())
            {
                Killed += Enemy->IsDeadEnemy() ? 1 : 0;
                if (const auto* Attributes = Enemy->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>()) RemainingEnemyHealth += Attributes->GetHealth();
            }
            else ++Killed;
        MovedEnemies = Moved.Num();
        auto StateNames = ObservedEnemyStates.Array(); StateNames.Sort();
        AddInfo(FString::Printf(TEXT("Observed enemy states: %s"), *FString::Join(StateNames, TEXT(", "))));
        TestTrue(TEXT("shipped enemies actually move in the encounter"), MovedEnemies > 0);
        TestTrue(TEXT("real attacks or received damage demonstrate live enemy threat"), bAttackObserved || LostPlayerHealth > 0);
        TestTrue(TEXT("ordinary delivery reduces actual enemy health"), InitialEnemyHealth > RemainingEnemyHealth);
        if (!bStarter) TestTrue(TEXT("Entropy delivery earns buildup or threshold activation"), MaxBuildup > 0 || bRotActivated || PeakRotNumber > 0);
        if (bAbility)
        {
            TestTrue(TEXT("actual ability resource gates admit paid casts"), Casts > 0);
            TestEqual(TEXT("ability-only row spends no weapon ammunition"), Rounds, 0);
        }
        else TestTrue(TEXT("actual converted rifle consumes ammunition"), Rounds > 0);
        AddInfo(FString::Printf(TEXT("LIVE ENTROPY RIFT %s area5 ilvl1 seconds%.2f kills%d/%d damage%.2f playerHealth%.2f->%.2f incomingHealthDamage%.2f moved%d attackObserved%d mana%.2f->%.2f fracture%d rot%d rounds%d reloads%d maxBuildup%.2f rotActivated%d peakObservedRotNumber%.2f outcome%s; ordinary starting resources, normal enemy AI, no forced clear or parity claim"),
            bAbility ? TEXT("Fracture+Rot") : bStarter ? TEXT("starterRifle") : TEXT("convertedRifle"), Elapsed, Killed, Enemies.Num(), InitialEnemyHealth - RemainingEnemyHealth,
            InitialHealth, Player->GetAttributes()->GetHealth(), LostPlayerHealth, MovedEnemies, bAttackObserved,
            InitialMana, Mana->GetMana(), Casts, RotCasts, Rounds, Reloads, MaxBuildup, bRotActivated, PeakRotNumber,
            Player->GetCombat()->IsDead() ? TEXT("playerDead") : Killed == Enemies.Num() ? TEXT("firstWaveCleared") : TEXT("timeLimit")));
    }
    return true;
}
#endif
