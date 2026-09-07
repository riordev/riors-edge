#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEntropy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEnemyEntropyRuntimeTest, "RiorsEdge.Combat.EnemyEntropyRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerEnemyEntropyRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated enemy combat world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!Player) return false;
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Player->SetActorTickEnabled(false);
    auto* PlayerStatus = Player->FindComponentByClass<UBreakerStatusComponent>();
    if (!TestNotNull(TEXT("player constructor provides real status receiver"), PlayerStatus)) return false;
    PlayerStatus->SetComponentTickEnabled(false);
    auto SpawnEnemy = [&](EBreakerEnemyFamily Family)
    {
        auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(100, 0, 100), FRotator::ZeroRotator);
        if (!Enemy) return Enemy;
        auto* Property = FindFProperty<FEnumProperty>(ABreakerEnemy::StaticClass(), TEXT("Family"));
        if (!Property) return static_cast<ABreakerEnemy*>(nullptr);
        Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Enemy), static_cast<uint64>(Family));
        Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
        if (auto* Movement = Enemy->GetMovementComponent()) Movement->SetComponentTickEnabled(false);
        return Enemy;
    };
    auto* Vestige = SpawnEnemy(EBreakerEnemyFamily::Vestige);
    if (!TestNotNull(TEXT("real melee Vestige"), Vestige)) return false;
    auto* VestigeStatus = Vestige->FindComponentByClass<UBreakerStatusComponent>();
    TestEqual(TEXT("family resistance comes from tuning without equipment"), VestigeStatus->GetEntropyResistancePercent(), BreakerEntropy::VestigeResistancePercent());
    auto Advance = [&](float Seconds)
    {
        for (float Time = 0; Time < Seconds; Time += .05f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
    };
    const float Before = Player->GetAttributes()->GetHealth();
    Vestige->Tick(.01f);
    const float Damage = Before - Player->GetAttributes()->GetHealth();
    TestEqual(TEXT("family conversion preserves base melee damage"), Damage, Vestige->GetEffectiveAttackDamage(), .01f);
    const float EarnedEntropy = Damage * BreakerEntropy::VestigeMeleeFraction();
    if (EarnedEntropy >= PlayerStatus->GetEntropyThreshold())
    {
        const auto* Rot = PlayerStatus->GetActiveStatuses().FindByPredicate([](const FBreakerActiveStatus& Entry)
            { return Entry.Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")); });
        if (!TestNotNull(TEXT("threshold-crossing real melee earns Rot immediately"), Rot)) return false;
        TestEqual(TEXT("threshold consumes rather than retains buildup"), PlayerStatus->GetEntropyBuildup(), 0.0f);
        TestEqual(TEXT("real melee snapshots half of earned Entropy damage"),
            Rot->Spec.BaseDamagePerTick * FMath::FloorToInt(Rot->Spec.Duration / Rot->Spec.TickInterval), EarnedEntropy * .5f, .01f);
    }
    else
        TestEqual(TEXT("subthreshold melee retains its authored Entropy share"), PlayerStatus->GetEntropyBuildup(), EarnedEntropy, .01f);
    const float Buildup = PlayerStatus->GetEntropyBuildup();
    Player->GetCombat()->DodgeChance = 1;
    Advance(Vestige->GetAttackCooldown() + .1f); Vestige->Tick(.01f);
    TestEqual(TEXT("real dodged melee causes no health loss"), Player->GetAttributes()->GetHealth(), Before - Damage, .01f);
    TestEqual(TEXT("real dodged melee causes no buildup"), PlayerStatus->GetEntropyBuildup(), Buildup, .01f);
    Player->GetCombat()->DodgeChance = 0;
    for (int32 Hit = 0; Hit < 4 && !PlayerStatus->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"))); ++Hit)
    { Advance(Vestige->GetAttackCooldown() + .1f); Vestige->Tick(.01f); }
    TestTrue(TEXT("actual repeated Vestige attacks earn player Rot"), PlayerStatus->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"))));
    PlayerStatus->ConsumeAllStatuses();
    Vestige->SetActorLocation(FVector(10000, 0, 100));
    auto* Altered = SpawnEnemy(EBreakerEnemyFamily::Altered);
    if (!Altered) return false;
    PlayerStatus->AdvanceStatuses(5);
    Player->GetAttributes()->ApplyHealth(Player->GetAttributes()->GetMaxHealth());
    const float BeforeAltered = Player->GetAttributes()->GetHealth(); Altered->Tick(.01f);
    TestTrue(TEXT("nonfamily base melee still actually hits"), Player->GetAttributes()->GetHealth() < BeforeAltered);
    TestEqual(TEXT("nonfamily melee does not acquire Entropy"), PlayerStatus->GetEntropyBuildup(), 0.0f);
    TestEqual(TEXT("nonfamily has no Vestige resistance"), Altered->FindComponentByClass<UBreakerStatusComponent>()->GetEntropyResistancePercent(), 0.0f);
    FBreakerDamageRequest Probe; Probe.BaseDamage = 1; Probe.bCanCritical = false;
    Probe.Element = EBreakerElement::Entropy; Probe.ElementalFraction = 1; Probe.DamageFamily = EBreakerDamageFamily::TrueDamage;
    const auto Result = Vestige->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Probe);
    TestEqual(TEXT("family resistance does not reduce damage"), Result.HealthDamage, 1.0f);
    TestEqual(TEXT("family resistance reduces actual buildup"), VestigeStatus->GetEntropyBuildup(), 1.0f - BreakerEntropy::VestigeResistancePercent() / 100.0f, .001f);
    return true;
}
#endif