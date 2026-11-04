#pragma once

#include "CoreMinimal.h"
#include "Game/Save/DigetherSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DigetherSaveSubsystem.generated.h"

struct FUniqueNetIdRepl;

// 호스트가 모은 값만 다룬다. 월드 객체를 보관하거나 참가자에게 저장 내용을 복제하지 않는다.
UCLASS()
class DIGETHER_API UDigetherSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// 서버 연결층에서 인증한 호스트 ID를 공급한다. 선택 계정의 해시가 주·백업 슬롯 이름을 정한다.
	EDigetherSaveResult BindHostIdentity(const FUniqueNetIdRepl& HostUniqueId);
	EDigetherSaveResult Write(const FDigetherSaveSnapshot& Snapshot, bool bExplicitNewGame, bool bCheckpoint);
	EDigetherSaveResult Read(FDigetherSaveSnapshot& OutSnapshot, bool bCheckpoint = false);
	bool IsCheckpointCurrent(const FDigetherSaveSnapshot& Current) const;

private:
	bool HasHostAuthority() const;
	EDigetherSaveResult ReadSlot(FDigetherSaveSlotData& OutSlot) const;

	FString HostAuthenticatedPlayerKey;
	TOptional<FDigetherSaveSlotData> LastWrittenSlot;
};
