#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

// 엔진 서브시스템 없이 단위 테스트 가능한 순수 계산만 모은다.
// T06/T07/T09/T11 패널은 이 함수들로 계획·검사를 만들고, Automation Tests도 같은 함수를 검증한다.
namespace WorkbenchToolsAlgo
{
	// ---- T09: 에셋 이름 변경 계획 ----

	struct WORKBENCHEDITOR_API FRenameCandidate
	{
		FString PackagePath; // 예: "/Game/Items"
		FString AssetName;   // 예: "Sword_01"
		bool bReadOnly = false;
		int32 ReferencerCount = 0; // 0이면 "참조 없음 후보"일 뿐 삭제 보장은 아니다.
		// true면 이름만 점유한다(리다이렉터·맵 등 이름 변경 대상이 아닌 에셋). 계획 항목은 만들지 않는다.
		bool bOccupyOnly = false;
	};

	enum class ERenamePlanStatus : uint8
	{
		Ok,
		NoChange,
		Collision,
		ReadOnly,
	};

	struct WORKBENCHEDITOR_API FRenamePlanEntry
	{
		FString PackagePath;
		FString OldName;
		FString NewName;
		ERenamePlanStatus Status = ERenamePlanStatus::Ok;
		bool bNoReferencersFound = false;
	};

	// 영문·숫자·밑줄만 허용, 길이 1..32. 에셋 이름에 쓸 수 없는 문자를 사전에 막는다.
	WORKBENCHEDITOR_API bool IsValidAssetNamePrefix(const FString& Prefix);

	// Prefix가 이미 붙어 있으면 NoChange. 그 외는 Prefix+AssetName.
	// 같은 PackagePath 안에서 NewName이 겹치거나(대소문자 무시) bOccupyOnly 에셋 이름과 겹치면 Collision.
	// ReadOnly는 더 강한 차단 신호라 Collision으로 덮어쓰지 않는다.
	WORKBENCHEDITOR_API TArray<FRenamePlanEntry> BuildRenamePlan(
		const TArray<FRenameCandidate>& Candidates,
		const FString& Prefix);

	// Apply 가능 여부: 계획에 Collision/ReadOnly가 없고 실제 변경 항목(Ok)이 1건 이상일 때만 true.
	WORKBENCHEDITOR_API bool IsRenamePlanApplyable(const TArray<FRenamePlanEntry>& Plan);

	// 사람이 읽는 복구용 목록(탭 구분: 경로, 현재 이름, 새 이름). Ok 항목만 담는다.
	WORKBENCHEDITOR_API FString FormatRenameRecoveryList(const TArray<FRenamePlanEntry>& Plan);

	// ---- T11: 배치 미리보기 ----

	struct WORKBENCHEDITOR_API FPlacementParams
	{
		FTransform Seed = FTransform::Identity;
		int32 Count = 1;
		FVector Spacing = FVector(100.f, 0.f, 0.f);
		FRotator RotationStep = FRotator::ZeroRotator;
		float UniformScaleStep = 0.f;
		int32 RandomSeed = 0;
		float PositionJitter = 0.f; // 각 축 +-Jitter 범위의 균등 난수
	};

	// 한 번에 배치할 수 있는 최대 개수(시드 포함). 에디터 정지를 막는 상한이다.
	constexpr int32 MaxPlacementCount = 200;

	// 비어 있으면 통과. 유한하지 않은 값(NaN/Inf), Count 범위(1..MaxPlacementCount), 시드/스텝으로 생기는 0·음수 스케일을 사유로 담는다.
	WORKBENCHEDITOR_API TArray<FString> ValidatePlacementParams(const FPlacementParams& Params);

	// ValidatePlacementParams가 사유를 내는 입력(Count<=0, 상한 초과, 비유한 값)에는 빈 배열을 반환한다.
	// 0번은 항상 Seed 그대로(지터 없음). 같은 RandomSeed면 결과가 같다.
	WORKBENCHEDITOR_API TArray<FTransform> BuildPlacementPreview(const FPlacementParams& Params);

	WORKBENCHEDITOR_API bool HasNegativeScale(const TArray<FTransform>& Preview);

	// 어느 축이든 비유한이거나 절댓값이 거의 0(퇴화)이거나 음수인 스케일이 있으면 true.
	WORKBENCHEDITOR_API bool HasInvalidScale(const TArray<FTransform>& Preview);

	// 적용 직전 재확인용: 위치·회전·스케일이 허용 오차 안에서 같고 모두 유한할 때만 true.
	WORKBENCHEDITOR_API bool IsTransformUnchanged(const FTransform& Expected, const FTransform& Current);

	struct WORKBENCHEDITOR_API FPlacementActorState
	{
		FString Label;
		bool bLocked = false;
		bool bAttachedToParent = false;
		bool bHasAttachedChildren = false;
		bool bExternalPackage = false;
		bool bInDifferentLevelThanSeed = false;
	};

	enum class EPlacementIssueKind : uint8
	{
		NoSelection,
		PlayInEditorActive,
		Locked,
		Attached,
		NegativeScale,  // 음수·0·비유한 스케일 모두 이 종류로 보고한다.
		ExternalActor,  // 경고: 외부 액터 패키지가 dirty 된다. 자동 저장은 하지 않는다.
		DifferentLevel, // 경고: 다른 서브레벨 소유
	};

	struct WORKBENCHEDITOR_API FPlacementIssue
	{
		int32 ActorIndex = INDEX_NONE; // 액터와 무관하면 INDEX_NONE
		EPlacementIssueKind Kind = EPlacementIssueKind::NoSelection;
		bool bBlocking = false;
	};

	// Locked/Attached/NegativeScale/PIE/선택 없음은 차단, ExternalActor/DifferentLevel은 경고.
	// 0번 액터는 시드라 움직이지 않으므로 Locked/Attached 검사는 1번부터 적용한다.
	WORKBENCHEDITOR_API TArray<FPlacementIssue> CheckPlacement(
		const TArray<FPlacementActorState>& Actors,
		const TArray<FTransform>& Preview,
		bool bPlayInEditorActive);

	WORKBENCHEDITOR_API bool HasBlockingIssue(const TArray<FPlacementIssue>& Issues);

	// ---- T06: 멀티 실행 계획 ----

	enum class ENetEmulationTarget : uint8
	{
		Server,
		Clients,
		Everyone,
	};

	struct WORKBENCHEDITOR_API FLaunchRoleSpec
	{
		FString RoleLabel;   // "Server" / "Host" / "Client1" ...
		FString CommandLine; // 실행 파일 경로와 프로젝트 경로를 제외한 인자
		FString LogPath;     // -abslog로 전달한 역할별 로그 절대 경로
		bool bHidden = false; // 서버 보조 프로세스는 true
	};

	struct WORKBENCHEDITOR_API FMultiRunRequest
	{
		FString MapPackagePath; // 필수. 예: "/Game/Maps/TestMap"
		int32 PlayerCount = 2;  // 1..4
		bool bDedicated = false;
		int32 Port = 7777;      // 1024..65535
		ENetEmulationTarget EmulationTarget = ENetEmulationTarget::Everyone;
		float MinLatencyMs = 0.f;
		float MaxLatencyMs = 0.f;
		float PacketLossPercent = 0.f;
	};

	// 실행 전 입력 검증. 사유가 있으면 실행하지 않는다(빈 배열이면 통과).
	WORKBENCHEDITOR_API TArray<FString> ValidateMultiRunRequest(const FMultiRunRequest& Request);

	// Listen: 총인원 N -> 호스트 1(리슨 서버 겸 클라) + 클라 N-1.
	// Dedicated: 총인원 N -> 서버 1(숨김) + 클라 N. 클라는 127.0.0.1:Port로 접속한다.
	// 각 역할은 LogDir 아래 "<Role>.log"를 -abslog로 받는다. PlayerCount는 1..4로 클램프한다.
	WORKBENCHEDITOR_API TArray<FLaunchRoleSpec> BuildLaunchPlan(
		const FMultiRunRequest& Request,
		const FGuid& RunId,
		const FString& LogDir);

	// 역할 하나의 실행 상태. 종료된 역할은 반드시 Exited*/Killed로 남고 NotLaunched로 되돌아가지 않는다.
	enum class ERoleRunState : uint8
	{
		NotLaunched,   // 아직 실행하지 않음(시작 대기)
		Running,
		ExitedOk,      // 종료 코드 0
		ExitedFailed,  // 종료 코드 != 0 또는 코드를 읽지 못함
		KilledByPanel, // 이 패널이 소유한 핸들로 종료함
		LaunchFailed,  // CreateProc 실패
	};

	WORKBENCHEDITOR_API FString RoleRunStateLabel(ERoleRunState State);

	struct WORKBENCHEDITOR_API FRoleRunRecord
	{
		FString RoleLabel;
		ERoleRunState State = ERoleRunState::NotLaunched;
		uint32 ProcessId = 0;      // 0이면 PID를 얻지 못했다.
		FString StartedAtUtc;      // ISO8601, 실행하지 않았으면 빈 문자열
		int32 ExitCode = 0;
		bool bExitCodeKnown = false;
		FString LogPath;
	};

	// 시작 단계(모든 역할 실행 완료 전)에서는 어느 역할이든 종료/실패하면 true.
	// 시작 이후에는 LaunchFailed 또는 ExitedFailed가 하나라도 있으면 true. 종료 코드 0(사용자가 창을 닫음)은 다른 역할을 유지한다.
	WORKBENCHEDITOR_API bool ShouldAbortRun(const TArray<FRoleRunRecord>& Records, bool bStartupPhase);

	struct WORKBENCHEDITOR_API FRunSummary
	{
		FString RunId;
		FString MapPackagePath;
		bool bDedicated = false;
		int32 Port = 0;
		int32 PlayerCount = 0;
		FString StartedAtUtc;
		FString EndedAtUtc;
		FString FinalStatus;   // 예: "정상 종료", "조기 실패로 회수", "시간 초과", "사용자 중지"
		FString FailureReason; // 비어 있을 수 있다.
		TArray<FRoleRunRecord> Roles;
	};

	// 실행별 result.json 본문. "connectionVerified"는 항상 false, "m2Verdict"는 항상 "미판정"이다
	// (프로세스 실행/종료만 기록하며 실제 접속·준비 완료나 M2 통과를 의미하지 않는다).
	WORKBENCHEDITOR_API FString BuildRunResultJson(const FRunSummary& Summary);

	// ---- T07: 인스펙터 프로필(bool 스칼라 조건 + 그룹) ----

	struct WORKBENCHEDITOR_API FInspectorRule
	{
		FString Property;           // 대상 클래스의 프로퍼티 이름
		FString Group;              // 비어 있으면 기존 카테고리 유지
		FString ConditionProperty;  // 비어 있으면 항상 표시. bool 프로퍼티 이름
		bool bNegateCondition = false;
	};

	struct WORKBENCHEDITOR_API FInspectorProfile
	{
		FString TargetClass; // 필수. 전체 클래스 경로("/Script/Module.Class" 또는 "/Game/Dir/BP_X.BP_X_C")
		TArray<FInspectorRule> Rules;
	};

	// "/Pkg/Path.Name" 형태(맨 앞 '/', '.' 정확히 1개, 양쪽 비어 있지 않음, 안전 문자)만 true. 짧은 이름·와일드카드는 false.
	WORKBENCHEDITOR_API bool IsFullClassPath(const FString& ClassPath);

	// JSON: {"targetClass":"/Game/Dir/BP_X.BP_X_C","rules":[{"property":"A","group":"G","visibleWhen":"bB","negate":false}]}
	// 실패 사유는 OutErrors에 담는다. 오류가 하나라도 있으면 false이며 OutProfile은 비운다.
	// 검사: targetClass 필수 + 전체 경로, 식별자 형식, 중복 프로퍼티, 자기 참조/순환 조건.
	WORKBENCHEDITOR_API bool ParseInspectorProfile(const FString& Json, FInspectorProfile& OutProfile, TArray<FString>& OutErrors);

	// 프로필 대상 전체 경로가 실제 클래스 계층의 전체 경로 중 하나와 정확히 일치할 때만 true.
	// TargetClass가 비었거나 전체 경로가 아니면 항상 false(와일드카드·이름만 일치 금지).
	WORKBENCHEDITOR_API bool ProfileMatchesClassChain(const FInspectorProfile& Profile, const TArray<FString>& ClassChainPaths);
}
