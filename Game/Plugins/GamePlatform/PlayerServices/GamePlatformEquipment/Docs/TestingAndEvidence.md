# TestingAndEvidence（测试与证据）

`Build/Validation/VerifyEquipment.ps1（装备静态验证入口）`依次执行 Backend、Server、ClientVisual、Respawn 四个门禁。全工作区 Test-PluginLayers 只验证结构/字面依赖。

UE 自动化测试源码覆盖共享 Equipment Definition/Slot 与 Client Visual Definition。Go 测试源码覆盖 Catalog/Slot 和 Equip/Unequip Service 输入验证。

静态门禁不能替代 UE Build、GAS Runtime、PostgreSQL 事务、Dedicated Server 双客户端、Remote Observer、Late Join、Respawn 和 Cook。当前 Runner 缺相关工具链，因此运行项保持未执行。