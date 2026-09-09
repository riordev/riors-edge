#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_HardStop.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerHardStopExpiryRuntimeTest,
    "RiorsEdge.Abilities.HardStopPaidExpiry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerHardStopExpiryRuntimeTest::RunTest(const FString& Parameters)
{
    for (bool bSpendToLive : {false, true})
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
    if (!TestTrue(TEXT("Actual Swift class"), Progression->ChoosePermanentClassById(EBreakerClassId::Swift))) return false;
    // Restored benchmark entitlement fixture, not a claim of a campaign run.
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Flags.Add(TEXT("Quest.Finale.Seal"));
    Progression->SettleDoctrineEntitlement(Flags);
    FText Reason;
    const int32 Tokens = Progression->GetUnspentAbilityTokens();
    if (!TestTrue(TEXT("Earned token unlocks actual Hard Stop"), Progression->SpendAbilityToken(TEXT("Swift.HardStop"), Reason))) return false;
    TestEqual(TEXT("Unlock spends a token"), Progression->GetUnspentAbilityTokens(), Tokens - 1);
    const auto Slot = EBreakerAbilitySlot::ClassAbilityOne;
    if (!TestTrue(TEXT("Actual loadout equips Hard Stop"), Progression->EquipAbility(Slot, TEXT("Swift.HardStop"), Reason))) return false;
    auto* Abilities = Player->GetAbilities();
    Abilities->RefreshGrants();
    if (!TestTrue(TEXT("Native ability is granted"), Abilities->IsSlotGranted(Slot))) return false;
    TestEqual(TEXT("Base cost before first activation"), Abilities->GetCost(Slot), 30.0f, .0001f);
    if (bSpendToLive)
    {
    const auto* Tree = UBreakerProgressionLibrary::GetSwiftKineticTree();
    for (const TCHAR* Node : {TEXT("Swift.Kinetic.ReadTheRoom"), TEXT("Swift.Kinetic.ReadTheRoom"),
        TEXT("Swift.Kinetic.Redirect"), TEXT("Swift.Kinetic.Redirect"),
        TEXT("Swift.Kinetic.Landing"), TEXT("Swift.Kinetic.Landing"), TEXT("Swift.Kinetic.SpendToLive")})
    {
        const bool Bought = Progression->PurchaseNode(Tree, Node, Reason);
        if (!TestTrue(FString::Printf(TEXT("Legal purchase %s: %s"), Node, *Reason.ToString()), Bought)) return false;
    }
    TestEqual(TEXT("Legal path spends exactly eight Doctrine"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
    TestEqual(TEXT("Live instance quotes doubled cost before first cast"), Abilities->GetCost(Slot), 60.0f, .0001f);
    }
    auto* Momentum = Player->GetMomentum();
    Momentum->BindAttributes(Attributes);
    Momentum->SetComponentTickEnabled(false);
    Movement->SetMovementMode(MOVE_Walking);
    auto EarnStep = [&]()
    {
        Movement->Velocity = FVector(Movement->WalkSpeed, 0, 0);
        Player->SetActorLocation(Player->GetActorLocation() + Movement->Velocity);
        Momentum->AdvanceLoop(1);
    };
    for (int32 Step = 0; Step < 40 && Momentum->GetMomentum() < 90; ++Step) EarnStep();
    Movement->StopMovementImmediately();
    const float Cost = bSpendToLive ? 60.0f : 30.0f;
    const float Before = Momentum->GetMomentum();
    if (!TestTrue(TEXT("Normal movement funds paid protection"), Before >= Cost)) return false;
    if (!TestTrue(TEXT("Actual paid Hard Stop"), Abilities->TryActivateSlot(Slot))) return false;
    TestEqual(TEXT("Actual authored payment"), Before - Momentum->GetMomentum(), Cost, .001f);
    auto* Combat = Player->GetCombat();
    auto Hit = [&]()
    {
        FBreakerDamageRequest Request;
        Request.BaseDamage = 20;
        Request.DamageFamily = EBreakerDamageFamily::Elemental;
        Request.bCanCritical = false;
        Request.bCanBeAvoided = false;
        Request.bBypassShield = true;
        return Combat->ReceiveDamage(Request).HealthDamage;
    };
    TestEqual(TEXT("Paid window applies its actual damage protection"), Hit(),
        20.0f * UBreakerAbility_HardStop::IncomingMultiplier(bSpendToLive,
            UBreakerAbility_HardStop::DamageReductionFraction), .001f);
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { GFrameCounter = Frame; };
    auto AdvanceFrames = [&](int32 Count)
    {
        for (int32 I=0; I<Count; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.05f); }
    };
    if (!bSpendToLive)
    {
        AdvanceFrames(4);
        // Controlled cooldown-reset supplement: no claim that a starter has
        // sub-window cooldowns. Both casts still pay ordinary earned Momentum.
        FGameplayTagContainer Tags;
        Tags.AddTag(Abilities->GetDefinitionForSlot(Slot)->CooldownTag);
        ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags));
        const float BeforeRecast = Momentum->GetMomentum();
        if (!TestTrue(TEXT("Paid recast after explicit diagnostic cooldown reset"),Abilities->TryActivateSlot(Slot))) return false;
        TestEqual(TEXT("Recast pays without a refill"),BeforeRecast-Momentum->GetMomentum(),Cost,.001f);
        AdvanceFrames(9);
        TestEqual(TEXT("Old deadline cannot remove refreshed protection"),Hit(),
            20.0f * UBreakerAbility_HardStop::IncomingMultiplier(false,UBreakerAbility_HardStop::DamageReductionFraction),.001f);
        AdvanceFrames(5);
    }
    else AdvanceFrames(16);
    TestEqual(TEXT("Actual world time releases protection after ability already ended"), Hit(),20.0f,.001f);
    FBreakerDamageRequest Lethal;
    Lethal.BaseDamage = Attributes->GetHealth() + Attributes->GetShield() + 1;
    Lethal.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Lethal.bCanCritical = false; Lethal.bCanBeAvoided = false;
    Combat->ReceiveDamage(Lethal);
    if (!TestTrue(TEXT("Expired immunity permits actual lethal damage"), Combat->IsDead())) return false;
    Combat->RestoreVitals();
    TestEqual(TEXT("Revive does not restore expired protection"),Hit(),20.0f,.001f);
    }
    return true;
}
#endif
