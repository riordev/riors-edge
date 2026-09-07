#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPrimaryChannelsRuntimeTest, "RiorsEdge.Items.PrimaryChannelsRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerPrimaryChannelsRuntimeTest::RunTest(const FString& Parameters)
{
    auto RollChannel = [&](FName Id, FBreakerItemInstance& Item)
    {
        // Endgame item-level fixture gives the restricted Pierce row access
        // to its real positive tiers. This does not claim fresh-player access.
        for (int32 Seed = 1; Seed <= 16384; ++Seed)
        {
            Item = UBreakerLootLibrary::RollItem(TEXT("Primary.Channels"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Exceptional, 120, Seed);
            if (Item.WeaponArchetype != EBreakerWeaponArchetype::Rifle) continue;
            if (!Item.Affixes.ContainsByPredicate([Id](const FBreakerRolledAffix& A) { return A.AffixId == Id && A.Value > 0; })) continue;
            // Preserve the actual tested row/tier/value, isolating its
            // consumer from unrelated damage and handling lines.
            for (auto& Affix : Item.Affixes) if (Affix.AffixId != Id) Affix.Value = 0;
            // Pierce's count comes from its exact tier, not serialized Value;
            // zeroing an unrelated row cannot disable that consumer.
            if (Id != FName(TEXT("Weapon.Pierce")))
                Item.Affixes.RemoveAll([](const FBreakerRolledAffix& A) { return A.AffixId == FName(TEXT("Weapon.Pierce")); });
            return true;
        }
        return false;
    };
    FBreakerItemInstance Accuracy, Pierce;
    if (!TestTrue(TEXT("real ordinary sustained accuracy roll"), RollChannel(TEXT("Weapon.SustainedAccuracy"), Accuracy))
        || !TestTrue(TEXT("real positive hitscan Pierce roll"), RollChannel(TEXT("Weapon.Pierce"), Pierce))) return false;
    FBreakerItemInstance Control = Accuracy;
    for (auto& Affix : Control.Affixes) Affix.Value = 0;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated channel world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real player without save-loading BeginPlay"), Player)) return false;
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    UBreakerEquipmentComponent* Equipment = Player->GetEquipment();
    Equipment->BindAttributes(Player->GetAttributes());
    UBreakerWeaponComponent* Weapon = Player->GetWeapon();
    if (!TestTrue(TEXT("control directly equips in explicit endgame consumer fixture"), Equipment->EquipItem(Control))) return false;
    Weapon->SyncArchetypesToEquipment();
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    UBreakerWeaponDefinition* Definition = Weapon->WeaponDefinition;
    Definition->bProjectile = false; Definition->PelletsPerShot = 1; Definition->BleedChance = 0;
    Definition->HipSpreadDegrees = 1; Definition->AimSpreadDegrees = 1;
    Definition->Recoil.FirstShotSpreadMultiplier = 1;
    Definition->Recoil.BloomPerShotDegrees = 1;
    Definition->Recoil.MaxBloomDegrees = 10;
    Definition->Recoil.BloomRecoveryDegreesPerSecond = 0;
    Definition->Recoil.BurstResetSeconds = 10;
    Definition->MaximumRange = 2000;
    auto Advance = [&](int32 Steps) { for (int32 I = 0; I < Steps; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.05f); } };
    auto Fire = [&]
    {
        const int32 Before = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        return TestEqual(TEXT("actual trigger consumes one fresh round"), Weapon->GetMagazineAmmo(), Before - 1);
    };
    Weapon->ResetAmmunition();
    const float FirstControl = Weapon->GetNextShotSpreadDegrees();
    if (!TestTrue(TEXT("accuracy equips"), Equipment->EquipItem(Accuracy))) return false;
    TestEqual(TEXT("accuracy cannot reduce first-shot base cone"), Weapon->GetNextShotSpreadDegrees(), FirstControl);
    if (!Fire()) return false;
    TestEqual(TEXT("actual first shot retains its unmodified base cone"), Weapon->GetLastShot().SpreadDegrees, FirstControl);
    const float TightBloom = Weapon->GetNextShotSpreadDegrees();
    if (!TestTrue(TEXT("control replacement removes accuracy"), Equipment->EquipItem(Control))) return false;
    const float OrdinaryBloom = Weapon->GetNextShotSpreadDegrees();
    TestTrue(TEXT("equipped row tightens accumulated bloom in HUD prediction"), TightBloom < OrdinaryBloom);
    TestTrue(TEXT("tightened bloom still preserves nonzero base cone"), TightBloom > FirstControl);
    if (!TestTrue(TEXT("accuracy re-equips for real sustained shot"), Equipment->EquipItem(Accuracy))) return false;
    Advance(4);
    const float Predicted = Weapon->GetNextShotSpreadDegrees();
    if (!Fire()) return false;
    TestEqual(TEXT("actual sustained shot uses the same tightened cone as HUD prediction"), Weapon->GetLastShot().SpreadDegrees, Predicted);
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim);
    Weapon->EquipSlot(2); Advance(14);
    if (!Fire()) return false;
    const float SecondaryWithPrimary = Weapon->GetNextShotSpreadDegrees();
    if (!TestTrue(TEXT("Primary accuracy removed while Secondary active"), Equipment->EquipItem(Control))) return false;
    TestEqual(TEXT("Secondary bloom is independent of Primary accuracy"), Weapon->GetNextShotSpreadDegrees(), SecondaryWithPrimary);

    Weapon->EquipSlot(1); Advance(14);
    Definition->HipSpreadDegrees = 0; Definition->AimSpreadDegrees = 0;
    Definition->Recoil.BloomPerShotDegrees = 0;
    auto SpawnTarget = [&](float Distance, UBreakerAttributeSet*& Health)
    {
        AActor* Target = World->SpawnActor<AActor>();
        if (!Target) return Target;
        USphereComponent* Body = NewObject<USphereComponent>(Target);
        Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
        Body->SetSphereRadius(30); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Body->SetCollisionResponseToAllChannels(ECR_Ignore);
        Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); Body->RegisterComponent();
        UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
        Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
        Health = NewObject<UBreakerAttributeSet>(Target);
        // Three ilvl120 Rifle hits exceed one million health; retain a live
        // target for the subsequent low-level Secondary isolation check.
        Health->ApplyMaxHealth(10000000); Health->ApplyHealth(10000000); Combat->BindAttributes(Health);
        Target->SetActorLocation(Eye + Aim.Vector() * Distance);
        return Target;
    };
    UBreakerAttributeSet* FirstHealth = nullptr; UBreakerAttributeSet* SecondHealth = nullptr;
    AActor* First = SpawnTarget(400, FirstHealth); AActor* Second = SpawnTarget(800, SecondHealth);
    if (!TestNotNull(TEXT("first real collision target"), First) || !TestNotNull(TEXT("second real collision target"), Second)) return false;
    const float FirstBefore = FirstHealth->GetHealth(); const float SecondBefore = SecondHealth->GetHealth();
    if (!Fire()) return false;
    TestTrue(TEXT("baseline damages first target"), FirstHealth->GetHealth() < FirstBefore);
    TestEqual(TEXT("baseline cannot damage second target"), SecondHealth->GetHealth(), SecondBefore);
    if (!TestTrue(TEXT("real Pierce row equips"), Equipment->EquipItem(Pierce))) return false;
    Advance(4);
    if (!Fire()) return false;
    TestTrue(TEXT("equipped Pierce damages second real target"), SecondHealth->GetHealth() < SecondBefore);
    TestTrue(TEXT("actual shot records its continuation"), Weapon->GetLastShot().SecondaryImpacts.Num() > 0);
    Second->SetActorLocation(Eye + Aim.Vector() * 2200);
    const float OutsideBefore = SecondHealth->GetHealth();
    Advance(4);
    if (!Fire()) return false;
    TestEqual(TEXT("piercing does not restart maximum range after first impact"), SecondHealth->GetHealth(), OutsideBefore);
    Second->SetActorLocation(Eye + Aim.Vector() * 800);
    Weapon->EquipSlot(2); Advance(14);
    const float SecondaryTargetBefore = SecondHealth->GetHealth();
    const float SecondaryFrontBefore = FirstHealth->GetHealth();
    if (!TestTrue(TEXT("endgame Primary shots leave a living Secondary target"), SecondaryFrontBefore > 0)) return false;
    if (!Fire()) return false;
    TestTrue(TEXT("Secondary actually hits the first target"), FirstHealth->GetHealth() < SecondaryFrontBefore);
    TestEqual(TEXT("Primary Pierce cannot continue Secondary shot"), SecondHealth->GetHealth(), SecondaryTargetBefore);
    return true;
}
#endif
