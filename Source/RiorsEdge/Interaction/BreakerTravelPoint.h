#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakerTravelPoint.generated.h"

// One entry in the travel registry: a place the interactable can send a
// player. Zero-setup convention (see BreakerAbilityDefinition::GetFallbackRegistry
// and BreakerProgressionLibrary's tree registry) — this ships as a C++
// fallback list, not content assets, so the slice needs no editor setup to
// have destinations at all.
//
// bEnabled exists because the owner's brief is explicit: a destination that
// is not real yet is DATA-ABSENT, not authored-and-disabled. The flag stays
// on the struct because a destination will need to go from disabled to
// enabled later without a registry reshape — but do not add fake entries to
// exercise it today.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerTravelDestination
{
    GENERATED_BODY()

    // Stable id. Never renamed once shipped, same rule as BreakerQuestFlags —
    // anything that keys a save, a delegate payload, or a UI selection is a
    // save-compatibility surface even if nothing serializes it directly yet.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Travel") FName Id = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Travel") FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Travel") FString Description;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Travel") bool bEnabled = true;

    // DOOR-ONLY. A destination that exists but is offered by ONE actor rather
    // than by every gate: the rift is entered through the door standing in
    // Fernhall, not chosen from a list at the Anchor. Distinct from bEnabled,
    // which answers "is this place real" — a door-only destination IS real, it
    // just is not somewhere you go from anywhere. The base point filters these
    // out; the actor that IS the door overrides GetAvailableDestinations and
    // offers its own.
    //
    // It stays in the ONE registry rather than living on the door, so
    // SelectDestination still validates every id against a single list and a
    // stale selection still fails closed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Travel") bool bDoorOnly = false;
};

// Raised when a player picks a destination through SelectDestination. The
// travel point does not teleport anyone itself — it does not know where the
// gym's playtest frame is (that lives in ABreakerGameMode, out of this
// actor's territory) — so it hands the choice to whoever binds this and lets
// that owner perform the actual travel. Plain multicast delegate, matching
// UBreakerQuestJournal's FBreakerQuestFlagChanged rather than a dynamic
// BlueprintAssignable, since the only bindable owner is C++ (the game mode).
DECLARE_MULTICAST_DELEGATE_TwoParams(FBreakerTravelRequested, FName /*DestinationId*/, APawn* /*RequestingPawn*/);

// The interactable that stands in for "other maps, quests, missions" from the
// hub. An F-key interactable in the same idiom as ABreakerNPC (a capsule body,
// an InteractionRange, nearest-in-range lookup done by the caller) but it is
// not an ABreakerNPC — it has no dialogue, it has a destination list — so it
// is its own actor rather than an NPC wearing a travel hat.
UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerTravelPoint : public AActor
{
    GENERATED_BODY()

public:
    ABreakerTravelPoint();
    // Presentation: paints the rift-teal identity onto the shapes. Dynamic
    // material instances need a live world, so not in the constructor.
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintPure, Category="Travel") float GetInteractionRange() const { return InteractionRange; }
    UFUNCTION(BlueprintPure, Category="Travel") FText GetPromptLabel() const { return PromptLabel; }

    // --- PUBLISHED: what a travel point CALLS ITSELF ----------------------
    // Owner GROUND, consumer GLASS. The overhead label was a literal "TRAVEL"
    // drawn over an "F TRAVEL" prompt, so the first new interactable in the
    // world said two different things about itself, one of them redundant.
    // The NPC idiom is the right one: a noun over the verb.
    //
    // VIRTUAL, so a door names the place it opens rather than its own kind —
    // the same dispatch that lets a rift door be a travel point at all. The
    // base returns its prompt's noun because a general gate IS travel; there
    // is no better name for it than what it does.
    UFUNCTION(BlueprintPure, Category="Travel")
    virtual FText GetDisplayName() const { return PromptLabel; }

    // The second line, empty on a general gate. A rift door fills it with its
    // own difficulty — "AREA 23" means level-23 content now that the required
    // level derives from item level, so this one number is the whole gauge.
    // GROUND owns the number; GLASS decides how it draws.
    UFUNCTION(BlueprintPure, Category="Travel")
    virtual FText GetDisplayDetail() const { return FText::GetEmpty(); }

    // Enabled destinations only, in registry order. The UI iterates this
    // instead of the raw registry so an absent/disabled destination can never
    // render as a choice.
    // VIRTUAL, and that is what lets a rift door be a travel point rather than
    // a second interactable. ABreakerCharacter finds travel points with a
    // TActorIterator over this class and the menu calls both of these through
    // a base pointer, so a subclass reaches the F key and the picker with no
    // change in Characters/ or UI/ — neither of which this lane owns.
    UFUNCTION(BlueprintPure, Category="Travel")
    virtual TArray<FBreakerTravelDestination> GetAvailableDestinations() const;

    // False for an unknown id or a disabled one — both refused the same way,
    // so a stale UI selection (built before a destination was disabled) fails
    // closed instead of firing travel to nowhere. True means the delegate
    // fired; the requester is responsible for what travel actually does.
    UFUNCTION(BlueprintCallable, Category="Travel")
    virtual bool SelectDestination(FName DestinationId, APawn* RequestingPawn);

    FBreakerTravelRequested OnDestinationSelected;

    // Zero-setup fallback registry: the gym, the hub, and the Fernhall
    // approach. See the struct comment above for why the rest of the owner's
    // requested destinations ("other maps, quests, missions") are not
    // represented here at all.
    static const TArray<FBreakerTravelDestination>& GetFallbackRegistry();
    static bool FindDestination(FName DestinationId, FBreakerTravelDestination& OutDestination);

    // The shipped destinations, exposed as stable ids so the game mode does
    // not have to hardcode strings to bind travel — a rename here is a
    // compile error at every binding site instead of a silent mismatch.
    static const FName GymDestinationId;
    static const FName HubDestinationId;
    static const FName FernhallDestinationId;
    // The Local Rift. DOOR-ONLY: it is in the registry so ids validate in one
    // place, and it is offered by ABreakerRiftDoor alone.
    static const FName RiftDestinationId;
    // Which destination this point IS, so it does not offer to send the
    // player where they already are. Set by whoever spawns it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Travel") FName ExcludedDestinationId;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UCapsuleComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Visual;
    // THE BEACON (owner playtest 2026-08-17: "i dont know where the travel
    // thing is in the anchor i simply just clicked around"). A tall unlit
    // additive light column in the reserved rift-teal — teal is canon-reserved
    // for rift objects, and travel IS the rift verb — plus a teal point light,
    // so the gate is findable from anywhere on the plaza without hunting. The
    // overhead "TRAVEL" label rides in the HUD with the NPC name labels.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Beacon;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UPointLightComponent> BeaconLight;

    // O2 PLACEHOLDER — layout, not balance, but sized like the NPC default
    // (BreakerNPC::InteractionRange) so the two interactable kinds feel the
    // same to walk up to.
    // WIDENED FROM 300 (ruled, defect (d)): hub arrival lands the player at
    // 320 cm from this point — twenty centimetres OUTSIDE the old radius of
    // the only interactive object in the room. A gate with a fourteen-metre
    // beacon is not a person; its reach is the threshold, not an arm. 450
    // puts the arriving player inside the radius with the prompt already
    // up, which is also how F gets taught. O2 PLACEHOLDER.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Travel", meta=(ClampMin="50")) float InteractionRange = 450.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Travel") FText PromptLabel = FText::FromString(TEXT("Travel"));
};
