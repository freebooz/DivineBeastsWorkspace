# 排障与安全边界

| 症状 | 检查与处理 |
| --- | --- |
| 一直Loading | 查TaskSnapshot哪些Waiting/Running；核对Dependencies和TaskTimeout；世界必须有当前操作的bootstrap声明，不能强行Complete |
| 100%不Ready | 检查任务真实状态，进度仅显示；Pending报告1的测试专门证明这一点 |
| Ready不足100% | Optional失败保留原权重与最后进度，是明确策略而非聚合出错 |
| Data失败 | 查看任务Error、主资产身份、ExpectedClass、真实生成物和扫描配置；不要用路径旁路或占位包修复 |
| SessionPrerequisiteMissing | Session没有真实公开适配；先完成Online/Session链，禁止在Loading发HTTP补票据 |
| 地图开了但World不匹配 | 查当前实例World、Game/PIE类型、BeginPlay/teardown、去PIE前缀包名和操作句柄；项目还要确认真实控制器/Pawn |
| 取消后收到旧事件 | 接收方应核对Loading完整句柄和Flow NodeToken；正常旧取消通知不会控制新操作，不用日志字串判断 |
| 多PIE串状态 | 不缓存全局服务，不使用GWorld；服务从显式实例获取；不同OwnerScopeId的句柄必须拒绝 |
| DependencyCycle | 修规格DAG，不能在运行时拆环或忽略依赖；重复依赖也应明确修正 |
| Busy | Ready操作仍持有租约；先Release，再开始新操作或撤销工厂，不能默默覆盖 |
| Cook后资产缺失 | 对照当前CustomConfig、主资产扫描和真正Cook清单；编辑器资产存在不是已烘焙证据 |
| Server拉入客户端模块 | 检查真实Build.cs/uplugin和最终依赖；Loading不应导入Session客户端、UMG、Niagara |
| UBT在插件描述处失败 | 本轮历史DivineBeastsPresentation.uplugin仅空白内容，未进入本插件编译；交由其所有者修复，不能擅自删除/禁用 |

调试只记录操作/任务身份和脱敏错误码，不把凭据、身份令牌或服务器控制秘密放进快照。失败证据保留旧目录，新一次验证使用新RunId；不要搜索历史Success作为本次证明。
