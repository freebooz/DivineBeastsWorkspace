# LifecycleAndPooling（生命周期与池化）

服务器 Pawn 被 Possess 后：取得 AIState/ASC/Combat → 记录 Home → 绑定 Combat/Control → 异步加载 AIDefinition → 配置 Perception → 异步加载 BT/BB → UseBlackboard/RunBehaviorTree → 启动 Decision Timer。每个阶段失败会进入 Disabled 并停止 Move/Brain。

UnPossess/EndPlay：停止 Brain/Timer/Move，取消 Definition/Brain Asset Handle，解绑 Combat/GameplayTag Delegate，ForgetAll感知，解绑候选销毁事件，清 Target/Candidate，并推进 AIInstanceGeneration。

Death：清 Target/Candidate/感知记忆，Stop Brain/Move，公开 Dead；RespawnReset：清旧 Blackboard/Perception/Candidate，更新 Generation，Restart Brain，重新进入决策。

Actor Pool（对象池）本轮未实现；但 Generation 与清理边界已为未来复用准备，旧异步资源回调必须匹配当前 Generation。
