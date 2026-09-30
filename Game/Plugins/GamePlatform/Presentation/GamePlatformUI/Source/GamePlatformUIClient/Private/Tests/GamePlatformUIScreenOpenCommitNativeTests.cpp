// F12同步重入：真实生产提交策略必须拒绝取消/旧代次/布局替换/退出/撤销租约/移除控件。
// 测试替身只描述CommonUI回调后的状态，不能替代真实UE激活、Slate和Data验收。
#if defined(GAMEPLATFORM_UI_OPEN_COMMIT_NATIVE_TEST)
#include "../Manager/GamePlatformUIScreenOpenCommitPolicy.h"
#include <cstdio>

int main()
{
    FGamePlatformUIScreenOpenCommitState Valid{true, true, true, true, true, true};
    int Failures = 0;
    for (int Case = 0; Case < 6; ++Case)
    {
        auto State = Valid;
        switch (Case)
        {
        case 0: State.bRequestCurrent = false; break;
        case 1: State.bLayoutCurrent = false; break;
        case 2: State.bScopeCurrent = false; break;
        case 3: State.bLeaseSucceeded = false; break;
        case 4: State.bScreenInStack = false; break;
        case 5: State.bScreenValid = false; break;
        }
        int Opened = 0, ActiveLeases = 0, WidgetCount = 1, ReleaseCount = 0;
        const bool Accepted = State.Complete(
            [&] { ++ActiveLeases; ++Opened; },
            [&] { --WidgetCount; ++ReleaseCount; });
        if (Accepted || Opened != 0 || ActiveLeases != 0 || WidgetCount != 0 || ReleaseCount != 1)
        {
            std::printf("回调后失效case=%d却提交控件或资源\n", Case);
            ++Failures;
        }
    }
    int Commits = 0, Rollbacks = 0;
    if (!Valid.Complete([&] { ++Commits; }, [&] { ++Rollbacks; }) || Commits != 1 || Rollbacks != 0)
        ++Failures;
    return Failures == 0 ? 0 : 1;
}
#endif
