#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerEnemyThreatMath.h"
#include "Combat/BreakerDeployable.h"
#include "AI/BreakerEnemyController.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "AI/BreakerLocomotionMath.h"

#include "Combat/BreakerEnemyBodyMath.h"
#include "Combat/BreakerHitReactionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"

#include "Items/BreakerAffixLibrary.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerModifierComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerEntropy.h"
#include "Combat/BreakerVoid.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UI/BreakerUIStyle.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Game/BreakerGameMode.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerLootLibrary.h"
#include "Items/BreakerLootPickup.h"
#include "Playtest/BreakerPlaytestComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    bool BreakerEnemyOwnsIgnoreMe(const AActor* Actor)
    {
        const auto* Progression = IsValid(Actor) ? Actor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
        const FGameplayTag IgnoreMe = FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Threat.IgnoreMe"), false);
        return Progression && IgnoreMe.IsValid() && Progression->HasNodeTag(IgnoreMe);
    }

    // Muted Vestige-ish grey-violet for the humanoid body parts. Local copy of
    // the dressing helper so the enemy never pulls in game mode internals.
    // The COLOUR is not a local copy: it is the same symbol the paint state
    // defaults to, because a family's declared paint and its painted paint
    // being two literals is exactly the drift O128 removes.
    void ApplyEnemyBodyColor(UStaticMeshComponent* Mesh, const FLinearColor& FamilyPaint)
    {
        if (!Mesh) return;
        UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!BaseMaterial) return;
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), FamilyPaint);
            Mesh->SetMaterial(0, Dynamic);
        }
    }
}

// Dev preview for the named-body hook: applies a mesh (and optional looped
// idle) to every LIVE enemy AND arms a session default that enemies spawned
// afterwards pick up at BeginPlay — -ExecCmds runs before the gym has spawned
// anything, so a live-only sweep from the command line applied to zero bodies
// and photographed nothing. Console-only state, cleared with no arguments;
// nothing ships through it, which NoEnemyShipsANamedBody keeps true.
//   Breaker.EnemyBody /Game/Breaker/Meshes/enemies/Rat.Rat [/Game/.../Rig_Idle.Rig_Idle]
//   Breaker.EnemyBody            (clears the preview for later spawns)
static FString BreakerEnemyBodyPreviewMesh;
static FString BreakerEnemyBodyPreviewAnim;
static FAutoConsoleCommandWithWorldAndArgs BreakerEnemyBodyPreviewCommand(
    TEXT("Breaker.EnemyBody"),
    TEXT("Preview: fit a skeletal mesh onto every live enemy and every later spawn. Args: <MeshObjectPath> [IdleAnimObjectPath]; none clears."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        BreakerEnemyBodyPreviewMesh = Args.Num() > 0 ? Args[0] : FString();
        BreakerEnemyBodyPreviewAnim = Args.Num() > 1 ? Args[1] : FString();
        if (!World || BreakerEnemyBodyPreviewMesh.IsEmpty())
        {
            UE_LOG(LogTemp, Display, TEXT("[BreakerEnemy] Breaker.EnemyBody preview cleared."));
            return;
        }
        int32 Applied = 0;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            It->BodyMeshAsset = FSoftObjectPath(BreakerEnemyBodyPreviewMesh);
            if (!BreakerEnemyBodyPreviewAnim.IsEmpty()) It->BodyIdleAnimation = FSoftObjectPath(BreakerEnemyBodyPreviewAnim);
            It->ApplyBodyMesh();
            ++Applied;
        }
        UE_LOG(LogTemp, Display, TEXT("[BreakerEnemy] Breaker.EnemyBody applied %s to %d live enemies (armed for later spawns)."),
            *BreakerEnemyBodyPreviewMesh, Applied);
    }));

// Dev photography for the paint layers: the hit flash is 0.07 s and the
// screenshot cadence is 2 s, so the reel can never catch one honestly. The
// probe drives the SAME public setters a fight drives — nothing is painted
// that a fight could not paint — it just holds the state still long enough
// to photograph. Instrument only; the burn is deliberately absent because
// StartDeathPresentation ends in the owner hiding its body, and a probe that
// hides the cast photographs nothing (the burn shares the flash's overlay
// path — colour lerp, strength 1 — so the flash frame is its witness).
//   Breaker.BodyPaint elite | wash | flash | clear
static FString BreakerEnemyBodyPaintProbeMode;
static FAutoConsoleCommandWithWorldAndArgs BreakerBodyPaintProbeCommand(
    TEXT("Breaker.BodyPaint"),
    TEXT("Preview paint states on every live enemy: elite | wash | flash | clear."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World || Args.Num() < 1) return;
        // ARMS as well as drives — Breaker.EnemyBody's pattern: -ExecCmds
        // runs before the gym spawns anything (and possibly on the pre-travel
        // world), so live sweeps see nobody. The static survives the travel
        // and BeginPlay applies it to every later spawn; "clear" disarms.
        BreakerEnemyBodyPaintProbeMode = Args[0].Equals(TEXT("clear"), ESearchCase::IgnoreCase)
            ? FString() : Args[0];
        int32 Driven = 0;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            It->DevDriveBodyPaintProbe(Args[0]);
            ++Driven;
        }
        UE_LOG(LogTemp, Display, TEXT("[BreakerEnemy] Breaker.BodyPaint %s on %d enemies (armed for later spawns)."), *Args[0], Driven);
    }));

// Dev photography for the MODIFIER layer's tells: the gym's carriers draw
// their modifiers from the seeded roll, so a specific tell cannot be
// summoned for the camera — ten modifiers, random draws, and the one you
// need never on screen. This drives the EXISTING exact-grant seam
// (ConfigureWithExactModifiers, the door the through-actor test already
// uses) on every live enemy, and ARMS for later spawns exactly like
// Breaker.BodyPaint. Instrument only; the shipped draw stays the roll.
//   Breaker.EnemyModifier <Name> | clear     (Volatile, Fleetfoot, Wakeful...)
static FString BreakerEnemyModifierProbeName;

static bool BreakerEnemyResolveModifierProbe(const FString& Name, EBreakerEnemyModifier& Out)
{
    const UEnum* Enum = StaticEnum<EBreakerEnemyModifier>();
    if (!Enum) return false;
    const int64 Value = Enum->GetValueByNameString(Name);
    if (Value <= static_cast<int64>(EBreakerEnemyModifier::None)
        || Value >= static_cast<int64>(EBreakerEnemyModifier::Count))
    {
        return false;
    }
    Out = static_cast<EBreakerEnemyModifier>(Value);
    return true;
}

static FAutoConsoleCommandWithWorldAndArgs BreakerEnemyModifierProbeCommand(
    TEXT("Breaker.EnemyModifier"),
    TEXT("Grant one exact modifier to every live enemy and arm it for later spawns: <ModifierName> | clear."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World || Args.Num() < 1) return;
        if (Args[0].Equals(TEXT("clear"), ESearchCase::IgnoreCase))
        {
            BreakerEnemyModifierProbeName.Empty();
            UE_LOG(LogTemp, Display, TEXT("[BreakerEnemy] Breaker.EnemyModifier disarmed (live grants keep what they have)."));
            return;
        }
        EBreakerEnemyModifier Modifier;
        if (!BreakerEnemyResolveModifierProbe(Args[0], Modifier))
        {
            UE_LOG(LogTemp, Warning, TEXT("[BreakerEnemy] Breaker.EnemyModifier: '%s' names no modifier — nothing armed."), *Args[0]);
            return;
        }
        BreakerEnemyModifierProbeName = Args[0];
        int32 Driven = 0;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            if (It->IsDeadEnemy()) continue;
            if (It->ConfigureWithExactModifiers({ Modifier })) ++Driven;
        }
        UE_LOG(LogTemp, Display, TEXT("[BreakerEnemy] Breaker.EnemyModifier %s on %d enemies (armed for later spawns)."), *Args[0], Driven);
    }));

void ABreakerEnemy::DevDriveBodyPaintProbe(const FString& Mode)
{
    if (!HitReaction) return;
    if (Mode.Equals(TEXT("elite"), ESearchCase::IgnoreCase))
    {
        HitReaction->SetRank(EBreakerMonsterRank::Elite);
    }
    else if (Mode.Equals(TEXT("wash"), ESearchCase::IgnoreCase))
    {
        HitReaction->SetHealthRampEnabled(true);
        HitReaction->SetHealthFraction(0.15f);
    }
    else if (Mode.Equals(TEXT("flash"), ESearchCase::IgnoreCase))
    {
        // Re-struck faster than it decays, so the blink holds for the camera.
        GetWorldTimerManager().SetTimer(DevPaintProbeTimer,
            FTimerDelegate::CreateWeakLambda(this, [this]()
            {
                if (HitReaction) HitReaction->NotifyHit(false);
            }), 0.05f, true);
    }
    else if (Mode.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
    {
        GetWorldTimerManager().ClearTimer(DevPaintProbeTimer);
        HitReaction->SetRank(MonsterRank);
        HitReaction->SetHealthFraction(1.0f);
    }
}

ABreakerEnemy::ABreakerEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
    SetRootComponent(BodyCollision);
    BodyCollision->InitCapsuleSize(45.0f, 90.0f);
    BodyCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

    // NAV-1: the body is possessed by an enemy controller the moment it is
    // spawned, and moves through its mover. Both live in AI/.
    Mover = CreateDefaultSubobject<UBreakerEnemyMovementComponent>(TEXT("Movement"));
    AIControllerClass = ABreakerEnemyController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // Humanoid silhouette from basic shapes: torso, head, two arms, two legs.
    // Purely cosmetic — every piece is NoCollision and the capsule, hit box
    // and weak point keep doing all the collision work. Elites inherit the
    // actor scale multiplier, so an elite simply reads as a bigger humanoid.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BodySphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    // BodyVisual is the torso now (was the single cylinder body).
    BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
    BodyVisual->SetupAttachment(BodyCollision);
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 22.0f));
    BodyVisual->SetRelativeScale3D(FVector(0.55f, 0.36f, 0.78f));
    if (CubeMesh.Succeeded()) BodyVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(BodyVisual, FamilyPaint);

    HeadVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadVisual"));
    HeadVisual->SetupAttachment(BodyCollision);
    HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeadVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 78.0f));
    HeadVisual->SetRelativeScale3D(FVector(0.34f));
    if (BodySphereMesh.Succeeded()) HeadVisual->SetStaticMesh(BodySphereMesh.Object);
    ApplyEnemyBodyColor(HeadVisual, FamilyPaint);

    LeftArmVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftArmVisual"));
    LeftArmVisual->SetupAttachment(BodyCollision);
    LeftArmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftArmVisual->SetRelativeLocation(FVector(0.0f, -34.0f, 24.0f));
    LeftArmVisual->SetRelativeRotation(FRotator(0.0f, 0.0f, 12.0f));
    LeftArmVisual->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.62f));
    if (CubeMesh.Succeeded()) LeftArmVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(LeftArmVisual, FamilyPaint);

    RightArmVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightArmVisual"));
    RightArmVisual->SetupAttachment(BodyCollision);
    RightArmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightArmVisual->SetRelativeLocation(FVector(0.0f, 34.0f, 24.0f));
    RightArmVisual->SetRelativeRotation(FRotator(0.0f, 0.0f, -12.0f));
    RightArmVisual->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.62f));
    if (CubeMesh.Succeeded()) RightArmVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(RightArmVisual, FamilyPaint);

    LeftLegVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLegVisual"));
    LeftLegVisual->SetupAttachment(BodyCollision);
    LeftLegVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftLegVisual->SetRelativeLocation(FVector(0.0f, -14.0f, -50.0f));
    LeftLegVisual->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.80f));
    if (CubeMesh.Succeeded()) LeftLegVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(LeftLegVisual, FamilyPaint);

    RightLegVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightLegVisual"));
    RightLegVisual->SetupAttachment(BodyCollision);
    RightLegVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightLegVisual->SetRelativeLocation(FVector(0.0f, 14.0f, -50.0f));
    RightLegVisual->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.80f));
    if (CubeMesh.Succeeded()) RightLegVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(RightLegVisual, FamilyPaint);

    BodyHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyHitBox"));
    BodyHitBox->SetupAttachment(BodyCollision);
    BodyHitBox->SetBoxExtent(FVector(42.0f, 42.0f, 58.0f));
    BodyHitBox->SetRelativeLocation(FVector(0.0f, 0.0f, 4.0f));
    BodyHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    BodyHitBox->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    // Traveling rounds use the authored Projectile object channel. Keep the
    // movement capsule overlapping and the weapon weak-point query separate.
    BodyHitBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);

    WeakPoint = CreateDefaultSubobject<USphereComponent>(TEXT("WeakPoint"));
    WeakPoint->SetupAttachment(BodyCollision);
    WeakPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 78.0f));
    WeakPoint->SetSphereRadius(20.0f);
    WeakPoint->ComponentTags.Add(TEXT("WeakPoint"));
    WeakPoint->SetCollisionResponseToAllChannels(ECR_Ignore);
    WeakPoint->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);

    WeakPointVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeakPointVisual"));
    WeakPointVisual->SetupAttachment(WeakPoint);
    WeakPointVisual->SetRelativeScale3D(FVector(0.4f));
    WeakPointVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // The named body: hidden until BodyMeshAsset resolves in ApplyBodyMesh.
    // Created unconditionally so an editor-placed enemy can be given a mesh
    // by property alone, the same shape as ABreakerNPC's BodyMesh.
    NamedBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("NamedBody"));
    NamedBody->SetupAttachment(BodyCollision);
    NamedBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NamedBody->SetVisibility(false);

    // DEFAULTS ON, owner-ruled (2026-08-29, overriding this hook's first
    // default-off landing): the intake mechs are the enemy cast. Base melee —
    // Skitter included, the leap is this class — wears Stan; subclasses
    // override in their own constructors, and the RANGED class CLEARS it
    // because the Lattice is composed primitives by ruling. The paint-language
    // cost recorded here at the ruling is CLOSED: the reaction layer drives an
    // overlay on the named body (flash / rank / wash / burn, livery intact —
    // BreakerBodyPaint::ResolveOverlayStrength is the rule). Still open and
    // FIELD's: a skeletal crowd is dearer than a primitive one (8.35 vs 5.48
    // ms at 100 patrol, measured pre-cast) — re-measure, not a reason to hide.
    BodyMeshAsset = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/Stan/Stan.Stan"));
    BodyIdleAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/Stan/StanRobotArmature_Walk.StanRobotArmature_Walk"));
    BodyDeathAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/Stan/StanRobotArmature_Death.StanRobotArmature_Death"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded()) WeakPointVisual->SetStaticMesh(SphereMesh.Object);

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    Attributes = CreateDefaultSubobject<UBreakerAttributeSet>(TEXT("Attributes"));
    Combat = CreateDefaultSubobject<UBreakerCombatComponent>(TEXT("Combat"));
    Status = CreateDefaultSubobject<UBreakerStatusComponent>(TEXT("Status"));
    // Every enemy carries the modifier component and the overwhelmingly common
    // case is an EMPTY one — no halo, no clocks, no cost beyond the object.
    // Universal rather than opt-in because a modifier must be composable onto
    // ANY enemy (Encounter-Design §1.0: modifiers are a field of the taxonomy,
    // not a subclass).
    ModifierComponent = CreateDefaultSubobject<UBreakerEnemyModifierComponent>(TEXT("Modifiers"));
    HitReaction = CreateDefaultSubobject<UBreakerHitReactionComponent>(TEXT("HitReaction"));
}

void ABreakerEnemy::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    Combat->OnDeath.AddDynamic(this, &ThisClass::HandleDeath);
    Combat->OnDamageReceived.AddDynamic(this, &ThisClass::HandleDamageReceived);
    Combat->OnDamageTaken.AddUniqueDynamic(this, &ThisClass::HandleThreatDamage);
    if (UBreakerStatusComponent* Conditions = FindComponentByClass<UBreakerStatusComponent>())
    {
        Conditions->OnStatusApplied.AddUniqueDynamic(this, &ThisClass::HandleStatusApplied);
        Conditions->OnStatusExpired.AddUniqueDynamic(this, &ThisClass::HandleStatusExpired);
    }
    // The reaction layer paints these six, and since O128 it is the ONLY
    // thing that paints them. The family paint goes in before the first
    // registration so a part never lands on a default that a later layer has
    // to correct; registration is still here rather than in the constructor
    // because the parts are not final until now.
    if (HitReaction)
    {
        HitReaction->SetFamilyPaint(FamilyPaint);
        for (UStaticMeshComponent* Part : { BodyVisual.Get(), HeadVisual.Get(), LeftArmVisual.Get(),
            RightArmVisual.Get(), LeftLegVisual.Get(), RightLegVisual.Get() })
        {
            HitReaction->RegisterPart(Part);
        }
        HitReaction->OnDeathPresentationFinished.AddUObject(this, &ABreakerEnemy::HandleDeathPresentationFinished);
    }
    if (LeashOrigin.IsNearlyZero()) LeashOrigin = GetActorLocation();
    // Captured once, before anything can have scaled them, so Fleetfoot
    // multiplies a base rather than compounding on itself.
    if (BaseMoveSpeed < 0.0f) BaseMoveSpeed = MoveSpeed;
    if (BaseWeaveStrength < 0.0f) BaseWeaveStrength = WeaveStrength;
    if (WeakPointVisual) WeakPointBaseScale = WeakPointVisual->GetRelativeScale3D().X;
    PooledBaseScale = GetActorScale3D();
    // Health was the literal constant 220 here at every level until O27. It is
    // now a function of the area level this monster belongs to.
    ApplyChassis();

    // Editor-placed enemies carry BodyMeshAsset as a property; spawners that
    // set it after SpawnActor call ApplyBodyMesh themselves — ABreakerNPC's
    // contract, kept identical so there is one rule to know. An UNSET body
    // takes the armed Breaker.EnemyBody preview, so a command issued from
    // -ExecCmds reaches enemies the gym spawns after it ran.
    if (!BodyMeshAsset.IsValid() && !BreakerEnemyBodyPreviewMesh.IsEmpty())
    {
        BodyMeshAsset = FSoftObjectPath(BreakerEnemyBodyPreviewMesh);
        if (!BreakerEnemyBodyPreviewAnim.IsEmpty()) BodyIdleAnimation = FSoftObjectPath(BreakerEnemyBodyPreviewAnim);
    }
    ApplyBodyMesh();
    // The armed paint probe reaches later spawns the same way.
    if (!BreakerEnemyBodyPaintProbeMode.IsEmpty())
    {
        DevDriveBodyPaintProbe(BreakerEnemyBodyPaintProbeMode);
    }
    // The armed modifier probe reaches later spawns the same way the paint
    // probe does — -ExecCmds runs before the gym spawns anybody, so the
    // command's live sweep sees zero and THIS is where the arm actually
    // lands. End of BeginPlay deliberately: the ability system is
    // initialised above, which Warded's shield write requires. The game
    // mode's own carrier/elite configuration runs later and may overwrite
    // an instrumented grant — acceptable for a camera instrument.
    if (!BreakerEnemyModifierProbeName.IsEmpty())
    {
        EBreakerEnemyModifier ProbeModifier;
        if (BreakerEnemyResolveModifierProbe(BreakerEnemyModifierProbeName, ProbeModifier))
        {
            ConfigureWithExactModifiers({ ProbeModifier });
        }
    }
}

void ABreakerEnemy::ApplyBodyMesh()
{
    if (!NamedBody || !BodyMeshAsset.IsValid()) return;
    USkeletalMesh* Named = Cast<USkeletalMesh>(BodyMeshAsset.TryLoad());
    if (!Named)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BreakerEnemy] %s: body mesh %s did not resolve — primitive fallback."),
            *GetName(), *BodyMeshAsset.ToString());
        return;
    }
    NamedBody->SetSkeletalMesh(Named);
    const FBoxSphereBounds MeshBounds = Named->GetBounds();
    // The fit owns the WHOLE relative transform: scale, location AND the yaw
    // that turns the mesh's own forward onto the actor's +X. The mechs are
    // authored facing +Y (a Blender/glTF biped), so an identity rotation
    // stands every one of them looking to the actor's right while the actor
    // itself is turned correctly by SetActorRotation(Facing.Rotation()).
    // The forward is READ from the rig, never authored per mesh; a rig with
    // no bilateral pair is a recorded gap, and the fit falls back to
    // identity while the log says so. Every revive routes back through here,
    // so the same three channels are re-applied after a death pose.
    NamedBodyMeshForward = ReadBodyMeshForwardAxis(Named);
    if (NamedBodyMeshForward.IsNearlyZero())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BreakerEnemy] %s: body mesh %s offers no left/right bone pair to read a forward from — standing at identity yaw; it may face sideways."),
            *GetName(), *BodyMeshAsset.ToString());
    }
    const BreakerEnemyBody::FBreakerBodyFit Fit = BreakerEnemyBody::FitBodyToCapsule(
        MeshBounds.Origin, MeshBounds.BoxExtent, BodyCollision->GetUnscaledCapsuleHalfHeight(),
        NamedBodyMeshForward);
    NamedBody->SetRelativeScale3D(FVector(Fit.Scale));
    NamedBody->SetRelativeLocation(Fit.RelativeLocation);
    NamedBody->SetRelativeRotation(Fit.RelativeRotation);
    if (UAnimSequence* Idle = Cast<UAnimSequence>(BodyIdleAnimation.TryLoad()))
    {
        NamedBody->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        NamedBody->PlayAnimation(Idle, /*bLooping=*/true);
    }
    NamedBody->SetVisibility(true);
    for (UStaticMeshComponent* Part : { BodyVisual.Get(), HeadVisual.Get(), LeftArmVisual.Get(),
        RightArmVisual.Get(), LeftLegVisual.Get(), RightLegVisual.Get() })
    {
        if (Part) Part->SetVisibility(false);
    }
    // THE PAINT PORT: the named body joins the reaction layer as an OVERLAY —
    // hit flash, rank badge, health wash and death burn all read on the mech
    // at BreakerBodyPaint's overlay strength, livery untouched underneath.
    // This closes the recorded cost from the mech-cast ruling.
    if (HitReaction)
    {
        HitReaction->RegisterOverlayBody(NamedBody);
    }
    // THE WEAK POINT RIDES THE NAMED HEAD. Authored at the primitive
    // humanoid's head height, it floated beside every mech like a balloon —
    // all four mechs ship a Head bone (read from the FBX, not guessed), so
    // the collision sphere and its visual snap onto it and track the gait.
    // Deliberately a GAMEPLAY change (the shot lands where the eye aims,
    // which is the point of a weak spot); a body without the bone keeps the
    // authored position, and the primitive fallback never reaches this.
    static const FName BreakerHeadBoneName(TEXT("Head"));
    if (WeakPoint && NamedBody->GetBoneIndex(BreakerHeadBoneName) != INDEX_NONE)
    {
        WeakPoint->AttachToComponent(NamedBody,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale, BreakerHeadBoneName);
        WeakPoint->SetRelativeLocation(FVector::ZeroVector);
        // AND IT STOPS BEING A BALL AROUND THE HEAD. Owner: "find a way to fix
        // the critical spots to the models so they dont just have random
        // circles coming out of them". The SOCKET half of that was already
        // done — the sphere has ridden the Head bone and tracked the gait for
        // some time — so what he is looking at is the drawing.
        //
        // AND THE DRAWING IS NOT DECORATION: the engine sphere is 100 cm
        // across and the authored 0.4 makes it exactly 40, which is exactly
        // twice the collision sphere's 20 cm radius. The ball IS the hitbox,
        // drawn true. It protrudes because the HITBOX is wider than a mech's
        // head, not because the art is careless — so shrinking the visual, as
        // this first tried, would have made the picture lie about where a
        // weak-point shot actually lands.
        //
        // So the hitbox is untouched and the drawing changes shape: a ring at
        // the same 20 cm, in gold (O179 spends gold on the weak point), which
        // still says exactly how far the weak point reaches and lets the head
        // be seen through it. Same argument as the Volatile blast, which went
        // from a filled disc to a dashed ring for the same reason and reads.
        if (WeakPointVisual)
        {
            WeakPointVisual->SetVisibility(false);
            BuildWeakPointRing();
        }
    }
}

void ABreakerEnemy::BuildWeakPointRing()
{
    // A WORLD, FIRST. RegisterComponent on an actor that is not in a world
    // ensures inside the engine, and the body-facing fixture builds an enemy
    // outside one on purpose — it is asking a question about the ref pose, not
    // about the level. The ring is cosmetic, so having none there is correct.
    if (!WeakPoint || WeakPointRing || !GetWorld()) return;
    WeakPointRing = NewObject<UInstancedStaticMeshComponent>(this, TEXT("WeakPointRing"));
    if (!WeakPointRing) return;
    WeakPointRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeakPointRing->SetCastShadow(false);
    // REGISTER THEN ATTACH. SetupAttachment is constructor-only and silently
    // does nothing at runtime; this project has shipped an invisible component
    // that way once already.
    WeakPointRing->RegisterComponent();
    WeakPointRing->AttachToComponent(WeakPoint, FAttachmentTransformRules::KeepRelativeTransform);
    if (UStaticMesh* Segment = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
    {
        WeakPointRing->SetStaticMesh(Segment);
    }
    // THE LIT BASIC SHAPE AND NOT THE ADDITIVE GLOW, and that is measured
    // rather than preferred: a material must declare
    // MATUSAGE_InstancedStaticMeshes to draw on an instanced component, and
    // EngineMaterials/EmissiveMeshMaterial does not while BasicShapeMaterial
    // does. The pocket tear paid for that finding by photographing solid black.
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        if (UMaterialInstanceDynamic* Gold = UMaterialInstanceDynamic::Create(Base, WeakPointRing))
        {
            Gold->SetVectorParameterValue(TEXT("Color"), BreakerUI::Gold);
            WeakPointRing->SetMaterial(0, Gold);
        }
    }

    // The ring's radius is READ FROM THE HITBOX, never restated. That is the
    // whole point of changing the shape rather than the size: the drawing has
    // to keep telling the truth about how far the weak point reaches.
    const float Radius = WeakPoint->GetUnscaledSphereRadius();
    constexpr int32 Segments = 14;               // O2 PLACEHOLDER
    constexpr float ThicknessCm = 2.4f;          // O2 PLACEHOLDER
    constexpr float DashFraction = 0.62f;        // O2 PLACEHOLDER
    const float Chord = 2.0f * PI * Radius / Segments * DashFraction;
    for (int32 Index = 0; Index < Segments; ++Index)
    {
        const float Angle = 2.0f * PI * Index / Segments;
        const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
        const FRotator Facing(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f);
        // The engine cube is 100 cm on a side, so every extent is cm/100.
        WeakPointRing->AddInstance(FTransform(Facing, Offset,
            FVector(Chord / 100.0f, ThicknessCm / 100.0f, ThicknessCm / 100.0f)));
    }
}

FVector ABreakerEnemy::ReadBodyMeshForwardAxis(const USkeletalMesh* Mesh)
{
    if (!Mesh) return FVector::ZeroVector;
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    // A bone's component-space ref-pose position: its local pose composed up
    // through every parent to the root (child * parent in FTransform order).
    const auto ComponentSpaceRefPosition = [&Ref](int32 BoneIndex) -> FVector
    {
        const TArray<FTransform>& Pose = Ref.GetRefBonePose();
        FTransform Composed = FTransform::Identity;
        for (int32 Bone = BoneIndex; Bone != INDEX_NONE; Bone = Ref.GetParentIndex(Bone))
        {
            Composed = Composed * Pose[Bone];
        }
        return Composed.GetLocation();
    };
    // Ordered: the widest, most reliably horizontal pair first. Leela ships
    // no arms, so the legs are a real leg of the search, not a formality.
    static const FName BreakerBilateralPairs[][2] =
    {
        { FName(TEXT("Shoulder_L")), FName(TEXT("Shoulder_R")) },
        { FName(TEXT("UpperArm_L")), FName(TEXT("UpperArm_R")) },
        { FName(TEXT("UpperLeg_L")), FName(TEXT("UpperLeg_R")) },
        // The QuadShell's pair: its rig names every limb Front_/Back_, so it
        // matched none of the biped pairs, stood at identity and faced +Y.
        { FName(TEXT("Front_Shoulder_L")), FName(TEXT("Front_Shoulder_R")) },
    };
    for (const auto& Pair : BreakerBilateralPairs)
    {
        const int32 Left = Ref.FindBoneIndex(Pair[0]);
        const int32 Right = Ref.FindBoneIndex(Pair[1]);
        if (Left == INDEX_NONE || Right == INDEX_NONE) continue;
        const FVector Axis = BreakerEnemyBody::BodyForwardAxisFromBilateralBones(
            ComponentSpaceRefPosition(Left), ComponentSpaceRefPosition(Right));
        if (!Axis.IsNearlyZero()) return Axis;
    }
    return FVector::ZeroVector;
}

FVector ABreakerEnemy::GetNamedBodyWorldForward() const
{
    if (!NamedBody || !NamedBody->GetSkeletalMeshAsset())
    {
        return GetActorForwardVector();
    }
    // A rig that offered no pair stood at identity yaw, so its forward is
    // presumed +X in mesh space — the same presumption the fit made.
    const FVector MeshForward = NamedBodyMeshForward.IsNearlyZero() ? FVector::ForwardVector : NamedBodyMeshForward;
    return NamedBody->GetComponentRotation().RotateVector(MeshForward);
}

void ABreakerEnemy::ApplyChassis()
{
    AreaLevel = UBreakerMonsterChassisLibrary::ClampAreaLevel(AreaLevel);
    // O29. This clamp was 50 and it silently undid the whole endgame ruling:
    // GetDropItemLevel was opened to 120, and then this line put it straight
    // back. EnemyLevel is what GrantLoot hands to the drop pipeline, so NO DROP
    // IN THE SHIPPING GAME carried the deeper ladder - while the automation
    // suite stayed green, because the composition test exercises the library
    // function and never touches an actor.
    //
    // That is the same failure shape as the third jump: a rule that was correct
    // one layer up and dead where the game actually reads it.
    // ELITE OR BETTER. This read IsElite(), which is exactly the Elite rank, so
    // a ModifierBearing champion and a Boss both took the plain item level --
    // the two ranks ABOVE the one the bonus was written for got less than it.
    EnemyLevel = UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel)
        + (IsEliteOrBetter() ? FMath::Max(EliteDropItemLevelBonus, 0) : 0);
    EnemyLevel = FMath::Clamp(EnemyLevel, 1, UBreakerAffixLibrary::MaxItemLevel);

    if (Status)
    {
        Status->EntropyResistancePercent = Family == EBreakerEnemyFamily::Vestige ? BreakerEntropy::VestigeResistancePercent() : 0.0f;
        Status->VoidResistancePercent = Family == EBreakerEnemyFamily::Altered ? BreakerVoid::AlteredResistancePercent() : 0.0f;
    }
    AttackDamage = UBreakerMonsterChassisLibrary::GetMonsterDamage(
        AreaLevel, MonsterRank, Chassis, ArchetypeDamageMultiplier);

    // O194: the silhouette carries rank. An absolute write against the spawned
    // scale, never a multiply of the current one, so every promoter, the
    // demotion restore and a pool revive land the same size for the same rank.
    SetActorScale3D(PooledBaseScale * UBreakerMonsterChassisLibrary::GetRankScaleMultiplier(MonsterRank));

    if (Attributes)
    {
        // The modifier count step composes with the archetype ratio and the
        // rank row rather than replacing either: rank says what a
        // ModifierBearing enemy is worth, the archetype says what a Lattice is
        // worth, and the count step is Encounter-Design §1.1's "+0.35x per
        // modifier beyond the first". Three inputs, one product, no second
        // source of truth for any of them.
        // ApplyMaxHealth, not SetMaxHealth: the generated setter ensures with no
        // owning ASC, which made an enemy untestable outside a world. Live
        // behaviour is identical -- both run PreAttributeChange.
        Attributes->ApplyMaxHealth(UBreakerMonsterChassisLibrary::GetMonsterHealth(
            AreaLevel, MonsterRank, Chassis, ArchetypeHealthMultiplier * ModifierCountHealthMultiplier));
        if (Combat) Combat->RestoreVitals();
    }

    // The chassis just moved rank and refilled vitals, so both of this
    // class's paint layers are stale. Immediate, not next-tick: the deferral
    // existed to let the reaction layer's capture read a finished body, and
    // there is no capture left to protect. No registered body part is painted
    // in any subclass BeginPlay — the Skirmisher's muzzle and insignia, the
    // Boss's apparatus and the Warden's shield are separate meshes with one
    // writer each — so nothing races this.
    RefreshBodyPaint();
}

void ABreakerEnemy::DebugPoseHealthFraction(float Fraction)
{
    if (!Attributes) return;
    const float MaxHealth = Attributes->GetMaxHealth();
    if (MaxHealth <= 0.0f) return;
    Attributes->SetHealth(MaxHealth * FMath::Clamp(Fraction, 0.0f, 1.0f));
    RefreshBodyPaint();
}

void ABreakerEnemy::RefreshBodyPaint()
{
    // "WHICH ONE IS THE ELITE" IN A GLANCE (ruled): silhouette carries
    // FAMILY, colour carries RANK, and in a fight of eighty nobody reads a
    // bar edge. Since O129 colour also carries HEALTH, which is the axis the
    // readability measurement says is worth the most separation — the ramp
    // travels 45-62 dE76 where rank manages 10-14.
    //
    // Both layers are pushed, never painted. The component composes family,
    // rank, health and reaction forward and writes once; that is what makes a
    // demotion, a pooled revive and a flash-in-flight all free.
    if (!HitReaction) return;
    HitReaction->SetFamilyPaint(FamilyPaint);
    HitReaction->SetRank(MonsterRank);
    // The ramp is the ENEMY's axis. The dummy leaves it off (its health is a
    // test fixture, not a threat read), which is why this is a flag and not
    // an unconditional layer.
    HitReaction->SetHealthRampEnabled(true);
    const float MaxHealth = Attributes ? Attributes->GetMaxHealth() : 0.0f;
    HitReaction->SetHealthFraction(MaxHealth > 0.0f ? Attributes->GetHealth() / MaxHealth : 1.0f);
}

float ABreakerEnemy::GetMonsterMaxHealth() const
{
    return Attributes ? Attributes->GetMaxHealth() : 0.0f;
}

bool ABreakerEnemy::UsesCoverDiscipline() const
{
    return UBreakerEnemyFamilyLibrary::StageUsesCover(Family, SeveranceStage);
}

bool ABreakerEnemy::FlinchesWhenHit() const
{
    return UBreakerEnemyFamilyLibrary::StageFlinches(Family, SeveranceStage);
}

int32 ABreakerEnemy::ConfigureWithModifiers(int32 Seed)
{
    if (!ModifierComponent) return 0;
    // The family gate is passed down, so a Vestige never rolls a tactical
    // modifier and an Altered never rolls an alien-body one.
    const TArray<EBreakerEnemyModifier> Granted = ModifierComponent->RollAndApplyModifiers(Seed, Family);
    if (!Granted.IsEmpty())
    {
        MonsterRank = UBreakerEnemyModifierLibrary::GetRankForModifierCount(Granted.Num());
        ModifierCountHealthMultiplier = UBreakerEnemyModifierLibrary::GetModifierCountHealthMultiplier(
            Granted.Num(), ModifierComponent->Params);
        ApplyChassis();
        // The ward is sized off max health, so it has to be re-derived AFTER
        // the chassis rebuild that the rank promotion just caused.
        ModifierComponent->SetModifiers(Granted);
        StateLabel = TEXT("PATROL");
    }
    return Granted.Num();
}

bool ABreakerEnemy::ConfigureWithExactModifiers(const TArray<EBreakerEnemyModifier>& InModifiers)
{
    if (!ModifierComponent || !ModifierComponent->SetModifiers(InModifiers)) return false;
    MonsterRank = UBreakerEnemyModifierLibrary::GetRankForModifierCount(InModifiers.Num());
    ModifierCountHealthMultiplier = UBreakerEnemyModifierLibrary::GetModifierCountHealthMultiplier(
        InModifiers.Num(), ModifierComponent->Params);
    ApplyChassis();
    ModifierComponent->SetModifiers(InModifiers);
    return true;
}

void ABreakerEnemy::SetModifierShield(float Amount)
{
    if (!Attributes) return;
    const float Clamped = FMath::Max(0.0f, Amount);
    Attributes->ApplyMaxShield(Clamped);
    Attributes->SetShield(Clamped);
}

void ABreakerEnemy::AddModifierShield(float Amount)
{
    if (!Attributes || Amount <= 0.0f) return;
    Attributes->SetShield(FMath::Min(Attributes->GetMaxShield(), Attributes->GetShield() + Amount));
}

void ABreakerEnemy::ApplyModifierMovementProfile(float SpeedMultiplier, float WeaveStrengthOverride)
{
    if (BaseMoveSpeed < 0.0f) BaseMoveSpeed = MoveSpeed;
    if (BaseWeaveStrength < 0.0f) BaseWeaveStrength = WeaveStrength;
    MoveSpeed = BaseMoveSpeed * FMath::Max(0.0f, SpeedMultiplier);
    WeaveStrength = WeaveStrengthOverride >= 0.0f ? WeaveStrengthOverride : BaseWeaveStrength;
}

void ABreakerEnemy::ApplyModifierSlowToTarget(float SpeedMultiplier, float Duration)
{
    AActor* Target = ModifierTrackedTarget.Get();
    if (!Target || Duration <= 0.0f) return;
    // Through the movement layer's own keyed push/pop, so the slow composes
    // with everything else that touches speed and expires on its own clock
    // instead of needing this enemy to survive long enough to remove it.
    if (UBreakerCharacterMovementComponent* Movement =
        Target->FindComponentByClass<UBreakerCharacterMovementComponent>())
    {
        Movement->PushSpeedMultiplier(TEXT("Modifier.Anchored"), SpeedMultiplier, Duration);
    }
}

void ABreakerEnemy::SetModifierUntargetable(bool bUntargetable)
{
    const ECollisionEnabled::Type Mode = bUntargetable
        ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly;
    if (BodyHitBox) BodyHitBox->SetCollisionEnabled(Mode);
    if (WeakPoint) WeakPoint->SetCollisionEnabled(Mode);
    SetBodyVisible(!bUntargetable);
}

void ABreakerEnemy::ConfigureAsSplitCopy(int32 InAreaLevel, float HealthFraction)
{
    bDropsLoot = false;
    bRespawns = false;
    // A copy that chain-detonates would make Splitting a pack-clearing gift.
    bExplodesOnDeath = false;
    // The copy is a smaller body for good: the shrink goes into the base the
    // chassis scales from, so every later ApplyChassis keeps it.
    PooledBaseScale *= 0.7f; // O2 PLACEHOLDER
    // Rank Trash, no modifiers: this is what stops a split from splitting.
    MonsterRank = EBreakerMonsterRank::Trash;
    ModifierCountHealthMultiplier = 1.0f;
    SetAreaLevel(InAreaLevel);
    if (Attributes)
    {
        Attributes->SetHealth(Attributes->GetMaxHealth() * FMath::Clamp(HealthFraction, 0.01f, 1.0f));
    }
    StateLabel = TEXT("SPLIT");
}

void ABreakerEnemy::SetAreaLevel(int32 NewAreaLevel)
{
    AreaLevel = UBreakerMonsterChassisLibrary::ClampAreaLevel(NewAreaLevel);
    ApplyChassis();
}

void ABreakerEnemy::SetMonsterRank(EBreakerMonsterRank NewRank)
{
    MonsterRank = NewRank;
    ApplyChassis();
}

void ABreakerEnemy::ConfigureWave(int32 NewAreaLevel)
{
    bRespawns = false;
    SetAreaLevel(NewAreaLevel);
}

UAbilitySystemComponent* ABreakerEnemy::GetAbilitySystemComponent() const { return AbilitySystem; }

void ABreakerEnemy::ConfigureEncounter(const FVector& NewLeashOrigin, float NewPatrolPhase)
{
    ClearThreat();
    LeashOrigin = NewLeashOrigin;
    PatrolPhase = NewPatrolPhase;
}

void ABreakerEnemy::ConfigureElite()
{
    // The elite's health and damage numbers used to live right here — a
    // hardcoded 440 health and a *= 1.5f damage, a second source of truth
    // sitting alongside the base chassis' hardcoded 220. Both are gone: rank
    // is now a row in the chassis rank table, and ApplyChassis composes it.
    // What stays here is what is genuinely elite BEHAVIOUR: the slower
    // implacable advance. The bigger silhouette is the rank's, not this
    // function's — ApplyChassis writes it from the rank scale row (O194).
    MonsterRank = EBreakerMonsterRank::Elite;
    MoveSpeed *= 0.85f;
    ApplyChassis();
    StateLabel = TEXT("PATROL");
}

FString ABreakerEnemy::GetEnemyModifierBanner() const
{
    // The announcement half of GetEnemyStateLabel, split out so a HUD pass can
    // print it WITHOUT the state line. Both are needed and they have different
    // budgets: Encounter-Design §1.2 requires a modifier to be identifiable
    // within 1.5s of the enemy entering view, so this always prints — whereas
    // the state line is restating a telegraph the world already shows, which
    // is what made six enemies' labels overlap into mush.
    //
    // The ALTERED family banner rides here rather than with the state for the
    // same reason: Assets/story-source.md §1.5's severance stage is meant to be readable
    // on sight, and it is the exception rather than the baseline, so it is an
    // announcement and not a status.
    TArray<FString> Parts;
    if (Family == EBreakerEnemyFamily::Altered)
    {
        Parts.Add(UBreakerEnemyFamilyLibrary::GetFamilyBanner(Family, SeveranceStage));
    }
    if (ModifierComponent)
    {
        const FString Banner = ModifierComponent->GetBanner();
        if (!Banner.IsEmpty()) Parts.Add(Banner);
    }
    return FString::Join(Parts, TEXT("\n"));
}

FString ABreakerEnemy::GetEnemyStateLabel() const
{
    // The modifier banner rides on the state label the HUD already prints over
    // every enemy's head. That is deliberate: Encounter-Design §1.2's first
    // acceptance test is that a modifier is identifiable within 1.5s of the
    // enemy entering view, and an unannounced modifier is an unfair death
    // rather than a challenge. Reusing the existing readout means the
    // announcement cannot be forgotten by a UI pass that does not know about
    // modifiers, and an unmodified enemy's label is byte-identical to before.
    // THE STATE, AND ONLY THE STATE. The modifier and family banners used to be
    // prefixed here, and moving them to GetEnemyModifierBanner is not merely a
    // presentation split — the concatenation was a live BUG well outside the
    // HUD. BreakerGameMode's wave alive-count asked `GetEnemyStateLabel() !=
    // "DEAD"`, so a dead enemy carrying any modifier answered
    // "WARDED | VOLATILE\nDEAD", compared unequal, and was counted ALIVE
    // FOREVER — a wave containing a modifier-bearing enemy could never clear.
    // That call site now asks IsDeadEnemy() instead, which is the question it
    // was actually trying to ask, but this function staying honest about its
    // own name is what stops the next caller repeating it.
    return StateLabel;
}

float ABreakerEnemy::GetBodyCapsuleRadius() const
{
    return BodyCollision ? BodyCollision->GetScaledCapsuleRadius() : 0.0f;
}

FVector ABreakerEnemy::ComputeCappedFacing(const FVector& CurrentForward, const FVector& DesiredDirection,
    float MaxDegreesPerSecond, float DeltaSeconds)
{
    const FVector Current = CurrentForward.GetSafeNormal2D();
    const FVector Desired = DesiredDirection.GetSafeNormal2D();
    if (Desired.IsNearlyZero()) return Current;
    if (Current.IsNearlyZero()) return Desired;

    const float DotClamped = FMath::Clamp(FVector::DotProduct(Current, Desired), -1.0f, 1.0f);
    const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(DotClamped));
    const float MaxStepDegrees = FMath::Max(0.0f, MaxDegreesPerSecond) * FMath::Max(0.0f, DeltaSeconds);
    if (AngleDegrees <= MaxStepDegrees || AngleDegrees <= KINDA_SMALL_NUMBER)
    {
        return Desired;
    }

    // Signed by the Z component of the cross product so the body turns the
    // SHORT way toward its target rather than always pivoting one direction.
    const float Cross = Current.X * Desired.Y - Current.Y * Desired.X;
    const float SignedStepDegrees = (Cross >= 0.0f ? 1.0f : -1.0f) * MaxStepDegrees;
    return Current.RotateAngleAxis(SignedStepDegrees, FVector::UpVector);
}

bool ABreakerEnemy::IsEligibleThreatTarget(const AActor* Candidate) const
{
    if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed() || Candidate->GetWorld() != GetWorld()
        || (!Candidate->IsA<ABreakerCharacter>() && !Candidate->IsA<ABreakerDeployable>())) return false;
    const auto* TargetCombat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
    if (!TargetCombat || TargetCombat->IsDead()) return false;
    if (const auto* Deployable = Cast<ABreakerDeployable>(Candidate))
    {
        if (Deployable->IsHidden()) return false;
        const auto* DeployableOwner = Deployable->GetOwningCharacter();
        const auto* OwnerCombat = IsValid(DeployableOwner) ? DeployableOwner->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!OwnerCombat || OwnerCombat->IsDead() || Deployable->GetRemainingLifetime() <= 0) return false;
    }
    const auto* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr;
    return !GameMode || !GameMode->IsInSafeZone(Candidate->GetActorLocation());
}

void ABreakerEnemy::ClearThreat()
{
    ThreatLedger.Reset(); CurrentThreatTarget.Reset(); CommittedAttackTarget.Reset();
    ProvokedTarget.Reset(); ProvokeEndsAt = 0; bProvokeEndsOnForeignDamage = false;
}

void ABreakerEnemy::HandleStatusApplied(const FBreakerActiveStatus& Applied)
{
    // The NEWEST condition wins the body. What just landed is the thing the
    // player did most recently and the thing they are waiting to see work.
    if (HitReaction && Applied.Spec.StatusTag.IsValid())
    {
        HitReaction->SetStatusWash(Applied.Spec.StatusTag);
    }
}

void ABreakerEnemy::HandleStatusExpired(const FBreakerActiveStatus& Expired)
{
    RefreshStatusWash();
}

void ABreakerEnemy::RefreshStatusWash()
{
    if (!HitReaction) return;
    const UBreakerStatusComponent* Conditions = FindComponentByClass<UBreakerStatusComponent>();
    // Whatever is still on the body, or nothing. Reading the list back rather
    // than counting applications: a status can leave by expiring, by being
    // consumed by a detonation, or by the body being cleaned, and only the
    // list knows about all three.
    const FGameplayTag Wash = Conditions && Conditions->GetActiveStatuses().Num() > 0
        ? Conditions->GetActiveStatuses()[0].Spec.StatusTag : FGameplayTag();
    HitReaction->SetStatusWash(Wash);
}

void ABreakerEnemy::HandleThreatDamage(const FBreakerHitContext& Hit)
{
    if (!HasAuthority() || bDead || (Combat && Combat->IsDead()) || Hit.Target != this) return;

    // THE BODY ANSWERS THE SHOT WITH MOTION. Owner: "enemies should stagger or
    // flinch when shot". Fired HERE, above every threat rule below it, because
    // a hit that earns no threat still landed — a hit from something this body
    // will not chase must still look like a hit.
    //
    // FlinchesWhenHit is the family's own answer and is already ruled: the
    // late-stage Altered does not flinch, and this does not overrule it.
    if (HitReaction && FlinchesWhenHit()
        && Hit.Result.HealthDamage + Hit.Result.ShieldDamage > 0.0f)
    {
        const AActor* From = Hit.Instigator ? Hit.Instigator.Get() : Hit.ThreatSource.Get();
        HitReaction->NotifyFlinch(From ? GetActorLocation() - From->GetActorLocation() : FVector::ZeroVector,
            Hit.Result.bWeakPoint);
    }

    if (bProvokeEndsOnForeignDamage && ProvokedTarget.IsValid() && Hit.Instigator != ProvokedTarget.Get()
        && Hit.Result.HealthDamage + Hit.Result.ShieldDamage > 0) ProvokedTarget.Reset();
    AActor* Source = Hit.ThreatSource.Get();
    if (!IsEligibleThreatTarget(Source)) return;
    AActor* StatOwner = Source;
    if (const auto* Deployable = Cast<ABreakerDeployable>(Source)) StatOwner = Deployable->GetOwningCharacter();
    const bool bIgnoreMe = BreakerEnemyOwnsIgnoreMe(StatOwner);
    if (bIgnoreMe && Source == StatOwner) return;
    const auto* Progression = StatOwner ? StatOwner->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    const float Multiplier = Progression ? Progression->GetNodeStats().ThreatGeneratedMultiplier : 1.0f;
    const float Earned = BreakerEnemyThreat::Earned(Hit.Result.HealthDamage, Hit.Result.ShieldDamage)
        * (FMath::IsFinite(Multiplier) ? FMath::Max(0.0f, Multiplier) : 1.0f)
        * (bIgnoreMe ? 2.0f : 1.0f); // O2 PLACEHOLDER: owned deployables only.
    // Ignore Me's ally doubling still needs an authoritative party relation.
    // Another spawned character is not proof of membership; do not infer it.
    if (Earned <= 0) return;
    float& Score = ThreatLedger.FindOrAdd(Source);
    Score = FMath::Min(static_cast<double>(Score) + Earned, static_cast<double>(MAX_flt));
}

void ABreakerEnemy::ApplyProvokeThreat(AActor* Source, float Amount, float Duration, bool bCancelOnForeignDamage)
{
    if (!HasAuthority() || bDead || !Combat || Combat->IsDead() || !IsEligibleThreatTarget(Source)
        || !FMath::IsFinite(Amount) || Amount < 0 || !FMath::IsFinite(Duration) || Duration <= 0) return;
    const auto* Progression = Source->FindComponentByClass<UBreakerProgressionComponent>();
    const float Multiplier = Progression ? Progression->GetNodeStats().ThreatGeneratedMultiplier : 1.f;
    if (!BreakerEnemyOwnsIgnoreMe(Source))
    {
        float& Score = ThreatLedger.FindOrAdd(Source);
        Score = FMath::Min(static_cast<double>(MAX_flt), static_cast<double>(Score)
            + Amount * (FMath::IsFinite(Multiplier) ? FMath::Max(0.f,Multiplier) : 1.f));
    }
    ProvokedTarget = Source; ProvokeEndsAt = GetWorld()->GetTimeSeconds() + Duration;
    bProvokeEndsOnForeignDamage = bCancelOnForeignDamage;
}
AActor* ABreakerEnemy::SelectThreatTarget()
{
    if (IsEligibleThreatTarget(ProvokedTarget.Get()) && GetWorld()->GetTimeSeconds() < ProvokeEndsAt
        && FVector::DistSquared2D(GetActorLocation(),ProvokedTarget->GetActorLocation()) <= FMath::Square(DetectionRange))
    { CurrentThreatTarget = ProvokedTarget; return ProvokedTarget.Get(); }
    ProvokedTarget.Reset();
    AActor* Best = nullptr;
    float BestScore = 0, BestDistance = TNumericLimits<float>::Max();
    for (auto It = ThreatLedger.CreateIterator(); It; ++It)
    {
        AActor* Candidate = It.Key().Get();
        if (!IsEligibleThreatTarget(Candidate)) { It.RemoveCurrent(); continue; }
        // Existing personal credit is inactive while the rule is owned; it is
        // not rewritten into credit for a deployable or erased by a respec.
        if (Candidate->IsA<ABreakerCharacter>() && BreakerEnemyOwnsIgnoreMe(Candidate)) continue;
        const float Distance = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
        if (Distance > FMath::Square(DetectionRange)) continue;
        if (It.Value() > 0 && (!Best || BreakerEnemyThreat::Prefer(It.Value(), Candidate == CurrentThreatTarget.Get(), Distance,
            BestScore, Best == CurrentThreatTarget.Get(), BestDistance)))
        { Best = Candidate; BestScore = It.Value(); BestDistance = Distance; }
    }
    if (!Best)
    {
        for (TActorIterator<ABreakerCharacter> It(GetWorld()); It; ++It)
        {
            if (!IsEligibleThreatTarget(*It)) continue;
            const float Distance = FVector::DistSquared2D(GetActorLocation(), It->GetActorLocation());
            if (Distance <= FMath::Square(DetectionRange) && Distance < BestDistance)
            { Best = *It; BestDistance = Distance; }
        }
        // No eligible local combatants means this encounter's threat is over.
        if (!Best) ClearThreat();
    }
    CurrentThreatTarget = Best;
    CommittedAttackTarget.Reset();
    return Best;
}

void ABreakerEnemy::PlayBodyHit()
{
    if (!NamedBody || !NamedBody->IsVisible() || bDead || (Combat && Combat->IsDead())) return;
    UAnimSequence* HitAnim = Cast<UAnimSequence>(BodyHitAnimation.TryLoad());
    if (!HitAnim) return;
    UWorld* World = GetWorld();
    if (!World) return;
    NamedBody->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    NamedBody->PlayAnimation(HitAnim, /*bLooping=*/false);
    bBodyHitPlaying = true;
    // Back to the gait when the sequence ends — a hit that left the body
    // frozen on its last frame would read as a second, worse bug. A second
    // hit inside the first simply restarts the clock.
    World->GetTimerManager().SetTimer(BodyHitTimer, this, &ABreakerEnemy::RestoreBodyGait,
        FMath::Max(0.05f, HitAnim->GetPlayLength()), false);
}

void ABreakerEnemy::RestoreBodyGait()
{
    bBodyHitPlaying = false;
    if (!NamedBody || !NamedBody->IsVisible() || bDead || (Combat && Combat->IsDead())) return;
    UAnimSequence* Gait = nullptr;
    if (bBodyRunning) Gait = Cast<UAnimSequence>(BodyRunAnimation.TryLoad());
    if (!Gait) Gait = Cast<UAnimSequence>(BodyIdleAnimation.TryLoad());
    if (!Gait) return;
    NamedBody->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    NamedBody->PlayAnimation(Gait, /*bLooping=*/true);
}

void ABreakerEnemy::UpdateBodyGait()
{
    // A rig with no Run has one gait and nothing to switch; the mechs' "idle"
    // IS their walk, which is why they never looked wrong standing still and
    // never looked right doing it either.
    if (!NamedBody || !NamedBody->IsVisible() || !BodyRunAnimation.IsValid()) return;
    if (bBodyHitPlaying || bDead || (Combat && Combat->IsDead())) return;
    constexpr float MovingSpeed = 40.0f;   // O2 PLACEHOLDER: under this the body is standing
    const bool bMoving = GetVelocity().SizeSquared2D() > MovingSpeed * MovingSpeed;
    if (bMoving == bBodyRunning) return;
    bBodyRunning = bMoving;
    RestoreBodyGait();
}

void ABreakerEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateBodyGait();
    // The cosmetic death beat advances in the reaction COMPONENT's own tick
    // now — client-legal presentation, so a dead server-side pawn still
    // finishes its crumple with no line here.
    if (!HasAuthority() || !GetWorld()) return;
    if (bDead)
    {
        // A Volatile corpse still has a fuse to run, and its strobe is the only
        // warning the player gets. Everything else about a dead enemy stops.
        if (ModifierComponent) ModifierComponent->AdvanceModifiers(DeltaSeconds);
        return;
    }

    if (Combat && Combat->IsStaggered()) { StateLabel = TEXT("STAGGERED"); return; }
    // O274: A BODY EMERGES BEFORE IT HUNTS. While the emergence clock runs
    // the threat selector is not consulted at all — not "consulted and
    // ignored" — so a player standing on the tear is not a target on frame
    // one, no attack target is committed and nothing below can fire. With no
    // player the distance test fails and the tick takes the patrol branch,
    // which is the walk to the post on the shipped leash. The clock ends in
    // EndEmergenceWindow and the next tick engages as it always did.
    const bool bEmergingNow = IsEmerging();
    if (bEmergingNow) { CurrentThreatTarget.Reset(); CommittedAttackTarget.Reset(); }
    AActor* NearestPlayer = bEmergingNow ? nullptr : SelectThreatTarget();
    const float NearestDistanceSq = NearestPlayer
        ? FVector::DistSquared2D(GetActorLocation(), NearestPlayer->GetActorLocation())
        : TNumericLimits<float>::Max();
    const ABreakerGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakerGameMode>();
    // The modifier layer's only view of the world: a bare AActor* whose
    // POSITION it may read. It never learns what class this is, let alone what
    // level or gear it carries (O27).
    ModifierTrackedTarget = NearestPlayer;
    if (ModifierComponent)
    {
        ModifierComponent->SetTrackedTarget(NearestPlayer);
        ModifierComponent->AdvanceModifiers(DeltaSeconds);
    }

    const float Distance = FMath::Sqrt(NearestDistanceSq);
    FVector DesiredDirection = FVector::ZeroVector;
    // Speed multiplier for this frame. 1.0 = the old constant walk.
    float SpeedScale = 1.0f;
    DesiredFacing = FVector::ZeroVector;
    bHasPathGoal = false;
    if (NearestPlayer && Distance <= DetectionRange)
    {
        TickEngagedBehaviour(NearestPlayer, Distance, DeltaSeconds, DesiredDirection, SpeedScale);
    }
    else
    {
        PatrolPhase += DeltaSeconds * 0.7f;
        const FVector PatrolTarget = LeashOrigin + FVector(0.0f, FMath::Sin(PatrolPhase) * 350.0f, 0.0f);
        // Within one capsule radius of the target the body is THERE: it hands
        // the mover a zero direction and holds its facing. The target drifts
        // slower than the body walks, so a body that kept steering would sit
        // on the target and flip its direction every time it overshot by a
        // step; the radius is the arrival threshold, the same number the
        // capsule already carries.
        const FVector ToTarget = PatrolTarget - GetActorLocation();
        DesiredDirection = ToTarget.Size2D() <= BodyCollision->GetScaledCapsuleRadius()
            ? FVector::ZeroVector
            : ToTarget.GetSafeNormal2D();
        // The same walk under two names: EMERGING is the leash walk out of the
        // tear (O274), PATROL is the leash walk once the body may hunt.
        StateLabel = bEmergingNow ? TEXT("EMERGING") : TEXT("PATROL");
    }

    // Facing is decided before movement so an archetype that strafes sideways
    // while aiming at the player still points its muzzle at the player. The
    // turn is rate-capped here, once, for every archetype: whatever the
    // behaviour asked for, the body rotates toward it by at most
    // MaxTurnRateDegreesPerSecond this frame. The lunge and skitter lock a
    // DIRECTION, not a facing, and are not touched by this.
    //
    // NAV-4: which way, and how fast while turning, are the mover's rule
    // (AI/BreakerLocomotionMath). A body on a path faces the leg the follower
    // is walking, not the chase line this function handed over; the mode read
    // is last frame's, because this frame's Drive has not run yet, and the
    // leg is the follower's own segment, not a velocity the speed scale
    // below could erase. Then TURN BEFORE WALK: the cap
    // rotates the body 1.7 degrees a frame but the mover used to take the
    // full direction the same frame, so a body spawned at zero yaw sprinted
    // sideways while it came round. The speed scale is now multiplied by the
    // cosine of the turn still owed, against the facing the body WANTS, not
    // the capped one it gets this frame: a body 90 degrees off stands and
    // turns; a retreat and a strafe still face the player (they set
    // DesiredFacing explicitly), so they are unaffected. A weaving melee
    // body asks for no facing and faces where it steers, weave included:
    // the weave is inside NAV-1's cone, so the turn it owes each half-cycle
    // is cos 31 at worst, never a stall. The committed lunge locks ToPlayer
    // at the end of a wind-up the body spent facing the player, so its owed
    // turn is whatever the player strafed during the tell — a few degrees,
    // not a stall.
    const FVector CurrentForward = GetActorForwardVector();
    const FVector WantedFacing = BreakerLocomotionMath::FacingFor(
        Mover ? Mover->GetLastMode() : EBreakerLocomotionMode::Steer,
        DesiredFacing, DesiredDirection, Mover ? Mover->GetPathHeading() : FVector::ZeroVector);
    SpeedScale *= BreakerLocomotionMath::AlignedSpeedScale(CurrentForward, WantedFacing);
    const FVector Facing = ComputeCappedFacing(CurrentForward, WantedFacing, MaxTurnRateDegreesPerSecond, DeltaSeconds);
    if (!Facing.IsNearlyZero()) SetActorRotation(Facing.Rotation());

    // The safe-zone edge is decided here, on the behaviour's own step, before
    // the mover sees anything: a held body hands the mover a zero direction
    // rather than a return, so it still decelerates and still ground-snaps.
    if (!DesiredDirection.IsNearlyZero() && GameMode)
    {
        const FVector NextLocation = GetActorLocation() + DesiredDirection * MoveSpeed * SpeedScale * DeltaSeconds;
        if (GameMode->IsInSafeZone(NextLocation))
        {
            StateLabel = TEXT("HELD");
            DesiredDirection = FVector::ZeroVector;
        }
    }

    // NAV-1: the mover (AI/BreakerEnemyMovementComponent) steers along the
    // direction, or paths to the player when the straight line is blocked,
    // and owns the ground snap that used to close this function. This is the
    // only line in Combat/ that moves an enemy.
    if (Mover)
    {
        Mover->Drive(DesiredDirection, SpeedScale, NearestPlayer, Distance, AttackRange, MoveSpeed,
            bHasPathGoal, PathGoal);
    }

    // THE GAIT FOLLOWS THE GROUND SPEED, AND ITS SIGN. The named body's walk
    // plays as a single-node loop; its rate is the body's velocity along its
    // own forward over MoveSpeed, so a held body stands still instead of
    // treadmilling, a closing body strides faster instead of sliding, and a
    // body backing off with its face on the player (the Retreat band) plays
    // its Walk in reverse instead of moonwalking — the rate used to be the
    // unsigned planar speed, so a body walking backwards at 0.6 strode
    // forward at 0.6. A single-node sequence advances by rate x time and
    // wraps when looping, so a negative rate is a legal reverse. The hit
    // one-shot is left at the rate it was started with: it is not a gait,
    // and reversing a flinch would be a new wrong.
    // O2 PLACEHOLDER: the reference speed is MoveSpeed until each sequence's
    // stride is measured; a sequence whose authored stride does not cover
    // MoveSpeed per cycle still slides by the ratio.
    // The Ranged archetype ships no sequence, so it has no gait to drive.
    // STEER adds input to the integrating mover. StopChase aborts only an
    // active path; repeated steering frames retain velocity. A path-to-steer
    // transition may stop the path follower once before steering resumes.
    if (Mover && NamedBody && MoveSpeed > 0.0f && NamedBody->IsPlaying() && !bBodyHitPlaying)
    {
        constexpr float MaxRate = 2.0f;   // O2 PLACEHOLDER
        const FVector PlanarVelocity(Mover->Velocity.X, Mover->Velocity.Y, 0.0);
        const float AlongForward = static_cast<float>(
            FVector::DotProduct(PlanarVelocity, GetActorForwardVector().GetSafeNormal2D()));
        NamedBody->SetPlayRate(FMath::Clamp(AlongForward / MoveSpeed, -MaxRate, MaxRate));
    }
}


void ABreakerEnemy::ApplyArrivalRing(AActor* Player, float Distance, const FVector& ToPlayer,
    FVector& OutDirection, float& OutSpeedScale, FVector& OutApproach)
{
    if (!Player) return;
    ArrivalBand = UBreakerRangedBehaviorLibrary::ClassifyBand(Distance,
        AttackRange * ArrivalInnerRatio, AttackRange, ArrivalHysteresisCm, ArrivalBand);
    // THE ARRIVAL ANGLE. Advance walks to a point ON the ring, not to the
    // player: the target-to-body bearing rotated by this body's signed
    // ArrivalOffsetDeg, handed to the mover through the goal channel so a
    // blocked line still paths to it. Two closers seeded apart split left and
    // right and arrive from two angles instead of stacking on one point. The
    // weave below folds onto this approach vector. A subclass with no contact
    // range has no ring and closes straight on the player.
    OutApproach = ToPlayer;
    if (ArrivalBand == EBreakerRangedBand::Advance && AttackRange > 0.0f)
    {
        const FVector Goal = BreakerLocomotionMath::ArrivalGoal(Player->GetActorLocation(), GetActorLocation(),
            AttackRange, BreakerLocomotionMath::ArrivalSign(PatrolPhase), BreakerLocomotionMath::ArrivalOffsetDeg);
        PathGoal = Goal;
        bHasPathGoal = true;
        const FVector ToGoal = (Goal - GetActorLocation()).GetSafeNormal2D();
        if (!ToGoal.IsNearlyZero()) OutApproach = ToGoal;
        OutDirection = OutApproach;
    }
    if (ArrivalBand != EBreakerRangedBand::Advance)
    {
        const float RadialSign = UBreakerRangedBehaviorLibrary::GetBandRadialSign(ArrivalBand);
        OutDirection = ToPlayer * RadialSign;
        OutSpeedScale = UBreakerRangedBehaviorLibrary::GetBandSpeedScale(
            ArrivalBand, 1.0f, ArrivalRetreatSpeedScale, 0.0f);
        // Facing is set explicitly because a held body has no movement
        // direction to derive one from, and an enemy attacking the player
        // while facing where it last walked is worse than the stack was.
        DesiredFacing = ToPlayer;
        StateLabel = ArrivalBand == EBreakerRangedBand::Hold ? TEXT("ATTACK") : TEXT("BACK OFF");
    }
}
void ABreakerEnemy::TickEngagedBehaviour(AActor* Player, float Distance, float DeltaSeconds,
    FVector& OutDirection, float& OutSpeedScale)
{
    // The melee chase, in three gears. Extracted verbatim from Tick so a
    // ranged archetype can replace the whole decision without forking the
    // shared target-selection, safe-zone, and ground-snap code around it.
    if (!Player || !GetWorld()) return;
    const double Now = GetWorld()->GetTimeSeconds();

    const FVector ToPlayer = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    OutDirection = ToPlayer;
    StateLabel = Distance <= AttackRange ? TEXT("ATTACK") : TEXT("CHASE");
    if (Distance <= AttackRange) { CommitAttackTarget(Player); PerformAttack(Player); }

    // THE ARRIVAL RING. Hold zeroes the RADIAL component and nothing else, so
    // a body at the ring stops closing, keeps facing its target and keeps
    // attacking. Retreat backs it off when the player walks in, which is what
    // stops the ring being swallowed rather than merely formed.
    //
    // Deliberately NOT an early return. The lunge blocks below own a COMMITTED
    // action with a locked direction, and a body that drifts into the band
    // mid-wind-up must be allowed to finish rather than freeze holding a
    // telegraph it never pays off. The ring governs the chase; it does not
    // govern a leap that has already been announced to the player.
    // The ring is a CALL now, not inherited code, so an archetype that
    // overrides the chase cannot silently drop it.
    FVector Approach = ToPlayer;
    ApplyArrivalRing(Player, Distance, ToPlayer, OutDirection, OutSpeedScale, Approach);

    // (a) Closing sprint: far away, they commit to closing the gap
    // instead of ambling. Inside SprintRange they drop to normal so the
    // player still gets readable spacing at knife range.
    if (Distance > SprintRange)
    {
        OutSpeedScale = SprintSpeedMultiplier;
        StateLabel = TEXT("CLOSING");
    }

    // (b) Strafe weave: a lateral sinusoid folded into the chase vector.
    // Elites are exempt — the identity is that an elite advances
    // implacably and does not juke (Encounter-Design §1.1 chassis).
    //
    // ELITE OR BETTER, and this asked exactly Elite until now. A
    // ModifierBearing champion and the Field Marshal both juked, which is the
    // stated identity inverted on the two ranks that carry it hardest -- a boss
    // weaving is the opposite of implacable. Same file, same predicate and the
    // same misreading as the drop gate above, which already spells it out.
    //
    // It survived that fix because the fix was scoped by CATEGORY -- the header
    // says IsEliteOrBetter is "the question every REWARD site was actually
    // asking" -- and this is a behaviour site. Scoping by category leaves every
    // instance the category does not name.
    if (!IsEliteOrBetter() && Distance > AttackRange)
    {
        WeaveTime += DeltaSeconds;
        const FVector Lateral = FVector::CrossProduct(FVector::UpVector, Approach).GetSafeNormal2D();
        const float Weave = FMath::Sin((WeaveTime + PatrolPhase) * WeaveFrequency) * WeaveStrength;
        OutDirection = (Approach + Lateral * Weave).GetSafeNormal2D();
        // THE FACE GOES WHERE THE FEET GO. This used to pin DesiredFacing to
        // the player while the feet walked the weave, so the body "looked at
        // the player over its shoulder" — on a rig with one forward Walk
        // cycle and no strafe cycle, that was a nearest-fit fake: a body
        // facing the camera squarely translating 31 degrees off its nose
        // every half-cycle, and up to 60 off it when the arrival angle was
        // in too. The owner saw the slide. No DesiredFacing here: the facing
        // step's NAV-4 default stands, a steering body faces the direction
        // it steers, and the weave is drawn by the whole body turning with
        // it. A pathing body's feet are the follower's, and it faces the leg
        // it walks, as before.
    }

    // (c) SKITTER's committed leap (Encounter-Design §2.1). Three stages:
    // wind-up, committed burst, cooldown.
    //
    // What changed from the shipping version, and why: the lunge had no
    // wind-up and re-solved its direction every frame, so it TRACKED the player
    // through the whole burst. That made it unanswerable by movement — there
    // was nothing to step out of — and O1 leaves the player no other defensive
    // input. §2.1 is explicit that "the leap direction is locked at wind-up —
    // it cannot track", and that the wind-up EXPOSES the weak point, so the
    // correct answer becomes "step sideways and shoot the thing it just showed
    // you". Both halves are now real.
    const bool bLungeActive = !bLungeWindingUp && (Now - LungeStartTime) < LungeDuration;

    if (bLungeWindingUp)
    {
        CommitAttackTarget(Player);
        // Crouched and slow. It has already chosen where it is going.
        // The wind-up length reads through the keyed telegraph seam, sampled
        // per frame: unkeyed the multiplier is exactly 1.0 and this IS the
        // authored LungeWindupSeconds; a Disruptor's Interdiction stretches
        // the remainder of the tell (delays, never cancels — TK8).
        const float EffectiveWindup = LungeWindupSeconds * GetComposedWindupDurationMultiplier();
        OutSpeedScale = LungeWindupMoveScale;
        OutDirection = ToPlayer;
        StateLabel = TEXT("WIND-UP");
        if (WeakPointVisual)
        {
            const float Alpha = EffectiveWindup > 0.0f
                ? FMath::Clamp(static_cast<float>(Now - LungeWindupStartTime) / EffectiveWindup, 0.0f, 1.0f)
                : 1.0f;
            WeakPointVisual->SetRelativeScale3D(FVector(
                FMath::Lerp(WeakPointBaseScale, WeakPointBaseScale * LungeWeakPointSwell, Alpha)));
        }
        if ((Now - LungeWindupStartTime) >= EffectiveWindup)
        {
            bLungeWindingUp = false;
            LungeStartTime = Now;
            // THE COMMITMENT. Locked here, once, and never touched again for
            // the duration of the burst.
            LungeLockedDirection = ToPlayer;
            if (WeakPointVisual) WeakPointVisual->SetRelativeScale3D(FVector(WeakPointBaseScale));
        }
    }
    else if (bLungeActive)
    {
        OutSpeedScale = LungeSpeedMultiplier;
        OutDirection = LungeLockedDirection.IsNearlyZero() ? ToPlayer : LungeLockedDirection;
        StateLabel = TEXT("LUNGE");
    }
    else if (Distance <= LungeRange && Distance > AttackRange
        && (Now - LungeStartTime) >= (LungeDuration + LungeCooldown))
    {
        CommitAttackTarget(Player);
        bLungeWindingUp = true;
        LungeWindupStartTime = Now;
        OutSpeedScale = LungeWindupMoveScale;
        StateLabel = TEXT("WIND-UP");
    }
}

void ABreakerEnemy::InterruptCombatAction()
{
    if (Mover) { Mover->StopMovementImmediately(); Mover->ConsumeInputVector(); }
    if (auto* EnemyController = Cast<ABreakerEnemyController>(GetController())) EnemyController->StopChase();
    bLungeWindingUp = false;
    LungeStartTime = -1000.0;
    LungeWindupStartTime = -1000.0;
    LungeLockedDirection = FVector::ZeroVector;
    LastAttackTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0;
    if (WeakPointVisual) WeakPointVisual->SetRelativeScale3D(FVector(WeakPointBaseScale));
    StateLabel = TEXT("STAGGERED");
}

void ABreakerEnemy::ApplyAuthoredAttackElement(FBreakerDamageRequest& Request) const
{
    // O2 family association and shares: Altered erase, Vestiges decay.
    Request.Element = Family == EBreakerEnemyFamily::Altered ? EBreakerElement::Void : EBreakerElement::Entropy;
    Request.ElementalFraction = Family == EBreakerEnemyFamily::Altered
        ? BreakerVoid::AlteredAttackFraction() : BreakerEntropy::VestigeMeleeFraction();
}

void ABreakerEnemy::PerformAttack(AActor* TargetPawn)
{
    if (!TargetPawn || !GetWorld() || GetWorld()->GetTimeSeconds() - LastAttackTime < AttackCooldown) return;
    // AN ENEMY NEVER STRIKES ANOTHER ENEMY (O261). The guard used to live only
    // in the target SELECTOR — IsEligibleThreatTarget accepts a character or a
    // deployable and nothing else — which is correct until something hands
    // this function a target by another route, and then a full AttackDamage
    // hit lands on a packmate with nothing to stop it. The rule belongs at the
    // strike as well as at the choice, which is where Combat/ states it for
    // zones and projectiles already.
    if (TargetPawn->IsA<ABreakerEnemy>()) return;
    // The AI's broad-phase distance is planar. A real strike must also reach
    // the target vertically and cannot cross intervening blocking geometry.
    const FVector StrikeOrigin = GetActorLocation();
    const FVector StrikeTarget = TargetPawn->GetActorLocation();
    if (FVector::DistSquared(StrikeOrigin, StrikeTarget) > FMath::Square(AttackRange)) return;
    FCollisionQueryParams StrikeQuery(SCENE_QUERY_STAT(BreakerEnemyMeleeObstruction), false, this);
    StrikeQuery.AddIgnoredActor(TargetPawn);
    FHitResult Obstruction;
    if (GetWorld()->LineTraceSingleByChannel(Obstruction, StrikeOrigin, StrikeTarget, ECC_GameTraceChannel2, StrikeQuery)) return;
    UBreakerCombatComponent* TargetCombat = TargetPawn->FindComponentByClass<UBreakerCombatComponent>();
    if (!TargetCombat) return;
    FBreakerDamageRequest Damage;
    // Through the outgoing seam: a WA6 mark softening this enemy lands here.
    // Unkeyed the lane composes to exactly 1.0 and this IS AttackDamage.
    Damage.BaseDamage = GetEffectiveAttackDamage();
    Damage.DamageFamily = EBreakerDamageFamily::Physical;
    ApplyAuthoredAttackElement(Damage);
    Damage.bCanCritical = false;
    Damage.SourceLocation = StrikeOrigin;
    Damage.bHasSourceLocation = true;
    Damage.SetInstigator(this);
    const FBreakerDamageResult Result = TargetCombat->ReceiveDamage(Damage);
    LastAttackTime = GetWorld()->GetTimeSeconds();
    // Anchored's slow and Cascading's hazard both hang off a LANDED hit rather
    // than a swing, so a whiff costs the player nothing.
    if (ModifierComponent && !Result.bDodged && !Result.bParried && Result.HealthDamage + Result.ShieldDamage > 0)
        ModifierComponent->NotifyAttackLanded(TargetPawn->GetActorLocation());
}

// ---------------------------------------------------------------------------
// The keyed enemy-side seams (the FlatArmorReduction pattern, one layer over:
// keyed replace-on-push, pop-by-key, composed as a PRODUCT, empty lane == 1.0
// exactly so every unkeyed consumer is bit-identical to its authored value).
// ---------------------------------------------------------------------------

namespace
{
    // Shared lane arithmetic so the three seams cannot drift apart.
    void BreakerEnemyPushSeamKey(TMap<FName, float>& Lane, FName Key, float Multiplier)
    {
        if (Key.IsNone()) return;
        // Re-pushing REPLACES — the anti-stack rule. Negative multipliers are
        // meaningless in every lane; clamp at zero rather than inverting.
        Lane.Add(Key, FMath::Max(0.0f, Multiplier));
    }

    float BreakerEnemyComposeSeamLane(const TMap<FName, float>& Lane)
    {
        float Product = 1.0f;
        for (const TPair<FName, float>& Entry : Lane) Product *= Entry.Value;
        return Product;
    }
}

void ABreakerEnemy::PushWindupDurationMultiplier(FName Key, float Multiplier)
{
    BreakerEnemyPushSeamKey(WindupDurationMultipliers, Key, Multiplier);
}

void ABreakerEnemy::PopWindupDurationMultiplier(FName Key)
{
    WindupDurationMultipliers.Remove(Key);
}

float ABreakerEnemy::GetComposedWindupDurationMultiplier() const
{
    return BreakerEnemyComposeSeamLane(WindupDurationMultipliers);
}

void ABreakerEnemy::PushAimErrorMultiplier(FName Key, float Multiplier)
{
    BreakerEnemyPushSeamKey(AimErrorMultipliers, Key, Multiplier);
}

void ABreakerEnemy::PopAimErrorMultiplier(FName Key)
{
    AimErrorMultipliers.Remove(Key);
}

float ABreakerEnemy::GetComposedAimErrorMultiplier() const
{
    float Multiplier = BreakerEnemyComposeSeamLane(AimErrorMultipliers);
    const AActor* Target = CommittedAttackTarget.Get();
    const auto* Progression = Target ? Target->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    if (IsEligibleThreatTarget(Target) && Progression
        && Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Provocation"))))
        Multiplier /= 0.9f;
    return Multiplier;
}

void ABreakerEnemy::PushOutgoingDamageMultiplier(FName Key, float Multiplier)
{
    BreakerEnemyPushSeamKey(OutgoingDamageMultipliers, Key, Multiplier);
}

void ABreakerEnemy::PopOutgoingDamageMultiplier(FName Key)
{
    OutgoingDamageMultipliers.Remove(Key);
}

float ABreakerEnemy::GetComposedOutgoingDamageMultiplier() const
{
    return BreakerEnemyComposeSeamLane(OutgoingDamageMultipliers);
}

float ABreakerEnemy::GetEffectiveSpreadDegrees(float AuthoredSpreadDegrees, float ComposedAimErrorMultiplier, float AimErrorUnitDegrees)
{
    const float M = FMath::Max(0.0f, ComposedAimErrorMultiplier);
    // At M == 1.0 both terms are exact float identities (x*1.0f and +0.0f), so
    // an unkeyed enemy's cone IS its authored spread bit for bit. The excess
    // over 1.0 opens fresh cone even on a zero-spread marksman; a below-1.0
    // buff can tighten an authored spread but never below zero.
    return FMath::Max(0.0f,
        AuthoredSpreadDegrees * M + FMath::Max(0.0f, AimErrorUnitDegrees) * (M - 1.0f));
}

void ABreakerEnemy::SetBodyVisible(bool bVisible)
{
    // ONLY WHAT THE BODY WEARS COMES BACK. This used to re-show the six
    // primitives and the 40 cm gold ball on every true, and every re-show
    // path but one followed it with ApplyBodyMesh, which hid them again. The
    // one that did not is SetModifierUntargetable(false): the Phasing
    // modifier's blink ends through it every 6 s, so a Phasing carrier came
    // back wearing the gold ball and the primitive humanoid inside the mech —
    // the "crit spot that randomly appears" the owner reported. A named body
    // keeps its primitives hidden; the ball stays hidden when the weak point
    // rides the named Head (ApplyBodyMesh's own test, restated here rather
    // than read from the ring, because the ring is built only inside a
    // world and the no-world fixture must see the same answer). The ring
    // follows the blink and the death one-shot like the rest of the drawing.
    // NamedBody is deliberately not in this list: the mech corpse stands
    // through its death one-shot (HandleDeathPresentationFinished) and the
    // Wakeful down hides it itself. RECORDED GAP: for the same reason a
    // Phasing blink on a mech body hides only the ring — the mech itself
    // stays drawn through its 0.35 s untargetable window, so the tell is
    // the ring going out and the shots passing through, not an absence.
    static const FName BreakerHeadBoneName(TEXT("Head"));
    const bool bNamed = NamedBody && NamedBody->GetSkeletalMeshAsset() != nullptr;
    const bool bWeakPointRidesNamedHead = bNamed && NamedBody->GetBoneIndex(BreakerHeadBoneName) != INDEX_NONE;
    for (UStaticMeshComponent* Part : { BodyVisual.Get(), HeadVisual.Get(), LeftArmVisual.Get(),
        RightArmVisual.Get(), LeftLegVisual.Get(), RightLegVisual.Get() })
    {
        if (Part) Part->SetVisibility(bVisible && !bNamed, true);
    }
    if (WeakPointVisual) WeakPointVisual->SetVisibility(bVisible && !bWeakPointRidesNamedHead, true);
    if (WeakPointRing) WeakPointRing->SetVisibility(bVisible, true);
}

void ABreakerEnemy::HandleDeath()
{
    ClearThreat();
    // WAKEFUL runs first, and it runs by an explicit call rather than by
    // binding OnDeath alongside this handler. Delegate broadcast order is
    // registration order, which is an accident of component initialisation and
    // not a contract — and a modifier that SUPPRESSES a death cannot be allowed
    // to run after the death has already dropped loot and fed the TTK sample.
    if (ModifierComponent && ModifierComponent->TryConsumeWakefulRevive(bLastHitWasWeakPoint))
    {
        EnterWakefulDowned();
        return;
    }

    bDead = true;
    StateLabel = TEXT("DEAD");
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeakPoint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // The body no longer vanishes on the death frame: it pops, crumples to
    // ash and THEN hides, all inside the corpse window the respawn timer and
    // SetLifeSpan already grant. Collision is off above, so the beat is pure
    // presentation. S2 NOTE (unowned domain): the death thump would fire here.
    StartDeathPresentation(bLastHitWasWeakPoint);
    // A named body dies its own death: the imported packs ship a *_Death
    // one-shot beside every mesh, and without it the gait loop runs through
    // the corpse beat. The primitive crumple above still runs on the hidden
    // parts, so the timing contract (pop, crumple, hide) is untouched.
    if (NamedBody && NamedBody->IsVisible())
    {
        // A hit's gait restore must not land on the corpse.
        if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BodyHitTimer);
        bBodyHitPlaying = false;
        if (UAnimSequence* DeathAnim = Cast<UAnimSequence>(BodyDeathAnimation.TryLoad()))
        {
            NamedBody->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            NamedBody->PlayAnimation(DeathAnim, /*bLooping=*/false);
        }
    }
    // Unconditional like XP, and for the same reason: GrantLoot pays the
    // crafting currency before its item roll, and gating the whole call on
    // bDropsLoot made the wallet inherit loot's wave-gating — in wave mode 5
    // of every 6 waves paid no currency at all, which defeated the "currency
    // is the steady income" comment inside. The ITEM half of GrantLoot still
    // honours bDropsLoot internally.
    if (HasAuthority()) GrantLoot();
    if (HasAuthority()) GrantAmmo();
    if (HasAuthority()) GrantExperience();

    // O168's RAISE, and its position in this function is the contract. It
    // fires AFTER loot and XP so the kill's own payouts are booked first — a
    // consumer that reacts by tearing the interior down must not be able to
    // beat this body's own drop out of the world — and BEFORE the respawn and
    // pool-park scheduling below, both of which are deferred anyway.
    //
    // Guarded on the mark, so an unmarked enemy costs one bool. Wakeful has
    // already returned above, which is why a down is not a death here.
    if (bRiftTerminator)
    {
        OnRiftTerminatorDefeated.Broadcast(this);
    }

    // On-death chain detonation: hurts other enemies only, so packed
    // spawns cascade without turning the player's own kills against them.
    if (HasAuthority() && bExplodesOnDeath && Attributes)
    {
        const float ExplosionDamage = Attributes->GetMaxHealth() * DeathExplosionHealthFraction;
        for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It)
        {
            if (*It == this || It->bDead) continue;
            if (FVector::DistSquared(It->GetActorLocation(), GetActorLocation()) > FMath::Square(DeathExplosionRadius)) continue;
            FBreakerDamageRequest ChainDamage;
            ChainDamage.BaseDamage = ExplosionDamage;
            ChainDamage.DamageFamily = EBreakerDamageFamily::Physical;
            ChainDamage.bCanCritical = false;
            ChainDamage.SourceLocation = GetActorLocation();
            ChainDamage.bHasSourceLocation = true;
            ChainDamage.SetInstigator(this);
            It->Combat->ReceiveDamage(ChainDamage);
        }
    }

    // Feed the time-to-kill instrument (Decisions.md O2).
    if (FirstDamageTime >= 0.0 && GetWorld())
    {
        APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
        if (UBreakerPlaytestComponent* Playtest = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerPlaytestComponent>() : nullptr)
        {
            // Engagement-gapped TTK: idle stretches between damage events are
            // capped, so target-switching doesn't inflate the sample the way
            // wall-clock first-damage-to-death did (session 3 finding).
            Playtest->AddTimeToKillSample(FMath::Max(EngagedSeconds, 0.05f), IsElite(), IsRangedForTelemetry());
        }
        FirstDamageTime = -1.0;
        LastDamageEventTime = -1.0;
        EngagedSeconds = 0.0f;
    }

    // Volatile's fuse and Splitting's copies. After the loot and the TTK
    // sample, because the kill is real — these are what the corpse does next.
    if (HasAuthority() && ModifierComponent) ModifierComponent->NotifyOwnerDied();

    if (bRespawns) GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RespawnEnemy);
    else
    {
        // Long enough for a Volatile fuse to finish before the actor goes
        // away. The old 2.0s was already comfortably past the 1.2s placeholder
        // fuse; this makes the dependency explicit instead of a coincidence.
        const float CorpseSeconds = FMath::Max(2.0f,
            ModifierComponent && ModifierComponent->HasModifier(EBreakerEnemyModifier::Volatile)
                ? ModifierComponent->Params.VolatileFuseSeconds + 1.0f : 0.0f);
        // A poolable body parks on the same corpse clock instead of dying for
        // real — the fuse, the crumple and the loot beat all finish first
        // either way.
        if (bPooledByGameMode)
        {
            GetWorldTimerManager().SetTimer(PoolParkTimer, this, &ThisClass::ParkPooledBody, CorpseSeconds, false);
        }
        else SetLifeSpan(CorpseSeconds);
    }
}

void ABreakerEnemy::ParkPooledBody()
{
    // The corpse becomes a reserve body: still bDead, hidden, inert, and
    // stripped of everything a reuse could inherit.
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
    // A parked body is still: its path is dropped and its velocity zeroed,
    // so a revive never inherits a chase from a previous life.
    if (ABreakerEnemyController* EnemyController = Cast<ABreakerEnemyController>(GetController())) EnemyController->StopChase();
    ClearThreat();
    if (Mover) Mover->ResetForRevive();
    // Statuses stop the silent way: zeroing durations lets each expire
    // through its own teardown on the component's next tick (popping the
    // seam-lane keys it pushed), where ConsumeAllStatuses would broadcast
    // consumption feedback over a corpse.
    if (Status) Status->ScaleRemainingDurations(0.0f);
    // Modifier teardown releases the aura and hazards, but not the Warded
    // ward — MaxShield is the one stat SetModifiers({}) leaves standing.
    if (ModifierComponent) ModifierComponent->SetModifiers({});
    SetModifierShield(0.0f);
    // KNOWN EDGE, accepted: a seam-lane push whose owner was destroyed the
    // same frame as this park has nobody left to pop it. The lanes are
    // emptied wholesale at revive, so nothing can cross into a reuse.
    if (OnParkedForPool.IsBound()) OnParkedForPool.Execute(this);
    else Destroy();
}


void ABreakerEnemy::GrantEmergenceWindow()
{
    UWorld* World = GetWorld();
    UBreakerCombatComponent* Emerging = FindComponentByClass<UBreakerCombatComponent>();
    if (!World || !Emerging || !HasAuthority() || EmergenceProtectedSeconds <= 0.0f) return;
    // Re-arming replaces the previous cleanup rather than stacking one, the
    // same way Hard Stop reuses its timer when a cooldown reset allows a
    // second cast inside the first window. A pooled body arrives many times.
    World->GetTimerManager().ClearTimer(EmergenceTimer);
    Emerging->PushIncomingDamageModifier(EmergenceModifierKey(), 0.0f);
    // O274: the same clock that keeps the body undeletable keeps it from
    // hunting. Tick reads this and takes the patrol branch — the walk to the
    // post on the shipped leash — until EndEmergenceWindow clears it.
    bEmerging = true;
    Emerging->OnDeath.AddUniqueDynamic(this, &ABreakerEnemy::EndEmergenceWindow);
    World->GetTimerManager().SetTimer(EmergenceTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        EndEmergenceWindow();
    }), EmergenceProtectedSeconds, false);
}

void ABreakerEnemy::EndEmergenceWindow()
{
    // Both halves of O274 end here and nowhere else: the next tick hunts.
    bEmerging = false;
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(EmergenceTimer);
    if (UBreakerCombatComponent* Emerged = FindComponentByClass<UBreakerCombatComponent>())
    {
        Emerged->RemoveIncomingDamageModifier(EmergenceModifierKey());
    }
}

void ABreakerEnemy::ReviveFromPool(const FVector& SpawnLocation)
{
    // RespawnEnemy's checklist, plus everything a PROMOTED body has to give
    // back. The caller replays the wave config sequence afterwards
    // (ConfigureEncounter, ConfigureWave, promotions, loot flag, telemetry),
    // so this only has to return a fresh, unranked body.
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);
    SetActorLocation(SpawnLocation);
    ClearThreat();
    if (Mover) Mover->ResetForRevive();
    SetActorScale3D(PooledBaseScale);
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BodyHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WeakPoint->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ResetDeathPresentation();
    SetBodyVisible(true);
    // A revived NAMED body is mid-death-pose otherwise: ApplyBodyMesh re-plays
    // the gait loop (idempotent — unset bodies stay primitives). Part of the
    // fresh-body promise, inside the function for the O128 reason above.
    ApplyBodyMesh();
    bDead = false;
    StateLabel = TEXT("PATROL");
    bLastHitWasWeakPoint = false;
    FirstDamageTime = -1.0;
    LastDamageEventTime = -1.0;
    EngagedSeconds = 0.0f;
    // TargetBandBroken's bit is a statement about the PREVIOUS HIT, and on a
    // reused body the previous hit belonged to a different life — without
    // this, the first hit on a revived enemy inherits a rider it didn't earn.
    if (Combat) Combat->ClearBandBreakTracking();
    LastAttackTime = -1000.0;
    WeaveTime = 0.0f;
    LungeStartTime = -1000.0;
    LungeWindupStartTime = -1000.0;
    LungeLockedDirection = FVector::ZeroVector;
    bLungeWindingUp = false;
    if (BaseMoveSpeed >= 0.0f) MoveSpeed = BaseMoveSpeed;
    if (BaseWeaveStrength >= 0.0f) WeaveStrength = BaseWeaveStrength;
    WindupDurationMultipliers.Empty();
    AimErrorMultipliers.Empty();
    OutgoingDamageMultipliers.Empty();
    // Demoted, and the gold given back HERE (O128). This used to be the
    // rank assignment alone, and the paint came back only because all three
    // pool callers happen to run ConfigureWave on the next line — correct by
    // caller, not by function, and only for callers that happen to. A
    // function's contract does not live in its callers, so the repaint is
    // inside the function that promises a fresh, unranked body.
    MonsterRank = EBreakerMonsterRank::Trash;
    ModifierCountHealthMultiplier = 1.0f;
    // O168's mark goes back with the gold. A reused body holds nothing open,
    // and a terminator that survived into a wave spawn would raise a
    // completion for a rift the player is no longer in.
    bRiftTerminator = false;
    OnRiftTerminatorDefeated.Clear();
    RefreshBodyPaint();
    // A parked body's health is still zero until the caller's chassis pass
    // refills it, and O129's ramp would read that corpse figure. This
    // function promises a FRESH body, so the paint says fresh rather than
    // inheriting a dead reading for the frame — the same reason the rank
    // restore moved in here rather than staying with whoever calls next.
    if (HitReaction) HitReaction->SetHealthFraction(1.0f);
}

void ABreakerEnemy::EnterWakefulDowned()
{
    // Down, not dead: no loot, no ammo, no chain detonation, no TTK sample, and
    // the enemy is NOT marked bDead — the kill has not happened yet.
    StateLabel = TEXT("DOWNED");
    if (BodyCollision) BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (BodyHitBox) BodyHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (WeakPoint) WeakPoint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetBodyVisible(false);
    // The down's whole tell is ABSENCE, and SetBodyVisible only speaks
    // primitive: a downed Wakeful mech kept standing in its idle loop,
    // which reads as a bug rather than a down. The named body vanishes
    // with the primitives; the rise below re-applies it, gait re-played.
    if (NamedBody) NamedBody->SetVisibility(false);

    const float Delay = ModifierComponent ? ModifierComponent->GetWakefulReviveDelay() : 4.0f;
    FTimerHandle ReviveTimer;
    GetWorldTimerManager().SetTimer(ReviveTimer, this, &ThisClass::FinishWakefulRevive,
        FMath::Max(0.01f, Delay), false);
}

void ABreakerEnemy::FinishWakefulRevive()
{
    const float Fraction = ModifierComponent ? ModifierComponent->GetWakefulReviveHealthFraction() : 0.35f;
    if (BodyCollision) BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (BodyHitBox) BodyHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    if (WeakPoint) WeakPoint->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SetBodyVisible(true);
    // The named body's half of the rise (see EnterWakefulDowned): visible
    // again with the gait re-played and the fit restored. Idempotent for
    // primitive bodies, the same one door every revive path takes.
    ApplyBodyMesh();
    if (Combat) Combat->RestoreVitals();
    if (Attributes) Attributes->SetHealth(Attributes->GetMaxHealth() * Fraction);
    // The ward does NOT come back with it: a Wakeful Warded enemy would be two
    // full health bars twice, which is exactly the durability stacking §1.3's
    // three-modifier rule exists to prevent.
    if (Attributes && ModifierComponent && ModifierComponent->HasModifier(EBreakerEnemyModifier::Warded))
    {
        Attributes->SetShield(0.0f);
    }
    // Wakeful rises at a FRACTION of max health, so it rises already reddened
    // — which is the honest read and the reason this is not a full restore.
    RefreshBodyPaint();
    StateLabel = TEXT("RISEN");
}

void ABreakerEnemy::HandleDamageReceived(const FBreakerDamageResult& Result)
{
    if (!GetWorld() || (Result.HealthDamage <= 0.0f && Result.ShieldDamage <= 0.0f)) return;
    // Wakeful denies its revive to a weak-point killing blow, so the LAST hit's
    // weak-point flag has to survive until HandleDeath reads it.
    bLastHitWasWeakPoint = Result.bWeakPoint;
    const double Now = GetWorld()->GetTimeSeconds();
    if (FirstDamageTime < 0.0) FirstDamageTime = Now;
    if (LastDamageEventTime >= 0.0)
    {
        // Gaps longer than 1.5s are disengagement, not fighting.
        EngagedSeconds += static_cast<float>(FMath::Min(Now - LastDamageEventTime, 1.5));
    }
    LastDamageEventTime = Now;

    // The body ANSWERS the hit: a one-blink material pulse, gold when the hit
    // was a weak point. Cosmetic only — nothing above reads it. The pulse
    // lives in the shared reaction component now (see its header), so the
    // target dummy answers exactly the same way.
    if (HitReaction) HitReaction->NotifyHit(Result.bWeakPoint);
    // And the rig answers, when it has an answer authored.
    PlayBodyHit();
    // O129's health ramp: the body reddens as it dies, and this is the event
    // that moves it. Pushed here rather than read on tick — a hundred enemies
    // sampling two attributes every frame to find out nothing changed is the
    // cost this event already pays for free.
    RefreshBodyPaint();
}

// --- Hit / death presentation (cosmetic only) ------------------------------
// The flash, the two-beat death and the revive restore moved verbatim into
// UBreakerHitReactionComponent. What stays here is what is the ENEMY'S: which
// parts get painted (registered in BeginPlay), and what vanishes when the
// crumple lands.

void ABreakerEnemy::StartDeathPresentation(bool bWeakPointKill)
{
    if (HitReaction) HitReaction->StartDeathPresentation(bWeakPointKill);
    else SetBodyVisible(false);
}

void ABreakerEnemy::HandleDeathPresentationFinished()
{
    SetBodyVisible(false);
}

void ABreakerEnemy::ResetDeathPresentation()
{
    if (HitReaction) HitReaction->ResetDeathPresentation();
}

void ABreakerEnemy::GrantExperience()
{
    // XP pays on EVERY kill, unconditionally — unlike loot, which most trash
    // deliberately does not drop, and unlike currency, which is gated by rank.
    // That difference is the point: XP is the channel that always moves, so
    // fighting always visibly progresses something even on the kills that pay
    // nothing else.
    //
    // Deliberately NOT gated on bDropsLoot: that flag says "this spawn is not
    // a loot source" (arena furniture, scripted spawns), which is a statement
    // about ITEMS. A kill the player earned still teaches the game something.
    APawn* PlayerPawn = GetWorld() && GetWorld()->GetFirstPlayerController()
        ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    UBreakerProgressionComponent* Progression = PlayerPawn
        ? PlayerPawn->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    if (!Progression) return;

    // Area level, not character level — see UBreakerExperienceLibrary::
    // XpForKill for why the reward tracks the content rather than the player.
    //
    // And AreaLevel, not EnemyLevel (audit finding #4): this call passed
    // EnemyLevel — the DROP item level, GetDropItemLevel(AreaLevel) plus the
    // elite bonus, clamped to 120 against the area ladder's 100 — while the
    // comment above claimed area level. Rank already pays the elite premium
    // through EliteXpMultiplier, so paying it again through the level scalar
    // double-charged it: an elite at area level 10 paid 102 XP off EnemyLevel
    // 15 where the area's own level pays 83, and past area level 100 the two
    // clamps let XP keep climbing 20 levels the area ladder does not have.
    // EnemyLevel stays what it is: the LOOT number GrantLoot hands the drop
    // pipeline, one comment down.
    Progression->AwardKillExperience(MonsterRank, AreaLevel);
}

void ABreakerEnemy::GrantLoot()
{
    APawn* PlayerPawn = GetWorld() ? GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr : nullptr;
    UBreakerEquipmentComponent* Equipment = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerEquipmentComponent>() : nullptr;
    if (!Equipment) return;

    ++KillCount;
    const int32 Seed = HashCombine(GetTypeHash(GetActorLocation()), KillCount);

    // CRAFTING CURRENCY PAYS BEFORE THE ITEM ROLL, and deliberately outside it.
    // Most trash kills drop no item at all by design (the rank drop-chance step
    // below), so crediting currency after that early-return would have made the
    // Forge economy inherit loot's sparsity — the player would fight for
    // minutes and see the wallet move only on the kills that already paid them
    // an item. Currency is the steady income; items are the spiky one.
    Equipment->CreditForgeCurrency(UBreakerDropTableLibrary::RollCurrencyDrop(
        Seed, EnemyLevel, MonsterRank, CurrencyDropTable));

    // ITEMS, from here down, are what bDropsLoot actually gates: "this spawn
    // is not a loot source" is a statement about items (arena furniture,
    // standard waves), not about the wallet above or the XP alongside.
    if (!bDropsLoot) return;

    // THE DROP PIPELINE (Items/BreakerDropTable.h). This used to be a bare
    // RollRarity call, which meant every death produced an item and the flat
    // rarity table was the whole system — the owner's playtest report from both
    // ends. Now: a per-rank DROP CHANCE step runs first (most trash kills drop
    // nothing at all), then the rarity is rolled against gates on drop item
    // level and monster rank, so a low-level trash kill is structurally
    // incapable of producing an Aberrant.
    EBreakerItemRarity Rarity = EBreakerItemRarity::Standard;
    if (!UBreakerDropTableLibrary::RollDrop(Seed, EnemyLevel, MonsterRank,
        Equipment->GetStats().DropChancePercent, DropTable, Rarity))
    {
        return;
    }

    // The elite floor survives the rewrite. It is a FLOOR on an elite that has
    // already decided to drop, not a second drop chance, so it composes with
    // the gates rather than competing with them: an elite in a level-3 area
    // still cannot exceed what its item level unlocks.
    // ELITE OR BETTER, for the same reason as the item-level bonus above: a
    // three-modifier champion is an elite with modifiers on it, and it was
    // getting no rarity floor at all.
    if (IsEliteOrBetter() && Rarity < EBreakerItemRarity::Exceptional
        && UBreakerDropTableLibrary::IsRarityUnlocked(EBreakerItemRarity::Exceptional, EnemyLevel, MonsterRank, DropTable))
    {
        Rarity = EBreakerItemRarity::Exceptional;
    }

    // The slot draw lives in the loot library now, salted — drawing it here
    // from FRandomStream(Seed) collided with RollItem's own first draw and
    // pinned every weapon drop to Machinegun or Sidearm (see RollDropSlot).
    const EBreakerEquipSlot Slot = UBreakerLootLibrary::RollDropSlot(Seed);
    const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("GymDrop"), Slot, Rarity, EnemyLevel, Seed);

    // Drops land on the ground now instead of teleporting into the backpack:
    // the player walks over, reads the popup, and presses F. Scatter keeps a
    // stack of kills on one spot from overlapping into a single column.
    FRandomStream ScatterStream(Seed ^ 0x5EED);
    const FVector Scatter(ScatterStream.FRandRange(-80.0f, 80.0f), ScatterStream.FRandRange(-80.0f, 80.0f), 0.0f);
    const FVector DropLocation = GetActorLocation() + Scatter + FVector(0.0f, 0.0f, 40.0f);
    if (ABreakerLootPickup* Pickup = GetWorld()->SpawnActor<ABreakerLootPickup>(ABreakerLootPickup::StaticClass(), DropLocation, FRotator::ZeroRotator))
    {
        Pickup->SetItem(Item);
    }
}

void ABreakerEnemy::GrantAmmo()
{
    // Owner feedback: "ran out of ammo after 3 waves — no way to regain
    // ammo". Kills now feed the gun. O2 placeholders: a normal kill returns
    // 15% of a magazine-weapon's starting reserve, an elite half of it —
    // roughly, sustained accurate play is ammo-neutral and sloppy play still
    // runs dry. Uses the first player pawn, same as GrantLoot.
    const float NormalKillFraction = 0.15f;
    const float EliteKillFraction = 0.50f;

    APawn* PlayerPawn = GetWorld() && GetWorld()->GetFirstPlayerController()
        ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    if (UBreakerWeaponComponent* Weapon = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerWeaponComponent>() : nullptr)
    {
        // ELITE OR BETTER. The third site found reading the narrow predicate for
        // a reward, and the same defect: a three-modifier champion and the Field
        // Marshal both returned the NORMAL 15% rather than the elite 50%, so the
        // two hardest things in the game fed the gun least.
        Weapon->AddReserveAmmoFraction(IsEliteOrBetter() ? EliteKillFraction : NormalKillFraction);
    }
}

void ABreakerEnemy::RespawnEnemy()
{
    FTimerHandle RespawnTimer;
    GetWorldTimerManager().SetTimer(RespawnTimer, [this]()
    {
        ClearThreat();
        SetActorLocation(LeashOrigin);
        BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BodyHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        WeakPoint->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        // Defensive: if the respawn delay was ever tuned under the death
        // beat's length, the body must come back at its own scale and colours.
        ResetDeathPresentation();
        SetBodyVisible(true);
        // The named body dies its own death (HandleDeath) and nothing here
        // ever brought it back: the mech returned holding the death
        // one-shot's final frame — alive, fighting, lying where it fell —
        // while SetBodyVisible(true) above un-hid the primitives underneath
        // it, a double body. The pool's revive already routes through
        // ApplyBodyMesh for exactly this reset (gait re-played, fit
        // restored, primitives re-hidden); the standing respawn now takes
        // the same door. Idempotent — a primitive body returns at the guard.
        ApplyBodyMesh();
        bDead = false;
        FirstDamageTime = -1.0;
        LastDamageEventTime = -1.0;
        EngagedSeconds = 0.0f;
        Combat->RestoreVitals();
    }, RespawnDelay, false);
}
