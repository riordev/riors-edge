#include "Combat/BreakerBossEnemy.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Combat/BreakerRangedBehavior.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace BreakerBossRuntime
{
    // Distinctively prefixed for the unity build, the same discipline as
    // BreakerModifierRuntime::BreakerAuraModifierKey. One key, so a re-push
    // replaces rather than stacks and a remove takes the whole gate off.
    static const FName BreakerBossAddGateKey(TEXT("Boss.AddGate"));
}

TSubclassOf<ABreakerBossEnemy> ABreakerBossEnemy::ClassForBossName(FName BossName)
{
    if (BossName.IsNone()) return nullptr;
    if (BossName == FName(TEXT("Holdfast"))) return ABreakerHoldfastEnemy::StaticClass();
    if (BossName == FName(TEXT("FieldMarshal"))) return ABreakerBossEnemy::StaticClass();
    return nullptr;
}

ABreakerBossEnemy::ABreakerBossEnemy()
{
    // Rank Boss rank is x75 (O2 PLACEHOLDER, owner ruling 2026-08-16: boss HP x3) health and x2 damage out of the chassis rank table
    // (Power-Curve §2), derived from O18's 20-45s band against a sub-1s trash
    // target. The boss does NOT author its own health number: §3.2's literal
    // 2400 was anchored to a placeholder baseline that no longer exists, and
    // duplicating it here would be the second-source-of-truth bug O27 already
    // deleted from ConfigureElite.
    MonsterRank = EBreakerMonsterRank::Boss;
    // §3.1's corollary: NOT a sponge. The archetype ratio is deliberately below
    // 1 so the Warden's own 3.2x does not compound into a boss with eighty
    // times a trash mob's health — the boss's interest is the adds, not its
    // health bar.
    ArchetypeHealthMultiplier = 0.35f;   // O2 PLACEHOLDER
    ArchetypeDamageMultiplier = 1.86f;   // inherits the Warden's sweep ratio

    // FAMILY (Assets/story-source.md §1.5). The Field Marshal is Altered at EARLY
    // severance, and that is a mechanical claim rather than a label: it is the
    // most lucid hostile thing in the game, because it still gives ORDERS.
    // Command is the highest-order cognition anything on this spectrum
    // retains, so the boss sits at the lucid end and the fight's whole shape —
    // adds that obey it — is what early severance MEANS. Assets/story-source.md §1.5's
    // "the first humanoid that demonstrably gives orders" lands as a mechanic
    // before any dialogue says it.
    //
    // It does not take cover (it holds the centre) and it does not flinch (§3.2
    // makes it stagger-immune), so the two stage contracts are deliberately not
    // claimed here even though the stage would allow them. Stage sets what is
    // POSSIBLE; the archetype still decides what it does.
    Family = EBreakerEnemyFamily::Altered;
    SeveranceStage = EBreakerSeveranceStage::Early;

    MoveSpeed = 300.0f;         // O2 PLACEHOLDER (§3.2: slower than the player)
    DetectionRange = 6000.0f;   // O2 PLACEHOLDER: it holds an arena, not a leash
    bRespawns = false;
    // §3.2 gives it no death detonation and no chain: the fight ENDS, and a
    // corpse that damages its own surviving adds would end it for the player.
    bExplodesOnDeath = false;
    SlamCooldownSeconds = 7.0f;
    SweepCooldownSeconds = 2.2f;
    // Phase 3 turns this on; it is off until then, because §1.2 puts lingering
    // hazards behind the Cascading MODIFIER and the boss borrows the behaviour
    // deliberately and late ("the player has already learned to read it").
    bSlamLeavesHazard = false;

    SetActorScale3D(FVector(1.75f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

    // The command apparatus. Mounted on the BACK, which is what makes §3.2's
    // weak-point rule coherent: it is the rear weak point, and the raise is the
    // only time it clears the shoulder line and becomes visible from the front.
    ApparatusVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApparatusVisual"));
    ApparatusVisual->SetupAttachment(BodyCollision);
    ApparatusVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ApparatusVisual->SetRelativeLocation(FVector(-38.0f, 0.0f, 46.0f));
    ApparatusVisual->SetRelativeScale3D(FVector(0.16f, 0.42f, 0.62f));
    if (CubeMesh.Succeeded()) ApparatusVisual->SetStaticMesh(CubeMesh.Object);

    ApparatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ApparatusLight"));
    ApparatusLight->SetupAttachment(ApparatusVisual);
    ApparatusLight->SetCastShadows(false);
    ApparatusLight->SetAttenuationRadius(1400.0f);
    ApparatusLight->SetIntensity(0.0f);

    // §3.3's four corner alcoves and two galleries, as offsets. In the flat gym
    // these are just four points around the boss; in the real arena they are
    // the room's corners and the N/S galleries at +600.
    AlcoveOffsets = {
        FVector( 1700.0f,  1700.0f, 0.0f),
        FVector( 1700.0f, -1700.0f, 0.0f),
        FVector(-1700.0f,  1700.0f, 0.0f),
        FVector(-1700.0f, -1700.0f, 0.0f),
    };
    GalleryOffsets = {
        FVector( 1900.0f, 0.0f, 600.0f),
        FVector(-1900.0f, 0.0f, 600.0f),
    };
}

void ABreakerBossEnemy::BeginPlay()
{
    Super::BeginPlay();

    if (GetClass() == ABreakerBossEnemy::StaticClass() && ShieldVisual)
    {
        // The old full-height slab hid the imported commander's entire body.
        // Keep a readable frontal plate below the head and shoulder line;
        // the existing front pool, break and windup still own its behavior.
        ConfigureShieldPresentation(FVector(45, 0, -15), FVector(0.10f, 0.78f, 0.80f));
        UStaticMesh* TrimMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        UMaterialInterface* TrimBase = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        auto AddTrim = [&](const TCHAR* Name, FVector Position, FVector Scale, FLinearColor Color, float Roll = 0.0f)
        {
            UStaticMeshComponent* Trim = NewObject<UStaticMeshComponent>(this, FName(Name));
            AddInstanceComponent(Trim);
            Trim->SetupAttachment(ShieldVisual);
            Trim->SetStaticMesh(TrimMesh);
            Trim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Trim->SetRelativeLocation(Position);
            Trim->SetRelativeScale3D(Scale);
            Trim->SetRelativeRotation(FRotator(0, 0, Roll));
            if (TrimBase)
            {
                UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(TrimBase, Trim);
                Material->SetVectorParameterValue(TEXT("Color"), Color);
                Trim->SetMaterial(0, Material);
            }
            Trim->RegisterComponent();
            Trim->SetVisibility(!IsFrontBroken());
        };
        const FLinearColor FrameColor(0.10f, 0.19f, 0.20f);
        AddTrim(TEXT("MarshalPlateLeft"), FVector(55, -46, 0), FVector(0.6f, 0.08f, 1.05f), FrameColor);
        AddTrim(TEXT("MarshalPlateRight"), FVector(55, 46, 0), FVector(0.6f, 0.08f, 1.05f), FrameColor);
        AddTrim(TEXT("MarshalPlateTop"), FVector(55, 0, 46), FVector(0.6f, 0.85f, 0.08f), FrameColor);
        AddTrim(TEXT("MarshalPlateBottom"), FVector(55, 0, -46), FVector(0.6f, 0.85f, 0.08f), FrameColor);
        const FLinearColor CommandColor(0.12f, 0.70f, 0.64f);
        AddTrim(TEXT("MarshalInsigniaLeft"), FVector(56, -9, 8), FVector(0.3f, 0.25f, 0.035f), CommandColor, 30);
        AddTrim(TEXT("MarshalInsigniaRight"), FVector(56, 9, 8), FVector(0.3f, 0.25f, 0.035f), CommandColor, -30);
        AddTrim(TEXT("MarshalInsigniaBar"), FVector(56, 0, -12), FVector(0.3f, 0.40f, 0.035f), CommandColor);
    }

    BaseSweepCooldown = SweepCooldownSeconds;
    BaseSlamCooldown = SlamCooldownSeconds;
    BaseBossMoveSpeed = MoveSpeed;

    if (ApparatusVisual)
    {
        ApparatusRestLocation = ApparatusVisual->GetRelativeLocation();
        if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            ApparatusMaterial = UMaterialInstanceDynamic::Create(Base, ApparatusVisual);
            if (ApparatusMaterial)
            {
                ApparatusMaterial->SetVectorParameterValue(TEXT("Color"), ApparatusIdleColor);
                ApparatusVisual->SetMaterial(0, ApparatusMaterial);
            }
        }
    }

    if (GetClass() == ABreakerBossEnemy::StaticClass()) BuildMarshalApparatusDetails();

    // §3.2's DoT stack cap. The status component already owns stacking, so this
    // is one number rather than a boss-specific status path.
    if (Status) Status->MaximumStacksPerStatus = FMath::Max(1, BossDamageOverTimeStackCap);

    // Defaults resolved here rather than in the constructor so a Blueprint can
    // override them without this class needing to know the add classes exist at
    // construction time.
    if (!DeployAddClass) DeployAddClass = ABreakerEnemy::StaticClass();
    if (!GalleryLatticeClass) GalleryLatticeClass = ABreakerRangedEnemy::StaticClass();

    Phase = EBreakerBossPhase::Deployment;
    // The rear weak point starts CLOSED. §3.2: exposed "only during Orders".
    SetApparatusExposed(false);
    StateLabel = TEXT("FIELD MARSHAL");
}

bool ABreakerBossEnemy::IsApparatusExposed() const
{
    return UBreakerBossPhaseLibrary::IsPunishWindowOpen(Phase, bOrderRaiseActive, FrontBreakWindowRemaining);
}

void ABreakerBossEnemy::OnFrontBroken()
{
    // The Warden hides the slab and stops it blocking; the boss adds its beat.
    Super::OnFrontBroken();
    // THE FIRST PUNISH WINDOW. Earned, not waited for: the front pool is spent
    // (O198) and the apparatus opens for the authored window, once per fight,
    // in whatever phase this lands. The punish is the exposed weak point plus
    // the front that no longer mitigates — no stagger, no multiplier, because
    // combat.md leaves that model open and this class does not fake one.
    FrontBreakWindowRemaining = FMath::Max(0.0f, PhaseParams.FrontBreakPunishSeconds);
    SetApparatusExposed(true);
}

void ABreakerBossEnemy::BuildMarshalApparatusDetails()
{
    if (!ApparatusVisual || !ApparatusSupportVisuals.IsEmpty()) return;
    // O2 ART PLACEHOLDERS: a compact ribbed command module on two visibly
    // telescoping rails. The module remains the only weapon-query surface.
    ApparatusVisual->SetRelativeScale3D(FVector(0.16f, 0.32f, 0.42f));
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    auto Part = [&](const FString& Name, USceneComponent* Parent, FVector Location, FVector Scale, FLinearColor Color)
    {
        UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, FName(*Name));
        AddInstanceComponent(Mesh);
        Mesh->SetupAttachment(Parent);
        Mesh->SetStaticMesh(Cube);
        Mesh->SetRelativeLocation(Location);
        Mesh->SetRelativeScale3D(Scale);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (Base)
        {
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Mesh);
            Material->SetVectorParameterValue(TEXT("Color"), Color);
            Mesh->SetMaterial(0, Material);
        }
        Mesh->RegisterComponent();
        return Mesh;
    };
    const FLinearColor Steel(0.075f, 0.095f, 0.10f);
    const FLinearColor Edge(0.30f, 0.34f, 0.33f);
    const FLinearColor Copper(0.48f, 0.24f, 0.08f);
    for (int32 Face : { -1, 1 })
    {
        const FString Prefix = FString::Printf(TEXT("MarshalModule%d"), Face);
        for (int32 Side : { -1, 1 })
        {
            Part(Prefix + FString::Printf(TEXT("Rail%d"), Side), ApparatusVisual,
                FVector(Face * 52.0f, Side * 45.0f, 0), FVector(0.08f, 0.10f, 1.0f), Steel);
            Part(Prefix + FString::Printf(TEXT("Cap%d"), Side), ApparatusVisual,
                FVector(Face * 52.0f, 0, Side * 45.0f), FVector(0.08f, 0.80f, 0.10f), Edge);
        }
        for (int32 Rib = 0; Rib < 5; ++Rib)
            Part(Prefix + FString::Printf(TEXT("CoolingRib%d"), Rib), ApparatusVisual,
                FVector(Face * 52.0f, 0, -28.0f + Rib * 14.0f), FVector(0.06f, 0.72f, 0.045f), Steel);
        Part(Prefix + TEXT("CoilSpine"), ApparatusVisual,
            FVector(Face * 54.0f, 0, 0), FVector(0.06f, 0.08f, 0.72f), Copper);
    }
    ApparatusSupportVisuals.Add(Part(TEXT("MarshalMastMount"), BodyCollision,
        ApparatusRestLocation + FVector(0, 0, -25), FVector(0.20f, 0.36f, 0.10f), Steel));
    for (int32 Side : { -1, 1 })
    {
        UStaticMeshComponent* Outer = Part(FString::Printf(TEXT("MarshalMastOuter%d"), Side), BodyCollision,
            ApparatusRestLocation, FVector(0.065f, 0.065f, 0.10f), Steel);
        UStaticMeshComponent* Inner = Part(FString::Printf(TEXT("MarshalMastInner%d"), Side), BodyCollision,
            ApparatusRestLocation, FVector(0.038f, 0.038f, 0.10f), Edge);
        ApparatusMastOuter.Add(Outer);
        ApparatusMastInner.Add(Inner);
        ApparatusSupportVisuals.Add(Outer);
        ApparatusSupportVisuals.Add(Inner);
    }
    UpdateApparatus(ApparatusPoseAlpha);
}
void ABreakerBossEnemy::ApplyBodyMesh()
{
    Super::ApplyBodyMesh();
    // Body fitting attaches the generic weak point to Head. The Marshal's
    // target is its command hardware, including after a mesh reapply/revive.
    RefreshApparatusWeakPoint();
}

void ABreakerBossEnemy::RefreshApparatusWeakPoint()
{
    if (GetClass() != ABreakerBossEnemy::StaticClass()) return;
    if (WeakPoint) WeakPoint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (WeakPointVisual) WeakPointVisual->SetVisibility(false, true);
    if (ApparatusVisual)
    {
        ApparatusVisual->ComponentTags.AddUnique(TEXT("WeakPoint"));
        ApparatusVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
        ApparatusVisual->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
        ApparatusVisual->SetCollisionEnabled(bApparatusExposed && bApparatusBodyVisible
            ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }
}

void ABreakerBossEnemy::SetApparatusExposed(bool bExposed)
{
    // Presentation writes are intentionally idempotent: the initial false
    // state must close inherited collision, and body restoration may reset it.
    bApparatusExposed = bExposed;
    if (ApparatusMaterial) UpdateApparatus(ApparatusPoseAlpha);
    if (GetClass() == ABreakerBossEnemy::StaticClass())
    {
        RefreshApparatusWeakPoint();
        return;
    }
    if (WeakPoint) WeakPoint->SetCollisionEnabled(bExposed && bApparatusBodyVisible
        ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    if (WeakPointVisual) WeakPointVisual->SetVisibility(bExposed && bApparatusBodyVisible, true);
}

void ABreakerBossEnemy::SetBodyVisible(bool bVisible)
{
    bApparatusBodyVisible = bVisible;
    Super::SetBodyVisible(bVisible);
    if (ApparatusVisual) ApparatusVisual->SetVisibility(bVisible, true);
    for (UStaticMeshComponent* Support : ApparatusSupportVisuals)
        if (Support) Support->SetVisibility(bVisible, true);
    SetApparatusExposed(bApparatusExposed);
}
void ABreakerBossEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Combat && Combat->IsStaggered()) return;
    if (!HasAuthority() || IsDeadEnemy() || !GetWorld()) return;

    UpdatePhase();
    UpdateAddGate();
    TickGalleryRespawn(DeltaSeconds);

    // The front-break window runs down here, not in the engaged tick, so a
    // boss that loses its player mid-window still closes it on time. When it
    // expires the apparatus closes — unless an order raise or Commitment is
    // holding it open, which the pure rule decides.
    if (UBreakerBossPhaseLibrary::AdvanceBreakWindow(FrontBreakWindowRemaining, DeltaSeconds))
    {
        SetApparatusExposed(UBreakerBossPhaseLibrary::IsPunishWindowOpen(Phase, bOrderRaiseActive, FrontBreakWindowRemaining));
    }

    // The DEPLOY spawn delay: adds appear this long after the raise finishes,
    // so the pointed alcove is previewed before anything comes out of it
    // (§5.1 — spawns are ALWAYS previewed).
    if (DeploySpawnCountdown > 0.0f)
    {
        DeploySpawnCountdown -= DeltaSeconds;
        if (DeploySpawnCountdown <= 0.0f)
        {
            DeploySpawnCountdown = -1.0f;
            SpawnDeployAdds(GetActorLocation() + PendingOrderOffset);
        }
    }
}

void ABreakerBossEnemy::InterruptCombatAction()
{
    Super::InterruptCombatAction();
    bOrderRaiseActive = false;
    OrderRaiseElapsed = 0;
    ActiveOrder = EBreakerBossOrder::None;
    TimeSinceLastOrder = 0;
    DeploySpawnCountdown = -1;
    UpdateApparatus(0);
    SetApparatusExposed(UBreakerBossPhaseLibrary::IsPunishWindowOpen(Phase, false, FrontBreakWindowRemaining));
}

void ABreakerBossEnemy::UpdatePhase()
{
    if (!Attributes) return;
    const float MaxHealth = Attributes->GetMaxHealth();
    if (MaxHealth <= 0.0f) return;
    const float Fraction = Attributes->GetHealth() / MaxHealth;

    // The gated step: with a zero reduction (the Marshal) this is AdvancePhase
    // exactly; with one authored, live adds hold the gate until they are dead.
    const EBreakerBossPhase Next = UBreakerBossPhaseLibrary::AdvancePhaseGated(Phase, Fraction, PhaseParams, CountLiveAdds());
    if (Next != Phase) EnterPhase(Next);
}

int32 ABreakerBossEnemy::CountLiveAdds()
{
    LiveAdds.RemoveAll([](const TWeakObjectPtr<ABreakerEnemy>& Add)
    {
        return !Add.IsValid() || Add->IsDeadEnemy();
    });
    GalleryLattices.RemoveAll([](const TWeakObjectPtr<ABreakerRangedEnemy>& Lattice)
    {
        return !Lattice.IsValid() || Lattice->IsDeadEnemy();
    });
    return LiveAdds.Num() + GalleryLattices.Num();
}

void ABreakerBossEnemy::UpdateAddGate()
{
    if (!Combat) return;
    const float Multiplier = UBreakerBossPhaseLibrary::AddGateIncomingMultiplier(Phase, CountLiveAdds(), PhaseParams);
    const bool bWantGate = Multiplier < 1.0f;
    if (bWantGate == bAddGatePushed) return;
    // Pushed and removed on the CROSSING only, on the boss's own component, so
    // a boss with no reduction never touches its modifier map at all and a
    // boss with one carries exactly one keyed entry while adds stand.
    if (bWantGate) Combat->PushIncomingDamageModifier(BreakerBossRuntime::BreakerBossAddGateKey, Multiplier);
    else Combat->RemoveIncomingDamageModifier(BreakerBossRuntime::BreakerBossAddGateKey);
    bAddGatePushed = bWantGate;
}

void ABreakerBossEnemy::EnterPhase(EBreakerBossPhase NewPhase)
{
    Phase = NewPhase;
    OnPhaseChanged.Broadcast(Phase);

    // Every phase rewrite is read out of the pure library, so the fight's shape
    // is checkable in automation and there is one place to change it.
    MoveSpeed = BaseBossMoveSpeed * UBreakerBossPhaseLibrary::GetPhaseSpeedMultiplier(Phase, PhaseParams);
    SweepCooldownSeconds = UBreakerBossPhaseLibrary::GetPhaseSweepCooldown(Phase, BaseSweepCooldown, PhaseParams);
    SlamCooldownSeconds = UBreakerBossPhaseLibrary::GetPhaseSlamCooldown(Phase, BaseSlamCooldown, PhaseParams);

    // An order in flight does not survive a phase change: the phase decides
    // what the order MEANS, and resolving a DEPLOY as a FIRE would be
    // incomprehensible. The front-break window DOES survive it: it is earned
    // once per fight, and FrontBreakWindowRemaining is deliberately not
    // touched here.
    bOrderRaiseActive = false;
    ActiveOrder = EBreakerBossOrder::None;
    OrderRaiseElapsed = 0.0f;
    TimeSinceLastOrder = 0.0f;
    UpdateApparatus(0.0f);

    switch (Phase)
    {
    case EBreakerBossPhase::Suppression:
        // "Two Lattices spawn permanently on the N and S galleries and do not
        // leave" (§3.4). They are its guns; killing them is worth doing, and
        // they come back, so timing matters.
        SpawnGalleryLattices();
        break;

    case EBreakerBossPhase::Commitment:
        // It stops commanding and fights. §3.4: each slam now leaves a
        // lingering hazard — the Cascading modifier's behaviour, reused
        // deliberately because the player has already learned to read it on
        // elites. The arena degrades and the fight ends because the floor runs
        // out, which is a movement-game ending rather than a DPS check.
        bSlamLeavesHazard = true;
        // "The apparatus stays permanently exposed, because it has stopped
        // commanding." Its damage rises and its defence falls.
        SetApparatusExposed(true);
        UpdateApparatus(1.0f);
        break;

    case EBreakerBossPhase::Deployment:
    default:
        break;
    }
}

FVector ABreakerBossEnemy::PickOrderTargetOffset()
{
    const TArray<FVector>& Pool = Phase == EBreakerBossPhase::Suppression ? GalleryOffsets : AlcoveOffsets;
    if (Pool.IsEmpty()) return FVector::ZeroVector;
    // Round-robin rather than random. §3.4's player lesson is "the adds are not
    // ambient — it CHOSE that corner", and a rotation is legible as a choice in
    // a way a random draw that repeats a corner three times is not.
    const FVector Chosen = Pool[OrderTargetIndex % Pool.Num()];
    ++OrderTargetIndex;
    return Chosen;
}

void ABreakerBossEnemy::TickEngagedBehaviour(ABreakerCharacter* Player, float Distance, float DeltaSeconds,
    FVector& OutDirection, float& OutSpeedScale)
{
    if (!Player || !GetWorld()) return;

    // The order machine runs BEFORE the Warden fight, and an order in progress
    // takes priority over both attacks. That is the whole read: an apparatus in
    // the air means "something else is about to happen", and a boss that swept
    // you mid-order would make the tell unreadable.
    if (bOrderRaiseActive)
    {
        const float RaiseSeconds = UBreakerBossPhaseLibrary::GetOrderRaiseSeconds(Phase, PhaseParams);
        OrderRaiseElapsed += DeltaSeconds;
        UpdateApparatus(UBreakerRangedBehaviorLibrary::GetTelegraphAlpha(OrderRaiseElapsed, RaiseSeconds));

        // It plants to give an order, and it POINTS: the direction is the
        // information, so it faces the alcove or gallery rather than the player.
        OutDirection = FVector::ZeroVector;
        DesiredFacing = PendingOrderOffset.GetSafeNormal2D();
        StateLabel = FString::Printf(TEXT("ORDER: %s"),
            *UBreakerBossPhaseLibrary::GetOrderName(ActiveOrder));

        if (OrderRaiseElapsed >= RaiseSeconds)
        {
            ResolveOrder();
        }
        return;
    }

    // Cadence. Phase 3 returns a negative interval and therefore never fires.
    const float Interval = UBreakerBossPhaseLibrary::GetOrderIntervalSeconds(Phase, PhaseParams);
    if (UBreakerBossPhaseLibrary::AdvanceOrderClock(TimeSinceLastOrder, DeltaSeconds, Interval))
    {
        BeginOrder();
        return;
    }

    // Otherwise it is a Warden. Same sweep, same slam, same facing armour, same
    // telegraphs — the player is not asked to learn a second melee vocabulary.
    Super::TickEngagedBehaviour(Player, Distance, DeltaSeconds, OutDirection, OutSpeedScale);
    // §3.4 phase 2: "the boss also begins rotating to face the player
    // continuously", so the rear weak point must be earned by out-turning it.
    // The Warden already always faces; what changes is that in phase 1 it is
    // slow enough to walk around and in phase 3 it is not.
    StateLabel = FString::Printf(TEXT("%s / %s"),
        *UBreakerBossPhaseLibrary::GetPhaseName(Phase), *StateLabel);
}

void ABreakerBossEnemy::BeginOrder()
{
    ActiveOrder = UBreakerBossPhaseLibrary::GetOrderForPhase(Phase);
    if (ActiveOrder == EBreakerBossOrder::None) return;

    bOrderRaiseActive = true;
    OrderRaiseElapsed = 0.0f;
    PendingOrderOffset = PickOrderTargetOffset();
    // THE PUNISH WINDOW OPENS HERE. §3.4 calls it generous on purpose: it is
    // the only moment in phases 1 and 2 that the rear weak point clears the
    // shoulder line and can be hit from the front, and O1's passive defence
    // means the player spends it repositioning rather than reacting.
    SetApparatusExposed(true);
}

void ABreakerBossEnemy::ResolveOrder()
{
    bOrderRaiseActive = false;
    OrderRaiseElapsed = 0.0f;
    ++OrdersGiven;

    switch (ActiveOrder)
    {
    case EBreakerBossOrder::Deploy:
        // The alcove is pointed at first and the adds arrive after a delay, so
        // the player can pre-aim or reposition (§3.4).
        DeploySpawnCountdown = FMath::Max(0.0f, PhaseParams.DeploySpawnDelaySeconds);
        if (DeploySpawnCountdown <= 0.0f)
        {
            DeploySpawnCountdown = -1.0f;
            SpawnDeployAdds(GetActorLocation() + PendingOrderOffset);
        }
        break;

    case EBreakerBossOrder::Fire:
        CommandGalleryVolley();
        break;

    default:
        break;
    }

    ActiveOrder = EBreakerBossOrder::None;
    // The order window closes — unless the front-break window is still running
    // or phase 3 has already opened it for good.
    SetApparatusExposed(UBreakerBossPhaseLibrary::IsPunishWindowOpen(Phase, false, FrontBreakWindowRemaining));
    UpdateApparatus(Phase == EBreakerBossPhase::Commitment ? 1.0f : 0.0f);
}

void ABreakerBossEnemy::UpdateApparatus(float Alpha)
{
    ApparatusPoseAlpha = Alpha;
    // The resting front-break window is exposed too: the actual hardware
    // must advertise it even when no Orders raise is moving the apparatus.
    const float GlowAlpha = GetClass() == ABreakerBossEnemy::StaticClass() && bApparatusExposed
        ? FMath::Max(Alpha, 0.65f) : Alpha; // O2 PLACEHOLDER: resting exposure glow floor.
    if (ApparatusVisual)
    {
        // It LIFTS. That is what makes a rear weak point hittable from the
        // front, and it is the same gesture for both orders by design (§3.4) —
        // the player reads which order from where it points, not from the pose.
        ApparatusVisual->SetRelativeLocation(ApparatusRestLocation + FVector(0.0f, 0.0f, ApparatusRaiseCm * Alpha));
    }
    // Two overlapping stages connect the backpack mount to the moving module;
    // they retract rather than leaving a detached rectangle above the head.
    const float Extension = FMath::Max(0.0f, ApparatusRaiseCm * Alpha);
    const float OuterLength = 10.0f + Extension * 0.55f;
    const float InnerLength = 10.0f + Extension * 0.55f;
    for (int32 Index = 0; Index < ApparatusMastOuter.Num(); ++Index)
    {
        const float Side = Index == 0 ? -1.0f : 1.0f;
        const FVector Start = ApparatusRestLocation + FVector(0, Side * 10.0f, -25.0f);
        ApparatusMastOuter[Index]->SetRelativeLocation(Start + FVector(0, 0, OuterLength * 0.5f));
        ApparatusMastOuter[Index]->SetRelativeScale3D(FVector(0.065f, 0.065f, OuterLength / 100.0f));
        ApparatusMastInner[Index]->SetRelativeLocation(Start + FVector(0, 0, Extension + 5.0f - InnerLength * 0.5f));
        ApparatusMastInner[Index]->SetRelativeScale3D(FVector(0.038f, 0.038f, InnerLength / 100.0f));
    }
    const FLinearColor Hot = ActiveOrder == EBreakerBossOrder::Fire ? ApparatusFireColor : ApparatusDeployColor;
    if (ApparatusMaterial)
    {
        ApparatusMaterial->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(ApparatusIdleColor, Hot, GlowAlpha));
    }
    if (ApparatusLight)
    {
        ApparatusLight->SetLightColor(Hot);
        // Squared, so the last third of the raise is where it really lights —
        // the same curve the Lattice telegraph uses, so the two tells feel like
        // one vocabulary.
        ApparatusLight->SetIntensity(ApparatusLightIntensity * GlowAlpha * GlowAlpha);
    }
}

void ABreakerBossEnemy::SpawnDeployAdds(const FVector& AlcoveWorldLocation)
{
    if (!GetWorld() || !HasAuthority() || !DeployAddClass) return;
    if (!UBreakerBossPhaseLibrary::ShouldSpawnAdds(Phase)) return;

    CountLiveAdds();
    // §5.3's density ceiling, enforced where the density is created. A boss
    // that deploys into an uncleared field turns a 12-enemy cap into 30.
    const int32 Room = FMath::Max(0, MaximumLiveAdds - LiveAdds.Num());
    const int32 ToSpawn = FMath::Min(FMath::Max(0, AddsPerDeploy), Room);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (int32 Index = 0; Index < ToSpawn; ++Index)
    {
        const FVector Offset(0.0f, 220.0f * (Index - (ToSpawn - 1) * 0.5f), 0.0f);
        ABreakerEnemy* Add = GetWorld()->SpawnActor<ABreakerEnemy>(
            DeployAddClass, AlcoveWorldLocation + Offset, GetActorRotation(), SpawnParams);
        if (!Add) continue;
        // Adds inherit the boss's AREA LEVEL, which is the only difficulty
        // input any of this has (O27). They do not respawn — the boss makes
        // more when it chooses to, which is the mechanic.
        Add->ConfigureWave(GetAreaLevel());
        LiveAdds.Add(Add);
    }
}

void ABreakerBossEnemy::SpawnGalleryLattices()
{
    if (!GetWorld() || !HasAuthority() || !GalleryLatticeClass || GalleryOffsets.IsEmpty()) return;
    CountLiveAdds();

    // §5.3's hard cap of 3 live Lattices, regardless of anything: "four
    // converging projectile sources removes all safe ground; this is the single
    // most dangerous scaling knob."
    const int32 Wanted = FMath::Min(FMath::Clamp(GalleryLatticeCount, 0, 3), GalleryOffsets.Num());
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    for (int32 Index = GalleryLattices.Num(); Index < Wanted; ++Index)
    {
        ABreakerRangedEnemy* Lattice = GetWorld()->SpawnActor<ABreakerRangedEnemy>(
            GalleryLatticeClass, GetActorLocation() + GalleryOffsets[Index % GalleryOffsets.Num()],
            GetActorRotation(), SpawnParams);
        if (!Lattice) continue;
        Lattice->ConfigureWave(GetAreaLevel());
        GalleryLattices.Add(Lattice);
    }
}

void ABreakerBossEnemy::TickGalleryRespawn(float DeltaSeconds)
{
    if (Phase != EBreakerBossPhase::Suppression) return;
    CountLiveAdds();
    const int32 Wanted = FMath::Min(FMath::Clamp(GalleryLatticeCount, 0, 3), GalleryOffsets.Num());
    if (GalleryLattices.Num() >= Wanted)
    {
        GalleryRespawnCountdown = -1.0f;
        return;
    }

    // §3.4: "they respawn 12s after death". The delay is the mechanic — killing
    // them is worth doing precisely because it buys a measured window rather
    // than a permanent one.
    if (GalleryRespawnCountdown < 0.0f)
    {
        GalleryRespawnCountdown = FMath::Max(0.0f, GalleryRespawnSeconds);
        return;
    }
    GalleryRespawnCountdown -= DeltaSeconds;
    if (GalleryRespawnCountdown <= 0.0f)
    {
        GalleryRespawnCountdown = -1.0f;
        SpawnGalleryLattices();
    }
}

void ABreakerBossEnemy::CommandGalleryVolley()
{
    // §3.4: "both Lattices volley SIMULTANEOUSLY at the player's position,
    // ignoring their own cadence." The simultaneity is the mechanic — six
    // projectiles converging on one point is trivially avoided by moving and
    // near-unavoidable if the player is reloading in the open. A discipline
    // check, not a reflex check.
    for (const TWeakObjectPtr<ABreakerRangedEnemy>& Weak : GalleryLattices)
    {
        if (ABreakerRangedEnemy* Lattice = Weak.Get())
        {
            if (!Lattice->IsDeadEnemy()) Lattice->CommandVolley();
        }
    }
}

void ABreakerBossEnemy::HandleDeath()
{
    SetApparatusExposed(false);
    Super::HandleDeath();
    // The gallery Lattices are its guns and they die with it. §3.4's phase 3
    // says "anything alive stays alive" about the ADDS, which are Skitters the
    // player can still fight; leaving two respawning turrets alive after the
    // boss is dead would be a fight with no end condition.
    for (const TWeakObjectPtr<ABreakerRangedEnemy>& Weak : GalleryLattices)
    {
        if (ABreakerRangedEnemy* Lattice = Weak.Get()) Lattice->Destroy();
    }
    GalleryLattices.Reset();
    // A dead boss takes no more damage, but the modifier map is state and a
    // pooled or revived body must not wake up gated.
    if (bAddGatePushed && Combat)
    {
        Combat->RemoveIncomingDamageModifier(BreakerBossRuntime::BreakerBossAddGateKey);
        bAddGatePushed = false;
    }
    OnBossDefeated.Broadcast();
}
