#include "Characters/BreakerCharacter.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
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
#include "Game/BreakerGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/ConfigCacheIni.h"

ABreakerCharacter::ABreakerCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UBreakerCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = false;
    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(-10.0, 0.0, 64.0));
    FirstPersonCamera->bUsePawnControlRotation = true;
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    Attributes = CreateDefaultSubobject<UBreakerAttributeSet>(TEXT("Attributes"));
    Progression = CreateDefaultSubobject<UBreakerProgressionComponent>(TEXT("Progression"));
    Combat = CreateDefaultSubobject<UBreakerCombatComponent>(TEXT("Combat"));
    Weapon = CreateDefaultSubobject<UBreakerWeaponComponent>(TEXT("Weapon"));
    Playtest = CreateDefaultSubobject<UBreakerPlaytestComponent>(TEXT("Playtest"));

    PrototypeWeaponVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeWeaponVisual"));
    PrototypeWeaponVisual->SetupAttachment(FirstPersonCamera);
    PrototypeWeaponVisual->SetRelativeLocation(FVector(48.0f, 18.0f, -18.0f));
    PrototypeWeaponVisual->SetRelativeScale3D(FVector(0.42f, 0.08f, 0.08f));
    PrototypeWeaponVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrototypeWeaponVisual->SetOnlyOwnerSee(true);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded()) PrototypeWeaponVisual->SetStaticMesh(CubeMesh.Object);
}

UAbilitySystemComponent* ABreakerCharacter::GetAbilitySystemComponent() const { return AbilitySystem; }

void ABreakerCharacter::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    PlaytestSpawnTransform = GetActorTransform();
    float SavedFOV = 90.0f;
    GConfig->GetFloat(TEXT("RiorsEdge.Playtest"), TEXT("FOV"), SavedFOV, GGameUserSettingsIni);
    GConfig->GetFloat(TEXT("RiorsEdge.Playtest"), TEXT("LookSensitivity"), LookSensitivity, GGameUserSettingsIni);
    LookSensitivity = FMath::Clamp(LookSensitivity, 0.2f, 3.0f);
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
}

void ABreakerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
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
        PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &ThisClass::StopSprint);
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
        PlayerInputComponent->BindAction(TEXT("SensitivityUp"), IE_Pressed, this, &ThisClass::IncreaseSensitivity);
        PlayerInputComponent->BindAction(TEXT("SensitivityDown"), IE_Pressed, this, &ThisClass::DecreaseSensitivity);
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
        Input->BindAction(InputConfig->Sprint, ETriggerEvent::Completed, this, &ThisClass::StopSprint);
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
    AddControllerPitchInput(Axis.Y * LookSensitivity);
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
void ABreakerCharacter::LookUpLegacy(float Value) { AddControllerPitchInput(Value * LookSensitivity); }

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
    if (UBreakerCharacterMovementComponent* Movement = GetBreakerMovement()) Movement->SetSprinting(true);
}

void ABreakerCharacter::StopSprint()
{
    if (UBreakerCharacterMovementComponent* Movement = GetBreakerMovement()) Movement->SetSprinting(false);
}

void ABreakerCharacter::HandleDashInput() { TryDash(); }

void ABreakerCharacter::HandleJumpInput()
{
    UBreakerCharacterMovementComponent* Movement = GetBreakerMovement();
    if (!Movement || !Movement->TryWallJump()) Jump();
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
    if (UBreakerCharacterMovementComponent* Movement = GetBreakerMovement(); Movement && Movement->BeginSlide()) OnSlideChanged(true);
}

void ABreakerCharacter::StopSlide()
{
    if (UBreakerCharacterMovementComponent* Movement = GetBreakerMovement(); Movement && Movement->IsSliding())
    {
        Movement->EndSlide();
        OnSlideChanged(false);
    }
}

void ABreakerCharacter::StartFire() { if (Weapon) Weapon->StartFire(); OnFireInput(true); }
void ABreakerCharacter::StopFire() { if (Weapon) Weapon->StopFire(); OnFireInput(false); }
void ABreakerCharacter::StartAim() { if (Weapon) Weapon->SetAiming(true); OnAimInput(true); }
void ABreakerCharacter::StopAim() { if (Weapon) Weapon->SetAiming(false); OnAimInput(false); }
void ABreakerCharacter::HandleReloadInput() { if (Weapon) Weapon->StartReload(); OnReloadInput(); }

float ABreakerCharacter::GetCurrentFOV() const { return FirstPersonCamera ? FirstPersonCamera->FieldOfView : 90.0f; }

void ABreakerCharacter::ResetPlaytest()
{
    SetActorTransform(PlaytestSpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    GetCharacterMovement()->StopMovementImmediately();
    if (Controller) Controller->SetControlRotation(PlaytestSpawnTransform.Rotator());
    if (Weapon) Weapon->ResetAmmunition();
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
    GConfig->SetFloat(TEXT("RiorsEdge.Playtest"), TEXT("LookSensitivity"), LookSensitivity, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}
