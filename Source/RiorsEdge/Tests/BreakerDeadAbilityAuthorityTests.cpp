#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Slipcut.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerSaveGame.h"
#include "Weapons/BreakerWeaponComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDeadAbilityAuthorityTest,
    "RiorsEdge.Abilities.DeadAuthorityRefusal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDeadAbilityAuthorityTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->SetActorTickEnabled(false);
    auto* Movement = Player->GetBreakerMovement();
    Movement->SetComponentTickEnabled(false);
    auto* Attributes = Player->GetAttributes();
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Attributes);
    Player->GetCombat()->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression();
    Progression->BindAttributes(Attributes);
    if (!TestTrue(TEXT("Fresh character chooses actual Swift kit"), Progression->ChoosePermanentClassById(EBreakerClassId::Swift))) return false;
    const auto Slot = EBreakerAbilitySlot::ClassAbilityOne;
    TestEqual(TEXT("Starter is Slipcut"), Progression->GetProgressionState().AbilityLoadout.ClassAbilityOne, FName(TEXT("Swift.Slipcut")));
    TestTrue(TEXT("Second slot intentionally starts empty"), Progression->GetProgressionState().AbilityLoadout.ClassAbilityTwo.IsNone());
    TestEqual(TEXT("Fresh starter needs no token purchase"), Progression->GetUnspentAbilityTokens(), 0);
    TestTrue(TEXT("Slipcut is unlocked through actual starter definition"), Progression->IsAbilityUnlocked(TEXT("Swift.Slipcut")));
    TestNull(TEXT("Retired Skim cannot resolve in the catalogue"), UBreakerAbilityDefinition::FindFallback(TEXT("Swift.Skim")));
    auto* Abilities = Player->GetAbilities();
    Abilities->RefreshGrants();
    if (!TestTrue(TEXT("Registered starter grants through equipment"), Abilities->IsSlotGranted(Slot))) return false;
    auto* Momentum = Player->GetMomentum();
    Momentum->BindAttributes(Attributes);
    Momentum->SetComponentTickEnabled(false);
    Movement->SetMovementMode(MOVE_Walking);
    const float Quoted = Abilities->GetCost(Slot);
    for (int32 Step = 0; Step < 40 && Momentum->GetMomentum() < Quoted; ++Step)
    {
        Movement->Velocity = FVector(Movement->WalkSpeed, 0, 0);
        Player->SetActorLocation(Player->GetActorLocation() + Movement->Velocity);
        Momentum->AdvanceLoop(1);
    }
    Movement->StopMovementImmediately();
    const float BaseRate = Player->GetWeapon()->GetFireRateMultiplier();
    const float Before = Momentum->GetMomentum();
    if (!TestTrue(TEXT("Native movement earns starter resource cost"), Before >= Quoted)) return false;
    // No Character BeginPlay or automatic respawn timer: exercise the actual
    // lethal combat state independently from input suppression/resource cleanup.
    FBreakerDamageRequest Lethal;
    Lethal.BaseDamage = Attributes->GetMaxHealth() + Attributes->GetMaxShield() + 1000.0f;
    Lethal.bCanCritical = false;
    Lethal.bCanBeAvoided = false;
    Lethal.bBypassShield = true;
    Player->GetCombat()->ReceiveDamage(Lethal);
    if (!TestTrue(TEXT("Actual combat damage leaves the owner dead"), Player->GetCombat()->IsDead())) return false;
    const float DeadResource = Momentum->GetMomentum();
    TestFalse(TEXT("Authority slot request refuses dead owner before spending"), Abilities->TryActivateSlot(Slot));
    const FGameplayAbilitySpec* StarterSpec = ASC->FindAbilitySpecFromClass(UBreakerAbility_Slipcut::StaticClass());
    if (!TestNotNull(TEXT("Real granted starter spec"), StarterSpec)) return false;
    TestFalse(TEXT("Direct GAS request cannot bypass dead-owner refusal"), ASC->TryActivateAbility(StarterSpec->Handle));
    TestEqual(TEXT("Refused requests preserve earned resource"), Momentum->GetMomentum(), DeadResource);
    TestFalse(TEXT("Dead requests create no ability window"), UBreakerAbilityStateComponent::FindOrAdd(Player)->IsWindowActive(UBreakerAbility_Slipcut::WindowKey()));
    TestEqual(TEXT("Dead requests add no cadence"), Player->GetWeapon()->GetFireRateMultiplier(), BaseRate);
    Player->GetCombat()->RestoreVitals();
    TestFalse(TEXT("Actual vitals restoration clears death"), Player->GetCombat()->IsDead());
    if (!TestTrue(TEXT("Fresh equipped Slipcut activates through actual slot"), Abilities->TryActivateSlot(Slot))) return false;
    TestEqual(TEXT("Starter still pays its actual resource cost"), Before - Momentum->GetMomentum(), Quoted, .001f);
    TestEqual(TEXT("Starter pays its actual weapon cadence lane"), Player->GetWeapon()->GetFireRateMultiplier(), BaseRate * 2.0f, .001f);
    TestTrue(TEXT("Native Slipcut window is active"), UBreakerAbilityStateComponent::FindOrAdd(Player)->IsWindowActive(UBreakerAbility_Slipcut::WindowKey()));
    return true;
}
#endif
