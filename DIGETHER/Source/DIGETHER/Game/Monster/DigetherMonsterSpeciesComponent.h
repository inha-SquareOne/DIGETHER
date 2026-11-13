#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigetherMonsterSpeciesComponent.generated.h"

class UDigetherMonsterDefinition;
struct FDigetherMonsterStimulusEvent;

// 종별 규칙 컴포넌트의 부모, 새 종은 이 하위 클래스 + 종 파라미터 구조체로 추가
// 상태·전이는 하위 클래스, 행동 순서는 BT(종 컴포넌트가 Blackboard에 기록)
UCLASS(Abstract, ClassGroup=(Digether))
class DIGETHER_API UDigetherMonsterSpeciesComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherMonsterSpeciesComponent();

	// 정의의 SpeciesParams가 이 타입(또는 하위)이어야 함, null이면 파라미터 불필요
	virtual const UScriptStruct* GetRequiredParamsStruct() const { return nullptr; }

	// 서버: 캐릭터 정의 주입 직후 BeginPlay 전 1회, 실패하면 캐릭터가 생성 실패로 처리
	// 클라이언트: 복제된 정의를 받을 때마다 표현용으로 호출(결과 무시)
	virtual bool InitializeSpecies(const UDigetherMonsterDefinition& Definition);

	// 서버, 감각 서브시스템이 0.1초 주기마다 모은 자극 전달
	virtual void HandleStimuli(TConstArrayView<FDigetherMonsterStimulusEvent> Events) {}

	const UDigetherMonsterDefinition* GetDefinition() const { return Definition; }

protected:
	UPROPERTY(Transient)
	TObjectPtr<const UDigetherMonsterDefinition> Definition;
};
