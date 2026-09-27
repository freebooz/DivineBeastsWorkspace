# ReplicationAndLateJoin（复制与晚加入）

共享 Equipment Component 使用真实 ReplicatedUsing/DOREPLIFETIME（复制注册）。

OwnerSnapshot 以 COND_OwnerOnly 发送完整 ItemInstance 信息；PublicSnapshot 面向观察者只含 Slot/EquipmentDefinition/VisualDefinition；EquipmentRuntimeGeneration 也复制。

Public Snapshot 是 Late Join（晚加入）视觉恢复的基础，但当前没有 Dedicated Server + 双客户端运行环境，因此 Owner/Observer/Replace/Remove/Late Join/乱序 Revision 的真实网络验证未执行。