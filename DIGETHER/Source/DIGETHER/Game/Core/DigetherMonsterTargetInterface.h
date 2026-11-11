#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DigetherMonsterTargetInterface.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UDigetherMonsterTargetInterface : public UInterface
{
	GENERATED_BODY()
};

// 몬스터가 플레이어 Pawn에 요구하는 접점, 구현은 플레이어 담당(정식 캐릭터)
// 구현하지 않은 Pawn에는 감속을 적용하지 않고 매몰 아님으로 간주(몬스터 쪽은 호출만)
class DIGETHER_API IDigetherMonsterTargetInterface
{
	GENERATED_BODY()

public:
	// 서버, 이동 속도 배율 추가(거미줄 감속 50% 등), 같은 Source는 교체
	virtual void AddMoveSpeedMultiplier(const UObject* Source, float Multiplier) = 0;

	// 서버, Source가 준 배율 제거
	virtual void RemoveMoveSpeedMultiplier(const UObject* Source) = 0;

	// 매몰 중이면 땅거미 접촉 피해 제외(기획)
	virtual bool IsBuriedForMonster() const = 0;
};
