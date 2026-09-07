#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusCycleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerVoidAbilityRuntimeTest, "RiorsEdge.Abilities.VoidDeliveryRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerVoidAbilityRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated ability world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Caster = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real caster without save-loading BeginPlay"), Caster)) return false;
    UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Caster, Caster); ASC->AddAttributeSetSubobject(Caster->GetAttributes());
    Caster->GetCombat()->BindAttributes(Caster->GetAttributes());
    Caster->GetProgression()->BindAttributes(Caster->GetAttributes());
    if (!TestTrue(TEXT("actual permanent Caster selection"), Caster->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    Caster->GetMana()->BindAttributes(Caster->GetAttributes());
    Caster->GetBreakerMovement()->SetComponentTickEnabled(false);
    Caster->GetAttributes()->ApplyClassResource(100);
    AActor* Target = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("real target"), Target)) return false;
    USphereComponent* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
    Body->SetSphereRadius(40); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    Body->RegisterComponent(); Target->SetActorLocation(FVector(500, 0, 0));
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    UBreakerAttributeSet* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); Combat->BindAttributes(Health);
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status); Status->RegisterComponent();
    const FGameplayTag VoidTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Void"));
    auto Advance = [&](int32 Steps) { for (int32 I = 0; I < Steps; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.05f); } };
    // Native grants isolate delivery, not acquisition: no purchases/earned-story claim.
    const auto Siphon = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Siphon::StaticClass(), 1));
    Combat->DodgeChance = 1;
    if (!TestTrue(TEXT("real Siphon activates against collision target"), ASC->TryActivateAbility(Siphon))) return false;
    Advance(12);
    TestEqual(TEXT("dodged channel tick deals no damage"), Health->GetHealth(), 10000.0f);
    TestFalse(TEXT("dodged channel tick cannot apply Void"), Status->HasStatus(VoidTag));
    Combat->DodgeChance = 0;
    Caster->GetAttributes()->ApplyHealth(20);
    Advance(12);
    TestTrue(TEXT("later real Siphon tick lands damage"), Health->GetHealth() < 10000);
    TestTrue(TEXT("Siphon still heals from actual landed damage"), Caster->GetAttributes()->GetHealth() > 20);
    TestFalse(TEXT("successful Siphon no longer applies retired Void"), Status->HasStatus(VoidTag));
    ASC->CancelAbilityHandle(Siphon);
    Status->ConsumeAllStatuses();
    const auto Fracture = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
    UBreakerStatusCycleComponent* Cycle = UBreakerStatusCycleComponent::FindOrAdd(Caster);
    if (!TestNotNull(TEXT("actual default cycle"), Cycle)) return false;
    TestEqual(TEXT("two physical positions plus earned Entropy delivery"), Cycle->GetCycleLength(), 3);
    TSet<FGameplayTag> Delivered;
    for (int32 I = 0; I < 2; ++I)
    {
        const auto Entry = Cycle->PeekNextEntry();
        TestFalse(TEXT("each default position is distinct"), Delivered.Contains(Entry.Spec.StatusTag));
        TSet<ABreakerProjectileBase*> Existing;
        for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) Existing.Add(*It);
        Caster->GetAttributes()->ApplyClassResource(100);
        if (!TestTrue(TEXT("actual Fracture activates"), ASC->TryActivateAbility(Fracture))) return false;
        ABreakerProjectileBase* Projectile = nullptr;
        for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) if (!Existing.Contains(*It) && !It->HasImpacted()) { Projectile = *It; break; }
        if (!TestNotNull(TEXT("actual Fracture projectile"), Projectile)) return false;
        // Exercise its production impact seam immediately; no synthetic status injection.
        Projectile->Impact(Target, Target->GetActorLocation());
        TestTrue(TEXT("Fracture applies the consumed actual status"), Status->HasStatus(Entry.Spec.StatusTag));
        Delivered.Add(Entry.Spec.StatusTag);
    }
    TestEqual(TEXT("both physical status types coexist"), Status->GetDistinctStatusTypeCount(), 2);
    TestFalse(TEXT("Fracture never emits retired Void"), Delivered.Contains(VoidTag));
    const FGameplayTag RotTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    TestEqual(TEXT("third position is explicit Entropy"), Cycle->PeekNextEntry().Element, EBreakerElement::Entropy);
    bool bFirstEntropyImpact = true;
    for (int32 Shot = 0; Shot < 120 && !Status->HasStatus(RotTag); ++Shot)
    {
        // Consume real intervening physical positions as well: no cursor override.
        const auto Entry = Cycle->PeekNextEntry();
        TSet<ABreakerProjectileBase*> Existing;
        for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) Existing.Add(*It);
        Caster->GetAttributes()->ApplyClassResource(100); // Cast funding, no offensive stats.
        if (!TestTrue(TEXT("actual follow-up Fracture cast"), ASC->TryActivateAbility(Fracture))) return false;
        ABreakerProjectileBase* Projectile = nullptr;
        for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It)
            if (!Existing.Contains(*It) && !It->HasImpacted()) { Projectile = *It; break; }
        if (!TestNotNull(TEXT("follow-up projectile actually spawned"), Projectile)) return false;
        const float BuildupBefore = Status->GetEntropyBuildup();
        const float HealthBefore = Health->GetHealth();
        Projectile->Impact(Target, Target->GetActorLocation());
        if (Entry.Element == EBreakerElement::Entropy && bFirstEntropyImpact)
        {
            TestTrue(TEXT("Entropy position deals one actual impact"), Health->GetHealth() < HealthBefore);
            TestTrue(TEXT("Entropy position builds through accepted hit"), Status->GetEntropyBuildup() > BuildupBefore);
            TestFalse(TEXT("below-threshold element position cannot fabricate Rot"), Status->HasStatus(RotTag));
            bFirstEntropyImpact = false;
        }
    }
    TestFalse(TEXT("real cycle reached an Entropy position"), bFirstEntropyImpact);
    TestTrue(TEXT("repeated real projectile impacts earn Rot threshold"), Status->HasStatus(RotTag));
    TestEqual(TEXT("earned Rot joins the two physical statuses"), Status->GetDistinctStatusTypeCount(), 3);
    return true;
}
#endif
