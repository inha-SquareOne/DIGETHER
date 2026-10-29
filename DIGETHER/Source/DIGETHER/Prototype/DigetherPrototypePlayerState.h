#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DigetherPrototypeTypes.h"
#include "DigetherPrototypePlayerState.generated.h"

UCLASS()
class DIGETHER_API ADigetherPrototypePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Prototype")
	FPrototypePrivateState PrivateState;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Prototype")
	EPrototypeLife PublicLife = EPrototypeLife::Active;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Prototype")
	int32 PublicBurialLevel = 0;

	// 개발 테스트에서만 사용하는 식별자이며 인증 수단이 아니다.
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Prototype")
	FString FixtureId;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
