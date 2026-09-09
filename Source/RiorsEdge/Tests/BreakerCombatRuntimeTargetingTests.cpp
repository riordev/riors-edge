#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_Rot.h"
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
    TestEqual(TEXT("shipped Cleave reach is 6.5 metres"), Sweep.RangeCm, 650.0f);
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
    // Derived from the authored reach, not a literal beside it: the fixture
    // shipped at 500 against a 450 reach and went green-to-red the moment the
    // authored number moved. The rule under test is "past the reach", so the
    // fixture states exactly that.
    Enemy->SetActorLocation(FVector(Sweep.RangeCm + 50.0f, 0, 0));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerZoneEntryAndEntropyTest,
    "RiorsEdge.Abilities.ZoneEntryAndEntropy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerZoneEntryAndEntropyTest::RunTest(const FString& Parameters)
{
    UWorld* World = BreakerTargetingWorld();
    if (!TestNotNull(TEXT("Rot world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    const UBreakerAbility_Rot* Rot = GetDefault<UBreakerAbility_Rot>();
    TestEqual(TEXT("Rot has actual ten damage Entropy hits"), Rot->ZoneDamagePerTick, 10.0f);
    TestEqual(TEXT("Rot hits every half second"), Rot->TickIntervalSeconds, 0.5f);
    FBreakerDamageSubject Target = BreakerMakeDamageSubject(World, FVector(100, 0, 0));
    ABreakerZoneActor* Zone = World->SpawnActor<ABreakerZoneActor>();
    FBreakerZoneSpec Spec;
    TestFalse(TEXT("Other zones retain delayed application unless opted in"), Spec.bApplyStatusOnEntry);
    // Generic physical poison entry behavior remains independent of the Rot ability.
    Spec.bAppliesStatus = true;
    Spec.bApplyStatusOnEntry = true;
    Spec.Duration = Rot->DurationSeconds;
    Spec.TickInterval = Rot->TickIntervalSeconds;
    Spec.StatusSpec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
    Spec.StatusSpec.BaseDamagePerTick = 2.5f;
    Spec.StatusSpec.Duration = 4.0f;
    Spec.StatusSpec.TickInterval = 0.5f;
    Zone->ConfigureZone(Spec, nullptr);
    if (!TestTrue(TEXT("Placement immediately applies poison to an occupant"), Target.Status->HasStatus(Spec.StatusSpec.StatusTag))) return false;
    TestEqual(TEXT("Application does not deal an instant extra hit"), Target.Attributes->GetHealth(), 100.0f);
    for (int32 Frame = 0; Frame < 4; ++Frame) Zone->AdvanceZone(0.1f);
    TestEqual(TEXT("Membership scans do not add stacks every frame"), Target.Status->GetActiveStatuses()[0].Stacks, 1);
    Target.Status->AdvanceStatuses(0.25f);
    TestEqual(TEXT("First damage respects the poison clock"), Target.Attributes->GetHealth(), 100.0f);
    Target.Status->AdvanceStatuses(0.25f);
    TestTrue(TEXT("Damage arrives by half a second instead of two seconds"), Target.Attributes->GetHealth() < 100.0f);
    const float AfterFirstTick = Target.Attributes->GetHealth();
    Target.Actor->SetActorLocation(FVector(2000, 0, 0));
    Zone->AdvanceZone(0.1f);
    TestEqual(TEXT("Leaving releases zone occupancy"), Zone->GetOccupantCount(), 0);
    Target.Status->AdvanceStatuses(0.5f);
    TestTrue(TEXT("Brief contact leaves the ordinary poison tail"), Target.Attributes->GetHealth() < AfterFirstTick);
    FBreakerDamageSubject EntropyTarget = BreakerMakeDamageSubject(World, FVector(100, 0, 0));
    ABreakerZoneActor* EntropyZone = World->SpawnActor<ABreakerZoneActor>();
    FBreakerZoneSpec EntropySpec;
    EntropySpec.Duration = Rot->DurationSeconds; EntropySpec.TickInterval = Rot->TickIntervalSeconds;
    EntropySpec.TickDamage.BaseDamage = Rot->ZoneDamagePerTick;
    EntropySpec.TickDamage.bCanCritical = false;
    EntropySpec.TickDamage.Element = EBreakerElement::Entropy; EntropySpec.TickDamage.ElementalFraction = 1;
    EntropySpec.TickDamage.DamageFamily = EBreakerDamageFamily::Elemental;
    EntropyZone->ConfigureZone(EntropySpec, nullptr);
    EntropyZone->AdvanceZone(.5f);
    TestEqual(TEXT("Threshold hit consumes earned buildup"), EntropyTarget.Status->GetEntropyBuildup(), 0.0f);
    TestTrue(TEXT("Actual ten damage zone hit earns Rot on the hundred-health fixture"), EntropyTarget.Status->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"))));
    TestFalse(TEXT("Entropy zone does not apply old Poison payload"), EntropyTarget.Status->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"))));
    const float BeforeRot = EntropyTarget.Attributes->GetHealth();
    EntropyTarget.Status->AdvanceStatuses(4);
    TestEqual(TEXT("Earned Rot pays half of triggering ten damage hit"), BeforeRot - EntropyTarget.Attributes->GetHealth(), 5.0f, .01f);
    return true;
}
#endif
