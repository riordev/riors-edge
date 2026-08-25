#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BreakerInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS(BlueprintType, Const)
class RIORSEDGE_API UBreakerInputConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputMappingContext> DefaultMappingContext;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> Move;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> Look;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> Jump;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> Sprint;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> Dash;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> Slide;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> Fire;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> Aim;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> Reload;
    // Talk / travel / take — the one contextual verb (ruled out of its raw
    // BindKey so it appears in the keybind list like everything else).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> Interact;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Abilities") TObjectPtr<UInputAction> AbilityOne;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Abilities") TObjectPtr<UInputAction> AbilityTwo;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Abilities") TObjectPtr<UInputAction> Ultimate;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Playtest") TObjectPtr<UInputAction> PlaytestReset;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Playtest") TObjectPtr<UInputAction> PlaytestReport;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Playtest") TObjectPtr<UInputAction> PlaytestDiagnostics;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Settings") TObjectPtr<UInputAction> FOVUp;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Settings") TObjectPtr<UInputAction> FOVDown;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Settings") TObjectPtr<UInputAction> SensitivityUp;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Settings") TObjectPtr<UInputAction> SensitivityDown;
};
