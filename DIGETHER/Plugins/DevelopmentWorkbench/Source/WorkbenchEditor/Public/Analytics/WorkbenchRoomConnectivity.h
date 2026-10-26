// T04 룸 연결 manifest 검사. 입력된 룸·문·연결 선언만으로 스키마·기하(허용 오차)·그래프 도달성/귀환을 검사한다.
// PCG·월드 충돌·NavMesh에 연결되어 있지 않으므로 그래프 통과는 실제 이동 가능을 뜻하지 않는다.
#pragma once

#include "CoreMinimal.h"

namespace WorkbenchAnalytics
{
	/** 룸 로컬 좌표계의 문 바깥 방향. 룸의 Yaw(90도 배수)만큼 회전해 월드 방향이 된다. */
	enum class ERoomDoorDirection : uint8
	{
		PosX,
		NegX,
		PosY,
		NegY,
	};

	struct WORKBENCHEDITOR_API FRoomDef
	{
		FString RoomId;
		double SizeX = 0.0; // 스케일 적용 전 내부 크기(cm)
		double SizeY = 0.0;
		double SizeZ = 0.0;
		double Scale = 1.0;
		double OriginX = 0.0; // 룸 중심의 월드 위치(cm)
		double OriginY = 0.0;
		double YawDegrees = 0.0; // 90도 배수만 지원
	};

	struct WORKBENCHEDITOR_API FRoomDoorDef
	{
		FString DoorId;
		FString RoomId;
		ERoomDoorDirection Direction = ERoomDoorDirection::PosX;
		double Offset = 0.0; // 벽 중앙에서 벽을 따라 이동한 거리(cm, 스케일 적용 전)
		double Width = 0.0;
		double Height = 0.0;
		FString ConnectsTo; // 이 문이 선언한 상대 문 ID. 비어 있으면 연결 선언 없음
	};

	struct WORKBENCHEDITOR_API FRoomManifest
	{
		TArray<FRoomDef> Rooms;
		TArray<FRoomDoorDef> Doors;
		TArray<FString> EntryRoomIds;
		TArray<FString> ExitRoomIds;
	};

	/** 허용 오차와 프로젝트 규격 입력. 0 이하인 기대 값은 해당 규격 검사를 하지 않는다. 게임 확정값이 아니다. */
	struct WORKBENCHEDITOR_API FRoomManifestSpec
	{
		double PositionToleranceCm = 1.0;
		double SizeToleranceCm = 1.0;
		double AngleToleranceDegrees = 1.0;
		double ScaleTolerance = 1e-3;
		double ExpectedScale = 0.0;
		double ExpectedDoorWidth = 0.0; // 스케일 적용 후 값
		double ExpectedDoorHeight = 0.0;
	};

	enum class ERoomIssueSeverity : uint8
	{
		Error,
		Warning,
	};

	enum class ERoomIssueType : uint8
	{
		EmptyId,
		DuplicateRoomId,
		DuplicateDoorId,
		InvalidRoomSize,
		InvalidScale,
		ScaleSpecMismatch,
		LinkedScaleMismatch,
		InvalidYaw,
		DoorUnknownRoom,
		InvalidDoorSize,
		DoorOutsideWall,
		DoorTooTall,
		DoorOverlap,
		DoorSizeSpecMismatch,
		RoomOverlap,
		LinkTargetMissing,
		LinkToSelf,
		LinkSameRoom,
		OneSidedLink,
		FlippedDirection,
		DirectionMismatch,
		PositionMismatch,
		DoorSizeMismatch,
		NoEntry,
		EntryUnknownRoom,
		ExitUnknownRoom,
		UnreachableRoom,
		NoReturnRoom,
		UnreachableExit,
	};

	struct WORKBENCHEDITOR_API FRoomManifestIssue
	{
		ERoomIssueType Type = ERoomIssueType::EmptyId;
		ERoomIssueSeverity Severity = ERoomIssueSeverity::Error;
		FString ObjectId; // 관련 룸/문 ID
		FString Message; // 한국어 설명
	};

	struct WORKBENCHEDITOR_API FRoomManifestReport
	{
		TArray<FRoomManifestIssue> Issues;
		int32 RoomCount = 0;
		int32 DoorCount = 0;
		int32 MutualLinkCount = 0;
		TArray<FString> ReachableRooms; // 입구에서 선언된 연결을 따라 도달 가능한 룸
		TArray<FString> RoomsWithoutReturn; // 도달은 되지만 입구로 되돌아올 수 없는 룸

		int32 CountErrors() const;
		int32 CountWarnings() const;
		bool HasIssue(ERoomIssueType Type) const;
		/** 오류가 0건이어도 "스키마·그래프 검사 통과"일 뿐 실제 이동 가능을 뜻하지 않는다. */
		bool PassesGraphChecks() const { return CountErrors() == 0; }
	};

	/** 텍스트 manifest 파싱. 행 형식은 아래와 같고 '#' 주석·빈 줄은 무시한다.
	 *  ROOM,RoomId,SizeX,SizeY,SizeZ,Scale,OriginX,OriginY,YawDegrees
	 *  DOOR,DoorId,RoomId,Direction(+X|-X|+Y|-Y),Offset,Width,Height[,ConnectsTo]
	 *  ENTRY,RoomId   /   EXIT,RoomId
	 */
	WORKBENCHEDITOR_API bool ParseRoomManifestText(const FString& Text, FRoomManifest& OutManifest, TArray<FString>& OutErrors);

	WORKBENCHEDITOR_API FRoomManifestReport AnalyzeRoomManifest(const FRoomManifest& Manifest, const FRoomManifestSpec& Spec);

	enum class ERoomManifestSample : uint8
	{
		Normal,
		OneSidedLink,
		FlippedDirection,
		ScaleMismatch,
		SizeMismatch,
		Overlap,
		IsolatedRoom,
	};

	/** 합성 예제 텍스트(실제 게임 룸 데이터 아님). Normal은 오류 0건이다. */
	WORKBENCHEDITOR_API FString GetRoomManifestSampleText(ERoomManifestSample Sample);

	WORKBENCHEDITOR_API FString GetRoomIssueTypeLabel(ERoomIssueType Type);
}
