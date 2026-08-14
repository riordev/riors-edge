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
#include "Abilities/BreakerAbilityComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Save/BreakerSaveGame.h"
#include "Save/BreakerQuestJournal.h"
#include "Interaction/BreakerNPC.h"
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
    AbilitySystem->InitAbilityActorInfo(this, this);
    if (Weapon) Weapon->OnShot.AddDynamic(this, &ThisClass::HandleShotCosmetics);
    if (Combat) Combat->OnDeath.AddDynamic(this, &ThisClass::HandlePlayerDeath);
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
    LookSensitivity = FMath::Clamp(LookSensitivity, 0.2f, 2.0f);
    BaseFieldOfView = FMath::Clamp(SavedFOV, 70.0f, 120.0f);
    ApplyBaseFieldOfView();
    // Presentation binds to the movement rule, never the other way round: the
    // component broadcasts that a dash happened and this class decides what the
    // camera does about it — the same split OnLandingImpact already uses.
    if (UBreakerCharacterMovementComponent* Movement = GetBreakerMovement())
    {
        Movement->OnDashStarted.AddDynamic(this, &ThisClass::HandleDashStarted);
    }
    if (const APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (InputConfig && InputConfig->DefaultMappingContext)
                    Subsystem->AddMappingContext(InputConfig->DefaultMappingContext, 0);
            }
        }
    }
    if (IsLocallyControlled())
    {
        GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ShowInitialMenu);
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
    Save->SlotOneArchetype = Weapon->GetSlotArchetype(1);
    Save->SlotTwoArchetype = Weapon->GetSlotArchetype(2);
    if (Quests)
    {
        Save->QuestFlags = Quests->GetState().Flags;
        Save->QuestCounters = Quests->GetState().Counters;
    }
    Save->SaveVersion = UBreakerSaveGame::CurrentSaveVersion;
    UGameplayStatics::SaveGameToSlot(Save, UBreakerSaveGame::DefaultSlotName(), 0);
}

void ABreakerCharacter::LoadGameState()
{
    if (!HasAuthority() || !Progression || !Equipment || !Weapon) return;
    UBreakerSaveGame* Save = Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromSlot(UBreakerSaveGame::DefaultSlotName(), 0));
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

void ABreakerCharacter::ActivateAbilityOne()
{
    if (Abilities) Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne);
}

void ABreakerCharacter::ActivateAbilityTwo()
{
    if (Abilities) Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo);
}

void ABreakerCharacter::ActivateUltimate()
{
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

void ABreakerCharacter::StartFire() { if (Weapon) Weapon->StartFire(); OnFireInput(true); }
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

FVector ABreakerCharacter::GetWeaponRestLocation() const
{
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
    const FRotator Rotation = Weapon ? Weapon->GetViewmodelRotationOffset() : FRotator::ZeroRotator;
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
    ResetPlaytest();
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
void ABreakerCharacter::IncreaseSensitivity() { LookSensitivity = FMath::Clamp(LookSensitivity + 0.1f, 0.2f, 3.0f); SavePlaytestSettings(); }
void ABreakerCharacter::DecreaseSensitivity() { LookSensitivity = FMath::Clamp(LookSensitivity - 0.1f, 0.2f, 3.0f); SavePlaytestSettings(); }

void ABreakerCharacter::SavePlaytestSettings() const
{
    GConfig->SetFloat(TEXT("RiorsEdge.Playtest"), TEXT("FOV"), GetCurrentFOV(), GGameUserSettingsIni);
    GConfig->SetFloat(TEXT("RiorsEdge.Playtest"), TEXT("Sensitivity"), LookSensitivity, GGameUserSettingsIni);
    GConfig->SetBool(TEXT("RiorsEdge.Playtest"), TEXT("InvertLookY"), bInvertLookY, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void ABreakerCharacter::ApplyMenuSettings(float NewSensitivity, float NewFOV, bool bNewInvertLookY)
{
    LookSensitivity = FMath::Clamp(NewSensitivity, 0.2f, 2.0f);
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

void ABreakerCharacter::TogglePauseMenu()
{
    if (!MenuWidget.IsValid())
    {
        OpenMenu(false);
        return;
    }
    MenuWidget->HandleEscape();
}
