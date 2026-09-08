#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerCoreTree.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRosterDefenceTest, "RiorsEdge.Progression.CoreRoster.Defence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRosterDefenceTest::RunTest(const FString&)
{
    TArray<FBreakerCoreWedgeDefinition> Wedges; BreakerCoreRoster::AppendDefence(GetTransientPackage(), Wedges);
    TestEqual(TEXT("Five authored Defence wedges"), Wedges.Num(), 5);
    FString Error;
    auto* Tree = BreakerCoreTree::Build(GetTransientPackage(), TEXT("Test.Core.Defence"), FText::FromString(TEXT("Defence")), Wedges, Error);
    if (!TestNotNull(TEXT("Authored Defence builds"), Tree)) return false;
    TestEqual(TEXT("Defence contains forty nodes"), Tree->Nodes.Num(), 40);
    int32 Offered=0; for (const UBreakerProgressionNode* N : Tree->Nodes) Offered += N->CostPerRank*N->MaxRank;
    TestEqual(TEXT("Defence offers ninety-one points"), Offered, 91);
    auto Check = [&](const TCHAR* Id, const TCHAR* Name, int32 Cost, int32 Ranks, std::initializer_list<FBreakerNodeEffect> Effects, const TCHAR* Tag)
    {
        const auto* Found = Tree->Nodes.FindByPredicate([&](const UBreakerProgressionNode* N) { return N->NodeId == FName(Id); });
        if (!TestTrue(Id, Found != nullptr)) return;
        const UBreakerProgressionNode* N=Found->Get();
        TestEqual(TEXT("Literal display name"), N->DisplayName.ToString(), FString(Name));
        TestFalse(TEXT("Authored description is present"), N->Description.IsEmpty());
        TestEqual(TEXT("Shape price"),N->CostPerRank,Cost); TestEqual(TEXT("Shape ranks"),N->MaxRank,Ranks);
        TestEqual(TEXT("Exact effect count"),N->Effects.Num(),static_cast<int32>(Effects.size()));
        int32 Index=0; for (const auto& E : Effects)
        {
            if (N->Effects.IsValidIndex(Index))
            {
                TestTrue(TEXT("Exact stat"),N->Effects[Index].StatTarget==E.StatTarget);
                TestTrue(TEXT("Exact bucket"),N->Effects[Index].StatBucket==E.StatBucket);
                TestEqual(TEXT("Exact authored magnitude"),N->Effects[Index].ValuePerRank,E.ValuePerRank);
                TestTrue(TEXT("No legacy conditional"),N->Effects[Index].Condition==E.Condition);
            }
            ++Index;
        }
        TestEqual(TEXT("No stray rule tags"),N->GrantedTags.Num(),Tag?1:0);
        if (Tag) TestTrue(TEXT("Actual runtime consumer tag"),N->GrantedTags.HasTagExact(FGameplayTag::RequestGameplayTag(Tag)));
    };
    using BreakerCoreRoster::Effect;
    Check(TEXT("Core.Aegis.Footing"), TEXT("Footing"), 1, 1, {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 60.0f)}, nullptr);
    Check(TEXT("Core.Aegis.IronFrame"), TEXT("Iron Frame"), 1, 3, {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 50.0f)}, nullptr);
    Check(TEXT("Core.Aegis.Brace"), TEXT("Brace"), 2, 1, {Effect(EBreakerNodeStatTarget::Armor, EBreakerNodeStatBucket::IncreasedPercent, 20.0f)}, nullptr);
    Check(TEXT("Core.Aegis.Plate"), TEXT("Plate"), 1, 3, {Effect(EBreakerNodeStatTarget::Armor, EBreakerNodeStatBucket::IncreasedPercent, 12.0f)}, nullptr);
    Check(TEXT("Core.Aegis.Bulk"), TEXT("Bulk"), 2, 1, {Effect(EBreakerNodeStatTarget::FrontShieldPercentMaxHealth, EBreakerNodeStatBucket::Flat, 8.0f)}, nullptr);
    Check(TEXT("Core.Aegis.SecondSkin"), TEXT("Second Skin"), 1, 3, {Effect(EBreakerNodeStatTarget::PhysicalDamageReduction, EBreakerNodeStatBucket::Flat, 6.0f)}, nullptr);
    Check(TEXT("Core.Aegis.AnsweringFire"), TEXT("Answering Fire"), 2, 1, {Effect(EBreakerNodeStatTarget::Armor, EBreakerNodeStatBucket::IncreasedPercent, 18.0f), Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 80.0f)}, nullptr);
    Check(TEXT("Core.Aegis.CleanHands"), TEXT("Clean Hands"), 1, 1, {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 40.0f)}, nullptr);
    Check(TEXT("Core.Aegis.SetStance"), TEXT("Set Stance"), 1, 1, {Effect(EBreakerNodeStatTarget::Armor, EBreakerNodeStatBucket::IncreasedPercent, 10.0f)}, nullptr);
    Check(TEXT("Core.Aegis.Bastion"), TEXT("Bastion"), 3, 1, {Effect(EBreakerNodeStatTarget::EffectiveHealth, EBreakerNodeStatBucket::MorePercent, 20.0f)}, nullptr);
    Check(TEXT("Core.Aegis.Immovable"), TEXT("Immovable"), 5, 1, {}, TEXT("Progression.Node.Core.Immovable"));
    Check(TEXT("Core.Bulwark.Read"), TEXT("Read"), 1, 1, {Effect(EBreakerNodeStatTarget::BlockChance, EBreakerNodeStatBucket::Flat, 5.0f)}, nullptr);
    Check(TEXT("Core.Bulwark.Guard"), TEXT("Guard"), 1, 3, {Effect(EBreakerNodeStatTarget::BlockChance, EBreakerNodeStatBucket::Flat, 3.0f)}, nullptr);
    Check(TEXT("Core.Bulwark.Parry"), TEXT("Parry"), 2, 1, {}, TEXT("Progression.Verb.Parry"));
    Check(TEXT("Core.Bulwark.Evade"), TEXT("Evade"), 1, 3, {Effect(EBreakerNodeStatTarget::DodgeChance, EBreakerNodeStatBucket::Flat, 3.0f)}, nullptr);
    Check(TEXT("Core.Bulwark.Counterweight"), TEXT("Counterweight"), 2, 1, {Effect(EBreakerNodeStatTarget::ParryCooldownReductionSeconds, EBreakerNodeStatBucket::Flat, 0.5f), Effect(EBreakerNodeStatTarget::ParryWindowAddedSeconds, EBreakerNodeStatBucket::Flat, 0.1f)}, nullptr);
    Check(TEXT("Core.Bulwark.Interpose"), TEXT("Interpose"), 1, 3, {Effect(EBreakerNodeStatTarget::FrontShieldPercentMaxHealth, EBreakerNodeStatBucket::Flat, 6.0f)}, nullptr);
    Check(TEXT("Core.Bulwark.Riposte"), TEXT("Riposte"), 2, 1, {}, TEXT("Progression.Node.Core.Bulwark.Riposte"));
    Check(TEXT("Core.Bulwark.Anticipate"), TEXT("Anticipate"), 1, 1, {Effect(EBreakerNodeStatTarget::ParryCooldownRecovery, EBreakerNodeStatBucket::IncreasedPercent, 8.0f)}, nullptr);
    Check(TEXT("Core.Bulwark.Footwork"), TEXT("Footwork"), 1, 1, {Effect(EBreakerNodeStatTarget::DodgeChance, EBreakerNodeStatBucket::Flat, 3.0f)}, nullptr);
    Check(TEXT("Core.Bulwark.Wall"), TEXT("Wall"), 3, 1, {}, TEXT("Progression.Node.Core.Bulwark.Wall"));
    Check(TEXT("Core.Bulwark.PerfectGuard"), TEXT("Perfect Guard"), 5, 1, {}, TEXT("Progression.Node.Core.Bulwark.PerfectGuard"));
    Check(TEXT("Core.Constitution.Frame"), TEXT("Frame"), 1, 1, {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 80.0f)}, nullptr);
    Check(TEXT("Core.Constitution.Mass"), TEXT("Mass"), 1, 3, {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 60.0f)}, nullptr);
    Check(TEXT("Core.Constitution.DeepReserve"), TEXT("Deep Reserve"), 2, 1, {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::IncreasedPercent, 12.0f)}, nullptr);
    Check(TEXT("Core.Constitution.Layered"), TEXT("Layered"), 1, 3, {Effect(EBreakerNodeStatTarget::ShieldPercentMaxHealth, EBreakerNodeStatBucket::Flat, 4.0f)}, nullptr);
    Check(TEXT("Core.Constitution.ThirdLayer"), TEXT("Third Layer"), 2, 1, {}, TEXT("Progression.Node.Core.Constitution.ThirdLayer"));
    Check(TEXT("Core.Constitution.Endurance"), TEXT("Endurance"), 2, 1, {}, TEXT("Progression.Node.Core.Endurance"));
    Check(TEXT("Core.Ward.Resist"), TEXT("Resist"), 1, 1, {Effect(EBreakerNodeStatTarget::ElementalResistance, EBreakerNodeStatBucket::Flat, 10.0f)}, nullptr);
    Check(TEXT("Core.Ward.Tolerance"), TEXT("Tolerance"), 1, 3, {Effect(EBreakerNodeStatTarget::ElementalResistance, EBreakerNodeStatBucket::Flat, 6.0f)}, nullptr);
    Check(TEXT("Core.Ward.Refusal"), TEXT("Refusal"), 2, 1, {Effect(EBreakerNodeStatTarget::AilmentAvoidance, EBreakerNodeStatBucket::Flat, 12.0f)}, nullptr);
    Check(TEXT("Core.Ward.Clean"), TEXT("Clean"), 1, 3, {Effect(EBreakerNodeStatTarget::AilmentAvoidance, EBreakerNodeStatBucket::Flat, 5.0f)}, nullptr);
    Check(TEXT("Core.Ward.Insulation"), TEXT("Insulation"), 2, 1, {}, TEXT("Progression.Node.Core.Insulation"));
    Check(TEXT("Core.Ward.Null"), TEXT("Null"), 2, 1, {}, TEXT("Progression.Node.Core.Null"));
    Check(TEXT("Core.Recovery.Mend"), TEXT("Mend"), 1, 1, {Effect(EBreakerNodeStatTarget::HealingReceived, EBreakerNodeStatBucket::IncreasedPercent, 15.0f)}, nullptr);
    Check(TEXT("Core.Recovery.Knit"), TEXT("Knit"), 1, 3, {Effect(EBreakerNodeStatTarget::HealthRegenPercentMaxHealth, EBreakerNodeStatBucket::Flat, 0.4f)}, nullptr);
    Check(TEXT("Core.Recovery.FieldDressing"), TEXT("Field Dressing"), 2, 1, {Effect(EBreakerNodeStatTarget::HealingReceived, EBreakerNodeStatBucket::IncreasedPercent, 30.0f)}, nullptr);
    Check(TEXT("Core.Recovery.Recharge"), TEXT("Recharge"), 1, 3, {Effect(EBreakerNodeStatTarget::ShieldRechargeDelayReduction, EBreakerNodeStatBucket::Flat, 0.4f)}, nullptr);
    Check(TEXT("Core.Recovery.Overheal"), TEXT("Overheal"), 2, 1, {}, TEXT("Progression.Node.Core.Recovery.Overheal"));
    Check(TEXT("Core.Recovery.SecondLife"), TEXT("Second Life"), 2, 1, {}, TEXT("Progression.Node.Core.SecondLife"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRosterDefenceRuntimeTest,"RiorsEdge.Progression.CoreRoster.DefenceRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRosterDefenceRuntimeTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto* Player=World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attr=Player->GetAttributes(); auto* ASC=Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr);
    auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr);
    auto* Combat=Player->GetCombat(); Combat->BeginPlay(); Combat->SetComponentTickEnabled(false);
    auto* Class=NewObject<UBreakerClassDefinition>(); Class->ClassId=EBreakerClassId::Caster;
    TArray<FBreakerCoreWedgeDefinition> Wedges; BreakerCoreRoster::AppendDefence(Class,Wedges);
    FString Error; auto* Tree=BreakerCoreTree::Build(Class,TEXT("Test.Core.Defence.Runtime"),FText::FromString(TEXT("Defence")),Wedges,Error);
    if (!Tree) return false; Class->BranchTrees.Add(Tree);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(30,Progression->ExperienceCurve));
    FText Reason;
    auto Buy=[&](const TCHAR* Id) { return TestTrue(Id,Progression->PurchaseNode(Tree,FName(Id),Reason)); };
    for (const TCHAR* Id : {TEXT("Core.Bulwark.Read"),TEXT("Core.Bulwark.Guard"),TEXT("Core.Bulwark.Parry")})
        if (!Buy(Id)) return false;
    TestEqual(TEXT("Authored Read and rank-one Guard give eight block percentage points"),Progression->GetNodeStats().BlockChanceBonus,.08f,.001f);
    if (!TestTrue(TEXT("Actual Parry purchase enables input"),Combat->TryParry())) return false;
    TestEqual(TEXT("New Read cannot inherit old parry-window bonus"),Combat->GetParryWindowRemaining(),.25f,.001f);
    TestEqual(TEXT("Native authored cooldown"),Combat->GetParryCooldownRemaining(),2.f,.001f);
    for (const TCHAR* Id : {TEXT("Core.Bulwark.Evade"),TEXT("Core.Bulwark.Counterweight"),TEXT("Core.Bulwark.Anticipate"),TEXT("Core.Bulwark.Interpose"),TEXT("Core.Bulwark.Riposte"),TEXT("Core.Bulwark.Wall")})
        if (!Buy(Id)) return false;
    TestEqual(TEXT("Purchase preserves running parry clock"),Combat->GetParryWindowRemaining(),.25f,.001f);
    for (int32 I=0;I<220;++I) {++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}
    if (!TestTrue(TEXT("Next accepted parry uses Counterweight"),Combat->TryParry())) return false;
    TestEqual(TEXT("Counterweight seconds are not percentages"),Combat->GetParryWindowRemaining(),.35f,.001f);
    TestEqual(TEXT("Anticipate follows flat cooldown reduction"),Combat->GetParryCooldownRemaining(),1.5f/1.08f,.001f);
    const float MaxHealth=Attr->GetMaxHealth(); Attr->ApplyHealth(MaxHealth*.5f);
    Combat->RefreshCoreFrontShieldCapacity();
    TestEqual(TEXT("Interpose adds capacity without free fill"),Combat->GetFrontShield(),0.f,.001f);
    FBreakerDamageRequest Hit; Hit.BaseDamage=20; Hit.bCanCritical=false; Hit.bCanBeAvoided=false;
    Hit.SetInstigator(World->SpawnActor<AActor>()); Hit.bHasSourceLocation=true;
    Hit.SourceLocation=Player->GetActorLocation()+Player->GetActorForwardVector()*100;
    TestTrue(TEXT("Native front hit is parried"),Combat->ReceiveDamage(Hit).bParried);
    TestEqual(TEXT("Authored Riposte pays eight percent physical health"),Attr->GetHealth(),MaxHealth*.58f,.001f);
    TestEqual(TEXT("Authored Wall fills purchased Interpose pool"),Combat->GetFrontShield(),MaxHealth*.06f,.001f);
    const auto Cost = BreakerCoreRespecCost(Progression->GetCharacterLevel());
    Player->GetEquipment()->GrantForgeCurrency(Cost.Amount + 1);
    if (!TestTrue(TEXT("Actual earned-point respec"),Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec debits its exact price"), Player->GetEquipment()->GetForgeWallet().Get(), 1);
    Combat->RefreshCoreFrontShieldCapacity();
    TestFalse(TEXT("Respec removes Parry input"),Combat->TryParry());
    TestEqual(TEXT("Respec removes Core front capacity"),Combat->GetFrontShield(),0.f,.001f);
    return true;
}
#endif
