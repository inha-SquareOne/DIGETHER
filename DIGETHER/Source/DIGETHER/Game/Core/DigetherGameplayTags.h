#pragma once

#include "NativeGameplayTags.h"

// C++에서 쓰는 태그는 여기서 네이티브로 선언해 오타를 컴파일 단계에서 잡는다.
// 최상위 분류는 docs/design/DEVELOPMENT_SYSTEM_DESIGN.md "정의 데이터와 태그"의 다섯 개만 쓴다.
// 실제로 코드가 읽는 태그만 선언한다. 나머지 분류는 사용하는 단계에서 추가한다.

// 아이템 종류. 정의 자산마다 하나를 고른다.
DIGETHER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Type_Cargo);
DIGETHER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Type_Equipment);

// 아이템 특성. 정의 자산에 여러 개를 붙일 수 있다.
DIGETHER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Trait_TwoHanded);
DIGETHER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Trait_EmergencyDevice);
