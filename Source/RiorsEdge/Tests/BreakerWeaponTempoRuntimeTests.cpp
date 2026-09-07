#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerWeaponTempoRuntimeTest, "RiorsEdge.Weapons.TempoRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerWeaponTempoRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated tempo world"), World)) return false;
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
    UBreakerWeaponComponent* Weapon = Player->GetWeapon();
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->ReloadDuration = 1.0f;
    Weapon->WeaponDefinition->SwapInDuration = 1.0f;
    Weapon->ResetAmmunition();
    auto Advance = [&](int32 Steps)
    {
        for (int32 I = 0; I < Steps; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.05f); }
    };
    auto Total = [&] { return Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo(); };
    auto Fire = [&]
    {
        const int32 Before = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        return TestEqual(TEXT("real trigger spends one round before reload"), Weapon->GetMagazineAmmo(), Before - 1);
    };
    TestEqual(TEXT("no bonus reload multiplier"), Weapon->GetReloadSpeedMultiplier(), 1.0f);
    TestEqual(TEXT("no bonus swap multiplier"), Weapon->GetSwapSpeedMultiplier(), 1.0f);
    if (!Fire()) return false;
    const int32 BaselineTotal = Total();
    Weapon->StartReload();
    if (!TestTrue(TEXT("ordinary reload starts"), Weapon->IsReloading())) return false;
    Advance(11);
    TestTrue(TEXT("ordinary one-second reload still active at .55s"), Weapon->IsReloading());
    Advance(10);
    TestFalse(TEXT("ordinary reload completes after authored duration"), Weapon->IsReloading());
    TestEqual(TEXT("ordinary reload preserves ammunition"), Total(), BaselineTotal);

    const FName A(TEXT("Tempo.SourceA")), B(TEXT("Tempo.SourceB")), C(TEXT("Tempo.SourceC"));
    Weapon->PushTempoBonus(A, 2.0f, 1.25f);
    Weapon->PushTempoBonus(B, 1.5f, 2.0f);
    TestEqual(TEXT("overlap selects strongest reload source"), Weapon->GetReloadSpeedMultiplier(), 2.0f);
    TestEqual(TEXT("overlap independently selects strongest swap source"), Weapon->GetSwapSpeedMultiplier(), 2.0f);
    Weapon->PopTempoBonus(A);
    TestEqual(TEXT("removing reload winner reveals remaining source"), Weapon->GetReloadSpeedMultiplier(), 1.5f);
    TestEqual(TEXT("removing another source preserves swap winner"), Weapon->GetSwapSpeedMultiplier(), 2.0f);
    if (!Fire()) return false;
    const int32 AcceleratedTotal = Total();
    Weapon->StartReload();
    if (!TestTrue(TEXT("tempo reload starts"), Weapon->IsReloading())) return false;
    Advance(5);
    Weapon->PopTempoBonus(B);
    Weapon->PushTempoBonus(C, 4.0f, 4.0f);
    Advance(5);
    TestTrue(TEXT("new faster bonus cannot shorten in-progress .667s reload"), Weapon->IsReloading());
    Advance(5);
    TestFalse(TEXT("removing old source cannot extend snapshotted reload"), Weapon->IsReloading());
    TestEqual(TEXT("accelerated reload only transfers existing rounds"), Total(), AcceleratedTotal);
    if (!Fire()) return false;
    const int32 FasterTotal = Total();
    Weapon->StartReload();
    Weapon->PopTempoBonus(C);
    Advance(3);
    TestTrue(TEXT("new reload retains .25s snapshot after bonus removal"), Weapon->IsReloading());
    Advance(3);
    TestFalse(TEXT("new action used stronger bonus present at its start"), Weapon->IsReloading());
    TestEqual(TEXT("short reload preserves ammunition"), Total(), FasterTotal);

    Weapon->PushTempoBonus(NAME_None, 10, 10);
    Weapon->PushTempoBonus(TEXT("Tempo.InvalidNan"), std::numeric_limits<float>::quiet_NaN(), 1);
    Weapon->PushTempoBonus(TEXT("Tempo.InvalidInfinity"), 1, std::numeric_limits<float>::infinity());
    Weapon->PushTempoBonus(TEXT("Tempo.InvalidNonpositive"), 0, -1);
    TestEqual(TEXT("invalid bonus inputs cannot change reload speed"), Weapon->GetReloadSpeedMultiplier(), 1.0f);
    TestEqual(TEXT("invalid bonus inputs cannot change swap speed"), Weapon->GetSwapSpeedMultiplier(), 1.0f);

    const int32 PrimaryTotal = Total();
    Weapon->EquipSlot(2);
    Advance(11);
    TestTrue(TEXT("ordinary swap still active at .55s"), Weapon->IsSwapping());
    Advance(10);
    TestFalse(TEXT("ordinary swap completes at authored duration"), Weapon->IsSwapping());
    const int32 SecondaryTotal = Total();
    Weapon->PushTempoBonus(A, 1, 2);
    Weapon->EquipSlot(1);
    Advance(3);
    Weapon->PopTempoBonus(A);
    Weapon->PushTempoBonus(B, 1, 4);
    Advance(3);
    TestTrue(TEXT("mid-swap faster source cannot shorten existing half-second snapshot"), Weapon->IsSwapping());
    Advance(5);
    TestFalse(TEXT("original accelerated swap still completes despite old-source removal"), Weapon->IsSwapping());
    TestEqual(TEXT("returning to Primary conserves its magazine and reserve"), Total(), PrimaryTotal);
    Weapon->EquipSlot(2);
    Weapon->PopTempoBonus(B);
    Advance(3);
    TestTrue(TEXT("quarter-second next swap remains active below snapshot"), Weapon->IsSwapping());
    Advance(3);
    TestFalse(TEXT("next swap captured stronger source before removal"), Weapon->IsSwapping());
    TestEqual(TEXT("Secondary ammunition remains independent and unchanged"), Total(), SecondaryTotal);
    return true;
}
#endif
