#pragma once
#include <array>
#include <cmath>
#include <string>
#include <string_view>

// 世界适配器与原生测试共用的纯决策函数；不保存全局状态或伪造引擎事实。
namespace GamePlatformWorldPolicy
{
inline bool SupportsWorld(bool Game, bool PIE, bool Commandlet) { return !Commandlet && (Game || PIE); }
inline bool AcceptsGeneration(std::string_view Expected, std::string_view Actual, bool WorldAlive, bool TearingDown)
{ return !Expected.empty() && Expected == Actual && WorldAlive && !TearingDown; }
inline bool AllowLocalDevelopment(bool Explicit, bool Shipping, bool NetworkClient, bool ListenServer)
{ return Explicit && !Shipping && !NetworkClient && !ListenServer; }
inline bool AllowsUpdates(bool Closing,bool Dispatching,bool Failed,bool WorldValid)
{return !Closing&&!Dispatching&&!Failed&&WorldValid;}
/** 已Ready新增义务获得一次新的预算；连续Pending不滑动续期。单位单调秒。 */
inline double NextDeadline(bool WasReady,bool Ready,double Now,double Budget,double Deadline)
{return WasReady&&!Ready?Now+Budget:Deadline;}
/** 顺序：世界、定义、地图、Session、区域、流送、未销毁、扩展贡献者。 */
struct FReadinessFacts
{
    std::array<bool,8> Facts{};
    FReadinessFacts() = default;
    FReadinessFacts(bool A,bool B,bool C,bool D,bool E,bool F,bool G,bool H):Facts{A,B,C,D,E,F,G,H}{}
    auto& Values(){return Facts;}
};
inline bool IsReady(const FReadinessFacts& Facts)
{ for(bool Fact:Facts.Facts){if(!Fact)return false;}return true; }
/** 坐标单位由UE适配器约定为厘米，边界采用闭区间。 */
struct FPoint { double X,Y,Z; };
struct FBox { FPoint Min,Max; };
inline bool ValidPoint(const FPoint& P){return std::isfinite(P.X)&&std::isfinite(P.Y)&&std::isfinite(P.Z);}
inline bool ValidBox(const FBox& B)
{return ValidPoint(B.Min)&&ValidPoint(B.Max)&&B.Min.X<B.Max.X&&B.Min.Y<B.Max.Y&&B.Min.Z<B.Max.Z;}
inline bool Contains(const FBox& B,const FPoint& P)
{return ValidBox(B)&&ValidPoint(P)&&P.X>=B.Min.X&&P.Y>=B.Min.Y&&P.Z>=B.Min.Z&&P.X<=B.Max.X&&P.Y<=B.Max.Y&&P.Z<=B.Max.Z;}
inline double Volume(const FBox& B){return (B.Max.X-B.Min.X)*(B.Max.Y-B.Min.Y)*(B.Max.Z-B.Min.Z);}
/** 正数左优，负数右优，0歧义；禁止通过ID或注册顺序打破同优先级同具体度冲突。 */
inline int CompareCandidate(int LeftPriority,const FBox& Left,int RightPriority,const FBox& Right)
{
    if(LeftPriority!=RightPriority)return LeftPriority>RightPriority?1:-1;
    const double L=Volume(Left),R=Volume(Right);
    return L==R?0:(L<R?1:-1);
}
/** 非敏感Session目标比较算法；不证明输入来自真实Session或网络准入。 */
struct FSessionTarget { std::string World,Map,Instance,Epoch,Build; };
inline bool MatchesTarget(const FSessionTarget& A,const FSessionTarget& B)
{return !A.World.empty()&&!A.Map.empty()&&!A.Instance.empty()&&!A.Epoch.empty()&&!A.Build.empty()&&
 A.World==B.World&&A.Map==B.Map&&A.Instance==B.Instance&&A.Epoch==B.Epoch&&A.Build==B.Build;}
}
