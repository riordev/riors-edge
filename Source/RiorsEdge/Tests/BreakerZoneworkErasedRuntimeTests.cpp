#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerZoneworkErasedRuntimeTest, "RiorsEdge.Abilities.ZoneworkErasedRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerZoneworkErasedRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Target = World->SpawnActor<AActor>();
    if (!Target) return false;
    auto* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
    Body->SetSphereRadius(20); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); Body->RegisterComponent();
    auto* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Attributes = NewObject<UBreakerAttributeSet>(Target);
    Attributes->ApplyMaxHealth(1000); Attributes->ApplyHealth(1000);
    Attributes->Armor.SetBaseValue(100); Attributes->Armor.SetCurrentValue(100);
    Combat->BindAttributes(Attributes);
    auto* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status); Status->RegisterComponent(); Status->BeginPlay();
    const FGameplayTag Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    FBreakerDamageRequest Hit;
    Hit.BaseDamage = Status->GetVoidThreshold(); Hit.bCanCritical = false;
    Hit.DamageFamily = EBreakerDamageFamily::Elemental;
    Hit.Element = EBreakerElement::Void; Hit.ElementalFraction = 1;
    const auto Result = Combat->ReceiveDamage(Hit);
    TestTrue(TEXT("real Void hit damages the target"), Result.HealthDamage > 0);
    if (!TestTrue(TEXT("accepted Void threshold creates Erased"), Status->HasStatus(Erased))) return false;
    const auto* Active = Status->GetActiveStatuses().FindByPredicate([&](const auto& Entry) { return Entry.Spec.StatusTag == Erased; });
    if (!Active) return false;
    TestEqual(TEXT("Erased is not represented as a periodic tick"), Active->Spec.BaseDamagePerTick, 0.0f);
    TestTrue(TEXT("Erased carries a real unpaid damage budget"), Active->UnpaidDamageBudget > 0);
    // An explicit zone-spec fixture isolates the existing Zonework consumer;
    // this does not claim progression purchase or new ability balance.
    auto* Zone = World->SpawnActor<ABreakerZoneActor>();
    if (!Zone) return false;
    FBreakerZoneSpec Spec;
    Spec.Duration = 10; Spec.RadiusCm = 300; Spec.HalfHeightCm = 200;
    Spec.FlatArmorReduction = 10; Spec.AfflictedArmorReduction = 20;
    Spec.TickDamage.BaseDamage = 0;
    // Fixed target armour makes both strip layers independently observable.
    const float Before = Combat->GetEffectiveArmor();
    TestEqual(TEXT("armoured target fixture starts at one hundred"), Before, 100.0f);
    Zone->ConfigureZone(Spec, nullptr); Zone->AdvanceZone(.05f);
    TestEqual(TEXT("real zone contains target"), Zone->GetOccupantCount(), 1);
    TestEqual(TEXT("Erased earns the additional afflicted armour strip"), Combat->GetEffectiveArmor(), FMath::Max(0.0f, Before - 30), .001f);
    bool bConsumed = false;
    Status->ConsumeStatus(Erased, bConsumed);
    TestTrue(TEXT("Erased is consumed through ordinary lifecycle"), bConsumed);
    Zone->AdvanceZone(.05f);
    TestEqual(TEXT("consuming Erased removes only the afflicted strip"), Combat->GetEffectiveArmor(), FMath::Max(0.0f, Before - 10), .001f);
    Target->SetActorLocation(FVector(1000, 0, 0)); Zone->AdvanceZone(.05f);
    TestEqual(TEXT("leaving the zone releases its remaining strip"), Combat->GetEffectiveArmor(), Before, .001f);
    return true;
}
#endif
