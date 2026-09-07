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
    Advance(12);
    TestTrue(TEXT("later real Siphon tick lands damage"), Health->GetHealth() < 10000);
    TestTrue(TEXT("successful Siphon tick applies real Void"), Status->HasStatus(VoidTag));
    ASC->CancelAbilityHandle(Siphon);
    Status->ConsumeAllStatuses();
    const auto Fracture = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
    UBreakerStatusCycleComponent* Cycle = UBreakerStatusCycleComponent::FindOrAdd(Caster);
    if (!TestNotNull(TEXT("actual default cycle"), Cycle)) return false;
    TestEqual(TEXT("three distinct default positions"), Cycle->GetCycleLength(), 3);
    TSet<FGameplayTag> Delivered;
    for (int32 I = 0; I < 3; ++I)
    {
        const auto Entry = Cycle->PeekNextEntry();
        TestFalse(TEXT("each default position is distinct"), Delivered.Contains(Entry.Spec.StatusTag));
        if (Entry.Spec.StatusTag == VoidTag)
        {
            TestEqual(TEXT("Void carries no synthetic DoT"), Entry.Spec.BaseDamagePerTick, 0.0f);
            TestEqual(TEXT("Void HUD label is authored"), Entry.DisplayName.ToString(), FString(TEXT("VOID")));
        }
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
    TestEqual(TEXT("all three real status types coexist"), Status->GetDistinctStatusTypeCount(), 3);
    TestTrue(TEXT("Siphon and Fracture share Void identity"), Delivered.Contains(VoidTag));
    return true;
}
#endif
