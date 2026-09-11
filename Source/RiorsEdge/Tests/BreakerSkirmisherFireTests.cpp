#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerEnemyProjectile.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"

// "The skirmishers always shoot other random enemies, not the player."
//
// They never did. The target was always the player and the aim was always
// muzzle-to-player; what went wrong was the ROUND. FireRound spawns deferred
// at Direction.Rotation(), arms a world-space velocity, then FinishSpawning
// runs UProjectileMovementComponent::InitializeComponent — which, with the
// engine-default bInitialVelocityInLocalSpace, rotated that velocity by the
// round's own yaw a second time. A shot at yaw θ flew at 2θ. A player on world
// +X (θ = 0) was hit; a player on +Y was shot at -X, which from the owner's
// seat reads as "it fired at some other enemy".
//
// +Y is the yaw this test stands on because it is the one where the doubling
// is a clean quarter turn: before the fix the dot with +Y is exactly 0.

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    // Spawns a round the way ABreakerSkirmisherEnemy::FireRound does — deferred,
    // at Direction.Rotation(), armed, then finished — with the two things that
    // would blur the assertion left out: no VRandCone spread (a random cone is
    // not a rule) and no muzzle mesh lookup (the muzzle's position is not the
    // bug; a fixed 60 cm offset along the aim keeps the direction exactly +Y).
    ABreakerEnemyProjectile* BreakerSkirmisherFireSpawnRoundAsFireRoundDoes(
        UWorld* World, ABreakerSkirmisherEnemy* Skirmisher, const AActor* Target, FVector& OutDirection)
    {
        const FVector Muzzle = Skirmisher->GetActorLocation()
            + (Target->GetActorLocation() - Skirmisher->GetActorLocation()).GetSafeNormal() * 60.0f;
        OutDirection = (Target->GetActorLocation() - Muzzle).GetSafeNormal();
        if (OutDirection.IsNearlyZero()) return nullptr;

        const FTransform SpawnTransform(OutDirection.Rotation(), Muzzle);
        ABreakerEnemyProjectile* Round = World->SpawnActorDeferred<ABreakerEnemyProjectile>(
            Skirmisher->ProjectileClass, SpawnTransform, Skirmisher, Skirmisher,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Round) return nullptr;

        Round->VisualScale = 0.30f;
        Round->OrbColor = Skirmisher->MuzzleHotColor;
        Round->CollisionRadiusCm = 14.0f;

        FBreakerDamageRequest Shot;
        Shot.BaseDamage = Skirmisher->GetEffectiveAttackDamage() * FMath::Max(0.0f, Skirmisher->DamagePerRoundFraction);
        Shot.DamageFamily = EBreakerDamageFamily::Physical;
        Shot.bCanCritical = false;
        Shot.SetInstigator(Skirmisher);
        Round->InitializeProjectile(Shot, OutDirection, Skirmisher->ProjectileSpeed);
        Round->FinishSpawning(SpawnTransform);
        return Round;
    }

    float BreakerSkirmisherFireAlignment(const FVector& Velocity, const FVector& Axis)
    {
        return static_cast<float>(FVector::DotProduct(Velocity.GetSafeNormal(), Axis));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSkirmisherRoundFliesAtThePlayerTest,
    "RiorsEdge.Combat.SkirmisherRoundFliesAtThePlayer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSkirmisherRoundFliesAtThePlayerTest::RunTest(const FString& Parameters)
{
    // SHIPPED CONFIGURATION. The default-constructed base, and the class the
    // Skirmisher actually spawns, both arm in world space. This is the line
    // that fixes the bug; if a subclass ever flips it back the doubling returns
    // for every deferred spawn and no travel test would notice.
    {
        const ABreakerProjectileBase* Base = GetDefault<ABreakerProjectileBase>();
        if (!TestNotNull(TEXT("the projectile base has a default object"), Base)) return false;
        const auto* BaseMovement = Base->FindComponentByClass<UProjectileMovementComponent>();
        if (!TestNotNull(TEXT("the projectile base ships a movement component"), BaseMovement)) return false;
        TestFalse(TEXT("the shipped base arms its initial velocity in WORLD space"), BaseMovement->bInitialVelocityInLocalSpace);

        const ABreakerSkirmisherEnemy* ShippedSkirmisher = GetDefault<ABreakerSkirmisherEnemy>();
        if (!TestNotNull(TEXT("the Skirmisher has a default object"), ShippedSkirmisher)) return false;
        if (!TestNotNull(TEXT("the shipped Skirmisher has a projectile class"), ShippedSkirmisher->ProjectileClass.Get())) return false;
        const auto* Fired = GetDefault<ABreakerProjectileBase>(ShippedSkirmisher->ProjectileClass);
        const auto* FiredMovement = Fired ? Fired->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
        if (!TestNotNull(TEXT("the shipped Skirmisher round has a movement component"), FiredMovement)) return false;
        TestFalse(TEXT("the shipped Skirmisher round arms in WORLD space too"), FiredMovement->bInitialVelocityInLocalSpace);
    }

    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };

    // The player stands 1000 cm up world +Y. Bare character, attributes bound
    // so it is a live combatant; its BeginPlay is deliberately not dispatched
    // (it would load the owner's save).
    const FVector Origin(0, 0, 100);
    const FVector PlusY(0, 1, 0);
    auto* Player = World->SpawnActor<ABreakerCharacter>(Origin + PlusY * 1000.0f, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("real player character"), Player)) return false;
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);

    // Real late-spawn BeginPlay for the enemies and the round: FinishSpawning
    // must run the round's BeginPlay exactly as it does in play, because that
    // is where the enemy-safe ignore list (O217) is written.
    World->SetBegunPlay(true);

    auto* Skirmisher = World->SpawnActor<ABreakerSkirmisherEnemy>(Origin, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("real authored Skirmisher"), Skirmisher)) return false;
    Skirmisher->SetActorTickEnabled(false);
    if (auto* Movement = Skirmisher->GetMovementComponent()) Movement->SetComponentTickEnabled(false);

    // A base enemy on the same line, halfway to the player: the "other random
    // enemy" the owner believed was being shot at.
    auto* Trash = World->SpawnActor<ABreakerEnemy>(Origin + PlusY * 500.0f, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("real base enemy between them"), Trash)) return false;
    Trash->SetActorTickEnabled(false);
    if (auto* Movement = Trash->GetMovementComponent()) Movement->SetComponentTickEnabled(false);

    // One tick: the real selector picks the target.
    Skirmisher->Tick(0.01f);
    TestTrue(TEXT("the Skirmisher's threat target is the player"), Skirmisher->GetThreatTarget() == Player);
    TestFalse(TEXT("another enemy is never an eligible threat target"), Skirmisher->IsEligibleThreatTarget(Trash));

    // Now the round, spawned the way FireRound spawns it.
    FVector Direction = FVector::ZeroVector;
    ABreakerEnemyProjectile* Round = BreakerSkirmisherFireSpawnRoundAsFireRoundDoes(World, Skirmisher, Player, Direction);
    if (!TestNotNull(TEXT("the round spawned"), Round)) return false;
    TestTrue(TEXT("the aim itself is exactly +Y (the fixture is the yaw that exposed the doubling)"),
        BreakerSkirmisherFireAlignment(Direction, PlusY) > 0.9999f);
    TestTrue(TEXT("the round has begun play through FinishSpawning, as in play"), Round->HasActorBegunPlay());

    auto* Movement = Round->FindComponentByClass<UProjectileMovementComponent>();
    if (!TestNotNull(TEXT("the round has a movement component"), Movement)) return false;
    // THE BUG. Before the fix this dot is 0: a +Y aim (yaw 90) flew at yaw 180.
    TestTrue(*FString::Printf(TEXT("the round flies at the player, not at 2x its yaw (alignment %.4f)"),
            BreakerSkirmisherFireAlignment(Movement->Velocity, PlusY)),
        BreakerSkirmisherFireAlignment(Movement->Velocity, PlusY) > 0.999f);
    TestEqual(TEXT("and at the authored speed"), static_cast<float>(Movement->Velocity.Size()), Skirmisher->ProjectileSpeed, 0.5f);
    TestTrue(TEXT("the round faces the way it flies"),
        BreakerSkirmisherFireAlignment(Round->GetActorForwardVector(), PlusY) > 0.999f);

    // ENEMY-SAFE (O217): the enemy in the line is neither an obstacle nor a
    // target. The obstacle half is the collision's move-ignore list, written by
    // the round's BeginPlay.
    const auto* Sphere = Round->FindComponentByClass<USphereComponent>();
    if (!TestNotNull(TEXT("the round has its collision sphere"), Sphere)) return false;
    TestTrue(TEXT("the enemy in the line is on the round's move-ignore list"), Sphere->GetMoveIgnoreActors().Contains(Trash));
    TestTrue(TEXT("and so is the shooter"), Sphere->GetMoveIgnoreActors().Contains(Skirmisher));

    // The target half. ShouldDamageActor is protected, so it is proved through
    // the one public entry that consults it: a resolved impact on the enemy
    // latches (it is a refusal, not a miss) and costs it nothing.
    auto* TrashHealth = FindObject<UBreakerAttributeSet>(Trash, TEXT("Attributes"));
    if (!TestNotNull(TEXT("the base enemy has attributes"), TrashHealth)) return false;
    TestTrue(TEXT("the round carries real damage, so the refusal below is not vacuous"), Round->GetProjectileDamage().BaseDamage > 0.0f);
    const float TrashBefore = TrashHealth->GetHealth();
    TestTrue(TEXT("the base enemy is alive before the impact"), TrashBefore > 0.0f);
    Movement->StopMovementImmediately(); Movement->Deactivate();
    Round->Impact(Trash, Trash->GetActorLocation());
    TestTrue(TEXT("the impact latched"), Round->HasImpacted());
    TestEqual(TEXT("ShouldDamageActor(enemy) is false: the enemy took nothing"), TrashHealth->GetHealth(), TrashBefore);
    return true;
}

#endif
