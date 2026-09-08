#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "EngineUtils.h"
#include "Abilities/BreakerAbility_CadenceBreak.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    struct FBreakerCadenceAfterimageFixture
    {
        UWorld* World = nullptr;
        ABreakerCharacter* Player = nullptr;
        AActor* Target = nullptr;
        UBreakerProgressionTree* CoreTree = nullptr;
        uint64 SavedFrame = GFrameCounter;
        FBreakerCadenceAfterimageFixture(EBreakerClassId Class, bool bOwnAfterimage)
        {
            UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
            if (!World) return;
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
            Player=World->SpawnActor<ABreakerCharacter>(); Player->bRefuseSavesForPendingCharacter=true; Player->SetActorTickEnabled(false);
            Player->GetBreakerMovement()->SetComponentTickEnabled(false); Player->GetBreakerMovement()->SetMovementMode(MOVE_Walking);
            auto* ASC=Player->GetAbilitySystemComponent(); auto* Attr=Player->GetAttributes();
            ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr); Player->GetCombat()->BindAttributes(Attr);
            auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr); Progression->ChoosePermanentClassById(Class);
            auto* Definition=DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition,Player);
            auto* Tree=NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId=TEXT("Test.Afterimage.Cadence"); Tree->Currency=EBreakerPointCurrency::CorePoints;
            Definition->BranchTrees.Add(Tree); Progression->ClassDefinition=Definition; CoreTree=Tree;
            auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Afterimage.Cadence.Rule"); Node->Currency=Tree->Currency;
            Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))); Tree->Nodes.Add(Node);
            Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(40,Progression->ExperienceCurve));
            FText Reason; if (bOwnAfterimage) Progression->PurchaseNode(Tree,Node->NodeId,Reason);
            Player->FindComponentByClass<UBreakerChargeComponent>()->BindAttributes(Attr);
            Player->FindComponentByClass<UBreakerChargeComponent>()->SetInCombat(true);
            Attr->ApplyClassResource(0); Player->FindComponentByClass<UBreakerChargeComponent>()->SetComponentTickEnabled(false);
            Player->GetMomentum()->BindAttributes(Attr); Player->GetMomentum()->SetComponentTickEnabled(false);
            UBreakerAbilityStateComponent::FindOrAdd(Player)->SetComponentTickEnabled(false);
            auto* Weapon=Player->GetWeapon(); Weapon->WeaponDefinition=DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(),Weapon);
            Weapon->WeaponDefinition->HipSpreadDegrees=Weapon->WeaponDefinition->AimSpreadDegrees=0; Weapon->WeaponDefinition->BleedChance=0;
            Weapon->ResetAmmunition();
            Target=World->SpawnActor<AActor>(); auto* Body=NewObject<USphereComponent>(Target); Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
            Body->SetSphereRadius(60); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Body->SetCollisionResponseToAllChannels(ECR_Ignore);
            Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block); Body->RegisterComponent(); AimTarget();
            auto* Sink=NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Sink); Sink->RegisterComponent();
            auto* Health=NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000); Sink->BindAttributes(Health);
        }
        ~FBreakerCadenceAfterimageFixture() { if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); } GFrameCounter=SavedFrame; }
        void AimTarget() { FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye,Aim); Target->SetActorLocation(Eye+Aim.Vector()*800); }
        void Tick(float Seconds)
        {
            // Registered state components have automatic ticking disabled;
            // the save-isolated fixture drives each native clock exactly once.
            for (int32 I = 0; I < FMath::CeilToInt(Seconds / .01f); ++I)
            {
                ++GFrameCounter;
                World->Tick(LEVELTICK_All, .01f);
                for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
                    if (auto* State = It->FindComponentByClass<UBreakerAbilityStateComponent>()) State->AdvanceTime(.01f);
                for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
                    if (auto* Charge = It->FindComponentByClass<UBreakerChargeComponent>()) Charge->AdvanceLoop(.01f);
            }
        }
        ABreakerCharacter* AddAlly(bool bOwnAfterimage, EBreakerClassId Class = EBreakerClassId::Swift)
        {
            FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Ally = World->SpawnActor<ABreakerCharacter>(FVector(0, 200, 0), FRotator::ZeroRotator, Spawn);
            Ally->bRefuseSavesForPendingCharacter = true; Ally->SetActorTickEnabled(false);
            Ally->GetBreakerMovement()->SetComponentTickEnabled(false); Ally->GetBreakerMovement()->SetMovementMode(MOVE_Walking);
            auto* ASC = Ally->GetAbilitySystemComponent(); auto* Attr = Ally->GetAttributes();
            ASC->InitAbilityActorInfo(Ally, Ally); ASC->AddAttributeSetSubobject(Attr); Ally->GetCombat()->BindAttributes(Attr);
            auto* P = Ally->GetProgression(); P->BindAttributes(Attr); P->ChoosePermanentClassById(Class);
            if (bOwnAfterimage)
            {
                auto* Definition = DuplicateObject<UBreakerClassDefinition>(P->ClassDefinition, Ally);
                auto* Tree = DuplicateObject<UBreakerProgressionTree>(CoreTree, Definition);
                Definition->BranchTrees.Add(Tree); P->ClassDefinition = Definition;
                P->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, P->ExperienceCurve));
                FText Reason; P->PurchaseNode(Tree, TEXT("Test.Afterimage.Cadence.Rule"), Reason);
            }
            auto* Charge = Ally->FindComponentByClass<UBreakerChargeComponent>();
            Charge->BindAttributes(Attr); Charge->SetComponentTickEnabled(false); Charge->SetInCombat(true);
            Attr->ApplyClassResource(0);
            UBreakerAbilityStateComponent::FindOrAdd(Ally)->SetComponentTickEnabled(false);
            auto* Weapon = Ally->GetWeapon(); Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
            Weapon->WeaponDefinition->HipSpreadDegrees = Weapon->WeaponDefinition->AimSpreadDegrees = 0;
            Weapon->WeaponDefinition->BleedChance = 0; Weapon->ResetAmmunition();
            return Ally;
        }
        float Shot(ABreakerCharacter* Shooter = nullptr)
        {
            if (!Shooter) Shooter = Player;
            FVector Eye; FRotator Aim; Shooter->GetActorEyesViewPoint(Eye, Aim); Target->SetActorLocation(Eye + Aim.Vector()*800);
            auto* W=Shooter->GetWeapon();
            const float CriticalMultiplier=Shooter->GetAttributes()->GetCriticalMultiplier();
            W->StartFire(); W->StopFire();
            const auto& Result=W->GetLastShot().DamageResult;
            // These real hits retain their ordinary critical rolls. Compare
            // the flat lane on the same pre-critical basis for each result;
            // progression recomposition must not change what is measured.
            return Result.HealthDamage / (Result.bCritical ? CriticalMultiplier : 1.0f);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCadenceAfterimageTest, "RiorsEdge.Abilities.AfterimageCadenceTempo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCadenceAfterimageTest::RunTest(const FString&)
{
    for (int32 Scenario = 0; Scenario < 5; ++Scenario)
    {
        FBreakerCadenceAfterimageFixture F(EBreakerClassId::Support, true);
        if (!F.Player) return false;
        auto* Source = F.Player; auto* Ally = F.AddAlly(false);
        auto* Second = Scenario == 1 ? F.AddAlly(false, EBreakerClassId::Support) : nullptr;
        if (Second) Second->SetActorLocation(FVector(0, -200, 0));
        auto Prepare = [&](ABreakerCharacter* Caster)
        {
            auto* P = Caster->GetProgression(); FText Reason;
            P->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(40, P->ExperienceCurve));
            const int32 Tokens = P->GetUnspentAbilityTokens();
            if (!TestTrue(TEXT("Earned token unlocks actual Cadence"), P->SpendAbilityToken(TEXT("Support.Cadence"), Reason))) return false;
            TestEqual(TEXT("Cadence costs one token"), P->GetUnspentAbilityTokens(), Tokens - 1);
            if (!TestTrue(TEXT("Purchased Cadence equips normally"), P->EquipAbility(EBreakerAbilitySlot::ClassAbilityOne, TEXT("Support.Cadence"), Reason))) return false;
            Caster->GetAbilities()->RefreshGrants();
            auto* Charge = Caster->FindComponentByClass<UBreakerChargeComponent>();
            auto* TargetCombat = F.Target->FindComponentByClass<UBreakerCombatComponent>();
            for (int32 I = 0; I < 60 && Charge->GetCharge() < 100; ++I)
            {
                FBreakerDamageRequest Damage; Damage.BaseDamage = 20000; Damage.DamageFamily = EBreakerDamageFamily::TrueDamage; Damage.bCanCritical = false;
                TargetCombat->ReceiveDamage(Damage);
                const auto Heal = TargetCombat->ApplyHealingAmount(20000, Caster, FGameplayTag());
                Charge->NotifyHealingDone(Heal.HealthHealed, 0, 100000, false, 1); F.Tick(1);
            }
            return TestEqual(TEXT("Effective healing earns native Charge"), Charge->GetCharge(), 100.0f, .001f);
        };
        if (!Prepare(Source) || (Second && !Prepare(Second))) return false;
        auto Cast = [&](ABreakerCharacter* Caster)
        {
            auto* Abilities = Caster->GetAbilities(); auto* Charge = Caster->FindComponentByClass<UBreakerChargeComponent>();
            const float Before = Charge->GetCharge(); const float Quote = Abilities->GetCost(EBreakerAbilitySlot::ClassAbilityOne);
            if (!TestTrue(TEXT("Native equipped Cadence activates"), Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne))) return FGameplayAbilitySpecHandle();
            TestEqual(TEXT("Cadence pays actual quote"), Before - Charge->GetCharge(), Quote, .001f);
            return Caster->GetAbilitySystemComponent()->FindAbilitySpecFromClass(UBreakerAbility_Cadence::StaticClass())->Handle;
        };
        const auto Handle = Cast(Source); if (!Handle.IsValid()) return false;
        const double Start = F.World->GetTimeSeconds();
        auto* State = UBreakerAbilityStateComponent::FindOrAdd(Ally);
        const float Duration = State->GetWindowRemaining(UBreakerAbility_Cadence::WindowKey());
        auto AdvanceTo = [&](double Offset) { F.Tick(FMath::Max(0.0f, static_cast<float>(Start + Offset - F.World->GetTimeSeconds()))); };
        auto* Weapon = Ally->GetWeapon();
        TestEqual(TEXT("Actual full reload tempo"), Weapon->GetReloadSpeedMultiplier(), 1.25f, .001f);
        auto MeasureReload = [&](float ExpectedMultiplier)
        {
            F.Shot(Ally); F.Tick(.2f);
            const float Authored = Weapon->GetActiveDefinition()->ReloadDuration;
            const double Before = F.World->GetTimeSeconds(); Weapon->StartReload();
            if (!TestTrue(TEXT("Actual reload starts"), Weapon->IsReloading())) return;
            for (int32 I = 0; I < 1000 && Weapon->IsReloading(); ++I) F.Tick(.01f);
            TestFalse(TEXT("Actual reload completes"), Weapon->IsReloading());
            TestEqual(TEXT("Measured reload uses its starting tempo"), static_cast<float>(F.World->GetTimeSeconds() - Before), Authored / ExpectedMultiplier, .02f);
        };
        auto MeasureSwap = [&](float ExpectedMultiplier)
        {
            const double Before = F.World->GetTimeSeconds(); Weapon->EquipSlot(Weapon->GetCurrentSlot() == 1 ? 2 : 1);
            const float Authored = Weapon->GetActiveDefinition()->SwapInDuration;
            if (!TestTrue(TEXT("Actual swap starts"), Weapon->IsSwapping())) return;
            for (int32 I = 0; I < 1000 && Weapon->IsSwapping(); ++I) F.Tick(.01f);
            TestFalse(TEXT("Actual swap completes"), Weapon->IsSwapping());
            TestEqual(TEXT("Measured swap uses its starting tempo"), static_cast<float>(F.World->GetTimeSeconds() - Before), Authored / ExpectedMultiplier, .02f);
        };
        if (Scenario == 0)
        {
            MeasureReload(1.25f); MeasureSwap(1.25f); MeasureSwap(1.25f);
            AdvanceTo(Duration + .05f);
            TestFalse(TEXT("Cadence permission ends before tail"), State->IsWindowActive(UBreakerAbility_Cadence::WindowKey()));
            TestEqual(TEXT("Numerical tail grants half base tempo"), Weapon->GetReloadSpeedMultiplier(), 1.125f, .001f);
            MeasureReload(1.125f); MeasureSwap(1.125f);
            AdvanceTo(Duration + 2.1f);
            TestEqual(TEXT("Idle tail expiry clears replicated tempo source"), Weapon->GetReloadSpeedMultiplier(), 1.0f, .001f);
            MeasureSwap(1.0f); MeasureReload(1.0f);
            continue;
        }
        if (Scenario == 1)
        {
            AdvanceTo(4); const auto SecondHandle = Cast(Second); if (!SecondHandle.IsValid()) return false;
            TestEqual(TEXT("Two sources use max rather than stacking"), Weapon->GetReloadSpeedMultiplier(), 1.25f, .001f);
            AdvanceTo(Duration + .05f);
            TestTrue(TEXT("Another owner's window remains active"), State->IsWindowActive(UBreakerAbility_Cadence::WindowKey()));
            TestEqual(TEXT("Live source wins over expired source tail"), Weapon->GetReloadSpeedMultiplier(), 1.25f, .001f);
            Second->GetAbilitySystemComponent()->CancelAbilityHandle(SecondHandle);
            TestEqual(TEXT("Removing winning source reveals other half tail"), Weapon->GetReloadSpeedMultiplier(), 1.125f, .001f);
            MeasureSwap(1.125f);
            continue;
        }
        if (Scenario == 2)
        {
            Ally->SetActorLocation(FVector(0, 2000, 0)); F.Tick(.1f);
            TestEqual(TEXT("Ordinary aura exit cancels without tail"), Weapon->GetReloadSpeedMultiplier(), 1.0f, .001f);
            Ally->SetActorLocation(FVector(0, 200, 0)); F.Tick(.1f);
            TestEqual(TEXT("Re-entry restores full existing cast"), Weapon->GetReloadSpeedMultiplier(), 1.25f, .001f);
            State->ExtendWindow(UBreakerAbility_Cadence::WindowKey(), 2);
            AdvanceTo(Duration + .1f);
            TestEqual(TEXT("Actual owned extension retains full tempo"), Weapon->GetReloadSpeedMultiplier(), 1.25f, .001f);
            AdvanceTo(Duration + 2.1f);
            TestEqual(TEXT("Tail begins after actual extended owner expiry"), Weapon->GetReloadSpeedMultiplier(), 1.125f, .001f);
            continue;
        }
        AdvanceTo(Duration + .05f);
        TestEqual(TEXT("Half tail exists before cleanup"), Weapon->GetReloadSpeedMultiplier(), 1.125f, .001f);
        if (Scenario == 3)
        {
            FBreakerDamageRequest Kill; Kill.BaseDamage = 100000; Kill.DamageFamily = EBreakerDamageFamily::TrueDamage; Kill.bCanCritical = false; Kill.SetInstigator(F.Target);
            Source->GetCombat()->ReceiveDamage(Kill); TestTrue(TEXT("Actual source death"), Source->GetCombat()->IsDead()); Source->GetCombat()->RestoreVitals();
        }
        else Source->GetAbilitySystemComponent()->ClearAbility(Handle);
        TestEqual(TEXT("Source death or removal revokes foreign tail"), Weapon->GetReloadSpeedMultiplier(), 1.0f, .001f);
    }
    return true;
}
#endif
