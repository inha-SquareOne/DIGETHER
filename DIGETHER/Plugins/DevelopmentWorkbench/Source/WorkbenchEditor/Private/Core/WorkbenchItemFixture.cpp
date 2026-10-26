#include "Core/WorkbenchItemFixture.h"
#include "Core/WorkbenchDataTableEditAdapter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "WorkbenchItemFixture"

void UWorkbenchItemDraftObject::CopyFromRow(FName InRowName, const FWorkbenchSyntheticItemRow& InRow)
{
	SourceRowName = InRowName;
	DefinitionId = InRow.DefinitionId;
	DisplayName = InRow.DisplayName;
	Category = InRow.Category;
	Price = InRow.Price;
	Weight = InRow.Weight;
	RarityId = InRow.RarityId;
}

FWorkbenchSyntheticItemRow UWorkbenchItemDraftObject::ToRow() const
{
	FWorkbenchSyntheticItemRow Row;
	Row.DefinitionId = DefinitionId;
	Row.DisplayName = DisplayName;
	Row.Category = Category;
	Row.Price = Price;
	Row.Weight = Weight;
	Row.RarityId = RarityId;
	return Row;
}

namespace
{
	const FName ColDefinitionId(TEXT("DefinitionId"));
	const FName ColDisplayName(TEXT("DisplayName"));
	const FName ColCategory(TEXT("Category"));
	const FName ColPrice(TEXT("Price"));
	const FName ColWeight(TEXT("Weight"));
	const FName ColRarityId(TEXT("RarityId"));
}

const TArray<FWorkbenchColumnSpec>& WorkbenchItemFixture::GetColumns()
{
	static const TArray<FWorkbenchColumnSpec> Columns = {
		{ ColDefinitionId, EWorkbenchScalarType::Name },
		{ ColDisplayName, EWorkbenchScalarType::String },
		{ ColCategory, EWorkbenchScalarType::Name },
		{ ColPrice, EWorkbenchScalarType::Int32 },
		{ ColWeight, EWorkbenchScalarType::Float },
		{ ColRarityId, EWorkbenchScalarType::Name },
	};
	return Columns;
}

FWorkbenchRowValues WorkbenchItemFixture::RowToValues(const FWorkbenchSyntheticItemRow& Row)
{
	FWorkbenchRowValues V;
	V.Add(ColDefinitionId, FWorkbenchScalarValue::MakeName(Row.DefinitionId));
	V.Add(ColDisplayName, FWorkbenchScalarValue::MakeString(Row.DisplayName));
	V.Add(ColCategory, FWorkbenchScalarValue::MakeName(Row.Category));
	V.Add(ColPrice, FWorkbenchScalarValue::MakeInt32(Row.Price));
	V.Add(ColWeight, FWorkbenchScalarValue::MakeFloat(Row.Weight));
	V.Add(ColRarityId, FWorkbenchScalarValue::MakeName(Row.RarityId));
	return V;
}

bool WorkbenchItemFixture::ApplyValuesToRow(FWorkbenchSyntheticItemRow& Row, const FWorkbenchRowValues& Values, FText& OutError)
{
	for (const TPair<FName, FWorkbenchScalarValue>& Pair : Values)
	{
		const FWorkbenchColumnSpec* Spec = GetColumns().FindByPredicate([&](const FWorkbenchColumnSpec& C) { return C.Name == Pair.Key; });
		if (!Spec || Spec->Type != Pair.Value.Type)
		{
			OutError = FText::Format(LOCTEXT("NotAllowed", "허용 목록에 없거나 타입이 다른 열입니다: {0}"), FText::FromName(Pair.Key));
			return false;
		}
		const FWorkbenchScalarValue& V = Pair.Value;
		if (Pair.Key == ColDefinitionId) { Row.DefinitionId = V.NameValue; }
		else if (Pair.Key == ColDisplayName) { Row.DisplayName = V.StringValue; }
		else if (Pair.Key == ColCategory) { Row.Category = V.NameValue; }
		else if (Pair.Key == ColPrice) { Row.Price = V.IntValue; }
		else if (Pair.Key == ColWeight) { Row.Weight = V.FloatValue; }
		else if (Pair.Key == ColRarityId) { Row.RarityId = V.NameValue; }
	}
	return true;
}

uint32 WorkbenchItemFixture::Fnv1a32(const FString& Text)
{
	FTCHARToUTF8 Utf8(*Text);
	uint32 H = 2166136261u;
	const uint8* Bytes = reinterpret_cast<const uint8*>(Utf8.Get());
	for (int32 i = 0; i < Utf8.Length(); ++i)
	{
		H ^= Bytes[i];
		H *= 16777619u;
	}
	return H;
}

namespace
{
	void AppendField(FString& Out, const FString& Value)
	{
		Out += FString::Printf(TEXT("%d:"), Value.Len());
		Out += Value;
		Out += TEXT(";");
	}
}

FString WorkbenchItemFixture::SerializeRowCanonical(const FWorkbenchSyntheticItemRow& Row)
{
	FString Out = TEXT("R1;");
	AppendField(Out, Row.DefinitionId.ToString());
	AppendField(Out, Row.DisplayName);
	AppendField(Out, Row.Category.ToString());
	AppendField(Out, FString::FromInt(Row.Price));
	// float를 왕복 가능한 문자열로 정규화하고 -0을 0과 같게 본다.
	AppendField(Out, FString::Printf(TEXT("%.9g"), static_cast<double>(Row.Weight == 0.f ? 0.f : Row.Weight)));
	AppendField(Out, Row.RarityId.ToString());
	return Out;
}

FString WorkbenchItemFixture::SerializeTableCanonical(const TMap<FName, FWorkbenchSyntheticItemRow>& Rows)
{
	TArray<FString> Names;
	for (const TPair<FName, FWorkbenchSyntheticItemRow>& Pair : Rows)
	{
		Names.Add(Pair.Key.ToString());
	}
	Names.Sort();
	FString Out = FString::Printf(TEXT("T1;%d;"), Names.Num());
	for (const FString& Name : Names)
	{
		AppendField(Out, Name);
		AppendField(Out, SerializeRowCanonical(Rows[FName(*Name)]));
	}
	return Out;
}

uint32 WorkbenchItemFixture::ComputeRowHash(const FWorkbenchSyntheticItemRow& Row)
{
	return Fnv1a32(SerializeRowCanonical(Row));
}

uint32 WorkbenchItemFixture::ComputeTableHash(const TMap<FName, FWorkbenchSyntheticItemRow>& Rows)
{
	return Fnv1a32(SerializeTableCanonical(Rows));
}

namespace
{
	TPair<FName, FWorkbenchSyntheticItemRow> MakeFixtureRow(const TCHAR* Id, const TCHAR* Name, const TCHAR* Cat, int32 Price, float Weight, const TCHAR* Rarity)
	{
		FWorkbenchSyntheticItemRow R;
		R.DefinitionId = FName(Id);
		R.DisplayName = Name;
		R.Category = FName(Cat);
		R.Price = Price;
		R.Weight = Weight;
		R.RarityId = FName(Rarity);
		return TPair<FName, FWorkbenchSyntheticItemRow>(FName(Id), R);
	}
}

TArray<TPair<FName, FWorkbenchSyntheticItemRow>> WorkbenchItemFixture::MakeCleanRows()
{
	TArray<TPair<FName, FWorkbenchSyntheticItemRow>> Rows;
	Rows.Add(MakeFixtureRow(TEXT("SYN_Scrap_A"), TEXT("[합성] 고철 A"), TEXT("Scrap"), 10, 1.5f, TEXT("SynCommon")));
	Rows.Add(MakeFixtureRow(TEXT("SYN_Scrap_B"), TEXT("[합성] 고철 B"), TEXT("Scrap"), 25, 4.25f, TEXT("SynCommon")));
	Rows.Add(MakeFixtureRow(TEXT("SYN_Scrap_C"), TEXT("[합성] 고철 C"), TEXT("Scrap"), 100, 12.f, TEXT("SynUncommon")));
	Rows.Add(MakeFixtureRow(TEXT("SYN_Gear_A"), TEXT("[합성] 장비 A"), TEXT("Equipment"), 250, 8.f, TEXT("SynRare")));
	Rows.Add(MakeFixtureRow(TEXT("SYN_Gear_B"), TEXT("[합성] 장비 B"), TEXT("Equipment"), 1200, 20.5f, TEXT("SynRare")));
	Rows.Add(MakeFixtureRow(TEXT("SYN_Gear_C"), TEXT("[합성] 장비 C"), TEXT("Equipment"), 9, 0.75f, TEXT("SynCommon")));
	return Rows;
}

TArray<TPair<FName, FWorkbenchSyntheticItemRow>> WorkbenchItemFixture::MakeInvalidRows()
{
	// 검증 규칙(중복 ID / 가격 0 결정 대기 / 음수 중량·빈 이름) 시연용 의도적 문제 행이다.
	TArray<TPair<FName, FWorkbenchSyntheticItemRow>> Rows;
	Rows.Add(MakeFixtureRow(TEXT("SYN_Problem_Dup1"), TEXT("[합성] 중복 ID 1"), TEXT("Scrap"), 5, 1.f, TEXT("SynCommon")));
	Rows.Last().Value.DefinitionId = FName(TEXT("SYN_Dup"));
	Rows.Add(MakeFixtureRow(TEXT("SYN_Problem_Dup2"), TEXT("[합성] 중복 ID 2"), TEXT("Scrap"), 6, 1.f, TEXT("SynCommon")));
	Rows.Last().Value.DefinitionId = FName(TEXT("SYN_Dup"));
	Rows.Add(MakeFixtureRow(TEXT("SYN_Problem_ZeroPrice"), TEXT("[합성] 가격 0"), TEXT("Scrap"), 0, 2.f, TEXT("SynCommon")));
	Rows.Add(MakeFixtureRow(TEXT("SYN_Problem_NegWeight"), TEXT(""), TEXT("Scrap"), 3, -1.f, TEXT("SynCommon")));
	return Rows;
}

TArray<TPair<FName, FWorkbenchSyntheticItemRow>> WorkbenchItemFixture::MakeSyntheticRows()
{
	TArray<TPair<FName, FWorkbenchSyntheticItemRow>> Rows = MakeCleanRows();
	Rows.Append(MakeInvalidRows());
	return Rows;
}

UDataTable* WorkbenchItemFixture::CreateTableFromRows(UObject* Outer, FName Name, const TArray<TPair<FName, FWorkbenchSyntheticItemRow>>& Rows, EObjectFlags Flags)
{
	UObject* UseOuter = Outer ? Outer : GetTransientPackage();
	UDataTable* Table = NewObject<UDataTable>(UseOuter, Name, Flags);
	Table->RowStruct = FWorkbenchSyntheticItemRow::StaticStruct();
	for (const TPair<FName, FWorkbenchSyntheticItemRow>& Pair : Rows)
	{
		Table->AddRow(Pair.Key, Pair.Value);
	}
	return Table;
}

UDataTable* WorkbenchItemFixture::CreateSyntheticTable(UObject* Outer)
{
	UObject* UseOuter = Outer ? Outer : GetTransientPackage();
	return CreateTableFromRows(UseOuter, MakeUniqueObjectName(UseOuter, UDataTable::StaticClass(), TEXT("WorkbenchSyntheticItems")), MakeSyntheticRows(), RF_Transactional);
}

FString WorkbenchItemFixture::GetSyntheticAssetPackagePath()
{
	return TEXT("/Game/Editor/DevelopmentWorkbench/Synthetic/DT_WorkbenchSyntheticItems");
}

UDataTable* WorkbenchItemFixture::FindOrCreateSyntheticAsset(FText& OutError)
{
	const FString PackagePath = GetSyntheticAssetPackagePath();
	const FString AssetName = FPackageName::GetShortName(PackagePath);
	const FString ObjectPath = PackagePath + TEXT(".") + AssetName;

	// 같은 경로에 이미 무엇이 있든 덮어쓰지 않는다: 메모리·디스크의 실제 객체 종류를 확인한 뒤 합성 테이블일 때만 재사용한다.
	const bool bOnDisk = FPackageName::DoesPackageExist(PackagePath);
	UObject* Occupant = nullptr;
	if (UPackage* Loaded = FindPackage(nullptr, *PackagePath))
	{
		Occupant = StaticFindObject(UObject::StaticClass(), Loaded, *AssetName);
	}
	if (!Occupant && bOnDisk)
	{
		Occupant = LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (Occupant)
	{
		UDataTable* ExistingTable = Cast<UDataTable>(Occupant);
		if (!ExistingTable || ExistingTable->RowStruct != FWorkbenchSyntheticItemRow::StaticStruct())
		{
			OutError = FText::Format(LOCTEXT("SyntheticPathOccupied", "합성 에셋 경로에 다른 종류의 에셋이 있어 덮어쓰지 않고 중단했습니다: {0}"), FText::FromString(ObjectPath));
			return nullptr;
		}
		return ExistingTable;
	}
	if (bOnDisk)
	{
		OutError = FText::Format(LOCTEXT("SyntheticPackageOccupied", "합성 에셋 패키지 파일이 있지만 같은 이름의 에셋을 읽지 못해 덮어쓰지 않고 중단했습니다: {0}"), FText::FromString(PackagePath));
		return nullptr;
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		OutError = LOCTEXT("CreatePackageFail", "합성 에셋 패키지를 만들지 못했습니다.");
		return nullptr;
	}
	Package->FullyLoad();

	UDataTable* Table = NewObject<UDataTable>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
	Table->RowStruct = FWorkbenchSyntheticItemRow::StaticStruct();
	for (const TPair<FName, FWorkbenchSyntheticItemRow>& Pair : MakeSyntheticRows())
	{
		Table->AddRow(Pair.Key, Pair.Value);
	}
	FAssetRegistryModule::AssetCreated(Table);
	Table->MarkPackageDirty();
	WorkbenchDataTableEditAdapter::MarkPackageCreatedByWorkbench(Table);
	return Table;
}

#undef LOCTEXT_NAMESPACE
