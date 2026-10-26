// 편집·CSV 패치가 다루는 허용 스칼라 타입의 공통 표현.
// 지원 타입은 Bool / Int32 / Float / Name / String 뿐이다. 배열·Map·UObject 포함·구조체는 지원하지 않으며
// 해당 필드는 허용 열(allowlist)에 넣을 수 없다.
#pragma once

#include "CoreMinimal.h"

enum class EWorkbenchScalarType : uint8
{
	Bool,
	Int32,
	Float,
	Name,
	String,

	/** 값을 모른다는 뜻이며 빈 문자열/None과 다르다. */
	Unset
};

/** 값 하나를 타입 태그와 함께 담는 값 타입. */
struct WORKBENCHEDITOR_API FWorkbenchScalarValue
{
	FWorkbenchScalarValue() = default;

	static FWorkbenchScalarValue MakeBool(bool bInValue);
	static FWorkbenchScalarValue MakeInt32(int32 InValue);
	static FWorkbenchScalarValue MakeFloat(float InValue);
	static FWorkbenchScalarValue MakeName(FName InValue);
	static FWorkbenchScalarValue MakeString(FString InValue);

	EWorkbenchScalarType Type = EWorkbenchScalarType::Unset;
	bool BoolValue = false;
	int32 IntValue = 0;
	float FloatValue = 0.f;
	FName NameValue;
	FString StringValue;

	bool IsSet() const { return Type != EWorkbenchScalarType::Unset; }

	/** 화면 표시용 문자열. */
	FString ToDisplayString() const;

	/** CSV 내보내기용 문자열. float는 왕복 가능한 정밀도로 쓴다. */
	FString ToCsvString() const;

	bool operator==(const FWorkbenchScalarValue& Other) const;
	bool operator!=(const FWorkbenchScalarValue& Other) const { return !(*this == Other); }
};

/** 허용 열 1개의 이름과 타입. */
struct FWorkbenchColumnSpec
{
	FName Name;
	EWorkbenchScalarType Type = EWorkbenchScalarType::Unset;
};

/** 한 행의 허용 열 값 묶음. */
using FWorkbenchRowValues = TMap<FName, FWorkbenchScalarValue>;

namespace WorkbenchScalar
{
	/**
	 * 문자열을 엄격하게 변환한다. 로케일 구분자(1,5 / 1.234,5), NaN/무한대, 16진수, 범위 초과, 공백만 있는 값은 거부한다.
	 * Bool은 true/false/1/0(대소문자 무시)만 허용한다.
	 */
	WORKBENCHEDITOR_API bool TryParse(EWorkbenchScalarType Type, const FString& Text, FWorkbenchScalarValue& OutValue, FText& OutError);

	WORKBENCHEDITOR_API const TCHAR* TypeToString(EWorkbenchScalarType Type);
}
