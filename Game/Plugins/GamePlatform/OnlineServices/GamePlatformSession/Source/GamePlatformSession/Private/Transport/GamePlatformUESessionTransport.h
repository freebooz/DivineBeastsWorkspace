#pragma once

#include "GamePlatformSessionClientSubsystem.h"

class UGameInstance;

/** 创建默认UE会话Transport；仅Client/Editor目标编译。 */
TSharedPtr<IGamePlatformSessionTransport>
CreateGamePlatformUESessionTransport(UGameInstance& GameInstance);
