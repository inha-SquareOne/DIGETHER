#pragma once

#include "CoreMinimal.h"
#include "Game/Save/DigetherSaveTypes.h"
#include "GameFramework/SaveGame.h"
#include "DigetherSaveGame.generated.h"

// 엔진 SaveGame 헤더 안에 v3 값과 CRC를 보관한다. 로드 대상 클래스는 저장 계층이 고정한다.
UCLASS(NotBlueprintable)
class DIGETHER_API UDigetherSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	virtual void Serialize(FArchive& Archive) override;

	EDigetherSaveResult SetSlotData(const FDigetherSaveSlotData& Slot);
	const FDigetherSaveSlotData& GetSlotData() const;
	EDigetherSaveResult GetDataResult() const { return DataResult; }
	EDigetherSaveResult EncodeToMemory(TArray<uint8>& OutBytes) const;
	EDigetherSaveResult DecodeFromMemory(const TArray<uint8>& Bytes);

private:
	FDigetherSaveSlotData Data;
	EDigetherSaveResult DataResult = EDigetherSaveResult::InvalidState;
};
