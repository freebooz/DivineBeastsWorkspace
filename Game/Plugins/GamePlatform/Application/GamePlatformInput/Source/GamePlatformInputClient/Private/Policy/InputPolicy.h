#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

/**
 * GamePlatformInputPolicy（游戏平台输入策略内核）。
 * 该文件故意不依赖 UObject/EnhancedInput，保证最关键的所有权、抑制和轴值算法可以用原生 C++ 在 Debug/Release 中重复验证。
 */
namespace GamePlatformInputPolicy
{
/** 单个 LocalPlayer（本地玩家）最多持有的上下文租约数；用于防止错误热重载或 UI 代码造成无界增长。 */
inline constexpr std::size_t MaxContextLeases = 64;
/** 单个 LocalPlayer 最多持有的输入阻断租约数。 */
inline constexpr std::size_t MaxBlockLeases = 64;
/** 轴进入激活态的最小中性阈值；只用于动作生命周期门禁，不替代 Profile 的模拟摇杆死区。 */
inline constexpr double NeutralThreshold = 0.01;

/** 单个 Context（输入上下文）租约记录。 */
struct ContextLease
{
    std::string Context;
    int Priority = 0;
};

/**
 * ContextLedger（上下文租约账本）。
 * 只管理“谁拥有某个上下文、有效优先级是多少”，不直接操作 UE 的 MappingContext（映射上下文）。
 */
class ContextLedger
{
public:
    bool Acquire(std::uint64_t Token, const std::string& Context, int Priority, bool bExternalConflict)
    {
        if (Token == 0 || Context.empty() || bExternalConflict || Leases.size() >= MaxContextLeases || Leases.find(Token) != Leases.end())
        {
            return false;
        }
        Leases.emplace(Token, ContextLease{Context, Priority});
        return true;
    }

    bool Release(std::uint64_t Token)
    {
        return Token != 0 && Leases.erase(Token) == 1;
    }

    std::optional<int> EffectivePriority(const std::string& Context) const
    {
        std::optional<int> Result;
        for (const auto& [Token, Lease] : Leases)
        {
            (void)Token;
            if (Lease.Context != Context) { continue; }
            Result = Result ? std::max(*Result, Lease.Priority) : Lease.Priority;
        }
        return Result;
    }

    std::size_t Num() const { return Leases.size(); }

private:
    std::unordered_map<std::uint64_t, ContextLease> Leases;
};

/**
 * BlockLedger（输入阻断账本）。
 * 多来源通过位掩码叠加；释放一个来源不能清掉其他来源仍持有的阻断位。
 */
class BlockLedger
{
public:
    bool Acquire(std::uint64_t Token, std::uint32_t Channels)
    {
        if (Token == 0 || Channels == 0 || Leases.size() >= MaxBlockLeases || Leases.find(Token) != Leases.end())
        {
            return false;
        }
        Leases.emplace(Token, Channels);
        return true;
    }

    bool Release(std::uint64_t Token)
    {
        return Token != 0 && Leases.erase(Token) == 1;
    }

    bool IsBlocked(std::uint32_t Channels) const
    {
        if (Channels == 0) { return false; }
        for (const auto& [Token, Mask] : Leases)
        {
            (void)Token;
            if ((Mask & Channels) != 0) { return true; }
        }
        return false;
    }

    std::uint32_t CombinedMask() const
    {
        std::uint32_t Result = 0;
        for (const auto& [Token, Mask] : Leases)
        {
            (void)Token;
            Result |= Mask;
        }
        return Result;
    }

    std::size_t Num() const { return Leases.size(); }

private:
    std::unordered_map<std::uint64_t, std::uint32_t> Leases;
};

/**
 * ActionGate（动作生命周期门禁）。
 * 中断后必须观察到一次真实中性值才能重新武装，防止菜单关闭、失焦恢复或设备切换后“仍按住的键”立即重新触发。
 */
class ActionGate
{
public:
    bool Observe(double Magnitude, bool bTerminal, bool bBlocked)
    {
        if (!std::isfinite(Magnitude) || Magnitude < 0.0)
        {
            Interrupt();
            return false;
        }

        const bool bNeutral = Magnitude <= NeutralThreshold;
        if (bBlocked)
        {
            Interrupt();
            return false;
        }

        if (bDisarmed)
        {
            if (bNeutral)
            {
                bDisarmed = false;
                bActive = false;
            }
            return false;
        }

        if (bTerminal)
        {
            if (!bActive) { return false; }
            bActive = false;
            return true;
        }

        if (!bActive)
        {
            if (bNeutral) { return false; }
            bActive = true;
            return true;
        }

        return true;
    }

    /** 中断当前动作并进入待中性值重新武装状态；返回中断前是否处于Active。 */
    bool Interrupt()
    {
        const bool bWasActive = bActive;
        bActive = false;
        bDisarmed = true;
        return bWasActive;
    }

private:
    bool bActive = false;
    bool bDisarmed = false;
};

/** 判断二维轴样本是否均为有限值。 */
inline bool IsFiniteAxis(double X, double Y)
{
    return std::isfinite(X) && std::isfinite(Y);
}

/**
 * 将二维轴安全限制到单位圆。
 * 使用最大分量先缩放，避免 DBL_MAX 对角输入在 hypot 之前发生溢出。
 */
inline std::pair<double, double> ClampAxis(double X, double Y)
{
    if (!IsFiniteAxis(X, Y)) { return {0.0, 0.0}; }
    const double MaxAbs = std::max(std::abs(X), std::abs(Y));
    if (MaxAbs <= 1.0 && std::hypot(X, Y) <= 1.0) { return {X, Y}; }
    if (MaxAbs == 0.0) { return {0.0, 0.0}; }

    const double SX = X / MaxAbs;
    const double SY = Y / MaxAbs;
    const double Length = std::hypot(SX, SY);
    if (!std::isfinite(Length) || Length <= 0.0) { return {0.0, 0.0}; }
    return {SX / Length, SY / Length};
}

/**
 * 应用径向死区并重新映射到 0..1。
 * 该算法同时适用于 PC 手柄摇杆和移动端虚拟摇杆，不进行每帧分配。
 */
inline std::pair<double, double> ApplyRadialDeadZone(double X, double Y, double DeadZone)
{
    auto Axis = ClampAxis(X, Y);
    if (!std::isfinite(DeadZone) || DeadZone < 0.0 || DeadZone >= 1.0) { return {0.0, 0.0}; }

    const double Length = std::hypot(Axis.first, Axis.second);
    if (Length <= DeadZone || Length <= 0.0) { return {0.0, 0.0}; }

    const double Remapped = std::clamp((Length - DeadZone) / (1.0 - DeadZone), 0.0, 1.0);
    return {Axis.first / Length * Remapped, Axis.second / Length * Remapped};
}

/** 对二维轴应用有限倍率后重新限制到单位圆；用于移动端虚拟摇杆手感缩放，避免放大后越界。 */
inline std::pair<double, double> ApplyAxisScale(double X, double Y, double Scale)
{
    if (!IsFiniteAxis(X, Y) || !std::isfinite(Scale) || Scale <= 0.0) { return {0.0, 0.0}; }
    return ClampAxis(X * Scale, Y * Scale);
}

/** 合并通用与设备特定灵敏度；任一值非法时失败关闭为0。 */
inline double CombineSensitivity(double BaseSensitivity, double DeviceSensitivity)
{
    if (!std::isfinite(BaseSensitivity) || BaseSensitivity <= 0.0 ||
        !std::isfinite(DeviceSensitivity) || DeviceSensitivity <= 0.0)
    {
        return 0.0;
    }
    const double Combined = BaseSensitivity * DeviceSensitivity;
    return std::isfinite(Combined) ? Combined : 0.0;
}

/** 应用视角反转和灵敏度倍率；不乘 DeltaTime，避免与消费层重复缩放。 */
inline std::pair<double, double> ApplyLookPreference(
    double X,
    double Y,
    double Sensitivity,
    bool bInvertX,
    bool bInvertY)
{
    if (!IsFiniteAxis(X, Y) || !std::isfinite(Sensitivity) || Sensitivity <= 0.0)
    {
        return {0.0, 0.0};
    }
    return {
        X * Sensitivity * (bInvertX ? -1.0 : 1.0),
        Y * Sensitivity * (bInvertY ? -1.0 : 1.0)};
}

/**
 * 构造不含明文账号语义的稳定本地设置键。
 * 采用长度前缀避免字段分隔碰撞；返回值只用于本机配置区段，不作为网络或数据库身份。
 */
inline std::string StableSettingsKey(
    const std::string& Namespace,
    const std::string& OpaqueUser,
    int LocalPlayerIndex,
    const std::string& Profile)
{
    if (Namespace.empty() || OpaqueUser.empty() || Profile.empty() || LocalPlayerIndex < 0)
    {
        return {};
    }

    auto Field = [](const std::string& Value)
    {
        return std::to_string(Value.size()) + "#" + Value;
    };

    return Field(Namespace) + "|" + Field(OpaqueUser) + "|" +
        std::to_string(LocalPlayerIndex) + "|" + Field(Profile);
}
}
