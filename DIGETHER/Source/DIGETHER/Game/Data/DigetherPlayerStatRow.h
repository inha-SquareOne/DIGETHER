#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DigetherPlayerStatRow.generated.h"

// 플레이어 이동·체형 수치 표의 행, 행 이름이 프로필 이름(현재 Default 하나)
// 값 원본은 Content/Data/Player/DT_PlayerStats.csv, 코드 기본값 없음(미설정 0은 범위 검사에서 걸러야 함)
// 아래 수치는 TODO 목록 기준이며 D26 기획 확인 전까지 확정값이 아니다
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherPlayerStatRow : public FTableRowBase
{
	GENERATED_BODY()

	// 걷기 최대 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|PlayerStat", meta=(ClampMin="0", Units="cm/s"))
	float WalkSpeed = 0.f;

	// 달리기 최대 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|PlayerStat", meta=(ClampMin="0", Units="cm/s"))
	float SprintSpeed = 0.f;

	// 웅크리기 최대 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|PlayerStat", meta=(ClampMin="0", Units="cm/s"))
	float CrouchSpeed = 0.f;

	// 점프 시작 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|PlayerStat", meta=(ClampMin="0", Units="cm/s"))
	float JumpZVelocity = 0.f;

	// 중력 배율(엔진 기본 1.0 기준)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|PlayerStat", meta=(ClampMin="0"))
	float GravityScale = 0.f;

	// 캐릭터 스케일 X·Y·Z
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|PlayerStat", meta=(ClampMin="0.1"))
	float ScaleX = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|PlayerStat", meta=(ClampMin="0.1"))
	float ScaleY = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|PlayerStat", meta=(ClampMin="0.1"))
	float ScaleZ = 0.f;

	// 서 있을 때 통과 가능한 폭(82×165cm 중 폭)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|PlayerStat", meta=(ClampMin="1", Units="cm"))
	float PassWidthCm = 0.f;

	// 서 있을 때 통과 가능한 높이(82×165cm 중 높이)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|PlayerStat", meta=(ClampMin="1", Units="cm"))
	float PassHeightCm = 0.f;

	// 앉았을 때 높이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|PlayerStat", meta=(ClampMin="1", Units="cm"))
	float CrouchHeightCm = 0.f;
};
