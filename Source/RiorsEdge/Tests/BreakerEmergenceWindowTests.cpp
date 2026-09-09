#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
