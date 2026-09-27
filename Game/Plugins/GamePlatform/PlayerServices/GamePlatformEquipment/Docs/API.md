# API（接口）

共享操作结构只允许表达 OperationId、CharacterId、SlotId、ItemInstanceId、ExpectedEquipmentRevision 和 ExpectedInventoryRevision，不接收 Attack、Defense、GameplayEffect Class、Mesh 路径或 SocketName。

PlayerData 内部 API 提供 Snapshot、Equip、Unequip 与 Operation Query，且仅允许绑定 Dedicated Server（专用服务器）调用。

EquipmentServer 通过异步 Persistence Port 调用后端；Client Visual 不参与权威操作。