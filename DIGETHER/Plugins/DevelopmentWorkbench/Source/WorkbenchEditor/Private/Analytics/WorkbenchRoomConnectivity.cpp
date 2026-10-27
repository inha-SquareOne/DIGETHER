#include "Analytics/WorkbenchRoomConnectivity.h"

#include "Analytics/WorkbenchSpawnProbability.h"

namespace WorkbenchAnalytics
{
	namespace
	{
		struct FVec2
		{
			double X = 0.0;
			double Y = 0.0;
		};

		FVec2 LocalDirection(ERoomDoorDirection Direction)
		{
			switch (Direction)
			{
			case ERoomDoorDirection::PosX: return { 1.0, 0.0 };
			case ERoomDoorDirection::NegX: return { -1.0, 0.0 };
			case ERoomDoorDirection::PosY: return { 0.0, 1.0 };
			default: return { 0.0, -1.0 };
			}
		}

		FVec2 RotateQuarterTurns(FVec2 V, int32 Turns)
		{
			for (int32 Index = 0; Index < Turns; ++Index)
			{
				V = { -V.Y, V.X };
			}
			return V;
		}

		/** Yaw를 90도 배수로 맞춘 회전 횟수(0~3). 허용 각도를 넘으면 INDEX_NONE. */
		int32 QuarterTurnsFromYaw(double YawDegrees, double ToleranceDegrees)
		{
			if (!FMath::IsFinite(YawDegrees))
			{
				return INDEX_NONE;
			}
			const double Snapped = FMath::RoundToDouble(YawDegrees / 90.0);
			if (FMath::Abs(YawDegrees - Snapped * 90.0) > FMath::Max(ToleranceDegrees, 0.0))
			{
				return INDEX_NONE;
			}
			const int64 Turns = static_cast<int64>(Snapped);
			return static_cast<int32>(((Turns % 4) + 4) % 4);
		}

		bool IsPositiveFinite(double Value)
		{
			return FMath::IsFinite(Value) && Value > 0.0;
		}

		struct FRoomGeometry
		{
			bool bValid = false;
			int32 Turns = 0;
			double HalfX = 0.0; // 월드 축 정렬 반치수(스케일 적용 후)
			double HalfY = 0.0;
		};

		struct FDoorGeometry
		{
			FVec2 Position;
			FVec2 Direction;
			double WorldWidth = 0.0;
			double WorldHeight = 0.0;
		};

		FString DirectionText(ERoomDoorDirection Direction)
		{
			switch (Direction)
			{
			case ERoomDoorDirection::PosX: return TEXT("+X");
			case ERoomDoorDirection::NegX: return TEXT("-X");
			case ERoomDoorDirection::PosY: return TEXT("+Y");
			default: return TEXT("-Y");
			}
		}

		bool TryParseDirection(const FString& Text, ERoomDoorDirection& Out)
		{
			if (Text == TEXT("+X")) { Out = ERoomDoorDirection::PosX; return true; }
			if (Text == TEXT("-X")) { Out = ERoomDoorDirection::NegX; return true; }
			if (Text == TEXT("+Y")) { Out = ERoomDoorDirection::PosY; return true; }
			if (Text == TEXT("-Y")) { Out = ERoomDoorDirection::NegY; return true; }
			return false;
		}

		TSet<FString> CollectReachable(const TArray<FString>& Starts, const TMap<FString, TArray<FString>>& Adjacency)
		{
			TSet<FString> Visited;
			TArray<FString> Stack;
			for (const FString& Start : Starts)
			{
				if (!Visited.Contains(Start))
				{
					Visited.Add(Start);
					Stack.Add(Start);
				}
			}
			while (Stack.Num() > 0)
			{
				const FString Current = Stack.Pop();
				if (const TArray<FString>* Next = Adjacency.Find(Current))
				{
					for (const FString& Neighbor : *Next)
					{
						if (!Visited.Contains(Neighbor))
						{
							Visited.Add(Neighbor);
							Stack.Add(Neighbor);
						}
					}
				}
			}
			return Visited;
		}
	}

	int32 FRoomManifestReport::CountErrors() const
	{
		int32 Count = 0;
		for (const FRoomManifestIssue& Issue : Issues)
		{
			Count += Issue.Severity == ERoomIssueSeverity::Error ? 1 : 0;
		}
		return Count;
	}

	int32 FRoomManifestReport::CountWarnings() const
	{
		return Issues.Num() - CountErrors();
	}

	bool FRoomManifestReport::HasIssue(ERoomIssueType Type) const
	{
		return Issues.ContainsByPredicate([Type](const FRoomManifestIssue& Issue) { return Issue.Type == Type; });
	}

	bool ParseRoomManifestText(const FString& Text, FRoomManifest& OutManifest, TArray<FString>& OutErrors)
	{
		OutManifest = FRoomManifest();
		OutErrors.Reset();

		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);

		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			FString Line = Lines[LineIndex];
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
			{
				continue;
			}

			TArray<FString> Columns;
			Line.ParseIntoArray(Columns, TEXT(","), /*bCullEmpty=*/false);
			for (FString& Column : Columns)
			{
				Column.TrimStartAndEndInline();
			}
			const int32 LineNo = LineIndex + 1;
			const FString Kind = Columns[0].ToUpper();

			auto ParseNumber = [&](int32 ColumnIndex, const TCHAR* Name, double& Out) -> bool
			{
				if (!TryParseDoubleStrict(Columns[ColumnIndex], Out) || !FMath::IsFinite(Out))
				{
					OutErrors.Add(FString::Printf(TEXT("%d행: %s '%s'를 유한한 숫자로 해석할 수 없습니다."), LineNo, Name, *Columns[ColumnIndex]));
					return false;
				}
				return true;
			};

			if (Kind == TEXT("ROOM"))
			{
				if (Columns.Num() != 9)
				{
					OutErrors.Add(FString::Printf(TEXT("%d행: ROOM은 \"ROOM,RoomId,SizeX,SizeY,SizeZ,Scale,OriginX,OriginY,YawDegrees\" 9열이어야 합니다."), LineNo));
					continue;
				}
				FRoomDef Room;
				Room.RoomId = Columns[1];
				const bool bOk = ParseNumber(2, TEXT("SizeX"), Room.SizeX) & ParseNumber(3, TEXT("SizeY"), Room.SizeY)
					& ParseNumber(4, TEXT("SizeZ"), Room.SizeZ) & ParseNumber(5, TEXT("Scale"), Room.Scale)
					& ParseNumber(6, TEXT("OriginX"), Room.OriginX) & ParseNumber(7, TEXT("OriginY"), Room.OriginY)
					& ParseNumber(8, TEXT("YawDegrees"), Room.YawDegrees);
				if (bOk)
				{
					OutManifest.Rooms.Add(Room);
				}
			}
			else if (Kind == TEXT("DOOR"))
			{
				if (Columns.Num() != 7 && Columns.Num() != 8)
				{
					OutErrors.Add(FString::Printf(TEXT("%d행: DOOR는 \"DOOR,DoorId,RoomId,Direction,Offset,Width,Height[,ConnectsTo]\" 7~8열이어야 합니다."), LineNo));
					continue;
				}
				FRoomDoorDef Door;
				Door.DoorId = Columns[1];
				Door.RoomId = Columns[2];
				if (!TryParseDirection(Columns[3], Door.Direction))
				{
					OutErrors.Add(FString::Printf(TEXT("%d행: 방향 '%s'는 +X, -X, +Y, -Y 중 하나여야 합니다."), LineNo, *Columns[3]));
					continue;
				}
				const bool bOk = ParseNumber(4, TEXT("Offset"), Door.Offset) & ParseNumber(5, TEXT("Width"), Door.Width)
					& ParseNumber(6, TEXT("Height"), Door.Height);
				Door.ConnectsTo = Columns.Num() == 8 ? Columns[7] : FString();
				if (bOk)
				{
					OutManifest.Doors.Add(Door);
				}
			}
			else if (Kind == TEXT("ENTRY") || Kind == TEXT("EXIT"))
			{
				if (Columns.Num() != 2)
				{
					OutErrors.Add(FString::Printf(TEXT("%d행: %s는 \"%s,RoomId\" 2열이어야 합니다."), LineNo, *Kind, *Kind));
					continue;
				}
				(Kind == TEXT("ENTRY") ? OutManifest.EntryRoomIds : OutManifest.ExitRoomIds).Add(Columns[1]);
			}
			else
			{
				OutErrors.Add(FString::Printf(TEXT("%d행: 알 수 없는 행 종류 '%s'입니다(ROOM, DOOR, ENTRY, EXIT)."), LineNo, *Columns[0]));
			}
		}

		return OutErrors.Num() == 0;
	}

	FRoomManifestReport AnalyzeRoomManifest(const FRoomManifest& Manifest, const FRoomManifestSpec& Spec)
	{
		FRoomManifestReport Report;
		Report.RoomCount = Manifest.Rooms.Num();
		Report.DoorCount = Manifest.Doors.Num();

		auto AddError = [&Report](ERoomIssueType Type, const FString& ObjectId, const FString& Message)
		{
			FRoomManifestIssue Issue;
			Issue.Type = Type;
			Issue.Severity = ERoomIssueSeverity::Error;
			Issue.ObjectId = ObjectId;
			Issue.Message = Message;
			Report.Issues.Add(Issue);
		};

		const double PositionTol = FMath::Max(Spec.PositionToleranceCm, 0.0);
		const double SizeTol = FMath::Max(Spec.SizeToleranceCm, 0.0);

		// 룸: ID·크기·스케일·Yaw 검증과 월드 치수 계산(첫 번째 정의가 우선)
		TMap<FString, int32> RoomIndexById;
		TMap<FString, FRoomGeometry> RoomGeometry;
		for (int32 Index = 0; Index < Manifest.Rooms.Num(); ++Index)
		{
			const FRoomDef& Room = Manifest.Rooms[Index];
			if (Room.RoomId.IsEmpty())
			{
				AddError(ERoomIssueType::EmptyId, FString(), FString::Printf(TEXT("%d번째 룸의 ID가 비어 있습니다."), Index + 1));
				continue;
			}
			if (RoomIndexById.Contains(Room.RoomId))
			{
				AddError(ERoomIssueType::DuplicateRoomId, Room.RoomId, FString::Printf(TEXT("룸 ID '%s'가 중복됩니다. 첫 번째 정의만 사용합니다."), *Room.RoomId));
				continue;
			}
			RoomIndexById.Add(Room.RoomId, Index);

			FRoomGeometry Geometry;
			bool bRoomOk = true;
			if (!IsPositiveFinite(Room.SizeX) || !IsPositiveFinite(Room.SizeY) || !IsPositiveFinite(Room.SizeZ))
			{
				AddError(ERoomIssueType::InvalidRoomSize, Room.RoomId,
					FString::Printf(TEXT("룸 '%s': 크기(%g x %g x %g)는 모두 0보다 큰 유한수여야 합니다."), *Room.RoomId, Room.SizeX, Room.SizeY, Room.SizeZ));
				bRoomOk = false;
			}
			if (!IsPositiveFinite(Room.Scale))
			{
				AddError(ERoomIssueType::InvalidScale, Room.RoomId, FString::Printf(TEXT("룸 '%s': 스케일 %g은(는) 0보다 큰 유한수여야 합니다."), *Room.RoomId, Room.Scale));
				bRoomOk = false;
			}
			else if (Spec.ExpectedScale > 0.0 && FMath::Abs(Room.Scale - Spec.ExpectedScale) > Spec.ScaleTolerance)
			{
				AddError(ERoomIssueType::ScaleSpecMismatch, Room.RoomId,
					FString::Printf(TEXT("룸 '%s': 스케일 %g이(가) 기대 스케일 %g과(와) 허용 오차 %g를 넘게 다릅니다."), *Room.RoomId, Room.Scale, Spec.ExpectedScale, Spec.ScaleTolerance));
			}
			if (!FMath::IsFinite(Room.OriginX) || !FMath::IsFinite(Room.OriginY))
			{
				AddError(ERoomIssueType::InvalidRoomSize, Room.RoomId, FString::Printf(TEXT("룸 '%s': 위치가 유한수가 아닙니다."), *Room.RoomId));
				bRoomOk = false;
			}
			const int32 Turns = QuarterTurnsFromYaw(Room.YawDegrees, Spec.AngleToleranceDegrees);
			if (Turns == INDEX_NONE)
			{
				AddError(ERoomIssueType::InvalidYaw, Room.RoomId,
					FString::Printf(TEXT("룸 '%s': Yaw %g도는 90도 배수(허용 오차 %g도)가 아닙니다."), *Room.RoomId, Room.YawDegrees, Spec.AngleToleranceDegrees));
				bRoomOk = false;
			}
			if (bRoomOk)
			{
				Geometry.bValid = true;
				Geometry.Turns = Turns;
				const double HalfX = Room.SizeX * 0.5 * Room.Scale;
				const double HalfY = Room.SizeY * 0.5 * Room.Scale;
				Geometry.HalfX = (Turns % 2 == 0) ? HalfX : HalfY;
				Geometry.HalfY = (Turns % 2 == 0) ? HalfY : HalfX;
			}
			RoomGeometry.Add(Room.RoomId, Geometry);
		}

		// 룸 AABB 겹침(Z는 보지 않는다)
		for (int32 A = 0; A < Manifest.Rooms.Num(); ++A)
		{
			const FRoomDef& RoomA = Manifest.Rooms[A];
			const FRoomGeometry* GeomA = RoomGeometry.Find(RoomA.RoomId);
			if (!GeomA || !GeomA->bValid || RoomIndexById.FindRef(RoomA.RoomId) != A)
			{
				continue;
			}
			for (int32 B = A + 1; B < Manifest.Rooms.Num(); ++B)
			{
				const FRoomDef& RoomB = Manifest.Rooms[B];
				const FRoomGeometry* GeomB = RoomGeometry.Find(RoomB.RoomId);
				if (!GeomB || !GeomB->bValid || RoomIndexById.FindRef(RoomB.RoomId) != B)
				{
					continue;
				}
				const double OverlapX = (GeomA->HalfX + GeomB->HalfX) - FMath::Abs(RoomA.OriginX - RoomB.OriginX);
				const double OverlapY = (GeomA->HalfY + GeomB->HalfY) - FMath::Abs(RoomA.OriginY - RoomB.OriginY);
				if (OverlapX > PositionTol && OverlapY > PositionTol)
				{
					AddError(ERoomIssueType::RoomOverlap, RoomA.RoomId,
						FString::Printf(TEXT("룸 '%s'와 '%s'의 평면 영역이 %g x %g cm 겹칩니다(허용 %g cm)."), *RoomA.RoomId, *RoomB.RoomId, OverlapX, OverlapY, PositionTol));
				}
			}
		}

		// 문: ID·룸 참조·크기·벽 적합성·월드 위치
		TMap<FString, int32> DoorIndexById;
		TMap<FString, FDoorGeometry> DoorGeometry;
		for (int32 Index = 0; Index < Manifest.Doors.Num(); ++Index)
		{
			const FRoomDoorDef& Door = Manifest.Doors[Index];
			if (Door.DoorId.IsEmpty())
			{
				AddError(ERoomIssueType::EmptyId, FString(), FString::Printf(TEXT("%d번째 문의 ID가 비어 있습니다."), Index + 1));
				continue;
			}
			if (DoorIndexById.Contains(Door.DoorId))
			{
				AddError(ERoomIssueType::DuplicateDoorId, Door.DoorId, FString::Printf(TEXT("문 ID '%s'가 중복됩니다. 첫 번째 정의만 사용합니다."), *Door.DoorId));
				continue;
			}
			DoorIndexById.Add(Door.DoorId, Index);

			const int32* RoomIndex = RoomIndexById.Find(Door.RoomId);
			if (!RoomIndex)
			{
				AddError(ERoomIssueType::DoorUnknownRoom, Door.DoorId, FString::Printf(TEXT("문 '%s'가 존재하지 않는 룸 '%s'를 가리킵니다."), *Door.DoorId, *Door.RoomId));
				continue;
			}
			const FRoomDef& Room = Manifest.Rooms[*RoomIndex];
			const FRoomGeometry& RoomGeom = RoomGeometry.FindRef(Room.RoomId);

			if (!IsPositiveFinite(Door.Width) || !IsPositiveFinite(Door.Height) || !FMath::IsFinite(Door.Offset))
			{
				AddError(ERoomIssueType::InvalidDoorSize, Door.DoorId,
					FString::Printf(TEXT("문 '%s': 너비 %g, 높이 %g는 0보다 큰 유한수, 오프셋 %g는 유한수여야 합니다."), *Door.DoorId, Door.Width, Door.Height, Door.Offset));
				continue;
			}
			if (!RoomGeom.bValid)
			{
				continue;
			}

			const FVec2 LocalDir = LocalDirection(Door.Direction);
			const bool bAlongX = LocalDir.X != 0.0;
			const double WallLength = bAlongX ? Room.SizeY : Room.SizeX;
			const double HalfAlong = (bAlongX ? Room.SizeX : Room.SizeY) * 0.5;
			if ((FMath::Abs(Door.Offset) + Door.Width * 0.5 - WallLength * 0.5) * Room.Scale > PositionTol)
			{
				AddError(ERoomIssueType::DoorOutsideWall, Door.DoorId,
					FString::Printf(TEXT("문 '%s': 오프셋 %g, 너비 %g가 벽 길이 %g를 벗어납니다."), *Door.DoorId, Door.Offset, Door.Width, WallLength));
			}
			if ((Door.Height - Room.SizeZ) * Room.Scale > SizeTol)
			{
				AddError(ERoomIssueType::DoorTooTall, Door.DoorId,
					FString::Printf(TEXT("문 '%s': 높이 %g가 룸 '%s'의 높이 %g보다 큽니다."), *Door.DoorId, Door.Height, *Room.RoomId, Room.SizeZ));
			}

			FDoorGeometry Geometry;
			Geometry.WorldWidth = Door.Width * Room.Scale;
			Geometry.WorldHeight = Door.Height * Room.Scale;
			const FVec2 Tangent = { -LocalDir.Y, LocalDir.X };
			const FVec2 LocalPos = { (LocalDir.X * HalfAlong + Tangent.X * Door.Offset) * Room.Scale,
				(LocalDir.Y * HalfAlong + Tangent.Y * Door.Offset) * Room.Scale };
			const FVec2 Rotated = RotateQuarterTurns(LocalPos, RoomGeom.Turns);
			Geometry.Position = { Room.OriginX + Rotated.X, Room.OriginY + Rotated.Y };
			Geometry.Direction = RotateQuarterTurns(LocalDir, RoomGeom.Turns);
			DoorGeometry.Add(Door.DoorId, Geometry);

			if (Spec.ExpectedDoorWidth > 0.0 && FMath::Abs(Geometry.WorldWidth - Spec.ExpectedDoorWidth) > SizeTol)
			{
				AddError(ERoomIssueType::DoorSizeSpecMismatch, Door.DoorId,
					FString::Printf(TEXT("문 '%s': 너비 %g(스케일 적용)가 기대 너비 %g와 허용 오차 %g를 넘게 다릅니다."), *Door.DoorId, Geometry.WorldWidth, Spec.ExpectedDoorWidth, SizeTol));
			}
			if (Spec.ExpectedDoorHeight > 0.0 && FMath::Abs(Geometry.WorldHeight - Spec.ExpectedDoorHeight) > SizeTol)
			{
				AddError(ERoomIssueType::DoorSizeSpecMismatch, Door.DoorId,
					FString::Printf(TEXT("문 '%s': 높이 %g(스케일 적용)가 기대 높이 %g와 허용 오차 %g를 넘게 다릅니다."), *Door.DoorId, Geometry.WorldHeight, Spec.ExpectedDoorHeight, SizeTol));
			}
		}

		// 같은 룸·같은 벽 문 겹침
		for (int32 A = 0; A < Manifest.Doors.Num(); ++A)
		{
			const FRoomDoorDef& DoorA = Manifest.Doors[A];
			if (DoorIndexById.FindRef(DoorA.DoorId) != A || !DoorGeometry.Contains(DoorA.DoorId))
			{
				continue;
			}
			for (int32 B = A + 1; B < Manifest.Doors.Num(); ++B)
			{
				const FRoomDoorDef& DoorB = Manifest.Doors[B];
				if (DoorIndexById.FindRef(DoorB.DoorId) != B || !DoorGeometry.Contains(DoorB.DoorId)
					|| DoorA.RoomId != DoorB.RoomId || DoorA.Direction != DoorB.Direction)
				{
					continue;
				}
				const double Scale = Manifest.Rooms[RoomIndexById.FindRef(DoorA.RoomId)].Scale;
				const double Overlap = (DoorA.Width + DoorB.Width) * 0.5 - FMath::Abs(DoorA.Offset - DoorB.Offset);
				if (Overlap * Scale > PositionTol)
				{
					AddError(ERoomIssueType::DoorOverlap, DoorA.DoorId,
						FString::Printf(TEXT("문 '%s'와 '%s'가 룸 '%s'의 %s 벽에서 %g 겹칩니다."), *DoorA.DoorId, *DoorB.DoorId, *DoorA.RoomId, *DirectionText(DoorA.Direction), Overlap));
				}
			}
		}

		// 연결 선언: 대상·상호성·방향·위치·크기·스케일. 그래프 간선은 선언된 방향 그대로 만든다.
		TMap<FString, TArray<FString>> Forward;
		TMap<FString, TArray<FString>> Backward;
		for (int32 Index = 0; Index < Manifest.Doors.Num(); ++Index)
		{
			const FRoomDoorDef& Door = Manifest.Doors[Index];
			if (Door.DoorId.IsEmpty() || DoorIndexById.FindRef(Door.DoorId) != Index || Door.ConnectsTo.IsEmpty())
			{
				continue;
			}
			if (Door.ConnectsTo == Door.DoorId)
			{
				AddError(ERoomIssueType::LinkToSelf, Door.DoorId, FString::Printf(TEXT("문 '%s'가 자기 자신에게 연결되어 있습니다."), *Door.DoorId));
				continue;
			}
			const int32* TargetIndex = DoorIndexById.Find(Door.ConnectsTo);
			if (!TargetIndex)
			{
				AddError(ERoomIssueType::LinkTargetMissing, Door.DoorId, FString::Printf(TEXT("문 '%s'의 연결 대상 문 '%s'가 없습니다."), *Door.DoorId, *Door.ConnectsTo));
				continue;
			}
			const FRoomDoorDef& Target = Manifest.Doors[*TargetIndex];
			if (Target.RoomId == Door.RoomId)
			{
				AddError(ERoomIssueType::LinkSameRoom, Door.DoorId, FString::Printf(TEXT("문 '%s'와 '%s'가 같은 룸 '%s'의 문입니다."), *Door.DoorId, *Target.DoorId, *Door.RoomId));
				continue;
			}

			const bool bBothRoomsKnown = RoomIndexById.Contains(Door.RoomId) && RoomIndexById.Contains(Target.RoomId);
			if (bBothRoomsKnown)
			{
				Forward.FindOrAdd(Door.RoomId).AddUnique(Target.RoomId);
				Backward.FindOrAdd(Target.RoomId).AddUnique(Door.RoomId);
			}

			if (Target.ConnectsTo != Door.DoorId)
			{
				AddError(ERoomIssueType::OneSidedLink, Door.DoorId,
					FString::Printf(TEXT("문 '%s'는 '%s'에 연결을 선언했지만 '%s'는 이 문에 연결을 선언하지 않았습니다(한쪽 연결)."), *Door.DoorId, *Door.ConnectsTo, *Target.DoorId));
				continue;
			}
			if (*TargetIndex < Index)
			{
				continue; // 상호 연결 쌍은 한 번만 검사한다.
			}
			++Report.MutualLinkCount;
			if (!bBothRoomsKnown)
			{
				continue;
			}

			const FRoomDef& RoomA = Manifest.Rooms[RoomIndexById.FindRef(Door.RoomId)];
			const FRoomDef& RoomB = Manifest.Rooms[RoomIndexById.FindRef(Target.RoomId)];
			if (IsPositiveFinite(RoomA.Scale) && IsPositiveFinite(RoomB.Scale) && FMath::Abs(RoomA.Scale - RoomB.Scale) > Spec.ScaleTolerance)
			{
				AddError(ERoomIssueType::LinkedScaleMismatch, Door.DoorId,
					FString::Printf(TEXT("연결된 룸 '%s'(스케일 %g)와 '%s'(스케일 %g)의 스케일이 허용 오차 %g를 넘게 다릅니다."), *RoomA.RoomId, RoomA.Scale, *RoomB.RoomId, RoomB.Scale, Spec.ScaleTolerance));
			}

			const FDoorGeometry* GeomA = DoorGeometry.Find(Door.DoorId);
			const FDoorGeometry* GeomB = DoorGeometry.Find(Target.DoorId);
			if (!GeomA || !GeomB)
			{
				continue;
			}
			const double Dot = GeomA->Direction.X * GeomB->Direction.X + GeomA->Direction.Y * GeomB->Direction.Y;
			if (Dot > 0.5)
			{
				AddError(ERoomIssueType::FlippedDirection, Door.DoorId,
					FString::Printf(TEXT("연결된 문 '%s'와 '%s'가 같은 월드 방향을 향합니다(뒤집힘). 서로 마주 봐야 합니다."), *Door.DoorId, *Target.DoorId));
			}
			else if (Dot > -0.5)
			{
				AddError(ERoomIssueType::DirectionMismatch, Door.DoorId,
					FString::Printf(TEXT("연결된 문 '%s'와 '%s'의 방향이 서로 수직입니다. 서로 마주 봐야 합니다."), *Door.DoorId, *Target.DoorId));
			}
			else
			{
				const double Distance = FMath::Sqrt(FMath::Square(GeomA->Position.X - GeomB->Position.X) + FMath::Square(GeomA->Position.Y - GeomB->Position.Y));
				if (Distance > PositionTol)
				{
					AddError(ERoomIssueType::PositionMismatch, Door.DoorId,
						FString::Printf(TEXT("연결된 문 '%s'와 '%s'의 월드 위치가 %g cm 떨어져 있습니다(허용 %g cm)."), *Door.DoorId, *Target.DoorId, Distance, PositionTol));
				}
			}
			if (FMath::Abs(GeomA->WorldWidth - GeomB->WorldWidth) > SizeTol || FMath::Abs(GeomA->WorldHeight - GeomB->WorldHeight) > SizeTol)
			{
				AddError(ERoomIssueType::DoorSizeMismatch, Door.DoorId,
					FString::Printf(TEXT("연결된 문 '%s'(%g x %g)와 '%s'(%g x %g)의 크기가 허용 오차 %g를 넘게 다릅니다."),
						*Door.DoorId, GeomA->WorldWidth, GeomA->WorldHeight, *Target.DoorId, GeomB->WorldWidth, GeomB->WorldHeight, SizeTol));
			}
		}

		// 입구·출구 참조와 도달성·귀환
		TArray<FString> ValidEntries;
		for (const FString& EntryId : Manifest.EntryRoomIds)
		{
			if (RoomIndexById.Contains(EntryId))
			{
				ValidEntries.AddUnique(EntryId);
			}
			else
			{
				AddError(ERoomIssueType::EntryUnknownRoom, EntryId, FString::Printf(TEXT("입구 '%s'가 존재하지 않는 룸입니다."), *EntryId));
			}
		}
		for (const FString& ExitId : Manifest.ExitRoomIds)
		{
			if (!RoomIndexById.Contains(ExitId))
			{
				AddError(ERoomIssueType::ExitUnknownRoom, ExitId, FString::Printf(TEXT("출구 '%s'가 존재하지 않는 룸입니다."), *ExitId));
			}
		}
		if (Manifest.EntryRoomIds.Num() == 0)
		{
			AddError(ERoomIssueType::NoEntry, FString(), TEXT("입구(ENTRY)가 지정되지 않아 도달성·귀환을 검사할 수 없습니다."));
			return Report;
		}
		if (ValidEntries.Num() == 0)
		{
			return Report;
		}

		const TSet<FString> Reachable = CollectReachable(ValidEntries, Forward);
		const TSet<FString> CanReturn = CollectReachable(ValidEntries, Backward);
		TSet<FString> Listed;
		for (const FRoomDef& Room : Manifest.Rooms)
		{
			if (Room.RoomId.IsEmpty() || Listed.Contains(Room.RoomId))
			{
				continue;
			}
			Listed.Add(Room.RoomId);
			if (!Reachable.Contains(Room.RoomId))
			{
				AddError(ERoomIssueType::UnreachableRoom, Room.RoomId, FString::Printf(TEXT("룸 '%s'는 입구에서 선언된 연결로 도달할 수 없습니다."), *Room.RoomId));
				continue;
			}
			Report.ReachableRooms.Add(Room.RoomId);
			if (!CanReturn.Contains(Room.RoomId))
			{
				Report.RoomsWithoutReturn.Add(Room.RoomId);
				AddError(ERoomIssueType::NoReturnRoom, Room.RoomId, FString::Printf(TEXT("룸 '%s'는 도달할 수 있지만 선언된 연결로 입구에 되돌아올 수 없습니다."), *Room.RoomId));
			}
		}
		for (const FString& ExitId : Manifest.ExitRoomIds)
		{
			if (RoomIndexById.Contains(ExitId) && !Reachable.Contains(ExitId))
			{
				AddError(ERoomIssueType::UnreachableExit, ExitId, FString::Printf(TEXT("출구 '%s'에 입구에서 도달할 수 없습니다."), *ExitId));
			}
		}

		return Report;
	}

	FString GetRoomManifestSampleText(ERoomManifestSample Sample)
	{
		const FString Normal = FString(TEXT("# 합성 예제입니다(실제 게임 룸 데이터 아님). 단위 cm, 문 오프셋은 벽 중앙 기준.\n"))
			+ TEXT("ROOM,Start,800,800,300,1,0,0,0\n")
			+ TEXT("ROOM,Hall,800,800,300,1,800,0,0\n")
			+ TEXT("ROOM,Vault,800,800,300,1,1600,0,0\n")
			+ TEXT("DOOR,D_S_E,Start,+X,0,200,250,D_H_W\n")
			+ TEXT("DOOR,D_H_W,Hall,-X,0,200,250,D_S_E\n")
			+ TEXT("DOOR,D_H_E,Hall,+X,0,200,250,D_V_W\n")
			+ TEXT("DOOR,D_V_W,Vault,-X,0,200,250,D_H_E\n")
			+ TEXT("ENTRY,Start\n")
			+ TEXT("EXIT,Vault\n");

		auto Variant = [&Normal](const TCHAR* From, const TCHAR* To)
		{
			return Normal.Replace(From, To, ESearchCase::CaseSensitive);
		};

		switch (Sample)
		{
		case ERoomManifestSample::OneSidedLink:
			return Variant(TEXT("DOOR,D_V_W,Vault,-X,0,200,250,D_H_E"), TEXT("DOOR,D_V_W,Vault,-X,0,200,250"));
		case ERoomManifestSample::FlippedDirection:
			return Variant(TEXT("DOOR,D_V_W,Vault,-X,"), TEXT("DOOR,D_V_W,Vault,+X,"));
		case ERoomManifestSample::ScaleMismatch:
			return Variant(TEXT("ROOM,Vault,800,800,300,1,1600,0,0"), TEXT("ROOM,Vault,800,800,300,2,1600,0,0"));
		case ERoomManifestSample::SizeMismatch:
			return Variant(TEXT("DOOR,D_V_W,Vault,-X,0,200,250,D_H_E"), TEXT("DOOR,D_V_W,Vault,-X,0,150,250,D_H_E"));
		case ERoomManifestSample::Overlap:
			return Variant(TEXT("ROOM,Vault,800,800,300,1,1600,0,0"), TEXT("ROOM,Vault,800,800,300,1,1000,0,0"));
		case ERoomManifestSample::IsolatedRoom:
			return Normal + TEXT("ROOM,Annex,400,400,300,1,0,1000,0\n");
		default:
			return Normal;
		}
	}

	FString GetRoomIssueTypeLabel(ERoomIssueType Type)
	{
		switch (Type)
		{
		case ERoomIssueType::EmptyId: return TEXT("빈 ID");
		case ERoomIssueType::DuplicateRoomId: return TEXT("룸 ID 중복");
		case ERoomIssueType::DuplicateDoorId: return TEXT("문 ID 중복");
		case ERoomIssueType::InvalidRoomSize: return TEXT("잘못된 룸 크기");
		case ERoomIssueType::InvalidScale: return TEXT("잘못된 스케일");
		case ERoomIssueType::ScaleSpecMismatch: return TEXT("스케일 규격 불일치");
		case ERoomIssueType::LinkedScaleMismatch: return TEXT("연결 룸 스케일 불일치");
		case ERoomIssueType::InvalidYaw: return TEXT("잘못된 Yaw");
		case ERoomIssueType::DoorUnknownRoom: return TEXT("문의 룸 참조 오류");
		case ERoomIssueType::InvalidDoorSize: return TEXT("잘못된 문 크기");
		case ERoomIssueType::DoorOutsideWall: return TEXT("문이 벽을 벗어남");
		case ERoomIssueType::DoorTooTall: return TEXT("문이 룸보다 높음");
		case ERoomIssueType::DoorOverlap: return TEXT("문 겹침");
		case ERoomIssueType::DoorSizeSpecMismatch: return TEXT("문 크기 규격 불일치");
		case ERoomIssueType::RoomOverlap: return TEXT("룸 영역 겹침");
		case ERoomIssueType::LinkTargetMissing: return TEXT("연결 대상 없음");
		case ERoomIssueType::LinkToSelf: return TEXT("자기 연결");
		case ERoomIssueType::LinkSameRoom: return TEXT("같은 룸 내 연결");
		case ERoomIssueType::OneSidedLink: return TEXT("한쪽 연결");
		case ERoomIssueType::FlippedDirection: return TEXT("방향 뒤집힘");
		case ERoomIssueType::DirectionMismatch: return TEXT("방향 불일치");
		case ERoomIssueType::PositionMismatch: return TEXT("문 위치 불일치");
		case ERoomIssueType::DoorSizeMismatch: return TEXT("연결 문 크기 불일치");
		case ERoomIssueType::NoEntry: return TEXT("입구 없음");
		case ERoomIssueType::EntryUnknownRoom: return TEXT("입구 룸 없음");
		case ERoomIssueType::ExitUnknownRoom: return TEXT("출구 룸 없음");
		case ERoomIssueType::UnreachableRoom: return TEXT("도달 불가 룸");
		case ERoomIssueType::NoReturnRoom: return TEXT("귀환 불가 룸");
		case ERoomIssueType::UnreachableExit: return TEXT("도달 불가 출구");
		default: return TEXT("알 수 없음");
		}
	}
}
