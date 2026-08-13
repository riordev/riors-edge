#include "Characters/BreakerCharacter.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
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
#include "Playtest/BreakerPlaytestComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Save/BreakerSaveGame.h"
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

    PrototypeWeaponVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeWeaponVisual"));
    PrototypeWeaponVisual->SetupAttachment(FirstPersonCamera);
    PrototypeWeaponVisual->SetRelativeLocation(FVector(48.0f, 18.0f, -18.0f));
    PrototypeWeaponVisual->SetRelativeScale3D(FVector(0.42f, 0.08f, 0.08f));
    PrototypeWeaponVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrototypeWeaponVisual->SetOnlyOwnerSee(true);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded()) PrototypeWeaponVisual->SetStaticMesh(CubeMesh.Object);

    PrototypeWeaponBarrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeWeaponBarrel"));
    PrototypeWeaponBarrel->SetupAttachment(PrototypeWeaponVisual);
    PrototypeWeaponBarrel->SetRelativeLocation(FVector(75.0f, 0.0f, 0.0f));
    PrototypeWeaponBarrel->SetRelativeScale3D(FVector(0.75f, 0.32f, 0.32f));
    PrototypeWeaponBarrel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrototypeWeaponBarrel->SetOnlyOwnerSee(true);
    if (CubeMesh.Succeeded()) PrototypeWeaponBarrel->SetStaticMesh(CubeMesh.Object);

    PrototypeWeaponSight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeWeaponSight"));
    PrototypeWeaponSight->SetupAttachment(PrototypeWeaponVisual);
    PrototypeWeaponSight->SetRelativeLocation(FVector(15.0f, 0.0f, 65.0f));
    PrototypeWeaponSight->SetRelativeScale3D(FVector(0.12f, 0.3f, 0.32f));
    PrototypeWeaponSight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrototypeWeaponSight->SetOnlyOwnerSee(true);
    if (CubeMesh.Succeeded()) PrototypeWeaponSight->SetStaticMesh(CubeMesh.Object);

    PrototypeMuzzleFlash = CreateDefaultSubobject<UPointLightComponent>(TEXT("PrototypeMuzzleFlash"));
    PrototypeMuzzleFlash->SetupAttachment(FirstPersonCamera);
    PrototypeMuzzleFlash->SetRelativeLocation(FVector(90.0f, 0.0f, -10.0f));
    PrototypeMuzzleFlash->SetLightColor(FLinearColor(1.0f, 0.35f, 0.05f));
    PrototypeMuzzleFlash->SetIntensity(0.0f);
    PrototypeMuzzleFlash->SetAttenuationRadius(220.0f);

    // Lightweight first-person assembly for the movement gym. Blueprint can
    // replace these blocks with authored arms without changing gameplay rules.
    LeftArmVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftArmVisual"));
    LeftArmVisual->SetupAttachment(FirstPersonCamera);
    LeftArmVisual->SetRelativeLocation(FVector(25.0f, -17.0f, -22.0f));
    LeftArmVisual->SetRelativeScale3D(FVector(0.28f, 0.045f, 0.045f));
    LeftArmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftArmVisual->SetOnlyOwnerSee(true);
    if (CubeMesh.Succeeded()) LeftArmVisual->SetStaticMesh(CubeMesh.Object);

    RightArmVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightArmVisual"));
    RightArmVisual->SetupAttachment(FirstPersonCamera);
    RightArmVisual->SetRelativeLocation(FVector(31.0f, 13.0f, -24.0f));
    RightArmVisual->SetRelativeScale3D(FVector(0.32f, 0.045f, 0.045f));
    RightArmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightArmVisual->SetOnlyOwnerSee(true);
    if (CubeMesh.Succeeded()) RightArmVisual->SetStaticMesh(CubeMesh.Object);
}

UAbilitySystemComponent* ABreakerCharacter::GetAbilitySystemComponent() const { return AbilitySystem; }

void ABreakerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
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
    if (HasAuthority()) LoadGameState();
    float SavedFOV = 90.0f;
    GConfig->GetFloat(TEXT("RiorsEdge.Playtest"), TEXT("FOV"), SavedFOV, GGameUserSettingsIni);
    GConfig->GetFloat(TEXT("RiorsEdge.Playtest"), TEXT("Sensitivity"), LookSensitivity, GGameUserSettingsIni);
    GConfig->GetBool(TEXT("RiorsEdge.Playtest"), TEXT("InvertLookY"), bInvertLookY, GGameUserSettingsIni);
    LookSensitivity = FMath::Clamp(LookSensitivity, 0.2f, 2.0f);
    FirstPersonCamera->SetFieldOfView(FMath::Clamp(SavedFOV, 70.0f, 120.0f));
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
    UGameplayStatics::SaveGameToSlot(Save, UBreakerSaveGame::DefaultSlotName(), 0);
}

void ABreakerCharacter::LoadGameState()
{
    if (!HasAuthority() || !Progression || !Equipment || !Weapon) return;
    UBreakerSaveGame* Save = Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromSlot(UBreakerSaveGame::DefaultSlotName(), 0));
    if (!Save) return;
    Progression->LoadProgressionState(Save->Progression);
    Equipment->RestoreState(Save->EquippedItems, Save->BackpackItems);
    Weapon->SetSlotArchetype(1, Save->SlotOneArchetype);
    Weapon->SetSlotArchetype(2, Save->SlotTwoArchetype);
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
    if (PrototypeWeaponVisual) PrototypeWeaponVisual->SetRelativeLocation(FVector(48.0f, 0.0f, -12.0f));
    OnAimInput(true);
}

void ABreakerCharacter::StopAim()
{
    if (Weapon) Weapon->SetAiming(false);
    if (PrototypeWeaponVisual) PrototypeWeaponVisual->SetRelativeLocation(FVector(48.0f, 18.0f, -18.0f));
    OnAimInput(false);
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

void ABreakerCharacter::ApplyWeaponPresentation()
{
    if (!Weapon || !PrototypeWeaponVisual || !PrototypeWeaponBarrel || !PrototypeWeaponSight) return;
    switch (Weapon->GetArchetype())
    {
        case EBreakerWeaponArchetype::Shotgun:
            PrototypeWeaponVisual->SetRelativeScale3D(FVector(0.34f, 0.12f, 0.11f));
            PrototypeWeaponBarrel->SetRelativeScale3D(FVector(0.55f, 0.48f, 0.48f));
            PrototypeWeaponSight->SetRelativeScale3D(FVector(0.08f, 0.22f, 0.22f));
            break;
        case EBreakerWeaponArchetype::Sniper:
            PrototypeWeaponVisual->SetRelativeScale3D(FVector(0.52f, 0.065f, 0.065f));
            PrototypeWeaponBarrel->SetRelativeScale3D(FVector(1.05f, 0.22f, 0.22f));
            PrototypeWeaponSight->SetRelativeScale3D(FVector(0.22f, 0.45f, 0.45f));
            break;
        case EBreakerWeaponArchetype::SMG:
            PrototypeWeaponVisual->SetRelativeScale3D(FVector(0.28f, 0.09f, 0.09f));
            PrototypeWeaponBarrel->SetRelativeScale3D(FVector(0.45f, 0.28f, 0.28f));
            PrototypeWeaponSight->SetRelativeScale3D(FVector(0.09f, 0.24f, 0.24f));
            break;
        case EBreakerWeaponArchetype::Rocket:
            PrototypeWeaponVisual->SetRelativeScale3D(FVector(0.4f, 0.16f, 0.16f));
            PrototypeWeaponBarrel->SetRelativeScale3D(FVector(0.9f, 0.6f, 0.6f));
            PrototypeWeaponSight->SetRelativeScale3D(FVector(0.14f, 0.34f, 0.34f));
            break;
        default:
            PrototypeWeaponVisual->SetRelativeScale3D(FVector(0.42f, 0.08f, 0.08f));
            PrototypeWeaponBarrel->SetRelativeScale3D(FVector(0.75f, 0.32f, 0.32f));
            PrototypeWeaponSight->SetRelativeScale3D(FVector(0.12f, 0.3f, 0.32f));
            break;
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
    if (PrototypeWeaponVisual)
    {
        const FVector RestingLocation = Weapon && Weapon->IsAiming()
            ? FVector(48.0f, 0.0f, -12.0f)
            : FVector(48.0f, 18.0f, -18.0f);
        PrototypeWeaponVisual->SetRelativeLocation(RestingLocation - FVector(4.0f, 0.0f, 0.0f));
    }
    GetWorldTimerManager().SetTimer(ShotCosmeticTimer, this, &ThisClass::EndShotCosmetics, 0.055f, false);
}

void ABreakerCharacter::EndShotCosmetics()
{
    if (PrototypeMuzzleFlash) PrototypeMuzzleFlash->SetIntensity(0.0f);
    if (PrototypeWeaponVisual)
    {
        PrototypeWeaponVisual->SetRelativeLocation(Weapon && Weapon->IsAiming()
            ? FVector(48.0f, 0.0f, -12.0f)
            : FVector(48.0f, 18.0f, -18.0f));
    }
}

float ABreakerCharacter::GetCurrentFOV() const { return FirstPersonCamera ? FirstPersonCamera->FieldOfView : 90.0f; }

void ABreakerCharacter::ResetPlaytest()
{
    SetActorTransform(PlaytestSpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    GetCharacterMovement()->StopMovementImmediately();
    if (Controller) Controller->SetControlRotation(PlaytestSpawnTransform.Rotator());
    if (Weapon) Weapon->ResetAmmunition();
    if (Combat) Combat->RestoreVitals();
    if (Playtest) Playtest->ResetStats();
    if (ABreakerGameMode* GameMode = GetWorld() ? Cast<ABreakerGameMode>(GetWorld()->GetAuthGameMode()) : nullptr) GameMode->ResetPlaytestTargets();
}

void ABreakerCharacter::CopyPlaytestReport() { if (Playtest) Playtest->CopyReportToClipboard(); }
void ABreakerCharacter::TogglePlaytestDiagnostics() { if (Playtest) Playtest->ToggleDiagnostics(); }
void ABreakerCharacter::IncreaseFOV() { FirstPersonCamera->SetFieldOfView(FMath::Clamp(GetCurrentFOV() + 5.0f, 70.0f, 120.0f)); SavePlaytestSettings(); }
void ABreakerCharacter::DecreaseFOV() { FirstPersonCamera->SetFieldOfView(FMath::Clamp(GetCurrentFOV() - 5.0f, 70.0f, 120.0f)); SavePlaytestSettings(); }
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
    if (FirstPersonCamera) FirstPersonCamera->SetFieldOfView(FMath::Clamp(NewFOV, 70.0f, 120.0f));
    SavePlaytestSettings();
}

void ABreakerCharacter::ShowInitialMenu()
{
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

void ABreakerCharacter::TogglePauseMenu()
{
    if (!MenuWidget.IsValid())
    {
        OpenMenu(false);
        return;
    }
    MenuWidget->HandleEscape();
}
