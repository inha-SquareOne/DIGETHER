#include "Game/Monster/Spider/DigetherGroundSpiderRules.h"

#define LOCTEXT_NAMESPACE "DigetherGroundSpider"

bool FDigetherGroundSpiderParams::CollectProblems(TArray<FText>& OutProblems) const
{
	const int32 Before = OutProblems.Num();
	if (TrackRadius <= 0.f || CorpseRadius < TrackRadius)
	{
		OutProblems.Add(LOCTEXT("SpiderAreas", "땅거미 추적 영역은 양수이고 시체 회수 영역은 추적 영역 이상이어야 합니다(기획)."));
	}
	if (LeapSpeed <= 0.f || LeapMaxDistance < 0.f || LeapWindupSeconds < 0.f || LeapLandingSeconds < 0.f)
	{
		OutProblems.Add(LOCTEXT("SpiderLeap", "도약 속도는 양수, 거리·시간은 0 이상이어야 합니다."));
	}
	if (ContactIntervalSeconds < 0.1f || LeapDamage < 0.f || ContactDamage < 0.f)
	{
		OutProblems.Add(LOCTEXT("SpiderDamage", "피해는 0 이상, 접촉 간격은 0.1초 이상이어야 합니다."));
	}
	if (WebBaseCount > 0 && WebClass.IsNull())
	{
		OutProblems.Add(LOCTEXT("SpiderWebClass", "거미줄 기준량이 있으면 WebClass를 지정해야 합니다."));
	}
	return OutProblems.Num() == Before;
}

#undef LOCTEXT_NAMESPACE

namespace DigetherGroundSpiderRules
{
	int32 SelectNearestReachable(const TConstArrayView<FTargetCandidate> Candidates)
	{
		int32 Best = INDEX_NONE;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FTargetCandidate& Candidate = Candidates[Index];
			if (!Candidate.bReachable || Candidate.PlayerKey.IsEmpty())
			{
				continue;
			}
			if (Best == INDEX_NONE || Candidate.DistanceSquared < Candidates[Best].DistanceSquared)
			{
				Best = Index;
			}
		}
		return Best;
	}

	EDigetherSpiderGiveUpReason EvaluateGiveUp(const FTrackingInput& Input)
	{
		if (!Input.bTargetValid)
		{
			return EDigetherSpiderGiveUpReason::TargetLost;
		}
		if (!Input.bReachable)
		{
			return EDigetherSpiderGiveUpReason::Unreachable;
		}
		if (!Input.bChaseOutsideArea && Input.TargetDistanceFromHome > Input.TrackRadius)
		{
			return EDigetherSpiderGiveUpReason::OutOfArea;
		}
		if (Input.SecondsSinceSeen > Input.ReacquireSeconds)
		{
			return EDigetherSpiderGiveUpReason::NotReacquired;
		}
		return EDigetherSpiderGiveUpReason::None;
	}

	FVector ComputeLeapDestination(const FVector& Start, const FVector& Target, const float MaxDistance)
	{
		const FVector Delta = Target - Start;
		const float Distance = Delta.Size();
		if (Distance <= MaxDistance || Distance <= KINDA_SMALL_NUMBER)
		{
			return Target;
		}
		return Start + Delta / Distance * FMath::Max(0.f, MaxDistance);
	}

	float ComputeLeapFlightSeconds(const FVector& Start, const FVector& Destination, const float Speed)
	{
		return Speed > 0.f ? FVector::Dist(Start, Destination) / Speed : 0.f;
	}

	bool ShouldApplyContactDamage(const FContactInput& Input)
	{
		if (Input.bLeapDamagedThisFrame || Input.bTargetBuried || Input.bSpiderBuried)
		{
			return false;
		}
		return Input.LastContactDamageTime < 0.0 || Input.Now - Input.LastContactDamageTime >= Input.IntervalSeconds - KINDA_SMALL_NUMBER;
	}
}

namespace DigetherGroundSpiderRules
{
	bool IsTrackableCandidate(const float DistanceFromHome, const float TrackRadius, const bool bAttackedFromOutside)
	{
		return bAttackedFromOutside || DistanceFromHome <= TrackRadius;
	}
}
