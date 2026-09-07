#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/BreakerGameInstance.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Save/BreakerQuestJournal.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDestinationArrivalTest, "RiorsEdge.Missions.DestinationWorldVerification",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDestinationArrivalTest::RunTest(const FString& Parameters)
{
    const FName Destinations[] = { ABreakerTravelPoint::HubDestinationId, ABreakerTravelPoint::GymDestinationId,
        ABreakerTravelPoint::FernhallDestinationId, ABreakerTravelPoint::RiftDestinationId, ABreakerTravelPoint::ErasedEarthDestinationId };
    const FString Maps[] = { TEXT("Lvl_Anchor"), TEXT("Lvl_Gym"), TEXT("Lvl_Fernhall"), TEXT("Lvl_FrontEnd"),
        TEXT("Lvl_ErasedEarth"), TEXT("UEDPIE_0_Lvl_ErasedEarth") };
    for (int32 MapIndex = 0; MapIndex < UE_ARRAY_COUNT(Maps); ++MapIndex)
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/Destination_%s/%s"),
            *FGuid::NewGuid().ToString(EGuidFormats::Digits), *Maps[MapIndex]));
        Package->SetFlags(RF_Transient);
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(*Maps[MapIndex]), Package,
            true, ERHIFeatureLevel::Num, &Init);
        if (!TestNotNull(TEXT("Isolated named world"), World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
        if (MapIndex == 5) World->StreamingLevelsPrefix = TEXT("UEDPIE_0_");
        for (int32 DestinationIndex = 0; DestinationIndex < UE_ARRAY_COUNT(Destinations); ++DestinationIndex)
        {
            const bool Expected = (MapIndex == 0 && DestinationIndex == 0)
                || (MapIndex == 1 && DestinationIndex == 1)
                || (MapIndex == 2 && (DestinationIndex == 2 || DestinationIndex == 3))
                || (MapIndex >= 4 && DestinationIndex == 4);
            TestEqual(FString::Printf(TEXT("Actual map %s verifies destination %s"), *Maps[MapIndex], *Destinations[DestinationIndex].ToString()),
                UBreakerGameInstance::IsDestinationMap(World, Destinations[DestinationIndex]), Expected);
        }
        TestFalse(TEXT("Unknown destination never verifies"), UBreakerGameInstance::IsDestinationMap(World, TEXT("Unknown")));
        if (MapIndex >= 4)
        {
            TestTrue(TEXT("Earth classification includes PIE prefix stripping"), UBreakerGameInstance::IsErasedEarthMap(World));
            TestFalse(TEXT("Earth never falls into Gym content"), UBreakerGameInstance::IsGymMap(World));
        }
        if (MapIndex != 0) continue;
        // Isolated gate-state fixture, not an earned campaign or travel claim.
        ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
        ABreakerTravelPoint* Gate = World->SpawnActor<ABreakerTravelPoint>();
        if (!TestNotNull(TEXT("Actual player"), Player) || !TestNotNull(TEXT("Actual gate"), Gate)) return false;
        Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
        Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        TestFalse(TEXT("Missing pawn cannot enter"), ABreakerTravelPoint::CanEnterErasedEarth(nullptr));
        TestFalse(TEXT("Unaccepted route rejected by actual gate"), Gate->SelectDestination(ABreakerTravelPoint::ErasedEarthDestinationId, Player));
        FBreakerQuestFlagSet Flags;
        Flags.Add(TEXT("Quest.Survivor.Accepted"));
        Player->GetQuestJournal()->RestoreFrom(Flags);
        TestFalse(TEXT("Acceptance without earned prior prerequisite refuses"), ABreakerTravelPoint::CanEnterErasedEarth(Player));
        Flags.Add(TEXT("Quest.Breach.TurnedIn"));
        Player->GetQuestJournal()->RestoreFrom(Flags);
        TestTrue(TEXT("Required live gate flags admit route"), ABreakerTravelPoint::CanEnterErasedEarth(Player));
        Flags.Add(TEXT("Quest.Survivor.Extracted"));
        Player->GetQuestJournal()->RestoreFrom(Flags);
        TestFalse(TEXT("Already extracted route cannot restart"), ABreakerTravelPoint::CanEnterErasedEarth(Player));
        Flags.Flags.Remove(TEXT("Quest.Survivor.Extracted"));
        Player->GetQuestJournal()->RestoreFrom(Flags);
        FBreakerDamageRequest Lethal;
        Lethal.BaseDamage = 1000000; Lethal.DamageFamily = EBreakerDamageFamily::TrueDamage;
        Lethal.bBypassShield = true; Lethal.bCanCritical = false;
        TestTrue(TEXT("Actual combat kills gate requester"), Player->GetCombat()->ReceiveDamage(Lethal).bKilled);
        TestFalse(TEXT("Dead requester refused despite prerequisite flags"), Gate->SelectDestination(ABreakerTravelPoint::ErasedEarthDestinationId, Player));
    }
    TestFalse(TEXT("No world cannot verify arrival"), UBreakerGameInstance::IsDestinationMap(nullptr, ABreakerTravelPoint::HubDestinationId));
    return true;
}
#endif
