#pragma once

#include "NativeGameplayTags.h"

// C++에서 쓰는 태그만 네이티브 선언, 오타를 컴파일 단계에서 차단
// 최상위 분류는 docs/design/DEVELOPMENT_SYSTEM_DESIGN.md "정의 데이터와 태그"의 다섯 개만 사용
// 나머지 분류는 코드가 읽는 단계에서 추가

// 아이템 종류, 정의 자산마다 하나
DIGETHER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Type_Cargo);
DIGETHER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Type_Equipment);

// 아이템 특성, 정의 자산에 여러 개 가능
DIGETHER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Trait_TwoHanded);
DIGETHER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Trait_EmergencyDevice);
