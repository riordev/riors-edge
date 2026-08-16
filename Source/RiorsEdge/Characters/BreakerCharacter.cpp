#include "Characters/BreakerCharacter.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CollisionShape.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Input/BreakerInputConfig.h"
#include "Settings/BreakerGameSettings.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Playtest/BreakerPlaytestComponent.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Classes/BreakerScrapComponent.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerChargeComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Save/BreakerSaveGame.h"
#include "Save/BreakerCharacterRoster.h"
#include "Game/BreakerGameInstance.h"
#include "Save/BreakerQuestJournal.h"
#include "Save/BreakerQuestContent.h"
#include "Combat/BreakerEnemy.h"
#include "Items/BreakerLootLibrary.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Items/BreakerLootPickup.h"
#include "Game/BreakerGameMode.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Game/BreakerGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/ConfigCacheIni.h"
#include "InputCoreTypes.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/KismetSystemLibrary.h"
// Explicit rather than transitive: this file uses GEngine, the world timer
// manager, ULocalPlayer's subsystem accessor and Slate's FOnClicked directly,
// and used to receive all four through other headers' include chains — which
// other passes are free to trim.
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"
#include "Framework/SlateDelegates.h"
#include "UI/BreakerMenu.h"

ABreakerCharacter::ABreakerCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UBreakerCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;
    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(-10.0, 0.0, 64.0));
    FirstPersonCamera->bUsePawnControlRotation = true;
    // O25 base kit. This is the authored default; at runtime
    // UBreakerCharacterMovementComponent::RefreshJumpGrant owns the budget,
    // because the third jump is class- and level-gated and has to survive a
    // mid-session class change. Two remains correct for every class.
    JumpMaxCount = 2;
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    Attributes = CreateDefaultSubobject<UBreakerAttributeSet>(TEXT("Attributes"));
    Progression = CreateDefaultSubobject<UBreakerProgressionComponent>(TEXT("Progression"));
    Combat = CreateDefaultSubobject<UBreakerCombatComponent>(TEXT("Combat"));
    Weapon = CreateDefaultSubobject<UBreakerWeaponComponent>(TEXT("Weapon"));
    Playtest = CreateDefaultSubobject<UBreakerPlaytestComponent>(TEXT("Playtest"));
    Equipment = CreateDefaultSubobject<UBreakerEquipmentComponent>(TEXT("Equipment"));
    Momentum = CreateDefaultSubobject<UBreakerMomentumComponent>(TEXT("Momentum"));
    Mana = CreateDefaultSubobject<UBreakerManaComponent>(TEXT("Mana"));
    // The other three loops, the same attach pattern: every pawn carries all
    // five and each gates itself on the permanent class internally.
    Scrap = CreateDefaultSubobject<UBreakerScrapComponent>(TEXT("Scrap"));
    Grit = CreateDefaultSubobject<UBreakerGritComponent>(TEXT("Grit"));
    Charge = CreateDefaultSubobject<UBreakerChargeComponent>(TEXT("Charge"));
    Abilities = CreateDefaultSubobject<UBreakerAbilityComponent>(TEXT("Abilities"));
    Quests = CreateDefaultSubobject<UBreakerQuestJournal>(TEXT("QuestJournal"));

    // --- The first-person blockout --------------------------------------
    // Composed engine primitives plus dynamic material instances, exactly the
    // technique the gym dressing uses, so a clean clone still plays with no
    // content. The layout table and the reasoning behind every proportion live
    // in Characters/BreakerViewmodelRig.{h,cpp}; this constructor only
    // ALLOCATES the pool. Shapes, sizes and colours are assigned at runtime by
    // RebuildViewmodelParts, because they change with the equipped archetype.
    PrototypeWeaponVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeWeaponVisual"));
    PrototypeWeaponVisual->SetupAttachment(FirstPersonCamera);
    PrototypeWeaponVisual->SetRelativeLocation(FVector(26.0f, 13.0f, -16.0f));
    PrototypeWeaponVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Deliberately MESHLESS. This is the transform the recoil spring drives;
    // giving it geometry is what made the old proxy a single grey slab.
    PrototypeWeaponVisual->SetOnlyOwnerSee(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

    auto MakeProxyPart = [this](const TCHAR* Name) -> UStaticMeshComponent*
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(PrototypeWeaponVisual);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetOnlyOwnerSee(true);
        // A viewmodel that casts world shadows draws a gun-shaped shadow on the
        // floor from a gun nobody else can see.
        Part->SetCastShadow(false);
        Part->SetVisibility(false);
        return Part;
    };

    // Slots 0 and 1 keep their historic names so BP_BreakerCharacter's
    // inherited-component records still resolve; they are ordinary pool slots.
    PrototypeWeaponBarrel = MakeProxyPart(TEXT("PrototypeWeaponBarrel"));
    PrototypeWeaponSight = MakeProxyPart(TEXT("PrototypeWeaponSight"));
    ViewmodelParts.Add(PrototypeWeaponBarrel);
    ViewmodelParts.Add(PrototypeWeaponSight);
    for (int32 Index = ViewmodelParts.Num(); Index < BreakerViewmodel::MaxProxyParts; ++Index)
    {
        ViewmodelParts.Add(MakeProxyPart(*FString::Printf(TEXT("ViewmodelPart_%02d"), Index)));
    }

    PrototypeMuzzleFlash = CreateDefaultSubobject<UPointLightComponent>(TEXT("PrototypeMuzzleFlash"));
    // Hung off the RIG rather than the camera: a flash nailed to the screen
    // does not move when the gun kicks, which is half the reason the recoil was
    // hard to read.
    PrototypeMuzzleFlash->SetupAttachment(PrototypeWeaponVisual);
    PrototypeMuzzleFlash->SetRelativeLocation(FVector(60.0f, 0.0f, 2.5f));
    PrototypeMuzzleFlash->SetLightColor(FLinearColor(1.0f, 0.35f, 0.05f));
    PrototypeMuzzleFlash->SetIntensity(0.0f);
    PrototypeMuzzleFlash->SetAttenuationRadius(220.0f);

    // Arms. Parented to the rig, not the camera, because the hands hold the
    // gun; that is also what makes the recoil read as the whole assembly
    // moving rather than a stick sliding past two static blocks.
    UStaticMesh* const CubeAsset = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
    auto MakeLimbPart = [this, CubeAsset](const TCHAR* Name) -> UStaticMeshComponent*
    {
        UStaticMeshComponent* Limb = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Limb->SetupAttachment(PrototypeWeaponVisual);
        Limb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Limb->SetOnlyOwnerSee(true);
        Limb->SetCastShadow(false);
        if (CubeAsset) Limb->SetStaticMesh(CubeAsset);
        return Limb;
    };
    LeftArmVisual = MakeLimbPart(TEXT("LeftArmVisual"));
    RightArmVisual = MakeLimbPart(TEXT("RightArmVisual"));
    LeftGloveVisual = MakeLimbPart(TEXT("LeftGloveVisual"));
    RightGloveVisual = MakeLimbPart(TEXT("RightGloveVisual"));
}

UAbilitySystemComponent* ABreakerCharacter::GetAbilitySystemComponent() const { return AbilitySystem; }

void ABreakerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // Backstop for every path that changes the equipped gun without going
    // through the 1/2 keys — the loadout screen, an item equip, a dev swap.
    // Early-outs on the first line when nothing changed.
    ApplyWeaponPresentation();
    UpdateViewmodelKick();
    UpdateDashCameraFeedback(DeltaSeconds);
    // Coarse poll for the Grit/Charge discrete state inputs (in-combat,
    // enemy-within-5m). Cheap: early-outs unless one of those loops is live.
    if (HasAuthority())
    {
        ClassResourcePollElapsed += DeltaSeconds;
        if (ClassResourcePollElapsed >= ClassResourcePollInterval)
        {
            ClassResourcePollElapsed = 0.0f;
            UpdateClassResourceStates();
        }
    }
    // Fall-out-of-map recovery: the template level has no kill volume, so
    // enforce our own floor relative to the spawn point.
    if (HasAuthority() && GetActorLocation().Z < FallKillZ)
    {
        ResetPlaytest();
        return;
    }
    if (!bMantling) return;

    MantleElapsed += DeltaSeconds;
    const float Alpha = FMath::Clamp(MantleElapsed / MantleDuration, 0.0f, 1.0f);
    const float SmoothedAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
    FHitResult MoveHit;
    SetActorLocation(FMath::Lerp(MantleStart, MantleTarget, SmoothedAlpha), true, &MoveHit, ETeleportType::None);
    if (MoveHit.bBlockingHit || Alpha >= 1.0f)
    {
        bMantling = false;
        if (UBreakerCharacterMovementComponent* Movement = GetBreakerMovement())
        {
            Movement->SetMovementMode(MOVE_Falling);
            Movement->Velocity = MoveHit.bBlockingHit ? FVector::ZeroVector : MantleExitVelocity;
        }
    }
}

void ABreakerCharacter::BeginPlay()
{
    Super::BeginPlay();
    // BEFORE anything reads a save. A level load destroyed the pawn that knew
    // which character was being played, so this one asks the session first —
    // otherwise every load after the first would read the legacy single slot.
    AdoptSessionCharacter();
    // Holstered for the whole life of an Anchor pawn (see IsWeaponsHolstered).
    bWeaponsHolstered = UBreakerGameInstance::IsAnchorMap(this);
    AbilitySystem->InitAbilityActorInfo(this, this);
    if (Weapon) Weapon->OnShot.AddDynamic(this, &ThisClass::HandleShotCosmetics);
    if (Combat) Combat->OnDeath.AddDynamic(this, &ThisClass::HandlePlayerDeath);
    // --- Class-resource wiring (T7 step 2) -----------------------------
    // Every resource loop's Notify* entry point gets its one real caller
    // here. Server-side facts only, so the bindings are authority-gated.
    if (HasAuthority())
    {
        if (Combat)
        {
            // Scrap: kills at the killing instance's coefficient (§1.1).
            Combat->OnKillDealt.AddDynamic(this, &ThisClass::HandleClassResourceKill);
            // Grit: post-mitigation damage taken (health/shield split), the
            // passive block proc, and the self-inflicted flag off Instigator.
            Combat->OnDamageTaken.AddDynamic(this, &ThisClass::HandleClassResourceDamageTaken);
            // Charge: healing/shielding done, effective/overheal split.
            Combat->OnHealingDealt.AddDynamic(this, &ThisClass::HandleClassResourceHealingDealt);
            // The in-combat derivation needs "recently dealt a hit" too.
            Combat->OnHitDealt.AddDynamic(this, &ThisClass::HandleClassResourceHitDealt);
        }
        if (Weapon)
        {
            // Scrap: completed reloads and full-magazine dumps, each carrying
            // its own anti-farm clause as the parameter.
            Weapon->OnReloadCompleted.AddDynamic(this, &ThisClass::HandleClassResourceReloadCompleted);
            Weapon->OnMagazineEmptied.AddDynamic(this, &ThisClass::HandleClassResourceMagazineEmptied);
        }
    }
    PlaytestSpawnTransform = GetActorTransform();
    FallKillZ = PlaytestSpawnTransform.GetLocation().Z - 4000.0f;
    // A weapon ITEM decides which gun its slot holds. Bound before the save
    // loads so a restored loadout arms the right archetypes, and called once
    // directly afterwards because the equipment component does not broadcast
    // for state it was constructed with.
    if (Weapon && Equipment)
    {
        Equipment->OnEquipmentChanged.AddDynamic(Weapon, &UBreakerWeaponComponent::SyncArchetypesToEquipment);
    }
    // Write-through persistence for quest state. The journal decides WHEN its
    // state must reach disk (only on a real change); the character owns the
    // slot and so is the only thing that can honour the request. Bound before
    // the load so a migration performed during LoadGameState is itself durable.
    if (Quests && HasAuthority())
    {
        Quests->OnPersistRequested.AddWeakLambda(this, [this]() { SaveGameState(); });
        Quests->OnFlagSet.AddWeakLambda(this, [this](FName Flag) { GrantQuestRewardForFlag(Flag); });
        if (Combat) Combat->OnKillDealt.AddDynamic(this, &ThisClass::HandleQuestKill);
    }
    if (HasAuthority()) LoadGameState();
    if (Weapon && Equipment && HasAuthority()) Weapon->SyncArchetypesToEquipment();
    // Build the blockout for whatever the save restored. Before this the proxy
    // was only ever rebuilt on a 1/2 keypress, so a fresh session showed the
    // constructor's rifle no matter what was actually equipped.
    ApplyWeaponPresentation();
    StartViewmodelCaptureCycle();
    float SavedFOV = 90.0f;
    GConfig->GetFloat(TEXT("RiorsEdge.Playtest"), TEXT("FOV"), SavedFOV, GGameUserSettingsIni);
    GConfig->GetFloat(TEXT("RiorsEdge.Playtest"), TEXT("Sensitivity"), LookSensitivity, GGameUserSettingsIni);
    GConfig->GetBool(TEXT("RiorsEdge.Playtest"), TEXT("InvertLookY"), bInvertLookY, GGameUserSettingsIni);
    // D27: ONE clamp for sensitivity, owned by the settings model — every
    // reader/writer of this ini key goes through it (see the -/= nudges and
    // ApplyMenuSettings below).
    LookSensitivity = UBreakerGameSettingsLibrary::ClampMouseSensitivity(LookSensitivity);
    BaseFieldOfView = FMath::Clamp(SavedFOV, 70.0f, 120.0f);
    ApplyBaseFieldOfView();
    // Presentation binds to the movement rule, never the other way round: the
    // component broadcasts that a dash happened and this class decides what the
    // camera does about it — the same split OnLandingImpact already uses.
    if (UBreakerCharacterMovementComponent* Movement = GetBreakerMovement())
    {
        Movement->OnDashStarted.AddDynamic(this, &ThisClass::HandleDashStarted);
    }
    // R10: the mapping context this pawn registers carries the player's saved
    // keybind overrides, not the untouched default — the settings screen's
    // rebinds are live. BeginPlay re-runs on every map arrival (OpenLevel
    // destroys the pawn), so overrides re-apply on every spawn; the delegate
    // below covers mid-session rebinds from the settings screen. AddUObject
    // binds weakly, so a destroyed pawn's entry is skipped and compacted at
    // the next broadcast — no unsubscribe needed.
    if (IsLocallyControlled() && InputConfig && InputConfig->DefaultMappingContext)
    {
        UBreakerGameSettings* ProfileSettings = NewObject<UBreakerGameSettings>(GetTransientPackage());
        ProfileSettings->LoadOrDefaults();
        ApplyKeybindOverrides(ProfileSettings->KeybindOverrides);
        UBreakerGameSettings::OnKeybindOverridesChanged().AddUObject(this, &ThisClass::ApplyKeybindOverrides);
    }
    // THE TITLE MENU BELONGS TO SESSIONS THAT HAVE NOT ENTERED THE WORLD YET.
    // OpenLevel destroys the pawn, so BeginPlay re-runs on every map arrival —
    // unguarded, this re-opened the title screen (paused) on ARRIVING in the
    // Anchor and again in the gym. The session id is the tell: a mid-session
    // arrival adopted a character two hundred lines up and needs no menu, while
    // the front end always shows it and a PIE drop-in on the template map (no
    // character chosen yet) still gets its character select.
    if (IsLocallyControlled())
    {
        const UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>();
        if (ShouldShowInitialMenu(UBreakerGameInstance::IsFrontEndMap(this),
                Session && Session->ActiveCharacterId.IsValid()))
        {
            GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ShowInitialMenu);
        }
    }
}

void ABreakerCharacter::SaveGameState()
{
    if (!HasAuthority() || !Progression || !Equipment || !Weapon) return;
    UBreakerSaveGame* Save = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Save) return;
    Save->Progression = Progression->GetProgressionState();
    Save->EquippedItems = Equipment->GetEquipped();
    Save->BackpackItems = Equipment->GetBackpack();
    Save->ForgeWallet = Equipment->GetForgeWallet();
    Save->SlotOneArchetype = Weapon->GetSlotArchetype(1);
    Save->SlotTwoArchetype = Weapon->GetSlotArchetype(2);
    if (Quests)
    {
        Save->QuestFlags = Quests->GetState().Flags;
        Save->QuestCounters = Quests->GetState().Counters;
    }
    Save->SaveVersion = UBreakerSaveGame::CurrentSaveVersion;
    UGameplayStatics::SaveGameToSlot(Save, ActiveSaveSlotName(), 0);
}

void ABreakerCharacter::AdoptSessionCharacter()
{
    // Runs on arrival in a new map. The pawn was destroyed by the level load
    // and this is a fresh one, so it has no idea who it is until it asks the
    // session — without this, travelling to the gym would silently load the
    // legacy single slot and play the wrong character.
    if (const UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>())
    {
        if (Session->ActiveCharacterId.IsValid())
        {
            ActiveCharacterId = Session->ActiveCharacterId;
        }
    }
}

FString ABreakerCharacter::ActiveSaveSlotName() const
{
    // The character's own slot once one has been chosen, and the legacy global
    // slot before that. Keeping the fallback is what lets a session that never
    // touches the character screen (a capture run, a PIE drop-in) still load
    // and save exactly as it always did.
    return ActiveCharacterId.IsValid()
        ? UBreakerCharacterRoster::SlotNameForCharacter(ActiveCharacterId)
        : FString(UBreakerSaveGame::DefaultSlotName());
}

void ABreakerCharacter::EnterWorldAsCharacter(const FGuid& CharacterId)
{
    // THE PATH FROM THE CHARACTER SCREEN INTO THE GAME. Before this, PLAY
    // selected a character and then had nowhere to go: LoadGameState was
    // hard-wired to the single legacy slot, so entering the world would have
    // silently played the WRONG character. Order matters here — the id has to
    // be set before the load, or the load reads the old global slot again.
    ActiveCharacterId = CharacterId;

    // The id has to survive the level load, because OpenLevel destroys this
    // pawn. The GameInstance is the only thing that outlives the transition,
    // so it carries the id and the pawn on the other side reads it back.
    if (UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>())
    {
        Session->ActiveCharacterId = CharacterId;
    }

    // FROM THE FRONT END, PLAY TRAVELS. It used to load the save and teleport,
    // because the hub and the gym were both already built in the one map the
    // game had. They are separate maps now, so the hub does not exist yet and
    // there is nothing to teleport to — this is a level load.
    if (UBreakerGameInstance::IsFrontEndMap(this))
    {
        UBreakerGameInstance::TravelTo(this, FName(UBreakerGameInstance::AnchorMapName()));
        return;
    }

    // Already in a world (a PIE drop-in, or the old single-map path): load and
    // resume exactly as before, so nothing that worked yesterday stops.
    LoadGameState();
    ResumeFromMenu();

    // Into the hub, not the gym. The hub is the persistent place a session
    // starts from (the owner's reference is Destiny's Tower / a PoE hideout);
    // the gym is one destination reachable from the hub's travel point.
    if (ABreakerGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr)
    {
        Mode->TeleportPawnToHub(this);
    }
}

void ABreakerCharacter::LoadGameState()
{
    if (!HasAuthority() || !Progression || !Equipment || !Weapon) return;
    UBreakerSaveGame* Save = Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromSlot(ActiveSaveSlotName(), 0));
    if (!Save) return;
    // Migrate BEFORE reading anything out of the payload. A file written by an
    // older build is not wrong, it is old; reading it with today's assumptions
    // is what silently misinterprets it.
    FString MigrationNote;
    if (!UBreakerSaveGame::MigrateToCurrent(*Save, MigrationNote))
    {
        // Refuse-to-load, per Save-Architecture 5.2: a file from a NEWER build
        // is not opened, not repaired, and not overwritten. Leaving the
        // character at defaults is recoverable; overwriting is not.
        UE_LOG(LogTemp, Error, TEXT("BreakerSave: refusing to load — %s"), *MigrationNote);
        return;
    }
    if (!MigrationNote.IsEmpty()) UE_LOG(LogTemp, Log, TEXT("BreakerSave: %s"), *MigrationNote);
    Progression->LoadProgressionState(Save->Progression);
    Equipment->RestoreState(Save->EquippedItems, Save->BackpackItems);
    Equipment->RestoreForgeWallet(Save->ForgeWallet);
    Weapon->SetSlotArchetype(1, Save->SlotOneArchetype);
    Weapon->SetSlotArchetype(2, Save->SlotTwoArchetype);
    if (Quests) Quests->RestoreFrom(Save->QuestFlags, Save->QuestCounters);
}

void ABreakerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SaveGameState();
    if (MenuWidget.IsValid() && GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
        MenuWidget.Reset();
    }
    Super::EndPlay(EndPlayReason);
}

void ABreakerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ThisClass::EquipPrimaryWeapon);
    PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ThisClass::EquipSecondaryWeapon);
    PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::TogglePauseMenu).bExecuteWhenPaused = true;
    // THE SAME PATH ESCAPE USES, and for the same reason. The title gate was
    // built on Slate keyboard focus (OnKeyDown, then OnPreviewKeyDown) and did
    // not fire in a standalone session: under FInputModeGameAndUI the menu
    // widget does not reliably hold keyboard focus, so no key event ever
    // reached it. Escape worked the whole time because it is bound here, with
    // bExecuteWhenPaused - the menu is open while the game is PAUSED, and an
    // input binding without that flag is dead in exactly that state.
    PlayerInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ThisClass::ConfirmMenuKey).bExecuteWhenPaused = true;
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ThisClass::ConfirmMenuKey).bExecuteWhenPaused = true;
    // KEY CAPTURE FOR REBINDING, on the path that is PROVEN to work while
    // paused. The settings screen also listens through Slate's preview chain,
    // and that may well be enough — but the title gate failed twice tonight on
    // exactly that assumption, and a rebind row that silently never captures
    // is indistinguishable from a frozen menu. AnyKey is safe here because
    // SBreakerMenu::HandleRebindKey is inert unless a row is actually
    // listening, and it treats Escape as cancel rather than as a binding.
    PlayerInputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &ThisClass::MenuRebindKey).bExecuteWhenPaused = true;
    PlayerInputComponent->BindKey(EKeys::I, IE_Pressed, this, &ThisClass::ToggleInventoryMenu).bExecuteWhenPaused = true;
    PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &ThisClass::InteractWithNearbyNPC);
    PlayerInputComponent->BindKey(EKeys::F4, IE_Pressed, this, &ThisClass::StartWave);
    // Raw-key ability fallbacks so the slice is playable before DA_PlayerInputConfig
    // gains ability actions. Q is dash, R is reload, C is slide, so abilities take
    // E / T / G. The Enhanced Input actions below override these once authored.
    PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &ThisClass::ActivateAbilityOne);
    PlayerInputComponent->BindKey(EKeys::T, IE_Pressed, this, &ThisClass::ActivateAbilityTwo);
    PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &ThisClass::ActivateUltimate);

    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!InputConfig || !Input)
    {
        PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ThisClass::MoveForwardLegacy);
        PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ThisClass::MoveRightLegacy);
        PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ThisClass::TurnLegacy);
        PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &ThisClass::LookUpLegacy);
        PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ThisClass::HandleJumpInput);
        PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
        PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &ThisClass::StartSprint);
        PlayerInputComponent->BindAction(TEXT("Dash"), IE_Pressed, this, &ThisClass::HandleDashInput);
        PlayerInputComponent->BindAction(TEXT("Slide"), IE_Pressed, this, &ThisClass::StartSlide);
        PlayerInputComponent->BindAction(TEXT("Slide"), IE_Released, this, &ThisClass::StopSlide);
        PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &ThisClass::StartFire);
        PlayerInputComponent->BindAction(TEXT("Fire"), IE_Released, this, &ThisClass::StopFire);
        PlayerInputComponent->BindAction(TEXT("Aim"), IE_Pressed, this, &ThisClass::StartAim);
        PlayerInputComponent->BindAction(TEXT("Aim"), IE_Released, this, &ThisClass::StopAim);
        PlayerInputComponent->BindAction(TEXT("Reload"), IE_Pressed, this, &ThisClass::HandleReloadInput);
        PlayerInputComponent->BindAction(TEXT("PlaytestReset"), IE_Pressed, this, &ThisClass::ResetPlaytest);
        PlayerInputComponent->BindAction(TEXT("PlaytestReport"), IE_Pressed, this, &ThisClass::CopyPlaytestReport);
        PlayerInputComponent->BindAction(TEXT("PlaytestDiagnostics"), IE_Pressed, this, &ThisClass::TogglePlaytestDiagnostics);
        PlayerInputComponent->BindAction(TEXT("FOVUp"), IE_Pressed, this, &ThisClass::IncreaseFOV);
        PlayerInputComponent->BindAction(TEXT("FOVDown"), IE_Pressed, this, &ThisClass::DecreaseFOV);
        return;
    }
    if (InputConfig->Move) Input->BindAction(InputConfig->Move, ETriggerEvent::Triggered, this, &ThisClass::Move);
    if (InputConfig->Look) Input->BindAction(InputConfig->Look, ETriggerEvent::Triggered, this, &ThisClass::Look);
    if (InputConfig->Jump) {
        Input->BindAction(InputConfig->Jump, ETriggerEvent::Started, this, &ThisClass::HandleJumpInput);
        Input->BindAction(InputConfig->Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
    }
    if (InputConfig->Sprint) {
        Input->BindAction(InputConfig->Sprint, ETriggerEvent::Started, this, &ThisClass::StartSprint);
    }
    if (InputConfig->Dash) Input->BindAction(InputConfig->Dash, ETriggerEvent::Started, this, &ThisClass::HandleDashInput);
    if (InputConfig->Slide) {
        Input->BindAction(InputConfig->Slide, ETriggerEvent::Started, this, &ThisClass::StartSlide);
        Input->BindAction(InputConfig->Slide, ETriggerEvent::Completed, this, &ThisClass::StopSlide);
    }
    if (InputConfig->Fire) {
        Input->BindAction(InputConfig->Fire, ETriggerEvent::Started, this, &ThisClass::StartFire);
        Input->BindAction(InputConfig->Fire, ETriggerEvent::Completed, this, &ThisClass::StopFire);
    }
    if (InputConfig->Aim) {
        Input->BindAction(InputConfig->Aim, ETriggerEvent::Started, this, &ThisClass::StartAim);
        Input->BindAction(InputConfig->Aim, ETriggerEvent::Completed, this, &ThisClass::StopAim);
    }
    if (InputConfig->Reload) Input->BindAction(InputConfig->Reload, ETriggerEvent::Started, this, &ThisClass::HandleReloadInput);
    if (InputConfig->PlaytestReset) Input->BindAction(InputConfig->PlaytestReset, ETriggerEvent::Started, this, &ThisClass::ResetPlaytest);
    if (InputConfig->PlaytestReport) Input->BindAction(InputConfig->PlaytestReport, ETriggerEvent::Started, this, &ThisClass::CopyPlaytestReport);
    if (InputConfig->PlaytestDiagnostics) Input->BindAction(InputConfig->PlaytestDiagnostics, ETriggerEvent::Started, this, &ThisClass::TogglePlaytestDiagnostics);
    if (InputConfig->FOVUp) Input->BindAction(InputConfig->FOVUp, ETriggerEvent::Started, this, &ThisClass::IncreaseFOV);
    if (InputConfig->FOVDown) Input->BindAction(InputConfig->FOVDown, ETriggerEvent::Started, this, &ThisClass::DecreaseFOV);
    if (InputConfig->AbilityOne) Input->BindAction(InputConfig->AbilityOne, ETriggerEvent::Started, this, &ThisClass::ActivateAbilityOne);
    if (InputConfig->AbilityTwo) Input->BindAction(InputConfig->AbilityTwo, ETriggerEvent::Started, this, &ThisClass::ActivateAbilityTwo);
    if (InputConfig->Ultimate) Input->BindAction(InputConfig->Ultimate, ETriggerEvent::Started, this, &ThisClass::ActivateUltimate);
}

void ABreakerCharacter::ApplyKeybindOverrides(const TMap<FName, FKey>& Overrides)
{
    // NOTE ON THE TWO INPUT PATHS: the legacy BindAction/BindAxis block in
    // SetupPlayerInputComponent above runs only when InputConfig or the
    // EnhancedInputComponent is missing. The shipped config has both —
    // DefaultInput.ini sets DefaultInputComponentClass to
    // EnhancedInputComponent and DA_PlayerInputConfig is authored — so the
    // Enhanced path is the live one, and it is the one overrides cover. The
    // legacy path is the "input asset not cooked" fallback, driven by the
    // ini's raw Action/AxisMappings; a build in that state has no default
    // mapping context to rewrite and keeps its ini keys.
    const APlayerController* PC = Cast<APlayerController>(GetController());
    ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
    UEnhancedInputLocalPlayerSubsystem* Subsystem =
        LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    if (!Subsystem || !InputConfig || !InputConfig->DefaultMappingContext)
    {
        return;
    }

    // Out with whatever this pawn registered before — the previous clone on
    // a mid-session rebind, or the plain default. RemoveMappingContext on a
    // context that was never added is a harmless no-op, so both removals can
    // run unconditionally.
    if (ActiveMappingContext)
    {
        Subsystem->RemoveMappingContext(ActiveMappingContext);
    }
    Subsystem->RemoveMappingContext(InputConfig->DefaultMappingContext);

    // A rebuilt clone when any override exists; the authored asset itself
    // when none do (RESET ALL lands here, restoring defaults live). Same
    // priority (0) the default was always registered at.
    UInputMappingContext* Rebuilt =
        UBreakerGameSettingsLibrary::BuildRuntimeMappingContext(InputConfig, Overrides, this);
    ActiveMappingContext = Rebuilt ? Rebuilt : InputConfig->DefaultMappingContext.Get();
    Subsystem->AddMappingContext(ActiveMappingContext, 0);
}

void ABreakerCharacter::ActivateAbilityOne()
{
    if (bWeaponsHolstered) return;   // holstered in the Anchor, same as fire
    if (Abilities) Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne);
}

void ABreakerCharacter::ActivateAbilityTwo()
{
    if (bWeaponsHolstered) return;
    if (Abilities) Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo);
}

void ABreakerCharacter::ActivateUltimate()
{
    if (bWeaponsHolstered) return;
    if (Abilities) Abilities->TryActivateSlot(EBreakerAbilitySlot::Ultimate);
}

void ABreakerCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    const FRotator Yaw(0.0, GetControlRotation().Yaw, 0.0);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void ABreakerCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    AddControllerYawInput(Axis.X * LookSensitivity);
    AddControllerPitchInput(Axis.Y * LookSensitivity * (bInvertLookY ? 1.0f : -1.0f));
}

void ABreakerCharacter::MoveForwardLegacy(float Value)
{
    if (FMath::IsNearlyZero(Value)) return;
    const FRotator Yaw(0.0, GetControlRotation().Yaw, 0.0);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Value);
}

void ABreakerCharacter::MoveRightLegacy(float Value)
{
    if (FMath::IsNearlyZero(Value)) return;
    const FRotator Yaw(0.0, GetControlRotation().Yaw, 0.0);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Value);
}

void ABreakerCharacter::TurnLegacy(float Value) { AddControllerYawInput(Value * LookSensitivity); }
void ABreakerCharacter::LookUpLegacy(float Value) { AddControllerPitchInput(Value * LookSensitivity * (bInvertLookY ? -1.0f : 1.0f)); }

float ABreakerCharacter::GetHorizontalSpeed() const
{
    return GetVelocity().Size2D();
}

UBreakerCharacterMovementComponent* ABreakerCharacter::GetBreakerMovement() const
{
    return Cast<UBreakerCharacterMovementComponent>(GetCharacterMovement());
}

bool ABreakerCharacter::IsSprinting() const
{
    const UBreakerCharacterMovementComponent* Movement = GetBreakerMovement();
    return Movement && Movement->IsSprinting();
}

bool ABreakerCharacter::IsSliding() const
{
    const UBreakerCharacterMovementComponent* Movement = GetBreakerMovement();
    return Movement && Movement->IsSliding();
}

bool ABreakerCharacter::IsWallRiding() const
{
    const UBreakerCharacterMovementComponent* Movement = GetBreakerMovement();
    return Movement && Movement->IsWallRiding();
}

void ABreakerCharacter::StartSprint()
{
    if (UBreakerCharacterMovementComponent* Movement = GetBreakerMovement()) Movement->SetSprinting(!Movement->IsSprinting());
}

void ABreakerCharacter::HandleDashInput() { TryDash(); }

void ABreakerCharacter::HandleJumpInput()
{
    UBreakerCharacterMovementComponent* Movement = GetBreakerMovement();
    if (Movement && Movement->TryWallJump())
    {
        return;
    }
    if (Movement && Movement->IsSliding())
    {
        Movement->PrepareSlideJump();
        OnSlideChanged(false);
        LaunchCharacter(FVector(0.0f, 0.0f, Movement->JumpZVelocity), false, true);
        JumpCurrentCount = FMath::Max(JumpCurrentCount, 1);
        return;
    }
    if (TryMantle())
    {
        return;
    }
    Jump();
}

bool ABreakerCharacter::TryMantle()
{
    if (bMantling || !GetWorld() || !GetCapsuleComponent()) return false;

    const FVector Up = FVector::UpVector;
    const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
    const FVector ActorLocation = GetActorLocation();
    const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FVector FeetLocation = ActorLocation - Up * CapsuleHalfHeight;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerMantle), false, this);

    FHitResult WallHit;
    const FVector WallTraceStart = ActorLocation + Up * 15.0f;
    if (!GetWorld()->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceStart + Forward * MantleReach, ECC_Visibility, Params)
        || FMath::Abs(WallHit.ImpactNormal.Z) > 0.35f)
    {
        return false;
    }

    FHitResult TopHit;
    const FVector TopProbe = WallHit.ImpactPoint + Forward * (CapsuleRadius + 12.0f) + Up * MantleMaximumHeight;
    if (!GetWorld()->LineTraceSingleByChannel(TopHit, TopProbe, TopProbe - Up * (MantleMaximumHeight + 25.0f), ECC_Visibility, Params)
        || TopHit.ImpactNormal.Z < 0.65f)
    {
        return false;
    }

    const float LedgeHeight = TopHit.ImpactPoint.Z - FeetLocation.Z;
    if (LedgeHeight < MantleMinimumHeight || LedgeHeight > MantleMaximumHeight) return false;

    MantleTarget = TopHit.ImpactPoint + Up * (CapsuleHalfHeight + 3.0f) + Forward * 18.0f;
    const FCollisionShape Capsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
    if (GetWorld()->OverlapBlockingTestByChannel(MantleTarget, FQuat::Identity, GetCapsuleComponent()->GetCollisionObjectType(), Capsule, Params))
    {
        return false;
    }

    UBreakerCharacterMovementComponent* Movement = GetBreakerMovement();
    if (!Movement) return false;
    MantleExitVelocity = Movement->Velocity;
    MantleExitVelocity.Z = 0.0f;
    MantleStart = ActorLocation;
    MantleElapsed = 0.0f;
    bMantling = true;
    Movement->StopMovementImmediately();
    Movement->SetMovementMode(MOVE_Flying);
    return true;
}

bool ABreakerCharacter::TryDash()
{
    FVector Direction = GetLastMovementInputVector().GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction = GetActorForwardVector().GetSafeNormal2D();
    UBreakerCharacterMovementComponent* Movement = GetBreakerMovement();
    const bool bSucceeded = Movement && Movement->TryDash(Direction);
    if (bSucceeded) OnDashPerformed();
    return bSucceeded;
}

void ABreakerCharacter::StartSlide()
{
    if (UBreakerCharacterMovementComponent* Movement = GetBreakerMovement())
    {
        Movement->SetSlideRequested(true);
        if (Movement->BeginSlide()) OnSlideChanged(true);
    }
}

void ABreakerCharacter::StopSlide()
{
    if (UBreakerCharacterMovementComponent* Movement = GetBreakerMovement())
    {
        const bool bWasSliding = Movement->IsSliding();
        Movement->SetSlideRequested(false);
        Movement->EndSlide();
        if (bWasSliding) OnSlideChanged(false);
    }
}

// HOLSTERED IN THE ANCHOR: the hub is a social space and the trigger does
// nothing there (owner: "gun should be lowered in the anchor and unable to
// shoot"). StopFire stays un-gated on purpose — a held trigger crossing a
// travel load must always be releasable.
void ABreakerCharacter::StartFire() { if (bWeaponsHolstered) return; if (Weapon) Weapon->StartFire(); OnFireInput(true); }
void ABreakerCharacter::StopFire() { if (Weapon) Weapon->StopFire(); OnFireInput(false); }
void ABreakerCharacter::StartAim()
{
    if (Weapon) Weapon->SetAiming(true);
    UpdateViewmodelKick();
    OnAimInput(true);
}

void ABreakerCharacter::StopAim()
{
    if (Weapon) Weapon->SetAiming(false);
    UpdateViewmodelKick();
    OnAimInput(false);
}

namespace
{
    // HOLSTERED CARRY (the Anchor). The rig drops out of the eyeline and
    // pitches down — the social-space read the owner asked for ("gun should
    // be lowered in the anchor") — expressed as offsets from each archetype's
    // OWN hip pose rather than as an authored pose per archetype, for the
    // same anti-rot reason the ADS pose is derived. Prefixed for the unity
    // build like every other file-scope name in Combat/ and Characters/.
    constexpr float BreakerHolsterDropCm = 24.0f;       // O2 PLACEHOLDER
    constexpr float BreakerHolsterPullBackCm = 10.0f;   // O2 PLACEHOLDER
    constexpr float BreakerHolsterPitchDegrees = -35.0f; // O2 PLACEHOLDER
    constexpr float BreakerHolsterYawDegrees = 10.0f;   // O2 PLACEHOLDER
}

FVector ABreakerCharacter::GetWeaponRestLocation() const
{
    // Holstered wins over everything, including a stale ADS flag: an Anchor
    // pawn is holstered for its whole life and its rig must never present a
    // ready pose. Location-only here; the matching pitch-down lives in
    // UpdateViewmodelKick, which owns the rig's rotation.
    if (bWeaponsHolstered)
    {
        return ActiveLayout.HipOffsetCm + FVector(-BreakerHolsterPullBackCm, 0.0f, -BreakerHolsterDropCm);
    }
    // ADS is DERIVED, not authored: the rig comes forward and drops by exactly
    // this weapon's sight height, which puts its own sight on the crosshair.
    // Authoring the aimed pose per archetype was the alternative and it rots —
    // move one part and the sight silently stops lining up.
    if (Weapon && Weapon->IsAiming())
    {
        return FVector(ActiveLayout.AdsForwardCm, 0.0f, -ActiveLayout.SightHeightCm * ViewmodelScale);
    }
    return ActiveLayout.HipOffsetCm;
}

void ABreakerCharacter::UpdateViewmodelKick()
{
    // The weapon component owns the spring; the character only reads it onto
    // the blockout rig. Presentation, never a damage input. The whole assembly
    // — every proxy part, both arms, the muzzle light — is parented to this one
    // transform, so the kick moves the gun AND the hands holding it.
    if (!PrototypeWeaponVisual) return;
    const FVector Rest = GetWeaponRestLocation();
    const FVector Offset = Weapon ? Weapon->GetViewmodelLocationOffset() : FVector::ZeroVector;
    FRotator Rotation = Weapon ? Weapon->GetViewmodelRotationOffset() : FRotator::ZeroRotator;
    // Holstered: the muzzle pitches down and eases slightly across the body,
    // finishing what the dropped rest location starts — lowered and out of
    // the eyeline for the whole life of an Anchor pawn. Composed onto the
    // spring's rotation rather than replacing it, though holstered pawns
    // cannot fire so the spring is at rest anyway.
    if (bWeaponsHolstered)
    {
        Rotation.Pitch += BreakerHolsterPitchDegrees;
        Rotation.Yaw += BreakerHolsterYawDegrees;
    }
    PrototypeWeaponVisual->SetRelativeLocation(Rest + Offset);
    PrototypeWeaponVisual->SetRelativeRotation(Rotation);

    // The shoulders do NOT move with the rig — they are the player's body — so
    // moving the rig into or out of the sights changes where the arms have to
    // reach. Re-posed on the REST pose only, never on the kick: a recoiling gun
    // takes the hands with it, so compensating for the spring would decouple
    // them and the recoil would stop reading.
    if (bViewmodelBuilt && !Rest.Equals(PosedArmRestLocation, 0.01f))
    {
        PosedArmRestLocation = Rest;
        PoseArm(LeftArmVisual, LeftGloveVisual, SupportShoulderAnchorCm, ActiveLayout.SupportHandCm);
        PoseArm(RightArmVisual, RightGloveVisual, FiringShoulderAnchorCm, ActiveLayout.FiringHandCm);
    }
}
void ABreakerCharacter::HandleReloadInput() { if (Weapon) Weapon->StartReload(); OnReloadInput(); }

void ABreakerCharacter::EquipPrimaryWeapon()
{
    if (Weapon) Weapon->EquipSlot(1);
    ApplyWeaponPresentation();
}

void ABreakerCharacter::EquipSecondaryWeapon()
{
    if (Weapon) Weapon->EquipSlot(2);
    ApplyWeaponPresentation();
}

namespace
{
    // One dynamic instance per pooled component, created on first use and
    // re-tinted forever after. The stock basic-shape material exposes a single
    // "Color" vector parameter, which is the whole reason this blockout needs
    // no assets — the same trick BreakerGameMode's gym dressing uses.
    UMaterialInstanceDynamic* GetOrCreateBlockoutMaterial(UMeshComponent* Component)
    {
        if (!Component) return nullptr;
        if (UMaterialInstanceDynamic* Existing = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)))
        {
            return Existing;
        }
        UMaterialInterface* Base = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!Base) return nullptr;
        UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Base, Component);
        if (Dynamic) Component->SetMaterial(0, Dynamic);
        return Dynamic;
    }
}

void ABreakerCharacter::StartViewmodelCaptureCycle()
{
    // -BreakerCycleWeapons=<seconds> walks the equipped archetype through the
    // whole enum on a timer. It exists for exactly one reason: the screenshot
    // harness photographs an idle standing player, so without it a capture run
    // can only ever verify ONE gun, and "each archetype is distinguishable" is
    // a claim nobody could check. Dev-only by construction — a command-line
    // switch, unreachable from a shipped build, and it never touches a rule.
    float Interval = 0.0f;
    if (!FParse::Value(FCommandLine::Get(), TEXT("BreakerCycleWeapons="), Interval) || Interval <= 0.0f) return;
    if (!Weapon) return;

    UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] cycling weapon archetypes every %.1fs."), Interval);
    GetWorldTimerManager().SetTimer(ViewmodelCycleTimer, [this]()
    {
        if (!Weapon) return;
        const int32 Count = static_cast<int32>(EBreakerWeaponArchetype::Count);
        const EBreakerWeaponArchetype Next = static_cast<EBreakerWeaponArchetype>(ViewmodelCycleIndex % Count);
        // Second lap is aimed. The ADS rest pose is DERIVED from each layout's
        // sight height, so "every archetype puts its own sight on the
        // crosshair" is a claim only a capture can check.
        const bool bAimed = (ViewmodelCycleIndex / Count) % 2 == 1;
        ++ViewmodelCycleIndex;
        Weapon->SetSlotArchetype(1, Next);
        Weapon->EquipSlot(1);
        Weapon->SetAiming(bAimed);
        UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] archetype -> %s (%s)"),
            *BreakerWeaponArchetypeNames::Display(Next), bAimed ? TEXT("ADS") : TEXT("hip"));
    }, Interval, true, FMath::Max(0.5f, Interval * 0.5f));

    // ...and pulse the trigger, because a capture of an idle player proves
    // nothing about the two things this blockout exists for: that the recoil
    // spring visibly moves the gun, and that the muzzle flash lands at the
    // muzzle. Semi-automatic archetypes need the release, hence a pulse rather
    // than a held trigger.
    GetWorldTimerManager().SetTimer(ViewmodelFireTimer, [this]()
    {
        if (!Weapon) return;
        Weapon->StartFire();
        FTimerHandle Release;
        GetWorldTimerManager().SetTimer(Release, [this]() { if (Weapon) Weapon->StopFire(); }, 0.12f, false);
    }, 0.3f, true, 1.0f);
}

FBreakerViewmodelLayout ABreakerCharacter::ResolveViewmodelLayout(EBreakerWeaponArchetype Archetype) const
{
    if (const FBreakerViewmodelLayout* Override = ViewmodelLayoutOverrides.Find(Archetype))
    {
        return *Override;
    }
    return BreakerViewmodel::ArchetypeLayout(Archetype);
}

void ABreakerCharacter::ApplyWeaponPresentation()
{
    if (!Weapon || !PrototypeWeaponVisual) return;
    const EBreakerWeaponArchetype Archetype = Weapon->GetArchetype();
    // Cheap idempotence, because Tick calls this every frame as a backstop: the
    // archetype can change from the loadout screen, from an item equip, or from
    // a dev swap, and NONE of those route through EquipPrimary/EquipSecondary.
    // That is why the proxy used to keep the previous gun's proportions after a
    // loadout change until the player pressed 1 or 2.
    if (bViewmodelBuilt && Archetype == PresentedArchetype) return;
    PresentedArchetype = Archetype;
    ActiveLayout = ResolveViewmodelLayout(Archetype);
    bViewmodelBuilt = true;
    RebuildViewmodelParts();
    UpdateViewmodelKick();
}

void ABreakerCharacter::RebuildViewmodelParts()
{
    // Loaded here rather than held as constructor references because a part's
    // SHAPE changes with the archetype and the constructor cannot know it.
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));

    PrototypeWeaponVisual->SetRelativeScale3D(FVector(ViewmodelScale));

    const int32 PartCount = ActiveLayout.Parts.Num();
    for (int32 Index = 0; Index < ViewmodelParts.Num(); ++Index)
    {
        UStaticMeshComponent* Component = ViewmodelParts[Index];
        if (!Component) continue;
        if (Index >= PartCount || !ActiveLayout.Parts[Index].IsUsed())
        {
            // Unused pool slots are hidden, never destroyed. Swapping from a
            // nine-part machinegun to a five-part sidearm must not allocate.
            Component->SetVisibility(false);
            continue;
        }

        const FBreakerProxyPart& Part = ActiveLayout.Parts[Index];
        UStaticMesh* ShapeMesh = Cube;
        switch (Part.Shape)
        {
        case EBreakerProxyShape::CylinderX:
        case EBreakerProxyShape::CylinderY: ShapeMesh = Cylinder; break;
        case EBreakerProxyShape::ConeX:     ShapeMesh = Cone; break;
        default: break;
        }
        if (Component->GetStaticMesh() != ShapeMesh) Component->SetStaticMesh(ShapeMesh);

        FVector Scale;
        FRotator Rotation;
        BreakerViewmodel::ResolvePartTransform(Part, Scale, Rotation);
        Component->SetRelativeLocation(Part.LocationCm);
        Component->SetRelativeRotation(Rotation);
        Component->SetRelativeScale3D(Scale);
        Component->SetVisibility(true);
        if (UMaterialInstanceDynamic* Dynamic = GetOrCreateBlockoutMaterial(Component))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), Part.Color);
        }
    }

    // The flash sits at the actual muzzle of the actual gun, so a sidearm
    // flashes 18 cm out and a sniper 73 cm out.
    if (PrototypeMuzzleFlash)
    {
        PrototypeMuzzleFlash->SetRelativeLocation(ActiveLayout.MuzzleCm);
    }

    PoseArm(LeftArmVisual, LeftGloveVisual, SupportShoulderAnchorCm, ActiveLayout.SupportHandCm);
    PoseArm(RightArmVisual, RightGloveVisual, FiringShoulderAnchorCm, ActiveLayout.FiringHandCm);
}

void ABreakerCharacter::PoseArm(UStaticMeshComponent* Forearm, UStaticMeshComponent* Glove,
    const FVector& AnchorCm, const FVector& HandRigCm)
{
    if (!Forearm || !Glove || !PrototypeWeaponVisual) return;

    // The anchor is authored in CAMERA space (it is a property of the player's
    // body) and the hand in RIG space (it is a property of the gun), so the
    // anchor has to be pulled into rig space before the limb can be measured.
    // The near plane clips the shoulder end, which is what makes the limbs read
    // as entering frame from the player rather than as floating sticks.
    const float Scale = FMath::Max(ViewmodelScale, KINDA_SMALL_NUMBER);
    const FVector AnchorInRig = (AnchorCm - GetWeaponRestLocation()) / Scale;

    FVector Centre;
    FRotator Rotation;
    float Length = 0.0f;
    BreakerViewmodel::ResolveLimb(AnchorInRig, HandRigCm, Centre, Rotation, Length);

    Forearm->SetRelativeLocation(Centre);
    Forearm->SetRelativeRotation(Rotation);
    Forearm->SetRelativeScale3D(FVector(Length, ForearmWidthCm, ForearmWidthCm) / 100.0f);
    Forearm->SetVisibility(true);
    if (UMaterialInstanceDynamic* Dynamic = GetOrCreateBlockoutMaterial(Forearm))
    {
        Dynamic->SetVectorParameterValue(TEXT("Color"), BreakerViewmodel::SleeveSlate);
    }

    Glove->SetRelativeLocation(HandRigCm);
    Glove->SetRelativeRotation(Rotation);
    Glove->SetRelativeScale3D(FVector(GloveSizeCm, GloveSizeCm, GloveSizeCm * 0.8f) / 100.0f);
    Glove->SetVisibility(true);
    if (UMaterialInstanceDynamic* Dynamic = GetOrCreateBlockoutMaterial(Glove))
    {
        Dynamic->SetVectorParameterValue(TEXT("Color"), BreakerViewmodel::GloveOlive);
    }
}

void ABreakerCharacter::HandlePlayerDeath()
{
    // Death zeroes Grit BEFORE the playtest reset restores vitals: no banking
    // through a death and no free Hold on respawn (Class-Kits-Tank §1.4).
    if (Grit) Grit->NotifyDeath();
    ResetPlaytest();
}

// ---------------------------------------------------------------------------
// Class-resource event fan-out (T7 step 2). Every handler forwards one real
// event to every loop that could read it; the loops' own class gates make the
// forwarding a no-op for non-owners, so there is not a single class branch
// here to go stale on a class swap.
// ---------------------------------------------------------------------------

void ABreakerCharacter::HandleClassResourceKill(const FBreakerHitContext& Hit)
{
    LastHitDealtTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Scrap)
    {
        // §1.1: a kill credits at the KILLING INSTANCE'S proc coefficient.
        // RECORDED GAP: FBreakerHitContext does not carry the coefficient, so
        // every kill credits at 1.0. The rule's anti-farm target — Tick
        // Frequency becoming a Scrap engine — cannot fire through THIS event
        // (a kill happens once per enemy regardless of tick rate), so 1.0 is
        // an over-credit only in the DoT-landed-the-kill case, and it is
        // visible here rather than hidden.
        Scrap->NotifyKill(1.0f);
    }
    // Grit's melee-kill source is fed by Rend itself (the Tank's only melee
    // verb) — this generic kill event cannot tell melee from a bullet.
}

void ABreakerCharacter::HandleClassResourceHitDealt(const FBreakerHitContext& Hit)
{
    // Timestamp only: "recently dealt a hit" is half the in-combat derivation.
    LastHitDealtTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void ABreakerCharacter::HandleClassResourceDamageTaken(const FBreakerHitContext& Hit)
{
    if (!Grit) return;
    // POST-MITIGATION, health and shield as separate quantities — the split
    // the Grit component demands so shield absorption pays half. Instigator on
    // the hit context is what makes the self-damage flag honest: a Breach
    // Charge self-hit arrives with Instigator == this and pays at the 25%
    // self-damage rate under its own sub-cap, so rocket-jumping cannot become
    // the cheapest Grit engine (§1.3 rule 3).
    //
    // RECORDED GAP: the context carries no proc coefficient, so a DoT tick on
    // the Tank pays at 1.0 rather than at its coefficient. No enemy applies a
    // DoT to the player today, which is why this is a recorded gap and not a
    // live exploit; the day one does, the coefficient must travel with the
    // context.
    Grit->NotifyDamageTaken(Hit.Result.HealthDamage, Hit.Result.ShieldDamage,
        /*bSelfInflicted=*/Hit.Instigator == this, /*ProcCoefficient=*/1.0f);
    // The passive block layer: an RNG proc, never an input (O1). The roll
    // already happened inside the damage resolve; this only reports it.
    if (Hit.Result.bBlocked)
    {
        Grit->NotifyBlockProc();
    }
}

void ABreakerCharacter::HandleClassResourceHealingDealt(const FBreakerHealContext& Heal)
{
    if (!Charge || !Heal.Target) return;
    // The effective/overheal split arrives pre-separated on the heal result —
    // the exact contract the Charge component's signature demands, so overheal
    // credits nothing by construction. Percentage-of-TARGET max health.
    float TargetMaxHealth = 0.0f;
    if (const ABreakerCharacter* TargetBreaker = Cast<ABreakerCharacter>(Heal.Target))
    {
        if (const UBreakerAttributeSet* TargetAttributes = TargetBreaker->GetAttributes())
        {
            TargetMaxHealth = TargetAttributes->GetMaxHealth();
        }
    }
    else if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Heal.Target))
    {
        if (const UAbilitySystemComponent* TargetASC = AbilityOwner->GetAbilitySystemComponent())
        {
            if (const UBreakerAttributeSet* TargetAttributes = TargetASC->GetSet<UBreakerAttributeSet>())
            {
                TargetMaxHealth = TargetAttributes->GetMaxHealth();
            }
        }
    }
    const bool bSelfTargeted = Heal.Target == this;
    // RECORDED GAP: heal contexts carry no proc coefficient (no heal-over-time
    // source exists to need one); 1.0 until one does.
    if (Heal.Result.HealthHealed > 0.0f || Heal.Result.Overheal > 0.0f)
    {
        Charge->NotifyHealingDone(Heal.Result.HealthHealed, Heal.Result.Overheal, TargetMaxHealth, bSelfTargeted, 1.0f);
    }
    // The shield-side twin: shield actually granted (the overheal-to-shield
    // routing) credits through the shielding source; over-cap shield was
    // already trimmed by the healing resolve and so never reaches the rule.
    if (Heal.Result.ShieldGranted > 0.0f)
    {
        Charge->NotifyShieldingDone(Heal.Result.ShieldGranted, 0.0f, TargetMaxHealth, bSelfTargeted);
    }
}

void ABreakerCharacter::HandleClassResourceReloadCompleted(bool bAnyRoundFired)
{
    if (Scrap) Scrap->NotifyReloadCompleted(bAnyRoundFired);
}

void ABreakerCharacter::HandleClassResourceMagazineEmptied(bool bStartedFull)
{
    if (Scrap) Scrap->NotifyMagazineEmptied(bStartedFull);
}

void ABreakerCharacter::UpdateClassResourceStates()
{
    // Only the two loops with discrete state inputs pay for the derivation,
    // and only when one of them is actually live for this class.
    const bool bGritLive = Grit && Grit->IsActiveForOwner();
    const bool bChargeLive = Charge && Charge->IsActiveForOwner();
    if (!bGritLive && !bChargeLive) return;
    UWorld* World = GetWorld();
    if (!World) return;

    // "In combat": took or dealt damage inside the window. The dumbest true
    // derivation (O2 PLACEHOLDER) — the project has no shared combat-state
    // concept for it to read instead.
    const double Now = World->GetTimeSeconds();
    const bool bInCombat = (Combat && Combat->GetSecondsSinceDamage() < CombatStateWindowSeconds)
        || (Now - LastHitDealtTime < CombatStateWindowSeconds);
    if (bGritLive) Grit->SetInCombat(bInCombat);
    if (bChargeLive) Charge->SetInCombat(bInCombat);

    // Grit's proximity source: an enemy within 5 m (Class-Kits-Tank §1.1).
    // A BOOL by construction — the component has no count overload to pay
    // per-enemy, so this scan stops at the first live one.
    if (bGritLive)
    {
        bool bEnemyNear = false;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            const ABreakerEnemy* Enemy = *It;
            if (!Enemy) continue;
            const UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
            if (!EnemyCombat || EnemyCombat->IsDead()) continue;
            if (FVector::DistSquared(GetActorLocation(), Enemy->GetActorLocation()) <= GritProximityRadiusCm * GritProximityRadiusCm)
            {
                bEnemyNear = true;
                break;
            }
        }
        Grit->SetEnemyInProximity(bEnemyNear);
    }
}

void ABreakerCharacter::HandleShotCosmetics(const FBreakerShotResult& Shot)
{
    if (!Shot.bFired) return;
    if (PrototypeMuzzleFlash)
    {
        PrototypeMuzzleFlash->SetIntensity(8500.0f);
    }
    // The weapon mesh kick is no longer a timed snap: UBreakerWeaponComponent
    // runs a spring that this character samples every Tick.
    GetWorldTimerManager().SetTimer(ShotCosmeticTimer, this, &ThisClass::EndShotCosmetics, 0.055f, false);
}

void ABreakerCharacter::EndShotCosmetics()
{
    if (PrototypeMuzzleFlash) PrototypeMuzzleFlash->SetIntensity(0.0f);
}

// Reports the player's SETTING, not the camera's live value, so a dash punch in
// flight can never be read back by the settings screen, the HUD readout, or
// SavePlaytestSettings and become the new preference.
float ABreakerCharacter::GetCurrentFOV() const { return BaseFieldOfView; }

void ABreakerCharacter::ApplyBaseFieldOfView()
{
    if (FirstPersonCamera) FirstPersonCamera->SetFieldOfView(BaseFieldOfView);
}

float ABreakerCharacter::GetDashFeedbackAlpha() const
{
    if (DashFeedbackElapsed < 0.0f) return 0.0f;
    const float Attack = FMath::Max(DashFOVPunchAttack, UE_KINDA_SMALL_NUMBER);
    const float Recovery = FMath::Max(DashFOVPunchRecovery, UE_KINDA_SMALL_NUMBER);
    if (DashFeedbackElapsed <= Attack)
    {
        return FMath::Clamp(DashFeedbackElapsed / Attack, 0.0f, 1.0f);
    }
    // Ease-out on the way back: a linear recovery reads as a mechanical slide,
    // a decaying one reads as the camera settling.
    const float Alpha = FMath::Clamp((DashFeedbackElapsed - Attack) / Recovery, 0.0f, 1.0f);
    const float Remaining = 1.0f - Alpha;
    return Remaining * Remaining;
}

void ABreakerCharacter::HandleDashStarted(FVector DashDirection, float DashSpeed)
{
    LastDashDirection = DashDirection;
    if (!bDashCameraFeedback || !IsLocallyControlled()) return;

    DashFeedbackElapsed = 0.0f;
    // Scale by how fast the dash actually came out, so a dash that carried real
    // momentum in reads harder than a standing one. This is the number the
    // owner said they could not see.
    DashFeedbackScale = FMath::Clamp(
        DashSpeed / FMath::Max(DashFeedbackReferenceSpeed, 1.0f),
        FMath::Min(DashFeedbackMinimumScale, DashFeedbackMaximumScale),
        FMath::Max(DashFeedbackMinimumScale, DashFeedbackMaximumScale));
    // Roll is signed by how lateral the dash was: a forward dash gets none, a
    // strafe dash gets all of it, and the sign follows the side you went.
    const FVector Right = GetActorRightVector().GetSafeNormal2D();
    DashFeedbackRollSign = FMath::Clamp(static_cast<float>(FVector::DotProduct(DashDirection.GetSafeNormal2D(), Right)), -1.0f, 1.0f);
}

void ABreakerCharacter::UpdateDashCameraFeedback(float DeltaSeconds)
{
    if (DashFeedbackElapsed < 0.0f)
    {
        return;
    }

    DashFeedbackElapsed += DeltaSeconds;
    const float Alpha = GetDashFeedbackAlpha();
    const bool bFinished = DashFeedbackElapsed >= DashFOVPunchAttack + DashFOVPunchRecovery;

    if (FirstPersonCamera)
    {
        FirstPersonCamera->SetFieldOfView(FMath::Clamp(BaseFieldOfView + DashFOVPunch * DashFeedbackScale * Alpha, 5.0f, 170.0f));
    }

    // Roll rides the control rotation rather than the camera transform: the
    // camera runs bUsePawnControlRotation, so it re-derives its world rotation
    // from the view rotation every frame and would discard a relative roll.
    // The character never uses controller roll for anything else
    // (bUseControllerRotationRoll is false, so the capsule does not tilt), which
    // is why this is safe to own outright — and roll about the view axis leaves
    // the aim direction, and therefore every weapon trace, untouched.
    const float Roll = DashCameraRoll * DashFeedbackScale * DashFeedbackRollSign * Alpha;
    if (Controller && (bDashRollApplied || !FMath::IsNearlyZero(Roll)))
    {
        FRotator ControlRotation = Controller->GetControlRotation();
        ControlRotation.Roll = bFinished ? 0.0f : Roll;
        Controller->SetControlRotation(ControlRotation);
        bDashRollApplied = !bFinished;
    }

    if (bFinished)
    {
        DashFeedbackElapsed = -1.0f;
        bDashRollApplied = false;
        ApplyBaseFieldOfView();
    }
}

void ABreakerCharacter::ResetPlaytest()
{
    SetActorTransform(PlaytestSpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    GetCharacterMovement()->StopMovementImmediately();
    // Drop any dash punch in flight before the rotation is rewritten, or the
    // next frame would re-apply a roll on top of the reset view.
    DashFeedbackElapsed = -1.0f;
    bDashRollApplied = false;
    ApplyBaseFieldOfView();
    if (Controller) Controller->SetControlRotation(PlaytestSpawnTransform.Rotator());
    if (Weapon) Weapon->ResetAmmunition();
    if (Combat) Combat->RestoreVitals();
    if (Playtest) Playtest->ResetStats();
    if (ABreakerGameMode* GameMode = GetWorld() ? Cast<ABreakerGameMode>(GetWorld()->GetAuthGameMode()) : nullptr) GameMode->ResetPlaytestTargets();
}

void ABreakerCharacter::CopyPlaytestReport() { if (Playtest) Playtest->CopyReportToClipboard(); }
void ABreakerCharacter::TogglePlaytestDiagnostics() { if (Playtest) Playtest->ToggleDiagnostics(); }
void ABreakerCharacter::IncreaseFOV() { BaseFieldOfView = FMath::Clamp(BaseFieldOfView + 5.0f, 70.0f, 120.0f); ApplyBaseFieldOfView(); SavePlaytestSettings(); }
void ABreakerCharacter::DecreaseFOV() { BaseFieldOfView = FMath::Clamp(BaseFieldOfView - 5.0f, 70.0f, 120.0f); ApplyBaseFieldOfView(); SavePlaytestSettings(); }
// D27: these nudges used to clamp to 0.2-3.0 while the settings screen and
// the ini load clamped the SAME value to 0.2-2.0, so a nudged 2.1+ silently
// snapped back on the next menu open or restart. One clamp now, the settings
// model's, everywhere the value is written.
void ABreakerCharacter::IncreaseSensitivity() { LookSensitivity = UBreakerGameSettingsLibrary::ClampMouseSensitivity(LookSensitivity + 0.1f); SavePlaytestSettings(); }
void ABreakerCharacter::DecreaseSensitivity() { LookSensitivity = UBreakerGameSettingsLibrary::ClampMouseSensitivity(LookSensitivity - 0.1f); SavePlaytestSettings(); }

void ABreakerCharacter::SavePlaytestSettings() const
{
    GConfig->SetFloat(TEXT("RiorsEdge.Playtest"), TEXT("FOV"), GetCurrentFOV(), GGameUserSettingsIni);
    GConfig->SetFloat(TEXT("RiorsEdge.Playtest"), TEXT("Sensitivity"), LookSensitivity, GGameUserSettingsIni);
    GConfig->SetBool(TEXT("RiorsEdge.Playtest"), TEXT("InvertLookY"), bInvertLookY, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void ABreakerCharacter::ApplyMenuSettings(float NewSensitivity, float NewFOV, bool bNewInvertLookY)
{
    LookSensitivity = UBreakerGameSettingsLibrary::ClampMouseSensitivity(NewSensitivity);  // D27: the one clamp
    bInvertLookY = bNewInvertLookY;
    BaseFieldOfView = FMath::Clamp(NewFOV, 70.0f, 120.0f);
    ApplyBaseFieldOfView();
    SavePlaytestSettings();
}

void ABreakerCharacter::ShowInitialMenu()
{
    // -BreakerAutoPlay skips the title menu and drops straight into the gym.
    // This exists so the game can be SMOKE-TESTED without a human at the
    // keyboard: a standalone run that stops on the title screen proves only
    // that startup works, and never executes the gym, the enemy spawns, the
    // HUD or anything else a change is likely to break. Dev-only by
    // construction — it is a command-line switch, so a shipped build cannot
    // reach it unless someone deliberately passes it.
    if (FParse::Param(FCommandLine::Get(), TEXT("BreakerAutoPlay")))
    {
        UE_LOG(LogTemp, Display, TEXT("[BreakerAutoPlay] Skipping the title menu; entering the gym directly."));
        // ...unless a capture run asked for a specific screen, in which case
        // the whole point is to be sitting on it.
        FString CaptureScreen;
        if (FParse::Value(FCommandLine::Get(), TEXT("BreakerCaptureMenu="), CaptureScreen) && !CaptureScreen.IsEmpty())
        {
            OpenMenuScreenForCapture(CaptureScreen);
        }
        return;
    }
    OpenMenu(true);
}

void ABreakerCharacter::OpenMenu(bool bInitialMenu)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !GEngine || !GEngine->GameViewport) return;

    bShowingInitialMenu = bInitialMenu;
    if (!MenuWidget.IsValid())
    {
        MenuWidget = SNew(SBreakerMenu).Character(this);
        GEngine->GameViewport->AddViewportWidgetContent(MenuWidget.ToSharedRef(), 100);
    }
    if (bInitialMenu) MenuWidget->ShowMainMenu();
    else MenuWidget->ShowPauseMenu();

    PC->SetPause(true);
    PC->bShowMouseCursor = true;
    PC->bEnableClickEvents = true;
    PC->bEnableMouseOverEvents = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(MenuWidget);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    PC->SetInputMode(InputMode);
}

void ABreakerCharacter::OpenMenuScreenForCapture(const FString& ScreenName)
{
    OpenMenu(true);
    if (!MenuWidget.IsValid()) return;

    const FString Wanted = ScreenName.ToUpper();
    EBreakerMenuScreen Screen = EBreakerMenuScreen::Main;
    if (Wanted == TEXT("INVENTORY")) Screen = EBreakerMenuScreen::Inventory;
    else if (Wanted == TEXT("SKILLTREES") || Wanted == TEXT("SKILLS")) Screen = EBreakerMenuScreen::SkillTrees;
    else if (Wanted == TEXT("LOADOUT")) Screen = EBreakerMenuScreen::Loadout;
    else if (Wanted == TEXT("SETTINGS")) Screen = EBreakerMenuScreen::Settings;
    else if (Wanted == TEXT("CLASS") || Wanted == TEXT("CLASSSELECT")) Screen = EBreakerMenuScreen::ClassSelect;
    else if (Wanted == TEXT("PAUSE")) Screen = EBreakerMenuScreen::Pause;
    // The front door. Added with the screens themselves rather than after the
    // fact: every screen in this project that shipped unphotographable also
    // shipped broken, and character create is the one screen a new player
    // cannot avoid.
    else if (Wanted == TEXT("CHARACTERSELECT") || Wanted == TEXT("CHARACTERS")) Screen = EBreakerMenuScreen::CharacterSelect;
    else if (Wanted == TEXT("CHARACTERCREATE") || Wanted == TEXT("CREATE")) Screen = EBreakerMenuScreen::CharacterCreate;
    else if (Wanted == TEXT("DEVSANDBOX") || Wanted == TEXT("SANDBOX")) Screen = EBreakerMenuScreen::DevSandbox;

    MenuWidget->ShowScreenForCapture(Screen);
    UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] menu screen '%s'"), *Wanted);
}

void ABreakerCharacter::ResumeFromMenu()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (MenuWidget.IsValid() && GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
        MenuWidget.Reset();
    }
    bShowingInitialMenu = false;
    if (!PC) return;
    PC->SetPause(false);
    PC->bShowMouseCursor = false;
    PC->bEnableClickEvents = false;
    PC->bEnableMouseOverEvents = false;
    PC->SetInputMode(FInputModeGameOnly());
}

void ABreakerCharacter::ReturnToTitleMenu()
{
    bShowingInitialMenu = true;
    if (MenuWidget.IsValid()) MenuWidget->ShowMainMenu();
}

void ABreakerCharacter::QuitFromMenu()
{
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
    }
}

void ABreakerCharacter::ToggleInventoryMenu()
{
    if (MenuWidget.IsValid())
    {
        ResumeFromMenu();
        return;
    }
    OpenMenu(false);
    if (MenuWidget.IsValid()) MenuWidget->ShowInventory();
}

void ABreakerCharacter::StartWave()
{
    if (ABreakerGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr)
    {
        GameMode->StartNextWave();
    }
}

ABreakerNPC* ABreakerCharacter::FindNearbyNPC() const
{
    if (!GetWorld()) return nullptr;
    ABreakerNPC* Nearest = nullptr;
    float NearestDistanceSq = TNumericLimits<float>::Max();
    for (TActorIterator<ABreakerNPC> It(GetWorld()); It; ++It)
    {
        const float DistanceSq = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (DistanceSq <= FMath::Square(It->GetInteractionRange()) && DistanceSq < NearestDistanceSq)
        {
            NearestDistanceSq = DistanceSq;
            Nearest = *It;
        }
    }
    return Nearest;
}

ABreakerTravelPoint* ABreakerCharacter::FindNearbyTravelPoint() const
{
    if (!GetWorld()) return nullptr;
    ABreakerTravelPoint* Nearest = nullptr;
    float NearestDistanceSq = TNumericLimits<float>::Max();
    for (TActorIterator<ABreakerTravelPoint> It(GetWorld()); It; ++It)
    {
        const float DistanceSq = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (DistanceSq <= FMath::Square(It->GetInteractionRange()) && DistanceSq < NearestDistanceSq)
        {
            NearestDistanceSq = DistanceSq;
            Nearest = *It;
        }
    }
    return Nearest;
}

ABreakerLootPickup* ABreakerCharacter::FindNearbyPickup() const
{
    if (!GetWorld()) return nullptr;
    ABreakerLootPickup* Nearest = nullptr;
    float NearestDistanceSq = TNumericLimits<float>::Max();
    for (TActorIterator<ABreakerLootPickup> It(GetWorld()); It; ++It)
    {
        const float DistanceSq = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (DistanceSq <= FMath::Square(It->GetInteractionRange()) && DistanceSq < NearestDistanceSq)
        {
            NearestDistanceSq = DistanceSq;
            Nearest = *It;
        }
    }
    return Nearest;
}

void ABreakerCharacter::ServerPickupLoot_Implementation(ABreakerLootPickup* Pickup)
{
    if (Pickup) Pickup->TryPickup(this);
}

void ABreakerCharacter::InteractWithNearbyNPC()
{
    if (MenuWidget.IsValid()) return;

    // Loot wins the F key when both are candidates.
    if (ABreakerLootPickup* Pickup = FindNearbyPickup())
    {
        if (HasAuthority()) Pickup->TryPickup(this);
        else ServerPickupLoot(Pickup);
        return;
    }

    // The travel point, before NPCs. It is a large fixed structure and the
    // vendors stand near it, so if both are in range the player who walked up
    // to the gate meant the gate.
    //
    // EVERY case routes through the picker, including the single-destination
    // one. F used to travel DIRECTLY when exactly one destination existed and
    // refuse with a log when more than one did, because no picker existed —
    // and the owner asked for "a navigation place to click into and select
    // where youd like to go". Since every point offers exactly one destination
    // today, the single-destination path is the ONLY one anyone would ever
    // see, so keeping the direct-travel shortcut would mean the navigation
    // screen shipped unreachable.
    if (ABreakerTravelPoint* Travel = FindNearbyTravelPoint())
    {
        OpenMenu(false);
        if (MenuWidget.IsValid()) MenuWidget->ShowTravel(Travel);
        return;
    }

    ABreakerNPC* NPC = FindNearbyNPC();
    if (!NPC) return;
    OpenMenu(false);
    if (MenuWidget.IsValid()) MenuWidget->ShowDialogue(NPC);
}

void ABreakerCharacter::AddQuestFlag(FName Flag)
{
    // The journal persists on change. This used to be an AddUnique into a bare
    // array with no save anywhere on the path, so a story beat survived only a
    // clean shutdown.
    if (Quests) Quests->SetFlag(Flag);
}

bool ABreakerCharacter::HasQuestFlag(FName Flag) const
{
    return Quests && Quests->HasFlag(Flag);
}

const TArray<FName>& ABreakerCharacter::GetQuestFlags() const
{
    static const TArray<FName> Empty;
    return Quests ? Quests->GetFlags() : Empty;
}

void ABreakerCharacter::SetQuestFlags(const TArray<FName>& NewFlags)
{
    if (Quests) Quests->RestoreFrom(NewFlags, TMap<FName, int32>());
}

void ABreakerCharacter::HandleQuestKill(const FBreakerHitContext& Hit)
{
    if (!Quests || !HasAuthority()) return;
    // Rank, not archetype: O27 made rank the flag for what an elite is, so the
    // objective reads the same number the chassis does. Anything above elite
    // (a boss) counts for an elite objective too.
    const ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Hit.Target);
    if (!Enemy) return;
    const bool bEliteOrAbove = Enemy->GetMonsterRank() != EBreakerMonsterRank::Trash;
    UBreakerQuestLibrary::NotifyEnemyKilled(*Quests, bEliteOrAbove);
}

void ABreakerCharacter::GrantQuestRewardForFlag(FName Flag)
{
    if (!Equipment || !HasAuthority()) return;
    FBreakerQuestDefinition Paid;
    bool bFound = false;
    for (const FBreakerQuestDefinition& Quest : UBreakerQuestLibrary::GetFallbackQuests())
    {
        if (Quest.TurnedInFlag == Flag) { Paid = Quest; bFound = true; break; }
    }
    if (!bFound) return;

    for (int32 Index = 0; Index < Paid.Reward.ItemCount; ++Index)
    {
        // Deterministic seed per (quest, index) so a reward is reproducible in
        // a bug report rather than a different item every time the case is
        // reproduced. Slot rotates so a multi-item reward is not eight helmets.
        const int32 Seed = GetTypeHash(Paid.QuestId) + Index * 7919;
        const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>((GetTypeHash(Paid.QuestId) + Index) % static_cast<int32>(EBreakerEquipSlot::Count));
        Equipment->AddToBackpack(UBreakerLootLibrary::RollItem(TEXT("QuestReward"), Slot, Paid.Reward.MinimumRarity, Paid.Reward.ItemLevel, Seed));
    }
    UE_LOG(LogTemp, Log, TEXT("Quest '%s' turned in; %d reward item(s) granted"), *Paid.QuestId.ToString(), Paid.Reward.ItemCount);
}

void ABreakerCharacter::MenuRebindKey(FKey Key)
{
    if (MenuWidget.IsValid()) MenuWidget->HandleRebindKey(Key);
}

void ABreakerCharacter::ConfirmMenuKey()
{
    // Only the menu cares, and today only the title gate does. Guarded rather
    // than unconditional so Enter keeps meaning nothing during play.
    if (MenuWidget.IsValid()) MenuWidget->HandleConfirmKey();
}

void ABreakerCharacter::TogglePauseMenu()
{
    if (!MenuWidget.IsValid())
    {
        OpenMenu(false);
        return;
    }
    MenuWidget->HandleEscape();
}
