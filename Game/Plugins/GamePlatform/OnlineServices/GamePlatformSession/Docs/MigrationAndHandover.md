# D10｜兼容、迁移、回退与职责交接

GamePlatformSession现已从“只有内部状态内核”演进为公开客户端Subsystem + 私有纯状态内核的结构，仍保持版本 `0.1.0-incomplete` 与默认禁用以反映真实Transport尚未完成。Shared新增Gateway `world-entry` OpenAPI路由，并通过contractcodegen生成；旧servertransfer票据字段未被客户端直接暴露为内部控制面API。

000003_session_admission.sql新增四表、五个事务函数和容量索引，属于服务器控制面内部持久化结构。执行前要求独占迁移窗口、已审阅备份及指定库；本轮只在本次临时测试库执行，没有向既有开发/生产数据库应用。SQL不是每次服务启动自动重放的脚本。session_authorizations保存经外部核验的短期决策，不能替代Identity/PlayerData所有权。

升级顺序：补齐真实身份与玩家决策源、服务身份和握手关联 → 明确共享契约 → 审查数据库角色权限/迁移 → 部署控制面适配 → 部署服务器租约及准入执行器 → 接入Online授权传输 → 开启Session。旧三步签票/消费接口不得与新领取/提交混用；需要兼容窗口时必须明确哪些客户端/服务器版本使用哪个状态模型。

回退前提：停止新预留，拒绝新领取，查询未完成操作；等待服务器权威租约结束或收到确切释放，再排空当前版本。禁止删除领取记录后允许旧材料继续验证。事务函数的版本和公开协议字段今后不得静默复用。未提供自动DROP回退脚本，原因是未建立完整运行装配和数据留存策略，不能替用户决定销毁准入审计。

当前实例表主键/外键和注册生命周期尚未实现完整同名进程换代协调；恢复时不能简单UPDATE boot_id绕过外键。应在接入真实注册服务前评审实例历史记录、当前Boot所有权、失效旧预留和旧心跳拒绝的迁移方案。现有错误Boot拒绝测试只证明匹配检查，不等于实例重启全流程已完成。

GamePlatformServer已经承担服务器注册/就绪/心跳，并新增Admission Subsystem/Provider边界；后续不再创建第二个服务器插件。仍待实现的是唯一生产Admission Provider、真实NetConnection/PlayerController关联、Boot/Protocol/Epoch握手和Logout释放。Session继续只保留客户端操作、Current/Pending Binding、代次和恢复语义，不保存服务器秘密。

未来Loading/World接管：资源与世界准备事实、地图/角色就绪组合；Data保持唯一加载租约所有者。当前State只聚合事实，不加载资产或操作世界。ApplicationFlow负责何时加入/迁移，不由Session新建主流程执行器。

续作首先读取当前工作树、Online/Server真实公开接口和《最佳修改方案与执行计划》。不得依据历史“Online为空”结论覆盖现有实现，也不得把测试Transport/固定Proof复制成生产认证适配。下一步先补完整可信Binding协议，再接UE默认Transport。
