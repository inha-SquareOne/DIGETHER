// 화면 디버그 표시 패널(V-xx), 값은 판정 코드의 컴포넌트·서브시스템에서 그대로 읽음
#include "Commands/DigetherDevCommands.h"
#include "DigetherDevDebugDraw.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Monster/DigetherMonsterCharacter.h"
#include "Game/Monster/DigetherMonsterDirectorSubsystem.h"

namespace DigetherDev
{
	namespace
	{
		template <typename EnumType>
		FString DisplayEnumName(const EnumType Value)
		{
			const UEnum* Enum = StaticEnum<EnumType>();
			return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : FString();
		}

		const UDigetherMonsterDirectorSubsystem* FindDisplayDirector(UWorld* World)
		{
			const UDigetherMonsterDirectorSubsystem* Director = World ? World->GetSubsystem<UDigetherMonsterDirectorSubsystem>() : nullptr;
			return Director && Director->IsActive() ? Director : nullptr;
		}

		// V-01 스폰 타이머 경과·판정 확률·자동 생성 정지
		void SpawnTimerLines(UWorld* World, TArray<FString>& OutLines)
		{
			const UDigetherMonsterDirectorSubsystem* Director = FindDisplayDirector(World);
			if (!Director)
			{
				OutLines.Add(TEXT("몬스터 디렉터 꺼짐"));
				return;
			}
			OutLines.Add(FString::Printf(TEXT("자동 생성: %s"), Director->IsAutoSpawnPausedForDev() ? TEXT("정지(DigetherMonsterSpawning 1로 재개)") : TEXT("동작")));
			if (!Director->HasActiveRun())
			{
				OutLines.Add(TEXT("몬스터 런 없음(출발 전)"));
				return;
			}
			if (Director->IsSpawnTimerRunning())
			{
				OutLines.Add(FString::Printf(TEXT("스폰 타이머 %.1f초 · 판정 확률 %.1f%%"), Director->GetSpawnTimerSeconds(),
					Director->GetCurrentChance() * 100.f));
			}
			else
			{
				OutLines.Add(TEXT("스폰 타이머 대기(갱도 도착 뒤 지연 후 시작)"));
			}
			TArray<ADigetherMonsterCharacter*> Monsters;
			Director->GetLiveMonsters(Monsters);
			OutLines.Add(FString::Printf(TEXT("살아 있는 몬스터 %d · 예고 대기 %d"), Monsters.Num(), Director->GetPendingSpawnCountForDev()));
		}

		// V-02 던전별 파워 사용량/한도
		void DungeonPowerLines(UWorld* World, TArray<FString>& OutLines)
		{
			const UDigetherMonsterDirectorSubsystem* Director = FindDisplayDirector(World);
			if (!Director)
			{
				OutLines.Add(TEXT("몬스터 디렉터 꺼짐"));
				return;
			}
			if (!Director->HasActiveRun())
			{
				OutLines.Add(TEXT("몬스터 런 없음(출발 전)"));
				return;
			}
			const int32 Limit = Director->GetDungeonPowerLimit();
			TArray<FName> DungeonIds;
			Director->GetDungeonIdsForDev(DungeonIds);
			if (DungeonIds.IsEmpty())
			{
				OutLines.Add(FString::Printf(TEXT("던전 구역 없음 · 던전당 한도 %d"), Limit));
				return;
			}
			for (const FName DungeonId : DungeonIds)
			{
				OutLines.Add(FString::Printf(TEXT("%s: %d / %d"), *DungeonId.ToString(), Director->GetDungeonPowerUsed(DungeonId), Limit));
			}
		}

		// V-03 엘리베이터 실제 중량·무게 단계·상승 횟수
		void ElevatorLines(UWorld* World, TArray<FString>& OutLines)
		{
			if (!World)
			{
				return;
			}
			int32 Found = 0;
			for (TActorIterator<ADigetherElevatorActor> It(World); It; ++It)
			{
				const UDigetherElevatorComponent* Elevator = It->GetElevatorComponent();
				if (!Elevator)
				{
					continue;
				}
				++Found;
				const FDigetherElevatorPublicState Public = Elevator->GetPublicState();
				FString Line = FString::Printf(TEXT("%s · 중량 %.1f / %.0fkg · 단계 %s · 상승 %d / %d"),
					*DisplayEnumName(Public.Phase), Elevator->GetLastLoadWeightKgForDev(), Elevator->GetCapacityKgForDev(),
					*DisplayEnumName(Public.WeightStage), Public.CompletedAscents, Elevator->GetTripLimitForDev());
				FDigetherElevatorAscentConfirmed LastAscent;
				if (Elevator->TryGetLastAscent(LastAscent))
				{
					Line += FString::Printf(TEXT(" · 최근 확정 %.1fkg"), LastAscent.TotalWeightKg);
				}
				OutLines.Add(MoveTemp(Line));
			}
			if (Found == 0)
			{
				OutLines.Add(TEXT("엘리베이터 없음"));
			}
		}
	}

	void RegisterDisplayPanels()
	{
		{
			Debug::FPanelDesc Panel;
			Panel.Id = TEXT("SpawnTimer");
			Panel.Title = TEXT("스폰 타이머");
			Panel.RequestIds = TEXT("V-01");
			Panel.bHostOnly = true;
			Panel.Text = &SpawnTimerLines;
			Debug::RegisterPanel(MoveTemp(Panel));
		}
		{
			Debug::FPanelDesc Panel;
			Panel.Id = TEXT("DungeonPower");
			Panel.Title = TEXT("던전 파워");
			Panel.RequestIds = TEXT("V-02");
			Panel.bHostOnly = true;
			Panel.Text = &DungeonPowerLines;
			Debug::RegisterPanel(MoveTemp(Panel));
		}
		{
			Debug::FPanelDesc Panel;
			Panel.Id = TEXT("Elevator");
			Panel.Title = TEXT("엘리베이터");
			Panel.RequestIds = TEXT("V-03");
			// 중량 합계·정원·운행 한도는 서버만 앎
			Panel.bHostOnly = true;
			Panel.Text = &ElevatorLines;
			Debug::RegisterPanel(MoveTemp(Panel));
		}
	}
}
