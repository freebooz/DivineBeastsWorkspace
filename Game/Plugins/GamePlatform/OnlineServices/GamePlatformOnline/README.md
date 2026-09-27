# GamePlatformOnline（游戏平台在线插件）

本插件包含客户端认证Provider生命周期，以及与传输无关的生产请求／认证状态机和公开在线契约。模块和描述依赖已归入规范插件目录；迁入不等于端到端在线能力完成。

## 当前实现边界

- `GamePlatformOnlineClient` 的认证Provider切换会使旧异步回调失效；空凭据失败关闭；未认证状态拒绝刷新；子系统销毁时清理Provider与状态。
- `GamePlatformOnline` 公开了在线门面声明，私有 `OnlineSession` 实现了纯逻辑请求与认证状态机；当前门面尚无函数定义，状态机也尚未接入该门面、UE HTTP、JSON解析或UObject子系统。
- 原生状态机测试和UE契约测试已随源码归入本插件。原生测试只能证明纯逻辑行为，不替代UE/UHT编译、自动化、真实HTTP或后端联调。
- 服务端模块目前只有模块注册入口，没有独立服务端职责实现；不得将其视为已完成的在线服务器功能。

迁移来源、历史测试范围、安全断点及后续风险见[在线实现与安全交接](Docs/LegacyImplementationHandover.md)；文件级目录见[目录规划说明](Docs/目录规划说明.md)。
