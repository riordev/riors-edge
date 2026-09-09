#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEntropyRuntimeTest, "RiorsEdge.Combat.EntropyRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerEntropyRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated Entropy world"), World)) return false;
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
    const FGameplayTag Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    auto Hit = [&](float Damage, float Fraction = 1.0f, float Proc = 1.0f)
    {
        FBreakerDamageRequest Request;
        Request.BaseDamage = Damage; Request.Element = EBreakerElement::Entropy;
        Request.ElementalFraction = Fraction; Request.ProcCoefficient = Proc;
        Request.bCanCritical = false; Request.SetInstigator(Source);
        return Combat->ReceiveDamage(Request);
    };
    TestEqual(TEXT("threshold follows actual chassis health"), Status->GetEntropyThreshold(), 100.0f);
    Health->ApplyMaxHealth(2000);
    TestEqual(TEXT("larger chassis requires proportionally more buildup"), Status->GetEntropyThreshold(), 200.0f);
    Health->ApplyMaxHealth(1000);
    Hit(0); Hit(10, 0); Hit(10, 1, 0);
    TestEqual(TEXT("zero damage, fraction and proc cannot accrue"), Status->GetEntropyBuildup(), 0.0f);
    Combat->DodgeChance = 1;
    TestTrue(TEXT("real dodge occurs"), Hit(20).bDodged);
    TestEqual(TEXT("dodged hit contributes nothing"), Status->GetEntropyBuildup(), 0.0f);
    Combat->DodgeChance = 0;
    Status->EntropyResistancePercent = 50;
    const auto Resisted = Hit(100);
    TestEqual(TEXT("resistance never reduces immediate damage"), Resisted.HealthDamage, 100.0f);
    TestEqual(TEXT("resistance reduces only buildup"), Status->GetEntropyBuildup(), 50.0f);
    Hit(100);
    TestTrue(TEXT("actual second accepted hit reaches Rot threshold"), Status->HasStatus(Rot));
    if (!TestEqual(TEXT("one threshold creates one status"), Status->GetActiveStatuses().Num(), 1)) return false;
    const auto Snapshot = Status->GetActiveStatuses()[0];
    TestEqual(TEXT("Rot snapshots four seconds"), Snapshot.RemainingDuration, 4.0f);
    TestEqual(TEXT("Rot snapshots half-second cadence"), Snapshot.Spec.TickInterval, .5f);
    TestEqual(TEXT("eight ticks spend half of triggering raw hit"), Snapshot.Spec.BaseDamagePerTick * 8, 50.0f);
    FBreakerStatusApplicationSpec Unearned = Snapshot.Spec;
    Unearned.InitialStacks = 8; Unearned.BaseDamagePerTick = 1000; Unearned.Duration = 100;
    Status->ApplyStatus(Unearned, EBreakerDamageFamily::Elemental, Source);
    TestEqual(TEXT("direct status application cannot inflate earned Rot"), Status->GetActiveStatuses()[0].Stacks, 1);
    TestEqual(TEXT("direct status application cannot refresh earned lifetime"), Status->GetActiveStatuses()[0].RemainingDuration, 4.0f);
    Hit(200);
    TestEqual(TEXT("active Rot does not multiply applications"), Status->GetActiveStatuses().Num(), 1);
    TestEqual(TEXT("later stronger hit cannot replace earned snapshot"), Status->GetActiveStatuses()[0].Spec.BaseDamagePerTick, Snapshot.Spec.BaseDamagePerTick);
    const float BeforeTicks = Health->GetHealth();
    Status->AdvanceStatuses(4.0f);
    TestEqual(TEXT("full lifetime pays earned budget exactly once"), BeforeTicks - Health->GetHealth(), 50.0f, .01f);
    TestFalse(TEXT("Rot expires after actual lifetime"), Status->HasStatus(Rot));
    TestEqual(TEXT("Rot ticks cannot build another Rot"), Status->GetEntropyBuildup(), 0.0f);
    Status->GrantStatusImmunity(1.0f);
    Hit(200);
    TestFalse(TEXT("status immunity refuses threshold effect"), Status->HasStatus(Rot));
    Status->AdvanceStatuses(1.0f);
    Hit(100000);
    TestTrue(TEXT("actual lethal damage kills target"), Combat->IsDead());
    TestFalse(TEXT("lethal hit does not apply Rot to corpse"), Status->HasStatus(Rot));
    Hit(200);
    TestFalse(TEXT("corpse cannot earn another status"), Status->HasStatus(Rot));
    // Ordinary level-one cast against a 100-health body. No damage/stat grants.
    auto* Caster = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("actual Rot caster"), Caster)) return false;
    Caster->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Caster->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Caster, Caster); ASC->AddAttributeSetSubobject(Caster->GetAttributes());
    Caster->GetCombat()->BindAttributes(Caster->GetAttributes());
    Caster->GetProgression()->BindAttributes(Caster->GetAttributes());
    if (!TestTrue(TEXT("actual Caster selection"), Caster->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    Caster->GetAttributes()->ApplyClassResource(100); // Cast funding only; no offensive stats.
    auto* Victim = World->SpawnActor<AActor>();
    if (!Victim) return false;
    auto* Body = NewObject<UBoxComponent>(Victim);
    Victim->AddInstanceComponent(Body); Victim->SetRootComponent(Body);
    Body->SetBoxExtent(FVector(40)); Body->SetCollisionObjectType(ECC_Pawn);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Body->SetCollisionResponseToAllChannels(ECR_Block);
    Body->RegisterComponent(); Victim->SetActorLocation(FVector(500, 0, 100));
    auto* VictimCombat = NewObject<UBreakerCombatComponent>(Victim);
    Victim->AddInstanceComponent(VictimCombat); VictimCombat->RegisterComponent();
    auto* VictimHealth = NewObject<UBreakerAttributeSet>(Victim);
    VictimHealth->ApplyMaxHealth(100); VictimHealth->ApplyHealth(100); VictimCombat->BindAttributes(VictimHealth);
    auto* VictimStatus = NewObject<UBreakerStatusComponent>(Victim);
    Victim->AddInstanceComponent(VictimStatus); VictimStatus->RegisterComponent();
    const auto Cast = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Rot::StaticClass(), 1));
    if (!TestTrue(TEXT("actual Rot GAS activation"), ASC->TryActivateAbility(Cast))) return false;
    BreakerResolvePendingCast(World, Caster);
    ABreakerZoneActor* Zone = nullptr;
    for (TActorIterator<ABreakerZoneActor> It(World); It; ++It)
        if (It->GetZoneInstigator() == Caster) { Zone = *It; break; }
    if (!TestNotNull(TEXT("real cast creates zone"), Zone)) return false;
    for (int32 Tick = 0; Tick < 4 && !VictimStatus->HasStatus(Rot); ++Tick) Zone->AdvanceZone(.5f);
    TestTrue(TEXT("ordinary cast damages actual overlapping target"), VictimHealth->GetHealth() < 100);
    TestTrue(TEXT("ordinary Rot zone earns threshold status through damage"), VictimStatus->HasStatus(Rot));
    return true;
}
#endif
