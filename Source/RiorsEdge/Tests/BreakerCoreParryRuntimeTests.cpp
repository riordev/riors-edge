#include "Tests/BreakerCoreParryRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "UI/BreakerSkillProjection.h"

void UBreakerCoreParryRuntimeObserver::OnHealed(const FBreakerHealResult& Result)
{
    ++HealingEvents;
    if (bReenter && Combat)
    {
        bReenter = false;
        bReentryParried = Combat->ReceiveDamage(Reentry).bParried;
    }
}

static_assert(static_cast<uint8>(EBreakerNodeStatTarget::ParryWindowAddedSeconds) == 75);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::ParryCooldownReductionSeconds) == 76);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::ParryCooldownRecovery) == 77);

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreParryRuntimeTest, "RiorsEdge.Progression.CoreParryRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreParryRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    auto* Attacker = World->SpawnActor<AActor>();
    if (!Player || !Attacker) return false;
    Player->SetActorTickEnabled(false);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attr = Player->GetAttributes();
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    auto* Combat = Player->GetCombat();
    auto* Progression = Player->GetProgression();
    Progression->BindAttributes(Attr);
    // Begin only the combat component to install its real progression callback;
    // Character BeginPlay would load owner saves unrelated to this fixture.
    Combat->BeginPlay();
    Combat->SetComponentTickEnabled(false);
    auto* Tree = NewObject<UBreakerProgressionTree>();
    Tree->TreeId = TEXT("Test.Core.Parry.Schema"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    for (int32 Index = 0; Index < 5; ++Index)
    {
        auto* Node = NewObject<UBreakerProgressionNode>(Tree);
        Node->NodeId = FName(*FString::Printf(TEXT("Test.Core.Parry.%d"), Index)); Node->Currency = Tree->Currency;
        Tree->Nodes.Add(Node);
    }
    Tree->Nodes[0]->GrantedTags.AddTag(BreakerNodeTags::Verb_Parry.GetTag());
    auto Add = [&](int32 Index, EBreakerNodeStatTarget Target, float Value, EBreakerNodeStatBucket Bucket)
    {
        FBreakerNodeEffect Effect; Effect.StatTarget = Target; Effect.ValuePerRank = Value; Effect.StatBucket = Bucket;
        Tree->Nodes[Index]->Effects.Add(Effect);
    };
    Add(1, EBreakerNodeStatTarget::ParryWindowAddedSeconds, .1f, EBreakerNodeStatBucket::Flat);
    Add(1, EBreakerNodeStatTarget::ParryCooldownReductionSeconds, .5f, EBreakerNodeStatBucket::Flat);
    Add(1, EBreakerNodeStatTarget::ParryCooldownRecovery, 8, EBreakerNodeStatBucket::IncreasedPercent);
    Add(1, EBreakerNodeStatTarget::HealingReceived, 50, EBreakerNodeStatBucket::IncreasedPercent);
    Tree->Nodes[2]->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Bulwark.Riposte")));
    Tree->Nodes[3]->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Bulwark.PerfectGuard")));
    Add(4, EBreakerNodeStatTarget::DodgeChance, 100, EBreakerNodeStatBucket::Flat);
    Add(4, EBreakerNodeStatTarget::BlockChance, 100, EBreakerNodeStatBucket::Flat);
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster; Class->BranchTrees.Add(Tree);
    if (!TestTrue(TEXT("Temporary primitive class selected"), Progression->ChoosePermanentClass(Class))) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(6, Progression->ExperienceCurve));
    FText Reason;
    auto Purchase = [&](int32 Index)
    { return TestTrue(TEXT("Level-earned Core purchase"), Progression->PurchaseNode(Tree, Tree->Nodes[Index]->NodeId, Reason)); };
    auto Advance = [&](int32 Frames)
    { for (int32 Frame = 0; Frame < Frames; ++Frame) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); } };
    UFunction* ParryRPC = Player->FindFunction(TEXT("ServerParry"));
    if (!TestNotNull(TEXT("Native character input RPC handler exists"), ParryRPC)) return false;
    auto Press = [&]() { Player->ProcessEvent(ParryRPC, nullptr); };
    Press(); TestFalse(TEXT("Native handler refuses unowned parry"), Combat->IsParryActive());
    if (!Purchase(0)) return false;
    Press();
    TestEqual(TEXT("Owned base parry opens through character RPC handler"), Combat->GetParryWindowRemaining(), .25f, .001f);
    TestEqual(TEXT("Base parry cooldown"), Combat->GetParryCooldownRemaining(), 2.0f, .001f);
    if (!Purchase(1)) return false;
    TestEqual(TEXT("Numeric purchase cannot extend an existing window"), Combat->GetParryWindowRemaining(), .25f, .001f);
    TestEqual(TEXT("Numeric purchase cannot recover existing cooldown"), Combat->GetParryCooldownRemaining(), 2.0f, .001f);
    Advance(6); Press();
    TestFalse(TEXT("Whiff ends window and repeated input still pays cooldown"), Combat->IsParryActive());
    Advance(35); Press();
    TestEqual(TEXT("Next input snapshots extra window"), Combat->GetParryWindowRemaining(), .35f, .001f);
    TestEqual(TEXT("Flat cooldown subtraction precedes recovery divisor"), Combat->GetParryCooldownRemaining(), 1.5f / 1.08f, .001f);
    if (!Purchase(2)) return false;
    FBreakerDamageRequest Hit;
    Hit.SetInstigator(Attacker); Hit.BaseDamage = 10; Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Hit.bCanCritical = false; Hit.bCanBeAvoided = false; Hit.bBypassShield = true;
    Hit.bHasSourceLocation = true; Hit.SourceLocation = Player->GetActorLocation() + FVector(100, 0, 0);
    auto* Observer = NewObject<UBreakerCoreParryRuntimeObserver>(Player);
    Observer->Combat = Combat; Observer->Reentry = Hit;
    Combat->OnHealed.AddDynamic(Observer, &UBreakerCoreParryRuntimeObserver::OnHealed);
    FBreakerDamageRequest Ineligible = Hit;
    Ineligible.bIsDamageOverTime = true;
    TestFalse(TEXT("Initial DoT cannot earn parry"), Combat->ReceiveDamage(Ineligible).bParried);
    Ineligible = Hit; Ineligible.SourceLocation = Player->GetActorLocation() - FVector(100, 0, 0);
    TestFalse(TEXT("Initial rear hit cannot earn parry"), Combat->ReceiveDamage(Ineligible).bParried);
    Ineligible = Hit; Ineligible.SetInstigator(Player);
    TestFalse(TEXT("Self damage cannot earn parry"), Combat->ReceiveDamage(Ineligible).bParried);
    Ineligible = Hit; Ineligible.BaseDamage = 0;
    Combat->ReceiveDamage(Ineligible);
    TestEqual(TEXT("Ineligible hits grant no Riposte"), Observer->HealingEvents, 0);
    // Leave enough missing health to observe the full eight-percent heal.
    Attr->ApplyHealth(Attr->GetMaxHealth() * .5f);
    const float HealBefore = Attr->GetHealth();
    Observer->bReenter = true;
    TestTrue(TEXT("Eligible frontal hit earns Riposte"), Combat->ReceiveDamage(Hit).bParried);
    TestEqual(TEXT("Riposte heals once through composed healing, callback hit still deals damage"),
        Attr->GetHealth() - HealBefore, Attr->GetMaxHealth() * .08f * 1.5f - 10, .001f);
    TestEqual(TEXT("Healing callback cannot reearn consumed parry"), Observer->HealingEvents, 1);
    TestFalse(TEXT("Ordinary parry does not protect callback hit"), Observer->bReentryParried);
    if (!Purchase(3) || !Purchase(4)) return false;
    const auto GuardSnapshot = BreakerSkillProjection::MakeSnapshot(Progression, Attr);
    const auto GuardRows = BreakerSkillProjection::CurrentTotals(GuardSnapshot);
    const auto WithoutGuardRows = BreakerSkillProjection::ProjectPurchase(GuardSnapshot, Tree->Nodes[3]->NodeId, -1);
    for (const TCHAR* Label : { TEXT("DODGE CHANCE"), TEXT("BLOCK CHANCE") })
    {
        const auto* Current = GuardRows.FindByPredicate([Label](const FBreakerStatLine& Row) { return Row.Label == Label; });
        const auto* Removed = WithoutGuardRows.FindByPredicate([Label](const FBreakerStatLine& Row) { return Row.Label == Label; });
        if (TestNotNull(TEXT("Actual passive defence readout exists"), Current))
            TestEqual(TEXT("Current readout includes Perfect Guard forfeit"), Current->Before, 0.0f);
        if (TestNotNull(TEXT("Removing rule preview exists"), Removed))
        {
            TestEqual(TEXT("Removal preview starts at forfeited chance"), Removed->Before, 0.0f);
            TestEqual(TEXT("Removal preview restores owned passive bonus"), Removed->After, 1.0f);
        }
    }
    Ineligible = Hit; Ineligible.bHasSourceLocation = false; Ineligible.bCanBeAvoided = true;
    const auto Outside = Combat->ReceiveDamage(Ineligible);
    TestFalse(TEXT("Perfect Guard forfeits purchased dodge outside guard"), Outside.bDodged);
    TestFalse(TEXT("Perfect Guard forfeits purchased block outside guard"), Outside.bBlocked);
    TestEqual(TEXT("Outside guard actual damage is not avoided"), Outside.HealthDamage, 10.0f, .001f);
    Advance(30); Press();
    Attr->ApplyHealth(Attr->GetMaxHealth() * .5f);
    const float GuardHealBefore = Attr->GetHealth();
    Observer->bReenter = true;
    TestTrue(TEXT("First valid parry opens Perfect Guard"), Combat->ReceiveDamage(Hit).bParried);
    TestTrue(TEXT("Guard lease is active"), Combat->IsPerfectGuardActive());
    TestTrue(TEXT("Healing callback is already protected by guard"), Observer->bReentryParried);
    TestEqual(TEXT("Only initial success pays Riposte"), Observer->HealingEvents, 2);
    TestEqual(TEXT("Perfect Guard Riposte amplifies exactly once"), Attr->GetHealth() - GuardHealBefore,
        Attr->GetMaxHealth() * .08f * 1.5f, .001f);
    const float GuardHealth = Attr->GetHealth();
    Ineligible = Hit; Ineligible.SourceLocation = Player->GetActorLocation() - FVector(100, 0, 0);
    TestTrue(TEXT("Follow-up rear hit is protected"), Combat->ReceiveDamage(Ineligible).bParried);
    Ineligible.bIsDamageOverTime = true;
    TestTrue(TEXT("Follow-up DoT is protected"), Combat->ReceiveDamage(Ineligible).bParried);
    Ineligible.bHasSourceLocation = false;
    TestTrue(TEXT("Follow-up unlocated hostile damage is protected"), Combat->ReceiveDamage(Ineligible).bParried);
    TestEqual(TEXT("Follow-up guard hits neither heal nor damage"), Attr->GetHealth(), GuardHealth);
    TestEqual(TEXT("Follow-up guard hits produce no extra healing events"), Observer->HealingEvents, 2);
    Ineligible = Hit; Ineligible.SetInstigator(Player);
    TestEqual(TEXT("Mandatory self damage still pays through guard"), Combat->ReceiveDamage(Ineligible).HealthDamage, 10.0f, .001f);
    Advance(18);
    TestTrue(TEXT("Guard still protects inside its one-second lease"), Combat->ReceiveDamage(Hit).bParried);
    Advance(3);
    TestFalse(TEXT("One-second guard expires without refreshing from follow-up hits"), Combat->IsPerfectGuardActive());
    TestEqual(TEXT("Expired guard restores real incoming damage"), Combat->ReceiveDamage(Hit).HealthDamage, 10.0f, .001f);
    Advance(10); Press(); Combat->ReceiveDamage(Hit);
    TestTrue(TEXT("Fresh success can open a new guard"), Combat->IsPerfectGuardActive());
    if (!TestTrue(TEXT("Real respec during guard"), Progression->RespecCore(Reason))) return false;
    TestFalse(TEXT("Respec immediately revokes guard"), Combat->IsPerfectGuardActive());
    if (!Purchase(0) || !Purchase(3)) return false;
    TestFalse(TEXT("Rebuy does not restore old guard lease"), Combat->IsPerfectGuardActive());
    Press(); Combat->ReceiveDamage(Hit);
    Ineligible = Hit; Ineligible.SetInstigator(Player); Ineligible.BaseDamage = Attr->GetMaxHealth() * 100;
    Combat->ReceiveDamage(Ineligible);
    TestTrue(TEXT("Actual self damage can kill guarded owner"), Combat->IsDead());
    TestFalse(TEXT("Death clears guard"), Combat->IsPerfectGuardActive());
    Combat->RestoreVitals();
    TestFalse(TEXT("Revive does not restore old guard"), Combat->IsPerfectGuardActive());
    Press();
    TestEqual(TEXT("Respec removed numeric window"), Combat->GetParryWindowRemaining(), .25f, .001f);
    TestEqual(TEXT("Respec removed numeric cooldown"), Combat->GetParryCooldownRemaining(), 2.0f, .001f);
    return true;
}
#endif
