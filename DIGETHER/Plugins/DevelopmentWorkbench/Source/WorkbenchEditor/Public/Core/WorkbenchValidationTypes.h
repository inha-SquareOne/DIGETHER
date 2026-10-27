// 검증 결과를 UI·commandlet·외부 Validator가 공유하는 공통 타입.
// §6.1 설계: Valid / Invalid / NotValidated / 결정 대기 / 지원 안 함 / 전체 미검사 / stale 구분.
#pragma once

#include "CoreMinimal.h"

/** 하나의 대상에 대한 하나의 검사 규칙의 결과 상태. */
enum class EWorkbenchValidationState : uint8
{
	/** 규칙을 평가했고 통과했다. */
	Valid,

	/** 규칙을 평가했고 위반했다. */
	Invalid,

	/** 아직 평가하지 않았다(예: 지연 로드 전). */
	NotValidated,

	/** 판단에 필요한 프로젝트 정책이 아직 승인되지 않았다. 통과로 표시하지 않는다. */
	PendingDecision,

	/** 이 도구가 아직 지원하지 않는 타입/구조다. */
	Unsupported,

	/** 이 규칙이 요구하는 전역 인덱스·범위를 이번 실행에서 검사하지 못했다. */
	NotChecked,

	/** 외부 변경으로 평가에 쓴 스냅샷이 더 이상 최신이 아니다. 재조회 전에는 신뢰할 수 없다. */
	Stale
};

/** 심각도. Invalid 규칙이라도 Error/Warning으로 나눠 적용 가능 여부를 다르게 다룬다. */
enum class EWorkbenchValidationSeverity : uint8
{
	Error,
	Warning,
	Info
};

/** 검사 규칙 1건의 평가 결과. 규칙 ID·버전·기획 근거를 보존한다. */
struct WORKBENCHEDITOR_API FWorkbenchValidationMessage
{
	/** 규칙의 안정적인 ID. 예: "Item.DuplicateDefinitionId". */
	FName RuleId;

	/** 이 규칙 구현의 버전. 규칙 로직이 바뀌면 올려 과거 결과와 구분한다. */
	int32 RuleVersion = 1;

	/** 검사 대상을 식별하는 ID(행 이름·DefinitionId 등). */
	FName TargetId;

	EWorkbenchValidationState State = EWorkbenchValidationState::NotValidated;
	EWorkbenchValidationSeverity Severity = EWorkbenchValidationSeverity::Error;

	/** 사용자에게 보여줄 한국어 설명. */
	FText Description;

	/**
	 * 이 규칙이 근거로 삼는 기획 개정 식별자. 승인된 정책이 없으면 NAME_None이며
	 * 그 경우 State는 PendingDecision이어야 한다(정책 부재를 Valid로 표시하지 않는다).
	 */
	FName PlanningRevision;

	bool IsBlocking() const
	{
		return State == EWorkbenchValidationState::Invalid && Severity == EWorkbenchValidationSeverity::Error;
	}
};
