#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Abilities/BreakerAbility_Resonance.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusCycleComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace BreakerMultispellRuntime
{
    void HitProjectile(ABreakerProjectileBase* Projectile, AActor* Target)
    {
        // The real authority impact seam used by collision. No world BeginPlay:
        // character save/load and mission startup stay out of this fixture.
        Projectile->Impact(Target, Target ? Target->GetActorLocation() : FVector(900, 0, 0));
    }

    ABreakerProjectileBase* CastFracture(ABreakerCharacter* Caster, FGameplayAbilitySpecHandle Handle)
    {
        TSet<ABreakerProjectileBase*> Existing;
        for (TActorIterator<ABreakerProjectileBase> It(Caster->GetWorld()); It; ++It) Existing.Add(*It);
        Caster->GetAttributes()->ApplyClassResource(100.0f);
        if (!Caster->GetAbilitySystemComponent()->TryActivateAbility(Handle)) return nullptr;
        for (TActorIterator<ABreakerProjectileBase> It(Caster->GetWorld()); It; ++It)
        {
            if (!Existing.Contains(*It) && !It->HasImpacted())
            {
                return *It;
            }
        }
        return nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerMultispellPurchasedRuntimeTest,
    "RiorsEdge.Abilities.MultispellPurchasedRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMultispellPurchasedRuntimeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerMultispellRuntime;
    UWorld::InitializationValues Initialization;
    Initialization.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("isolated runtime world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABreakerCharacter* Caster = World->SpawnActor<ABreakerCharacter>(ABreakerCharacter::StaticClass(),
        FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("real caster"), Caster)) return false;
    UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Caster, Caster);
    ASC->AddAttributeSetSubobject(Caster->GetAttributes());
    Caster->GetCombat()->BindAttributes(Caster->GetAttributes());
    UBreakerProgressionComponent* Progression = Caster->GetProgression();
    TestTrue(TEXT("caster class chosen"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster));
    // Provision only this transient fixture. This tests purchased effects, not
    // story entitlement or ability acquisition, which have their own coverage.
    Progression->GrantPlaytestPoints(20, 0);
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetCasterMultispellTree();
    auto Buy = [&](const TCHAR* Id)
    {
        FText Reason;
        return TestTrue(Id, Progression->PurchaseNode(Tree, FName(Id), Reason));
    };
    const FGameplayAbilitySpecHandle Fracture = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
    const FGameplayAbilitySpecHandle Resonance = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Resonance::StaticClass(), 1));
    UBreakerStatusCycleComponent* Cycle = UBreakerStatusCycleComponent::FindOrAdd(Caster);
    if (!TestNotNull(TEXT("Default cycle exists before the first cast"), Cycle)) return false;
    Cycle->BeginPlay(); // Bind real progression events without Character save loading.
    TestFalse(TEXT("Unpurchased Cycle does not preview ahead"), Cycle->CanPreviewAhead());
    // No net driver exists in this isolated world. Initialize the same lazy
    // class layout the driver would request before collecting lifetime rows;
    // otherwise uninitialized RepIndex values collide with superclass rows.
    Cycle->GetClass()->SetUpRuntimeReplicationData();
    TArray<FLifetimeProperty> Replicated;
    Cycle->GetLifetimeReplicatedProps(Replicated);
    for (const TCHAR* Name : { TEXT("AvailableStatuses"), TEXT("Cursor"), TEXT("bAdvanceOnHit"), TEXT("bPreviewAhead") })
    {
        const FProperty* Property = FindFProperty<FProperty>(UBreakerStatusCycleComponent::StaticClass(), Name);
        if (!TestNotNull(TEXT("Cycle property is reflected"), Property)) return false;
        const FLifetimeProperty* Lifetime = Replicated.FindByPredicate([Property](const FLifetimeProperty& Entry) { return Entry.RepIndex == Property->RepIndex; });
        if (!TestNotNull(TEXT("Cycle state registered for replication"), Lifetime)) return false;
        TestTrue(TEXT("Cycle state is owner-only"), Lifetime->Condition == COND_OwnerOnly);
    }

    AActor* Target = World->SpawnActor<AActor>();
    USphereComponent* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body);
    Target->SetRootComponent(Body);
    Body->SetSphereRadius(40.0f);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    Body->RegisterComponent();
    Target->SetActorLocation(FVector(500, 0, 0));
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat);
    Combat->RegisterComponent();
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Target);
    Attributes->ApplyMaxHealth(10000.0f);
    Attributes->ApplyHealth(10000.0f);
    Combat->BindAttributes(Attributes);
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status);
    Status->RegisterComponent();

    ABreakerProjectileBase* Baseline = CastFracture(Caster, Fracture);
    if (!TestNotNull(TEXT("baseline real GAS cast spawns projectile"), Baseline)) return false;
    TestEqual(TEXT("baseline advances at cast"), Cycle->GetCursor(), 1);
    TestEqual(TEXT("baseline carries one status"), Baseline->GetImpactStatuses().Num(), 1);
    HitProjectile(Baseline, nullptr);

    if (!Buy(TEXT("Caster.Multispell.Cycle"))) return false;
    TestFalse(TEXT("First Cycle rank changes timing without granting preview"), Cycle->CanPreviewAhead());
    const int32 BeforeMiss = Cycle->GetCursor();
    ABreakerProjectileBase* Miss = CastFracture(Caster, Fracture);
    if (!TestNotNull(TEXT("cycle miss cast"), Miss)) return false;
    TestEqual(TEXT("purchased Cycle keeps cursor at cast"), Cycle->GetCursor(), BeforeMiss);
    HitProjectile(Miss, nullptr);
    TestEqual(TEXT("wall impact preserves cycle"), Cycle->GetCursor(), BeforeMiss);
    ABreakerProjectileBase* Landed = CastFracture(Caster, Fracture);
    if (!TestNotNull(TEXT("cycle hit cast"), Landed)) return false;
    HitProjectile(Landed, Target);
    const int32 AfterHit = (BeforeMiss + 1) % Cycle->GetCycleLength();
    TestEqual(TEXT("live hit advances once"), Cycle->GetCursor(), AfterHit);
    HitProjectile(Landed, Target);
    TestEqual(TEXT("duplicate impact does not advance twice"), Cycle->GetCursor(), AfterHit);
    TestTrue(TEXT("projectile actually damaged target"), Attributes->GetHealth() < 10000.0f);

    if (!Buy(TEXT("Caster.Multispell.Variance")) || !Buy(TEXT("Caster.Multispell.Variance"))
        || !Buy(TEXT("Caster.Multispell.Chain"))
        || !Buy(TEXT("Caster.Multispell.Fracture"))) return false;
    Status->ConsumeAllStatuses();
    ABreakerProjectileBase* Double = CastFracture(Caster, Fracture);
    if (!TestNotNull(TEXT("two-position cast"), Double)) return false;
    const TArray<FBreakerCarriedStatus> Carried = Double->GetImpactStatuses();
    if (!TestEqual(TEXT("purchased Fracture carries two positions"), Carried.Num(), 2)) return false;
    TestTrue(TEXT("positions have distinct status identities"), Carried[0].Spec.StatusTag != Carried[1].Spec.StatusTag);
    HitProjectile(Double, Target);
    TestEqual(TEXT("both statuses reach the real target"), Status->GetDistinctStatusTypeCount(), 2);
    Caster->GetAttributes()->ApplyClassResource(100.0f);
    const float BeforeBurst = Attributes->GetHealth();
    TestTrue(TEXT("baseline Resonance activates"), ASC->TryActivateAbility(Resonance));
    TestEqual(TEXT("baseline consumes statuses"), Status->GetDistinctStatusTypeCount(), 0);
    TestTrue(TEXT("baseline burst deals damage"), Attributes->GetHealth() < BeforeBurst);

    if (!Buy(TEXT("Caster.Multispell.Payment")) || !Buy(TEXT("Caster.Multispell.Resonance"))) return false;
    for (const FBreakerCarriedStatus& Entry : Carried) Status->ApplyStatus(Entry.Spec, Entry.DamageFamily, Caster);
    if (!TestEqual(TEXT("both statuses reapplied before rewrite"), Status->GetActiveStatuses().Num(), 2)) return false;
    const float DurationBefore = Status->GetActiveStatuses()[0].RemainingDuration;
    Caster->GetAttributes()->ApplyClassResource(100.0f);
    const float BeforeRewriteBurst = Attributes->GetHealth();
    TestTrue(TEXT("purchased Resonance activates"), ASC->TryActivateAbility(Resonance));
    if (!TestEqual(TEXT("purchased Resonance preserves both types"), Status->GetDistinctStatusTypeCount(), 2)) return false;
    TestEqual(TEXT("surviving status duration is halved"), Status->GetActiveStatuses()[0].RemainingDuration, DurationBefore * 0.5f);
    TestEqual(TEXT("rewrite retains the same detonation damage"), BeforeRewriteBurst - Attributes->GetHealth(),
        BeforeBurst - BeforeRewriteBurst, 0.01f);
    if (!Buy(TEXT("Caster.Multispell.Cycle"))) return false;
    TestTrue(TEXT("Second purchased Cycle rank immediately enables next-status preview"), Cycle->CanPreviewAhead());
    const FGameplayTag Predicted = Cycle->PeekNext(1);
    Cycle->AdvanceCycle();
    TestEqual(TEXT("Preview predicts the next real cursor position"), Cycle->PeekNext(), Predicted);
    FText RespecFailure;
    TestTrue(TEXT("Actual Forge respec succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, RespecFailure));
    TestFalse(TEXT("Respec immediately removes preview without a cast"), Cycle->CanPreviewAhead());
    return true;
}
#endif
