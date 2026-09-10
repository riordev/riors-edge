#include "Interaction/BreakerSupplyChest.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/BreakerSupplyChestMath.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Items/BreakerLootPickup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UI/BreakerUIStyle.h"

ABreakerSupplyChest::ABreakerSupplyChest()
{
    bReplicates = true;
    ConfigureConsoleBody();
    // A CHEST, NOT A CONSOLE. The console body is the right components and the
    // wrong proportions: wide, low and lidded reads as something you open,
    // where tall and narrow reads as something you operate.
    if (Visual)
    {
        Visual->SetRelativeScale3D(FVector(0.90f, 0.62f, 0.42f));
        Visual->SetRelativeLocation(FVector(0.0f, 0.0f, -62.0f));
    }
    if (Trim)
    {
        // The lid band, sitting on the box's top edge. GOLD, because O179
        // spends gold on reward and a chest is nothing else.
        Trim->SetRelativeRotation(FRotator::ZeroRotator);
        Trim->SetRelativeScale3D(FVector(0.96f, 0.68f, 0.07f));
        Trim->SetRelativeLocation(FVector(0.0f, 0.0f, -34.0f));
        if (UMaterialInstanceDynamic* Band = Trim->CreateDynamicMaterialInstance(0))
        {
            Band->SetVectorParameterValue(TEXT("Color"), BreakerUI::Gold);
        }
    }
    DisplayName = FText::FromString(TEXT("Supply chest"));
    DialogueId = NAME_None;
    DialogueNodes.Reset();
    EntryOverrides.Reset();
    Tags.Add(TEXT("Fernhall.Chest"));
}

void ABreakerSupplyChest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABreakerSupplyChest, bOpened);
}

void ABreakerSupplyChest::Configure(int32 AreaLevel, int32 Seed)
{
    if (!HasAuthority() || bConfigured) return;
    bConfigured = true;
    ItemLevel = FMath::Max(1, AreaLevel);
    ContentSeed = Seed;
}

bool ABreakerSupplyChest::PaysCurrency() const
{
    return BreakerSupplyChest::PaysCurrency(ContentSeed);
}

FText ABreakerSupplyChest::GetChestPrompt() const
{
    // NO PREVIEW OF THE CONTENTS. The chest knows what is inside it before it
    // is opened, and saying so would delete the only moment it has.
    return bOpened ? FText::GetEmpty() : FText::FromString(TEXT("OPEN CHEST"));
}

bool ABreakerSupplyChest::IsInteractionReachable(const ABreakerCharacter* Player) const
{
    if (IsActorBeingDestroyed() || bOpened || !IsValid(Player) || !GetWorld()
        || Player->GetWorld() != GetWorld() || !Player->GetCombat() || Player->GetCombat()->IsDead()
        || FVector::DistSquared(Player->GetActorLocation(), GetActorLocation())
            > FMath::Square(InteractionRange)) return false;
    FCollisionQueryParams Visibility(SCENE_QUERY_STAT(SupplyChestVisibility), false, Player);
    Visibility.AddIgnoredActor(this);
    FHitResult Obstruction;
    if (GetWorld()->LineTraceSingleByObjectType(Obstruction, Player->GetActorLocation(), GetActorLocation(),
        FCollisionObjectQueryParams(ECC_WorldStatic), Visibility)) return false;
    return true;
}

bool ABreakerSupplyChest::TryOpen(ABreakerCharacter* Player)
{
    if (!HasAuthority() || !IsInteractionReachable(Player) || !Player->HasAuthority()
        || !Player->GetEquipment()) return false;
    // Claim before anything can fail: a chest that pays nothing because a spawn
    // failed must not be re-openable for another attempt at the same reward.
    bOpened = true;

    if (BreakerSupplyChest::PaysCurrency(ContentSeed))
    {
        // The shipped currency roll at trash odds. Riftglass is credited
        // straight to the wallet rather than dropped, because there is no
        // physical currency pickup in this project and inventing one here
        // would be a nearest-fit primitive for a thing nobody has ruled on.
        const FBreakerForgeWallet PerKill = UBreakerDropTableLibrary::RollCurrencyDrop(
            ContentSeed, ItemLevel, EBreakerMonsterRank::Trash, FBreakerCurrencyDropParams());
        FBreakerForgeWallet Yield;
        Yield.Riftglass = BreakerSupplyChest::CurrencyPayout(PerKill.Get(), ItemLevel);
        Player->GetEquipment()->CreditForgeCurrency(Yield);
        return true;
    }

    // AN ITEM, ROLLED AT THE LEAST GENEROUS ODDS THE TABLE OFFERS and then
    // tempered down. Trash rank, and the player's own Drop Chance is NOT
    // passed in: a chest is not a kill, and letting gear improve a free reward
    // is how a stat meant to pay for fighting starts paying for walking.
    const EBreakerItemRarity Rolled = UBreakerDropTableLibrary::RollGatedRarity(
        ContentSeed, ItemLevel, EBreakerMonsterRank::Trash, 0.0f, FBreakerDropTableParams());
    const EBreakerItemRarity Rarity = BreakerSupplyChest::Temper(Rolled, ContentSeed);
    const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("SupplyChest"),
        UBreakerLootLibrary::RollDropSlot(ContentSeed), Rarity, ItemLevel, ContentSeed);
    // On the approach side, the same offset the cache uses.
    const FVector At = GetActorLocation() + GetActorForwardVector() * 120.0f - FVector(0, 0, 40.0f);
    ABreakerLootPickup* Pickup = GetWorld()->SpawnActor<ABreakerLootPickup>(
        ABreakerLootPickup::StaticClass(), At, FRotator::ZeroRotator);
    if (!Pickup) return false;
    Pickup->SetItem(Item);
    return true;
}
