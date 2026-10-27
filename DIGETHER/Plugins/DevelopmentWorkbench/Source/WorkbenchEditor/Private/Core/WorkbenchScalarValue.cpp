#include "Core/WorkbenchScalarValue.h"

#define LOCTEXT_NAMESPACE "WorkbenchScalar"

FWorkbenchScalarValue FWorkbenchScalarValue::MakeBool(bool bInValue)
{
	FWorkbenchScalarValue V;
	V.Type = EWorkbenchScalarType::Bool;
	V.BoolValue = bInValue;
	return V;
}

FWorkbenchScalarValue FWorkbenchScalarValue::MakeInt32(int32 InValue)
{
	FWorkbenchScalarValue V;
	V.Type = EWorkbenchScalarType::Int32;
	V.IntValue = InValue;
	return V;
}

FWorkbenchScalarValue FWorkbenchScalarValue::MakeFloat(float InValue)
{
	FWorkbenchScalarValue V;
	V.Type = EWorkbenchScalarType::Float;
	V.FloatValue = InValue;
	return V;
}

FWorkbenchScalarValue FWorkbenchScalarValue::MakeName(FName InValue)
{
	FWorkbenchScalarValue V;
	V.Type = EWorkbenchScalarType::Name;
	V.NameValue = InValue;
	return V;
}

FWorkbenchScalarValue FWorkbenchScalarValue::MakeString(FString InValue)
{
	FWorkbenchScalarValue V;
	V.Type = EWorkbenchScalarType::String;
	V.StringValue = MoveTemp(InValue);
	return V;
}

FString FWorkbenchScalarValue::ToDisplayString() const
{
	switch (Type)
	{
	case EWorkbenchScalarType::Bool:   return BoolValue ? TEXT("true") : TEXT("false");
	case EWorkbenchScalarType::Int32:  return FString::FromInt(IntValue);
	case EWorkbenchScalarType::Float:  return FString::SanitizeFloat(FloatValue);
	case EWorkbenchScalarType::Name:   return NameValue.ToString();
	case EWorkbenchScalarType::String: return StringValue;
	default: return FString();
	}
}

FString FWorkbenchScalarValue::ToCsvString() const
{
	if (Type == EWorkbenchScalarType::Float)
	{
		return FString::Printf(TEXT("%.9g"), static_cast<double>(FloatValue));
	}
	return ToDisplayString();
}

bool FWorkbenchScalarValue::operator==(const FWorkbenchScalarValue& Other) const
{
	if (Type != Other.Type)
	{
		return false;
	}
	switch (Type)
	{
	case EWorkbenchScalarType::Bool:   return BoolValue == Other.BoolValue;
	case EWorkbenchScalarType::Int32:  return IntValue == Other.IntValue;
	case EWorkbenchScalarType::Float:  return FloatValue == Other.FloatValue;
	case EWorkbenchScalarType::Name:   return NameValue == Other.NameValue;
	case EWorkbenchScalarType::String: return StringValue.Equals(Other.StringValue, ESearchCase::CaseSensitive);
	default: return true;
	}
}

namespace
{
	bool IsAsciiDigit(TCHAR C) { return C >= TEXT('0') && C <= TEXT('9'); }

	// [+-]?digits 만 허용한다.
	bool LooksLikeInteger(const FString& S)
	{
		int32 I = 0;
		if (S.Len() > 0 && (S[0] == TEXT('+') || S[0] == TEXT('-')))
		{
			I = 1;
		}
		if (I >= S.Len())
		{
			return false;
		}
		for (; I < S.Len(); ++I)
		{
			if (!IsAsciiDigit(S[I]))
			{
				return false;
			}
		}
		return true;
	}

	// [+-]?digits[.digits][e[+-]digits] 형태만 허용한다. 로케일 구분자·16진수·nan/inf는 모양에서 걸러진다.
	bool LooksLikeDecimal(const FString& S)
	{
		int32 I = 0;
		const int32 N = S.Len();
		if (I < N && (S[I] == TEXT('+') || S[I] == TEXT('-')))
		{
			++I;
		}
		int32 IntDigits = 0;
		while (I < N && IsAsciiDigit(S[I])) { ++I; ++IntDigits; }
		int32 FracDigits = 0;
		if (I < N && S[I] == TEXT('.'))
		{
			++I;
			while (I < N && IsAsciiDigit(S[I])) { ++I; ++FracDigits; }
		}
		if (IntDigits + FracDigits == 0)
		{
			return false;
		}
		if (I < N && (S[I] == TEXT('e') || S[I] == TEXT('E')))
		{
			++I;
			if (I < N && (S[I] == TEXT('+') || S[I] == TEXT('-')))
			{
				++I;
			}
			int32 ExpDigits = 0;
			while (I < N && IsAsciiDigit(S[I])) { ++I; ++ExpDigits; }
			if (ExpDigits == 0)
			{
				return false;
			}
		}
		return I == N;
	}
}

bool WorkbenchScalar::TryParse(EWorkbenchScalarType Type, const FString& Text, FWorkbenchScalarValue& OutValue, FText& OutError)
{
	OutValue = FWorkbenchScalarValue();
	switch (Type)
	{
	case EWorkbenchScalarType::Bool:
	{
		if (Text.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Text == TEXT("1"))
		{
			OutValue = FWorkbenchScalarValue::MakeBool(true);
			return true;
		}
		if (Text.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Text == TEXT("0"))
		{
			OutValue = FWorkbenchScalarValue::MakeBool(false);
			return true;
		}
		OutError = LOCTEXT("BadBool", "불리언은 true/false/1/0만 허용합니다.");
		return false;
	}
	case EWorkbenchScalarType::Int32:
	{
		if (!LooksLikeInteger(Text) || Text.Len() > 12)
		{
			OutError = LOCTEXT("BadInt", "정수 형식이 아닙니다(천 단위 구분자·소수점·공백 불가).");
			return false;
		}
		const int64 Parsed = FCString::Atoi64(*Text);
		if (Parsed < MIN_int32 || Parsed > MAX_int32)
		{
			OutError = LOCTEXT("IntRange", "32비트 정수 범위를 벗어났습니다.");
			return false;
		}
		OutValue = FWorkbenchScalarValue::MakeInt32(static_cast<int32>(Parsed));
		return true;
	}
	case EWorkbenchScalarType::Float:
	{
		if (!LooksLikeDecimal(Text))
		{
			OutError = LOCTEXT("BadFloat", "실수 형식이 아닙니다(쉼표 구분자·NaN·무한대·16진수 불가).");
			return false;
		}
		const double Parsed = FCString::Atod(*Text);
		if (!FMath::IsFinite(Parsed) || FMath::Abs(Parsed) > static_cast<double>(MAX_flt))
		{
			OutError = LOCTEXT("FloatRange", "float 범위를 벗어났거나 유한하지 않은 값입니다.");
			return false;
		}
		OutValue = FWorkbenchScalarValue::MakeFloat(static_cast<float>(Parsed));
		return true;
	}
	case EWorkbenchScalarType::Name:
	{
		if (Text.Len() >= NAME_SIZE)
		{
			OutError = LOCTEXT("NameTooLong", "이름이 너무 깁니다.");
			return false;
		}
		OutValue = FWorkbenchScalarValue::MakeName(FName(*Text));
		return true;
	}
	case EWorkbenchScalarType::String:
		OutValue = FWorkbenchScalarValue::MakeString(Text);
		return true;
	default:
		OutError = LOCTEXT("UnsupportedType", "지원하지 않는 타입입니다.");
		return false;
	}
}

const TCHAR* WorkbenchScalar::TypeToString(EWorkbenchScalarType Type)
{
	switch (Type)
	{
	case EWorkbenchScalarType::Bool:   return TEXT("bool");
	case EWorkbenchScalarType::Int32:  return TEXT("int32");
	case EWorkbenchScalarType::Float:  return TEXT("float");
	case EWorkbenchScalarType::Name:   return TEXT("name");
	case EWorkbenchScalarType::String: return TEXT("string");
	default: return TEXT("unset");
	}
}

#undef LOCTEXT_NAMESPACE
