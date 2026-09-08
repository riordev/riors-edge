#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreLoadoutRuntimeTest, "RiorsEdge.Weapons.CoreLoadoutRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreLoadoutRuntimeTest::RunTest(const FString& Parameters)
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
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr); Attr->SetCriticalChance(0);
    Player->GetCombat()->BindAttributes(Attr);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster;
    auto* Tree = NewObject<UBreakerProgressionTree>(Class); Tree->TreeId = TEXT("Test.Core.Loadout");
    Tree->Currency = EBreakerPointCurrency::CorePoints; Class->BranchTrees.Add(Tree);
    auto Make = [&](const TCHAR* Id, const TCHAR* Tag)
    {
        auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = Id; Node->Currency = Tree->Currency;
        Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(Tag)); Tree->Nodes.Add(Node); return Node;
    };
    auto* Quickdraw = Make(TEXT("Test.Core.Loadout.Quickdraw"), TEXT("Progression.Node.Core.Quickdraw"));
    auto* TwoGuns = Make(TEXT("Test.Core.Loadout.TwoGuns"), TEXT("Progression.Node.Core.TwoGuns"));
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, Progression->ExperienceCurve));
    auto* Weapon = Player->GetWeapon(); Weapon->SetSlotArchetype(2, EBreakerWeaponArchetype::Rifle);
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = 0; Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    Weapon->WeaponDefinition->Recoil.BloomPerShotDegrees = 0; Weapon->WeaponDefinition->BleedChance = 0;
    Weapon->ResetAmmunition();
    // No player BeginPlay/save loading. Advance real timers and explicitly run
    // the native component tick that normally owns the holstered transfer.
    auto Clock = [&](float Seconds)
    {
        const int32 Steps = FMath::RoundToInt(Seconds * 100);
        for (int32 I = 0; I < Steps; ++I)
        {
            ++GFrameCounter; World->Tick(LEVELTICK_All, .01f);
            Weapon->TickComponent(.01f, LEVELTICK_All, nullptr);
        }
    };
    auto Fire = [&]()
    {
        Clock(.2f); const int32 Before = Weapon->GetMagazineAmmo(); Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("Native trigger debits one round"), Weapon->GetMagazineAmmo(), Before - 1);
    };
    auto Swap = [&](int32 Slot) { Weapon->EquipSlot(Slot); Clock(1); TestFalse(TEXT("Swap completes"), Weapon->IsSwapping()); };
    auto* Target = World->SpawnActor<AActor>(); auto* Shape = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Shape); Target->SetRootComponent(Shape); Shape->SetSphereRadius(50);
    Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
    Shape->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); Shape->RegisterComponent();
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim); Target->SetActorLocation(Eye + Aim.Vector() * 500);
    auto* TargetCombat = NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(TargetCombat); TargetCombat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000); TargetCombat->BindAttributes(Health);
    FText Reason;
    if (!Progression->PurchaseNode(Tree, Quickdraw->NodeId, Reason)) return false;
    auto Damage = [&]() { const float Before = Health->GetHealth(); Fire(); return Before - Health->GetHealth(); };
    const float Ordinary = Damage(); TestTrue(TEXT("Ordinary rifle hits target"), Ordinary > 0);
    Swap(2); Weapon->EquipSlot(1);
    const int32 BeforeRefusal = Weapon->GetMagazineAmmo(); Weapon->StartFire(); Weapon->StopFire();
    TestEqual(TEXT("Swap-time refusal spends neither ammo nor first shot"), Weapon->GetMagazineAmmo(), BeforeRefusal);
    Clock(1);
    const float ExpectedBonus = Weapon->GetScaledBaseDamage() * .30f;
    TestEqual(TEXT("First swapped shot joins Increased bucket"), Damage(), Ordinary + ExpectedBonus, .02f);
    TestEqual(TEXT("Second shot cannot reuse bonus"), Damage(), Ordinary, .02f);
    Swap(2); Swap(1); Target->SetActorEnableCollision(false); Fire(); Target->SetActorEnableCollision(true);
    TestEqual(TEXT("A missed first shot consumes entitlement"), Damage(), Ordinary, .02f);
    Swap(2); Swap(1);
    if (!Progression->RespecCore(Reason) || !Progression->PurchaseNode(Tree, Quickdraw->NodeId, Reason)) return false;
    TestEqual(TEXT("Respec and rebuy cannot restore armed shot"), Damage(), Ordinary, .02f);
    Swap(2); Swap(1); Player->GetCombat()->OnDeath.Broadcast();
    TestEqual(TEXT("Death event discards armed shot"), Damage(), Ordinary, .02f);

    if (!Progression->RespecCore(Reason)) return false;
    Weapon->ResetAmmunition(); const int32 Capacity = Weapon->GetEffectiveMagazineSize();
    for (int32 I = 0; I < Capacity; ++I) Fire();
    const int32 EmptyReserve = Weapon->GetReserveAmmo();
    Swap(2); Clock(6); Weapon->EquipSlot(1);
    TestEqual(TEXT("Unowned holster never reloads"), Weapon->GetMagazineAmmo(), 0); Clock(1);
    if (!Progression->PurchaseNode(Tree, TwoGuns->NodeId, Reason)) return false;
    TestTrue(TEXT("Owned holstered rule enables idle ticking"), Weapon->IsComponentTickEnabled());
    Weapon->EquipSlot(2); Clock(3); Weapon->EquipSlot(1);
    TestEqual(TEXT("Half the six-second clock transfers half a magazine"), Weapon->GetMagazineAmmo(), Capacity / 2);
    TestEqual(TEXT("Partial transfer conserves rounds"), Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo(), EmptyReserve);
    Clock(1); Weapon->EquipSlot(2); Clock(6); Weapon->EquipSlot(1);
    TestEqual(TEXT("Holstered magazine fills from actual reserve"), Weapon->GetMagazineAmmo(), Capacity);
    TestEqual(TEXT("Full transfer conserves rounds"), Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo(), EmptyReserve); Clock(1);
    Fire(); const int32 Missing = Weapon->GetMagazineAmmo();
    Weapon->EquipSlot(2);
    if (!Progression->RespecCore(Reason)) return false;
    Clock(6); Weapon->EquipSlot(1);
    TestEqual(TEXT("Respec stops a pending holstered reload"), Weapon->GetMagazineAmmo(), Missing);
    return true;
}
#endif
