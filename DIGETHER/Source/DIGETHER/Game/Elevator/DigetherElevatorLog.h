#pragma once

#include "Logging/LogMacros.h"

// 탑승자 ID와 서버 매니페스트의 개별 하중은 로그에 출력하지 않는다.
DIGETHER_API DECLARE_LOG_CATEGORY_EXTERN(LogDigetherElevator, Log, All);
