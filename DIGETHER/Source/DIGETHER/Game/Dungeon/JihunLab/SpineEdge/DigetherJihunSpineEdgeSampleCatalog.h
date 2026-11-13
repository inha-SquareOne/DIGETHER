#pragma once

#include "CoreMinimal.h"
#include "DigetherJihunSpineEdgeTypes.h"

namespace DigetherJihunSpineEdge
{
	// I_School_Map_GleyBox MCP 실측값(2026-11-11)으로 만든 기획 모듈 정의
	// 테스트와 Data Asset 입력 기준용, 기획 모듈 개정 시 함께 갱신
	DIGETHER_API TArray<FDigetherJihunSpineEdgeModuleSpec> MakeGleyBoxCatalog();
}
