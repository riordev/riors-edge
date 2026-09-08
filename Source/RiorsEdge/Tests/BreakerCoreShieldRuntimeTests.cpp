#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Tests/BreakerCoreParryRuntimeObserver.h"
#include "UI/BreakerSkillProjection.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerShieldCapacityInputsTest, "RiorsEdge.Attributes.ShieldCapacityInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerShieldCapacityInputsTest::RunTest(const FString& Parameters)
{
    auto* Attr = NewObject<UBreakerAttributeSet>();
    Attr->ApplyMaxHealth(100);
    Attr->ApplyMaxShield(100); Attr->ApplyShield(80); Attr->ApplyMaxShield(100);
    TestEqual(TEXT("Native capacity setter replaces rather than adds"), Attr->GetMaxShield(), 100.0f);
    TestEqual(TEXT("Repeated native setter does not refill"), Attr->GetShield(), 80.0f);
    Attr->ApplyMaxShield(0);
    TestEqual(TEXT("Native capacity removal clamps current"), Attr->GetShield(), 0.0f);
    Attr->SetEquipmentShieldCapacity(20); Attr->SetCoreShieldHealthFraction(.12f);
    TestEqual(TEXT("Gear and Core add from zero native base"), Attr->GetMaxShield(), 32.0f);
    Attr->SetTankShieldHealthFloor(.25f); Attr->SetSupportShieldHealthFloor(0);
    TestEqual(TEXT("Inactive other class cannot remove tank floor or permanent capacity"), Attr->GetMaxShield(), 32.0f);
    Attr->SetTemporaryShieldHealthFraction(.1f);
    TestEqual(TEXT("Temporary headroom adds above permanent capacity"), Attr->GetMaxShield(), 42.0f);
    TestEqual(TEXT("Capacity alone never fills shield"), Attr->GetShield(), 0.0f);
    Attr->ApplyShield(40); Attr->SetTemporaryShieldHealthFraction(0);
    TestEqual(TEXT("Removing headroom clamps only excess"), Attr->GetShield(), 32.0f);
    Attr->SetSupportShieldHealthFloor(.8f);
    TestEqual(TEXT("Class floor is a floor, not additive"), Attr->GetMaxShield(), 80.0f);
    TestEqual(TEXT("Larger floor grants no current shield"), Attr->GetShield(), 32.0f);
    Attr->SetSupportShieldHealthFloor(0); Attr->ApplyMaxHealth(200);
    TestEqual(TEXT("Health change recomposes class and Core fractions"), Attr->GetMaxShield(), 50.0f);
    Attr->SetTankShieldHealthFloor(0);
    TestEqual(TEXT("Removing class floor reveals permanent capacity"), Attr->GetMaxShield(), 44.0f);
    Attr->SetCoreShieldHealthFraction(0);
    TestEqual(TEXT("Removing Core retains gear alone"), Attr->GetMaxShield(), 20.0f);
    TestEqual(TEXT("Removing Core clamps existing shield"), Attr->GetShield(), 20.0f);
    Attr->SetEquipmentShieldCapacity(0);
    TestEqual(TEXT("All source withdrawals restore native zero"), Attr->GetMaxShield(), 0.0f);
    Attr->ApplyMaxHealth(200); Attr->SetCoreShieldHealthFraction(.5f); Attr->ApplyShield(90);
    Attr->BeginShieldCapacityUpdate(); Attr->ApplyMaxHealth(100); Attr->SetEquipmentShieldCapacity(50); Attr->EndShieldCapacityUpdate();
    TestEqual(TEXT("Atomic health-to-gear exchange preserves equal final capacity"), Attr->GetMaxShield(), 100.0f);
    TestEqual(TEXT("Intermediate lower health cannot destroy shield"), Attr->GetShield(), 90.0f);
    Attr->BeginShieldCapacityUpdate(); Attr->SetEquipmentShieldCapacity(0); Attr->ApplyMaxHealth(200); Attr->EndShieldCapacityUpdate();
    TestEqual(TEXT("Inverse exchange cannot destroy shield"), Attr->GetShield(), 90.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreShieldRuntimeTest, "RiorsEdge.Progression.CoreShieldRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreShieldRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    auto* Attacker = World->SpawnActor<AActor>();
    if (!Player || !Attacker) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Controller = World->SpawnActor<APlayerController>(); Controller->Possess(Player);
    Player->SetPlayerState(World->SpawnActor<APlayerState>());
    auto* Attr = Player->GetAttributes(); auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    auto* Combat = Player->GetCombat(); auto* Progression = Player->GetProgression();
    Progression->BindAttributes(Attr); Combat->BeginPlay(); Combat->SetComponentTickEnabled(false);
    auto* Tree = NewObject<UBreakerProgressionTree>(); Tree->TreeId = TEXT("Test.Core.Shield"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->Currency = Tree->Currency;
        Node->NodeId = FName(*FString::Printf(TEXT("Test.Core.Shield.%d"), Index)); Tree->Nodes.Add(Node);
    }
    auto Add = [&](int32 Index, EBreakerNodeStatTarget Target, float Value)
    { FBreakerNodeEffect Effect; Effect.StatTarget = Target; Effect.StatBucket = EBreakerNodeStatBucket::Flat; Effect.ValuePerRank = Value; Tree->Nodes[Index]->Effects.Add(Effect); };
    Add(0, EBreakerNodeStatTarget::FrontShieldPercentMaxHealth, 8);
    Tree->Nodes[1]->MaxRank = 3;
    Add(1, EBreakerNodeStatTarget::FrontShieldPercentMaxHealth, 6); Add(1, EBreakerNodeStatTarget::ShieldPercentMaxHealth, 4);
    Tree->Nodes[2]->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Constitution.ThirdLayer")));
    Tree->Nodes[3]->GrantedTags.AddTag(BreakerNodeTags::Verb_Parry.GetTag());
    Tree->Nodes[3]->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Bulwark.Wall")));
    Tree->Nodes[3]->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Bulwark.Riposte")));
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster; Class->BranchTrees.Add(Tree);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(7, Progression->ExperienceCurve));
    FText Reason;
    for (int32 Index : {0, 1, 1, 1, 2, 3})
        if (!TestTrue(TEXT("Actual level-earned capacity and rule purchase"), Progression->PurchaseNode(Tree, Tree->Nodes[Index]->NodeId, Reason))) return false;
    auto* Gear = Player->GetEquipment(); Gear->BindAttributes(Attr);
    FBreakerItemInstance Body;
    for (int32 Seed = 1; Seed <= 100; ++Seed)
    { Body = UBreakerLootLibrary::RollItem(TEXT("Test.Core.Shield.Body"), EBreakerEquipSlot::BodyArmour, EBreakerItemRarity::Standard, 1, Seed); if (Body.ArmourArchetype == EBreakerArmourArchetype::Shield) break; }
    if (!TestTrue(TEXT("Actual rolled shield armour equips"), Body.ArmourArchetype == EBreakerArmourArchetype::Shield && Gear->EquipItem(Body))) return false;
    Combat->RefreshCoreFrontShieldCapacity();
    const float CoreMax = Attr->GetMaxHealth() * .26f;
    TestEqual(TEXT("All three front ranks compose with Bulk"), Combat->GetFrontShieldMax(), CoreMax, .001f);
    TestEqual(TEXT("Layered and actual shield gear share capacity"), Attr->GetMaxShield(), Gear->GetStats().BaseShieldFromGear + Attr->GetMaxHealth() * .12f, .001f);
    TestEqual(TEXT("Purchases and gear cannot refill Core front"), Combat->GetFrontShield(), 0.0f);
    TestEqual(TEXT("Purchases and gear cannot refill ward"), Attr->GetShield(), 0.0f);
    const auto Snapshot = BreakerSkillProjection::MakeSnapshot(Progression, Attr);
    const auto Preview = BreakerSkillProjection::ProjectPurchase(Snapshot, Tree->Nodes[1]->NodeId, -1);
    for (const auto& Row : Preview)
    {
        if (Row.Label == TEXT("BASE MAX SHIELD"))
        { TestEqual(TEXT("Ward preview matches live capacity"), Row.Before, Attr->GetMaxShield(), .001f); TestEqual(TEXT("Removing Layered rank previews four percent health"), Row.Before - Row.After, Attr->GetMaxHealth() * .04f, .001f); }
        if (Row.Label == TEXT("MAX FRONT POOL"))
        { TestEqual(TEXT("Front preview matches live capacity"), Row.Before, CoreMax, .001f); TestEqual(TEXT("Removing Interpose rank previews six percent health"), Row.Before - Row.After, Attr->GetMaxHealth() * .06f, .001f); }
    }
    const float ShieldGearHealth = Attr->GetMaxHealth();
    const float ShieldGearCap = Attr->GetMaxShield();
    FBreakerItemInstance LifeBody;
    for (int32 Seed = 1; Seed <= 100; ++Seed)
    { LifeBody = UBreakerLootLibrary::RollItem(TEXT("Test.Core.Life.Body"), EBreakerEquipSlot::BodyArmour, EBreakerItemRarity::Standard, 1, Seed); if (LifeBody.ArmourArchetype == EBreakerArmourArchetype::Life) break; }
    if (!TestTrue(TEXT("Actual life armour equips for capacity exchange"), LifeBody.ArmourArchetype == EBreakerArmourArchetype::Life && Gear->EquipItem(LifeBody))) return false;
    const float LifeGearCap = Attr->GetMaxShield();
    const float IntermediateCap = ShieldGearHealth * .12f;
    const float ExchangeShield = (IntermediateCap + FMath::Min(LifeGearCap, ShieldGearCap)) * .5f;
    if (!TestTrue(TEXT("Rolled exchange exercises otherwise destructive intermediate cap"), ExchangeShield > IntermediateCap && ExchangeShield <= LifeGearCap)) return false;
    Attr->ApplyShield(ExchangeShield);
    if (!Gear->EquipItem(Body)) return false;
    TestEqual(TEXT("Actual life-to-shield swap preserves current shield"), Attr->GetShield(), ExchangeShield, .001f);
    if (!Gear->EquipItem(LifeBody)) return false;
    TestEqual(TEXT("Actual inverse armour swap preserves current shield"), Attr->GetShield(), ExchangeShield, .001f);
    if (!Gear->EquipItem(Body)) return false;
    Attr->ApplyShield(0);
    auto Advance = [&](int32 Frames) { for (int32 Frame = 0; Frame < Frames; ++Frame) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); } };
    FBreakerDamageRequest Hit; Hit.SetInstigator(Attacker); Hit.BaseDamage = 1; Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Hit.bCanBeAvoided = false; Hit.bCanCritical = false; Hit.bHasSourceLocation = true; Hit.SourceLocation = FVector(100, 0, 0);
    Combat->ReceiveDamage(Hit); Advance(60); Combat->TickComponent(1, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Incoming combat delay suppresses front recovery"), Combat->GetFrontShield(), 0.0f);
    Advance(25); Combat->TickComponent(1, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Core front uses actual quiet recovery clock and twenty-percent rate"), Combat->GetFrontShield(), CoreMax * .2f, .001f);
    const float Recovering = Combat->GetFrontShield();
    Progression->RefreshBuildConditions(); Gear->BindAttributes(Attr); Combat->RefreshCoreFrontShieldCapacity();
    TestEqual(TEXT("Repeated recomposition does not refill"), Combat->GetFrontShield(), Recovering, .001f);
    Combat->TickComponent(5, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Core front recovers to capacity"), Combat->GetFrontShield(), CoreMax, .001f);
    Hit.SourceLocation = FVector(-100, 0, 0); Hit.BaseDamage = CoreMax / 10.0f;
    const float WardBefore = Attr->GetShield();
    for (int32 Index = 0; Index < 5; ++Index) Combat->ReceiveDamage(Hit);
    TestEqual(TEXT("Small rear hits consume exactly half effective front durability"), Combat->GetFrontShield(), 0.0f, .001f);
    TestEqual(TEXT("Front pool still precedes ward on rear hits"), Attr->GetShield(), WardBefore, .001f);
    UFunction* ParryRPC = Player->FindFunction(TEXT("ServerParry")); if (!ParryRPC) return false;
    auto* Observer = NewObject<UBreakerCoreParryRuntimeObserver>(Player); Observer->Combat = Combat;
    Hit.SourceLocation = FVector(100, 0, 0); Hit.BaseDamage = CoreMax * .25f;
    Observer->Reentry = Hit; Observer->bReenter = true;
    Combat->OnHealed.AddDynamic(Observer, &UBreakerCoreParryRuntimeObserver::OnHealed);
    Attr->ApplyHealth(Attr->GetMaxHealth() * .5f);
    Player->ProcessEvent(ParryRPC, nullptr); TestTrue(TEXT("Actual parry triggers Wall"), Combat->ReceiveDamage(Hit).bParried);
    TestEqual(TEXT("Wall rebuilds once before callback damage"), Combat->GetFrontShield(), CoreMax * .75f, .001f);
    TestEqual(TEXT("Wall callback cannot repeat success heal"), Observer->HealingEvents, 1);
    Hit.bBypassShield = true;
    const float BeforeBypass = Combat->GetFrontShield(); Combat->ReceiveDamage(Hit);
    TestEqual(TEXT("Explicit bypass does not spend front pool"), Combat->GetFrontShield(), BeforeBypass);
    Hit.bBypassShield = false; Hit.BaseDamage = 1000000;
    Combat->ReceiveDamage(Hit);
    TestTrue(TEXT("Real lethal hit kills shield owner"), Combat->IsDead());
    Advance(100);
    Combat->TickComponent(2, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Dead owner cannot recharge ward"), Attr->GetShield(), 0.0f);
    TestEqual(TEXT("Dead owner cannot recharge Core front pool"), Combat->GetFrontShield(), 0.0f);
    Combat->RestoreVitals();
    TestEqual(TEXT("Explicit respawn restores ward"), Attr->GetShield(), Attr->GetMaxShield());
    TestEqual(TEXT("Explicit respawn restores Core front pool"), Combat->GetFrontShield(), CoreMax, .001f);
    if (!TestTrue(TEXT("Actual Core respec removes capacity"), Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec removes Core front maximum"), Combat->GetFrontShieldMax(), 0.0f);
    TestEqual(TEXT("Respec removes current Core front"), Combat->GetFrontShield(), 0.0f);
    TestEqual(TEXT("Respec retains only equipped ward base"), Attr->GetMaxShield(), Gear->GetStats().BaseShieldFromGear, .001f);
    if (!TestTrue(TEXT("Actual unequip removes gear source"), Gear->UnequipSlot(EBreakerEquipSlot::BodyArmour))) return false;
    TestEqual(TEXT("Unequip after respec restores native zero"), Attr->GetMaxShield(), 0.0f);
    // Model a remote replica receiving final attributes. Local named inputs
    // cannot overwrite authoritative capacity when health later replicates.
    Attr->ApplyMaxShield(77); Player->SetRole(ROLE_SimulatedProxy);
    Attr->SetCoreShieldHealthFraction(.5f); Attr->SetEquipmentShieldCapacity(200);
    Attr->ApplyMaxHealth(300);
    TestEqual(TEXT("Remote health update preserves replicated shield capacity"), Attr->GetMaxShield(), 77.0f);
    float ReplicatedCap = 91;
    Attr->PreAttributeChange(UBreakerAttributeSet::GetMaxShieldAttribute(), ReplicatedCap);
    TestEqual(TEXT("Remote shield update is not recomposed from local inputs"), ReplicatedCap, 91.0f);
    Player->SetRole(ROLE_Authority);
    return true;
}
#endif
