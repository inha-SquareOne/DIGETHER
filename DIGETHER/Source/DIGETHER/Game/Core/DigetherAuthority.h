#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

namespace DigetherAuthority
{
	// 컴포넌트의 상태 변경 권한 판정. 소유 Actor가 유효하고 그 Actor가 권한을 가질 때만 참이다.
	inline bool HasOwnerAuthority(const UActorComponent& Component)
	{
		const AActor* OwnerActor = Component.GetOwner();
		return IsValid(OwnerActor) && OwnerActor->HasAuthority();
	}
}
