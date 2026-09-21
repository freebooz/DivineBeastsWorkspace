#include "../DBAPCGLoadingLifetime.h"

namespace
{
using DBA::PCG::FLoadingLifetime;

// 编译期回归直接使用生产状态：成功后Loading释放不能清除世界持有的输出。
constexpr bool SuccessfulTaskRetainsOutput()
{
    FLoadingLifetime Lifetime;
    Lifetime.AcceptRequest();
    Lifetime.ObserveSuccess();
    return !Lifetime.ReleaseTask() && !Lifetime.ReleaseTask() && Lifetime.IsReleased();
}

// 尚未被任务观察为成功的请求，即使原生生成刚刚成功，也必须走取消/释放路径。
constexpr bool UnobservedRequestIsReleasedOnce()
{
    FLoadingLifetime Lifetime;
    Lifetime.AcceptRequest();
    return Lifetime.ReleaseTask() && !Lifetime.ReleaseTask();
}

// Start同步失败不得释放组件中另一个任务的结果；释放后的迟到成功不能复活任务。
constexpr bool FailedStartAndLateSuccessAreSafe()
{
    FLoadingLifetime Lifetime;
    const bool bShouldRelease = Lifetime.ReleaseTask();
    Lifetime.AcceptRequest();
    Lifetime.ObserveSuccess();
    return !bShouldRelease && Lifetime.IsReleased() && !Lifetime.HasSucceeded() && !Lifetime.ReleaseTask();
}

static_assert(SuccessfulTaskRetainsOutput(), "Loading success must retain world output");
static_assert(UnobservedRequestIsReleasedOnce(), "Pending or failed attempt must release exactly once");
static_assert(FailedStartAndLateSuccessAreSafe(), "Failed Start and late callbacks must not claim output");
}
