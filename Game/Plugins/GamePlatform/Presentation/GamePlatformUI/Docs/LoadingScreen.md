# LoadingScreen（加载界面服务）

UGamePlatformLoadingScreenService（游戏平台加载界面服务）维护多个 FGamePlatformLoadingToken（加载令牌）。Acquire/Update/Release（获取/更新/释放）改变聚合快照，只有全部Token释放后才结束Loading。

Snapshot（快照）包含 ActiveTokenCount、Stage（阶段）和Progress（进度）。当前进度规则：

- Progress < 0统一归一为 -1，表示权威进度未知。
- 0～1保留。
- Progress > 1夹紧到1。
- UI不得自行从0动画到100并冒充真实事务进度。

多个Token同时存在时，快照采用最近一次Acquire/Update的Token作为当前Stage/Progress展示来源；ActiveTokenCount仍反映全部并发Loading事务。

该服务属于World Runtime UMG（世界运行期界面），不等于MoviePlayer/启动阶段同步Loading。

具体Loading Widget仍需Unreal Editor（虚幻编辑器）资产并通过Loading Layer接入，当前没有伪造任何 .uasset。

