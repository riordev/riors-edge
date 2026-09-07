#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSiphonTargetDeathTest, "RiorsEdge.Abilities.SiphonTargetDeath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSiphonTargetDeathTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated channel world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Caster = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real Caster"), Caster)) return false;
    UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Caster, Caster);
    ASC->AddAttributeSetSubobject(Caster->GetAttributes());
    Caster->GetCombat()->BindAttributes(Caster->GetAttributes());
    Caster->GetProgression()->BindAttributes(Caster->GetAttributes());
    if (!TestTrue(TEXT("actual permanent class selection"), Caster->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    Caster->GetMana()->BindAttributes(Caster->GetAttributes());
    Caster->GetBreakerMovement()->SetComponentTickEnabled(false);
    Caster->GetAttributes()->ApplyClassResource(100);
    // Native grant isolates channel lifetime, not progression acquisition.
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Siphon::StaticClass(), 1));
    auto Active = [&]() { const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle); return Spec && Spec->IsActive(); };
    auto Advance = [&](int32 Steps) { for (int32 I = 0; I < Steps; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); } };
    auto SpawnTarget = [&](float HitPoints)
    {
        AActor* Target = World->SpawnActor<AActor>();
        USphereComponent* Body = NewObject<USphereComponent>(Target);
        Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
        Body->SetSphereRadius(40); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Body->SetCollisionResponseToAllChannels(ECR_Ignore);
        Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
        Body->RegisterComponent(); Target->SetActorLocation(FVector(500, 0, 0));
        UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
        Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
        UBreakerAttributeSet* Health = NewObject<UBreakerAttributeSet>(Target);
        Health->ApplyMaxHealth(HitPoints); Health->ApplyHealth(HitPoints); Combat->BindAttributes(Health);
        return Combat;
    };
    UBreakerCombatComponent* Target = SpawnTarget(10000);
    if (!TestTrue(TEXT("channel activates against living target"), ASC->TryActivateAbility(Handle))) return false;
    UBreakerAbilityStateComponent* State = Caster->FindComponentByClass<UBreakerAbilityStateComponent>();
    if (!TestNotNull(TEXT("channel publishes HUD window"), State)) return false;
    TestTrue(TEXT("channel is active before death"), Active());
    Advance(3); // Before the first authored damage tick.
    FBreakerDamageRequest Kill;
    Kill.BaseDamage = 100000; Kill.bCanCritical = false;
    Target->ReceiveDamage(Kill);
    TestTrue(TEXT("target is dead but its corpse actor persists"), Target->IsDead() && IsValid(Target->GetOwner()));
    TestFalse(TEXT("external kill immediately ends GAS channel without another tick"), Active());
    TestFalse(TEXT("external kill immediately clears HUD channel window"), State->IsWindowActive(UBreakerAbility_Siphon::ChannelWindowKey()));
    const float ManaBeforeCorpse = Caster->GetAttributes()->GetClassResource();
    ASC->TryActivateAbility(Handle);
    TestFalse(TEXT("corpse cannot start another channel"), Active());
    TestEqual(TEXT("corpse acquisition spends no Mana"), Caster->GetAttributes()->GetClassResource(), ManaBeforeCorpse);
    Target->GetOwner()->SetActorEnableCollision(false);
    UBreakerCombatComponent* NextTarget = SpawnTarget(1);
    Caster->GetAttributes()->ApplyHealth(20);
    if (!TestTrue(TEXT("new living target is immediately castable after death"), ASC->TryActivateAbility(Handle))) return false;
    TestTrue(TEXT("new channel actually remains active until its hit"), Active());
    Advance(12);
    TestTrue(TEXT("Siphon's own damage kills the next target"), NextTarget->IsDead());
    TestFalse(TEXT("lethal tick ends its own channel safely"), Active());
    TestFalse(TEXT("lethal tick clears the new HUD window"), State->IsWindowActive(UBreakerAbility_Siphon::ChannelWindowKey()));
    TestTrue(TEXT("lethal tick still heals from damage actually earned"), Caster->GetAttributes()->GetHealth() > 20);
    const float HealthAfterKill = Caster->GetAttributes()->GetHealth();
    Advance(20);
    TestEqual(TEXT("dead-target channel cannot continue healing on timer ticks"), Caster->GetAttributes()->GetHealth(), HealthAfterKill);
    return true;
}
#endif
