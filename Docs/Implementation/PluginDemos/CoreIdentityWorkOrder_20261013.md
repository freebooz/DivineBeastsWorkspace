# SSLM核心逻辑身份演示工作单

范围：R04中GamePlatformCore的逻辑身份能力，不代表该插件全部功能已审计。已读实际Public/Types/GamePlatformId.h、Private/Types/GamePlatformId.cpp及README；Result、ErrorCode、Version、VersionRange等仍需独立工作单。

唯一公开调用链为`FGamePlatformId::TryCreate(Namespace, Name, LogicalVersion, OutId)` → `ToString()` → `TryParse(Text, OutId)` → 相等及容器哈希比较。调用方为项目层开发适配器，平台层不引用本工作单的场景/资源，也不新增身份解析器。

| 独立身份 | 输入与实际期望 | 证据 |
| --- | --- | --- |
| GamePlatformCore.Identity.Normal | Sample.Catalog、Item、1；创建/规范化/回环与相等哈希真实一致 | state_dump、log；菜单显示规范文本 |
| GamePlatformCore.Identity.InvalidName | Name为Invalid.Name；平台单段名称拒绝，原始操作Failed，不改写成成功 | state_dump、log |
| GamePlatformCore.Identity.InvalidVersion | LogicalVersion为0；平台真实1..INT32_MAX边界拒绝，不截断为1 | state_dump、log |

所属功能为GamePlatformCore.Identity，适配身份CoreIdentity；使用真实总览地图而不创建空关卡。类型化参数按Namespace/Name/LogicalVersion顺序消费，身份参数由菜单通用校验约束，领域更严格的单段名称/分段长度仍由平台公开API拒绝。版本参数0..2147483647仅是包含非法边界的开发夹具范围，不是生产合法范围或性能预算。

文件责任：项目Demo模块Private/Scenarios/DivineBeastsDemoCoreIdentityScenario.h/.cpp（真实能力适配），Private/Tests/DivineBeastsDemoMenuTests.cpp（纯值回归），既有Catalog注册及JSON作者源/派生表。GamePlatformCore源码与稳定接口不改名、不复制、不访问私有算法头。

Prepare核对有效Token及本世界，Execute只操作自包含值，没有账号、余额、持久状态或资源租约。Stop使本次适配失活，World失效/停止后继续Execute拒绝；没有权威动作或待取消异步请求，不适用网络取消/玩家复位，此边界须在最终人工核验确认。超时使用既有准备/清理机制，不建立Tick。

原生测试先写：规范回环成功、单段名称拒绝、0版本拒绝、非法类型/参数数量、失败不返回旧规范文本。由于用户禁止构建，原生红绿均未运行，不将离线编目通过称作能力运行通过。新目录/DataAsset/HubScreen仍等待匹配完整Editor及Monolith资产，不标launchable。人工按用户要求放在全范围实施之后。
