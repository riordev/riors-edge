#include "Interaction/BreakerSupplyChest.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/BreakerSupplyChestMath.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Items/BreakerLootPickup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Progression/BreakerRiftRewardMath.h"
#include "UI/BreakerChestFeedback.h"
#include "UI/BreakerGlowMaterial.h"
#include "UI/BreakerUIStyle.h"

ABreakerSupplyChest::ABreakerSupplyChest()
{
    bReplicates = true;
    ConfigureConsoleBody();
    // A CHEST, NOT A CONSOLE. The console body is the right components and the
    // wrong proportions: wide, low and lidded reads as something you open,
    // where tall and narrow reads as something you operate. This cube is the
    // FALLBACK, shown only when the crate props below fail to load.
    if (Visual)
    {
        Visual->SetRelativeScale3D(FVector(0.90f, 0.62f, 0.42f)); // O2 PLACEHOLDER
        Visual->SetRelativeLocation(FVector(0.0f, 0.0f, -62.0f)); // O2 PLACEHOLDER
    }
    // The band's fallback placement: the cube's top edge. Its GOLD is written
    // in BeginPlay, after the NPC base has dressed it as a sash.
    if (Trim)
    {
        Trim->SetRelativeRotation(FRotator::ZeroRotator);
        Trim->SetRelativeScale3D(FVector(0.96f, 0.68f, 0.07f)); // O2 PLACEHOLDER
        Trim->SetRelativeLocation(FVector(0.0f, 0.0f, -34.0f)); // O2 PLACEHOLDER
    }
    float LidTopCm = GlintLidTopCm;

    // O280: THE CRATE. The composer's body stands on the capsule's bottom and
    // replaces the cube; the lid hangs from BodyMesh at the hinge. BodyMeshAsset
    // stays unset on purpose: ApplyBodyMesh returns early without it, so the
    // base class never re-fits or re-hides what is placed here.
    //
    // COLLISION, RECORDED NOT FAKED: the Body capsule is 34 cm in radius —
    // site collection reads that radius, so it is not touched — and a ~1.10 m
    // crate overhangs a 68 cm capsule by ~21 cm at each end. Those ends do not
    // block the player. A crate-shaped blocker is a separate ruling.
    UStaticMesh* CrateBody = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Breaker/Meshes/props/props/StaticMeshes/prop_chest_body.prop_chest_body"));
    UStaticMesh* CrateLid = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Breaker/Meshes/props/props/StaticMeshes/prop_chest_lid.prop_chest_lid"));
    Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
    Lid->SetupAttachment(BodyMesh);
    Lid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Lid->SetVisibility(false);
    if (BodyMesh && CrateBody)
    {
        BodyMesh->SetStaticMesh(CrateBody);
        BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CrateFloorCm));
        // The composer keeps the kit's native facing: the lid's free edge is
        // the body's +Y. Yawed -90 so the lip faces +X, the side the chest
        // faces toward the approach and drops its pickup on.
        BodyMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        BodyMesh->SetVisibility(true);
        if (Visual) Visual->SetVisibility(false);

        // THE SEAM AND THE HINGE. The seam is the body's top. The composer
        // exports the lid with its mesh origin at the hinge — the back-bottom
        // edge of the lid, which sits at the body's -Y face (in the body's own
        // frame, before the yaw) at the composer's hinge height, 0.302 of a
        // 0.534 m body: the lid's vertices hang forward (+Y) from there over
        // the body. Read off the body's bounds, never typed.
        const FBoxSphereBounds Bounds = CrateBody->GetBounds();
        const bool bHasBounds = Bounds.BoxExtent.Z > KINDA_SMALL_NUMBER;
        const float SeamCm = bHasBounds ? CrateFloorCm + Bounds.Origin.Z + Bounds.BoxExtent.Z : SeamFallbackCm;
        const float BackCm = bHasBounds ? Bounds.Origin.Y - Bounds.BoxExtent.Y : 0.0f;
        const float HingeCm = bHasBounds ? (Bounds.Origin.Z + Bounds.BoxExtent.Z) * HingeHeightFraction : SeamCm - CrateFloorCm;
        LidTopCm = SeamCm;
        if (Trim)
        {
            // The gold band, wrapped around the seam: the body's footprint plus
            // a clearance each side, one band-thickness tall, centred on the seam.
            // The band hangs off the root, the body is yawed -90 under it:
            // the body's Y is the actor's X.
            const FVector Wrap = bHasBounds
                ? FVector((2.0f * Bounds.BoxExtent.Y + 2.0f * BandClearanceCm) / 100.0f,
                          (2.0f * Bounds.BoxExtent.X + 2.0f * BandClearanceCm) / 100.0f, BandThickness)
                : FVector(BandFallbackLong, BandFallbackWide, BandThickness);
            Trim->SetRelativeScale3D(Wrap);
            // The band rides the visible seam — the hinge line, where lid
            // meets body — not the rim.
            Trim->SetRelativeLocation(FVector(0.0f, 0.0f, CrateFloorCm + HingeCm));
        }
        if (CrateLid)
        {
            Lid->SetStaticMesh(CrateLid);
            Lid->SetRelativeLocation(FVector(0.0f, BackCm, HingeCm));
            Lid->SetVisibility(true);
            const FBoxSphereBounds LidBounds = CrateLid->GetBounds();
            LidTopCm = SeamCm + LidBounds.Origin.Z + LidBounds.BoxExtent.Z;
        }
    }

    // THE MOTE, so a chest can be seen from the lane. Created here because
    // SetupAttachment is constructor-only; its material is made in BeginPlay,
    // where a dynamic instance belongs. It floats above whichever lid shipped.
    PrimaryActorTick.bCanEverTick = true;
    Glint = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Glint"));
    Glint->SetupAttachment(GetRootComponent());
    Glint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Glint->SetCastShadow(false);
    Glint->SetRelativeLocation(FVector(0.0f, 0.0f, LidTopCm + GlintHeightCm));
    Glint->SetRelativeScale3D(FVector(GlintSizeCm / 100.0f));
    if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
    {
        Glint->SetStaticMesh(Sphere);
    }

    DisplayName = FText::FromString(TEXT("Supply chest"));
    DialogueId = NAME_None;
    DialogueNodes.Reset();
    EntryOverrides.Reset();
    Tags.Add(TEXT("Fernhall.Chest"));
}

void ABreakerSupplyChest::BeginPlay()
{
    Super::BeginPlay();
    // GOLD, because O179 spends gold on reward and a chest is nothing else.
    // Written HERE, after Super: ABreakerNPC::BeginPlay dresses every Trim in
    // the amber sash material, and a gold instance made in the constructor was
    // being replaced by it — the band shipped amber for as long as it lived
    // there.
    if (Trim)
    {
        if (UMaterialInstanceDynamic* Band = Trim->CreateDynamicMaterialInstance(0))
        {
            Band->SetVectorParameterValue(TEXT("Color"), BreakerUI::Gold);
        }
    }
    // UNLIT ADDITIVE, so the mote reads as light across a 56 m yard rather
    // than as a painted ball that goes grey in a building's shadow. A plain
    // mesh component, so the additive glow actually draws on it — it does not
    // declare instanced usage, which is what made the pocket tear black.
    if (Glint)
    {
        GlintMaterial = BreakerUI::MakeGlowMaterial(Glint);
        if (GlintMaterial) Glint->SetMaterial(0, GlintMaterial);
    }
}

void ABreakerSupplyChest::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bOpened)
    {
        // SPENT, AND IT SAYS SO. An opened chest that kept glinting would send
        // the player back across the yard to something they already took.
        if (Glint) Glint->SetVisibility(false);
        // THE LID SWINGS, from shut to LidOpenRotation over LidOpenSeconds,
        // and stays there. bOpened replicates, so a client's lid swings on the
        // same tick its prompt goes away. A zero duration snaps it open.
        if (Lid && LidOpenAge <= LidOpenSeconds)
        {
            LidOpenAge += DeltaSeconds;
            const float Alpha = LidOpenSeconds > KINDA_SMALL_NUMBER
                ? FMath::Clamp(LidOpenAge / LidOpenSeconds, 0.0f, 1.0f) : 1.0f;
            Lid->SetRelativeRotation(LidOpenRotation * Alpha);
        }
        // THE FADE: once the pickup it paid is taken (the pickup destroys
        // itself on transfer), the chest waits FadeDelaySeconds, shrinks to
        // nothing over FadeSeconds and leaves. Server-authoritative — the
        // actor's destruction replicates.
        if (HasAuthority() && bPaid && !PaidPickup.IsValid())
        {
            if (FadeAge < 0.0f) { FadeAge = 0.0f; FadeScale = GetActorScale3D(); }
            FadeAge += DeltaSeconds;
            const float Shrink = FMath::Clamp((FadeAge - FadeDelaySeconds) / FMath::Max(FadeSeconds, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
            if (Shrink > 0.0f) SetActorScale3D(FadeScale * (1.0f - Shrink));
            if (Shrink >= 1.0f) Destroy();
        }
        return;
    }
    if (!Glint) return;
    GlintAge += DeltaSeconds;
    const float Breath = 0.5f + 0.5f * FMath::Sin(GlintAge * GlintHz * 2.0f * PI);
    if (GlintMaterial)
    {
        BreakerUI::SetGlowColor(GlintMaterial, BreakerUI::Gold, FMath::Lerp(GlintLow, GlintHigh, Breath));
    }
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
    ItemLevel = BreakerSupplyChest::ChestItemLevel(AreaLevel);
    ContentSeed = Seed;
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
    // O275: opening one is a director verb. GLASS owns the sound; this is the
    // declared Interaction->UI crossing, on BreakerRift.cpp's precedent.
    BreakerChestFeedback::PlayOpen(this, Player);

    // O275: BOTH, always. The currency floor first — credited straight to the
    // wallet rather than dropped, because there is no physical currency pickup
    // in this project and inventing one here would be a nearest-fit primitive
    // for a thing nobody has ruled on. The shipped trash roll is the base the
    // floor sits under, so a retuned kill number carries the chest with it.
    const FBreakerForgeWallet PerKill = UBreakerDropTableLibrary::RollCurrencyDrop(
        ContentSeed, ItemLevel, EBreakerMonsterRank::Trash, FBreakerCurrencyDropParams());
    FBreakerForgeWallet Yield;
    Yield.Riftglass = BreakerSupplyChest::CurrencyPayout(PerKill.Get(), ItemLevel);
    Player->GetEquipment()->CreditForgeCurrency(Yield);

    // AND ONE ITEM AT THE COMPLETION FLOOR — the codebase's one "decent"
    // rarity, gated by the chest's own item level exactly as a rift's offer
    // is. Not a roll: the seed picks the slot and the affixes, never whether
    // the item is worth bending down for.
    const EBreakerItemRarity Rarity = BreakerRiftReward::CompletionRarity(ItemLevel, FBreakerDropTableParams{});
    const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("SupplyChest"),
        UBreakerLootLibrary::RollDropSlot(ContentSeed), Rarity, ItemLevel, ContentSeed);
    // On the approach side, the same offset the cache uses.
    const FVector At = GetActorLocation() + GetActorForwardVector() * 120.0f - FVector(0, 0, 40.0f);
    ABreakerLootPickup* Pickup = GetWorld()->SpawnActor<ABreakerLootPickup>(
        ABreakerLootPickup::StaticClass(), At, FRotator::ZeroRotator);
    if (!Pickup) return false;
    Pickup->SetItem(Item);
    PaidPickup = Pickup;
    bPaid = true;
    return true;
}
