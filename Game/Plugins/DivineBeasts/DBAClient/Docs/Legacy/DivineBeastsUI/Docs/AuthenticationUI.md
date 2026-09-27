# AuthenticationUI（认证界面）

Login（登录）页面只收集账号和瞬时Password（密码），通过 ViewModel → UI Command Port → ApplicationFlow Owner 提交。

当前UI状态包括：

- 是否已认证
- Busy（忙碌）
- Maintenance（维护）
- ErrorCode / 本地化ErrorText
- Retry（重试）
- Logout / Account Switch（登出/切换账号）

Password不进入 FDivineBeastsUIViewState，不作为UPROPERTY保存，也不写Telemetry（遥测）或日志。

Login按钮点击不直接打开下一个页面；ApplicationFlow返回新的权威View State后，FDivineBeastsUIRoutingPolicy（路由策略）才根据CurrentStep推荐角色/加载/错误页面。
