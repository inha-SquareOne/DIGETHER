#pragma once

#include "CoreMinimal.h"
#include "Game/Save/DigetherSaveSlotTypes.h"
#include "Game/Save/DigetherSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DigetherSaveSubsystem.generated.h"

struct FUniqueNetIdRepl;

// 호스트가 모은 값만 다룸, 월드 객체 보관·참가자 복제 없음
// 읽기·쓰기는 방 생성 때 고른 진행 슬롯(SlotSelection) 하나만 대상, 다른 슬롯은 목록 조회로만 읽음
UCLASS()
class DIGETHER_API UDigetherSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// 서버 연결층이 인증한 호스트 ID 공급, 이 계정 해시가 주·백업 슬롯 이름을 결정
	EDigetherSaveResult BindHostIdentity(const FUniqueNetIdRepl& HostUniqueId);
	// 선택한 진행 슬롯에 쓰기, 게임 오버 뒤 새 게임 초기화도 이 경로라 현재 슬롯만 변경(S30 208행)
	EDigetherSaveResult Write(const FDigetherSaveSnapshot& Snapshot, bool bExplicitNewGame);
	// 선택한 진행 슬롯에서 읽기
	EDigetherSaveResult Read(FDigetherSaveSnapshot& OutSnapshot);

	// 방 생성 시(메인 메뉴, 독립 실행 월드) 호스트가 고른 슬롯·시작 방식 지정, 게임 맵 GameMode 시작 시 사용
	// 방 안(리슨 서버)·클라이언트에서는 변경 거절, 진행 중 자동 저장이 다른 슬롯으로 새는 것 방지
	// 범위 밖 슬롯 번호는 false, 기존 선택 유지
	UFUNCTION(BlueprintCallable, Category="Digether|Save")
	bool SetSlotSelection(const FDigetherSaveSlotSelection& Selection);

	UFUNCTION(BlueprintPure, Category="Digether|Save")
	FDigetherSaveSlotSelection GetSlotSelection() const { return SlotSelection; }

	// 진행 슬롯 수(임시 상수, 기획 미정), 슬롯 선택 화면 칸 수용
	UFUNCTION(BlueprintPure, Category="Digether|Save")
	static int32 GetMaxSlotCount();

	// 모든 진행 슬롯의 존재·요약 읽기, 호스트(독립 실행·리슨 서버) 전용, 클라이언트는 빈 배열
	// 호스트 계정은 바인딩된 ID(쓰기와 같은 키), 없으면 첫 로컬 플레이어 온라인 ID 사용(바인딩은 유지)
	// 디스크 동기 읽기라 메뉴 열기·새로 고침 때만 호출
	UFUNCTION(BlueprintCallable, Category="Digether|Save")
	TArray<FDigetherSaveSlotSummary> ListSlots() const;

	// 새 게임 초기화 저장 후 GameMode가 호출, 같은 방에서 게임 맵을 다시 열 때 재초기화하지 않도록
	// 같은 슬롯 이어하기로 전환
	void CompleteNewGameRequest();

private:
	bool HasHostAuthority() const;
	EDigetherSaveResult ReadSlot(FDigetherSaveSlotData& OutSlot) const;
	FString ResolveListingHostKey() const;

	FString HostAuthenticatedPlayerKey;
	FDigetherSaveSlotSelection SlotSelection;
};
