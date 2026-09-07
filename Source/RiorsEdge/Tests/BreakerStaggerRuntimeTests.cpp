#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerStaggerRuntimeTest, "RiorsEdge.Combat.StaggerRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerStaggerRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated stagger world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    AActor* Victim = World->SpawnActor<AActor>();
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Victim);
    Victim->AddInstanceComponent(Combat); Combat->RegisterComponent();
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Victim);
    Attributes->ApplyMaxHealth(100); Attributes->ApplyHealth(100); Combat->BindAttributes(Attributes);
    auto Advance = [&](int32 Steps)
    {
        for (int32 I = 0; I < Steps; ++I)
        { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f); }
    };
    Advance(1); // Activate actual world timer manager before timing assertions.
    TestFalse(TEXT("zero stagger refused"), Combat->ApplyStagger(0));
    TestFalse(TEXT("nonfinite stagger refused"), Combat->ApplyStagger(std::numeric_limits<float>::infinity()));
    Combat->StaggerResistance = .5f;
    TestTrue(TEXT("real stagger accepted"), Combat->ApplyStagger(1));
    TestEqual(TEXT("resistance scales duration"), Combat->GetStaggerRemaining(), .5f, .001f);
    TestTrue(TEXT("shorter stagger accepted without shortening existing one"), Combat->ApplyStagger(.2f));
    TestEqual(TEXT("strongest remaining expiry wins"), Combat->GetStaggerRemaining(), .5f, .001f);
    Advance(12);
    TestFalse(TEXT("stagger expires through actual world clock"), Combat->IsStaggered());
    Combat->GrantStaggerImmunity(.5f);
    TestTrue(TEXT("immunity is live"), Combat->IsStaggerImmune());
    TestFalse(TEXT("immunity refuses stagger"), Combat->ApplyStagger(2));
    Advance(12);
    TestFalse(TEXT("immunity expires"), Combat->IsStaggerImmune());
    TestTrue(TEXT("stagger can apply again after immunity"), Combat->ApplyStagger(1));
    FBreakerDamageRequest Death; Death.BaseDamage = 1000; Death.bCanCritical = false;
    Combat->ReceiveDamage(Death);
    TestTrue(TEXT("real lethal damage"), Combat->IsDead());
    TestFalse(TEXT("death clears stagger"), Combat->IsStaggered());
    TestFalse(TEXT("dead body refuses stagger"), Combat->ApplyStagger(1));
    Combat->RestoreVitals();
    TestFalse(TEXT("revival does not restore old stagger"), Combat->IsStaggered());

    ABreakerRangedEnemy* Ranged = World->SpawnActor<ABreakerRangedEnemy>(FVector(2000, 0, 0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("real ranged enemy"), Ranged)) return false;
    auto* EnemyAttributes = Cast<UBreakerAttributeSet>(Ranged->GetDefaultSubobjectByName(TEXT("Attributes")));
    if (!TestNotNull(TEXT("real enemy attributes"), EnemyAttributes)) return false;
    Ranged->GetAbilitySystemComponent()->AddAttributeSetSubobject(EnemyAttributes);
    Ranged->DispatchBeginPlay(); Ranged->SetActorTickEnabled(false);
    auto* EnemyCombat = Ranged->FindComponentByClass<UBreakerCombatComponent>();
    if (!TestNotNull(TEXT("real enemy combat"), EnemyCombat)) return false;
    TestTrue(TEXT("real order begins ranged windup"), Ranged->CommandVolley());
    TestTrue(TEXT("windup is actually active"), Ranged->IsWindingUp());
    TestTrue(TEXT("enemy accepts real stagger"), EnemyCombat->ApplyStagger(1));
    TestFalse(TEXT("accepted stagger cancels committed ranged windup"), Ranged->IsWindingUp());
    TestFalse(TEXT("orders cannot rearm while staggered"), Ranged->CommandVolley());
    Advance(22);
    TestTrue(TEXT("fresh order can start after stagger expires"), Ranged->CommandVolley());
    Ranged->bStaggerImmune = true;
    TestFalse(TEXT("authored immune enemy refuses stagger"), EnemyCombat->ApplyStagger(1));
    TestTrue(TEXT("immune refusal preserves its windup"), Ranged->IsWindingUp());
    auto* Warden = World->SpawnActor<ABreakerWardenEnemy>(FVector(4000, 0, 0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("actual Warden"), Warden)) return false;
    TestEqual(TEXT("Warden authors high duration resistance"), Warden->FindComponentByClass<UBreakerCombatComponent>()->StaggerResistance, .5f);

    FBreakerDamageRequest Hit; Hit.BaseDamage = 10; Hit.bCanCritical = false;
    FBreakerDefenseState Defense; Defense.Health = 100; Defense.DodgeChance = 1; Defense.BlockChance = 1;
    TestTrue(TEXT("ordinary damage preserves passive avoidance"), UBreakerDamageLibrary::ResolveDamage(Hit, Defense).bDodged);
    Hit.bCanBeAvoided = false;
    const auto Unavoidable = UBreakerDamageLibrary::ResolveDamage(Hit, Defense);
    TestFalse(TEXT("explicit unavoidable hit cannot dodge"), Unavoidable.bDodged);
    TestFalse(TEXT("explicit unavoidable hit cannot block"), Unavoidable.bBlocked);
    TestEqual(TEXT("unavoidable hit still delivers ordinary damage"), Unavoidable.HealthDamage, 10.0f);
    return true;
}
#endif
