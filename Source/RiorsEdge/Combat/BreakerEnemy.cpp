#include "Combat/BreakerEnemy.h"

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
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
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

ABreakerEnemy::ABreakerEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
    SetRootComponent(BodyCollision);
    BodyCollision->InitCapsuleSize(45.0f, 90.0f);
    BodyCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

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
    const BreakerEnemyBody::FBreakerBodyFit Fit = BreakerEnemyBody::FitBodyToCapsule(
        MeshBounds.Origin, MeshBounds.BoxExtent, BodyCollision->GetUnscaledCapsuleHalfHeight());
    NamedBody->SetRelativeScale3D(FVector(Fit.Scale));
    NamedBody->SetRelativeLocation(Fit.RelativeLocation);
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

    AttackDamage = UBreakerMonsterChassisLibrary::GetMonsterDamage(
        AreaLevel, MonsterRank, Chassis, ArchetypeDamageMultiplier);

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
    Attributes->SetMaxShield(Clamped);
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
    SetActorScale3D(GetActorScale3D() * 0.7f);
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
    LeashOrigin = NewLeashOrigin;
    PatrolPhase = NewPatrolPhase;
}

void ABreakerEnemy::ConfigureElite()
{
    // The elite's health and damage numbers used to live right here — a
    // hardcoded 440 health and a *= 1.5f damage, a second source of truth
    // sitting alongside the base chassis' hardcoded 220. Both are gone: rank
    // is now a row in the chassis rank table, and ApplyChassis composes it.
    // What stays here is what is genuinely elite PRESENTATION and BEHAVIOUR:
    // the bigger silhouette, the slower implacable advance, the loot floor.
    MonsterRank = EBreakerMonsterRank::Elite;
    SetActorScale3D(GetActorScale3D() * 1.25f);
    MoveSpeed *= 0.85f;
    ApplyChassis();
    StateLabel = TEXT("ELITE PATROL");
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

void ABreakerEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
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

    ABreakerCharacter* NearestPlayer = nullptr;
    float NearestDistanceSq = TNumericLimits<float>::Max();
    for (TActorIterator<ABreakerCharacter> It(GetWorld()); It; ++It)
    {
        const float DistanceSq = FVector::DistSquared2D(GetActorLocation(), It->GetActorLocation());
        if (DistanceSq < NearestDistanceSq)
        {
            NearestDistanceSq = DistanceSq;
            NearestPlayer = *It;
        }
    }

    // Players standing in the safe zone are off-limits, and enemies stop at
    // its edge rather than following them in.
    const ABreakerGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakerGameMode>();
    if (NearestPlayer && GameMode && GameMode->IsInSafeZone(NearestPlayer->GetActorLocation()))
    {
        NearestPlayer = nullptr;
    }

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
    if (NearestPlayer && Distance <= DetectionRange)
    {
        TickEngagedBehaviour(NearestPlayer, Distance, DeltaSeconds, DesiredDirection, SpeedScale);
    }
    else
    {
        PatrolPhase += DeltaSeconds * 0.7f;
        const FVector PatrolTarget = LeashOrigin + FVector(0.0f, FMath::Sin(PatrolPhase) * 350.0f, 0.0f);
        DesiredDirection = (PatrolTarget - GetActorLocation()).GetSafeNormal2D();
        StateLabel = TEXT("PATROL");
    }

    // Facing is decided before movement so an archetype that strafes sideways
    // while aiming at the player still points its muzzle at the player.
    const FVector Facing = DesiredFacing.IsNearlyZero() ? DesiredDirection : DesiredFacing;
    if (!Facing.IsNearlyZero()) SetActorRotation(Facing.Rotation());

    if (!DesiredDirection.IsNearlyZero())
    {
        const FVector Step = DesiredDirection * MoveSpeed * SpeedScale * DeltaSeconds;
        const FVector NextLocation = GetActorLocation() + Step;
        if (GameMode && GameMode->IsInSafeZone(NextLocation))
        {
            StateLabel = TEXT("HELD");
            return;
        }
        FHitResult MoveHit;
        AddActorWorldOffset(Step, true, &MoveHit);
    }

    // Ground snap: enemies move by offset with no gravity, so without this
    // they hover over the apron slabs or float where spawn height was off.
    // Trace down, plant the capsule base on whatever is below.
    {
        const float HalfHeight = BodyCollision ? BodyCollision->GetScaledCapsuleHalfHeight() : 88.0f;
        const FVector TraceStart = GetActorLocation() + FVector(0, 0, 60.0f);
        const FVector TraceEnd = TraceStart - FVector(0, 0, 4000.0f);
        FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(BreakerEnemyGround), false, this);
        FHitResult Ground;
        if (GetWorld()->LineTraceSingleByChannel(Ground, TraceStart, TraceEnd, ECC_WorldStatic, GroundParams))
        {
            const float TargetZ = Ground.ImpactPoint.Z + HalfHeight;
            const float CurrentZ = GetActorLocation().Z;
            // Snap down instantly, step up smoothly, so slabs read as steps
            // rather than teleports.
            const float NewZ = CurrentZ > TargetZ
                ? FMath::Max(TargetZ, CurrentZ - 1200.0f * DeltaSeconds)
                : FMath::Min(TargetZ, CurrentZ + 600.0f * DeltaSeconds);
            SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, NewZ), false);
        }
    }
}

void ABreakerEnemy::TickEngagedBehaviour(ABreakerCharacter* Player, float Distance, float DeltaSeconds,
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
    if (Distance <= AttackRange) PerformAttack(Player);

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
    ArrivalBand = UBreakerRangedBehaviorLibrary::ClassifyBand(Distance,
        AttackRange * ArrivalInnerRatio, AttackRange, ArrivalHysteresisCm, ArrivalBand);
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
        const FVector Lateral = FVector::CrossProduct(FVector::UpVector, ToPlayer).GetSafeNormal2D();
        const float Weave = FMath::Sin((WeaveTime + PatrolPhase) * WeaveFrequency) * WeaveStrength;
        OutDirection = (ToPlayer + Lateral * Weave).GetSafeNormal2D();
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
        bLungeWindingUp = true;
        LungeWindupStartTime = Now;
        OutSpeedScale = LungeWindupMoveScale;
        StateLabel = TEXT("WIND-UP");
    }
}

void ABreakerEnemy::PerformAttack(APawn* TargetPawn)
{
    if (!TargetPawn || !GetWorld() || GetWorld()->GetTimeSeconds() - LastAttackTime < AttackCooldown) return;
    UBreakerCombatComponent* TargetCombat = TargetPawn->FindComponentByClass<UBreakerCombatComponent>();
    if (!TargetCombat) return;
    FBreakerDamageRequest Damage;
    // Through the outgoing seam: a WA6 mark softening this enemy lands here.
    // Unkeyed the lane composes to exactly 1.0 and this IS AttackDamage.
    Damage.BaseDamage = GetEffectiveAttackDamage();
    Damage.DamageFamily = EBreakerDamageFamily::Physical;
    Damage.bCanCritical = false;
    Damage.SetInstigator(this);
    TargetCombat->ReceiveDamage(Damage);
    LastAttackTime = GetWorld()->GetTimeSeconds();
    // Anchored's slow and Cascading's hazard both hang off a LANDED hit rather
    // than a swing, so a whiff costs the player nothing.
    if (ModifierComponent) ModifierComponent->NotifyAttackLanded(TargetPawn->GetActorLocation());
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
    return BreakerEnemyComposeSeamLane(AimErrorMultipliers);
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
    for (UStaticMeshComponent* Part : { BodyVisual.Get(), HeadVisual.Get(), LeftArmVisual.Get(),
        RightArmVisual.Get(), LeftLegVisual.Get(), RightLegVisual.Get(), WeakPointVisual.Get() })
    {
        if (Part) Part->SetVisibility(bVisible, true);
    }
}

void ABreakerEnemy::HandleDeath()
{
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
    SetActorScale3D(PooledBaseScale);
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BodyHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WeakPoint->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ResetDeathPresentation();
    SetBodyVisible(true);
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
        SetActorLocation(LeashOrigin);
        BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BodyHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        WeakPoint->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        // Defensive: if the respawn delay was ever tuned under the death
        // beat's length, the body must come back at its own scale and colours.
        ResetDeathPresentation();
        SetBodyVisible(true);
        bDead = false;
        FirstDamageTime = -1.0;
        LastDamageEventTime = -1.0;
        EngagedSeconds = 0.0f;
        Combat->RestoreVitals();
    }, RespawnDelay, false);
}
