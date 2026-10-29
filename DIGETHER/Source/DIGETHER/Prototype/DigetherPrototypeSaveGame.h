#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DigetherPrototypeTypes.h"
#include "DigetherPrototypeSaveGame.generated.h"

USTRUCT()
struct FPrototypeSavedWorldItem
{
	GENERATED_BODY()

	UPROPERTY()
	FPrototypeItem Item;

	UPROPERTY()
	FTransform Transform;
};

UCLASS()
class DIGETHER_API UDigetherPrototypeSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 2;

	UPROPERTY()
	FPrototypePublicState PublicState;

	UPROPERTY()
	TMap<FString, FPrototypePrivateState> Players;

	UPROPERTY()
	TArray<FPrototypeSavedWorldItem> ReturnedEquipment;
};
