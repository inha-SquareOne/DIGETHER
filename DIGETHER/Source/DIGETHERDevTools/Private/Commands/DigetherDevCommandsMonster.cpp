// 몬스터 개발 명령(호스트), 기존 4종 이전 + 소환 확장·전부 제거·자동 생성 정지(F-01·F-02)
#include "Commands/DigetherDevCommands.h"

#include "CollisionQueryParams.h"
#include "Engine/AssetManager.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterDirectorSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace DigetherDev
{
	namespace
	{
		UDigetherMonsterDirectorSubsystem* FindActiveDirector(const FContext& Context)
		{
			UDigetherMonsterDirectorSubsystem* Director = Context.World
				? Context.World->GetSubsystem<UDigetherMonsterDirectorSubsystem>() : nullptr;
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

		TArray<FString> ListDefinitionNames()
		{
			TArray<FString> Names;
			if (UAssetManager::IsInitialized())
			{
				TArray<FPrimaryAssetId> Ids;
				UAssetManager::Get().GetPrimaryAssetIdList(UDigetherMonsterDefinition::PrimaryAssetType, Ids);
				for (const FPrimaryAssetId& Id : Ids)
				{
					Names.Add(Id.PrimaryAssetName.ToString());
				}
			}
			Names.Sort();
			return Names;
		}

		const TCHAR* NoDirector = TEXT("몬스터 디렉터가 꺼져 있습니다(호스트 게임 맵에서 사용).");
		const TCHAR* MonsterAutoSpawnPausedText = TEXT("자동 생성 정지 중입니다(DigetherMonsterSpawning 1로 재개). 수동 소환은 DigetherMonsterSpawn");

		// 요청서 F-01 수량 범위
		constexpr int32 MonsterSpawnMaxCount = 20;
		// Front·Point 대체 위치, 시점 앞 5m
		constexpr float MonsterFrontDistance = 500.f;
		// Aim 선 추적 최대 거리, 개발 편의값
		constexpr float MonsterAimTraceDistance = 10000.f;

		// 화면 중앙 선 추적 지점, 캡슐이 바닥·벽에 묻히지 않게 법선 방향과 위로 띄움
		bool TraceMonsterAimLocation(const FContext& Context, const FVector& ViewLocation, const FRotator& ViewRotation,
			const UDigetherMonsterDefinition& Definition, FVector& OutLocation, FString& OutError)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(DigetherDevMonsterAim), false);
			if (Context.ViewController)
			{
				Params.AddIgnoredActor(Context.ViewController->GetPawn());
				Params.AddIgnoredActor(Context.ViewController->GetViewTarget());
			}
			if (Context.GameController)
			{
				Params.AddIgnoredActor(Context.GameController->GetPawn());
			}
			FHitResult Hit;
			const FVector End = ViewLocation + ViewRotation.Vector() * MonsterAimTraceDistance;
			if (!Context.World || !Context.World->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
			{
				OutError = FString::Printf(TEXT("화면 중앙 %.0fm 안에 조준한 지형이 없습니다."), MonsterAimTraceDistance / 100.f);
				return false;
			}
			const FVector Normal = Hit.ImpactNormal.IsNearlyZero() ? FVector::UpVector : Hit.ImpactNormal;
			OutLocation = Hit.ImpactPoint + Normal * Definition.Movement.CapsuleRadius
				+ FVector(0.f, 0.f, Definition.Movement.CapsuleHalfHeight);
			return true;
		}

		FCommandDesc MakeMonsterDesc(const TCHAR* Name, const TCHAR* RequestIds, const TCHAR* Summary)
		{
			FCommandDesc Desc;
			Desc.Name = Name;
			Desc.Category = TEXT("몬스터");
			Desc.RequestIds = RequestIds;
			Desc.Summary = Summary;
			Desc.Scope = EScope::Host;
			return Desc;
		}
	}

	void RegisterMonsterCommands()
	{
		{
			FCommandDesc Desc = MakeMonsterDesc(TEXT("DigetherMonsterSpawn"), TEXT("F-01"),
				TEXT("정의 에셋 이름으로 몬스터를 만든다. Point는 허용 스폰 지점 우선(없으면 시점 앞 5m), Aim은 화면 중앙 조준 지점, Front는 시점 앞 5m 강제. NoPower는 던전 파워에 넣지 않음"));
			FParam Count = MakeRangeParam(TEXT("수"), EParamType::Int, 1, MonsterSpawnMaxCount, true);
			Count.Default = TEXT("1");
			// 웹 화면은 빈 선택 인자에서 멈추므로 기본값을 채워 뒤 인자까지 전달
			FParam Placement = MakeEnumParam(TEXT("위치"), { TEXT("Point"), TEXT("Aim"), TEXT("Front") }, true);
			Placement.Default = TEXT("Point");
			FParam Power = MakeEnumParam(TEXT("파워"), { TEXT("Power"), TEXT("NoPower") }, true);
			Power.Default = TEXT("Power");
			Desc.Params = { MakeParam(TEXT("종"), EParamType::Text, false, TEXT("예: DA_Monster_GroundSpider")), Count, Placement, Power };
			Register(MoveTemp(Desc), [](const FContext& Context, const TArray<FString>& Args)
			{
				UDigetherMonsterDirectorSubsystem* Director = FindActiveDirector(Context);
				if (!Director)
				{
					return FResult::Fail(NoDirector);
				}
				UDigetherMonsterDefinition* Definition = LoadDefinition(Args[0]);
				if (!Definition)
				{
					return FResult::Fail(FString::Printf(TEXT("몬스터 정의 %s를 찾지 못했습니다. 있는 정의: %s"), *Args[0],
						*FString::Join(ListDefinitionNames(), TEXT(", "))));
				}
				const int32 SpawnCount = FMath::Clamp(FCString::Atoi(*ArgOr(Args, 1, TEXT("1"))), 1, MonsterSpawnMaxCount);
				const FString PlacementName = ArgOr(Args, 2, TEXT("Point"));
				const bool bAim = PlacementName.Equals(TEXT("Aim"), ESearchCase::IgnoreCase);
				const bool bFront = PlacementName.Equals(TEXT("Front"), ESearchCase::IgnoreCase);
				const bool bChargePower = !ArgOr(Args, 3, TEXT("Power")).Equals(TEXT("NoPower"), ESearchCase::IgnoreCase);

				FVector ViewLocation = FVector::ZeroVector;
				FRotator ViewRotation = FRotator::ZeroRotator;
				if (Context.ViewController)
				{
					Context.ViewController->GetPlayerViewPoint(ViewLocation, ViewRotation);
				}
				else if (bAim || bFront)
				{
					return FResult::Fail(TEXT("시점을 가진 로컬 플레이어가 없어 Aim·Front를 쓸 수 없습니다(Point 사용)."));
				}
				const FRotator FaceViewer(0.f, ViewRotation.Yaw + 180.f, 0.f);
				FVector BaseLocation = ViewLocation + ViewRotation.Vector() * MonsterFrontDistance;
				if (bAim)
				{
					FString AimError;
					if (!TraceMonsterAimLocation(Context, ViewLocation, ViewRotation, *Definition, BaseLocation, AimError))
					{
						return FResult::Fail(AimError);
					}
				}

				UDigetherMonsterDirectorSubsystem::FDevSpawnOptions Options;
				Options.bForceTransform = bAim || bFront;
				Options.bChargePower = bChargePower;
				// 여러 마리는 같은 지점에 겹치지 않게 기준점 둘레에 배치, Point는 마리마다 지점을 새로 고름
				const float RingRadius = FMath::Max(150.f, Definition->Movement.CapsuleRadius * 2.5f);
				int32 Spawned = 0;
				TMap<FString, int32> Failures;
				for (int32 Index = 0; Index < SpawnCount; ++Index)
				{
					FVector Location = BaseLocation;
					if (Index > 0)
					{
						const float Angle = 2.f * UE_PI * static_cast<float>(Index - 1) / static_cast<float>(FMath::Max(1, SpawnCount - 1));
						Location += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * RingRadius;
					}
					const EDigetherMonsterSpawnResult Result = Director->DevSpawnWithOptions(Definition, FTransform(FaceViewer, Location), Options);
					if (Result == EDigetherMonsterSpawnResult::Spawned)
					{
						++Spawned;
					}
					else
					{
						++Failures.FindOrAdd(UEnum::GetValueAsString(Result));
					}
				}
				FString Text = FString::Printf(TEXT("%s %d/%d마리 생성(위치 %s, 파워 %s)"), *Definition->GetName(), Spawned, SpawnCount,
					bAim ? TEXT("Aim") : bFront ? TEXT("Front") : TEXT("Point"), bChargePower ? TEXT("부과") : TEXT("제외"));
				for (const TPair<FString, int32>& Failure : Failures)
				{
					Text += FString::Printf(TEXT(", 실패 %s ×%d"), *Failure.Key, Failure.Value);
				}
				if (Director->IsAutoSpawnPausedForDev())
				{
					Text += TEXT(" · 자동 생성 정지 중(수동 소환은 동작)");
				}
				return Spawned > 0 ? FResult::Ok(Text) : FResult::Fail(Text);
			});
		}
		Register(MakeMonsterDesc(TEXT("DigetherMonsterClear"), TEXT("F-02"), TEXT("살아 있는 몬스터를 전부 제거하고 대기 중인 예고 스폰을 취소한다")),
			[](const FContext& Context, const TArray<FString>&)
			{
				UDigetherMonsterDirectorSubsystem* Director = FindActiveDirector(Context);
				if (!Director)
				{
					return FResult::Fail(NoDirector);
				}
				int32 Cancelled = 0;
				const int32 Removed = Director->DevClearMonsters(&Cancelled);
				return FResult::Ok(FString::Printf(TEXT("몬스터 %d마리 제거, 대기 예고 %d건 취소(둥지 등 부속은 유지)"), Removed, Cancelled));
			});
		{
			FCommandDesc Desc = MakeMonsterDesc(TEXT("DigetherMonsterSpawning"), TEXT("F-02"),
				TEXT("자동 생성(주기 판정·예고 스폰·서식지 배치)을 켜거나 끈다. 끄면 대기 예고를 취소, 수동 소환은 계속 동작"));
			FParam On = MakeParam(TEXT("켜기"), EParamType::Bool);
			On.Default = TEXT("0");
			Desc.Params = { On };
			Register(MoveTemp(Desc), [](const FContext& Context, const TArray<FString>& Args)
			{
				UDigetherMonsterDirectorSubsystem* Director = FindActiveDirector(Context);
				if (!Director)
				{
					return FResult::Fail(NoDirector);
				}
				const FString& Value = Args[0];
				const bool bOn = Value == TEXT("1") || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
					|| Value.Equals(TEXT("on"), ESearchCase::IgnoreCase);
				const int32 Cancelled = Director->DevSetAutoSpawnPaused(!bOn);
				return FResult::Ok(bOn ? FString(TEXT("몬스터 자동 생성 재개"))
					: FString::Printf(TEXT("몬스터 자동 생성 정지, 대기 예고 %d건 취소(맵 준비 때 건너뛴 서식지 배치는 재개해도 다시 하지 않음)"), Cancelled));
			});
		}
		Register(MakeMonsterDesc(TEXT("DigetherMonsterForceHabitat"), TEXT("F-05"), TEXT("서식지 확률을 무시하고 둥지 몬스터를 배치한다")),
			[](const FContext& Context, const TArray<FString>&)
			{
				UDigetherMonsterDirectorSubsystem* Director = FindActiveDirector(Context);
				if (!Director || !Director->HasActiveRun())
				{
					return FResult::Fail(TEXT("진행 중인 몬스터 런이 없습니다(출발 후 사용)."));
				}
				// 서식지 배치는 자동 생성 경로라 정지 중이면 디렉터가 막음
				if (Director->IsAutoSpawnPausedForDev())
				{
					return FResult::Fail(MonsterAutoSpawnPausedText);
				}
				Director->DevForceHabitat();
				return FResult::Ok(TEXT("서식지 확률을 무시하고 배치했습니다."));
			});
		{
			FCommandDesc Desc = MakeMonsterDesc(TEXT("DigetherMonsterAdvanceSpawnTimer"), TEXT("F-04"), TEXT("스폰 타이머를 초 단위로 앞당긴다"));
			Desc.Params = { MakeRangeParam(TEXT("초"), EParamType::Float, 0, 3600) };
			Register(MoveTemp(Desc), [](const FContext& Context, const TArray<FString>& Args)
			{
				UDigetherMonsterDirectorSubsystem* Director = FindActiveDirector(Context);
				if (!Director || !Director->IsSpawnTimerRunning())
				{
					return FResult::Fail(TEXT("스폰 타이머가 돌고 있지 않습니다(갱도 도착 10초 뒤 시작)."));
				}
				Director->DevAdvanceSpawnTimer(FCString::Atof(*Args[0]));
				return FResult::Ok(FString::Printf(TEXT("스폰 타이머 %.1f초, 현재 판정 확률 %.3f"), Director->GetSpawnTimerSeconds(),
					Director->GetCurrentChance()));
			});
		}
		Register(MakeMonsterDesc(TEXT("DigetherMonsterEvaluate"), TEXT("F-04"), TEXT("스폰 판정을 즉시 한 번 실행한다")),
			[](const FContext& Context, const TArray<FString>&)
			{
				UDigetherMonsterDirectorSubsystem* Director = FindActiveDirector(Context);
				if (!Director || !Director->HasActiveRun())
				{
					return FResult::Fail(TEXT("진행 중인 몬스터 런이 없습니다."));
				}
				// 판정은 자동 생성 경로라 정지 중이면 디렉터가 건너뜀
				if (Director->IsAutoSpawnPausedForDev())
				{
					return FResult::Fail(MonsterAutoSpawnPausedText);
				}
				Director->DevRunEvaluationNow();
				return FResult::Ok(FString::Printf(TEXT("스폰 판정 1회 실행(확률 %.3f), 결과는 LogDigetherMonster 확인"),
					Director->GetCurrentChance()));
			});
	}
}
