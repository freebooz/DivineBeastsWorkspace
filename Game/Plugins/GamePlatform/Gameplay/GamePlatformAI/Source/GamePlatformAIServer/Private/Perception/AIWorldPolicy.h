#pragma once
// AI服务器世界运行资格投影；无UObject访问，编辑器与Commandlet事实由实际入口提供。
namespace GamePlatformAIWorldPolicy
{
inline bool CanRun(bool GameOrPIE, bool Authority, bool Commandlet)
{ return GameOrPIE && Authority && !Commandlet; }
}
