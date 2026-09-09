#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Tests/BreakerCastTestHelpers.h"

// ---------------------------------------------------------------------------
// O266, the half of the rule that is a RISK: damage interrupts a cast, and the
// Mana is not returned. Both halves are owner-ruled and neither is provable
// from the pure rule, so this is a real world with a real hit landing inside a
// real wind-up.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCastInterruptRuntimeTest,
    "RiorsEdge.Abilities.CastTime.InterruptRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCastInterruptRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto Clock = [&](float Seconds) { for (float T = 0; T < Seconds; T += .01f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); } };

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Origin(0, 0, 0);
    ABreakerCharacter* Caster = World->SpawnActor<ABreakerCharacter>(Origin, FRotator::ZeroRotator, Spawn);
    ABreakerCharacter* Target = World->SpawnActor<ABreakerCharacter>(Origin + FVector(200, 0, 0), FRotator(0, 180, 0), Spawn);
    if (!Caster || !Target) return false;
    for (ABreakerCharacter* Actor : { Caster, Target })
    {
        Actor->SetActorTickEnabled(false);
        Actor->GetBreakerMovement()->SetComponentTickEnabled(false);
        UAbilitySystemComponent* ASC = Actor->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Actor, Actor);
        ASC->AddAttributeSetSubobject(Actor->GetAttributes());
        Actor->GetCombat()->BindAttributes(Actor->GetAttributes());
        Actor->GetProgression()->BindAttributes(Actor->GetAttributes());
    }
    if (!TestTrue(TEXT("Caster locks its class"), Caster->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    UBreakerManaComponent* Mana = Caster->GetMana();
    Mana->BindAttributes(Caster->GetAttributes());
    UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Caster);
    State->RegisterAllComponentTickFunctions(true);
    State->SetComponentTickEnabled(true);
    if (!State->HasBegunPlay()) State->BeginPlay();
    UBreakerAbilityComponent* Abilities = Caster->GetAbilities();
    FText Reason;
    if (!TestTrue(TEXT("Cleave equips"), Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityOne, TEXT("Caster.Cleave"), Reason))) return false;
    Abilities->RefreshGrants();

    const float WindUp = BreakerAuthoredCastSeconds(TEXT("Caster.Cleave"));
    if (!TestTrue(TEXT("Cleave authors a wind-up to interrupt"), WindUp > 0.05f)) return false;

    // --- The interrupted cast ------------------------------------------------
    const float ManaBefore = Mana->GetMana();
    const float TargetBefore = Target->GetAttributes()->GetHealth();
    if (!TestTrue(TEXT("Cleave activates"), Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne))) return false;

    // O266: the price is paid on the KEYPRESS, before anything has resolved.
    const float ManaAfterPress = Mana->GetMana();
    TestTrue(TEXT("the keypress has already paid"), ManaAfterPress < ManaBefore);
    TestTrue(TEXT("the wind-up is running as a HUD window"),
        State->IsWindowActive(UBreakerGameplayAbility::CastWindowKey(FName(TEXT("Caster.Cleave")))));

    // A real hit lands on the caster a third of the way into the wind-up.
    Clock(WindUp * .33f);
    FBreakerDamageRequest Hit;
    Hit.BaseDamage = 12.0f;
    Hit.bCanCritical = false;
    Hit.DamageFamily = EBreakerDamageFamily::Physical;
    Hit.SetInstigator(Target);
    const FBreakerDamageResult Landed = Caster->GetCombat()->ReceiveDamage(Hit);
    if (!TestTrue(TEXT("the interrupting hit actually lands"), Landed.HealthDamage + Landed.ShieldDamage > 0.0f)) return false;

    // Past where the swing would have resolved.
    Clock(WindUp + .10f);

    TestEqual(TEXT("an interrupted cast never swings"), Target->GetAttributes()->GetHealth(), TargetBefore, .0001f);
    TestFalse(TEXT("and its window is gone"),
        State->IsWindowActive(UBreakerGameplayAbility::CastWindowKey(FName(TEXT("Caster.Cleave")))));
    // NO REFUND, owner-ruled. Mana may only have moved UPWARD from ordinary
    // regeneration since the press; it must never have been handed back in a
    // lump, so it stays strictly below what a refund would have restored.
    TestTrue(TEXT("an interrupted cast is not refunded"), Mana->GetMana() < ManaBefore);

    // --- And an uninterrupted one still lands --------------------------------
    // The control: without the hit, the same cast resolves. Without this the
    // test above would pass just as well against an ability that never worked.
    Clock(GetDefault<UBreakerAbility_Cleave>()->AnimationLockSeconds + .10f);
    const float SecondBefore = Target->GetAttributes()->GetHealth();
    if (!TestTrue(TEXT("Cleave activates again"), Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne))) return false;
    Clock(WindUp + .10f);
    TestTrue(TEXT("an undisturbed cast does swing"), Target->GetAttributes()->GetHealth() < SecondBefore);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
