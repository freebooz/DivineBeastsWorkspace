# 故障定位与安全恢复

| 现象 | 检查与恢复 |
|---|---|
| WorldSubsystem没创建 | 确认插件已启用、模块已构建，WorldType为Game/PIE且不是Commandlet |
| Editor/Preview执行运行逻辑 | 检查DoesSupportWorldType；不要改为全部支持以绕过测试 |
| Definition找不到 | 确认真实资产已生成、AssetManager为平台管理器、基础扫描包含Definitions |
| MapIdentityMismatch | 比对真实去PIE前缀包名与WorldDefinition软引用；不忽略或模糊匹配 |
| SessionPrerequisiteMissing | 当前Session公开服务与真实联调缺失，不能使用本地例外处理网络客户端 |
| Loading一直等待 | 分项读取WorldReadinessSnapshot，定位Data、Region、Streaming或贡献者；不得固定时间放行 |
| DuplicateRegionId | 撤销原Provider或修正项目内容重复，不按加载顺序覆盖 |
| Provider销毁 | 弱引用自动清理，观察者离开，必需区域就绪失效；重新注册需当前代次 |
| Streaming不完成 | 核查WP/传统路径、真实底层状态、Owner世界和deadline；不要卸载其他用户资源 |
| 旧World回调 | 校验弱世界、GUID及关闭状态；禁止把旧句柄改成新GUID重放 |
| 多PIE串状态 | 检查调用方UWorld/GetGameInstance来源，不使用GWorld或固定第零玩家 |
| 专服依赖UI/VFX | 检查运行模块依赖，不能为了编译关闭其他正常模块 |
| Cook后定义缺失 | 确认显式开发CustomConfig，基础扫描与CookRule，不将开发资产放进正式包 |
| 历史空描述阻断 | 保持三个文件原位，记录正式构建失败；未经用户决定不得移走/关闭/绕过 |

恢复只撤销本次拥有的句柄/进程，保留用户地图、DB和未提交修改。真实负例放隔离夹具，不删除正式资产。

