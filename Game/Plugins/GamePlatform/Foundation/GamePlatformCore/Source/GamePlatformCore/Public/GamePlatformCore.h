#pragma once

#include "Logging/LogMacros.h"

/** 平台核心诊断分类，可供宿主消费；调用者不得记录凭据或未脱敏的用户输入。 */
GAMEPLATFORMCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogGamePlatformCore, Log, All);
