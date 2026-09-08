#include "Tests/BreakerCoreBreakRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponComponent.h"

void UBreakerCoreBreakRuntimeObserver::OnHit(const FBreakerHitContext& Hit)
{
    Hits.Add(Hit);
    ArmorAtCallback.Add(Combat ? Combat->GetEffectiveArmor() : -1);
    if (bReenter && Combat)
    {
        bReenter = false;
        Combat->ReceiveDamage(Reentry);
    }
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreBreakRuntimeTest, "RiorsEdge.Combat.CoreBreakRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreBreakRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    World->InitializeActorsForPlay(FURL());
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attr = Player->GetAttributes(); auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    auto* Tree = NewObject<UBreakerProgressionTree>(); Tree->TreeId = TEXT("Test.Core.Break"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = TEXT("Test.Core.Break.Rule"); Node->Currency = Tree->Currency;
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Ballistics.Break"))); Tree->Nodes.Add(Node);
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster; Class->BranchTrees.Add(Tree);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2, Progression->ExperienceCurve));
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim);
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(Eye + Aim.Vector() * 500, FRotator::ZeroRotator);
    if (!Enemy) return false;
    Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    if (auto* Movement = Enemy->FindComponentByClass<UBreakerEnemyMovementComponent>()) Movement->SetComponentTickEnabled(false);
    if (auto* Status = Enemy->FindComponentByClass<UBreakerStatusComponent>()) Status->SetComponentTickEnabled(false);
    auto* Victim = Enemy->FindComponentByClass<UBreakerCombatComponent>();
    auto* TargetAttr = const_cast<UBreakerAttributeSet*>(Enemy->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>());
    if (!Victim || !TargetAttr) return false;
    Victim->SetComponentTickEnabled(false);
    TargetAttr->SetAggregatedAttributeBase(EBreakerAggregatedAttribute::Armor, 100);
    Victim->PushArmorReduction(TEXT("Test.Break.Flat"), 20);
    auto* Observer = NewObject<UBreakerCoreBreakRuntimeObserver>(); Observer->Combat = Victim;
    Victim->OnDamageTaken.AddDynamic(Observer, &UBreakerCoreBreakRuntimeObserver::OnHit);
    auto Advance = [&](int32 Frames)
    { for (int32 Frame = 0; Frame < Frames; ++Frame) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); } };
    auto* Weapon = Player->GetWeapon(); Weapon->ResetAmmunition();
    auto Fire = [&]()
    {
        Advance(5); const int32 Before = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        return TestEqual(TEXT("Default rifle trigger debits one real round"), Weapon->GetMagazineAmmo(), Before - 1);
    };
    if (!Fire() || !TestTrue(TEXT("Default rifle hits actual enemy body"), !Observer->Hits.IsEmpty())) return false;
    TestEqual(TEXT("Unowned rifle leaves armour unchanged"), Victim->GetEffectiveArmor(), 80.0f, .001f);
    FText Reason;
    if (!TestTrue(TEXT("Earned Core point purchases Break"), Progression->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    Observer->Hits.Reset(); Observer->ArmorAtCallback.Reset();
    if (!Fire() || !TestTrue(TEXT("Purchased default rifle delivers enabling hit"), !Observer->Hits.IsEmpty())) return false;
    TestEqual(TEXT("Rifle shreds after flat armour reduction"), Victim->GetEffectiveArmor(), 73.6f, .001f);
    TestEqual(TEXT("Damage callback sees the newly earned stack"), Observer->ArmorAtCallback[0], 73.6f, .001f);
    FBreakerDamageRequest Raw; Raw.BaseDamage = Observer->Hits[0].Result.RawDamage; Raw.bCanCritical = false; Raw.bCanBeAvoided = false;
    FBreakerDefenseState Defense; Defense.Health = TargetAttr->GetMaxHealth(); Defense.Armor = 80;
    TestEqual(TEXT("Enabling rifle hit still pays the previous armour"), Observer->Hits[0].Result.HealthDamage,
        UBreakerDamageLibrary::ResolveDamage(Raw, Defense).HealthDamage, .001f);

    FBreakerDamageRequest Stored; Stored.BaseDamage = 10; Stored.bCanCritical = false; Stored.bCanBeAvoided = false; Stored.SetInstigator(Player);
    UBreakerDamageLibrary::FillSourcePools(Attr, EBreakerDamageDelivery::Weapon, Stored);
    TestTrue(TEXT("Emitted weapon request snapshots purchased Break"), Stored.bWeaponArmorShred);
    auto Ability = Stored; UBreakerDamageLibrary::FillSourcePools(Attr, EBreakerDamageDelivery::Ability, Ability);
    TestFalse(TEXT("Ability source cannot snapshot weapon shred"), Ability.bWeaponArmorShred);
    auto Periodic = Stored; Periodic.bIsDamageOverTime = true;
    UBreakerDamageLibrary::FillSourcePools(Attr, EBreakerDamageDelivery::Weapon, Periodic);
    TestFalse(TEXT("Periodic source cannot snapshot direct weapon shred"), Periodic.bWeaponArmorShred);
    if (!TestTrue(TEXT("Native respec removes live ownership"), Progression->RespecCore(Reason))) return false;
    auto AfterRespec = Stored; UBreakerDamageLibrary::FillSourcePools(Attr, EBreakerDamageDelivery::Weapon, AfterRespec);
    TestFalse(TEXT("Requests emitted after respec no longer shred"), AfterRespec.bWeaponArmorShred);
    Victim->RestoreVitals();
    Victim->ReceiveDamage(Stored);
    TestEqual(TEXT("Already-emitted request retains its rule after respec"), Victim->GetEffectiveArmor(), 73.6f, .001f);

    Victim->RestoreVitals();
    auto Zero = Stored; Zero.BaseDamage = 0; Victim->ReceiveDamage(Zero);
    TestEqual(TEXT("Zero damage cannot build stacks"), Victim->GetEffectiveArmor(), 80.0f, .001f);
    Victim->DodgeChance = 1;
    auto Avoided = Stored; Avoided.bCanBeAvoided = true;
    const auto Avoidance = Victim->ReceiveDamage(Avoided);
    TestTrue(TEXT("Native dodge refuses the hit"), Avoidance.bDodged);
    TestEqual(TEXT("Avoided hit cannot build stacks"), Victim->GetEffectiveArmor(), 80.0f, .001f);
    Victim->DodgeChance = 0;
    Periodic = Stored; Periodic.bIsDamageOverTime = true; Victim->ReceiveDamage(Periodic);
    Ability = Stored; Ability.Delivery = EBreakerDamageDelivery::Ability; Victim->ReceiveDamage(Ability);
    TestEqual(TEXT("Copied flag cannot turn ability or periodic delivery into a shred"), Victim->GetEffectiveArmor(), 80.0f, .001f);
    Observer->Hits.Reset(); Observer->ArmorAtCallback.Reset(); Observer->Reentry = Stored; Observer->bReenter = true;
    Victim->ReceiveDamage(Stored);
    if (!TestEqual(TEXT("Actual damage callback dispatches one nested weapon hit"), Observer->Hits.Num(), 2)) return false;
    TestEqual(TEXT("Outer callback sees first stack"), Observer->ArmorAtCallback[0], 73.6f, .001f);
    TestEqual(TEXT("Nested callback sees second stack"), Observer->ArmorAtCallback[1], 67.2f, .001f);
    Defense.Armor = 73.6f;
    TestEqual(TEXT("Nested hit pays armour including the outer stack"), Observer->Hits[1].Result.HealthDamage,
        UBreakerDamageLibrary::ResolveDamage(Stored, Defense).HealthDamage, .001f);

    Victim->RestoreVitals(); Victim->ReceiveDamage(Stored);
    Advance(20); Victim->ReceiveDamage(Stored);
    Advance(20); Victim->ReceiveDamage(Stored);
    TestEqual(TEXT("Three hits cap at twenty-four percent shred"), Victim->GetEffectiveArmor(), 60.8f, .001f);
    Advance(20); Victim->ReceiveDamage(Stored);
    TestEqual(TEXT("Fourth hit replaces oldest instead of granting a fourth stack"), Victim->GetEffectiveArmor(), 60.8f, .001f);
    Advance(22);
    TestEqual(TEXT("Replaced oldest expiry no longer removes a stack"), Victim->GetEffectiveArmor(), 60.8f, .001f);
    Advance(20); TestEqual(TEXT("Second independent expiry leaves two stacks"), Victim->GetEffectiveArmor(), 67.2f, .001f);
    Advance(20); TestEqual(TEXT("Third independent expiry leaves one stack"), Victim->GetEffectiveArmor(), 73.6f, .001f);
    Advance(20); TestEqual(TEXT("Final independent expiry restores flat-reduced armour"), Victim->GetEffectiveArmor(), 80.0f, .001f);
    Victim->PopArmorReduction(TEXT("Test.Break.Flat"));
    TestEqual(TEXT("Removing independent flat shred restores physical armour"), Victim->GetEffectiveArmor(), 100.0f, .001f);
    return true;
}
#endif
