#include "Game/Monster/DigetherMonsterDevCommands.h"

#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Game/Dev/DigetherDevCommands.h"
#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterDirectorSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectGlobals.h"

namespace DigetherMonsterDev
{
#if !UE_BUILD_SHIPPING
	namespace
	{
		// UObject 미캡처, 실행 시점에 경로로 Controller 조회
		APlayerController* FindLocalController(const TArray<FString>& Args)
		{
			APlayerController* Controller = Args.Num() >= 1 ? FindObject<APlayerController>(nullptr, *Args[0]) : nullptr;
			return IsValid(Controller) && Controller->IsLocalController() ? Controller : nullptr;
		}

		UDigetherMonsterDirectorSubsystem* FindActiveDirector(const APlayerController* Controller)
		{
			UWorld* World = Controller ? Controller->GetWorld() : nullptr;
			UDigetherMonsterDirectorSubsystem* Director = World ? World->GetSubsystem<UDigetherMonsterDirectorSubsystem>() : nullptr;
			return Director && Director->IsActive() ? Director : nullptr;
		}

		UDigetherMonsterDefinition* LoadDefinition(const FString& Name)
		{
			if (!UAssetManager::IsInitialized())
			{
				return nullptr;
			}
			const FPrimaryAssetId Id(UDigetherMonsterDefinition::PrimaryAssetType, FName(*Name));
			const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(Id);
			return Cast<UDigetherMonsterDefinition>(Path.TryLoad());
		}

		const TCHAR* NoDirectorMessage = TEXT("호스트에서만 쓸 수 있습니다(몬스터 디렉터 비활성).");
	}
#endif

	void RegisterCommands()
	{
#if !UE_BUILD_SHIPPING
		DigetherDev::Register(TEXT("DigetherMonsterSpawn"), [](const TArray<FString>& Args)
		{
			APlayerController* Controller = FindLocalController(Args);
			UDigetherMonsterDirectorSubsystem* Director = FindActiveDirector(Controller);
			if (!Director)
			{
				return FString(NoDirectorMessage);
			}
			if (Args.Num() < 2)
			{
				return FString(TEXT("사용법: DigetherMonsterSpawn <정의 에셋 이름>"));
			}
			UDigetherMonsterDefinition* Definition = LoadDefinition(Args[1]);
			if (!Definition)
			{
				return FString::Printf(TEXT("몬스터 정의 %s를 찾지 못했습니다."), *Args[1]);
			}
			// 허용 지점이 없으면 시점 앞 5m
			FVector Location = FVector::ZeroVector;
			FRotator Rotation = FRotator::ZeroRotator;
			Controller->GetPlayerViewPoint(Location, Rotation);
			const FTransform Fallback(FRotator(0.f, Rotation.Yaw + 180.f, 0.f), Location + Rotation.Vector() * 500.f);
			const EDigetherMonsterSpawnResult Result = Director->DevSpawn(Definition, Fallback);
			return FString::Printf(TEXT("몬스터 생성 결과: %s"), *UEnum::GetValueAsString(Result));
		});

		DigetherDev::Register(TEXT("DigetherMonsterForceHabitat"), [](const TArray<FString>& Args)
		{
			UDigetherMonsterDirectorSubsystem* Director = FindActiveDirector(FindLocalController(Args));
			if (!Director || !Director->HasActiveRun())
			{
				return FString(TEXT("진행 중인 몬스터 런이 없습니다(출발 후 사용)."));
			}
			Director->DevForceHabitat();
			return FString(TEXT("서식지 확률을 무시하고 배치했습니다."));
		});

		DigetherDev::Register(TEXT("DigetherMonsterAdvanceSpawnTimer"), [](const TArray<FString>& Args)
		{
			UDigetherMonsterDirectorSubsystem* Director = FindActiveDirector(FindLocalController(Args));
			if (!Director || !Director->IsSpawnTimerRunning())
			{
				return FString(TEXT("스폰 타이머가 돌고 있지 않습니다(갱도 도착 10초 뒤 시작)."));
			}
			const float Seconds = Args.Num() >= 2 ? FCString::Atof(*Args[1]) : 0.f;
			Director->DevAdvanceSpawnTimer(Seconds);
			return FString::Printf(TEXT("스폰 타이머 %.1f초, 현재 판정 확률 %.3f"), Director->GetSpawnTimerSeconds(), Director->GetCurrentChance());
		});

		DigetherDev::Register(TEXT("DigetherMonsterEvaluate"), [](const TArray<FString>& Args)
		{
			UDigetherMonsterDirectorSubsystem* Director = FindActiveDirector(FindLocalController(Args));
			if (!Director || !Director->HasActiveRun())
			{
				return FString(TEXT("진행 중인 몬스터 런이 없습니다."));
			}
			Director->DevRunEvaluationNow();
			return FString::Printf(TEXT("스폰 판정 1회 실행(확률 %.3f), 결과는 LogDigetherMonster 확인"), Director->GetCurrentChance());
		});
#endif
	}
}
