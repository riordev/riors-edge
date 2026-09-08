#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreBaseSpreadRuntimeTest, "RiorsEdge.Weapons.CoreBaseSpreadRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreBaseSpreadRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attributes = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attributes);
    Player->GetCombat()->BindAttributes(Attributes); Player->GetEquipment()->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attributes);
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster;
    auto* Tree = NewObject<UBreakerProgressionTree>(Class);
    Tree->TreeId = TEXT("Test.Core.BaseSpread"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    Class->BranchTrees.Add(Tree);
    auto* Reduction = NewObject<UBreakerProgressionNode>(Tree);
    Reduction->NodeId = TEXT("Test.Core.BaseSpread.Reduction"); Reduction->Currency = Tree->Currency;
    FBreakerNodeEffect Effect; Effect.StatTarget = EBreakerNodeStatTarget::WeaponBaseSpreadReduction;
    Effect.StatBucket = EBreakerNodeStatBucket::Flat; Effect.ValuePerRank = 10;
    Reduction->Effects.Add(Effect); Tree->Nodes.Add(Reduction);
    auto* Fan = NewObject<UBreakerProgressionNode>(Tree);
    Fan->NodeId = TEXT("Test.Core.BaseSpread.Fan"); Fan->Currency = Tree->Currency;
    Fan->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Fan")));
    Tree->Nodes.Add(Fan);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, Progression->ExperienceCurve));
    auto* Weapon = Player->GetWeapon(); Weapon->ResetAmmunition();
    const auto* Definition = Weapon->GetActiveDefinition(); if (!Definition) return false;
    auto Clock = [&](float Seconds)
    {
        for (int32 I = 0; I < FMath::CeilToInt(Seconds * 100); ++I)
        { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); }
    };
    auto Fire = [&]()
    {
        const float Prediction = Weapon->GetNextShotSpreadDegrees();
        const int32 Ammunition = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("Native trigger spends one round"), Weapon->GetMagazineAmmo(), Ammunition - 1);
        TestEqual(TEXT("Fired cone matches prediction"), Weapon->GetLastShot().SpreadDegrees, Prediction, .0001f);
    };
    FText Reason;
    for (bool bAimed : { false, true })
    {
        Weapon->SetAiming(bAimed); Clock(1);
        Fire();
        // Purchase between real burst shots without advancing the bloom clock.
        const float Base = bAimed ? Definition->AimSpreadDegrees : Definition->HipSpreadDegrees;
        const float Before = Weapon->GetNextShotSpreadDegrees();
        const float Bloom = Weapon->GetBloomDegrees();
        TestTrue(TEXT("Shipped rifle emits nonzero bloom"), Bloom > 0);
        if (!TestTrue(TEXT("Reduction uses earned purchase"), Progression->PurchaseNode(Tree, Reduction->NodeId, Reason))) return false;
        TestEqual(TEXT("Exactly ten percent of base is removed, not the total cone"),
            Weapon->GetNextShotSpreadDegrees(), Before - Base * .10f, .0001f);
        TestEqual(TEXT("Buying base reduction leaves accumulated bloom intact"), Weapon->GetBloomDegrees(), Bloom, .0001f);
        Clock(60.0f / Weapon->GetEffectiveRoundsPerMinute(Definition) + .01f); Fire();
        const float Reduced = Weapon->GetNextShotSpreadDegrees();
        if (!TestTrue(TEXT("Real respec withdraws the reduction"), Progression->RespecCore(Reason))) return false;
        TestEqual(TEXT("Respec restores exactly the base contribution"), Weapon->GetNextShotSpreadDegrees(), Reduced + Base * .10f, .0001f);
    }
    if (!Progression->PurchaseNode(Tree, Fan->NodeId, Reason)) return false;
    const float FanCone = Weapon->GetNextShotSpreadDegrees();
    if (!Progression->PurchaseNode(Tree, Reduction->NodeId, Reason)) return false;
    TestEqual(TEXT("Fan refuses base-cone reduction"), Weapon->GetNextShotSpreadDegrees(), FanCone, .0001f);
    Clock(1); Fire();
    TestTrue(TEXT("Fan firing retains the hip floor"), Weapon->GetLastShot().SpreadDegrees >= Definition->HipSpreadDegrees);
    return true;
}
#endif
