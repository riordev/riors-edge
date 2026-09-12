#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"

// ---------------------------------------------------------------------------
// THE EMERGENCE WINDOW. A body that has just arrived cannot be deleted in the
// frame it appears.
//
// This is the SHIPPED immunity shape and not a new one — a keyed 0.0 incoming
// modifier with an owned timer, exactly as Hard Stop does under Spend to Live.
// O228 forbids routing binary immunity through PushWindowIncomingDamageModifier
// ("numerical defence only; binary immunity must use ordinary ownership"), so
// the test asserts the OWNERSHIP as well as the effect: the window must end on
// its own clock, and it must end early on death so a pooled corpse cannot
// revive still carrying it.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEmergenceWindowTest,
    "RiorsEdge.Combat.Emergence.Window",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEmergenceWindowTest::RunTest(const FString& Parameters)
{
    // The shipped configuration against a default-constructed body: a window
    // that shipped at zero would protect nothing and no other assertion here
    // would notice.
    const ABreakerEnemy* Shipped = GetDefault<ABreakerEnemy>();
    if (!TestNotNull(TEXT("the enemy class default exists"), Shipped)) return false;
    TestEqual(TEXT("the shipped emergence window is 0.8s"), Shipped->EmergenceProtectedSeconds, 0.8f, 0.0001f);
    TestTrue(TEXT("and it is long enough to survive a frame"), Shipped->EmergenceProtectedSeconds > 0.0f);

    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("emergence world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    // The window ends on a world timer, so this fixture has to be tickable.
    World->InitializeActorsForPlay(FURL());
    const uint64 EntryFrame = GFrameCounter;
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter = EntryFrame; };
    auto Clock = [&](float Seconds) { for (float T = 0; T < Seconds; T += .02f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .02f); } };

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // A body wired the way ABreakerEnemy::BeginPlay wires one — nothing more.
    // Without this the combat component holds no attribute set and EVERY hit
    // returns an empty result, which would make the protection assertions
    // below pass against a body that simply cannot be damaged at all.
    struct FArrival { ABreakerEnemy* Enemy = nullptr; UBreakerCombatComponent* Combat = nullptr; UBreakerAttributeSet* Health = nullptr; };
    auto MakeArrival = [&](const FVector& Where) -> FArrival
    {
        FArrival Made;
        Made.Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), Where, FRotator::ZeroRotator, SpawnParams);
        if (!Made.Enemy) return Made;
        Made.Enemy->SetActorTickEnabled(false);
        Made.Combat = Made.Enemy->FindComponentByClass<UBreakerCombatComponent>();
        Made.Health = Cast<UBreakerAttributeSet>(Made.Enemy->GetDefaultSubobjectByName(TEXT("Attributes")));
        UAbilitySystemComponent* ASC = Made.Enemy->GetAbilitySystemComponent();
        if (!Made.Combat || !Made.Health || !ASC) { Made.Enemy = nullptr; return Made; }
        ASC->InitAbilityActorInfo(Made.Enemy, Made.Enemy);
        ASC->AddAttributeSetSubobject(Made.Health);
        ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetMaxHealthAttribute(), 1000.0f);
        ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetHealthAttribute(), 1000.0f);
        Made.Combat->BindAttributes(Made.Health);
        return Made;
    };
    auto Hit = [](float Damage)
    {
        FBreakerDamageRequest Request;
        Request.BaseDamage = Damage;
        Request.bCanCritical = false;
        Request.bBypassShield = true;
        return Request;
    };

    // CONTROL. Before anything is claimed about protection, prove this fixture
    // can lose health at all.
    FArrival Plain = MakeArrival(FVector(1000, 0, 0));
    if (!TestNotNull(TEXT("a plain body"), Plain.Enemy)) return false;
    Plain.Combat->ReceiveDamage(Hit(100.0f));
    if (!TestTrue(TEXT("an unprotected body loses health"), Plain.Health->GetHealth() < 1000.0f)) return false;

    // INSIDE the window: a lethal hit lands and does nothing.
    FArrival Emerging = MakeArrival(FVector::ZeroVector);
    if (!TestNotNull(TEXT("an arriving body"), Emerging.Enemy)) return false;
    Emerging.Enemy->GrantEmergenceWindow();
    const float Before = Emerging.Health->GetHealth();
    if (!TestTrue(TEXT("the arriving body starts with health to lose"), Before > 0.0f)) return false;
    const FBreakerDamageResult Blocked = Emerging.Combat->ReceiveDamage(Hit(1000000.0f));
    TestEqual(TEXT("an emerging body takes no health damage"), Blocked.HealthDamage, 0.0f, 0.0001f);
    TestFalse(TEXT("and cannot be killed on arrival"), Emerging.Enemy->IsDeadEnemy());
    TestEqual(TEXT("its health is untouched"), Emerging.Health->GetHealth(), Before, 0.0001f);

    // AFTER it, on the window's OWN clock. Without this half the test would
    // pass against a body that was simply invulnerable forever.
    Clock(Shipped->EmergenceProtectedSeconds + 0.1f);
    Emerging.Combat->ReceiveDamage(Hit(1000000.0f));
    TestEqual(TEXT("once the window closes the same hit lands in full"), Emerging.Health->GetHealth(), 0.0f, 0.0001f);

    // OWNERSHIP, the half O228 actually cares about. EndEmergenceWindow is the
    // same function bound to death, so a body that dies mid-window cannot
    // revive from the pool still carrying the modifier.
    FArrival Second = MakeArrival(FVector(500, 0, 0));
    if (!TestNotNull(TEXT("a second body"), Second.Enemy)) return false;
    Second.Enemy->GrantEmergenceWindow();
    Second.Enemy->EndEmergenceWindow();
    Second.Combat->ReceiveDamage(Hit(1000000.0f));
    TestEqual(TEXT("a window ended early stops protecting immediately"), Second.Health->GetHealth(), 0.0f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// O274: A BODY EMERGES BEFORE IT HUNTS. The window above kept an arriving body
// alive; it did not keep it from hunting, so a body stepping out of a tear
// acquired the player standing on the tear on frame one. While the emergence
// clock runs the tick takes the patrol branch regardless of distance: it
// prints EMERGING, holds no threat target, and fires nothing. When the clock
// ends the next tick engages as it always did.
//
// The emerging clock and the damage-immune clock are ONE dial, so the test
// asserts the relationship as shipped and that both halves end on the same
// tick: the hit that lands is the tick that hunts.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEmergenceNoThreatTest,
    "RiorsEdge.Combat.Emergence.NoThreat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEmergenceNoThreatTest::RunTest(const FString& Parameters)
{
    // Shipped configuration against the default-constructed body. One dial:
    // the emerging window IS the protected window, and both are non-zero.
    const ABreakerEnemy* Shipped = GetDefault<ABreakerEnemy>();
    if (!TestNotNull(TEXT("the enemy class default exists"), Shipped)) return false;
    TestEqual(TEXT("the emerging window is the protected window (one dial)"),
        Shipped->GetEmergingSeconds(), Shipped->EmergenceProtectedSeconds, 0.0001f);
    TestTrue(TEXT("the shipped emerging window is longer than a frame"), Shipped->GetEmergingSeconds() > 0.0f);
    TestTrue(TEXT("the shipped protected window is longer than a frame"), Shipped->EmergenceProtectedSeconds > 0.0f);
    TestFalse(TEXT("a default body is not emerging"), Shipped->IsEmerging());
    // The player stands this close to the tear. It must be INSIDE the shipped
    // detection range or the patrol branch below proves nothing.
    const float PlayerDistance = 500.0f;
    if (!TestTrue(TEXT("the player stands inside the shipped detection range"),
        Shipped->GetDetectionRange() > PlayerDistance)) return false;

    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("emergence world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 EntryFrame = GFrameCounter;
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter = EntryFrame; };
    // The world clock runs the window's timer; the body's own tick is driven
    // by hand so every step can be inspected.
    auto Clock = [&](float Seconds) { for (float T = 0; T < Seconds; T += .02f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .02f); } };

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // A live player: the threat selector's nearest-combatant fallback needs a
    // character with a living combat component, nothing more.
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>(ABreakerCharacter::StaticClass(),
        FVector(PlayerDistance, 0, 100), FRotator::ZeroRotator, SpawnParams);
    if (!TestNotNull(TEXT("a player on the tear"), Player)) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetProgression()->BindAttributes(Player->GetAttributes());

    // A body wired through its own BeginPlay, so Tick has the chassis, the
    // leash origin and the bound combat component it runs on in the game.
    ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(),
        FVector(0, 0, 100), FRotator::ZeroRotator, SpawnParams);
    if (!TestNotNull(TEXT("an arriving body"), Enemy)) return false;
    UBreakerAttributeSet* Health = Cast<UBreakerAttributeSet>(Enemy->GetDefaultSubobjectByName(TEXT("Attributes")));
    UBreakerCombatComponent* Combat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
    if (!TestNotNull(TEXT("the body's attributes"), Health) || !TestNotNull(TEXT("the body's combat"), Combat)) return false;
    Enemy->GetAbilitySystemComponent()->AddAttributeSetSubobject(Health);
    Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    Health->ApplyMaxHealth(1000.0f); Health->ApplyHealth(1000.0f);
    auto Hit = []()
    {
        FBreakerDamageRequest Request;
        Request.BaseDamage = 100.0f;
        Request.bCanCritical = false;
        Request.bBypassShield = true;
        return Request;
    };

    // CONTROL. Before anything is claimed about emerging, prove this fixture
    // engages at all: no window, player in range, one tick, a target.
    Enemy->Tick(0.0f);
    if (!TestTrue(TEXT("without a window the body hunts the player in range"), Enemy->GetThreatTarget() == Player)) return false;
    if (!TestNotEqual(TEXT("and its label is not the leash walk"), Enemy->GetEnemyStateLabel(), FString(TEXT("PATROL")))) return false;

    // ARMED. The window is one grant; the tick after it is the emergence.
    Enemy->GrantEmergenceWindow();
    TestTrue(TEXT("a granted window is an emerging body"), Enemy->IsEmerging());
    Enemy->Tick(0.0f);
    TestEqual(TEXT("an emerging body prints EMERGING"), Enemy->GetEnemyStateLabel(), FString(TEXT("EMERGING")));
    TestNull(TEXT("and holds no threat target, even the one it held a tick ago"), Enemy->GetThreatTarget());
    TestNull(TEXT("and has committed no attack target"), Enemy->GetCommittedAttackTarget());

    // THROUGH THE CLOCK. Every tick of the window, the same three facts; and
    // the damage half runs on the same clock, so a hit does nothing here.
    const float Window = Shipped->GetEmergingSeconds();
    bool bHeldThrough = true;
    for (float T = 0.0f; T < Window - 0.05f; T += 0.02f)
    {
        Clock(0.02f);
        Enemy->Tick(0.0f);
        bHeldThrough &= Enemy->IsEmerging() && Enemy->GetThreatTarget() == nullptr
            && Enemy->GetEnemyStateLabel() == TEXT("EMERGING");
    }
    TestTrue(TEXT("the body hunts nothing for the whole clock"), bHeldThrough);
    const float BeforeHit = Health->GetHealth();
    Combat->ReceiveDamage(Hit());
    TestEqual(TEXT("and the hit on the same clock does nothing"), Health->GetHealth(), BeforeHit, 0.0001f);

    // AFTER THE CLOCK. One clock, two halves: the tick that hunts is the tick
    // that can be hit.
    Clock(0.1f + 0.05f);
    TestFalse(TEXT("when the clock ends the body is no longer emerging"), Enemy->IsEmerging());
    Enemy->Tick(0.0f);
    TestTrue(TEXT("the next tick engages the player"), Enemy->GetThreatTarget() == Player);
    TestNotEqual(TEXT("and the label is no longer EMERGING"), Enemy->GetEnemyStateLabel(), FString(TEXT("EMERGING")));
    Combat->ReceiveDamage(Hit());
    TestTrue(TEXT("and the same hit now lands"), Health->GetHealth() < BeforeHit);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
