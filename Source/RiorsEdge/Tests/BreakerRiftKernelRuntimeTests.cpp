#include "Tests/BreakerRiftRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusRules.h"
#include "Combat/BreakerRift.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void UBreakerRiftRuntimeObserver::OnDamage(const FBreakerDamageResult& Result)
{
    Results.Add(Result);
    if (!Status) return;
    const auto Unstable = FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"));
    if (const auto* Active = Status->GetActiveStatuses().FindByPredicate([Unstable](const auto& Entry)
        { return Entry.Spec.StatusTag == Unstable; })) SeenSerial = Active->ApplicationSerial;
    if (bConsumeNext)
    {
        bConsumeNext = false;
        bool bFound = false;
        const auto Consumed = Status->ConsumeStatus(Unstable, bFound);
        ConsumedBudget = bFound ? Consumed.UnpaidDamageBudget : 0;
    }
}

void UBreakerRiftRuntimeObserver::OnDeath() { ++Deaths; }

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRiftKernelRuntimeTest, "RiorsEdge.Combat.RiftKernelRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRiftKernelRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated Rift world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    AActor* Source = World->SpawnActor<AActor>();
    AActor* Target = World->SpawnActor<AActor>();
    if (!Source || !Target) return false;
    auto* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(1000); Health->ApplyHealth(1000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status); Status->RegisterComponent(); Status->SetComponentTickEnabled(false);
    auto* Observer = NewObject<UBreakerRiftRuntimeObserver>(Target);
    Observer->Status = Status;
    Combat->OnDamageReceived.AddDynamic(Observer, &UBreakerRiftRuntimeObserver::OnDamage);
    Combat->OnDeath.AddDynamic(Observer, &UBreakerRiftRuntimeObserver::OnDeath);
    const auto Unstable = FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"));
    const auto* Rule = BreakerStatusRules::FindRule(Unstable);
    if (!TestNotNull(TEXT("shipped Unstable rule"), Rule)) return false;
    TestTrue(TEXT("application damage only, no periodic or expiry payment"), Rule->bDealsDamageOnApplication
        && !Rule->bDealsPeriodicDamage && !Rule->bDealsDamageOnExpiry && !Rule->bSpreadsOnPierce);
    TestEqual(TEXT("O2 threshold follows chassis health"), Status->GetRiftThreshold(), 100.0f);
    TestEqual(TEXT("O2 marker one second"), BreakerRift::MarkerSeconds(), 1.0f);
    TestEqual(TEXT("O2 swept displacement distance"), BreakerRift::DisplacementCm(), 200.0f);
    auto Hit = [&](float Damage, float Fraction = 1.0f, float Proc = 1.0f)
    {
        FBreakerDamageRequest Request;
        Request.BaseDamage = Damage; Request.DamageFamily = EBreakerDamageFamily::Elemental;
        Request.Element = EBreakerElement::Rift; Request.ElementalFraction = Fraction;
        Request.ProcCoefficient = Proc; Request.bCanCritical = false; Request.SetInstigator(Source);
        Request.SourceLocation = FVector(-300, 40, 0); Request.bHasSourceLocation = true;
        return Combat->ReceiveDamage(Request);
    };
    auto Reset = [&]()
    {
        Status->ConsumeAllStatuses(); Status->AdvanceStatuses(20);
        Combat->RestoreVitals(); Status->RiftResistancePercent = 0;
        Observer->Results.Reset(); Observer->bConsumeNext = false; Observer->ConsumedBudget = 0;
    };
    Hit(0); Hit(10, 0); Hit(10, 1, 0);
    TestEqual(TEXT("zero damage/share/proc cannot build Rift"), Status->GetRiftBuildup(), 0.0f);
    Combat->DodgeChance = 1;
    TestTrue(TEXT("real dodge occurs"), Hit(100).bDodged);
    Combat->DodgeChance = 0;
    TestEqual(TEXT("dodged hit contributes nothing"), Status->GetRiftBuildup(), 0.0f);
    Status->GrantStatusImmunity(1); Hit(100);
    TestEqual(TEXT("status immunity blocks Rift buildup"), Status->GetRiftBuildup(), 0.0f);
    Status->AdvanceStatuses(1); Reset();
    Status->RiftResistancePercent = 50;
    TestEqual(TEXT("resistance never changes direct hit damage"), Hit(100).HealthDamage, 100.0f);
    TestEqual(TEXT("resistance halves buildup only"), Status->GetRiftBuildup(), 50.0f);
    Observer->Results.Reset();
    Hit(100);
    if (!TestTrue(TEXT("accepted threshold creates Unstable"), Status->HasStatus(Unstable))) return false;
    if (!TestEqual(TEXT("outer hit and earned activation each dispatch once"), Observer->Results.Num(), 2)) return false;
    TestEqual(TEXT("outer hit callback sees pre-activation health"), Observer->Results[0].RemainingHealth, 800.0f);
    TestEqual(TEXT("activation callback follows outer callback"), Observer->Results[1].RemainingHealth, 750.0f);
    TestEqual(TEXT("only triggering hit earns fifty damage"), Observer->Results[1].HealthDamage, 50.0f);
    TestEqual(TEXT("paid marker contains no recyclable damage"), Status->GetActiveStatuses()[0].UnpaidDamageBudget, 0.0f);
    TestEqual(TEXT("actual impact source location survives snapshot"), Status->GetActiveStatuses()[0].SourceLocationSnapshot, FVector(-300, 40, 0));
    const uint64 FirstSerial = Observer->SeenSerial;
    const float BeforeReplay = Health->GetHealth();
    Status->FlushRiftActivation(FirstSerial);
    TestEqual(TEXT("replaying exact activation token cannot pay again"), Health->GetHealth(), BeforeReplay);
    Status->AdvanceStatuses(.5f);
    Observer->Results.Reset(); Hit(200);
    TestEqual(TEXT("active marker permits only direct hit, no repeated impulse/burst"), Observer->Results.Num(), 1);
    TestEqual(TEXT("active marker deadline never refreshes"), Status->GetActiveStatuses()[0].RemainingDuration, .5f);
    const float BeforeExpiry = Health->GetHealth();
    Status->AdvanceStatuses(.5f);
    TestFalse(TEXT("marker expires"), Status->HasStatus(Unstable));
    TestEqual(TEXT("expiry has no second budget"), Health->GetHealth(), BeforeExpiry);
    Reset(); Hit(100);
    TestTrue(TEXT("new threshold gets a distinct application token"), Observer->SeenSerial != FirstSerial);

    Reset(); Observer->bConsumeNext = true;
    Hit(100);
    TestEqual(TEXT("actual outer damage callback consumes pending unpaid budget"), Observer->ConsumedBudget, 50.0f);
    TestEqual(TEXT("consumption prevents child damage callback"), Observer->Results.Num(), 1);
    TestEqual(TEXT("only original hit damages after callback consumption"), Health->GetHealth(), 900.0f);
    TestFalse(TEXT("consumed pending marker is gone"), Status->HasStatus(Unstable));
    Status->FlushRiftActivation(Observer->SeenSerial);
    TestEqual(TEXT("stale token cannot resurrect consumed damage"), Health->GetHealth(), 900.0f);
    Reset(); Health->ApplyHealth(120); Observer->Deaths = 0;
    const auto Outer = Hit(100);
    TestFalse(TEXT("triggering hit itself was not lethal"), Outer.bKilled);
    TestTrue(TEXT("earned child burst actually kills"), Combat->IsDead());
    if (!TestEqual(TEXT("lethal activation still dispatches two ordered results"), Observer->Results.Num(), 2)) return false;
    TestEqual(TEXT("outer callback precedes lethal activation"), Observer->Results[0].RemainingHealth, 20.0f);
    TestTrue(TEXT("second callback carries actual kill"), Observer->Results[1].bKilled);
    TestEqual(TEXT("actual death broadcast is singular"), Observer->Deaths, 1);
    TestFalse(TEXT("death clears Unstable"), Status->HasStatus(Unstable));
    TestEqual(TEXT("death clears pending buildup"), Status->GetRiftBuildup(), 0.0f);

    Reset();
    FBreakerDamageRequest Critical;
    Critical.BaseDamage = 25; Critical.SourceDamageMultiplier = 4;
    Critical.CriticalChance = 1; Critical.CriticalMultiplier = 2;
    Critical.Element = EBreakerElement::Rift; Critical.ElementalFraction = .5f; Critical.SetInstigator(Source);
    const auto CriticalResult = Combat->ReceiveDamage(Critical);
    TestTrue(TEXT("real applying hit crits"), CriticalResult.bCritical);
    if (!TestEqual(TEXT("critical source still gives one activation"), Observer->Results.Num(), 2)) return false;
    TestEqual(TEXT("applying power and critical folded only once into half-share budget"), Observer->Results[1].HealthDamage, 50.0f);
    TestFalse(TEXT("activation cannot reroll a critical"), Observer->Results[1].bCritical);
    Reset(); Combat->GrantStaggerImmunity(10); Hit(100);
    TestTrue(TEXT("existing movement-adjacent immunity is active"), Combat->IsStaggerImmune());
    TestEqual(TEXT("displacement immunity does not block earned damage"), Health->GetHealth(), 850.0f);
    Reset();
    Status->AilmentAvoidanceChance = .75f;
    bool bAvoided = false;
    for (int32 Attempt = 0; Attempt < 32 && !bAvoided; ++Attempt)
    {
        Reset(); Hit(100);
        bAvoided = !Status->HasStatus(Unstable);
    }
    TestTrue(TEXT("ordinary avoidance can refuse threshold application"), bAvoided);
    TestEqual(TEXT("refused application cannot pay a child hit"), Observer->Results.Num(), 1);
    Status->AilmentAvoidanceChance = 0;
    Reset();
    FBreakerStatusApplicationSpec Fabricated;
    Fabricated.StatusTag = Unstable; Fabricated.Duration = 100; Fabricated.TickInterval = .1f;
    Fabricated.BaseDamagePerTick = 1000;
    Status->ApplyStatus(Fabricated, EBreakerDamageFamily::Elemental, Source);
    TestFalse(TEXT("carried status cannot fabricate Rift damage"), Status->HasStatus(Unstable));
    Hit(50);
    TestEqual(TEXT("partial buildup exists"), Status->GetRiftBuildup(), 50.0f);
    Status->AdvanceStatuses(4);
    TestEqual(TEXT("partial buildup expires"), Status->GetRiftBuildup(), 0.0f);
    Hit(50); Hit(10000);
    TestEqual(TEXT("lethal hit clears partial buildup without activation"), Status->GetRiftBuildup(), 0.0f);

    // Bridge the accepted-hit kernel to real native enemy locomotion. The
    // separate geometry fixture covers wall and ledge truncation in detail.
    auto* Floor = World->SpawnActor<AActor>();
    if (!Floor) return false;
    auto* FloorShape = NewObject<UBoxComponent>(Floor);
    Floor->AddInstanceComponent(FloorShape); Floor->SetRootComponent(FloorShape);
    FloorShape->SetBoxExtent(FVector(1000, 500, 25));
    FloorShape->SetCollisionObjectType(ECC_WorldStatic);
    FloorShape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    FloorShape->SetCollisionResponseToAllChannels(ECR_Block); FloorShape->RegisterComponent();
    Floor->SetActorLocation(FVector(0, 2000, -25));
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(0, 2000, 200), FRotator::ZeroRotator, Spawn);
    if (!Enemy) return false;
    auto* Capsule = Cast<UCapsuleComponent>(Enemy->GetRootComponent());
    auto* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
    auto* EnemyStatus = Enemy->FindComponentByClass<UBreakerStatusComponent>();
    auto* EnemyHealth = FindObject<UBreakerAttributeSet>(Enemy, TEXT("Attributes"));
    if (!Capsule || !EnemyCombat || !EnemyStatus || !EnemyHealth) return false;
    auto* EnemyASC = Enemy->GetAbilitySystemComponent();
    EnemyASC->InitAbilityActorInfo(Enemy, Enemy); EnemyASC->AddAttributeSetSubobject(EnemyHealth);
    EnemyHealth->ApplyMaxHealth(1000); EnemyHealth->ApplyHealth(1000); EnemyCombat->BindAttributes(EnemyHealth);
    const FVector Start(0, 2000, Capsule->GetScaledCapsuleHalfHeight() + 2);
    Enemy->SetActorLocation(Start);
    FBreakerDamageRequest Shove;
    Shove.BaseDamage = 100; Shove.bCanCritical = false;
    Shove.Element = EBreakerElement::Rift; Shove.ElementalFraction = 1;
    Shove.SetInstigator(Source); Shove.bHasSourceLocation = true;
    Shove.SourceLocation = Start - FVector(500, 0, 0);
    EnemyCombat->ReceiveDamage(Shove);
    TestEqual(TEXT("actual accepted threshold displaces native enemy on floor"), Enemy->GetActorLocation().X - Start.X, 200.0, .01);
    TestEqual(TEXT("native displacement also pays finite earned damage"), EnemyHealth->GetHealth(), 850.0f, .01f);
    EnemyStatus->ConsumeAllStatuses();
    EnemyCombat->GrantStaggerImmunity(10);
    const FVector ImmuneStart = Enemy->GetActorLocation();
    EnemyCombat->ReceiveDamage(Shove);
    TestEqual(TEXT("actual immunity suppresses native movement"), Enemy->GetActorLocation(), ImmuneStart);
    TestEqual(TEXT("immune native enemy still takes direct plus earned damage"), EnemyHealth->GetHealth(), 700.0f, .01f);
    return true;
}
#endif
