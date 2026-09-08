#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Tests/BreakerCoreParryRuntimeObserver.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreOverhealRuntimeTest, "RiorsEdge.Progression.CoreOverhealRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreOverhealRuntimeTest::RunTest(const FString& Parameters)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    auto* Attacker = World->SpawnActor<AActor>();
    if (!Player || !Attacker) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attr = Player->GetAttributes(); auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    auto* Progression = Player->GetProgression(); auto* Combat = Player->GetCombat();
    Progression->BindAttributes(Attr); Combat->BeginPlay(); Combat->SetComponentTickEnabled(false);
    auto* Tree = NewObject<UBreakerProgressionTree>(); Tree->TreeId = TEXT("Test.Core.Overheal"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Rule = NewObject<UBreakerProgressionNode>(Tree); Rule->Currency = Tree->Currency; Rule->NodeId = TEXT("Test.Core.Overheal.Rule");
    Rule->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Recovery.Overheal")));
    Tree->Nodes.Add(Rule);
    auto* Healing = NewObject<UBreakerProgressionNode>(Tree); Healing->Currency = Tree->Currency; Healing->NodeId = TEXT("Test.Core.Overheal.Healing");
    FBreakerNodeEffect Effect; Effect.StatTarget = EBreakerNodeStatTarget::HealingReceived;
    Effect.StatBucket = EBreakerNodeStatBucket::IncreasedPercent; Effect.ValuePerRank = 30;
    Healing->Effects.Add(Effect); Tree->Nodes.Add(Healing);
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster; Class->BranchTrees.Add(Tree);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(3, Progression->ExperienceCurve));
    FText Reason;
    Attr->ApplyHealth(Attr->GetMaxHealth());
    TestEqual(TEXT("Unowned overheal cannot create a shield"), Combat->ApplyHealingAmount(10, Player, FGameplayTag()).ShieldGranted, 0.0f);
    if (!TestTrue(TEXT("Earned point buys Overheal"), Progression->PurchaseNode(Tree, Rule->NodeId, Reason))) return false;
    const float CoreCap = Attr->GetMaxHealth() * UBreakerCombatComponent::CoreOverhealHealthFraction;
    TestEqual(TEXT("Purchase immediately establishes fifteen-percent physical-health floor"), Attr->GetMaxShield(), CoreCap, .001f);
    TestEqual(TEXT("Capacity purchase grants no shield"), Attr->GetShield(), 0.0f);
    auto* Observer = NewObject<UBreakerCoreParryRuntimeObserver>(); Observer->Combat = Combat;
    Combat->OnHealed.AddDynamic(Observer, &UBreakerCoreParryRuntimeObserver::OnHealed);
    Attr->ApplyHealth(Attr->GetMaxHealth() - 5);
    const auto Partial = Combat->ApplyHealingAmount(10, Player, FGameplayTag());
    TestEqual(TEXT("Healing fills physical missing health first"), Partial.HealthHealed, 5.0f, .001f);
    TestEqual(TEXT("Only actual excess converts"), Partial.ShieldGranted, 5.0f, .001f);
    TestEqual(TEXT("Conversion emits one heal event"), Observer->HealingEvents, 1);
    const auto Capped = Combat->ApplyHealingAmount(100, Player, FGameplayTag());
    TestEqual(TEXT("Existing shield consumes cap headroom"), Capped.ShieldGranted, CoreCap - 5, .001f);
    TestEqual(TEXT("Repeated overheal cannot exceed its cap"), Attr->GetShield(), CoreCap, .001f);
    TestEqual(TEXT("A full shield cannot pay again"), Combat->ApplyHealingAmount(100, Player, FGameplayTag()).ShieldGranted, 0.0f);

    if (!TestTrue(TEXT("Second earned point buys healing received"), Progression->PurchaseNode(Tree, Healing->NodeId, Reason))) return false;
    Attr->ApplyShield(0); Attr->ApplyHealth(Attr->GetMaxHealth() - 1);
    const int32 EventsBefore = Observer->HealingEvents;
    const auto Amplified = Combat->ApplyHealingAmount(10, Player, FGameplayTag());
    TestEqual(TEXT("Healing modifier applies once before excess is computed"), Amplified.RequestedAmount, 13.0f, .001f);
    TestEqual(TEXT("Amplified excess is not amplified a second time"), Amplified.ShieldGranted, 12.0f, .001f);
    TestEqual(TEXT("Amplified conversion still has one event"), Observer->HealingEvents, EventsBefore + 1);

    Attr->ApplyShield(0); Attr->ApplyHealth(Attr->GetMaxHealth() * .5f);
    FBreakerHealRequest Conversion; Conversion.Amount = 10; Conversion.bConversionOnly = true;
    Conversion.bOverhealToShield = true; Conversion.SetHealer(Player); Conversion.ProcCoefficient = 0;
    const float PhysicalBefore = Attr->GetHealth();
    const auto Converted = Combat->ApplyHealing(Conversion);
    TestEqual(TEXT("Already-earned conversion cannot heal another missing pool"), Attr->GetHealth(), PhysicalBefore);
    TestEqual(TEXT("Already-earned conversion ignores healing amplification"), Converted.ShieldGranted, 10.0f, .001f);
    Attr->ApplyShield(0); Attr->ApplyHealth(Attr->GetMaxHealth());
    Observer->Reentry.BaseDamage = 2; Observer->Reentry.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Observer->Reentry.bCanCritical = false; Observer->Reentry.bCanBeAvoided = false; Observer->Reentry.bBypassShield = true;
    Observer->Reentry.SetInstigator(Attacker); Observer->bReenter = true;
    const int32 ReentryEvents = Observer->HealingEvents;
    Combat->ApplyHealingAmount(10, Player, FGameplayTag());
    TestEqual(TEXT("Damage from callback is not healed by a second conversion pass"), Attr->GetHealth(), Attr->GetMaxHealth() - 2, .001f);
    TestEqual(TEXT("Callback reentry cannot multiply heal dispatch"), Observer->HealingEvents, ReentryEvents + 1);

    auto* Gear = Player->GetEquipment(); Gear->BindAttributes(Attr);
    FBreakerItemInstance Body;
    for (int32 Seed = 1; Seed <= 100; ++Seed)
    {
        Body = UBreakerLootLibrary::RollItem(TEXT("Test.Core.Overheal.Body"), EBreakerEquipSlot::BodyArmour, EBreakerItemRarity::Standard, 50, Seed);
        if (Body.ArmourArchetype == EBreakerArmourArchetype::Shield) break;
    }
    if (!TestTrue(TEXT("Actual shield armour equips"), Body.ArmourArchetype == EBreakerArmourArchetype::Shield && Gear->EquipItem(Body))) return false;
    const float GearCap = Gear->GetStats().BaseShieldFromGear;
    const float GearCoreCap = Attr->GetMaxHealth() * UBreakerCombatComponent::CoreOverhealHealthFraction;
    if (!TestTrue(TEXT("Actual gear exercises a larger ward than Core conversion cap"), GearCap > GearCoreCap)) return false;
    TestEqual(TEXT("Overheal floor never adds on top of a larger ward"), Attr->GetMaxShield(), GearCap, .001f);
    Attr->ApplyShield(GearCoreCap - 2); Attr->ApplyHealth(Attr->GetMaxHealth());
    TestEqual(TEXT("Larger gear capacity does not enlarge Core conversion"), Combat->ApplyHealingAmount(1000, Player, FGameplayTag()).ShieldGranted, 2.0f, .001f);
    FBreakerHealRequest Explicit; Explicit.Amount = 1000; Explicit.bOverhealToShield = true; Explicit.SetHealer(Player);
    const auto Larger = Combat->ApplyHealing(Explicit);
    TestEqual(TEXT("Explicit existing conversion retains its larger gear cap"), Larger.ShieldGranted, GearCap - GearCoreCap, .001f);
    TestEqual(TEXT("Explicit and Core conversion share one payout"), Attr->GetShield(), GearCap, .001f);
    if (!TestTrue(TEXT("Respec succeeds with equipped shield armour"), Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec retains independent gear capacity"), Attr->GetMaxShield(), GearCap, .001f);
    TestEqual(TEXT("Respec retains shield that fits gear"), Attr->GetShield(), GearCap, .001f);
    if (!Gear->UnequipSlot(EBreakerEquipSlot::BodyArmour)) return false;
    if (!Progression->PurchaseNode(Tree, Rule->NodeId, Reason)) return false;
    TestEqual(TEXT("Repurchase does not fill a newly established floor"), Attr->GetShield(), 0.0f);
    Attr->ApplyHealth(Attr->GetMaxHealth()); Combat->ApplyHealingAmount(100, Player, FGameplayTag());
    if (!Progression->RespecCore(Reason)) return false;
    TestEqual(TEXT("Sole capacity floor disappears immediately on respec"), Attr->GetMaxShield(), 0.0f);
    TestEqual(TEXT("Removing sole capacity clamps current shield"), Attr->GetShield(), 0.0f);
    return true;
}
#endif
