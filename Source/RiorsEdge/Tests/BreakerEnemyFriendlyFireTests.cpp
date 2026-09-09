#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// O261: A PACK DOES NOT THIN ITSELF. An enemy death damages other enemies only
// where a player source says it does, and nothing authors such a source yet.
//
// This asserts the CONSEQUENCE rather than the flag, because the flag was
// never the problem — it shipped true and every reader of it was correct.
// What was wrong was what a player saw: kill one body in a pack and the
// neighbours lost 35% of their maximum health to a corpse, paying the player
// nothing for it. So the test kills one enemy beside another and looks at the
// survivor.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEnemyFriendlyFireTest,
    "RiorsEdge.Combat.EnemyFriendlyFire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyFriendlyFireTest::RunTest(const FString&)
{
    // The shipped configuration, before any world exists: the class default is
    // what every spawned body inherits, and it is the thing the ruling moves.
    TestFalse(TEXT("no enemy detonates on death by default (O261)"),
        GetDefault<ABreakerEnemy>()->DoesExplodeOnDeath());

    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };

    auto Spawn = [&](FVector P) -> ABreakerEnemy*
    {
        auto* E = World->SpawnActor<ABreakerEnemy>(P, FRotator::ZeroRotator);
        if (!E) return nullptr;
        E->SetAreaLevel(1);
        E->ConfigureCrowdProbe();
        E->DispatchBeginPlay();
        E->SetActorTickEnabled(false);
        if (auto* M = E->FindComponentByClass<UBreakerEnemyMovementComponent>()) M->SetComponentTickEnabled(false);
        if (auto* S = E->FindComponentByClass<UBreakerStatusComponent>()) S->SetComponentTickEnabled(false);
        return E;
    };

    // Well inside the radius the retired chain used, so a survivor at this
    // distance is the strongest statement the fixture can make.
    const FVector Origin(3000, 0, 0);
    auto* Dying = Spawn(Origin);
    auto* Neighbour = Spawn(Origin + FVector(60, 0, 0));
    if (!Dying || !Neighbour) return false;

    TestFalse(TEXT("a spawned enemy inherits the default"), Dying->DoesExplodeOnDeath());

    auto* DyingCombat = Dying->FindComponentByClass<UBreakerCombatComponent>();
    const auto* NeighbourAttr = Neighbour->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>();
    if (!DyingCombat || !NeighbourAttr) return false;
    const float NeighbourHealthBefore = NeighbourAttr->GetHealth();

    FBreakerDamageRequest Lethal;
    Lethal.BaseDamage = DyingCombat->GetMaxHealth() * 2.0f;
    Lethal.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Lethal.bCanCritical = false;
    Lethal.bCanBeAvoided = false;
    DyingCombat->ReceiveDamage(Lethal);
    if (!TestTrue(TEXT("the fixture actually killed it"), DyingCombat->IsDead())) return false;

    // Let any corpse-clock effect that wanted to fire have its chance.
    for (int32 I = 0; I < 200; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); }

    TestEqual(TEXT("the body beside it lost nothing to that death"),
        NeighbourAttr->GetHealth(), NeighbourHealthBefore);

    return true;
}

#endif
