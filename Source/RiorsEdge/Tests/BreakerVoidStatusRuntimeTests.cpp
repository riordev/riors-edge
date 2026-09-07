#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Components/SceneComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusRules.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerVoidStatusRuntimeTest, "RiorsEdge.Combat.VoidStatusRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerVoidStatusRuntimeTest::RunTest(const FString& Parameters)
{
    const auto Spec = BreakerStatusRules::MakeVoidSpec();
    TestTrue(TEXT("Real registered Void factory"), Spec.StatusTag.IsValid());
    TestEqual(TEXT("Authored O2 four second duration"), Spec.Duration, 4.0f);
    TestEqual(TEXT("Void carries no DoT damage"), Spec.BaseDamagePerTick, 0.0f);
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    AActor* Target = World->SpawnActor<AActor>();
    AActor* Source = World->SpawnActor<AActor>();
    if (!Target || !Source) return false;
    UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Target);
    Target->AddInstanceComponent(ASC); ASC->RegisterComponent(); ASC->InitAbilityActorInfo(Target, Target);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Target);
    ASC->AddAttributeSetSubobject(Attributes);
    Attributes->ApplyMaxHealth(1000); Attributes->ApplyHealth(500);
    ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetArmorAttribute(), 100);
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent(); Combat->BindAttributes(Attributes);
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status); Status->RegisterComponent();
    auto Apply = [&] { Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, Source); };
    Apply();
    if (!TestEqual(TEXT("One real timed Void entry"), Status->GetActiveStatuses().Num(), 1)) return false;
    TestEqual(TEXT("Armor reduction is twenty percent of actual armor"), Combat->GetEffectiveArmor(), 80.0f);
    TestEqual(TEXT("Healing receives twenty-five percent reduction"), Combat->ApplyHealingAmount(100, Source, FGameplayTag()).HealthHealed, 75.0f);
    const float Before = Attributes->GetHealth();
    Status->AdvanceStatuses(2);
    TestEqual(TEXT("Effect-only status never damages during elapsed time"), Attributes->GetHealth(), Before);
    TestEqual(TEXT("Effect-only status emits no synthetic damage ticks"), Status->GetActiveStatuses()[0].TicksDelivered, 0);
    Apply();
    TestEqual(TEXT("Refresh keeps one status type"), Status->GetActiveStatuses().Num(), 1);
    TestEqual(TEXT("Refresh cannot multiply stacks"), Status->GetActiveStatuses()[0].Stacks, 1);
    TestEqual(TEXT("Refresh restores duration"), Status->GetActiveStatuses()[0].RemainingDuration, 4.0f);
    TestEqual(TEXT("Refresh does not compound armor reduction"), Combat->GetEffectiveArmor(), 80.0f);
    Status->AdvanceStatuses(4);
    TestFalse(TEXT("Exact expiry removes Void"), Status->HasStatus(Spec.StatusTag));
    TestEqual(TEXT("Expiry restores actual armor"), Combat->GetEffectiveArmor(), 100.0f);
    TestEqual(TEXT("Expiry restores healing"), Combat->ApplyHealingAmount(100, Source, FGameplayTag()).HealthHealed, 100.0f);
    Apply(); bool Found = false;
    Status->ConsumeStatus(Spec.StatusTag, Found);
    TestTrue(TEXT("Consume returns real Void entry"), Found);
    TestEqual(TEXT("Consume removes effect immediately"), Combat->GetEffectiveArmor(), 100.0f);
    Status->GrantStatusImmunity(1); Apply();
    TestFalse(TEXT("Existing immunity rejects Void"), Status->HasStatus(Spec.StatusTag));
    Status->AdvanceStatuses(1);
    Status->AilmentAvoidanceChance = .75f;
    int32 Avoided = 0, Accepted = 0;
    for (int32 Attempt = 0; Attempt < 64; ++Attempt)
    {
        Apply();
        if (Status->HasStatus(Spec.StatusTag)) ++Accepted; else ++Avoided;
        Status->ConsumeStatus(Spec.StatusTag, Found);
    }
    TestTrue(TEXT("Void uses ordinary avoidance roll rather than bypassing it"), Avoided > 0 && Accepted > 0);
    Status->AilmentAvoidanceChance = 0;
    ABreakerCharacter* Caster = World->SpawnActor<ABreakerCharacter>();
    if (!Caster) return false;
    Caster->GetAbilitySystemComponent()->InitAbilityActorInfo(Caster, Caster);
    Caster->GetAbilitySystemComponent()->AddAttributeSetSubobject(Caster->GetAttributes());
    Caster->GetProgression()->BindAttributes(Caster->GetAttributes());
    if (!Caster->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
    Caster->GetProgression()->GrantPlaytestPoints(3, 0);
    FText Error;
    const auto* Tree = UBreakerProgressionLibrary::GetCasterMultispellTree();
    for (const TCHAR* Node : {TEXT("Caster.Multispell.Variance"), TEXT("Caster.Multispell.Variance"), TEXT("Caster.Multispell.Chain")})
        if (!TestTrue(TEXT("Real purchased Chain path"), Caster->GetProgression()->PurchaseNode(Tree, Node, Error))) return false;
    AActor* Recipient = World->SpawnActor<AActor>();
    if (!Recipient) return false;
    USceneComponent* Root = NewObject<USceneComponent>(Recipient);
    Recipient->AddInstanceComponent(Root); Recipient->SetRootComponent(Root); Root->RegisterComponent();
    Recipient->SetActorLocation(FVector(150, 0, 0));
    UBreakerCombatComponent* RecipientCombat = NewObject<UBreakerCombatComponent>(Recipient);
    Recipient->AddInstanceComponent(RecipientCombat); RecipientCombat->RegisterComponent();
    UBreakerAttributeSet* RecipientAttributes = NewObject<UBreakerAttributeSet>(Recipient);
    RecipientAttributes->ApplyMaxHealth(100); RecipientAttributes->ApplyHealth(100); RecipientCombat->BindAttributes(RecipientAttributes);
    UBreakerStatusComponent* RecipientStatus = NewObject<UBreakerStatusComponent>(Recipient);
    Recipient->AddInstanceComponent(RecipientStatus); RecipientStatus->RegisterComponent();
    FBreakerStatusApplicationSpec Poison;
    Poison.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison")); Poison.Duration = 10; Poison.TickInterval = 1; Poison.BaseDamagePerTick = 1;
    Status->ApplyStatus(Poison, EBreakerDamageFamily::Physical, Caster);
    Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, Caster);
    TestTrue(TEXT("Actual purchased Chain carries real Void onto nearby target"), RecipientStatus->HasStatus(Spec.StatusTag));
    TestEqual(TEXT("Chained Void has its actual healing effect"), RecipientStatus->GetHealingReceivedMultiplier(), .75f);
    RecipientStatus->ConsumeAllStatuses();
    TestEqual(TEXT("Consuming chained Void restores recipient"), RecipientStatus->GetHealingReceivedMultiplier(), 1.0f);
    Apply();
    FBreakerDamageRequest Lethal; Lethal.BaseDamage = 100000; Lethal.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Lethal.bBypassShield = true; Lethal.bCanCritical = false; Lethal.SetInstigator(Source);
    Combat->ReceiveDamage(Lethal);
    TestTrue(TEXT("Actual damage kills afflicted owner"), Combat->IsDead());
    TestFalse(TEXT("Death consumes timed defense effect"), Status->HasStatus(Spec.StatusTag));
    TestEqual(TEXT("Dead owner retains no Void armor multiplier"), Status->GetArmorMultiplier(), 1.0f);
    Apply(); TestFalse(TEXT("Corpse rejects status reapplication"), Status->HasStatus(Spec.StatusTag));
    return true;
}
#endif
