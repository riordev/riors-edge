#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerOverreachRuntimeTest,"RiorsEdge.Abilities.Caster.OverreachRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerOverreachRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto Clock=[&](float Seconds){for(float T=0;T<Seconds;T+=.01f){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}};
    auto* Player=World->SpawnActor<ABreakerCharacter>();if(!Player)return false;
    Player->SetActorTickEnabled(false);Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC=Player->GetAbilitySystemComponent();auto* Attributes=Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attributes);
    auto* Combat=Player->GetCombat();Combat->BindAttributes(Attributes);
    auto* Progression=Player->GetProgression();Progression->BindAttributes(Attributes);
    // Shipped Caster kit; temporary Conduction schema tests the existing
    // keystone interaction at its real five-point price, not a free tag.
    auto* Class=DuplicateObject<UBreakerClassDefinition>(UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Caster),Player);
    auto* Core=NewObject<UBreakerProgressionTree>(Class);Core->TreeId=TEXT("Test.Core.OverreachConduction");Core->Currency=EBreakerPointCurrency::CorePoints;
    auto* Conduction=NewObject<UBreakerProgressionNode>(Core);Conduction->NodeId=TEXT("Test.Core.Conduction");Conduction->Currency=Core->Currency;
    Conduction->CostPerRank=5;Conduction->MaxRank=1;Conduction->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Conduction")));
    Core->Nodes.Add(Conduction);Class->BranchTrees.Add(Core);
    if(!Progression->ChoosePermanentClass(Class))return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(6,Progression->ExperienceCurve));
    auto* Mana=Player->GetMana();Mana->BindAttributes(Attributes);
    // Begin native resource/window components only; Character BeginPlay would
    // access owner saves. Recovery below runs through real world ticks.
    Mana->RegisterAllComponentTickFunctions(true);Mana->SetComponentTickEnabled(true);if(!Mana->HasBegunPlay())Mana->BeginPlay();
    auto* State=UBreakerAbilityStateComponent::FindOrAdd(Player);
    State->RegisterAllComponentTickFunctions(true);State->SetComponentTickEnabled(true);if(!State->HasBegunPlay())State->BeginPlay();
    auto* Abilities=Player->GetAbilities();FText Reason;
    const auto Melee=EBreakerAbilitySlot::ClassAbilityOne;const auto Spell=EBreakerAbilitySlot::ClassAbilityTwo;const auto Ultimate=EBreakerAbilitySlot::Ultimate;
    if(!Progression->IsAbilityUnlocked(TEXT("Caster.Fracture"))&&!Progression->SpendAbilityToken(TEXT("Caster.Fracture"),Reason))return false;
    if(!Abilities->TryEquipAbility(Melee,TEXT("Caster.Cleave"),Reason)||!Abilities->TryEquipAbility(Spell,TEXT("Caster.Fracture"),Reason)
        ||!Abilities->TryEquipAbility(Ultimate,TEXT("Caster.Unmake"),Reason))return false;
    Abilities->RefreshGrants();
    const auto* Tree=UBreakerProgressionLibrary::GetCasterSpellbladeTree();const FName Overreach(TEXT("Caster.Spellblade.Overreach"));
    const auto* Node=Tree->FindNode(Overreach);if(!TestNotNull(TEXT("Real authored Overreach"),Node))return false;
    TestEqual(TEXT("Tier four"),Node->Tier,4);TestEqual(TEXT("Two points"),Node->CostPerRank,2);TestEqual(TEXT("One rank"),Node->MaxRank,1);
    TestEqual(TEXT("Six investment gate"),Node->RequiredTreeInvestment,6);
    if(!TestEqual(TEXT("One historical prerequisite"),Node->Prerequisites.Num(),1))return false;
    TestEqual(TEXT("Historical Debt prerequisite"),Node->Prerequisites[0].NodeId,FName(TEXT("Caster.Spellblade.Debt")));
    FBreakerQuestFlagSet Flags;for(const auto& Mission:UBreakerMissionLibrary::GetMissions())for(const auto& Beat:Mission.Beats)
        for(const auto& Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat))Flags.Add(Flag);
    Progression->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("Real campaign entitlement is eight"),Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),8);
    TestFalse(TEXT("Wallet alone does not bypass investment"),Progression->PurchaseNode(Tree,Overreach,Reason));
    auto BuySix=[&](){for(const TCHAR* Id:{TEXT("Caster.Spellblade.ContactCharge"),TEXT("Caster.Spellblade.FollowThrough"),TEXT("Caster.Spellblade.Close"),TEXT("Caster.Spellblade.Debt"),TEXT("Caster.Spellblade.MomentumTransfer"),TEXT("Caster.Spellblade.Bloodprice")})
        if(!Progression->PurchaseNode(Tree,Id,Reason))return false;return true;};
    if(!BuySix())return false;
    auto CastSlot=[&](EBreakerAbilitySlot Slot,float ExpectedCost)
    {
        const float Before=Mana->GetMana();TestEqual(TEXT("Live quote"),Abilities->GetResourceCostForSlot(Slot),ExpectedCost,.001f);
        if(!TestTrue(TEXT("Equipped native cast succeeds"),Abilities->TryActivateSlot(Slot)))return false;
        TestEqual(TEXT("Actual debit matches live quote"),Mana->GetMana(),Before-ExpectedCost,.001f);return true;
    };
    auto EnterDebt=[&]()
    {
        // O266 slowed the spam this loop depends on: a wind-up plus a lock is
        // longer than the old 0.55s spacing, so Mana regenerates more per cast
        // and the bank takes more repetitions to go negative. That is the
        // point of the wind-up, not a defect — the loop's job is only to REACH
        // debt so the overcast rules below can be tested.
        for(int32 Cast=0;Cast<60;++Cast)
        {
            if(Mana->GetMana()<0)return true;
            if(!CastSlot(Melee,15))return false;
            if(Mana->GetMana()<0)return true;
            // O266: one Cleave now occupies its WIND-UP and then its animation
            // lock, so the spacing between repeat casts is both, read from the
            // file rather than restated as a literal.
            Clock(BreakerAuthoredCastSeconds(TEXT("Caster.Cleave"))
                +GetDefault<UBreakerAbility_Cleave>()->AnimationLockSeconds+.05f);
        }
        AddError(TEXT("Normal repeated paid Cleave never reached debt"));return false;
    };
    if(!EnterDebt())return false;
    // Synchronize the native overcast transition while retaining the small debt.
    Clock(.01f);
    TestEqual(TEXT("Unowned debt has base penalty"),Mana->GetOvercastIncomingDamageTaken(),.15f,.001f);
    TestEqual(TEXT("Base penalty uses existing incoming lane"),Combat->GetComposedIncomingDamageMultiplier(),1.15f,.001f);
    if(!Progression->PurchaseNode(Tree,Overreach,Reason))return false;
    TestEqual(TEXT("Eight-point route spends exact entitlement"),Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),0);
    TestEqual(TEXT("Authored tuning exposes thirty-percent penalty"),UBreakerManaComponent::GetResourceTuning().OverreachIncomingDamageTaken,.30f,.001f);
    TestEqual(TEXT("Acquiring while already negative updates HUD penalty"),Mana->GetOvercastIncomingDamageTaken(),.30f,.001f);
    TestEqual(TEXT("Acquiring while already negative replaces existing lane"),Combat->GetComposedIncomingDamageMultiplier(),1.30f,.001f);
    FBreakerDamageRequest Incoming;Incoming.BaseDamage=1;Incoming.bCanCritical=false;Incoming.bCanBeAvoided=false;Incoming.DamageFamily=EBreakerDamageFamily::TrueDamage;
    TestEqual(TEXT("Native incoming hit pays thirty-percent penalty"),Combat->ReceiveDamage(Incoming).HealthDamage,1.30f,.001f);
    if(!CastSlot(Spell,0))return false;
    Clock(1.5f);
    TestTrue(TEXT("Ordinary free casting does not suppress native recovery"),Mana->GetMana()>=0);
    TestFalse(TEXT("Overreach ends automatically at nonnegative Mana"),Mana->IsOverreachActive());
    TestEqual(TEXT("Ordinary price returns when debt clears"),Abilities->GetResourceCostForSlot(Spell),30.f,.001f);
    TestEqual(TEXT("Incoming penalty disappears at zero"),Combat->GetComposedIncomingDamageMultiplier(),1.f,.001f);
    Clock(.55f);
    if(!EnterDebt())return false;
    if(!CastSlot(Ultimate,0))return false;
    const float SuspendedBank=Mana->GetMana();
    // O266: THE ULTIMATE WINDS UP NOW, and the suspension starts with the CAST
    // rather than with the window. That ordering is the whole reason this
    // fixture's interaction survives a wind-up: a suspension that waited for
    // the payoff would let the bank regenerate for the length of the cast and
    // clear the very debt these lines exist to hold.
    TestTrue(TEXT("The suspension is live during the wind-up, not only the window"),Mana->IsGenerationSuspended());
    Clock(BreakerAuthoredCastSeconds(TEXT("Caster.Unmake"))+.05f);
    TestEqual(TEXT("The wind-up itself generates nothing"),Mana->GetMana(),SuspendedBank,.001f);
    TestTrue(TEXT("Literal Overreach includes Unmake suspension"),Mana->IsGenerationSuspended());
    const float Duration=State->GetWindowRemaining(UBreakerCasterAbility::UnmakeWindowKey());
    if(!TestTrue(TEXT("The window opens on the far side of the cast"),Duration>0.f))return false;
    Clock(Duration-.05f);
    TestEqual(TEXT("Free Unmake retains exact negative bank while live"),Mana->GetMana(),SuspendedBank,.001f);
    Clock(.10f);
    TestFalse(TEXT("Unmake still expires on its real clock"),Mana->IsGenerationSuspended());
    TestTrue(TEXT("Small post-expiry recovery leaves debt for another cast"),Mana->GetMana()<0);
    if(!CastSlot(Ultimate,0))return false;
    // Cancellation and immediate renewal contain no recovery frame. This is
    // the literal authored interaction, recorded rather than silently nerfed.
    const float RenewedBank=Mana->GetMana();ASC->CancelAllAbilities();
    TestFalse(TEXT("Cancellation releases suspension"),Mana->IsGenerationSuspended());
    // The cancelled-cast teardown is proved in RiorsEdge.Abilities
    // .InterruptRuntime, NOT here: catching it needs a tick past the whole
    // wind-up, and these lines depend on containing no recovery frame at all.
    if(!CastSlot(Ultimate,0))return false;
    TestEqual(TEXT("Same-frame free renewal retains debt"),Mana->GetMana(),RenewedBank,.001f);
    if(!Progression->PurchaseNode(Core,Conduction->NodeId,Reason))return false;
    Clock(.55f);
    if(!CastSlot(Spell,0))return false;
    TestTrue(TEXT("Free casts still record Conduction cadence"),Abilities->GetConductionCostMultiplier()>1.f);
    TestEqual(TEXT("Conduction cannot turn zero into a price"),Abilities->GetResourceCostForSlot(Melee),0.f,.001f);
    Clock(.5f);ASC->CancelAllAbilities();
    if(!Progression->RespecCore(Reason))return false;
    if(!Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints,true,Reason))return false;
    TestFalse(TEXT("Respec removes Overreach immediately in debt"),Mana->IsOverreachActive());
    TestEqual(TEXT("Respec in debt restores base incoming penalty"),Combat->GetComposedIncomingDamageMultiplier(),1.15f,.001f);
    if(!Abilities->TryEquipAbility(Melee,TEXT("Caster.Cleave"),Reason))return false;Abilities->RefreshGrants();
    TestEqual(TEXT("Respec restores ordinary quote"),Abilities->GetResourceCostForSlot(Melee),15.f,.001f);
    TestFalse(TEXT("Positive-cost casting is again refused in debt"),Abilities->TryActivateSlot(Melee));
    if(!BuySix()||!Progression->PurchaseNode(Tree,Overreach,Reason))return false;
    if(!Abilities->TryEquipAbility(Ultimate,TEXT("Caster.Unmake"),Reason))return false;Abilities->RefreshGrants();
    if(!CastSlot(Ultimate,0))return false;
    TestTrue(TEXT("Death probe begins with owned Overreach and live suspension"),Mana->IsOverreachActive()&&Mana->IsGenerationSuspended());
    FBreakerDamageRequest Lethal;Lethal.BaseDamage=100000;Lethal.bCanCritical=false;Lethal.bCanBeAvoided=false;Lethal.DamageFamily=EBreakerDamageFamily::TrueDamage;
    Combat->ReceiveDamage(Lethal);
    TestFalse(TEXT("Death immediately closes the ultimate window"),State->IsWindowActive(UBreakerCasterAbility::UnmakeWindowKey()));
    TestFalse(TEXT("Death immediately releases generation before respawn"),Mana->IsGenerationSuspended());
    Combat->RestoreVitals();
    TestFalse(TEXT("Death/restoration leaves no stale debt rewrite"),Mana->IsOverreachActive());
    TestFalse(TEXT("Death/restoration leaves no stale suspension"),Mana->IsGenerationSuspended());
    TestEqual(TEXT("Restored full bank carries no debt penalty"),Combat->GetComposedIncomingDamageMultiplier(),1.f,.001f);
    return true;
}
#endif
