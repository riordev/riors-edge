#include "Tests/BreakerFractureTestHelpers.h"
#include "Tests/BreakerReactionRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "UI/BreakerSkillProjection.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDeadeyeCompositionTest, "RiorsEdge.Attributes.DeadeyeComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDeadeyeCompositionTest::RunTest(const FString& Parameters)
{
    using Attr = EBreakerAggregatedAttribute;
    FBreakerAttributeAggregator Fold;
    Fold.SetBase(Attr::CriticalChance, .05f); Fold.SetBase(Attr::CriticalMultiplier, 1.5f);
    FBreakerAttributeContribution Tree, Gear;
    Tree.SetDeadeye(true); Tree.AddFlat(Attr::CriticalMultiplier, .50f); Tree.AddFlat(Attr::CriticalMultiplier, -.20f);
    Gear.AddFlat(Attr::CriticalMultiplier, .40f); Gear.AddFlat(Attr::CriticalMultiplier, -.10f);
    Fold.SetContribution(EBreakerAttributeContributor::Progression, Tree);
    Fold.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    TestEqual(TEXT("Deadeye guarantees critical chance"), Fold.Compose(Attr::CriticalChance), 1.0f);
    TestEqual(TEXT("Native base and negative penalties stay intact, positive bonuses halve"), Fold.Compose(Attr::CriticalMultiplier), 1.65f, .0001f);
    Gear.AddIncreasedPercent(Attr::CriticalMultiplier, 20); Gear.AddIncreasedPercent(Attr::CriticalMultiplier, -5);
    Fold.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    TestEqual(TEXT("Signed Increased crit bucket follows same forfeit"), Fold.Compose(Attr::CriticalMultiplier), 1.65f * 1.05f, .0001f);
    Gear.ComposeMore(Attr::CriticalMultiplier, 1.4f); Gear.ComposeMore(Attr::CriticalMultiplier, .8f);
    Fold.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    TestEqual(TEXT("Positive crit product halves its bonus and leaves penalty product"), Fold.Compose(Attr::CriticalMultiplier), 1.65f * 1.05f * 1.2f * .8f, .0001f);
    Tree.SetDeadeye(false); Fold.SetContribution(EBreakerAttributeContributor::Progression, Tree);
    TestEqual(TEXT("Removing rule restores ordinary chance"), Fold.Compose(Attr::CriticalChance), .05f);
    TestEqual(TEXT("Removing rule restores raw contributions"), Fold.Compose(Attr::CriticalMultiplier), 2.10f * 1.15f * 1.4f * .8f, .0001f);
    Tree.Reset(); TestTrue(TEXT("Reset withdraws critical rule and signed bookkeeping"), Tree.IsIdentity());
    FBreakerDamageRequest Hit; Hit.BaseDamage = 10; Hit.CriticalChance = 0; Hit.CriticalMultiplier = 1.65f;
    Hit.bCanCritical = false; Hit.bForceCriticalStrike = true;
    TestTrue(TEXT("Direct force overrides ordinary non-critical delivery"), UBreakerDamageLibrary::ResolveDamage(Hit, {}).bCritical);
    Hit.bIsDamageOverTime = true;
    TestFalse(TEXT("Periodic damage cannot acquire direct force"), UBreakerDamageLibrary::ResolveDamage(Hit, {}).bCritical);
    Hit.bCanCritical = true; Hit.bUseSnapshotCritical = true; Hit.bSnapshotCriticalResult = false;
    TestFalse(TEXT("Existing non-critical ailment snapshot cannot be rewritten"), UBreakerDamageLibrary::ResolveDamage(Hit, {}).bCritical);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreDeadeyeRuntimeTest, "RiorsEdge.Combat.CoreDeadeyeRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreDeadeyeRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attr = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr); auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    if (!TestTrue(TEXT("Real Caster selection"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    Player->GetMana()->BindAttributes(Attr); Player->GetMana()->AdvanceLoop(20);
    auto* Equipment = Player->FindComponentByClass<UBreakerEquipmentComponent>();
    if (!TestNotNull(TEXT("Native equipment"), Equipment)) return false;
    Equipment->BindAttributes(Attr);
    FBreakerItemInstance Helmet;
    bool bFound = false;
    for (int32 Seed = 1; Seed <= 4096; ++Seed)
    {
        Helmet = UBreakerLootLibrary::RollItem(TEXT("Test.Core.Deadeye.Helmet"), EBreakerEquipSlot::Helmet, EBreakerItemRarity::Standard, 5, Seed);
        if (Helmet.Affixes.ContainsByPredicate([](const FBreakerRolledAffix& Affix) { return Affix.AffixId == FName(TEXT("Crit.Damage")); }))
        { bFound = true; break; }
    }
    if (!TestTrue(TEXT("Critical bonus comes from actual ordinary loot roll"), bFound)
        || !TestTrue(TEXT("Rolled item really equips"), Equipment->EquipItem(Helmet))) return false;
    const float BeforeMultiplier = Attr->GetCriticalMultiplier();
    const float NativeBase = Attr->GetAttributeBase(EBreakerAggregatedAttribute::CriticalMultiplier);
    TestTrue(TEXT("Actual gear contributes positive critical bonus"), BeforeMultiplier > NativeBase);
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition, Player);
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId = TEXT("Test.Core.Deadeye"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = TEXT("Test.Core.Deadeye.Rule"); Node->Currency = Tree->Currency;
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Deadeye")));
    Tree->Nodes.Add(Node); Definition->BranchTrees.Add(Tree); Progression->ClassDefinition = Definition;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, Progression->ExperienceCurve));
    FText Reason;
    if (!TestTrue(TEXT("Core rule purchased using level-earned point"), Progression->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    const float RuleMultiplier = NativeBase + (BeforeMultiplier - NativeBase) * .5f;
    TestEqual(TEXT("Native critical chance readout is guaranteed"), Attr->GetCriticalChance(), 1.0f);
    TestEqual(TEXT("Actual gear bonus is halved"), Attr->GetCriticalMultiplier(), RuleMultiplier, .0001f);
    const auto Rows = BreakerSkillProjection::CurrentTotals(BreakerSkillProjection::MakeSnapshot(Progression, Attr));
    const auto* CritRow = Rows.FindByPredicate([](const FBreakerStatLine& Row) { return Row.Label == TEXT("CRIT CHANCE"); });
    if (TestNotNull(TEXT("Critical projection row"), CritRow)) TestEqual(TEXT("Projection agrees with actual guarantee"), CritRow->Before, 1.0f);
    auto* Target = World->SpawnActor<AActor>(); auto* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body); Target->SetRootComponent(Body); Body->SetSphereRadius(50);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); Body->RegisterComponent();
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim); Target->SetActorLocation(Eye + Aim.Vector() * 500);
    auto* Combat = NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Target); Target->AddInstanceComponent(Status); Status->RegisterComponent(); Status->SetComponentTickEnabled(false);
    auto* Observer = NewObject<UBreakerReactionRuntimeObserver>(Target); Combat->OnDamageTaken.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnHit);
    auto Advance = [&](int32 Steps) { for (int32 I = 0; I < Steps; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); } };
    auto* Weapon = Player->GetWeapon();
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    Weapon->WeaponDefinition->Recoil.BloomPerShotDegrees = 0; Weapon->WeaponDefinition->BleedChance = 0; Weapon->ResetAmmunition();
    Advance(4); const int32 Ammo = Weapon->GetMagazineAmmo(); Weapon->StartFire(); Weapon->StopFire();
    TestEqual(TEXT("Real rifle pull spends one round"), Weapon->GetMagazineAmmo(), Ammo - 1);
    if (!TestTrue(TEXT("Rifle delivered a real hit"), !Observer->Hits.IsEmpty())) return false;
    TestTrue(TEXT("Rifle actual hit is critical"), Observer->Hits.Last().Result.bCritical);
    Observer->Hits.Reset();
    // With no controller, this ability's native view starts at actor location;
    // the weapon uses actor eyes. Aim the fixture at the real ability trace.
    Target->SetActorLocation(Player->GetActorLocation() + Player->GetControlRotation().Vector() * 500);
    const auto Siphon = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Siphon::StaticClass(), 1));
    const float ManaBefore = Attr->GetClassResource();
    if (!TestTrue(TEXT("Paid Siphon activates"), ASC->TryActivateAbility(Siphon))) return false;
    BreakerResolvePendingCast(World, Player);
    for (int32 I = 0; I < 20 && Observer->Hits.IsEmpty(); ++I) Advance(1);
    TestTrue(TEXT("Native ability debit occurred"), Attr->GetClassResource() < ManaBefore);
    if (!TestTrue(TEXT("Paid channel delivered"), !Observer->Hits.IsEmpty())) return false;
    TestTrue(TEXT("Paid ability hit is critical"), Observer->Hits.Last().Result.bCritical); ASC->CancelAbilityHandle(Siphon);
    Observer->Hits.Reset(); Status->ConsumeAllStatuses();
    Player->GetMana()->AdvanceLoop(20);
    const auto Fracture = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
    const float FractureMana = Attr->GetClassResource();
    if (!TestTrue(TEXT("Paid Fracture emits projectile"), ASC->TryActivateAbility(Fracture))) return false;
    BreakerResolvePendingCast(World, Player);
    TestTrue(TEXT("Projectile cast debits resource"), Attr->GetClassResource() < FractureMana);
    if (!BreakerWaitForFractureCast(World, ASC, Fracture)) return false;
    ABreakerProjectileBase* Projectile = nullptr;
    for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It)
        if (It->GetOwner() == Player && !It->HasImpacted()) { Projectile = *It; break; }
    if (!TestNotNull(TEXT("Actual emitted projectile"), Projectile)) return false;
    TestTrue(TEXT("Projectile carries forced critical rule"), Projectile->GetProjectileDamage().bForceCriticalStrike);
    if (!TestTrue(TEXT("First cycle carries physical ailment"), !Projectile->GetImpactStatuses().IsEmpty())) return false;
    const auto Carried = Projectile->GetImpactStatuses()[0];
    TestTrue(TEXT("Physical ailment owns critical roll at paid cast"), Carried.Spec.Snapshot.bRolledCritical);
    TestEqual(TEXT("Ailment snapshot owns reduced bonus"), Carried.Spec.Snapshot.CriticalMultiplier, RuleMultiplier, .0001f);
    if (!TestTrue(TEXT("Real low-level respec succeeds while projectile exists"), Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec restores full gear critical bonus"), Attr->GetCriticalMultiplier(), BeforeMultiplier, .0001f);
    FBreakerDamageRequest Fresh; UBreakerDamageLibrary::FillSourcePools(Attr, EBreakerDamageDelivery::Ability, Fresh);
    TestFalse(TEXT("New emission after respec loses forced critical"), Fresh.bForceCriticalStrike);
    Projectile->Impact(Target, Target->GetActorLocation());
    if (!TestTrue(TEXT("Stored projectile resolves actual impact"), !Observer->Hits.IsEmpty())) return false;
    TestTrue(TEXT("Stored projectile keeps guarantee after respec"), Observer->Hits[0].Result.bCritical);
    if (!TestEqual(TEXT("Physical ailment actually applied"), Status->GetActiveStatuses().Num(), 1)) return false;
    Observer->Hits.Reset(); Status->AdvanceStatuses(Carried.Spec.TickInterval);
    if (!TestTrue(TEXT("Native periodic snapshot ticks"), !Observer->Hits.IsEmpty())) return false;
    TestTrue(TEXT("Periodic tick keeps original critical roll after respec"), Observer->Hits[0].Result.bCritical);
    TestEqual(TEXT("Periodic damage uses reduced snapshotted multiplier once"), Observer->Hits[0].Result.RawDamage,
        Carried.Spec.BaseDamagePerTick * Carried.Spec.Snapshot.SourcePower * RuleMultiplier, .01f);
    return true;
}
#endif
