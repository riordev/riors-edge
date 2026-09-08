#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerRocketProjectile.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreFanRuntimeTest, "RiorsEdge.Weapons.CoreFanRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreFanRuntimeTest::RunTest(const FString& Parameters)
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
    Player->GetCombat()->BindAttributes(Attr); Player->GetEquipment()->BindAttributes(Attr);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster;
    auto* Tree = NewObject<UBreakerProgressionTree>(Class); Tree->TreeId = TEXT("Test.Core.Fan"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    Class->BranchTrees.Add(Tree);
    auto* Fan = NewObject<UBreakerProgressionNode>(Tree); Fan->NodeId = TEXT("Test.Core.Fan.Rule"); Fan->Currency = Tree->Currency;
    Fan->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Fan"))); Tree->Nodes.Add(Fan);
    auto* Numeric = NewObject<UBreakerProgressionNode>(Tree); Numeric->NodeId = TEXT("Test.Core.Fan.Numeric"); Numeric->Currency = Tree->Currency;
    FBreakerNodeEffect Spread; Spread.StatTarget = EBreakerNodeStatTarget::WeaponSpread; Spread.StatBucket = EBreakerNodeStatBucket::IncreasedPercent; Spread.ValuePerRank = 100;
    Numeric->Effects.Add(Spread);
    FBreakerNodeEffect Multishot; Multishot.StatTarget = EBreakerNodeStatTarget::ProjectileCount; Multishot.ValuePerRank = .5f;
    Numeric->Effects.Add(Multishot); Tree->Nodes.Add(Numeric);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, Progression->ExperienceCurve));
    auto* Weapon = Player->GetWeapon();
    auto Clock = [&](float Seconds) { for (int32 I=0; I<FMath::RoundToInt(Seconds*100); ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.01f); } };
    auto SetArchetype = [&](EBreakerWeaponArchetype Archetype)
    {
        Weapon->WeaponDefinition = nullptr; Weapon->SetSlotArchetype(1, Archetype); Clock(1);
        Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
        Weapon->WeaponDefinition->BleedChance = 0; Weapon->ResetAmmunition();
    };
    auto Fire = [&]()
    {
        // Rocket's authored interval is longer than one second. Advance its
        // real cadence rather than repeatedly testing a refused early pull.
        Clock(FMath::Max(1.0f, 60.0f / FMath::Max(1.0f, Weapon->GetEffectiveRoundsPerMinute(Weapon->GetActiveDefinition())) + .05f));
        const float Predicted = Weapon->GetNextShotSpreadDegrees(); const int32 Ammo = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("Every native pull spends one round regardless of multiplicity"), Weapon->GetMagazineAmmo(), Ammo-1);
        TestEqual(TEXT("Predicted cone equals actual fired cone"), Weapon->GetLastShot().SpreadDegrees, Predicted, .001f);
    };
    FText Reason; SetArchetype(EBreakerWeaponArchetype::Rifle);
    Weapon->SetAiming(true); Clock(1);
    const float BaselineAimed = Weapon->GetNextShotSpreadDegrees();
    Fire(); TestEqual(TEXT("Baseline rifle emits one native pellet"), Weapon->GetLastShot().Pellets.Num(), Weapon->GetActiveDefinition()->PelletsPerShot);
    if (!TestTrue(TEXT("Fan uses actual earned purchase"), Progression->PurchaseNode(Tree, Fan->NodeId, Reason))) return false;
    Fire(); TestEqual(TEXT("Fan emits two extra rifle projectiles"), Weapon->GetLastShot().Pellets.Num(), Weapon->GetActiveDefinition()->PelletsPerShot+2);
    TestTrue(TEXT("Fan cannot narrow below authored hip spread while aiming"), Weapon->GetLastShot().SpreadDegrees >= Weapon->GetActiveDefinition()->HipSpreadDegrees-.001f);
    TestTrue(TEXT("Aimed baseline demonstrates a real forfeited reduction"), BaselineAimed < Weapon->GetLastShot().SpreadDegrees);
    SetArchetype(EBreakerWeaponArchetype::Shotgun); Fire();
    TestEqual(TEXT("Shotgun retains authored pellet count plus two"), Weapon->GetLastShot().Pellets.Num(), Weapon->GetActiveDefinition()->PelletsPerShot+2);
    if (!TestTrue(TEXT("Numeric lines also use earned purchase"), Progression->PurchaseNode(Tree, Numeric->NodeId, Reason))) return false;
    Fire();
    TestTrue(TEXT("Core spread reduction cannot bypass Fan hip floor"), Weapon->GetLastShot().SpreadDegrees >= Weapon->GetActiveDefinition()->HipSpreadDegrees-.001f);
    // Remove fractional carry from this pull by making the matching half pull.
    Fire();
    if (!TestTrue(TEXT("Real respec withdraws Fan and numeric lines"), Progression->RespecCore(Reason))) return false;
    SetArchetype(EBreakerWeaponArchetype::Rifle); Weapon->SetAiming(true); Clock(1); Fire();
    TestEqual(TEXT("Respec restores baseline multiplicity"), Weapon->GetLastShot().Pellets.Num(), Weapon->GetActiveDefinition()->PelletsPerShot);
    TestTrue(TEXT("Respec restores aimed narrowing"), Weapon->GetLastShot().SpreadDegrees < Weapon->GetActiveDefinition()->HipSpreadDegrees);
    if (!Progression->PurchaseNode(Tree, Fan->NodeId, Reason)) return false;
    FBreakerItemInstance Item;
    for (int32 Seed=1; Seed<=4096; ++Seed)
    {
        Item = UBreakerLootLibrary::RollItem(TEXT("Test.Core.Fan.Ramp"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
        if (Item.Affixes.ContainsByPredicate([](const FBreakerRolledAffix& Affix) { return Affix.AffixId == FName(TEXT("Weapon.DamageRamp")); })) break;
    }
    if (!TestTrue(TEXT("Real ordinary ramp roll acquired"), Item.Affixes.ContainsByPredicate([](const FBreakerRolledAffix& Affix) { return Affix.AffixId == FName(TEXT("Weapon.DamageRamp")); }))
        || !TestTrue(TEXT("Real ramp Primary equipped"), Player->GetEquipment()->EquipItem(Item))) return false;
    SetArchetype(EBreakerWeaponArchetype::Rocket);
    auto* Victim = World->SpawnActor<AActor>(); auto* Shape=NewObject<USphereComponent>(Victim); Victim->AddInstanceComponent(Shape); Victim->SetRootComponent(Shape);
    Shape->SetSphereRadius(60); Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Shape->SetCollisionResponseToAllChannels(ECR_Ignore); Shape->RegisterComponent();
    const FVector Impact(10000,0,0); Victim->SetActorLocation(Impact);
    auto* VictimCombat=NewObject<UBreakerCombatComponent>(Victim); Victim->AddInstanceComponent(VictimCombat); VictimCombat->RegisterComponent();
    auto* Health=NewObject<UBreakerAttributeSet>(Victim); Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000); VictimCombat->BindAttributes(Health);
    auto Launch = [&]()
    {
        if (Weapon->GetMagazineAmmo()==0) { Weapon->StartReload(); Clock(5); }
        Fire(); TArray<ABreakerRocketProjectile*> Rockets;
        for (TActorIterator<ABreakerRocketProjectile> It(World); It; ++It)
            if (It->GetOwner()==Player && !It->HasExploded())
            {
                Rockets.Add(*It); It->SetActorEnableCollision(false);
                if (auto* Movement=It->FindComponentByClass<UProjectileMovementComponent>()) { Movement->StopMovementImmediately(); Movement->Deactivate(); }
            }
        return Rockets;
    };
    auto Miss = [&](ABreakerRocketProjectile* Rocket) { Rocket->Explode(FVector(100000,0,0)); };
    auto Rockets=Launch(); if (!TestEqual(TEXT("Fan launches three actual rocket actors"), Rockets.Num(),3)) return false;
    const FVector FirstVelocity=Rockets[0]->GetActorForwardVector();
    TestTrue(TEXT("Rocket siblings have independently sampled spread directions"), !FirstVelocity.Equals(Rockets[1]->GetActorForwardVector(),.000001f)
        || !FirstVelocity.Equals(Rockets[2]->GetActorForwardVector(),.000001f));
    Rockets[0]->Explode(Impact,Victim); Rockets[1]->Explode(Impact,Victim); Miss(Rockets[2]);
    TestEqual(TEXT("Multiple sibling hits grant one ramp stack"), Weapon->GetDamageRampStacks(),1);
    const float AfterExplosion = Health->GetHealth(); Rockets[0]->Explode(Impact,Victim);
    TestEqual(TEXT("Duplicate sibling impact cannot deal damage twice"), Health->GetHealth(), AfterExplosion);
    TestEqual(TEXT("Duplicate sibling impact cannot add a ramp stack"), Weapon->GetDamageRampStacks(),1);
    Rockets=Launch(); if (!TestEqual(TEXT("Second paid rocket pull also emits three"),Rockets.Num(),3)) return false;
    Miss(Rockets[0]); TestEqual(TEXT("First sibling miss preserves previous ramp"),Weapon->GetDamageRampStacks(),1);
    Rockets[1]->Explode(Impact,Victim); TestEqual(TEXT("Later sibling hit advances once"),Weapon->GetDamageRampStacks(),2);
    Miss(Rockets[2]); TestEqual(TEXT("Remaining sibling miss cannot reset successful pull"),Weapon->GetDamageRampStacks(),2);
    Rockets=Launch(); if (Rockets.Num()!=3) return false;
    Miss(Rockets[0]); Miss(Rockets[1]); TestEqual(TEXT("Incomplete all-miss pull retains ramp"),Weapon->GetDamageRampStacks(),2);
    // Give this native projectile its real lifecycle before destruction; the
    // fixture never runs the player's save-loading BeginPlay.
    if (!Rockets[2]->HasActorBegunPlay()) Rockets[2]->DispatchBeginPlay();
    Rockets[2]->Destroy(); TestEqual(TEXT("Last destroyed sibling settles failed pull and resets ramp"),Weapon->GetDamageRampStacks(),0);
    if (!Progression->PurchaseNode(Tree,Numeric->NodeId,Reason)) return false;
    Rockets=Launch(); if (!TestEqual(TEXT("First half-projectile pull emits three"),Rockets.Num(),3)) return false;
    for (auto* Rocket:Rockets) Miss(Rocket);
    Rockets=Launch(); if (!TestEqual(TEXT("Second half-projectile pull emits fourth rocket"),Rockets.Num(),4)) return false;
    for (auto* Rocket:Rockets) Miss(Rocket);
    if (!Progression->RespecCore(Reason)) return false;
    Rockets=Launch(); TestEqual(TEXT("Respec restores one real rocket"),Rockets.Num(),1);
    for (auto* Rocket:Rockets) Miss(Rocket);
    return true;
}
#endif
