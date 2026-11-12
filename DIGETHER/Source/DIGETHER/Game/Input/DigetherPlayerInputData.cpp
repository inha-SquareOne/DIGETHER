#include "Game/Input/DigetherPlayerInputData.h"

#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Misc/DataValidation.h"

bool UDigetherPlayerInputData::CollectMissingActions(TArray<FName>& OutMissing) const
{
	const auto Require = [&OutMissing](const TCHAR* Name, const UInputAction* Action)
	{
		if (Action == nullptr)
		{
			OutMissing.Add(Name);
		}
	};
	Require(TEXT("LookAction"), LookAction);
	Require(TEXT("MoveAction"), MoveAction);
	Require(TEXT("JumpAction"), JumpAction);
	Require(TEXT("SprintAction"), SprintAction);
	Require(TEXT("CrouchAction"), CrouchAction);
	Require(TEXT("AttackAction"), AttackAction);
	return OutMissing.IsEmpty();
}

UInputMappingContext* UDigetherPlayerInputData::BuildMappingContext(UObject* Outer) const
{
	UInputMappingContext* Context = NewObject<UInputMappingContext>(Outer);

	// 수정자는 상태가 없어 매핑끼리 공유한다
	UInputModifierNegate* NegateAll = NewObject<UInputModifierNegate>(Context);
	UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(Context);
	NegateY->bX = false;
	NegateY->bZ = false;
	UInputModifierSwizzleAxis* SwizzleYXZ = NewObject<UInputModifierSwizzleAxis>(Context);
	SwizzleYXZ->Order = EInputAxisSwizzle::YXZ;

	// 2D 축: D는 +X, A는 -X, W는 스위즐로 +Y, S는 스위즐 뒤 반전으로 -Y
	if (MoveAction != nullptr)
	{
		Context->MapKey(MoveAction, EKeys::D);
		Context->MapKey(MoveAction, EKeys::A).Modifiers.Add(NegateAll);
		Context->MapKey(MoveAction, EKeys::W).Modifiers.Add(SwizzleYXZ);
		FEnhancedActionKeyMapping& Back = Context->MapKey(MoveAction, EKeys::S);
		Back.Modifiers.Add(SwizzleYXZ);
		Back.Modifiers.Add(NegateAll);
	}
	// 마우스 Y는 위로 움직일 때 양수라서 반전해야 피치 입력(양수가 아래)과 맞는다
	if (LookAction != nullptr)
	{
		Context->MapKey(LookAction, EKeys::Mouse2D).Modifiers.Add(NegateY);
	}
	if (JumpAction != nullptr)
	{
		Context->MapKey(JumpAction, EKeys::SpaceBar);
	}
	if (SprintAction != nullptr)
	{
		Context->MapKey(SprintAction, EKeys::LeftShift);
	}
	if (CrouchAction != nullptr)
	{
		Context->MapKey(CrouchAction, EKeys::LeftControl);
	}
	if (AttackAction != nullptr)
	{
		Context->MapKey(AttackAction, EKeys::LeftMouseButton);
	}
	return Context;
}

#if WITH_EDITOR
EDataValidationResult UDigetherPlayerInputData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FName> Missing;
	if (!CollectMissingActions(Missing))
	{
		for (const FName Name : Missing)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s가 비어 있다"), *Name.ToString())));
		}
		Result = EDataValidationResult::Invalid;
	}
	else if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}
	return Result;
}
#endif
