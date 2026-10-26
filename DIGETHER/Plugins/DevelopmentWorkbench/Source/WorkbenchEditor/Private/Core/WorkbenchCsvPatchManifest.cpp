#include "Core/WorkbenchCsvPatchManifest.h"
#include "Serialization/Csv/CsvParser.h"

#define LOCTEXT_NAMESPACE "WorkbenchCsvPatchManifest"

const TCHAR* const GWorkbenchCsvExplicitNullToken = TEXT("<NULL>");
const TCHAR* const GWorkbenchCsvExplicitBlankToken = TEXT("<BLANK>");

namespace
{
	const TCHAR* const MetaMarker = TEXT("#WorkbenchCsv");

	bool StartsWithFormulaChar(const FString& S)
	{
		if (S.IsEmpty())
		{
			return false;
		}
		const TCHAR C = S[0];
		return C == TEXT('=') || C == TEXT('+') || C == TEXT('-') || C == TEXT('@') || C == TEXT('\t') || C == TEXT('\r') || C == TEXT('\'');
	}

	bool IsToken(const FString& S)
	{
		return S == GWorkbenchCsvExplicitNullToken || S == GWorkbenchCsvExplicitBlankToken;
	}

	FString QuoteIfNeeded(const FString& Cell)
	{
		if (!Cell.Contains(TEXT(",")) && !Cell.Contains(TEXT("\"")) && !Cell.Contains(TEXT("\n")) && !Cell.Contains(TEXT("\r")))
		{
			return Cell;
		}
		return TEXT("\"") + Cell.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
	}

	bool TryParseHex32(const FString& S, uint32& Out)
	{
		if (S.IsEmpty() || S.Len() > 8)
		{
			return false;
		}
		for (TCHAR C : S)
		{
			if (!FChar::IsHexDigit(C))
			{
				return false;
			}
		}
		Out = static_cast<uint32>(FCString::Strtoui64(*S, nullptr, 16));
		return true;
	}

	// RowId 셀: 수식 문자·토큰·앞뒤 공백이 있으면 ' 접두로 감싸고, 읽을 때는 접두가 있으면 그대로(공백 보존) 벗긴다.
	FString EncodeRowId(FName RowId)
	{
		const FString Text = RowId.ToString();
		const bool bEdgeSpace = !Text.IsEmpty() && (FChar::IsWhitespace(Text[0]) || FChar::IsWhitespace(Text[Text.Len() - 1]));
		if (StartsWithFormulaChar(Text) || IsToken(Text) || bEdgeSpace)
		{
			return TEXT("'") + Text;
		}
		return Text;
	}

	FString DecodeRowId(const FString& Raw)
	{
		if (Raw.Len() > 0 && Raw[0] == TEXT('\''))
		{
			return Raw.RightChop(1);
		}
		return Raw.TrimStartAndEnd();
	}

	bool IsBlankRow(const TArray<const TCHAR*>& Row)
	{
		for (const TCHAR* Cell : Row)
		{
			if (Cell && *Cell)
			{
				return false;
			}
		}
		return true;
	}
}

FString WorkbenchCsvPatchManifest::EncodeCell(const FWorkbenchScalarValue& Value)
{
	switch (Value.Type)
	{
	case EWorkbenchScalarType::Name:
		if (Value.NameValue.IsNone())
		{
			return GWorkbenchCsvExplicitNullToken;
		}
		break;
	case EWorkbenchScalarType::String:
		if (Value.StringValue.IsEmpty())
		{
			return GWorkbenchCsvExplicitBlankToken;
		}
		break;
	default:
		return Value.ToCsvString();
	}

	const FString Text = Value.ToDisplayString();
	if (StartsWithFormulaChar(Text) || IsToken(Text))
	{
		return TEXT("'") + Text;
	}
	return Text;
}

FString WorkbenchCsvPatchManifest::ExportToCsv(
	const TArray<FName>& RowOrder,
	const TMap<FName, FWorkbenchRowValues>& Rows,
	const TArray<FWorkbenchColumnSpec>& Columns,
	uint32 BaselineHash)
{
	FString Out;
	Out += FString::Printf(TEXT("%s,%d,%08X\r\n"), MetaMarker, FWorkbenchCsvPatchManifest::CurrentSchemaVersion, BaselineHash);

	Out += TEXT("RowId");
	for (const FWorkbenchColumnSpec& Col : Columns)
	{
		Out += TEXT(",") + QuoteIfNeeded(Col.Name.ToString());
	}
	Out += TEXT("\r\n");

	for (const FName& RowId : RowOrder)
	{
		const FWorkbenchRowValues* Values = Rows.Find(RowId);
		if (!Values)
		{
			continue;
		}
		Out += QuoteIfNeeded(EncodeRowId(RowId));
		for (const FWorkbenchColumnSpec& Col : Columns)
		{
			const FWorkbenchScalarValue* V = Values->Find(Col.Name);
			Out += TEXT(",");
			if (V)
			{
				Out += QuoteIfNeeded(EncodeCell(*V));
			}
		}
		Out += TEXT("\r\n");
	}
	return Out;
}

FWorkbenchCsvPatchManifest WorkbenchCsvPatchManifest::BuildFromCsv(const FString& CsvContent, const TArray<FWorkbenchColumnSpec>& AllowedColumns)
{
	FWorkbenchCsvPatchManifest M;

	FString Text = CsvContent;
	if (Text.Len() > 0 && Text[0] == 0xFEFF)
	{
		Text.RightChopInline(1);
	}

	const FCsvParser Parser(Text);
	const TArray<TArray<const TCHAR*>>& AllRows = Parser.GetRows();

	int32 RowIndex = 0;
	auto SkipBlank = [&]()
	{
		while (RowIndex < AllRows.Num() && IsBlankRow(AllRows[RowIndex]))
		{
			++RowIndex;
		}
	};

	SkipBlank();
	if (RowIndex >= AllRows.Num())
	{
		M.RejectionErrors.Add(LOCTEXT("EmptyCsv", "CSV에 내용이 없습니다."));
		return M;
	}

	if (AllRows[RowIndex].Num() > 0 && FString(AllRows[RowIndex][0]) == MetaMarker)
	{
		const TArray<const TCHAR*>& Meta = AllRows[RowIndex];
		const FString VersionText = Meta.Num() > 1 ? FString(Meta[1]).TrimStartAndEnd() : FString();
		bool bDigitsOnly = !VersionText.IsEmpty() && VersionText.Len() <= 6;
		for (TCHAR C : VersionText)
		{
			bDigitsOnly = bDigitsOnly && C >= TEXT('0') && C <= TEXT('9');
		}
		if (!bDigitsOnly)
		{
			M.RejectionErrors.Add(LOCTEXT("BadMetaVersion", "메타 줄의 스키마 버전을 읽을 수 없습니다."));
		}
		else
		{
			const int32 Version = FCString::Atoi(*VersionText);
			M.SchemaVersion = Version;
			if (Version < 1 || Version > FWorkbenchCsvPatchManifest::CurrentSchemaVersion)
			{
				M.RejectionErrors.Add(FText::Format(LOCTEXT("FutureSchema", "지원하지 않는 CSV 스키마 버전입니다: {0} (지원: 1~{1})"),
					FText::AsNumber(Version), FText::AsNumber(FWorkbenchCsvPatchManifest::CurrentSchemaVersion)));
			}
		}
		const FString HashText = Meta.Num() > 2 ? FString(Meta[2]).TrimStartAndEnd() : FString();
		if (!HashText.IsEmpty())
		{
			if (TryParseHex32(HashText, M.BaselineHash))
			{
				M.bHasBaselineHash = true;
			}
			else
			{
				M.RejectionErrors.Add(LOCTEXT("BadMetaHash", "메타 줄의 기준 해시가 16진수 8자리 이하가 아닙니다."));
			}
		}
		++RowIndex;
		SkipBlank();
	}

	if (RowIndex >= AllRows.Num())
	{
		M.RejectionErrors.Add(LOCTEXT("NoHeader", "헤더 행이 없습니다."));
		return M;
	}

	const TArray<const TCHAR*>& Header = AllRows[RowIndex];
	const int32 HeaderLine = RowIndex + 1;
	++RowIndex;

	const FString FirstHeader = Header.Num() > 0 ? FString(Header[0]).TrimStartAndEnd() : FString();
	if (!(FirstHeader.IsEmpty() || FirstHeader.Equals(TEXT("RowId"), ESearchCase::IgnoreCase) || FirstHeader == TEXT("---")))
	{
		M.RejectionErrors.Add(FText::Format(LOCTEXT("BadFirstHeader", "{0}행: 첫 열 이름은 RowId 또는 ---여야 합니다(현재 '{1}')."),
			FText::AsNumber(HeaderLine), FText::FromString(FirstHeader)));
		return M;
	}

	TArray<FName> ColumnByIndex;
	TArray<EWorkbenchScalarType> TypeByIndex;
	TArray<bool> EmptyHeaderByIndex;
	ColumnByIndex.SetNum(Header.Num());
	TypeByIndex.Init(EWorkbenchScalarType::Unset, Header.Num());
	EmptyHeaderByIndex.Init(false, Header.Num());

	TSet<FName> SeenColumns;
	for (int32 I = 1; I < Header.Num(); ++I)
	{
		const FString Name = FString(Header[I]).TrimStartAndEnd();
		if (Name.IsEmpty())
		{
			EmptyHeaderByIndex[I] = true;
			continue;
		}
		const FName ColumnName(*Name);
		const FWorkbenchColumnSpec* Spec = AllowedColumns.FindByPredicate([&](const FWorkbenchColumnSpec& C) { return C.Name == ColumnName; });
		if (!Spec)
		{
			M.UnknownColumns.AddUnique(ColumnName);
			M.RejectionErrors.Add(FText::Format(LOCTEXT("UnknownColumn", "{0}행: 허용 목록에 없는 열입니다: {1}"),
				FText::AsNumber(HeaderLine), FText::FromString(Name)));
			continue;
		}
		if (SeenColumns.Contains(ColumnName))
		{
			M.RejectionErrors.Add(FText::Format(LOCTEXT("DupColumn", "{0}행: 열이 중복되었습니다: {1}"),
				FText::AsNumber(HeaderLine), FText::FromString(Name)));
			continue;
		}
		SeenColumns.Add(ColumnName);
		ColumnByIndex[I] = ColumnName;
		TypeByIndex[I] = Spec->Type;
		M.HeaderColumns.Add(ColumnName);
	}

	TSet<FName> SeenRows;
	bool bWarnedEmptyColumn = false;
	for (; RowIndex < AllRows.Num(); ++RowIndex)
	{
		const TArray<const TCHAR*>& Cells = AllRows[RowIndex];
		if (IsBlankRow(Cells))
		{
			continue;
		}
		const int32 Line = RowIndex + 1;

		const FString RowIdText = Cells.Num() > 0 ? DecodeRowId(Cells[0] ? FString(Cells[0]) : FString()) : FString();
		if (RowIdText.IsEmpty())
		{
			M.RejectionErrors.Add(FText::Format(LOCTEXT("EmptyRowId", "{0}행: RowId가 비어 있습니다."), FText::AsNumber(Line)));
			continue;
		}
		const FName RowId(*RowIdText);
		if (SeenRows.Contains(RowId))
		{
			M.RejectedDuplicateIds.AddUnique(RowId);
			M.RejectionErrors.Add(FText::Format(LOCTEXT("DupRow", "{0}행: RowId가 중복되었습니다: {1}"), FText::AsNumber(Line), FText::FromString(RowIdText)));
			continue;
		}
		SeenRows.Add(RowId);
		M.RowOrder.Add(RowId);

		for (int32 I = 1; I < Cells.Num(); ++I)
		{
			const FString Raw = Cells[I] ? FString(Cells[I]) : FString();
			if (I >= Header.Num())
			{
				if (!Raw.IsEmpty())
				{
					M.RejectionErrors.Add(FText::Format(LOCTEXT("ExtraCell", "{0}행: 헤더보다 많은 열에 값이 있습니다."), FText::AsNumber(Line)));
				}
				continue;
			}
			if (EmptyHeaderByIndex[I])
			{
				if (!Raw.IsEmpty())
				{
					M.RejectionErrors.Add(FText::Format(LOCTEXT("ValueUnderEmptyHeader", "{0}행: 이름 없는 열에 값이 있습니다."), FText::AsNumber(Line)));
				}
				else if (!bWarnedEmptyColumn)
				{
					bWarnedEmptyColumn = true;
					M.Warnings.Add(LOCTEXT("EmptyHeaderWarn", "이름이 없는 빈 열을 무시했습니다."));
				}
				continue;
			}
			if (ColumnByIndex[I].IsNone() || Raw.IsEmpty())
			{
				continue;
			}

			const EWorkbenchScalarType Type = TypeByIndex[I];
			FWorkbenchCsvCellPatch Patch;
			Patch.RowId = RowId;
			Patch.Column = ColumnByIndex[I];
			Patch.SourceLine = Line;

			const bool bTextual = Type == EWorkbenchScalarType::Name || Type == EWorkbenchScalarType::String;
			if (Raw == GWorkbenchCsvExplicitNullToken)
			{
				if (Type != EWorkbenchScalarType::Name)
				{
					M.RejectionErrors.Add(FText::Format(LOCTEXT("NullTokenMisuse", "{0}행 {1}: <NULL>은 이름(Name) 열에서만 쓸 수 있습니다."),
						FText::AsNumber(Line), FText::FromName(Patch.Column)));
					continue;
				}
				Patch.Value = FWorkbenchScalarValue::MakeName(NAME_None);
			}
			else if (Raw == GWorkbenchCsvExplicitBlankToken)
			{
				if (Type != EWorkbenchScalarType::String)
				{
					M.RejectionErrors.Add(FText::Format(LOCTEXT("BlankTokenMisuse", "{0}행 {1}: <BLANK>는 문자열(String) 열에서만 쓸 수 있습니다."),
						FText::AsNumber(Line), FText::FromName(Patch.Column)));
					continue;
				}
				Patch.Value = FWorkbenchScalarValue::MakeString(FString());
			}
			else
			{
				FString CellText = Raw;
				if (bTextual && CellText.Len() > 0 && CellText[0] == TEXT('\''))
				{
					CellText.RightChopInline(1);
				}
				else if (!bTextual)
				{
					CellText.TrimStartAndEndInline();
				}
				FText Error;
				if (!WorkbenchScalar::TryParse(Type, CellText, Patch.Value, Error))
				{
					M.RejectionErrors.Add(FText::Format(LOCTEXT("CellParseFail", "{0}행 {1}: {2}"),
						FText::AsNumber(Line), FText::FromName(Patch.Column), Error));
					continue;
				}
			}
			M.Patches.Add(MoveTemp(Patch));
		}
	}

	if (M.RowOrder.IsEmpty() && M.RejectionErrors.IsEmpty())
	{
		M.Warnings.Add(LOCTEXT("NoDataRows", "데이터 행이 없습니다."));
	}
	return M;
}

#undef LOCTEXT_NAMESPACE
