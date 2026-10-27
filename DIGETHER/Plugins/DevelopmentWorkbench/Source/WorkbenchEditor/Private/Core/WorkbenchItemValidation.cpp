#include "Core/WorkbenchItemValidation.h"

#define LOCTEXT_NAMESPACE "WorkbenchItemValidation"

namespace
{
	FWorkbenchValidationMessage MakeMessage(FName Rule, FName Target, EWorkbenchValidationState State, EWorkbenchValidationSeverity Severity, const FText& Description)
	{
		FWorkbenchValidationMessage M;
		M.RuleId = Rule;
		M.TargetId = Target;
		M.State = State;
		M.Severity = Severity;
		M.Description = Description;
		return M;
	}
}

TMap<FName, int32> WorkbenchItemValidation::BuildDefinitionIdCounts(const TMap<FName, FWorkbenchSyntheticItemRow>& Rows)
{
	TMap<FName, int32> Counts;
	for (const TPair<FName, FWorkbenchSyntheticItemRow>& Pair : Rows)
	{
		if (!Pair.Value.DefinitionId.IsNone())
		{
			Counts.FindOrAdd(Pair.Value.DefinitionId)++;
		}
	}
	return Counts;
}

TArray<FWorkbenchValidationMessage> WorkbenchItemValidation::ValidateRow(
	FName RowName,
	const FWorkbenchSyntheticItemRow& Row,
	const TMap<FName, int32>& DefinitionIdCounts)
{
	TArray<FWorkbenchValidationMessage> Out;

	if (Row.DefinitionId.IsNone())
	{
		Out.Add(MakeMessage(TEXT("Item.DefinitionIdEmpty"), RowName, EWorkbenchValidationState::Invalid, EWorkbenchValidationSeverity::Error,
			LOCTEXT("IdEmpty", "DefinitionId가 비어 있습니다.")));
	}
	else if (const int32* Count = DefinitionIdCounts.Find(Row.DefinitionId); Count && *Count > 1)
	{
		Out.Add(MakeMessage(TEXT("Item.DefinitionIdDuplicate"), RowName, EWorkbenchValidationState::Invalid, EWorkbenchValidationSeverity::Error,
			FText::Format(LOCTEXT("IdDup", "DefinitionId '{0}'가 같은 테이블에서 {1}번 쓰입니다."), FText::FromName(Row.DefinitionId), FText::AsNumber(*Count))));
	}

	if (Row.DisplayName.TrimStartAndEnd().IsEmpty())
	{
		Out.Add(MakeMessage(TEXT("Item.DisplayNameEmpty"), RowName, EWorkbenchValidationState::Invalid, EWorkbenchValidationSeverity::Error,
			LOCTEXT("NameEmpty", "표시 이름이 비어 있습니다(합성 구조의 필수 필드).")));
	}

	if (!FMath::IsFinite(Row.Weight) || Row.Weight < 0.f)
	{
		Out.Add(MakeMessage(TEXT("Item.WeightInvalid"), RowName, EWorkbenchValidationState::Invalid, EWorkbenchValidationSeverity::Error,
			LOCTEXT("WeightBad", "Weight는 유한한 0 이상의 값이어야 합니다.")));
	}

	if (Row.Price < 0)
	{
		Out.Add(MakeMessage(TEXT("Item.PriceNegative"), RowName, EWorkbenchValidationState::Invalid, EWorkbenchValidationSeverity::Error,
			LOCTEXT("PriceNeg", "Price가 음수입니다.")));
	}
	else if (Row.Price == 0)
	{
		Out.Add(MakeMessage(TEXT("Item.PriceZero"), RowName, EWorkbenchValidationState::PendingDecision, EWorkbenchValidationSeverity::Warning,
			LOCTEXT("PriceZero", "Price 0 허용 여부는 판매 정책이 정해지지 않아 판정하지 않습니다(결정 대기).")));
	}

	return Out;
}

TArray<FWorkbenchValidationMessage> WorkbenchItemValidation::ValidateTable(const TMap<FName, FWorkbenchSyntheticItemRow>& Rows)
{
	TArray<FWorkbenchValidationMessage> Out;
	const TMap<FName, int32> Counts = BuildDefinitionIdCounts(Rows);

	TArray<FName> Names;
	Rows.GetKeys(Names);
	Names.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	for (const FName& Name : Names)
	{
		Out.Append(ValidateRow(Name, Rows[Name], Counts));
	}

	Out.Add(MakeMessage(TEXT("Item.GlobalDefinitionIdIndex"), NAME_None, EWorkbenchValidationState::NotChecked, EWorkbenchValidationSeverity::Info,
		LOCTEXT("GlobalNotChecked", "프로젝트 전체 DefinitionId 충돌은 검사하지 않았습니다. 같은 테이블 안 중복만 검사했습니다.")));
	return Out;
}

bool WorkbenchItemValidation::HasBlocking(const TArray<FWorkbenchValidationMessage>& Messages)
{
	return Messages.ContainsByPredicate([](const FWorkbenchValidationMessage& M) { return M.IsBlocking(); });
}

FText WorkbenchItemValidation::StateToText(EWorkbenchValidationState State)
{
	switch (State)
	{
	case EWorkbenchValidationState::Valid:           return LOCTEXT("SValid", "통과");
	case EWorkbenchValidationState::Invalid:         return LOCTEXT("SInvalid", "위반");
	case EWorkbenchValidationState::NotValidated:    return LOCTEXT("SNotValidated", "미검증");
	case EWorkbenchValidationState::PendingDecision: return LOCTEXT("SPending", "결정 대기");
	case EWorkbenchValidationState::Unsupported:     return LOCTEXT("SUnsupported", "지원 안 함");
	case EWorkbenchValidationState::NotChecked:      return LOCTEXT("SNotChecked", "미검사");
	case EWorkbenchValidationState::Stale:           return LOCTEXT("SStale", "오래됨(재조회 필요)");
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
