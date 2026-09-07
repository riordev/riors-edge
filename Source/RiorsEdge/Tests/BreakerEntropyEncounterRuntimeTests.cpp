#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Data/BreakerDataFile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEntropyEncounterRuntimeTest, "RiorsEdge.Combat.EntropyEncounterRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerEntropyEncounterRuntimeTest::RunTest(const FString& Parameters)
{
    BreakerDataFile::FBreakerDataErrors Errors;
    const auto Elements = BreakerDataFile::Load(TEXT("Data/elements.json"), Errors);
    if (!TestTrue(TEXT("shipped elemental tuning loads"), Elements.IsValid() && Errors.IsClean())) return false;
    const float BudgetFraction = Elements->GetNumberField(TEXT("entropyDamageFraction"));
    const float RotDuration = Elements->GetNumberField(TEXT("entropyDurationSeconds"));
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated stationary encounter world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    int32 Scenario = 0;
    const FGameplayTag RotTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    for (const int32 Depth : {1, 50})
    for (const auto Rank : {EBreakerMonsterRank::Trash, EBreakerMonsterRank::ModifierBearing})
    {
        const FString Label = FString::Printf(TEXT("ilvl%d area%d %s"), Depth, Depth,
            Rank == EBreakerMonsterRank::Trash ? TEXT("Trash") : TEXT("Veteran chassis"));
        const FVector Origin(Scenario++ * 10000.0f, 0, 100);
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Caster = World->SpawnActor<ABreakerCharacter>(ABreakerCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Spawn);
        if (!Caster) return false;
        Caster->SetActorTickEnabled(false); Caster->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC = Caster->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Caster, Caster); ASC->AddAttributeSetSubobject(Caster->GetAttributes());
        Caster->GetCombat()->BindAttributes(Caster->GetAttributes());
        Caster->GetProgression()->BindAttributes(Caster->GetAttributes());
        if (!TestTrue(*(Label + TEXT(" chooses actual Caster")), Caster->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
        FBreakerItemInstance Rifle;
        for (int32 Seed = 1; Seed <= 4096; ++Seed)
        {
            Rifle = UBreakerLootLibrary::RollItem(TEXT("Entropy.Encounter"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, Depth, Seed);
            if (Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle) break;
        }
        if (!TestTrue(*(Label + TEXT(" rolls a real rifle")), Rifle.IsValid() && Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle)) return false;
        // Isolate equipped depth, without adding offensive stats or progression points.
        for (auto& Affix : Rifle.Affixes) Affix.Value = 0;
        Caster->GetEquipment()->BindAttributes(Caster->GetAttributes());
        if (!TestTrue(*(Label + TEXT(" equips rolled depth fixture")), Caster->GetEquipment()->EquipItem(Rifle))) return false;
        Caster->GetWeapon()->SyncArchetypesToEquipment(); Caster->GetWeapon()->EquipSlot(1);
        auto* Mana = Caster->FindComponentByClass<UBreakerManaComponent>();
        if (!TestNotNull(TEXT("native Caster Mana consumer"), Mana)) return false;
        Mana->BindAttributes(Caster->GetAttributes());
        Mana->AdvanceLoop(20.0f); // Ordinary passive recovery funds one cast; no refill during the measurement.

        TArray<ABreakerEnemy*> Enemies;
        TArray<float> StartingHealth;
        TArray<int32> Activations;
        TArray<float> FirstActivation;
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const FVector Location = Origin + FVector(600, (Index - 1) * 150, 0);
            auto* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), Location, FRotator::ZeroRotator, Spawn);
            if (!Enemy) return false;
            Enemy->ConfigureCrowdProbe(); Enemy->SetAreaLevel(Depth); Enemy->SetMonsterRank(Rank);
            Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
            if (auto* Movement = Enemy->GetMovementComponent()) Movement->SetComponentTickEnabled(false);
            auto* Status = Enemy->FindComponentByClass<UBreakerStatusComponent>();
            Status->SetComponentTickEnabled(false);
            const auto* Attributes = Enemy->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>();
            if (!TestNotNull(TEXT("shipped enemy attributes"), Attributes)) return false;
            TestEqual(*(Label + TEXT(" has no authored armor in this chassis fixture")), Attributes->GetArmor(), 0.0f);
            TestEqual(*(Label + TEXT(" has no shield masking direct damage")), Attributes->GetShield(), 0.0f);
            Enemies.Add(Enemy); StartingHealth.Add(Attributes->GetHealth()); Activations.Add(0); FirstActivation.Add(-1);
        }
        const auto CastHandle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Rot::StaticClass(), 1));
        const float ManaBefore = Mana->GetMana();
        if (!TestTrue(*(Label + TEXT(" pays for a real Rot cast")), ASC->TryActivateAbility(CastHandle))) return false;
        ABreakerZoneActor* Zone = nullptr;
        for (const auto& Held : ABreakerZoneActor::GetLiveZones())
            if (auto* Candidate = Held.Get(); Candidate && Candidate->GetZoneInstigator() == Caster) { Zone = Candidate; break; }
        if (!TestNotNull(*(Label + TEXT(" spawns its actual zone")), Zone)) return false;
        const float ManaSpent = ManaBefore - Mana->GetMana();
        TestTrue(*(Label + TEXT(" actual cast spends Mana")), ManaSpent > 0);
        // The cast still owns placement. The stationary fixture is centered on its actual impact.
        for (int32 Index = 0; Index < Enemies.Num(); ++Index)
            Enemies[Index]->SetActorLocation(Zone->GetActorLocation() + FVector(0, (Index - 1) * 150, 0));
        const FBreakerZoneSpec Spec = Zone->GetSpec();
        const int32 ExpectedTicks = FMath::FloorToInt((Spec.Duration + UE_KINDA_SMALL_NUMBER) / Spec.TickInterval);
        const int32 Steps = FMath::CeilToInt((Spec.Duration + .05f) / .05f);
        for (int32 Step = 0; Step < Steps; ++Step)
        {
            TArray<float> BeforeZone;
            TArray<bool> HadRot;
            for (auto* Enemy : Enemies)
            {
                auto* Status = Enemy->FindComponentByClass<UBreakerStatusComponent>();
                Status->AdvanceStatuses(.05f);
                BeforeZone.Add(Enemy->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>()->GetHealth());
                HadRot.Add(Status->HasStatus(RotTag));
            }
            if (!Zone->IsReleased()) Zone->AdvanceZone(.05f);
            for (int32 Index = 0; Index < Enemies.Num(); ++Index)
            {
                auto* Status = Enemies[Index]->FindComponentByClass<UBreakerStatusComponent>();
                if (HadRot[Index] || !Status->HasStatus(RotTag)) continue;
                ++Activations[Index];
                if (FirstActivation[Index] < 0) FirstActivation[Index] = (Step + 1) * .05f;
                const auto* Active = Status->GetActiveStatuses().FindByPredicate([&](const FBreakerActiveStatus& Entry) { return Entry.Spec.StatusTag == RotTag; });
                const float ApplyingDamage = BeforeZone[Index] - Enemies[Index]->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>()->GetHealth();
                const int32 TickCount = FMath::FloorToInt(Active->Spec.Duration / Active->Spec.TickInterval);
                TestEqual(*(Label + TEXT(" earned Rot budget snapshots the actual applying hit once")), Active->Spec.BaseDamagePerTick * TickCount, ApplyingDamage * BudgetFraction, .01f);
            }
        }
        TestEqual(*(Label + TEXT(" full zone lifetime includes the boundary tick")), Zone->GetTicksDelivered(), ExpectedTicks);
        TestTrue(*(Label + TEXT(" zone ends at its authored duration")), Zone->IsReleased());
        // Let earned damage finish after the floor volume ends; no extra casts or resource grants.
        for (int32 Step = 0; Step < FMath::CeilToInt(RotDuration / .05f) + 1; ++Step)
            for (auto* Enemy : Enemies) Enemy->FindComponentByClass<UBreakerStatusComponent>()->AdvanceStatuses(.05f);
        float PackDamage = 0;
        for (int32 Index = 0; Index < Enemies.Num(); ++Index)
        {
            const auto* Attributes = Enemies[Index]->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>();
            const float Damage = StartingHealth[Index] - Attributes->GetHealth(); PackDamage += Damage;
            TestTrue(*(Label + TEXT(" actual volume damages each retained pack body")), Damage > 0);
            AddInfo(FString::Printf(TEXT("ENTROPY ENCOUNTER %s target%d hp%.3f threshold%.3f damage%.3f rot%d first%.2fs"),
                *Label, Index, StartingHealth[Index], Enemies[Index]->FindComponentByClass<UBreakerStatusComponent>()->GetEntropyThreshold(), Damage, Activations[Index], FirstActivation[Index]));
        }
        AddInfo(FString::Printf(TEXT("ENTROPY ENCOUNTER %s zone %.2fs ticks%d/%d mana%.2f packDamage%.3f damagePerMana%.3f; stationary retention, no weapon parity claim"),
            *Label, Spec.Duration, Zone->GetTicksDelivered(), ExpectedTicks, ManaSpent, PackDamage, PackDamage / ManaSpent));
        for (auto* Enemy : Enemies) Enemy->Destroy();
        Caster->Destroy();
    }
    return true;
}
#endif
