#pragma once
#include "CoreMinimal.h"
#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDigetherSoilPerformance, Log, All);
namespace DigetherSoilProfiling
{
	// ms 단위, 총시간은 로그 출력 전까지 측정
	struct FDigetherSoilTimings
	{
		double Copy = 0, Carve = 0, Surface = 0, Normals = 0, Remove = 0, MeshCollision = 0, Commit = 0, Total = 0;
		int32 Result = -1;
	};
	inline TAutoConsoleVariable<int32> Enabled(TEXT("digether.SoilProfile"), 0,
											   TEXT("0: off, 1: record last excavation, 2: also log milliseconds"));
	// 스레드별 현재 요청과 마지막 완료값, 굴착의 멀티스레드 안전 보장과 별개
	inline thread_local FDigetherSoilTimings* Active = nullptr;
	inline thread_local FDigetherSoilTimings Last;
	// 누적할 시간 필드 입력 → 종료 시 ms 합산, 조기 return도 계측
	class FDigetherSoilStageTimer
	{
	public:
		explicit FDigetherSoilStageTimer(double FDigetherSoilTimings::* Field)
			: Target(Active ? &(Active->*Field) : nullptr), Start(Target ? FPlatformTime::Seconds() : 0)
		{
		}
		~FDigetherSoilStageTimer()
		{
			Stop();
		}
		void Stop()
		{
			if (Target)
			{
				*Target += (FPlatformTime::Seconds() - Start) * 1000.0;
				Target = nullptr;
			}
		}

	private:
		// 요청 시간 필드의 비소유 참조, Stop 후 nullptr로 중복 합산 방지
		double* Target;
		// 구간 시작 시각(초), 출력 때 ms로 변환
		double Start;
	};
	// 요청 전체 시간·결과 기록, 로그 출력 비용은 Total에서 제외
	class FDigetherSoilRequestTimer
	{
	public:
		FDigetherSoilRequestTimer() : Mode(Enabled.GetValueOnGameThread()), Previous(Active)
		{
			if (Mode)
			{
				Start = FPlatformTime::Seconds();
				Active = &Values;
			}
		}
		~FDigetherSoilRequestTimer()
		{
			if (!Mode)
			{
				return;
			}
			Values.Total = (FPlatformTime::Seconds() - Start) * 1000.0;
			Last = Values;
			Active = Previous;
			if (Mode >= 2)
			{
				UE_LOG(LogDigetherSoilPerformance, Display,
					   TEXT("ms Copy=%.6f Carve=%.6f Surface=%.6f Normals=%.6f Remove=%.6f MeshCollision=%.6f "
							"Commit=%.6f Total=%.6f Result=%d"),
					   Values.Copy, Values.Carve, Values.Surface, Values.Normals, Values.Remove, Values.MeshCollision,
					   Values.Commit, Values.Total, Values.Result);
			}
		}
		FDigetherSoilTimings Values;

	private:
		// 0 끔 / 1 마지막 기록 / 2 로그, Previous는 바깥 계측 복원용
		int32 Mode;
		FDigetherSoilTimings* Previous;
		double Start = 0;
	};
} // namespace DigetherSoilProfiling
#define DIGETHER_SOIL_TIMER(Name, Field)                                                                                   \
	DigetherSoilProfiling::FDigetherSoilStageTimer Name(&DigetherSoilProfiling::FDigetherSoilTimings::Field)
#define DIGETHER_SOIL_STOP(Name) Name.Stop()
#else
#define DIGETHER_SOIL_TIMER(Name, Field)
#define DIGETHER_SOIL_STOP(Name)
#endif
