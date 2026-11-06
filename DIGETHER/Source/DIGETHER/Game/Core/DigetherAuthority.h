#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

namespace DigetherAuthority
{
	// 컴포넌트 상태 변경 권한 판정, 소유 Actor가 유효하고 권한이 있을 때만 참
	inline bool HasOwnerAuthority(const UActorComponent& Component)
	{
		const AActor* OwnerActor = Component.GetOwner();
		return IsValid(OwnerActor) && OwnerActor->HasAuthority();
	}
}
