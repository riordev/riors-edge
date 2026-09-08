#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::PhysicalDamageReduction)==36);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::ElementalResistance)==37);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::AilmentAvoidance)==38);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::AbilityCastRate)==39);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::AbilityChannelRate)==40);
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreDefencePrimitiveTest,"RiorsEdge.Progression.CoreDefencePrimitives",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreDefencePrimitiveTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Player=World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    auto* Attr=Player->GetAttributes(); auto* ASC=Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr);
    auto* Combat=Player->GetCombat(); Combat->BindAttributes(Attr);
    auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr);
    auto* Tree=NewObject<UBreakerProgressionTree>(); Tree->TreeId=TEXT("Test.Defence.Schema"); Tree->Currency=EBreakerPointCurrency::CorePoints;
    auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Defence.Lanes"); Node->Currency=Tree->Currency;
    auto Add=[&](EBreakerNodeStatTarget Target,float Value,EBreakerNodeStatBucket Bucket=EBreakerNodeStatBucket::IncreasedPercent)
    {
        FBreakerNodeEffect Effect; Effect.StatTarget=Target; Effect.StatBucket=Bucket;
        Effect.ValuePerRank=Value; Node->Effects.Add(Effect);
    };
    // Temporary primitive schema, not replacement Core node authoring. Large
    // test-only percentages exercise real clamps and shared aggregation.
    Add(EBreakerNodeStatTarget::Health,50); Add(EBreakerNodeStatTarget::Armor,50);
    Add(EBreakerNodeStatTarget::PhysicalDamageReduction,80,EBreakerNodeStatBucket::Flat);
    Add(EBreakerNodeStatTarget::ElementalResistance,25,EBreakerNodeStatBucket::Flat);
    Add(EBreakerNodeStatTarget::AilmentAvoidance,90,EBreakerNodeStatBucket::Flat); Add(EBreakerNodeStatTarget::AbilityCastRate,20);
    Add(EBreakerNodeStatTarget::AbilityChannelRate,30); Tree->Nodes.Add(Node);
    auto* Class=NewObject<UBreakerClassDefinition>(); Class->ClassId=EBreakerClassId::Caster; Class->BranchTrees.Add(Tree);
    if (!TestTrue(TEXT("Actual schema class selected"),Progression->ChoosePermanentClass(Class))) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2,Progression->ExperienceCurve));
    auto* Gear=Player->GetEquipment(); Gear->BindAttributes(Attr);
    const auto Body=UBreakerLootLibrary::RollItem(TEXT("Defence.Body"),EBreakerEquipSlot::BodyArmour,EBreakerItemRarity::Standard,1,1);
    if (!TestTrue(TEXT("Actual unmodified body armour equips"),Gear->EquipItem(Body))) return false;
    FBreakerItemInstance Neck; bool Found=false;
    for (int32 Seed=1; Seed<=8192; ++Seed)
    {
        Neck=UBreakerLootLibrary::RollItem(TEXT("Defence.Neck"),EBreakerEquipSlot::Necklace,EBreakerItemRarity::Standard,1,Seed);
        if (Neck.Affixes.ContainsByPredicate([](const auto& Line){return Line.AffixId==TEXT("Core.PhysicalDR");})) {Found=true;break;}
    }
    if (!TestTrue(TEXT("Actual ordinary Physical DR roll found"),Found) || !TestTrue(TEXT("Unmodified necklace equips"),Gear->EquipItem(Neck))) return false;
    const float BeforeHealth=Attr->GetMaxHealth(), BeforeArmor=Attr->GetArmor();
    const float GearDR=Gear->GetStats().PhysicalDamageReductionPercent;
    auto* Status=Player->FindComponentByClass<UBreakerStatusComponent>();
    if (!Status) return false;
    const float BeforeResistance=Status->GetEntropyResistancePercent();
    const float BeforeAvoidance=Status->GetEffectiveAilmentAvoidanceChance();
    FText Reason;
    if (!TestTrue(TEXT("Earned point purchases schema effect"),Progression->PurchaseNode(Tree,Node->NodeId,Reason))) return false;
    // Rolled gear uses flat vitality/armour here; any existing Increased stays
    // in the same bucket, checked by removing only the new node below.
    TestEqual(TEXT("Percentage health scales existing flat base and gear once"),Attr->GetMaxHealth(),BeforeHealth*1.5f,.001f);
    TestEqual(TEXT("Percentage armour scales existing flat base and gear once"),Attr->GetArmor(),BeforeArmor*1.5f,.001f);
    TestEqual(TEXT("Cast rate percentage composes"),Progression->GetNodeStats().AbilityCastRateMultiplier,1.2f,.0001f);
    TestEqual(TEXT("Channel rate percentage composes"),Progression->GetNodeStats().AbilityChannelRateMultiplier,1.3f,.0001f);
    auto Damage=[&](EBreakerDamageFamily Family,float Fraction=0.0f,bool bBuildup=false,EBreakerElement Element=EBreakerElement::Void)
    {
        Combat->RestoreVitals(); FBreakerDamageRequest Hit;
        Hit.BaseDamage=bBuildup?1.0f:10.0f; Hit.DamageFamily=Family; Hit.ArmorPenetration=10000;
        Hit.bCanBeAvoided=false; Hit.bCanCritical=false; Hit.bBypassShield=true;
        Hit.Element=Fraction>0?Element:EBreakerElement::None; Hit.ElementalFraction=Fraction;
        Hit.bCanApplyElementBuildup=bBuildup;
        return Combat->ReceiveDamage(Hit).HealthDamage;
    };
    TestEqual(TEXT("Gear plus Core shares physical cap"),Damage(EBreakerDamageFamily::Physical),4.0f,.001f);
    TestEqual(TEXT("Physical cap affects only unconverted half"),Damage(EBreakerDamageFamily::Physical,.5f),7.0f,.001f);
    TestEqual(TEXT("Core physical DR does not reduce elemental damage"),Damage(EBreakerDamageFamily::Elemental,1),10.0f,.001f);
    TestEqual(TEXT("Core physical DR does not reduce True damage"),Damage(EBreakerDamageFamily::TrueDamage),10.0f,.001f);
    const float Resistance=FMath::Min(100.0f,BeforeResistance+25);
    TestEqual(TEXT("Entropy consumes Core resistance"),Status->GetEntropyResistancePercent(),Resistance,.001f);
    TestEqual(TEXT("Void consumes Core resistance"),Status->GetVoidResistancePercent(),Resistance,.001f);
    TestEqual(TEXT("Rift consumes Core resistance"),Status->GetRiftResistancePercent(),Resistance,.001f);
    for (const auto Element : {EBreakerElement::Entropy,EBreakerElement::Void,EBreakerElement::Rift})
        TestEqual(TEXT("Resistance leaves applying-hit damage unchanged"),Damage(EBreakerDamageFamily::Elemental,1,true,Element),1.0f,.001f);
    const float ExpectedBuildup=1.0f-Resistance/100.0f;
    TestEqual(TEXT("Actual Entropy buildup reduced"),Status->GetEntropyBuildup(),ExpectedBuildup,.001f);
    TestEqual(TEXT("Actual Void buildup reduced"),Status->GetVoidBuildup(),ExpectedBuildup,.001f);
    TestEqual(TEXT("Actual Rift buildup reduced"),Status->GetRiftBuildup(),ExpectedBuildup,.001f);
    TestEqual(TEXT("Core avoidance respects existing cap"),Status->GetEffectiveAilmentAvoidanceChance(),UBreakerStatusComponent::MaxAilmentAvoidanceChance,.001f);
    int32 Applied=0;
    for (int32 I=0;I<32;++I)
    {
        Status->ConsumeAllStatuses(); FBreakerStatusApplicationSpec Ailment;
        Ailment.StatusTag=FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed")); Ailment.Duration=2; Ailment.TickInterval=1; Ailment.BaseDamagePerTick=1;
        Status->ApplyStatus(Ailment,EBreakerDamageFamily::Physical,nullptr);
        if (Status->HasStatus(Ailment.StatusTag)) ++Applied;
    }
    TestTrue(TEXT("Actual applications exercise both avoidance and non-immunity"),Applied>0 && Applied<32);
    if (!TestTrue(TEXT("Actual Core respec"),Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Health percentage removed without ratchet"),Attr->GetMaxHealth(),BeforeHealth,.001f);
    TestEqual(TEXT("Armour percentage removed without ratchet"),Attr->GetArmor(),BeforeArmor,.001f);
    TestEqual(TEXT("Only original gear physical reduction remains"),Damage(EBreakerDamageFamily::Physical),10*(1-GearDR/100),.001f);
    TestEqual(TEXT("Ward resistance removed"),Status->GetEntropyResistancePercent(),BeforeResistance,.001f);
    TestEqual(TEXT("Ward avoidance removed"),Status->GetEffectiveAilmentAvoidanceChance(),BeforeAvoidance,.001f);
    return true;
}
#endif
