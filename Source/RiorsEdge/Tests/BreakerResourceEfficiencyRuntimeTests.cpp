#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
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
#include "Abilities/BreakerAbility_Siphon.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Classes/BreakerManaComponent.h"
#include "Components/SphereComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerResourceEfficiencyRuntimeTest,
    "RiorsEdge.Abilities.ResourceEfficiencyRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerResourceEfficiencyRuntimeTest::RunTest(const FString& Parameters)
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
    // O272: Redirect -> SpendToLive is the whole route, one point each, no gate.
    const auto* Tree = UBreakerProgressionLibrary::GetSwiftKineticTree();
    TestEqual(TEXT("Fixture ships the eight-point doctrine wallet"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
    for (const TCHAR* Node : {TEXT("Swift.Kinetic.Redirect"), TEXT("Swift.Kinetic.SpendToLive")})
    {
        const bool Bought = Progression->PurchaseNode(Tree, Node, Reason);
        if (!TestTrue(FString::Printf(TEXT("Legal purchase %s: %s"), Node, *Reason.ToString()), Bought)) return false;
    }
    TestEqual(TEXT("Legal path spends two of eight Doctrine, six unspent"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 6);
    TestEqual(TEXT("Live instance quotes doubled cost before first cast"), Abilities->GetCost(Slot), 60.0f, .0001f);
    FBreakerItemInstance Gear;
    bool bFound = false;
    for (int32 Seed = 1; Seed <= 4096 && !bFound; ++Seed)
    {
        Gear = UBreakerLootLibrary::RollItem(TEXT("Cost.Runtime"), EBreakerEquipSlot::Helmet, EBreakerItemRarity::Standard, 1, Seed);
        bFound = Gear.Affixes.ContainsByPredicate([](const auto& Row) { return Row.AffixId == TEXT("Core.ResourceEfficiency"); });
    }
    if (!TestTrue(TEXT("Ordinary rolled gear reaches resource efficiency"), bFound)) return false;
    auto* Equipment = Player->FindComponentByClass<UBreakerEquipmentComponent>();
    Equipment->BindAttributes(Attributes);
    if (!TestTrue(TEXT("Real gear equips with all rolled lines intact"), Equipment->EquipItem(Gear))) return false;
    const float Multiplier = Attributes->GetResourceCostMultiplier();
    if (!TestTrue(TEXT("Actual gear reduces live multiplier"), Multiplier < 1 && Multiplier > 0)) return false;
    TestEqual(TEXT("Swift HUD applies efficiency before Spend to Live"), Abilities->GetCost(Slot), 60 * Multiplier, .001f);    auto* Momentum = Player->GetMomentum();
    Momentum->BindAttributes(Attributes);
    Momentum->SetComponentTickEnabled(false);
    Movement->SetMovementMode(MOVE_Walking);
    auto EarnStep = [&]()
    {
        Movement->Velocity = FVector(Movement->WalkSpeed, 0, 0);
        Player->SetActorLocation(Player->GetActorLocation() + Movement->Velocity);
        Momentum->AdvanceLoop(1);
    };
    for (int32 Step = 0; Step < 40 && Momentum->GetMomentum() < 35; ++Step) EarnStep();
    Movement->StopMovementImmediately();
    const float Below = Momentum->GetMomentum();
    if (!TestTrue(TEXT("Normal movement income lies between old and live costs"), Below >= 30 && Below < 60)) return false;
    TestFalse(TEXT("HUD refuses insufficient live cost"), Abilities->CanAffordSlot(Slot));
    TestFalse(TEXT("GAS slot activation also refuses"), Abilities->TryActivateSlot(Slot));
    TestEqual(TEXT("Refusal spends nothing"), Momentum->GetMomentum(), Below, .0001f);
    for (int32 Step = 0; Step < 40 && Momentum->GetMomentum() < 65; ++Step) EarnStep();
    Movement->StopMovementImmediately();
    const float Before = Momentum->GetMomentum();
    if (!TestTrue(TEXT("Normal income funds doubled payment"), Before >= 60)) return false;
    TestTrue(TEXT("HUD agrees payment is affordable"), Abilities->CanAffordSlot(Slot));
    if (!TestTrue(TEXT("Real paid slot cast succeeds"), Abilities->TryActivateSlot(Slot))) return false;
    TestEqual(TEXT("Actual debit equals the displayed live cost"), Before - Momentum->GetMomentum(), 60.0f * Multiplier, .001f);
    TestTrue(TEXT("Actual Hard Stop window opened"), UBreakerAbilityStateComponent::FindOrAdd(Player)->IsWindowActive(UBreakerAbility_HardStop::WindowKey()));
    if (!TestTrue(TEXT("Unequip restores ordinary live Swift price"), Equipment->UnequipSlot(EBreakerEquipSlot::Helmet))) return false;
    TestEqual(TEXT("Swift live price returns to sixty"), Abilities->GetCost(Slot), 60.0f, .001f);

    auto* Caster = World->SpawnActor<ABreakerCharacter>(FVector(5000, 0, 0), FRotator::ZeroRotator);
    if (!Caster) return false;
    Caster->SetActorTickEnabled(false); Caster->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* CasterASC = Caster->GetAbilitySystemComponent(); auto* CasterAttributes = Caster->GetAttributes();
    CasterASC->InitAbilityActorInfo(Caster, Caster); CasterASC->AddAttributeSetSubobject(CasterAttributes);
    Caster->GetCombat()->BindAttributes(CasterAttributes); Caster->GetProgression()->BindAttributes(CasterAttributes);
    if (!Caster->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
    Caster->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Caster->GetProgression()->ExperienceCurve));
    if (!Caster->GetProgression()->IsAbilityUnlocked(TEXT("Caster.Siphon")))
        if (!TestTrue(TEXT("Earned token unlocks Siphon"), Caster->GetProgression()->SpendAbilityToken(TEXT("Caster.Siphon"), Reason))) return false;
    if (!Caster->GetProgression()->EquipAbility(Slot, TEXT("Caster.Siphon"), Reason)) return false;
    Caster->GetAbilities()->RefreshGrants();
    auto* CasterGear = Caster->FindComponentByClass<UBreakerEquipmentComponent>(); CasterGear->BindAttributes(CasterAttributes);
    if (!CasterGear->EquipItem(Gear)) return false;
    auto* Mana = Caster->GetMana(); Mana->BindAttributes(CasterAttributes); Mana->AdvanceLoop(60);
    auto* Target = World->SpawnActor<AActor>();
    auto* Body = NewObject<USphereComponent>(Target); Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
    Body->SetSphereRadius(40); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    Body->RegisterComponent(); Target->SetActorLocation(FVector(5500, 0, 0));
    auto* TargetCombat = NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(TargetCombat); TargetCombat->RegisterComponent();
    auto* TargetAttributes = NewObject<UBreakerAttributeSet>(Target); TargetCombat->BindAttributes(TargetAttributes);
    const float CasterMultiplier = CasterAttributes->GetResourceCostMultiplier();
    const float AuthoredCost = Caster->GetAbilities()->GetDefinitionForSlot(Slot)->ResourceCost;
    TestEqual(TEXT("Caster HUD applies live efficiency exactly once"), Caster->GetAbilities()->GetCost(Slot), AuthoredCost * CasterMultiplier, .001f);
    const float BeforeMana = CasterAttributes->GetClassResource();
    if (!TestTrue(TEXT("Real paid efficient Siphon casts"), Caster->GetAbilities()->TryActivateSlot(Slot))) return false;
    BreakerResolvePendingCast(World, Caster);
    TestEqual(TEXT("Caster debit is not squared efficiency"), BeforeMana - CasterAttributes->GetClassResource(), AuthoredCost * CasterMultiplier, .001f);
    CasterASC->CancelAllAbilities();
    Mana->AdvanceLoop(60);
    const auto Unmake = CasterASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Unmake::StaticClass(), 1));
    if (!TestTrue(TEXT("Actual paid Unmake opens free window"), CasterASC->TryActivateAbility(Unmake))) return false;
    BreakerResolvePendingCast(World, Caster);
    TestEqual(TEXT("Unmake remains downstream and makes efficient Siphon free"), Caster->GetAbilities()->GetCost(Slot), 0.0f);
    const float BeforeFree = CasterAttributes->GetClassResource();
    if (!TestTrue(TEXT("Native free-window Siphon casts"), Caster->GetAbilities()->TryActivateSlot(Slot))) return false;
    BreakerResolvePendingCast(World, Caster);
    TestEqual(TEXT("Free window cannot debit a second efficiency cost"), CasterAttributes->GetClassResource(), BeforeFree);    return true;
}
#endif
