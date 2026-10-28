#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Prototype/DigetherPrototypeTypes.h"
#include "DigetherPrototypePlayerController.generated.h"

class ADigetherPrototypeGameMode;
class ADigetherPrototypeStation;
class UDigetherDepositWidget;
struct FHitResult;

// 시험용 플레이어 입력 진입점. 거리·시야·빈도 재검사는 서버에서 수행하고
// 최종 판정은 코어 GameMode/월드 Interactable에 위임한다.
UCLASS()
class DIGETHER_API ADigetherPrototypePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADigetherPrototypePlayerController();
	virtual void PawnLeavingGame() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void FlushPressedKeys() override;

	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target, bool bAlternate);

	UFUNCTION(Server, Reliable)
	void ServerPawnAction(APawn* Target, bool bAttack);

	// The server chooses the hit and applies the terrain modification.
	UFUNCTION(Server, Reliable)
	void ServerDig();

	UFUNCTION(Server, Reliable)
	void ServerSupportTerrain();

	UFUNCTION(Client, Reliable)
	void ClientDigConfirmed();
	UFUNCTION(Server, Reliable)
	void ServerDrop(int32 Slot);
	UFUNCTION(Server, Reliable)
	void ServerSelfEscape();
	UFUNCTION(Server, Reliable)
	void ServerSaveOrLoad(bool bLoad);

	UFUNCTION(Server, Reliable)
	void ServerCancelInteraction();

	UFUNCTION(Client, Reliable)
	void ClientPrototypeMessage(const FString& Message);

	// 납부 입력 세션. 서버가 ClientOpenDeposit으로 세션을 열고 ServerDepositAmount로 제출한다.
	// ExpectedCycle은 서버가 발급한 세션 ID이며 납부대·기간 세대·일차·목표도 일치해야 처리한다.
	UFUNCTION(Client, Reliable)
	void ClientOpenDeposit(ADigetherPrototypeStation* Station, int32 SessionId);

	UFUNCTION(Client, Reliable)
	void ClientCloseDeposit(int32 SessionId, const FString& Message);

	UFUNCTION(Client, Reliable)
	void ClientDepositRejected(int32 SessionId, const FString& Message);

	UFUNCTION(Server, Reliable)
	void ServerDepositAmount(ADigetherPrototypeStation* Station, int32 Amount, int32 ExpectedCycle);

	UFUNCTION(Server, Reliable)
	void ServerCancelDeposit(int32 SessionId);

	// 입력창과 시험 프로브가 같이 쓰는 로컬 API.
	void SetDepositInputText(const FString& Text);
	void SubmitDepositInput();
	void CloseDepositInput();
	bool IsDepositInputOpen() const { return LocalDepositSessionId != 0; }
	FString GetDepositInputText() const { return DepositInputText; }
	FString GetDepositInputError() const { return DepositInputError; }
	int32 GetDepositSessionId() const { return LocalDepositSessionId; }

	// ASCII 숫자만 허용하는 양의 int32 해석. 실패하면 OutError에 사유를 채운다.
	static bool TryParseDepositAmount(const FString& Input, int32& OutAmount, FString& OutError);

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Deposit")
	TSubclassOf<UDigetherDepositWidget> DepositWidgetClass;

	// 테스트 픽스처이며 최종 기획 값이 아니다.
	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Deposit", meta = (ClampMin = "5"))
	float DepositSessionTimeoutSeconds = 120.f;

	// 로컬 전용 조준 표시. 서버 판정을 대체하지 않으며 HUD 출력에만 사용한다.
	void SetAimTarget(AActor* NewTarget, const FString& Hint);

	UFUNCTION(BlueprintPure, Category = "Prototype")
	AActor* GetAimTarget() const { return AimTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Prototype")
	FString GetAimHint() const { return AimHint; }

	UFUNCTION(BlueprintPure, Category = "Prototype")
	FString GetLastServerMessage() const;

	// 테스트 픽스처이며 최종 기획 값이 아니다.
	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Fixture", meta = (ClampMin = "0.01"))
	float MinInteractIntervalSeconds = 0.2f;

private:
	bool ValidateInteractRequest(AActor* Target, FString& OutMessage) const;
	bool TraceTerrainRequest(bool bRequireTool, FHitResult& OutHit, FString& OutMessage);

	void OpenServerDepositSession(ADigetherPrototypeStation* Station);
	bool ValidateServerDepositSession(FString& OutMessage);
	void EndServerDepositSession(const FString& Message);
	void ClearServerDepositSession();
	void TickServerDeposit();
	void TickLocalDeposit();
	void ApplyLocalDepositUI();
	void CloseLocalDeposit();
	void RefreshDepositWidget();

	// 서버 전용 세션.
	int32 ServerDepositSessionId = 0;
	TWeakObjectPtr<ADigetherPrototypeStation> ServerDepositStation;
	int32 ServerDepositDay = 0;
	int32 ServerDepositQuotaTarget = 0;
	uint64 ServerDepositPeriodGeneration = 0;
	double ServerDepositExpiresAt = 0.0;
	double NextServerDepositCheck = 0.0;
	double LastDepositSubmitTime = -1000.0;
	int32 NextDepositSessionId = 1;

	// 로컬 전용 입력 상태.
	int32 LocalDepositSessionId = 0;
	TWeakObjectPtr<ADigetherPrototypeStation> LocalDepositStation;
	FString DepositInputText;
	FString DepositInputError;
	bool bDepositSubmitting = false;
	bool bDepositInputApplied = false;
	bool bSavedShowMouseCursor = false;
	UPROPERTY(Transient)
	TObjectPtr<UDigetherDepositWidget> DepositWidget;

	TWeakObjectPtr<AActor> AimTarget;
	FString AimHint;
	FString LastServerMessage;
	float LastMessageUntil = 0.f;
	double LastInteractTime = -1000.0;
};
