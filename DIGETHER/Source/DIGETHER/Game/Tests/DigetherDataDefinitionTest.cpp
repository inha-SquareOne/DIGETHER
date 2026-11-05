#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/AssetManager.h"
#include "Game/Core/DigetherGameplayTags.h"
#include "Game/Data/DigetherItemDefinition.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Misc/AutomationTest.h"
#include "Prototype/DigetherPrototypeDataBridge.h"
#include "Prototype/DigetherPrototypeGameMode.h"
#include "Prototype/World/DigetherPrototypeWorld.h"

namespace
{
	const TCHAR* TuningAssetPath = TEXT("/Game/Data/Tuning/DA_Tuning_Prototype.DA_Tuning_Prototype");
	const TCHAR* CargoAssetPath = TEXT("/Game/Data/Items/DA_Item_PrototypeCargo.DA_Item_PrototypeCargo");
	const TCHAR* GameModeClassPath = TEXT("/Game/Prototype/Blueprints/BP_PrototypeGameMode.BP_PrototypeGameMode_C");
	const TCHAR* PickupClassPath = TEXT("/Game/Prototype/Blueprints/BP_PrototypePickup.BP_PrototypePickup_C");

	FDigetherRunTuning MakeValidTuning()
	{
		FDigetherRunTuning Tuning;
		Tuning.ProfileVersion = 1;
		Tuning.InitialQuotaCredits = 100;
		Tuning.RunSeconds = 600.f;
		Tuning.InteractionDistanceCm = 350.f;
		Tuning.ElevatorCapacityKg = 40.f;
		Tuning.ElevatorTripLimit = 2;
		Tuning.AscentPenaltySeconds = 10.f;
		Tuning.ElevatorTravelSeconds = 4.f;
		Tuning.DepositIntervalSeconds = 0.25f;
		Tuning.EmergencyWeightLimitKg = 5.f;
		Tuning.DeathLossPercent = 100;
		Tuning.CorpseRefundPercent = 50;
		return Tuning;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherItemDefinitionTest,
	"DIGETHER.Game.Data.ItemDefinition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherItemDefinitionTest::RunTest(const FString& Parameters)
{
	UDigetherItemDefinition* Cargo = NewObject<UDigetherItemDefinition>(GetTransientPackage(), TEXT("TestCargo"));
	Cargo->ItemType = TAG_Item_Type_Cargo;
	Cargo->Traits.AddTag(TAG_Item_Trait_TwoHanded);
	Cargo->ValueCredits = 40;
	Cargo->WeightKg = 5.f;

	TArray<FText> Problems;
	TestTrue(TEXT("정상 회수품 정의는 통과"), Cargo->CollectProblems(Problems));
	TestEqual(TEXT("Primary Asset 타입"), Cargo->GetPrimaryAssetId().PrimaryAssetType, UDigetherItemDefinition::PrimaryAssetType);

	const FGuid Id = FGuid::NewGuid();
	const FPrototypeItem Item = DigetherPrototypeData::MakeItem(*Cargo, Id);
	TestEqual(TEXT("인스턴스 ID 유지"), Item.Id, Id);
	TestEqual(TEXT("가치 복사"), Item.Value, 40);
	TestEqual(TEXT("무게 복사"), Item.WeightKg, 5.f);
	TestTrue(TEXT("양손 특성 → bTwoHanded"), Item.bTwoHanded);
	TestFalse(TEXT("회수품은 장비 아님"), Item.bEquipment);
	TestFalse(TEXT("긴급 장치 특성 없음"), Item.bEmergencyDevice);

	UDigetherItemDefinition* Untyped = NewObject<UDigetherItemDefinition>(GetTransientPackage(), TEXT("TestUntyped"));
	Problems.Reset();
	TestFalse(TEXT("종류 태그 없는 정의는 실패"), Untyped->CollectProblems(Problems));

	UDigetherItemDefinition* PricedTool = NewObject<UDigetherItemDefinition>(GetTransientPackage(), TEXT("TestPricedTool"));
	PricedTool->ItemType = TAG_Item_Type_Equipment;
	PricedTool->ValueCredits = 10;
	Problems.Reset();
	TestFalse(TEXT("정산 가치가 있는 장비는 실패"), PricedTool->CollectProblems(Problems));

	UDigetherItemDefinition* NegativeWeight = NewObject<UDigetherItemDefinition>(GetTransientPackage(), TEXT("TestNegativeWeight"));
	NegativeWeight->ItemType = TAG_Item_Type_Cargo;
	NegativeWeight->WeightKg = -1.f;
	Problems.Reset();
	TestFalse(TEXT("음수 무게는 실패"), NegativeWeight->CollectProblems(Problems));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherTuningProfileTest,
	"DIGETHER.Game.Data.TuningProfile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherTuningProfileTest::RunTest(const FString& Parameters)
{
	TArray<FText> Problems;
	TestFalse(TEXT("기본값(빈 프로필)은 실패"), FDigetherRunTuning().CollectProblems(Problems));
	TestEqual(TEXT("빈 프로필의 문제 수(0이 허용되는 4개 필드 제외)"), Problems.Num(), 8);

	Problems.Reset();
	TestTrue(TEXT("정상 프로필 통과"), MakeValidTuning().CollectProblems(Problems));

	FDigetherRunTuning ThreeTrips = MakeValidTuning();
	ThreeTrips.ElevatorTripLimit = 3;
	Problems.Reset();
	TestFalse(TEXT("운행 3회는 실패(일반 상승 2회 확정)"), ThreeTrips.CollectProblems(Problems));

	// ELV_ASCENT_LIMIT는 2회 확정이다. 1회 비교 프로필은 쓰지 않으므로 거절한다.
	FDigetherRunTuning OneTrip = MakeValidTuning();
	OneTrip.ElevatorTripLimit = 1;
	Problems.Reset();
	TestFalse(TEXT("운행 1회는 실패(1회 비교 프로필 폐기)"), OneTrip.CollectProblems(Problems));
	TestEqual(TEXT("운행 1회 거절 사유 한 개"), Problems.Num(), 1);

	FDigetherRunTuning OverRefund = MakeValidTuning();
	OverRefund.CorpseRefundPercent = 101;
	Problems.Reset();
	TestFalse(TEXT("복구 101%는 실패"), OverRefund.CollectProblems(Problems));

	FDigetherRunTuning NaNTime = MakeValidTuning();
	NaNTime.RunSeconds = NAN;
	Problems.Reset();
	TestFalse(TEXT("NaN 시간은 실패"), NaNTime.CollectProblems(Problems));

	UDigetherTuningProfile* Profile = NewObject<UDigetherTuningProfile>(GetTransientPackage(), TEXT("TestProfile"));
	Profile->Values = MakeValidTuning();
	const FDigetherRunTuning Snapshot = Profile->MakeSnapshot();
	TestEqual(TEXT("스냅샷 출처 기록"), Snapshot.SourceProfile, Profile->GetPrimaryAssetId());
	TestEqual(TEXT("스냅샷 값 복사"), Snapshot.RunSeconds, 600.f);
	TestFalse(TEXT("프로필 원본에는 출처가 비어 있음"), Profile->Values.SourceProfile.IsValid());
	return true;
}

// P1 이전의 BP·C++ 값이 DA로 그대로 옮겨졌고 BP가 그 DA를 가리키는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPrototypeDataAssetsTest,
	"DIGETHER.Game.Data.PrototypeAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPrototypeDataAssetsTest::RunTest(const FString& Parameters)
{
	const UDigetherTuningProfile* Profile = LoadObject<UDigetherTuningProfile>(nullptr, TuningAssetPath);
	const UDigetherItemDefinition* Cargo = LoadObject<UDigetherItemDefinition>(nullptr, CargoAssetPath);
	if (!TestNotNull(TEXT("DA_Tuning_Prototype"), Profile) || !TestNotNull(TEXT("DA_Item_PrototypeCargo"), Cargo))
	{
		return false;
	}

	TArray<FText> Problems;
	TestTrue(TEXT("튜닝 DA 값 검사"), Profile->Values.CollectProblems(Problems));
	TestTrue(TEXT("회수품 DA 값 검사"), Cargo->CollectProblems(Problems));

	// 이전 값: BP_PrototypeGameMode 기본값(RunSeconds만 600으로 덮어씀), Rules.h의 100%/50%.
	const FDigetherRunTuning& Values = Profile->Values;
	TestEqual(TEXT("첫 목표액"), Values.InitialQuotaCredits, 100);
	TestEqual(TEXT("제한 시간(BP 덮어쓴 값)"), Values.RunSeconds, 600.f);
	TestEqual(TEXT("상호작용 거리"), Values.InteractionDistanceCm, 350.f);
	TestEqual(TEXT("엘리베이터 용량"), Values.ElevatorCapacityKg, 40.f);
	TestEqual(TEXT("운행 횟수"), Values.ElevatorTripLimit, 2);
	TestEqual(TEXT("상승 시간 차감"), Values.AscentPenaltySeconds, 10.f);
	TestEqual(TEXT("운행 시간"), Values.ElevatorTravelSeconds, 4.f);
	TestEqual(TEXT("납부 간격"), Values.DepositIntervalSeconds, 0.25f);
	TestEqual(TEXT("긴급 탈출 중량"), Values.EmergencyWeightLimitKg, 5.f);
	TestEqual(TEXT("사망 손실"), Values.DeathLossPercent, 100);
	TestEqual(TEXT("시체 복구"), Values.CorpseRefundPercent, 50);

	// 이전 값: ADigetherPrototypePickup 기본값 40원·5kg·한 손.
	TestTrue(TEXT("회수품 종류"), Cargo->IsCargo());
	TestEqual(TEXT("회수품 가치"), Cargo->ValueCredits, 40);
	TestEqual(TEXT("회수품 무게"), Cargo->WeightKg, 5.f);
	TestFalse(TEXT("회수품 한 손"), Cargo->HasTrait(TAG_Item_Trait_TwoHanded));

	const UClass* GameModeClass = LoadClass<ADigetherPrototypeGameMode>(nullptr, GameModeClassPath);
	const UClass* PickupClass = LoadClass<ADigetherPrototypePickup>(nullptr, PickupClassPath);
	if (TestNotNull(TEXT("BP_PrototypeGameMode"), GameModeClass))
	{
		TestTrue(TEXT("GameMode가 튜닝 DA를 가리킴"),
			GameModeClass->GetDefaultObject<ADigetherPrototypeGameMode>()->TuningProfile.Get() == Profile);
	}
	if (TestNotNull(TEXT("BP_PrototypePickup"), PickupClass))
	{
		TestTrue(TEXT("픽업이 회수품 DA를 가리킴"),
			PickupClass->GetDefaultObject<ADigetherPrototypePickup>()->ItemDefinition.Get() == Cargo);
	}

	// Asset Manager 등록(DefaultGame.ini)으로 두 DA를 ID로 찾을 수 있어야 cook 규칙도 적용된다.
	UAssetManager& AssetManager = UAssetManager::Get();
	FAssetData Found;
	TestTrue(TEXT("Asset Manager에 튜닝 등록"), AssetManager.GetPrimaryAssetData(Profile->GetPrimaryAssetId(), Found));
	TestTrue(TEXT("Asset Manager에 회수품 등록"), AssetManager.GetPrimaryAssetData(Cargo->GetPrimaryAssetId(), Found));
	return true;
}

#endif
