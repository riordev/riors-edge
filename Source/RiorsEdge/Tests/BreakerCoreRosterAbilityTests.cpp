#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerProjectileBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
UBreakerProgressionTree* AbilityCandidate(UObject* Outer, FString& Error)
{
    TArray<FBreakerCoreWedgeDefinition> Wedges;
    BreakerCoreRoster::AppendAbility(Outer, Wedges);
    return BreakerCoreTree::Build(Outer, TEXT("Test.CoreRoster.Ability"), FText::FromString(TEXT("Ability candidate")), Wedges, Error);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRosterAbilityAuthoringTest, "RiorsEdge.Progression.CoreRoster.AbilityAuthoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRosterAbilityAuthoringTest::RunTest(const FString& Parameters)
{
    FString Error; auto* Tree = AbilityCandidate(GetTransientPackage(), Error);
    if (!TestNotNull(*Error, Tree)) return false;
    TestEqual(TEXT("Four Ability wedges"), Tree->CoreWedgeOrder.Num(), 4);
    TestEqual(TEXT("Exact Ability node count"), Tree->Nodes.Num(), 34);
    int32 Offered = 0, EffectCount = 0;
    TSet<FName> Ids;
    for (const UBreakerProgressionNode* N : Tree->Nodes)
    {
        Offered += N->MaxRank * N->CostPerRank; EffectCount += N->Effects.Num();
        TestFalse(TEXT("No duplicate identity"), Ids.Contains(N->NodeId)); Ids.Add(N->NodeId);
        TestTrue(TEXT("Every node has real effects or rules"), !N->Effects.IsEmpty() || !N->GrantedTags.IsEmpty());
        TestEqual(TEXT("Explicit Ability sector"), Tree->CoreWedgeSectors.FindRef(N->Constellation), FName(TEXT("Ability")));
        if (N->CoreRole == EBreakerCoreNodeRole::LaneMinor) TestEqual(TEXT("Ranked lane"), N->MaxRank, 3);
        if (N->CoreRole == EBreakerCoreNodeRole::Keystone) TestEqual(TEXT("Local keystone gate"), N->RequiredConstellationInvestment, 18);
    }
    TestEqual(TEXT("Ability offered points"), Offered, 78);
    TestEqual(TEXT("Exact authored lane count"), EffectCount, 30);
    auto Find = [&](const TCHAR* Id) -> const UBreakerProgressionNode*
    { for (const UBreakerProgressionNode* N : Tree->Nodes) if (N->NodeId == FName(Id)) return N; AddError(FString(TEXT("Missing ")) + Id); return nullptr; };
    using T = EBreakerNodeStatTarget; using B = EBreakerNodeStatBucket;
    auto Effect = [&](const TCHAR* Id, T Target, B Bucket, float Value)
    {
        const auto* N = Find(Id); if (!N) return;
        const auto* E = N->Effects.FindByPredicate([&](const FBreakerNodeEffect& Row) { return Row.StatTarget == Target && Row.StatBucket == Bucket; });
        if (TestNotNull(Id, E)) TestEqual(Id, E->ValuePerRank, Value);
    };
    Effect(TEXT("Core.Arc.Prime"), T::AbilityDamage, B::IncreasedPercent, 8.f);
    Effect(TEXT("Core.Arc.Channel"), T::AbilityDamage, B::IncreasedPercent, 6.f);
    Effect(TEXT("Core.Arc.Widen"), T::AbilityArea, B::IncreasedPercent, 15.f);
    Effect(TEXT("Core.Arc.Vent"), T::AddedAbilityPower, B::Flat, 4.f);
    Effect(TEXT("Core.Arc.Reach"), T::AbilityDamage, B::IncreasedPercent, 25.f);
    Effect(TEXT("Core.Arc.Anchor"), T::AbilityArea, B::IncreasedPercent, 6.f);
    Effect(TEXT("Core.Arc.Persistence"), T::ZoneAndWindowDuration, B::IncreasedPercent, 20.f);
    Effect(TEXT("Core.Arc.Recycle"), T::AddedAbilityPower, B::Flat, 3.f);
    Effect(TEXT("Core.Arc.Spillover"), T::AbilityArea, B::IncreasedPercent, 8.f);
    Effect(TEXT("Core.Arc.Overflow"), T::AbilityDamage, B::MorePercent, 26.f);
    Effect(TEXT("Core.Tempo.Metronome"), T::AbilityCastRate, B::IncreasedPercent, 6.f);
    Effect(TEXT("Core.Tempo.Quicken"), T::AbilityCastRate, B::IncreasedPercent, 4.f);
    Effect(TEXT("Core.Tempo.Reset"), T::AbilityDamage, B::IncreasedPercent, 15.f);
    Effect(TEXT("Core.Tempo.Recovery"), T::AbilityCooldown, B::IncreasedPercent, 5.f);
    Effect(TEXT("Core.Tempo.SecondWind"), T::AbilityCost, B::IncreasedPercent, 12.f);
    Effect(TEXT("Core.Tempo.Flow"), T::AbilityChannelRate, B::IncreasedPercent, 4.f);
    Effect(TEXT("Core.Tempo.Cascade"), T::AbilityCooldown, B::IncreasedPercent, 12.f);
    Effect(TEXT("Core.Tempo.Cascade"), T::AbilityCastRate, B::IncreasedPercent, 12.f);
    Effect(TEXT("Core.Tempo.Prep"), T::AbilityCost, B::IncreasedPercent, 8.f);
    Effect(TEXT("Core.Tempo.FollowThrough"), T::AbilityCastRate, B::IncreasedPercent, 5.f);
    Effect(TEXT("Core.Reservoir.Capacity"), T::MaxClassResource, B::IncreasedPercent, 10.f);
    Effect(TEXT("Core.Reservoir.Draw"), T::ClassResourceGeneration, B::IncreasedPercent, 5.f);
    Effect(TEXT("Core.Reservoir.DeepPockets"), T::MaxClassResource, B::IncreasedPercent, 20.f);
    Effect(TEXT("Core.Reservoir.Tithe"), T::AbilityCost, B::IncreasedPercent, 5.f);
    Effect(TEXT("Core.Reservoir.Wellspring"), T::ClassResourceGeneration, B::IncreasedPercent, 15.f);
    Effect(TEXT("Core.Reservoir.Wellspring"), T::ClassResourceRegen, B::Flat, 1.f);
    Effect(TEXT("Core.Duration.Hold"), T::AbilityDuration, B::IncreasedPercent, 10.f);
    Effect(TEXT("Core.Duration.Extend"), T::AbilityDuration, B::IncreasedPercent, 6.f);
    Effect(TEXT("Core.Duration.Uptime"), T::BuffAndWindowDuration, B::IncreasedPercent, 15.f);
    Effect(TEXT("Core.Duration.Settle"), T::AbilityDamage, B::IncreasedPercent, 5.f);
    const TCHAR* RuleIds[] = {TEXT("Core.Arc.Detonation"), TEXT("Core.Tempo.Overclock"), TEXT("Core.Tempo.Conduction"), TEXT("Core.Reservoir.SecondShift"), TEXT("Core.Duration.Standing"), TEXT("Core.Duration.Afterimage")};
    const TCHAR* Tags[] = {TEXT("Progression.Node.Core.Detonation"), TEXT("Progression.Node.Core.Overclock"), TEXT("Progression.Node.Core.Conduction"), TEXT("Progression.Node.Core.SecondShift"), TEXT("Progression.Node.Core.Standing"), TEXT("Progression.Node.Core.Afterimage")};
    for (int32 I = 0; I < UE_ARRAY_COUNT(RuleIds); ++I)
        if (const auto* N = Find(RuleIds[I])) TestTrue(RuleIds[I], N->GrantedTags.HasTagExact(FGameplayTag::RequestGameplayTag(Tags[I])));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRosterAbilityRuntimeTest, "RiorsEdge.Progression.CoreRoster.AbilityPurchasedRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRosterAbilityRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    Player->bRefuseSavesForPendingCharacter = true;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attr = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr); Player->GetCombat()->SetComponentTickEnabled(false);
    auto* P = Player->GetProgression(); P->BindAttributes(Attr);
    if (!TestTrue(TEXT("Actual Caster class selected"), P->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(P->ClassDefinition, Player);
    // Candidate IDs deliberately match the replacement roster. The collector
    // searches class trees before global fallbacks, so remove the copied live
    // Core definition to make this candidate the authoritative fixture schema.
    // Retain Doctrine and the actual registered class kit/loadout.
    Definition->BranchTrees.RemoveAll([](const TObjectPtr<UBreakerProgressionTree>& Existing)
    { return Existing && Existing->Currency == EBreakerPointCurrency::CorePoints; });
    FString Error; auto* Tree = AbilityCandidate(Definition, Error); if (!TestNotNull(*Error, Tree)) return false;
    Definition->BranchTrees.Add(Tree); P->ClassDefinition = Definition;
    P->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(10, P->ExperienceCurve));
    FText Reason;
    if (!TestTrue(TEXT("Fracture unlocked with earned token"), P->SpendAbilityToken(TEXT("Caster.Fracture"), Reason))) return false;
    if (!TestTrue(TEXT("Fracture equipped through actual loadout"), Player->GetAbilities()->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityOne, TEXT("Caster.Fracture"), Reason))) return false;
    auto* Mana = Player->GetMana(); Mana->BindAttributes(Attr); Mana->SetComponentTickEnabled(false); Mana->AdvanceLoop(30);
    auto Tick = [&](float Seconds) { for (float Elapsed = 0; Elapsed < Seconds; Elapsed += .001f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .001f); } };
    auto Shots = [&]() { int32 Count = 0; for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) if (It->GetOwner() == Player && !It->IsActorBeingDestroyed()) ++Count; return Count; };
    const float BaseRate = UBreakerGameplayAbility::AbilityCastRateMultiplierFor(Player);
    const int32 Budget = P->GetUnspentPoints(Tree->Currency);
    if (!TestTrue(TEXT("Earned gateway purchase"), P->PurchaseNode(Tree, TEXT("Core.Tempo.Metronome"), Reason))) return false;
    for (int32 Rank = 0; Rank < 3; ++Rank)
        if (!TestTrue(TEXT("Actual ranked Quicken purchase"), P->PurchaseNode(Tree, TEXT("Core.Tempo.Quicken"), Reason))) return false;
    TestEqual(TEXT("Four points debited"), P->GetUnspentPoints(Tree->Currency), Budget - 4);
    const float Rate = UBreakerGameplayAbility::AbilityCastRateMultiplierFor(Player);
    TestEqual(TEXT("Gateway plus three ranks reaches cast consumer"), Rate, BaseRate + .18f, .0001f);
    const float Duration = GetDefault<UBreakerAbility_Fracture>()->BaseCastSeconds / Rate;
    const float BeforeMana = Mana->GetMana();
    if (!TestTrue(TEXT("Equipped Fracture cast starts"), Player->GetAbilities()->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne))) return false;
    TestTrue(TEXT("Native resource payment"), Mana->GetMana() < BeforeMana);
    Tick(Duration - .005f); TestEqual(TEXT("No premature projectile"), Shots(), 0);
    Tick(.01f); TestEqual(TEXT("Actual ranked tempo completes emission"), Shots(), 1);
    if (!TestTrue(TEXT("Native Core respec"), P->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec restores cast consumer"), UBreakerGameplayAbility::AbilityCastRateMultiplierFor(Player), BaseRate, .0001f);
    TestEqual(TEXT("Respec refunds earned points"), P->GetUnspentPoints(Tree->Currency), Budget);
    return true;
}
#endif
