#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace GamePlatformPCGPolicy
{
// 初始开发预算，不是硬件性能达标值；入口拒绝超过范围的请求，不建立平行调度器。
struct NumericProfile
{
    std::array<double,3> HalfExtentCm = {500,500,50};
    double SpacingCm = 100;
    double Scale = 1;
    double Density = 1;
    double TimeoutSeconds = 30;
    std::uint32_t MinOutputs = 0;
    std::uint32_t MaxOutputs = 4096;
};
inline std::string Validate(const NumericProfile& Profile)
{
    for (const double Extent : Profile.HalfExtentCm)
    {
        if (!std::isfinite(Extent) || Extent <= 0) { return "InvalidBounds"; }
        if (Extent > 5000) { return "BoundsBudgetExceeded"; }
    }
    if (!std::isfinite(Profile.SpacingCm) || Profile.SpacingCm < 10 || Profile.SpacingCm > 10000) { return "InvalidSpacing"; }
    if (!std::isfinite(Profile.Scale) || Profile.Scale <= 0 || Profile.Scale > 10) { return "InvalidScale"; }
    if (!std::isfinite(Profile.Density) || Profile.Density < 0 || Profile.Density > 1) { return "InvalidDensity"; }
    if (!Profile.MaxOutputs || Profile.MaxOutputs > 4096 || Profile.MinOutputs > Profile.MaxOutputs) { return "InvalidOutputBudget"; }
    if (!std::isfinite(Profile.TimeoutSeconds) || Profile.TimeoutSeconds <= 0 || Profile.TimeoutSeconds > 120) { return "InvalidTimeout"; }
    const double GridPoints = std::ceil(2 * Profile.HalfExtentCm[0] / Profile.SpacingCm) *
        std::ceil(2 * Profile.HalfExtentCm[1] / Profile.SpacingCm);
    if (GridPoints > 4096) { return "PointBudgetExceeded"; }
    return {};
}

// FNV-1a32，UTF-8字节+little-endian长度/整数编码；仅种子派生，不作密码或内容真实性证明。
inline std::uint32_t StableSeed(const std::string& World,const std::string& Region,const std::string& Profile,
    std::uint32_t Revision,std::uint32_t Seed)
{
    std::uint32_t Hash = 2166136261u;
    const auto Byte = [&](std::uint8_t Value) { Hash = (Hash ^ Value) * 16777619u; };
    const auto Integer = [&](std::uint32_t Value)
    { for (int Shift = 0; Shift < 32; Shift += 8) { Byte(static_cast<std::uint8_t>((Value >> Shift) & 255u)); } };
    for (const auto* Text : {&World,&Region,&Profile})
    { Integer(static_cast<std::uint32_t>(Text->size())); for (unsigned char Value : *Text) { Byte(Value); } }
    Integer(Revision); Integer(Seed); return Hash;
}

enum class Phase { Idle, Loading, Generating, Retained, Cleaning, Cleaned };
enum class Outcome { Pending, Succeeded, Failed, Cancelled, TimedOut };
class Lifecycle
{
public:
    GamePlatformPCGPolicy::Phase Phase = Phase::Idle;
    GamePlatformPCGPolicy::Outcome Outcome = Outcome::Pending;
    bool Begin(std::uint64_t Execution)
    { if (Phase != Phase::Idle || !Execution) { return false; } Generation = Execution; Phase = Phase::Loading; return true; }
    bool Loaded(std::uint64_t Execution)
    { if (Execution != Generation || Phase != Phase::Loading) { return false; } Phase = Phase::Generating; return true; }
    bool Generated(std::uint64_t Execution,bool Validated)
    {
        if (Execution != Generation || Phase != Phase::Generating) { return false; }
        Outcome = Validated ? Outcome::Succeeded : Outcome::Failed;
        Phase = Validated ? Phase::Retained : Phase::Cleaning; return true;
    }
    void Stop(GamePlatformPCGPolicy::Outcome Reason)
    {
        if (Phase == Phase::Loading || Phase == Phase::Generating)
        { Outcome = Reason; Phase = Phase::Cleaning; }
    }
    bool BeginCleanup()
    {
        if (Phase == Phase::Cleaned || Phase == Phase::Cleaning || Phase == Phase::Idle) { return false; }
        if (Outcome == Outcome::Pending) { Outcome = Outcome::Cancelled; }
        Phase = Phase::Cleaning; return true;
    }
    bool Cleaned(std::uint64_t Execution,bool NativeDrained,bool ResourcesEmpty)
    {
        if (Execution != Generation || Phase != Phase::Cleaning || !NativeDrained || !ResourcesEmpty) { return false; }
        Phase = Phase::Cleaned; return true;
    }
private:
    std::uint64_t Generation = 0;
};
}
