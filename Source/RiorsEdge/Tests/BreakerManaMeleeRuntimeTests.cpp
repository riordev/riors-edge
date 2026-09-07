#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerManaMeleeRuntimeTest,
    "RiorsEdge.Classes.Mana.MeleeContactChargeRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerManaMeleeRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Transient combat world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AActor* Caster = World->SpawnActor<AActor>();
    AActor* Target = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Caster"), Caster) || !TestNotNull(TEXT("Target"), Target)) return false;
    auto AddComponent = [](AActor* Owner, UActorComponent* Component)
    {
        Owner->AddInstanceComponent(Component);
        Component->RegisterComponent();
    };
    UBreakerAttributeSet* SourceAttributes = NewObject<UBreakerAttributeSet>(Caster);
    UBreakerAttributeSet* TargetAttributes = NewObject<UBreakerAttributeSet>(Target);
    UBreakerCombatComponent* SourceCombat = NewObject<UBreakerCombatComponent>(Caster);
    UBreakerCombatComponent* TargetCombat = NewObject<UBreakerCombatComponent>(Target);
    AddComponent(Caster, SourceCombat);
    AddComponent(Target, TargetCombat);
    SourceCombat->BindAttributes(SourceAttributes);
    TargetCombat->BindAttributes(TargetAttributes);
    TargetAttributes->ApplyMaxHealth(1000);
    TargetAttributes->ApplyHealth(1000);
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Caster);
    AddComponent(Caster, Progression);
    Progression->BindAttributes(SourceAttributes);
    FBreakerProgressionState State;
    State.PermanentClass = EBreakerClassId::Caster;
    State.UnspentDoctrinePoints = 8;
    Progression->LoadProgressionState(State);
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>(Caster);
    AddComponent(Caster, Weapon);
    UBreakerManaComponent* Mana = NewObject<UBreakerManaComponent>(Caster);
    AddComponent(Caster, Mana);
    Mana->PassiveRegenPerSecond = 0;
    Mana->BindAttributes(SourceAttributes);
    // Rebinding must not register another melee listener.
    Mana->BindAttributes(SourceAttributes);
    SourceAttributes->ApplyClassResource(0);
    FBreakerDamageRequest Melee;
    Melee.BaseDamage = 10;
    Melee.bCanCritical = false;
    Melee.bBypassShield = true;
    Melee.SetInstigator(Caster);
    Melee.SourceTags.AddTag(BreakerAbilityTags::Damage_Melee.GetTag());
    Melee.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Cleave.GetTag());
    TestTrue(TEXT("Actual melee damage lands"), TargetCombat->ReceiveDamage(Melee).HealthDamage > 0);
    TestEqual(TEXT("Melee generation is queued, not an uncapped refund"), Mana->GetMana(), 0.0f);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Baseline melee pays the existing weapon-hit rate once"), Mana->GetMana(), Mana->WeaponHitGain);
    FText Failure;
    if (!TestTrue(TEXT("Real Contact Charge purchase succeeds"), Progression->PurchaseNode(
        UBreakerProgressionLibrary::GetCasterSpellbladeTree(), TEXT("Caster.Spellblade.ContactCharge"), Failure))) return false;
    float Before = Mana->GetMana();
    TargetCombat->ReceiveDamage(Melee);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Purchased Contact Charge replaces hit rate with weak-point rate"), Mana->GetMana() - Before, Mana->WeakPointGain);
    Before = Mana->GetMana();
    TargetCombat->ReceiveDamage(Melee);
    TargetCombat->ReceiveDamage(Melee);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Multiple melee hits share the existing conditional-generation cap"), Mana->GetMana() - Before, Mana->GlobalGenerationCap);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Metered melee credit retains its unpaid remainder"), Mana->GetMana() - Before, 2 * Mana->WeakPointGain);
    Before = Mana->GetMana();
    TargetCombat->DodgeChance = 1;
    TestTrue(TEXT("Fixture produces an actual dodge"), TargetCombat->ReceiveDamage(Melee).bDodged);
    TargetCombat->DodgeChance = 0;
    FBreakerDamageRequest Dot = Melee;
    Dot.bIsDamageOverTime = true;
    Dot.ProcCoefficient = 0;
    TestTrue(TEXT("Tagged DoT still damages target"), TargetCombat->ReceiveDamage(Dot).HealthDamage > 0);
    FBreakerDamageRequest Zero = Melee;
    Zero.BaseDamage = 0;
    TargetCombat->ReceiveDamage(Zero);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Dodge, DoT and zero damage generate no melee Mana"), Mana->GetMana(), Before);
    // Real bullet damage plus the existing aggregate shot event must pay
    // only that event. The melee listener cannot pay per bullet/pellet too.
    FBreakerDamageRequest Bullet = Melee;
    Bullet.SourceTags.Reset();
    TargetCombat->ReceiveDamage(Bullet);
    FBreakerShotResult Shot;
    Shot.bFired = true;
    Shot.bHit = true;
    Weapon->OnShot.Broadcast(Shot);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Weapon retains one normalized shot credit"), Mana->GetMana() - Before, Mana->WeaponHitGain);
    Before = Mana->GetMana();
    Mana->PushGenerationSuspension(TEXT("Test.Unmake"));
    TargetCombat->ReceiveDamage(Melee);
    Mana->PopGenerationSuspension(TEXT("Test.Unmake"));
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Suspension discards melee income instead of banking it"), Mana->GetMana(), Before);
    FBreakerDamageRequest Kill = Melee;
    Kill.BaseDamage = 100000;
    TestTrue(TEXT("Actual killing melee lands"), TargetCombat->ReceiveDamage(Kill).bKilled);
    TargetCombat->ReceiveDamage(Kill);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Killing hit pays once; striking corpse pays nothing"), Mana->GetMana() - Before, Mana->WeakPointGain);
    // A class change removes the resource hook on the next loop refresh.
    Progression->DevForceClass(EBreakerClassId::Swift);
    Mana->AdvanceLoop(1);
    TargetAttributes->ApplyHealth(1000);
    Before = Mana->GetMana();
    TargetCombat->ReceiveDamage(Melee);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Non-Caster does not gain Mana from melee"), Mana->GetMana(), Before);
    return true;
}
#endif
