#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusRules.h"
#include "Combat/BreakerProjectileBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerExperience.h"
#include "TimerManager.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerWeaponAvoidedStatusRuntimeTest,
    "RiorsEdge.Weapons.AvoidedStatusRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerWeaponAvoidedStatusRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 SavedFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = SavedFrame; };
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto MakePlayer = [&](FVector Location, FRotator Rotation, bool bDurableReceiver)
    {
        auto* Player = World->SpawnActor<ABreakerCharacter>(Location, Rotation, Spawn);
        if (!Player) return Player;
        Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        // Capture the durable receiver base before progression binds it. Parry
        // legitimately recomposes attributes when its success condition changes.
        if (bDurableReceiver)
        {
            Player->GetAttributes()->ApplyMaxHealth(10000);
            Player->GetAttributes()->ApplyHealth(10000);
        }
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        Player->GetCombat()->BeginPlay();
        Player->FindComponentByClass<UBreakerStatusComponent>()->SetComponentTickEnabled(false);
        return Player;
    };
    auto* Source = MakePlayer(FVector::ZeroVector, FRotator::ZeroRotator, false);
    auto* First = MakePlayer(FVector(500, 0, 0), FRotator(0, 180, 0), true);
    auto* Second = MakePlayer(FVector(800, 3000, 0), FRotator(0, 180, 0), true);
    if (!Source || !First || !Second) return false;
    // Durable native receivers isolate status eligibility from lethal damage.
    // No player offensive stat, status chance, ammo budget or resource is granted.
    for (auto* Target : {First, Second})
    {
        Target->GetCombat()->BlockChance = 0; Target->GetCombat()->DodgeChance = 0;
    }
    auto* Progression = Source->GetProgression();
    if (!TestTrue(TEXT("real permanent Caster supplies status-income consumer"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    // Explicit level entitlement fixture, not a campaign leveling claim.
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(13, Progression->ExperienceCurve));
    FText Reason;
    auto Buy = [&](const TCHAR* Id)
    {
        const bool bPurchased = Progression->PurchaseNode(UBreakerProgressionLibrary::GetCoreSliceTree(), Id, Reason);
        return TestTrue(*FString::Printf(TEXT("%s: %s"), Id, *Reason.ToString()), bPurchased);
    };
    for (const TCHAR* Id : {TEXT("Core.Elements.Conductive"), TEXT("Core.Elements.ChargeUp"), TEXT("Core.Elements.Threshold")})
        if (!Buy(Id)) return false;
    TestEqual(TEXT("Threshold costs four ordinary Core points"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), 9);
    auto* TargetProgression = First->GetProgression();
    TargetProgression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(4, TargetProgression->ExperienceCurve));
    for (const TCHAR* Id : {TEXT("Core.Bulwark.SetStance"), TEXT("Core.Bulwark.Read"), TEXT("Core.Bulwark.Parry")})
        if (!TestTrue(TEXT("target purchases real Parry path"), TargetProgression->PurchaseNode(UBreakerProgressionLibrary::GetCoreSliceTree(), Id, Reason))) return false;
    First->GetCombat()->BlockChance = 0; // Isolate active Parry from the purchased passive block chance.
    for (auto* Target : {First, Second})
    {
        // Progression recomposes the target chassis, so observation health is
        // established only after its real purchases have completed.
        Target->GetAttributes()->ApplyMaxHealth(10000); Target->GetAttributes()->ApplyHealth(10000);
    }

    FBreakerItemInstance SMG;
    for (int32 Seed = 1; Seed <= 4096; ++Seed)
    {
        SMG = UBreakerLootLibrary::RollItem(TEXT("Weapon.Avoidance"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
        if (SMG.WeaponArchetype == EBreakerWeaponArchetype::SMG) break;
    }
    if (!TestTrue(TEXT("actual ordinary level-one SMG roll"), SMG.IsValid() && SMG.WeaponArchetype == EBreakerWeaponArchetype::SMG)) return false;
    for (auto& Affix : SMG.Affixes) Affix.Value = 0; // Isolate co-rolls; no added offense or status chance.
    Source->GetEquipment()->BindAttributes(Source->GetAttributes());
    if (!TestTrue(TEXT("actual SMG equips"), Source->GetEquipment()->EquipItem(SMG))) return false;
    auto* Weapon = Source->GetWeapon(); Weapon->SyncArchetypesToEquipment();
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = 0; Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    Weapon->WeaponDefinition->Recoil.BloomPerShotDegrees = 0; // Exact collision fixture, not an accuracy claim.
    TestEqual(TEXT("shipped Bleed chance stays one quarter"), Weapon->WeaponDefinition->BleedChance, .25f);
    TestEqual(TEXT("purchased path does not inflate status chance"), Progression->GetNodeStats().StatusChanceMultiplier, 1.0f);
    Weapon->ResetAmmunition(); // Normal starting load, once; later reloads spend its reserve.
    auto* Mana = Source->GetMana(); Mana->BindAttributes(Source->GetAttributes());
    Mana->AdvanceLoop(20); Mana->SetComponentTickEnabled(false);
    Mana->PassiveRegenPerSecond = 0; // Isolate hit/status income from passive regeneration.
    if (!TestTrue(TEXT("spend existing Mana to leave income headroom"), Mana->TrySpendMana(50))) return false;
    auto* FirstStatus = First->FindComponentByClass<UBreakerStatusComponent>();
    auto* SecondStatus = Second->FindComponentByClass<UBreakerStatusComponent>();
    const auto Bleed = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
    const auto Poison = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
    FVector Eye; FRotator Facing; Source->GetActorEyesViewPoint(Eye, Facing);
    const FVector FirstPosition = Eye + FVector(500, 0, 0);
    const FVector SecondPosition = Eye + FVector(800, 0, 0);
    First->SetActorLocation(FirstPosition); Second->SetActorLocation(SecondPosition + FVector(0, 3000, 0));
    FHitResult Probe; FCollisionQueryParams Query(SCENE_QUERY_STAT(BreakerAvoidedStatusFixture), true, Source);
    if (!TestTrue(TEXT("native weapon ray actually reaches first receiver"), World->LineTraceSingleByChannel(Probe, Eye, Eye + FVector(1500, 0, 0), ECC_GameTraceChannel2, Query) && Probe.GetActor() == First)) return false;
    auto Advance = [&](int32 Steps)
    {
        for (int32 Step = 0; Step < Steps; ++Step)
        {
            ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
            if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
        }
    };
    auto Fire = [&]()
    {
        if (Weapon->GetMagazineAmmo() == 0) { Weapon->StartReload(); Advance(40); }
        const int32 Ammo = Weapon->GetMagazineAmmo();
        Weapon->StartFire();
        Weapon->StopFire();
        return TestEqual(TEXT("every probe fires and spends an actual round"), Weapon->GetMagazineAmmo(), Ammo - 1);
    };
    auto Shots = [&](int32 Count)
    {
        for (int32 Shot = 0; Shot < Count; ++Shot) { Advance(3); if (!Fire()) return false; }
        return true;
    };
    if (!Shots(4)) return false;
    if (!TestTrue(TEXT("four paid quarter-chance hits earn actual Bleed through Threshold"), FirstStatus->HasStatus(Bleed))) return false;
    // Four actual weapon hits plus the new status queue eight Mana. The six/s
    // meter needs two seconds to settle that existing income before avoidance.
    const float BeforeIncome = Mana->GetMana(); Mana->AdvanceLoop(2);
    TestTrue(TEXT("accepted status/hits retain normal earned income"), Mana->GetMana() > BeforeIncome);
    FirstStatus->ConsumeAllStatuses();
    First->GetCombat()->DodgeChance = 1;
    const float DodgeHealth = First->GetAttributes()->GetHealth(), DodgeMana = Mana->GetMana();
    if (!Shots(4)) return false;
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("dodged rounds cause no health damage"), First->GetAttributes()->GetHealth(), DodgeHealth);
    TestFalse(TEXT("dodged rounds cannot apply Bleed"), FirstStatus->HasStatus(Bleed));
    TestEqual(TEXT("dodged rounds give no status income"), Mana->GetMana(), DodgeMana, .001f);
    First->GetCombat()->DodgeChance = 0;
    const float ParryMana = Mana->GetMana();
    for (int32 Shot = 0; Shot < 4; ++Shot)
    {
        Advance(42);
        if (!TestTrue(TEXT("actual purchased Parry opens before round"), First->GetCombat()->TryParry())) return false;
        if (!Fire()) return false;
        TestFalse(TEXT("incoming shot consumes actual Parry window"), First->GetCombat()->IsParryActive());
        // Parry changes build conditions and legitimately recomposes maximum
        // health. The actual submitted hit result isolates damage from that.
        TestEqual(TEXT("parried round applies no health damage"), Weapon->GetLastShot().DamageResult.HealthDamage, 0.0f);
        TestEqual(TEXT("parried round applies no shield damage"), Weapon->GetLastShot().DamageResult.ShieldDamage, 0.0f);
    }
    Mana->AdvanceLoop(1);
    TestFalse(TEXT("parried rounds cannot apply Bleed"), FirstStatus->HasStatus(Bleed));
    TestEqual(TEXT("parried rounds give no status income"), Mana->GetMana(), ParryMana, .001f);

    // Acquire an actual piercing channel, then earn the source status by firing.
    // Enter Vector at Split and follow the actual wrap edge through Line to Wake.
    for (const TCHAR* Id : {TEXT("Core.Vector.Split"), TEXT("Core.Vector.Line"), TEXT("Core.Vector.Wake"), TEXT("Core.Vector.PierceDiscipline")})
        if (!Buy(Id)) return false;
    if (!Shots(4)) return false;
    if (!TestTrue(TEXT("accepted source earns Bleed after avoided rounds"), FirstStatus->HasStatus(Bleed))) return false;
    // Shipped Bleed deliberately does not spread on pierce; Poison does.
    // Earn that separate source through paid Fracture delivery, not ApplyStatus.
    if (!Progression->IsAbilityUnlocked(TEXT("Caster.Fracture")))
        if (!TestTrue(TEXT("actual level token unlocks Fracture"), Progression->SpendAbilityToken(TEXT("Caster.Fracture"), Reason))) return false;
    const auto Fracture = Source->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
    for (int32 Cast = 0; Cast < 2; ++Cast)
    {
        TSet<ABreakerProjectileBase*> Existing;
        for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) Existing.Add(*It);
        const float BeforeCast = Mana->GetMana();
        if (!TestTrue(TEXT("real paid Fracture prepares spreading Poison"), Source->GetAbilitySystemComponent()->TryActivateAbility(Fracture))) return false;
        TestTrue(TEXT("Fracture spends existing Mana"), Mana->GetMana() < BeforeCast);
        ABreakerProjectileBase* Projectile = nullptr;
        for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It)
            if (!Existing.Contains(*It)) { Projectile = *It; break; }
        if (!TestNotNull(TEXT("paid Fracture projectile exists"), Projectile)) return false;
        // Exercise the actual authority impact seam; no natural flight claim.
        Projectile->Impact(Cast == 0 ? nullptr : First, FirstPosition);
    }
    if (!TestTrue(TEXT("paid second cycle hit earns actual spreading Poison"), FirstStatus->HasStatus(Poison))) return false;
    Second->SetActorLocation(SecondPosition);
    First->GetCombat()->DodgeChance = 1;
    const float SecondBefore = Second->GetAttributes()->GetHealth();
    if (!Shots(1)) return false;
    TestTrue(TEXT("pierce still reaches accepted second receiver"), Second->GetAttributes()->GetHealth() < SecondBefore);
    TestFalse(TEXT("next accepted receiver has not yet earned its own Bleed"), SecondStatus->HasStatus(Bleed));
    TestFalse(TEXT("avoided first receiver cannot spread its earned Poison"), SecondStatus->HasStatus(Poison));
    First->GetCombat()->DodgeChance = 0; Second->GetCombat()->DodgeChance = 1;
    const float RefusedSecondHealth = Second->GetAttributes()->GetHealth();
    if (!Shots(1)) return false;
    TestEqual(TEXT("second pierced receiver actually dodges"), Second->GetAttributes()->GetHealth(), RefusedSecondHealth);
    TestFalse(TEXT("avoided pierced receiver cannot earn original Bleed"), SecondStatus->HasStatus(Bleed));
    TestFalse(TEXT("avoided pierced receiver cannot receive copied Poison"), SecondStatus->HasStatus(Poison));

    // Normalize the deterministic bank through accepted single-body firing,
    // never by writing its state; its next four eligible applications remain .25.
    Second->SetActorLocation(SecondPosition + FVector(0, 3000, 0));
    bool bRemovedBleed = false; FirstStatus->ConsumeStatus(Bleed, bRemovedBleed);
    for (int32 Shot = 0; Shot < 4 && !FirstStatus->HasStatus(Bleed); ++Shot) if (!Shots(1)) return false;
    if (!TestTrue(TEXT("paid source firing reaches next deterministic threshold"), FirstStatus->HasStatus(Bleed))) return false;
    const auto* SourcePoison = FirstStatus->GetActiveStatuses().FindByPredicate([Poison](const FBreakerActiveStatus& Status) { return Status.Spec.StatusTag == Poison; });
    if (!TestNotNull(TEXT("actual earned Poison remains on source body"), SourcePoison)) return false;
    const float CopyDamage = SourcePoison->Spec.BaseDamagePerTick * BreakerStatusRules::PierceSpreadPayloadFraction();
    const float CopyDuration = SourcePoison->RemainingDuration;
    Second->GetCombat()->DodgeChance = 0; Second->SetActorLocation(SecondPosition);
    if (!Shots(1)) return false;
    const auto* Copy = SecondStatus->GetActiveStatuses().FindByPredicate([Poison](const FBreakerActiveStatus& Status) { return Status.Spec.StatusTag == Poison; });
    if (!TestTrue(*FString::Printf(TEXT("accepted pierce transfers earned Poison at zero proc: exists=%d proc=%.3f sourcePoison=%d"),
        Copy != nullptr, Copy ? Copy->Spec.ProcCoefficient : -1, FirstStatus->HasStatus(Poison)),
        Copy && Copy->Spec.ProcCoefficient == 0)) return false;
    TestEqual(TEXT("pierce copy retains only authored fraction of source payload"), Copy->Spec.BaseDamagePerTick, CopyDamage, .001f);
    TestEqual(TEXT("pierce copy uses source remaining duration"), Copy->RemainingDuration, CopyDuration, .001f);

    for (const TCHAR* Id : {TEXT("Core.Vector.Spread"), TEXT("Core.Vector.Carom"), TEXT("Core.Vector.Chainwork")})
        if (!Buy(Id)) return false;
    TestEqual(TEXT("all mechanics and connecting rims use thirteen level-earned Core points"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), 0);
    SecondStatus->ConsumeAllStatuses(); Second->SetActorLocation(FirstPosition + FVector(0, 300, 0));
    Second->GetCombat()->DodgeChance = 1;
    const float ArcHealth = Second->GetAttributes()->GetHealth();
    if (!Shots(4)) return false;
    TestEqual(TEXT("chain recipient actually avoids damage"), Second->GetAttributes()->GetHealth(), ArcHealth);
    TestFalse(TEXT("avoided chain recipient cannot receive Bleed"), SecondStatus->HasStatus(Bleed));
    FirstStatus->ConsumeAllStatuses();
    for (int32 Shot = 0; Shot < 4 && !FirstStatus->HasStatus(Bleed); ++Shot) if (!Shots(1)) return false;
    if (!TestTrue(TEXT("accepted first hits reset bank naturally while chain recipient dodges"), FirstStatus->HasStatus(Bleed))) return false;
    Second->GetCombat()->DodgeChance = 0;
    if (!Shots(2)) return false;
    TestTrue(TEXT("accepted chain still deals damage"), Second->GetAttributes()->GetHealth() < ArcHealth);
    TestTrue(TEXT("accepted chain still earns its own original Bleed"), SecondStatus->HasStatus(Bleed));
    return true;
}
#endif
