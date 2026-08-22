#include "Combat/BreakerBossEnemy.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
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

    // Captured AFTER the Warden's BeginPlay has published its armour, so the
    // phase-3 halving works off the real number rather than a copy of it.
    BaseFrontalArmor = FrontalArmor;
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
    return UBreakerBossPhaseLibrary::IsApparatusExposed(Phase, bOrderRaiseActive);
}

void ABreakerBossEnemy::SetApparatusExposed(bool bExposed)
{
    if (bApparatusExposed == bExposed) return;
    bApparatusExposed = bExposed;
    // Literally untargetable when closed rather than a damage filter: a weak
    // point the player can hit but that scores nothing teaches them the weak
    // point does not work.
    if (WeakPoint) WeakPoint->SetCollisionEnabled(bExposed
        ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    if (WeakPointVisual) WeakPointVisual->SetVisibility(bExposed, true);
}

void ABreakerBossEnemy::SetBodyVisible(bool bVisible)
{
    Super::SetBodyVisible(bVisible);
    if (ApparatusVisual) ApparatusVisual->SetVisibility(bVisible, true);
    // The weak point follows the apparatus rule, not the body rule: showing it
    // on a respawn would open a window §3.2 says is closed.
    if (WeakPointVisual) WeakPointVisual->SetVisibility(bVisible && bApparatusExposed, true);
}

void ABreakerBossEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || IsDeadEnemy() || !GetWorld()) return;

    UpdatePhase();
    TickGalleryRespawn(DeltaSeconds);

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

void ABreakerBossEnemy::UpdatePhase()
{
    if (!Attributes) return;
    const float MaxHealth = Attributes->GetMaxHealth();
    if (MaxHealth <= 0.0f) return;
    const float Fraction = Attributes->GetHealth() / MaxHealth;

    const EBreakerBossPhase Next = UBreakerBossPhaseLibrary::AdvancePhase(Phase, Fraction, PhaseParams);
    if (Next != Phase) EnterPhase(Next);
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
    FrontalArmor = UBreakerBossPhaseLibrary::GetPhaseFrontalArmor(Phase, BaseFrontalArmor, PhaseParams);
    if (Attributes) Attributes->SetArmor(FMath::Max(0.0f, FrontalArmor));

    // An order in flight does not survive a phase change: the phase decides
    // what the order MEANS, and resolving a DEPLOY as a FIRE would be
    // incomprehensible.
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
    // The window closes — unless phase 3 has already opened it for good.
    SetApparatusExposed(UBreakerBossPhaseLibrary::IsApparatusExposed(Phase, false));
    UpdateApparatus(Phase == EBreakerBossPhase::Commitment ? 1.0f : 0.0f);
}

void ABreakerBossEnemy::UpdateApparatus(float Alpha)
{
    if (ApparatusVisual)
    {
        // It LIFTS. That is what makes a rear weak point hittable from the
        // front, and it is the same gesture for both orders by design (§3.4) —
        // the player reads which order from where it points, not from the pose.
        ApparatusVisual->SetRelativeLocation(ApparatusRestLocation + FVector(0.0f, 0.0f, ApparatusRaiseCm * Alpha));
    }
    const FLinearColor Hot = ActiveOrder == EBreakerBossOrder::Fire ? ApparatusFireColor : ApparatusDeployColor;
    if (ApparatusMaterial)
    {
        ApparatusMaterial->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(ApparatusIdleColor, Hot, Alpha));
    }
    if (ApparatusLight)
    {
        ApparatusLight->SetLightColor(Hot);
        // Squared, so the last third of the raise is where it really lights —
        // the same curve the Lattice telegraph uses, so the two tells feel like
        // one vocabulary.
        ApparatusLight->SetIntensity(ApparatusLightIntensity * Alpha * Alpha);
    }
}

void ABreakerBossEnemy::SpawnDeployAdds(const FVector& AlcoveWorldLocation)
{
    if (!GetWorld() || !HasAuthority() || !DeployAddClass) return;
    if (!UBreakerBossPhaseLibrary::ShouldSpawnAdds(Phase)) return;

    LiveAdds.RemoveAll([](const TWeakObjectPtr<ABreakerEnemy>& Add)
    {
        return !Add.IsValid() || Add->IsDeadEnemy();
    });
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
    GalleryLattices.RemoveAll([](const TWeakObjectPtr<ABreakerRangedEnemy>& Lattice)
    {
        return !Lattice.IsValid() || Lattice->IsDeadEnemy();
    });

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
    GalleryLattices.RemoveAll([](const TWeakObjectPtr<ABreakerRangedEnemy>& Lattice)
    {
        return !Lattice.IsValid() || Lattice->IsDeadEnemy();
    });
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
    OnBossDefeated.Broadcast();
}
