#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace BreakerCasterContinuation
{
    ABreakerCharacter* Caster(UWorld* World)
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>(ABreakerCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
        Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        FBreakerProgressionState State;
        State.PermanentClass = EBreakerClassId::Caster;
        State.UnspentDoctrinePoints = 8; // Wiring fixture at future full doctrine budget; not campaign entitlement.
        Player->GetProgression()->LoadProgressionState(State);
        Player->GetMana()->BindAttributes(Player->GetAttributes());
        Player->GetMana()->PassiveRegenPerSecond = 0;
        Player->GetAttributes()->ApplyClassResource(0);
        return Player;
    }
    AActor* Target(UWorld* World)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        USphereComponent* Body = NewObject<USphereComponent>(Actor);
        Actor->AddInstanceComponent(Body); Actor->SetRootComponent(Body);
        Body->SetSphereRadius(40); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
        Body->RegisterComponent(); Actor->SetActorLocation(FVector(500, 0, 0));
        UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Actor);
        Actor->AddInstanceComponent(Combat); Combat->RegisterComponent();
        UBreakerAttributeSet* Attr = NewObject<UBreakerAttributeSet>(Actor);
        Attr->ApplyMaxHealth(10000); Attr->ApplyHealth(10000); Combat->BindAttributes(Attr);
        UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Actor);
        Actor->AddInstanceComponent(Status); Status->RegisterComponent();
        return Actor;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCasterContinuationRuntimeTest, "RiorsEdge.Classes.Mana.ContinuationRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCasterContinuationRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated continuation world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    ABreakerCharacter* Player = BreakerCasterContinuation::Caster(World);
    AActor* Target = BreakerCasterContinuation::Target(World);
    UBreakerManaComponent* Mana = Player->GetMana();
    auto Buy = [&](const UBreakerProgressionTree* Tree, const TCHAR* Id)
    { FText Reason; return TestTrue(Id, Player->GetProgression()->PurchaseNode(Tree, Id, Reason)); };
    const UBreakerProgressionTree* Blade = UBreakerProgressionLibrary::GetCasterSpellbladeTree();
    FBreakerShotResult Shot;
    Shot.bFired = Shot.bHit = true; Shot.HitActor = Target; Shot.DamageResult.HealthDamage = 10;
    Shot.ImpactPoint = FVector(500, 0, 0);
    auto Fire = [&] { const float Before = Mana->GetMana(); Player->GetWeapon()->OnShot.Broadcast(Shot); Mana->AdvanceLoop(1); return Mana->GetMana() - Before; };
    TestEqual(TEXT("No Close node uses baseline weapon income"), Fire(), 1.5f);
    if (!Buy(Blade, TEXT("Caster.Spellblade.Close"))) return false;
    TestEqual(TEXT("Close doubles income inside six metres"), Fire(), 3.0f);
    Shot.ImpactPoint.X = 800;
    TestEqual(TEXT("Rank one excludes distant hit"), Fire(), 1.5f);
    if (!Buy(Blade, TEXT("Caster.Spellblade.Close"))) return false;
    TestEqual(TEXT("Rank two reaches nine metres"), Fire(), 3.0f);
    Shot.DamageResult.bDodged = true;
    TestEqual(TEXT("Dodge pays no baseline or Close income"), Fire(), 0.0f);
    Shot.DamageResult.bDodged = false;
    TestEqual(TEXT("Default Overcast floor"), Mana->GetOvercastFloor(), -20.0f);
    if (!Buy(Blade, TEXT("Caster.Spellblade.ContactCharge"))) return false;
    for (int32 Rank = 1; Rank <= 2; ++Rank)
    {
        if (!Buy(Blade, TEXT("Caster.Spellblade.Debt"))) return false;
        Mana->AdvanceLoop(0.01f);
        TestEqual(TEXT("Purchased Debt reaches published ability-cost floor"), Player->GetAttributes()->GetClassResourceFloor(), -20.0f - Rank * 10.0f);
    }
    FBreakerDamageRequest Melee;
    Melee.BaseDamage = 20; Melee.bCanCritical = false; Melee.bBypassShield = true;
    Melee.SetInstigator(Player); Melee.SourceTags.AddTag(BreakerAbilityTags::Damage_Melee.GetTag());
    Player->GetAttributes()->ApplyHealth(50); Player->GetAttributes()->ApplyClassResource(-10);
    Target->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Melee);
    TestEqual(TEXT("Negative Mana alone does not heal"), Player->GetAttributes()->GetHealth(), 50.0f);
    for (int32 Rank = 1; Rank <= 2; ++Rank)
    {
        if (!Buy(Blade, TEXT("Caster.Spellblade.Bloodprice"))) return false;
        const float Before = Player->GetAttributes()->GetHealth();
        const FBreakerDamageResult Hit = Target->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Melee);
        TestEqual(TEXT("Bloodprice heals fraction of actual melee damage"), Player->GetAttributes()->GetHealth() - Before, Hit.HealthDamage * Rank * 0.1f, 0.001f);
    }
    const float BeforeExclusions = Player->GetAttributes()->GetHealth();
    Melee.bIsDamageOverTime = true;
    Target->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Melee);
    Melee.bIsDamageOverTime = false; Melee.ProcCoefficient = 0;
    Target->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Melee);
    Melee.ProcCoefficient = 1; Player->GetAttributes()->ApplyClassResource(0);
    Target->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Melee);
    TestEqual(TEXT("Bloodprice excludes DoT, zero-proc and nonnegative Mana"), Player->GetAttributes()->GetHealth(), BeforeExclusions);

    Player->Destroy();
    Player = BreakerCasterContinuation::Caster(World); Mana = Player->GetMana();
    const UBreakerProgressionTree* Void = UBreakerProgressionLibrary::GetCasterVoidWhispererTree();
    Mana->PassiveRegenPerSecond = 6;
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("No Patience means ordinary passive regen"), Mana->GetMana(), 6.0f);
    if (!Buy(Void, TEXT("Caster.VoidWhisperer.Patience"))) return false;
    Player->GetAttributes()->ApplyClassResource(0);
    FBreakerShotResult Miss; Miss.bFired = true;
    Player->GetWeapon()->OnShot.Broadcast(Miss);
    Mana->AdvanceLoop(3.5f); Mana->AdvanceLoop(1);
    TestEqual(TEXT("Miss resets Patience and crossing frame grants only eligible half-second bonus"), Mana->GetMana(), 30.0f);
    if (!Buy(Void, TEXT("Caster.VoidWhisperer.Patience"))) return false;
    Player->GetAttributes()->ApplyClassResource(0); Player->GetWeapon()->OnShot.Broadcast(Miss); Mana->AdvanceLoop(3);
    TestEqual(TEXT("Rank two idle threshold is two seconds"), Mana->GetMana(), 24.0f);
    Mana->PassiveRegenPerSecond = 0;
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    const FGameplayAbilitySpecHandle Siphon = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Siphon::StaticClass(), 1));
    for (int32 Rank = 0; Rank <= 2; ++Rank)
    {
        if (Rank && !Buy(Void, TEXT("Caster.VoidWhisperer.Drain"))) return false;
        Player->GetAttributes()->ApplyClassResource(100); Player->GetAttributes()->ApplyHealth(100);
        if (!TestTrue(TEXT("Actual Siphon channel activates"), ASC->TryActivateAbility(Siphon))) return false;
        FBreakerDamageRequest Incoming; Incoming.BaseDamage = Rank == 2 ? 12 : 8;
        Incoming.bCanCritical = false; Incoming.bBypassShield = true;
        Player->GetCombat()->ReceiveDamage(Incoming);
        TestEqual(TEXT("Purchased Drain changes live channel interruption"), ASC->FindAbilitySpecFromHandle(Siphon)->IsActive(), Rank > 0);
        Incoming.BaseDamage = 30; Player->GetCombat()->ReceiveDamage(Incoming);
        TestFalse(TEXT("Large damage still interrupts every Drain rank"), ASC->FindAbilitySpecFromHandle(Siphon)->IsActive());
    }
    Player->Destroy(); Player = BreakerCasterContinuation::Caster(World); Mana = Player->GetMana();
    UBreakerStatusComponent* Status = Target->FindComponentByClass<UBreakerStatusComponent>();
    const UBreakerProgressionTree* Multi = UBreakerProgressionLibrary::GetCasterMultispellTree();
    for (int32 Rank = 0; Rank <= 2; ++Rank)
    {
        if (Rank && !Buy(Multi, TEXT("Caster.Multispell.Variance"))) return false;
        Status->ConsumeAllStatuses();
        FBreakerStatusApplicationSpec Spec; Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
        Spec.Duration = 4; Spec.TickInterval = 1; Spec.BaseDamagePerTick = 1;
        const float Before = Mana->GetMana();
        Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, Player); Mana->AdvanceLoop(1);
        TestEqual(TEXT("Variance ranks scale only a new type"), Mana->GetMana() - Before, 2.0f * (Rank + 1));
        const float After = Mana->GetMana(); Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, Player); Mana->AdvanceLoop(1);
        TestEqual(TEXT("Variance never pays a refresh"), Mana->GetMana(), After);
    }
    if (!Buy(Void, TEXT("Caster.VoidWhisperer.Seep"))) return false;
    Status->ConsumeAllStatuses();
    FBreakerStatusApplicationSpec Spec; Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison")); Spec.Duration = 4; Spec.TickInterval = 1;
    const float BeforeStack = Mana->GetMana(); Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, Player); Mana->AdvanceLoop(2);
    TestEqual(TEXT("Seep and Variance share one baseline plus bonuses"), Mana->GetMana() - BeforeStack, 7.0f);
    return true;
}
#endif
