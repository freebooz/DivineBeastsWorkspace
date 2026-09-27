# ProjectContext（项目上下文）

`FDivineBeastsPresentationProjectContext（神兽联盟项目表现上下文）`只保存稳定逻辑身份和表现提示。它不是网络权威对象，也不携带资源路径、Token（令牌）、TransferTicket（迁移票据）或 Backend DTO（后端数据传输对象）。

| Field（字段） | Source（来源） | Trust（可信度） | Nullable（可空） | Client visible（客户端可见） | Server visible（服务器可见） | Presentation-only（仅表现） |
| --- | --- | --- | --- | --- | --- | --- |
| `ProjectId（项目编号）` | `DivineBeastsRuntime（项目核心运行时）` | 高；固定项目身份 | 否；提交前必须归一到正式ProjectId | 是 | 是；Runtime类型可编译 | 是 |
| `HeroDefinitionId（英雄定义编号）` | 已验证角色/流程事实 | 中高；依赖事实Owner | 是 | 是 | 可见；不作为表现层权威判定 | 是 |
| `AbilityId（技能编号）` | GAS/Ability事实或组合层 | 中高；依赖Ability事实Owner | 是 | 是 | 可见；Presentation不反向驱动Ability | 是 |
| `SkinId（皮肤编号）` | 已安装、已注册且业务已验证的Skin/Content Pack | 中；项目表现只消费业务验证结果 | 是 | 是 | 可见但Server不加载皮肤资源 | 是 |
| `WorldId（世界编号）` | ApplicationFlow Assignment / World事实 | 高；来自已验证世界分配投影 | 是 | 是 | 是 | 是 |
| `ExperienceId（体验编号）` | ApplicationFlow Assignment | 高；来自正式Experience | 是 | 是 | 是 | 是 |
| `RegionId（区域编号）` | World/Assignment/Region事实 | 中高；由World事实Owner提供 | 是 | 是 | 是 | 是 |
| `ArenaModeId（竞技模式编号）` | Arena权威流程组合层 | 高；由Arena事实Owner提供 | 是 | 是 | 是 | 是 |
| `ContentPackId（内容包编号）` | 已激活并注册的Content Pack | 中高；注册句柄限定生命周期 | 是 | 是 | 可见；Server不加载纯客户端资源 | 是 |
| `PlatformId（平台编号）` | 客户端平台/设备Profile | 低权威；仅质量/适配提示 | 是 | 是 | 可见但不得影响Gameplay权威 | 是 |
| `WorldGeneration（世界代次）` | Session/World生命周期 | 高；用于拒绝旧World请求 | 是；0表示未知/不贡献 | 是 | 是 | 是 |
| `AvatarGeneration（化身代次）` | Character/Avatar生命周期 | 高；用于拒绝旧Avatar表现 | 是；0表示未知/不贡献 | 是 | 是 | 是 |
| `LocalPlayerRelation（本地玩家关系）` | LocalPlayer视角/关系投影 | 客户端派生；非Gameplay权威 | 是；Unknown表示不贡献 | 是 | 不要求服务器使用 | 是 |
| `QualityTier（质量档位）` | Device/Profile/Quality设置 | 客户端提示 | 是；Unknown表示不贡献 | 是 | 不要求服务器使用 | 是 |

规则：

- `ProjectId` 必须匹配 `DivineBeastsRuntime（神兽联盟项目核心）` 正式身份。
- `WorldGeneration / AvatarGeneration` 不允许负值；0表示当前没有代次贡献。
- `HeroDefinitionId / AbilityId / SkinId` 只用于目录匹配，不能成为Gameplay授权或资格依据。
- `SkinId / ContentPackId` 只能来自已安装、已注册且业务上已经验证的内容包；Presentation不自行判断购买/权益。
- `QualityTier / PlatformId / LocalPlayerRelation` 只能影响表现解析和Provider行为，绝不能改变伤害、命中、任务、经济或竞技结果。
- 所有可空字段使用 `NAME_None / 0 / Unknown` 表示“当前无贡献”，而不是伪造默认业务值。
- 禁止任意 `UObject`、`AssetPath（资产路径）`、Token、Ticket或Backend DTO进入Project Context。
