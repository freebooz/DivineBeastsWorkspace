# SecurityAndRobustness（安全与健壮性）

关键规则：

- Password只瞬时传递，不保存在View State/UPROPERTY。
- Token、TransferTicket、后端DTO不进入UI Contract。
- 命令携带ExpectedRevision，旧View拒绝提交。
- ViewModel使用PageGeneration/Revision拒绝旧异步回调。
- 页面关闭解绑Delegate并尝试取消Pending命令。
- UI只提交Intent，不直接设置Health/Ability/Quest/Score/Winner/Team/TutorialComplete。
- 业务Owner不提供端口时Fail Closed，而不是伪造成功。
- 错误展示用本地化通用文案，避免泄露内部技术信息。
- Server Target显式禁用UI插件。

当前统一Command Cancel端口在业务Owner层尚不完整；物理取消不足时仍由Generation/Revision保证旧结果不会污染新页面。
