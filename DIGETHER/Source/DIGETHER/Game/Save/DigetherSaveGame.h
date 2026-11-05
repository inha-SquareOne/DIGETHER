#pragma once

#include "CoreMinimal.h"
#include "Game/Save/DigetherSaveTypes.h"
#include "GameFramework/SaveGame.h"
#include "DigetherSaveGame.generated.h"

// 엔진 SaveGame 헤더 안에 v3 진행 값의 wire payload(쓰기 v5, 읽기 v3·v4·v5)와 CRC를 보관한다.
// 진행 스키마·슬롯 이름과 wire 버전은 별개이며 로드 대상 클래스는 저장 계층이 고정한다.
UCLASS(NotBlueprintable)
class DIGETHER_API UDigetherSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	virtual void Serialize(FArchive& Archive) override;

	EDigetherSaveResult SetSlotData(const FDigetherSaveSlotData& Slot);
	const FDigetherSaveSlotData& GetSlotData() const;
	EDigetherSaveResult GetDataResult() const { return DataResult; }
	// SetSlotData가 검증·직렬화한 바이트가 있으면 다시 직렬화하지 않고 돌려준다.
	EDigetherSaveResult EncodeToMemory(TArray<uint8>& OutBytes) const;
	EDigetherSaveResult DecodeFromMemory(const TArray<uint8>& Bytes);

private:
	FDigetherSaveSlotData Data;
	EDigetherSaveResult DataResult = EDigetherSaveResult::InvalidState;
	// Data를 SerializeSlot한 결과다. Data를 바꾸는 경로(SetSlotData·로드)에서 함께 갱신하거나 비운다.
	TOptional<TArray<uint8>> EncodedPayload;
};
