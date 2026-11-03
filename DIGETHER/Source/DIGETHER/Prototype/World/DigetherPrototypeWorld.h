#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Prototype/DigetherPrototypeTypes.h"
#include "DigetherPrototypeWorld.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class ADigetherPrototypeElevatorCab;
class ADigetherExcavationTerrain;
class ADigetherPrototypeMineScene;
class ADigetherPrototypePortal;
class UMaterialInterface;
class UDigetherItemDefinition;

UENUM(BlueprintType)
enum class EPrototypeStationKind : uint8
{
	Start, Elevator, Deposit, Terminal, EmergencyExit, Shop
};

UCLASS()
class DIGETHER_API ADigetherPrototypeStation : public AActor, public IDigetherPrototypeInteractable
{
	GENERATED_BODY()
public:
	ADigetherPrototypeStation();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool InteractPrototype(APlayerController* Caller, bool bAlternate, FString& OutMessage) override;
	ADigetherPrototypeElevatorCab* GetElevatorCab() const { return ElevatorCab; }
	void SetElevatorCab(ADigetherPrototypeElevatorCab* Cab) { ElevatorCab = Cab; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Kind, Category="Prototype")
	EPrototypeStationKind Kind = EPrototypeStationKind::Terminal;
private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(Replicated)
	TObjectPtr<ADigetherPrototypeElevatorCab> ElevatorCab;
	UFUNCTION()
	void OnRep_Kind();
};

// 시험용 승강기: 진행률은 서버만 변경하고 탑승/회수 판정은 코어가 맡는다.
UCLASS()
class DIGETHER_API ADigetherPrototypeElevatorCab : public AActor
{
	GENERATED_BODY()
public:
	ADigetherPrototypeElevatorCab();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	void SetTravelProgress(float Progress);
	bool ContainsLocation(const FVector& Location) const;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prototype|Fixture", meta=(ClampMin="1"))
	float BoardingRadius = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prototype|Fixture", meta=(ClampMin="1"))
	float TravelHeight = 500.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Appearance")
	TObjectPtr<UMaterialInterface> CageMaterial;
private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Floor;
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> CageParts;
	FVector GroundLocation = FVector::ZeroVector;
};

// 시험용 지정 굴착 대상. 보강·여진 값은 정식 밸런스가 아니다.
UCLASS()
class DIGETHER_API ADigetherPrototypeDigCell : public AActor, public IDigetherPrototypeInteractable
{
	GENERATED_BODY()
public:
	ADigetherPrototypeDigCell();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool InteractPrototype(APlayerController* Caller, bool bAlternate, FString& OutMessage) override;
	UFUNCTION(BlueprintPure, Category="Prototype")
	bool IsExcavated() const { return bExcavated; }
	UFUNCTION(BlueprintPure, Category="Prototype")
	int32 GetSupportStrength() const { return SupportStrength; }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SupportMesh;
	UPROPERTY(ReplicatedUsing=OnRep_CellState)
	bool bExcavated = false;
	UPROPERTY(ReplicatedUsing=OnRep_CellState)
	int32 SupportStrength = 0;
	UPROPERTY(ReplicatedUsing=OnRep_CellState)
	bool bHasSupport = false;
	UPROPERTY(EditAnywhere, Category="Prototype|Fixture", meta=(ClampMin="1"))
	float TremorSeconds = 25.f;
	float SecondsSinceTremor = 0.f;
	FGuid LastRunId;
	FTimerHandle WorldTimer;
	UFUNCTION()
	void OnRep_CellState();
	void CheckTremor();
	void ApplyAppearance();
};

// 회수품 픽업. 값과 무게는 정의 자산에서 오며 서버만 인스턴스를 가진다.
UCLASS()
class DIGETHER_API ADigetherPrototypePickup : public AActor, public IDigetherPrototypeInteractable
{
	GENERATED_BODY()
public:
	ADigetherPrototypePickup();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool InteractPrototype(APlayerController* Caller, bool bAlternate, FString& OutMessage) override;
	UPROPERTY(Replicated)
	bool bReserved = false;
	// 서버 전용 인스턴스. 사망 드롭은 원래 ID와 장비 유형을 유지한다.
	FPrototypeItem StoredItem;
	// 이 픽업이 처음 만들 아이템 종류. BP_PrototypePickup에서 지정한다.
	// 사망 드롭·승강 복원·불러오기처럼 StoredItem을 직접 채우는 경로는 비워 둔 C++ 클래스를 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|Item")
	TObjectPtr<UDigetherItemDefinition> ItemDefinition;
private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
};

// 종별 기획 이전에 서버 피해 경로를 확인하는 정지형 위험 시험체.
UCLASS()
class DIGETHER_API ADigetherPrototypeHazard : public AActor
{
	GENERATED_BODY()
public:
	ADigetherPrototypeHazard();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(EditAnywhere, Category="Prototype|Fixture", meta=(ClampMin="1"))
	float Radius = 700.f;
	UPROPERTY(EditAnywhere, Category="Prototype|Fixture", meta=(ClampMin="0"))
	float Damage = 10.f;
	FTimerHandle HazardTimer;
	float AttackCooldown = 0.f;
	FVector SpawnLocation = FVector::ZeroVector;
	FGuid LastRunId;
	void ApplyHazard();
};

// 테스트 맵에 하나만 배치한다. 서버가 시험용 액터를 결정적인 위치에 만든다.
UCLASS()
class DIGETHER_API ADigetherPrototypeWorldBuilder : public AActor
{
	GENERATED_BODY()
public:
	ADigetherPrototypeWorldBuilder();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Layout, Category="Prototype|Fixture")
	int32 TestSeed = 3271;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Fixture")
	TSubclassOf<ADigetherPrototypeElevatorCab> ElevatorCabClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Fixture")
	TSubclassOf<ADigetherPrototypeStation> StationClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Fixture")
	TSubclassOf<ADigetherPrototypeDigCell> DigCellClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Fixture")
	TSubclassOf<ADigetherPrototypePickup> PickupClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Fixture")
	TSubclassOf<ADigetherPrototypeHazard> HazardClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Excavation")
	TSubclassOf<ADigetherExcavationTerrain> TerrainClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Excavation")
	TSubclassOf<ADigetherPrototypeMineScene> MineSceneClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Fixture")
	TSubclassOf<ADigetherPrototypePortal> PortalClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Excavation")
	TObjectPtr<UMaterialInterface> GroundMaterial;
private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Floor;
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> ExcavationFloorSegments;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> NorthPartition;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SouthPartition;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ZoneOneLabel;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ZoneTwoLabel;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BaseBarrier;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ElevatorPlatform;
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> RoomWalls;
	FTimerHandle ResetTimer;
	TWeakObjectPtr<ADigetherPrototypeMineScene> LocalScene;
	FGuid LastRunId;
	TArray<TWeakObjectPtr<ADigetherPrototypePickup>> RunPickups;
	void RefreshRun();
	UFUNCTION()
	void OnRep_Layout();
	bool ApplyLayout();
};
