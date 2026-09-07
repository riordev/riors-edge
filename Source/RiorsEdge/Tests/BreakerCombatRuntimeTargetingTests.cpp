#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerMeleeSweep.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    UWorld* BreakerTargetingWorld()
    {
        UWorld::InitializationValues Initialization;
        Initialization.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::Num, &Initialization);
    }

    struct FBreakerDamageSubject
    {
        AActor* Actor;
        UBreakerAttributeSet* Attributes;
        UBreakerStatusComponent* Status;
    };

    FBreakerDamageSubject BreakerMakeDamageSubject(UWorld* World, const FVector& Location)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        USphereComponent* Body = NewObject<USphereComponent>(Actor);
        Actor->AddInstanceComponent(Body);
        Actor->SetRootComponent(Body);
        Body->SetSphereRadius(1.0f);
        Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Body->SetCollisionResponseToAllChannels(ECR_Ignore);
        Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
        Body->RegisterComponent();
        Actor->SetActorLocation(Location);
        UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Actor);
        Actor->AddInstanceComponent(Combat);
        Combat->RegisterComponent();
        UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Actor);
        Combat->BindAttributes(Attributes);
        UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Actor);
        Actor->AddInstanceComponent(Status);
        Status->RegisterComponent();
        return { Actor, Attributes, Status };
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCleaveWorldOcclusionTest,
    "RiorsEdge.Abilities.CleaveWorldOcclusion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCleaveWorldOcclusionTest::RunTest(const FString& Parameters)
{
    UWorld* World = BreakerTargetingWorld();
    if (!TestNotNull(TEXT("targeting world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), FVector(400, 0, 0), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("shipped enemy collision"), Enemy)) return false;
    FBreakerMeleeSweepParams Sweep;
    const UBreakerAbility_Cleave* Cleave = GetDefault<UBreakerAbility_Cleave>();
    Sweep.RangeCm = Cleave->ComputeEffectiveRangeCm(nullptr);
    Sweep.ArcDegrees = Cleave->ComputeEffectiveArcDegrees(nullptr);
    Sweep.Forward = FVector::ForwardVector;
    TestEqual(TEXT("shipped Cleave reach is 4.5 metres"), Sweep.RangeCm, 450.0f);
    TestTrue(TEXT("open enemy does not occlude itself"), UBreakerMeleeSweep::SweepTargets(World, nullptr, Sweep).Contains(Enemy));
    AActor* Wall = World->SpawnActor<AActor>();
    UBoxComponent* WallBody = NewObject<UBoxComponent>(Wall);
    Wall->AddInstanceComponent(WallBody);
    Wall->SetRootComponent(WallBody);
    WallBody->SetBoxExtent(FVector(10, 150, 150));
    WallBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WallBody->SetCollisionObjectType(ECC_WorldStatic);
    WallBody->SetCollisionResponseToAllChannels(ECR_Block);
    WallBody->RegisterComponent();
    Wall->SetActorLocation(FVector(200, 0, 0));
    TestFalse(TEXT("wall still blocks the swing"), UBreakerMeleeSweep::SweepTargets(World, nullptr, Sweep).Contains(Enemy));
    Wall->Destroy();
    Enemy->SetActorLocation(FVector(500, 0, 0));
    TestFalse(TEXT("enemy beyond authored reach is refused"), UBreakerMeleeSweep::SweepTargets(World, nullptr, Sweep).Contains(Enemy));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerStatusLifetimeCatchupTest,
    "RiorsEdge.Combat.StatusLifetimeCatchup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStatusLifetimeCatchupTest::RunTest(const FString& Parameters)
{
    UWorld* World = BreakerTargetingWorld();
    if (!TestNotNull(TEXT("status world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    FBreakerDamageSubject Sliced = BreakerMakeDamageSubject(World, FVector::ZeroVector);
    FBreakerDamageSubject Hitch = BreakerMakeDamageSubject(World, FVector(1000, 0, 0));
    FBreakerStatusApplicationSpec Spec;
    Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
    Spec.BaseDamagePerTick = 1.0f;
    Spec.Duration = 4.0f;
    Spec.TickInterval = 0.25f;
    Sliced.Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, nullptr);
    Hitch.Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, nullptr);
    for (int32 Frame = 0; Frame < 64; ++Frame) Sliced.Status->AdvanceStatuses(1.0f / 16.0f);
    Hitch.Status->AdvanceStatuses(8.0f);
    TestTrue(TEXT("status damage actually reaches health"), Hitch.Attributes->GetHealth() < 100.0f);
    TestEqual(TEXT("all sixteen ticks survive a hitch, no post-expiry ticks"), Hitch.Attributes->GetHealth(), Sliced.Attributes->GetHealth(), 0.0001f);
    TestEqual(TEXT("status expires after caught-up ticks"), Hitch.Status->GetActiveStatuses().Num(), 0);
    const float AfterExpiry = Hitch.Attributes->GetHealth();
    Hitch.Status->AdvanceStatuses(1.0f);
    TestEqual(TEXT("expired status cannot deal more damage"), Hitch.Attributes->GetHealth(), AfterExpiry);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerZoneLifetimeAndBoundsTest,
    "RiorsEdge.Combat.Zone.RuntimeLifetimeAndBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerZoneLifetimeAndBoundsTest::RunTest(const FString& Parameters)
{
    UWorld* World = BreakerTargetingWorld();
    if (!TestNotNull(TEXT("zone world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    FBreakerDamageSubject Edge = BreakerMakeDamageSubject(World, FVector(399, 0, 249));
    ABreakerZoneActor* Zone = World->SpawnActor<ABreakerZoneActor>();
    FBreakerZoneSpec Spec;
    Spec.RadiusCm = 400.0f;
    Spec.HalfHeightCm = 250.0f;
    Spec.Duration = 1.25f;
    Spec.TickInterval = 1.0f;
    Spec.TickDamage.BaseDamage = 3.0f;
    Spec.TickDamage.bCanCritical = false;
    Zone->ConfigureZone(Spec, nullptr);
    Zone->AdvanceZone(0.25f);
    TestEqual(TEXT("cylinder upper rim is discovered by broad phase"), Zone->GetOccupantCount(), 1);
    const float Before = Edge.Attributes->GetHealth();
    Zone->AdvanceZone(5.0f);
    TestEqual(TEXT("long frame delivers only ticks within lifetime"), Zone->GetTicksDelivered(), 1);
    TestTrue(TEXT("zone tick damages its occupant"), Edge.Attributes->GetHealth() < Before);
    return true;
}

#endif
