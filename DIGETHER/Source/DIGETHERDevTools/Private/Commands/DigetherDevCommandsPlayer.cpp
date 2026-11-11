// 캐릭터·멀티 개발 명령, 판정 변경은 DIGETHER 모듈의 개발 진입점만 호출
#include "Commands/DigetherDevCommands.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"

namespace DigetherDev
{
	namespace
	{
		using FPlayerModeAction = TFunction<bool(ADigetherGameplayGameMode& Mode, const FContext& Context,
			const TArray<FString>& Args, FString& OutMessage)>;

		// 시작 지점 겹침 방지 높이, 개발 이동 기존 값(DigetherTeleportOut)과 같음
		constexpr double PlayerStartLiftCm = 50.0;
		// Here로 다른 참가자를 부를 때 시점 앞 거리와 사람 간 옆 간격(시험 Pawn 기준 개발 값)
		constexpr double SummonForwardCm = 150.0;
		constexpr double SummonSideSpacingCm = 100.0;

		struct FBookmark
		{
			FString MapName;
			FVector Location = FVector::ZeroVector;
			FRotator Rotation = FRotator::ZeroRotator;
		};

		// 호스트 프로세스 메모리 보관, 맵 이름과 함께 저장해 다른 맵 좌표로 이동 방지
		TMap<FString, FBookmark>& Bookmarks()
		{
			static TMap<FString, FBookmark> Map;
			return Map;
		}

		void RegisterPlayerHost(const TCHAR* Name, const TCHAR* RequestIds, const TCHAR* Summary, TArray<FParam> Params,
			FPlayerModeAction Action, const TCHAR* UnavailableReason = TEXT(""))
		{
			FCommandDesc Desc;
			Desc.Name = Name;
			Desc.Category = TEXT("캐릭터");
			Desc.RequestIds = RequestIds;
			Desc.Summary = Summary;
			Desc.Params = MoveTemp(Params);
			Desc.Scope = EScope::Host;
			Desc.UnavailableReason = UnavailableReason;
			Register(MoveTemp(Desc), [Action = MoveTemp(Action)](const FContext& Context, const TArray<FString>& Args)
			{
				ADigetherGameplayGameMode* Mode = Context.GetGameMode();
				if (Mode == nullptr)
				{
					return FResult::Fail(TEXT("게임 맵(거점·갱도)에서만 쓸 수 있습니다."));
				}
				FString Message;
				return Action(*Mode, Context, Args, Message) ? FResult::Ok(Message) : FResult::Fail(Message);
			});
		}

		bool ForEachTargetPlayer(const FContext& Context, const FString& Token, FString& OutMessage,
			const TFunctionRef<bool(ADigetherGameplayPlayerState* Player, int32 Index, FString& OutLine)> Action)
		{
			TArray<ADigetherGameplayPlayerState*> Players;
			if (!ResolvePlayers(Context, Token, Players, OutMessage))
			{
				return false;
			}
			TArray<FString> Lines;
			int32 Succeeded = 0;
			for (int32 Index = 0; Index < Players.Num(); ++Index)
			{
				FString Line;
				Succeeded += Action(Players[Index], Index, Line) ? 1 : 0;
				Lines.Add(MoveTemp(Line));
			}
			OutMessage = FString::Join(Lines, TEXT(" / "));
			return Succeeded > 0;
		}

		bool IsPlayerArgOn(const FString& Value)
		{
			return Value == TEXT("1") || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("on"), ESearchCase::IgnoreCase);
		}

		FParam MakeTargetParam()
		{
			return MakeParam(TEXT("대상"), EParamType::Player, true, TEXT("self·all·플레이어 번호"));
		}

		ADigetherGameplayPlayerState* GetSelf(const FContext& Context)
		{
			return Context.GameController ? Context.GameController->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
		}

		FString GetMapKey(const UWorld* World)
		{
			return World ? UWorld::RemovePIEPrefix(World->GetMapName()) : FString();
		}

		bool TryParseCoordinate(const FString& Text, double& OutValue)
		{
			const FString Trimmed = Text.TrimStartAndEnd();
			if (!Trimmed.IsNumeric())
			{
				return false;
			}
			OutValue = FCString::Atod(*Trimmed);
			return FMath::IsFinite(OutValue);
		}

		bool TeleportToStart(ADigetherGameplayGameMode& Mode, ADigetherGameplayPlayerState* Player, FString& OutLine)
		{
			AController* Controller = Cast<AController>(Player->GetOwner());
			AActor* Start = IsValid(Controller) ? Mode.FindPlayerStart(Controller) : nullptr;
			if (!IsValid(Start))
			{
				// 연결 끊긴 참가자는 컨트롤러가 없어 첫 PlayerStart 사용(귀환 처리와 같은 대체)
				for (TActorIterator<APlayerStart> It(Mode.GetWorld()); It; ++It)
				{
					Start = *It;
					break;
				}
			}
			if (!IsValid(Start))
			{
				OutLine = TEXT("PlayerStart가 없습니다.");
				return false;
			}
			return Mode.DevTeleportPlayer(Player, Start->GetActorLocation() + FVector(0.0, 0.0, PlayerStartLiftCm),
				FRotator(0.0, Start->GetActorRotation().Yaw, 0.0), OutLine);
		}
	}

	void RegisterPlayerCommands()
	{
		// --- C-01 무적·스태미나 ---
		RegisterPlayerHost(TEXT("DigetherGod"), TEXT("C-01"),
			TEXT("몸이 피해를 받지 않게 한다(엔진 god와 같은 피해 수신 끄기). 체력 시스템이 없어 몬스터 엔진 피해만 막고, 시간 종료 사망은 막지 못함. 몸이 새로 생기면 다시 켤 것"),
			{ MakeParam(TEXT("켜기"), EParamType::Bool), MakeTargetParam() },
			[](ADigetherGameplayGameMode&, const FContext& Context, const TArray<FString>& Args, FString& OutMessage)
			{
				const bool bGod = IsPlayerArgOn(Args[0]);
				return ForEachTargetPlayer(Context, ArgOr(Args, 1, TEXT("self")), OutMessage,
					[bGod](ADigetherGameplayPlayerState* Player, int32, FString& OutLine)
					{
						APawn* Pawn = Player->GetGameplayPawn();
						if (!IsValid(Pawn))
						{
							OutLine = FString::Printf(TEXT("플레이어 %d: 몸 없음"), Player->GetPlayerId());
							return false;
						}
						// bCanBeDamaged는 복제 속성, 서버에서 바꾸면 전원에게 반영
						Pawn->SetCanBeDamaged(!bGod);
						OutLine = FString::Printf(TEXT("플레이어 %d 무적 %s"), Player->GetPlayerId(), bGod ? TEXT("켬") : TEXT("끔"));
						return true;
					});
			});
		RegisterPlayerHost(TEXT("DigetherInfiniteStamina"), TEXT("C-01"), TEXT("스태미나를 소모하지 않게 한다"),
			{ MakeParam(TEXT("켜기"), EParamType::Bool, true), MakeTargetParam() },
			[](ADigetherGameplayGameMode&, const FContext&, const TArray<FString>&, FString& OutMessage)
			{
				OutMessage = TEXT("미연결");
				return false;
			},
			TEXT("스태미나 시스템이 없음(정식 플레이어 캐릭터 구현 뒤 연결)"));

		// --- C-02 이동 ---
		RegisterPlayerHost(TEXT("DigetherTeleport"), TEXT("C-02"),
			TEXT("대상을 엘리베이터 안·시작 지점·내 시점 위치(Here)·좌표(XYZ x y z)로 옮긴다. Elevator·Start·Here는 좌표 없이 [대상]만, 예: DigetherTeleport Here 2"),
			{ MakeEnumParam(TEXT("위치"), { TEXT("Elevator"), TEXT("Start"), TEXT("Here"), TEXT("XYZ") }),
			  MakeParam(TEXT("x"), EParamType::Text, true, TEXT("XYZ일 때 cm")),
			  MakeParam(TEXT("y"), EParamType::Text, true, TEXT("XYZ일 때 cm")),
			  MakeParam(TEXT("z"), EParamType::Text, true, TEXT("XYZ일 때 cm")),
			  MakeTargetParam() },
			[](ADigetherGameplayGameMode& Mode, const FContext& Context, const TArray<FString>& Args, FString& OutMessage)
			{
				const FString Where = Args[0];
				// 웹 화면은 빈 칸을 빼고 보내므로 좌표 없는 방식은 둘째 인자를 대상으로 해석
				const bool bXyz = Where.Equals(TEXT("XYZ"), ESearchCase::IgnoreCase);
				FVector Coordinates = FVector::ZeroVector;
				if (bXyz)
				{
					if (Args.Num() < 4 || !TryParseCoordinate(Args[1], Coordinates.X) || !TryParseCoordinate(Args[2], Coordinates.Y)
						|| !TryParseCoordinate(Args[3], Coordinates.Z))
					{
						OutMessage = TEXT("XYZ는 숫자 좌표 세 개가 필요합니다. 예: DigetherTeleport XYZ 100 0 200 [대상]");
						return false;
					}
				}
				else if (Args.Num() > 2)
				{
					OutMessage = TEXT("Elevator·Start·Here에는 좌표를 넣지 않습니다. 예: DigetherTeleport Start 2");
					return false;
				}
				const FString Token = ArgOr(Args, bXyz ? 4 : 1, TEXT("self"));
				if (Where.Equals(TEXT("Elevator"), ESearchCase::IgnoreCase))
				{
					TArray<ADigetherGameplayPlayerState*> Players;
					return ResolvePlayers(Context, Token, Players, OutMessage) && Mode.DevMovePlayersIntoElevator(Players, OutMessage);
				}
				if (Where.Equals(TEXT("Start"), ESearchCase::IgnoreCase))
				{
					return ForEachTargetPlayer(Context, Token, OutMessage,
						[&Mode](ADigetherGameplayPlayerState* Player, int32, FString& OutLine)
						{
							return TeleportToStart(Mode, Player, OutLine);
						});
				}
				if (bXyz)
				{
					return ForEachTargetPlayer(Context, Token, OutMessage,
						[&Mode, Coordinates](ADigetherGameplayPlayerState* Player, int32, FString& OutLine)
						{
							const APawn* Pawn = Player->GetGameplayPawn();
							const FRotator Rotation = IsValid(Pawn) ? Pawn->GetActorRotation() : FRotator::ZeroRotator;
							return Mode.DevTeleportPlayer(Player, Coordinates, Rotation, OutLine);
						});
				}
				// Here: 지금 화면 시점(자유 카메라면 카메라 위치), 자기 자신은 그 자리·다른 사람은 시점 앞에 옆으로 나란히
				if (Context.ViewController == nullptr)
				{
					OutMessage = TEXT("시점을 가진 로컬 화면이 없습니다.");
					return false;
				}
				FVector Eye;
				FRotator ViewRotation;
				Context.ViewController->GetPlayerViewPoint(Eye, ViewRotation);
				const FRotator Facing(0.0, ViewRotation.Yaw, 0.0);
				const ADigetherGameplayPlayerState* Self = GetSelf(Context);
				return ForEachTargetPlayer(Context, Token, OutMessage,
					[&Mode, Eye, Facing, Self](ADigetherGameplayPlayerState* Player, const int32 Index, FString& OutLine)
					{
						const FVector Side = Facing.Quaternion().GetRightVector();
						const FVector Location = Player == Self ? Eye
							: Eye + Facing.Vector() * SummonForwardCm + Side * (SummonSideSpacingCm * Index);
						return Mode.DevTeleportPlayer(Player, Location, Facing, OutLine);
					});
			});
		RegisterPlayerHost(TEXT("DigetherBookmark"), TEXT("C-02"),
			TEXT("내 위치를 이름으로 저장(Save)·이동(Go)·목록(List). 호스트 프로세스 메모리 보관, 같은 맵에서만 이동"),
			{ MakeEnumParam(TEXT("동작"), { TEXT("Save"), TEXT("Go"), TEXT("List") }),
			  MakeParam(TEXT("이름"), EParamType::Text, true, TEXT("비우면 기본")) },
			[](ADigetherGameplayGameMode& Mode, const FContext& Context, const TArray<FString>& Args, FString& OutMessage)
			{
				const FString Action = Args[0];
				const FString MapKey = GetMapKey(Context.World);
				if (Action.Equals(TEXT("List"), ESearchCase::IgnoreCase))
				{
					TArray<FString> Lines;
					for (const TPair<FString, FBookmark>& Pair : Bookmarks())
					{
						Lines.Add(FString::Printf(TEXT("%s(%s %s)"), *Pair.Key, *Pair.Value.MapName, *Pair.Value.Location.ToCompactString()));
					}
					OutMessage = Lines.IsEmpty() ? FString(TEXT("저장한 위치가 없습니다.")) : FString::Join(Lines, TEXT(", "));
					return true;
				}
				const FString Name = JoinFrom(Args, 1).TrimStartAndEnd().IsEmpty() ? FString(TEXT("기본")) : JoinFrom(Args, 1).TrimStartAndEnd();
				ADigetherGameplayPlayerState* Self = GetSelf(Context);
				const APawn* Pawn = IsValid(Self) ? Self->GetGameplayPawn() : nullptr;
				if (!IsValid(Pawn))
				{
					OutMessage = TEXT("내 몸이 없습니다.");
					return false;
				}
				if (Action.Equals(TEXT("Save"), ESearchCase::IgnoreCase))
				{
					FBookmark Bookmark;
					Bookmark.MapName = MapKey;
					Bookmark.Location = Pawn->GetActorLocation();
					Bookmark.Rotation = FRotator(0.0, Pawn->GetActorRotation().Yaw, 0.0);
					Bookmarks().Add(Name, Bookmark);
					OutMessage = FString::Printf(TEXT("위치 '%s' 저장: %s"), *Name, *Bookmark.Location.ToCompactString());
					return true;
				}
				const FBookmark* Found = Bookmarks().Find(Name);
				if (Found == nullptr)
				{
					OutMessage = FString::Printf(TEXT("저장한 위치 '%s'가 없습니다."), *Name);
					return false;
				}
				if (Found->MapName != MapKey)
				{
					OutMessage = FString::Printf(TEXT("위치 '%s'는 다른 맵(%s)에서 저장했습니다."), *Name, *Found->MapName);
					return false;
				}
				return Mode.DevTeleportPlayer(Self, Found->Location, Found->Rotation, OutMessage);
			});

		// --- C-03 사망·부활 ---
		RegisterPlayerHost(TEXT("DigetherKill"), TEXT("C-03"),
			TEXT("탐사 중 참가자를 즉사시킨다(생존 표시·관전 전환만). 사망 손실·사망 전 잔고 기록·드롭·시체는 재현되지 않음(사망 조율·인벤토리 구현 뒤)"),
			{ MakeTargetParam() },
			[](ADigetherGameplayGameMode& Mode, const FContext& Context, const TArray<FString>& Args, FString& OutMessage)
			{
				return ForEachTargetPlayer(Context, ArgOr(Args, 0, TEXT("self")), OutMessage,
					[&Mode](ADigetherGameplayPlayerState* Player, int32, FString& OutLine)
					{
						return Mode.DevKillPlayer(Player, OutLine);
					});
			});
		RegisterPlayerHost(TEXT("DigetherRevive"), TEXT("C-03"),
			TEXT("사망한 참가자를 그 자리에서 부활시킨다(관전 해제·시점 복귀). 탈출자는 대상 아님, 사망 손실 되돌림 없음"),
			{ MakeTargetParam() },
			[](ADigetherGameplayGameMode& Mode, const FContext& Context, const TArray<FString>& Args, FString& OutMessage)
			{
				return ForEachTargetPlayer(Context, ArgOr(Args, 0, TEXT("self")), OutMessage,
					[&Mode](ADigetherGameplayPlayerState* Player, int32, FString& OutLine)
					{
						return Mode.DevRevivePlayer(Player, OutLine);
					});
			});
	}
}
