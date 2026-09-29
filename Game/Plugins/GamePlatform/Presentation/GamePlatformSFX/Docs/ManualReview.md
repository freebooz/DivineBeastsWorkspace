# GamePlatformSFX 人工审核说明

版本：0.2.0｜2026-09-29

## 1. 核心审核问题

架构审核：确认仍为 GamePlatform 平台层、ClientOnly，不出现 DivineBeasts/MobaCommon 反向依赖，不把SFX变成第二个Presentation Resolver。

资源审核：确认具体Sound/MetaSound/Attenuation/Concurrency均为软引用，Gameplay不直接PlaySound、不硬引用项目音频资产。

生命周期审核：确认Attached Owner销毁、World切换、异步Definition晚到、Stop淡出和AudioFinished后均能释放租约与组件。

性能审核：确认没有Tick、目录扫描、LoadSynchronous或无界实例容器；项目内容必须配置合理SoundConcurrency。

权威审核：任何SFX失败都不能改变Gameplay事实。

## 2. 组件清单

- `UGamePlatformSFXDefinition（游戏平台音效定义）`：Definition资产合同，保存软声音、衰减、并发、播放空间、淡入淡出和参数白名单。
- `FGamePlatformSFXRequest（音效请求）`：逻辑DefinitionId、位置、附着弱引用、倍率和参数。
- `FGamePlatformSFXHandle（实例句柄）`：Id + Generation + Weak World，控制一次播放事务。
- `FGamePlatformSFXResult（播放受理结果）`：区分Queued/Played/拒绝原因。
- `IGamePlatformSFXService（音效服务）`：对外播放、停止、参数和诊断入口。
- `UGamePlatformSFXWorldSubsystem（世界级执行器）`：Definition租约、AudioComponent、实例生命周期和请求去重。
- `FGamePlatformSFXPolicy（纯值策略）`：请求校验和规范逻辑ID→主资产ID转换。
- `UGamePlatformSFXPresentationBridgeSubsystem（SFX表现桥）`：注册ProviderChannel=SFX并转换中立Presentation Request。
- `GamePlatformSFXPolicyTests（SFX策略测试）`：纯值自动化测试。
- `TestGamePlatformSFXArchitecture.ps1（SFX架构门禁）`：独立静态规则检查。

## 3. 内容开发审核

每个音效Definition至少核对：

- LogicalId稳定且合法；
- Sound不是硬引用；
- PlaybackSpace符合用途；
- 3D音效有合理Attenuation；
- 高频战斗音效有明确Concurrency；
- MetaSound请求参数全部在白名单；
- FadeOut不会导致长时间不可清理的循环音；
- UI提示不会被当服务器权威事件；
- 内容包卸载/切图后没有活动实例泄漏。

## 4. 发布前证据

只有同时取得Editor/Client编译、Server隔离、UE Automation、真实音频资产播放和目标平台性能数据后，才能称为Production Ready（生产就绪）。
