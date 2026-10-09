#pragma once

/**
 * 平台客户端开屏事务的提交条件；不拥有UObject、资源或网络权威。
 * UIManager在CommonUI同步回调返回后填入真实状态，Native测试复用此生产策略。
 */
struct FGamePlatformUIScreenOpenCommitState
{
    bool bScreenValid = false; // 新控件仍可访问。
    bool bScopeCurrent = false; // LocalPlayer/GI/World未关闭或切换。
    bool bRequestCurrent = false; // 同请求代次尚未取消。
    bool bLayoutCurrent = false; // 同布局代次、Root及目标Stack仍生效。
    bool bLeaseSucceeded = false; // 本请求租约尚未被Data撤销。
    bool bScreenInStack = false; // 同步事件未把新控件移出原容器。

    bool CanCommit() const
    {
        return bScreenValid && bScopeCurrent && bRequestCurrent &&
            bLayoutCurrent && bLeaseSucceeded && bScreenInStack;
    }

    /** 拒绝时先撤回控件再释放构造租约；只有成功提交才允许发布Opened。 */
    template <typename Commit, typename Rollback>
    bool Complete(Commit&& CommitOwnership, Rollback&& RollbackWidget) const
    {
        if (!CanCommit())
        {
            RollbackWidget();
            return false;
        }
        CommitOwnership();
        return true;
    }
};
