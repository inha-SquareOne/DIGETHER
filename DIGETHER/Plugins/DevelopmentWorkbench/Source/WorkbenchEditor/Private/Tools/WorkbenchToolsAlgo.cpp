#include "Tools/WorkbenchToolsAlgo.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace WorkbenchToolsAlgo
{
	namespace
	{
		bool IsIdentifier(const FString& Text)
		{
			if (Text.IsEmpty())
			{
				return false;
			}
			for (int32 Index = 0; Index < Text.Len(); ++Index)
			{
				const TCHAR Char = Text[Index];
				const bool bAlpha = (Char >= 'A' && Char <= 'Z') || (Char >= 'a' && Char <= 'z') || Char == '_';
				const bool bDigit = (Char >= '0' && Char <= '9');
				if (!bAlpha && !(bDigit && Index > 0))
				{
					return false;
				}
			}
			return true;
		}

		// "/Game/Maps/Test_Map" 형태만 허용한다. 명령줄에 그대로 붙이므로 공백·따옴표·'?'·'.'은 거부한다.
		bool IsSafePackagePath(const FString& Path)
		{
			if (Path.Len() < 2 || Path[0] != '/' || Path.Contains(TEXT("//")) || Path.EndsWith(TEXT("/")))
			{
				return false;
			}
			for (const TCHAR Char : Path)
			{
				const bool bOk = (Char >= 'A' && Char <= 'Z') || (Char >= 'a' && Char <= 'z')
					|| (Char >= '0' && Char <= '9') || Char == '_' || Char == '-' || Char == '/';
				if (!bOk)
				{
					return false;
				}
			}
			return true;
		}

		FString MakeKey(const FString& PackagePath, const FString& Name)
		{
			return PackagePath + TEXT("/") + Name.ToLower();
		}
	}

	// ---- T09 ----

	bool IsValidAssetNamePrefix(const FString& Prefix)
	{
		if (Prefix.Len() < 1 || Prefix.Len() > 32)
		{
			return false;
		}
		for (const TCHAR Char : Prefix)
		{
			const bool bOk = (Char >= 'A' && Char <= 'Z') || (Char >= 'a' && Char <= 'z') || (Char >= '0' && Char <= '9') || Char == '_';
			if (!bOk)
			{
				return false;
			}
		}
		return true;
	}

	TArray<FRenamePlanEntry> BuildRenamePlan(const TArray<FRenameCandidate>& Candidates, const FString& Prefix)
	{
		TArray<FRenamePlanEntry> Plan;
		TSet<FString> OccupiedKeys;

		for (const FRenameCandidate& Candidate : Candidates)
		{
			if (Candidate.bOccupyOnly)
			{
				OccupiedKeys.Add(MakeKey(Candidate.PackagePath, Candidate.AssetName));
				continue;
			}

			FRenamePlanEntry Entry;
			Entry.PackagePath = Candidate.PackagePath;
			Entry.OldName = Candidate.AssetName;
			Entry.bNoReferencersFound = (Candidate.ReferencerCount == 0);

			const bool bAlreadyPrefixed = !Prefix.IsEmpty() && Candidate.AssetName.StartsWith(Prefix, ESearchCase::CaseSensitive);
			Entry.NewName = (bAlreadyPrefixed || Prefix.IsEmpty()) ? Candidate.AssetName : (Prefix + Candidate.AssetName);
			Entry.Status = (Entry.NewName == Entry.OldName) ? ERenamePlanStatus::NoChange : ERenamePlanStatus::Ok;
			if (Candidate.bReadOnly)
			{
				Entry.Status = ERenamePlanStatus::ReadOnly;
			}
			Plan.Add(Entry);
		}

		TMap<FString, TArray<int32>> KeyToIndices;
		for (int32 Index = 0; Index < Plan.Num(); ++Index)
		{
			KeyToIndices.FindOrAdd(MakeKey(Plan[Index].PackagePath, Plan[Index].NewName)).Add(Index);
		}

		for (const TPair<FString, TArray<int32>>& Pair : KeyToIndices)
		{
			const bool bSharedAmongEntries = Pair.Value.Num() > 1;
			const bool bOccupied = OccupiedKeys.Contains(Pair.Key);
			if (!bSharedAmongEntries && !bOccupied)
			{
				continue;
			}
			for (const int32 Index : Pair.Value)
			{
				FRenamePlanEntry& Entry = Plan[Index];
				if (Entry.Status != ERenamePlanStatus::ReadOnly)
				{
					Entry.Status = ERenamePlanStatus::Collision;
				}
			}
		}

		return Plan;
	}

	bool IsRenamePlanApplyable(const TArray<FRenamePlanEntry>& Plan)
	{
		bool bHasChange = false;
		for (const FRenamePlanEntry& Entry : Plan)
		{
			if (Entry.Status == ERenamePlanStatus::Collision || Entry.Status == ERenamePlanStatus::ReadOnly)
			{
				return false;
			}
			bHasChange |= (Entry.Status == ERenamePlanStatus::Ok);
		}
		return bHasChange;
	}

	FString FormatRenameRecoveryList(const TArray<FRenamePlanEntry>& Plan)
	{
		FString Text;
		for (const FRenamePlanEntry& Entry : Plan)
		{
			if (Entry.Status == ERenamePlanStatus::Ok)
			{
				Text += FString::Printf(TEXT("%s\t%s\t%s\n"), *Entry.PackagePath, *Entry.OldName, *Entry.NewName);
			}
		}
		return Text;
	}

	// ---- T11 ----

	namespace
	{
		bool IsFiniteVector(const FVector& Vector)
		{
			return FMath::IsFinite(Vector.X) && FMath::IsFinite(Vector.Y) && FMath::IsFinite(Vector.Z);
		}

		bool IsFiniteRotator(const FRotator& Rotator)
		{
			return FMath::IsFinite(Rotator.Pitch) && FMath::IsFinite(Rotator.Yaw) && FMath::IsFinite(Rotator.Roll);
		}

		bool IsFiniteTransform(const FTransform& Transform)
		{
			return !Transform.ContainsNaN() && IsFiniteVector(Transform.GetLocation())
				&& IsFiniteVector(Transform.GetScale3D()) && IsFiniteRotator(Transform.Rotator());
		}

		bool IsInvalidScaleVector(const FVector& Scale)
		{
			if (!IsFiniteVector(Scale))
			{
				return true;
			}
			return Scale.X <= UE_KINDA_SMALL_NUMBER || Scale.Y <= UE_KINDA_SMALL_NUMBER || Scale.Z <= UE_KINDA_SMALL_NUMBER;
		}

		bool AreFinitePlacementInputs(const FPlacementParams& Params)
		{
			return IsFiniteTransform(Params.Seed) && IsFiniteVector(Params.Spacing) && IsFiniteRotator(Params.RotationStep)
				&& FMath::IsFinite(Params.UniformScaleStep) && FMath::IsFinite(Params.PositionJitter);
		}
	}

	TArray<FString> ValidatePlacementParams(const FPlacementParams& Params)
	{
		TArray<FString> Errors;
		if (Params.Count < 1 || Params.Count > MaxPlacementCount)
		{
			Errors.Add(FString::Printf(TEXT("개수는 1~%d여야 한다."), MaxPlacementCount));
		}
		if (!AreFinitePlacementInputs(Params))
		{
			Errors.Add(TEXT("시드 변환·간격·회전 단위·스케일 단위·지터 중 유한하지 않은 값(NaN/Inf)이 있다."));
			return Errors;
		}
		if (Params.PositionJitter < 0.f)
		{
			Errors.Add(TEXT("위치 지터는 0 이상이어야 한다."));
		}
		if (Params.Count >= 1 && Params.Count <= MaxPlacementCount)
		{
			const FVector SeedScale = Params.Seed.GetScale3D();
			const FVector LastScale = SeedScale + FVector(Params.UniformScaleStep * static_cast<double>(Params.Count - 1));
			if (IsInvalidScaleVector(SeedScale) || IsInvalidScaleVector(LastScale))
			{
				Errors.Add(TEXT("시드 또는 마지막 복제본의 스케일이 0 이하(또는 거의 0)가 된다."));
			}
		}
		return Errors;
	}

	bool HasInvalidScale(const TArray<FTransform>& Preview)
	{
		for (const FTransform& Transform : Preview)
		{
			if (IsInvalidScaleVector(Transform.GetScale3D()))
			{
				return true;
			}
		}
		return false;
	}

	bool IsTransformUnchanged(const FTransform& Expected, const FTransform& Current)
	{
		if (!IsFiniteTransform(Expected) || !IsFiniteTransform(Current))
		{
			return false;
		}
		return Expected.GetLocation().Equals(Current.GetLocation(), 0.01)
			&& Expected.GetScale3D().Equals(Current.GetScale3D(), 0.0001)
			&& Expected.GetRotation().Equals(Current.GetRotation(), 0.0001);
	}

	TArray<FTransform> BuildPlacementPreview(const FPlacementParams& Params)
	{
		TArray<FTransform> Preview;
		if (Params.Count <= 0 || Params.Count > MaxPlacementCount || !AreFinitePlacementInputs(Params))
		{
			return Preview;
		}

		Preview.Reserve(Params.Count);
		FRandomStream Stream(Params.RandomSeed);

		for (int32 Index = 0; Index < Params.Count; ++Index)
		{
			if (Index == 0)
			{
				Preview.Add(Params.Seed);
				continue;
			}

			FVector Location = Params.Seed.GetLocation() + Params.Spacing * static_cast<double>(Index);
			if (Params.PositionJitter > 0.f)
			{
				Location.X += Stream.FRandRange(-Params.PositionJitter, Params.PositionJitter);
				Location.Y += Stream.FRandRange(-Params.PositionJitter, Params.PositionJitter);
				Location.Z += Stream.FRandRange(-Params.PositionJitter, Params.PositionJitter);
			}

			const FRotator Rotation = Params.Seed.Rotator() + (Params.RotationStep * static_cast<double>(Index));
			const FVector Scale = Params.Seed.GetScale3D() + FVector(Params.UniformScaleStep * static_cast<double>(Index));
			Preview.Add(FTransform(Rotation, Location, Scale));
		}

		return Preview;
	}

	bool HasNegativeScale(const TArray<FTransform>& Preview)
	{
		for (const FTransform& Transform : Preview)
		{
			const FVector Scale = Transform.GetScale3D();
			if (Scale.X < 0.0 || Scale.Y < 0.0 || Scale.Z < 0.0)
			{
				return true;
			}
		}
		return false;
	}

	TArray<FPlacementIssue> CheckPlacement(const TArray<FPlacementActorState>& Actors, const TArray<FTransform>& Preview, bool bPlayInEditorActive)
	{
		TArray<FPlacementIssue> Issues;
		auto AddIssue = [&Issues](int32 ActorIndex, EPlacementIssueKind Kind, bool bBlocking)
		{
			FPlacementIssue Issue;
			Issue.ActorIndex = ActorIndex;
			Issue.Kind = Kind;
			Issue.bBlocking = bBlocking;
			Issues.Add(Issue);
		};

		if (bPlayInEditorActive)
		{
			AddIssue(INDEX_NONE, EPlacementIssueKind::PlayInEditorActive, true);
		}
		if (Actors.Num() < 2)
		{
			AddIssue(INDEX_NONE, EPlacementIssueKind::NoSelection, true);
		}

		for (int32 Index = 1; Index < Actors.Num(); ++Index)
		{
			const FPlacementActorState& Actor = Actors[Index];
			if (Actor.bLocked)
			{
				AddIssue(Index, EPlacementIssueKind::Locked, true);
			}
			if (Actor.bAttachedToParent || Actor.bHasAttachedChildren)
			{
				AddIssue(Index, EPlacementIssueKind::Attached, true);
			}
			if (Preview.IsValidIndex(Index))
			{
				if (IsInvalidScaleVector(Preview[Index].GetScale3D()))
				{
					AddIssue(Index, EPlacementIssueKind::NegativeScale, true);
				}
			}
			if (Actor.bExternalPackage)
			{
				AddIssue(Index, EPlacementIssueKind::ExternalActor, false);
			}
			if (Actor.bInDifferentLevelThanSeed)
			{
				AddIssue(Index, EPlacementIssueKind::DifferentLevel, false);
			}
		}
		return Issues;
	}

	bool HasBlockingIssue(const TArray<FPlacementIssue>& Issues)
	{
		for (const FPlacementIssue& Issue : Issues)
		{
			if (Issue.bBlocking)
			{
				return true;
			}
		}
		return false;
	}

	// ---- T06 ----

	TArray<FString> ValidateMultiRunRequest(const FMultiRunRequest& Request)
	{
		TArray<FString> Errors;
		if (!IsSafePackagePath(Request.MapPackagePath))
		{
			Errors.Add(TEXT("맵 패키지 경로가 비었거나 안전하지 않다. 영문·숫자·밑줄·하이픈·'/'만 쓰는 '/Game/...' 형식이어야 한다."));
		}
		if (Request.PlayerCount < 1 || Request.PlayerCount > 4)
		{
			Errors.Add(TEXT("플레이어 수는 1~4여야 한다."));
		}
		if (Request.Port < 1024 || Request.Port > 65535)
		{
			Errors.Add(TEXT("포트는 1024~65535여야 한다."));
		}
		if (!FMath::IsFinite(Request.MinLatencyMs) || !FMath::IsFinite(Request.MaxLatencyMs)
			|| Request.MinLatencyMs < 0.f || Request.MaxLatencyMs < 0.f || Request.MaxLatencyMs > 5000.f || Request.MinLatencyMs > Request.MaxLatencyMs)
		{
			Errors.Add(TEXT("지연은 유한한 0~5000ms 범위에서 최소 <= 최대여야 한다."));
		}
		if (!FMath::IsFinite(Request.PacketLossPercent) || Request.PacketLossPercent < 0.f || Request.PacketLossPercent > 100.f)
		{
			Errors.Add(TEXT("패킷 손실은 유한한 0~100%여야 한다."));
		}
		return Errors;
	}

	FString RoleRunStateLabel(ERoleRunState State)
	{
		switch (State)
		{
		case ERoleRunState::NotLaunched: return TEXT("시작 대기");
		case ERoleRunState::Running: return TEXT("실행 중(프로세스 생존만 확인)");
		case ERoleRunState::ExitedOk: return TEXT("종료(코드 0)");
		case ERoleRunState::ExitedFailed: return TEXT("비정상 종료");
		case ERoleRunState::KilledByPanel: return TEXT("패널이 종료함");
		case ERoleRunState::LaunchFailed: return TEXT("실행 실패");
		}
		return TEXT("알 수 없음");
	}

	bool ShouldAbortRun(const TArray<FRoleRunRecord>& Records, bool bStartupPhase)
	{
		for (const FRoleRunRecord& Record : Records)
		{
			switch (Record.State)
			{
			case ERoleRunState::LaunchFailed:
			case ERoleRunState::ExitedFailed:
				return true;
			case ERoleRunState::ExitedOk:
				if (bStartupPhase)
				{
					return true;
				}
				break;
			default:
				break;
			}
		}
		return false;
	}

	FString BuildRunResultJson(const FRunSummary& Summary)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("runId"), Summary.RunId);
		Root->SetStringField(TEXT("map"), Summary.MapPackagePath);
		Root->SetStringField(TEXT("mode"), Summary.bDedicated ? TEXT("dedicated") : TEXT("listen"));
		Root->SetNumberField(TEXT("port"), Summary.Port);
		Root->SetNumberField(TEXT("playerCount"), Summary.PlayerCount);
		Root->SetStringField(TEXT("startedAtUtc"), Summary.StartedAtUtc);
		Root->SetStringField(TEXT("endedAtUtc"), Summary.EndedAtUtc);
		Root->SetStringField(TEXT("finalStatus"), Summary.FinalStatus);
		Root->SetStringField(TEXT("failureReason"), Summary.FailureReason);
		Root->SetBoolField(TEXT("connectionVerified"), false);
		Root->SetStringField(TEXT("m2Verdict"), TEXT("미판정"));

		TArray<TSharedPtr<FJsonValue>> RoleValues;
		for (const FRoleRunRecord& Role : Summary.Roles)
		{
			const TSharedRef<FJsonObject> RoleObject = MakeShared<FJsonObject>();
			RoleObject->SetStringField(TEXT("role"), Role.RoleLabel);
			RoleObject->SetStringField(TEXT("state"), RoleRunStateLabel(Role.State));
			RoleObject->SetNumberField(TEXT("pid"), static_cast<double>(Role.ProcessId));
			RoleObject->SetStringField(TEXT("startedAtUtc"), Role.StartedAtUtc);
			if (Role.bExitCodeKnown)
			{
				RoleObject->SetNumberField(TEXT("exitCode"), Role.ExitCode);
			}
			RoleObject->SetStringField(TEXT("log"), Role.LogPath);
			RoleValues.Add(MakeShared<FJsonValueObject>(RoleObject));
		}
		Root->SetArrayField(TEXT("roles"), RoleValues);

		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		FJsonSerializer::Serialize(Root, Writer);
		return Text;
	}

	TArray<FLaunchRoleSpec> BuildLaunchPlan(const FMultiRunRequest& Request, const FGuid& RunId, const FString& LogDir)
	{
		TArray<FLaunchRoleSpec> Plan;
		const int32 Players = FMath::Clamp(Request.PlayerCount, 1, 4);
		const bool bHasEmulation = Request.MaxLatencyMs > 0.f || Request.PacketLossPercent > 0.f;
		const FString RunIdText = RunId.ToString(EGuidFormats::Digits);

		const FString NetArgs = bHasEmulation
			? FString::Printf(TEXT(" -PktLagMin=%d -PktLagMax=%d -PktLoss=%d"),
				FMath::RoundToInt(Request.MinLatencyMs), FMath::RoundToInt(Request.MaxLatencyMs), FMath::RoundToInt(Request.PacketLossPercent))
			: FString();

		auto AddRole = [&](const FString& Role, const FString& Target, const FString& Mode, bool bServerSide, bool bHidden)
		{
			const bool bEmulate = bHasEmulation && (Request.EmulationTarget == ENetEmulationTarget::Everyone
				|| (Request.EmulationTarget == ENetEmulationTarget::Server && bServerSide)
				|| (Request.EmulationTarget == ENetEmulationTarget::Clients && !bServerSide));

			FLaunchRoleSpec Spec;
			Spec.RoleLabel = Role;
			Spec.LogPath = LogDir / (Role + TEXT(".log"));
			Spec.bHidden = bHidden;
			Spec.CommandLine = FString::Printf(TEXT("%s%s -log -abslog=\"%s\"%s%s -WorkbenchRunId=%s -WorkbenchRole=%s -WorkbenchExpectedPlayers=%d"),
				*Target, *Mode, *Spec.LogPath,
				bHidden ? TEXT("") : TEXT(" -windowed -ResX=960 -ResY=540"),
				bEmulate ? *NetArgs : TEXT(""),
				*RunIdText, *Role, Players);
			Plan.Add(Spec);
		};

		const FString ClientTarget = FString::Printf(TEXT("127.0.0.1:%d"), Request.Port);
		const FString PortArg = FString::Printf(TEXT(" -port=%d"), Request.Port);

		if (Request.bDedicated)
		{
			AddRole(TEXT("Server"), Request.MapPackagePath, TEXT(" -game -server") + PortArg, true, true);
			for (int32 Index = 1; Index <= Players; ++Index)
			{
				AddRole(FString::Printf(TEXT("Client%d"), Index), ClientTarget, TEXT(" -game"), false, false);
			}
		}
		else
		{
			AddRole(TEXT("Host"), Request.MapPackagePath + TEXT("?listen"), TEXT(" -game") + PortArg, true, false);
			for (int32 Index = 1; Index <= Players - 1; ++Index)
			{
				AddRole(FString::Printf(TEXT("Client%d"), Index), ClientTarget, TEXT(" -game"), false, false);
			}
		}
		return Plan;
	}

	// ---- T07 ----

	bool ParseInspectorProfile(const FString& Json, FInspectorProfile& OutProfile, TArray<FString>& OutErrors)
	{
		OutProfile = FInspectorProfile();
		OutErrors.Reset();

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutErrors.Add(TEXT("프로필 JSON을 읽지 못했다. 구문을 확인하라."));
			return false;
		}

		FInspectorProfile Parsed;
		Root->TryGetStringField(TEXT("targetClass"), Parsed.TargetClass);
		if (!IsFullClassPath(Parsed.TargetClass))
		{
			OutErrors.Add(TEXT("'targetClass'는 필수이며 전체 클래스 경로(예: /Game/Dir/BP_X.BP_X_C)여야 한다. 짧은 이름·빈 값은 허용하지 않는다."));
		}

		const TArray<TSharedPtr<FJsonValue>>* RulesJson = nullptr;
		if (!Root->TryGetArrayField(TEXT("rules"), RulesJson) || RulesJson->Num() == 0)
		{
			OutErrors.Add(TEXT("'rules' 배열이 없거나 비어 있다."));
			return false;
		}

		TSet<FString> SeenProperties;
		for (int32 Index = 0; Index < RulesJson->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* RuleObject = nullptr;
			if (!(*RulesJson)[Index]->TryGetObject(RuleObject) || !RuleObject)
			{
				OutErrors.Add(FString::Printf(TEXT("규칙 %d: 객체가 아니다."), Index));
				continue;
			}

			FInspectorRule Rule;
			(*RuleObject)->TryGetStringField(TEXT("property"), Rule.Property);
			(*RuleObject)->TryGetStringField(TEXT("group"), Rule.Group);
			(*RuleObject)->TryGetStringField(TEXT("visibleWhen"), Rule.ConditionProperty);
			(*RuleObject)->TryGetBoolField(TEXT("negate"), Rule.bNegateCondition);

			if (!IsIdentifier(Rule.Property))
			{
				OutErrors.Add(FString::Printf(TEXT("규칙 %d: 'property'가 없거나 식별자 형식이 아니다."), Index));
				continue;
			}
			if (!Rule.ConditionProperty.IsEmpty() && !IsIdentifier(Rule.ConditionProperty))
			{
				OutErrors.Add(FString::Printf(TEXT("규칙 %d(%s): 'visibleWhen'이 식별자 형식이 아니다."), Index, *Rule.Property));
				continue;
			}
			if (Rule.Group.Len() > 64)
			{
				OutErrors.Add(FString::Printf(TEXT("규칙 %d(%s): 그룹 이름은 64자 이하여야 한다."), Index, *Rule.Property));
				continue;
			}
			if (Rule.ConditionProperty == Rule.Property)
			{
				OutErrors.Add(FString::Printf(TEXT("규칙 %d(%s): 자기 자신을 조건으로 쓸 수 없다."), Index, *Rule.Property));
				continue;
			}
			if (SeenProperties.Contains(Rule.Property))
			{
				OutErrors.Add(FString::Printf(TEXT("규칙 %d: 프로퍼티 '%s'가 중복이다."), Index, *Rule.Property));
				continue;
			}
			SeenProperties.Add(Rule.Property);
			Parsed.Rules.Add(Rule);
		}

		// 조건 체인을 따라가며 순환을 찾는다. 숨기지 않고 오류로 보고한다.
		TMap<FString, FString> ConditionOf;
		for (const FInspectorRule& Rule : Parsed.Rules)
		{
			if (!Rule.ConditionProperty.IsEmpty())
			{
				ConditionOf.Add(Rule.Property, Rule.ConditionProperty);
			}
		}
		for (const FInspectorRule& Rule : Parsed.Rules)
		{
			TSet<FString> Visited;
			FString Current = Rule.Property;
			while (const FString* Next = ConditionOf.Find(Current))
			{
				if (Visited.Contains(Current))
				{
					OutErrors.Add(FString::Printf(TEXT("'%s'의 조건 체인이 순환한다."), *Rule.Property));
					break;
				}
				Visited.Add(Current);
				Current = *Next;
			}
		}

		if (OutErrors.Num() > 0)
		{
			return false;
		}
		OutProfile = MoveTemp(Parsed);
		return true;
	}

	bool IsFullClassPath(const FString& ClassPath)
	{
		int32 DotIndex = INDEX_NONE;
		int32 LastDotIndex = INDEX_NONE;
		if (ClassPath.Len() < 4 || ClassPath[0] != '/' || !ClassPath.FindChar('.', DotIndex) || !ClassPath.FindLastChar('.', LastDotIndex) || DotIndex != LastDotIndex)
		{
			return false;
		}
		if (DotIndex <= 1 || DotIndex >= ClassPath.Len() - 1 || ClassPath[DotIndex - 1] == '/')
		{
			return false;
		}
		for (const TCHAR Char : ClassPath)
		{
			const bool bOk = (Char >= 'A' && Char <= 'Z') || (Char >= 'a' && Char <= 'z') || (Char >= '0' && Char <= '9')
				|| Char == '_' || Char == '-' || Char == '/' || Char == '.';
			if (!bOk)
			{
				return false;
			}
		}
		return !ClassPath.Contains(TEXT("//"));
	}

	bool ProfileMatchesClassChain(const FInspectorProfile& Profile, const TArray<FString>& ClassChainPaths)
	{
		return IsFullClassPath(Profile.TargetClass) && ClassChainPaths.Contains(Profile.TargetClass);
	}
}
