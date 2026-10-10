// 项目共享GameMode只指定项目控制器；认证和生成仍由平台唯一门禁拥有。
#include "Gameplay/DivineBeastsWorldGameMode.h"
#include "Gameplay/DivineBeastsWorldPlayerController.h"
ADivineBeastsWorldGameMode::ADivineBeastsWorldGameMode()
{
    PlayerControllerClass=ADivineBeastsWorldPlayerController::StaticClass();
}
