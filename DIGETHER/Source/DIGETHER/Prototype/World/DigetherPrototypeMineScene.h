#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherPrototypeMineScene.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;

// 광산 실내 외형과 안전 외곽 충돌을 담당하는 비복제 정적 구조. 각 peer가 로컬에서 spawn하며 게임 규칙·승강기·굴착 지형은 다루지 않는다.
// 좌표는 액터 로컬 기준(기지 x-500~2000, 갱도 x2000~6100). 굴착 구멍(x4000~4800,y±400)과 굴착벽 패치(5300,0)는 비워 둔다.
UCLASS(Blueprintable)
class DIGETHER_API ADigetherPrototypeMineScene : public AActor
{
	GENERATED_BODY()

public:
	ADigetherPrototypeMineScene();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	// 색상별 MID의 부모 재질. 비우면 엔진 기본 도형 재질을 시도한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine Scene|Material")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine Scene|Material")
	FName TintParameterName = TEXT("Tint");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine Scene|Material")
	FLinearColor WoodColor = FLinearColor(0.20f, 0.10f, 0.04f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine Scene|Material")
	FLinearColor RockColor = FLinearColor(0.07f, 0.07f, 0.075f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine Scene|Material")
	FLinearColor MetalColor = FLinearColor(0.18f, 0.19f, 0.20f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine Scene|Material")
	FLinearColor RailColor = FLinearColor(0.80f, 0.55f, 0.03f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine Scene|Lighting")
	FLinearColor LightColor = FLinearColor(1.f, 0.62f, 0.30f);

	// 루멘 단위. 사용자 노출 시험 후 조정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine Scene|Lighting", meta = (ClampMin = "0"))
	float LightIntensityLumens = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine Scene|Lighting", meta = (ClampMin = "0"))
	float LightAttenuationRadius = 1100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine Scene|Lighting")
	bool bLightsCastShadows = false;

	UFUNCTION(BlueprintPure, Category = "Mine Scene")
	int32 GetPartCount() const { return Parts.Num(); }

	UFUNCTION(BlueprintPure, Category = "Mine Scene")
	int32 GetLightCount() const { return Lights.Num(); }

private:
	void ApplyMaterials();
	void ApplyLighting();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMaterials;

	// Parts/Lights와 같은 인덱스의 색상·조명 종류. 생성자에서만 채운다.
	TArray<uint8> PartTints;
	TArray<uint8> LightKinds;
};
