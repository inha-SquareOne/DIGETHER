#pragma once

#include "CoreMinimal.h"
#include "Game/Save/DigetherSaveSlotTypes.h"
#include "Game/Save/DigetherSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DigetherSaveSubsystem.generated.h"

struct FUniqueNetIdRepl;

// 호스트가 모은 값만 다룬다. 월드 객체를 보관하거나 참가자에게 저장 내용을 복제하지 않는다.
// 읽기·쓰기는 방 생성 때 고른 진행 슬롯(SlotSelection) 하나만 대상으로 한다. 다른 슬롯은 목록 조회로만 읽는다.
UCLASS()
class DIGETHER_API UDigetherSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// 서버 연결층에서 인증한 호스트 ID를 공급한다. 선택 계정의 해시가 주·백업 슬롯 이름을 정한다.
	EDigetherSaveResult BindHostIdentity(const FUniqueNetIdRepl& HostUniqueId);
	// 선택한 진행 슬롯에 쓴다. 게임 오버 뒤 새 게임 초기화도 이 경로라 현재 슬롯만 바뀐다(S30 208행).
	EDigetherSaveResult Write(const FDigetherSaveSnapshot& Snapshot, bool bExplicitNewGame);
	// 선택한 진행 슬롯에서 읽는다.
	EDigetherSaveResult Read(FDigetherSaveSnapshot& OutSnapshot);

	// 방을 만들 때(메인 메뉴, 독립 실행 월드) 호스트가 고른 슬롯·시작 방식을 정한다. 게임 맵의 GameMode가 시작할 때 쓴다.
	// 방 안(리슨 서버)·클라이언트에서는 바꾸지 않는다. 진행 중 자동 저장이 다른 슬롯으로 새지 않게 하려는 제한이다.
	// 슬롯 번호가 범위를 벗어나면 false이고 기존 선택을 유지한다.
	UFUNCTION(BlueprintCallable, Category="Digether|Save")
	bool SetSlotSelection(const FDigetherSaveSlotSelection& Selection);

	UFUNCTION(BlueprintPure, Category="Digether|Save")
	FDigetherSaveSlotSelection GetSlotSelection() const { return SlotSelection; }

	// 진행 슬롯 수(임시 상수, 기획 미정)다. 슬롯 선택 화면이 칸 수를 정할 때 쓴다.
	UFUNCTION(BlueprintPure, Category="Digether|Save")
	static int32 GetMaxSlotCount();

	// 모든 진행 슬롯의 존재·요약을 읽는다. 호스트(독립 실행·리슨 서버)에서만 읽으며 클라이언트에서는 빈 배열이다.
	// 호스트 계정은 바인딩된 ID가 있으면 그것을(쓰기와 같은 키), 없으면 첫 로컬 플레이어의 온라인 ID를 쓴다(바인딩은 바꾸지 않는다).
	// 디스크를 동기로 읽으므로 메뉴를 열 때·새로 고칠 때만 부른다.
	UFUNCTION(BlueprintCallable, Category="Digether|Save")
	TArray<FDigetherSaveSlotSummary> ListSlots() const;

	// 새 게임 초기화를 저장했으면 GameMode가 부른다. 같은 방에서 게임 맵을 다시 열 때 또 초기화하지 않도록
	// 같은 슬롯의 이어하기로 바꾼다.
	void CompleteNewGameRequest();

private:
	bool HasHostAuthority() const;
	EDigetherSaveResult ReadSlot(FDigetherSaveSlotData& OutSlot) const;
	FString ResolveListingHostKey() const;

	FString HostAuthenticatedPlayerKey;
	FDigetherSaveSlotSelection SlotSelection;
};
