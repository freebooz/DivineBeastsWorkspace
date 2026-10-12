# PCG Schema v1与UE5.8元数据存储边界

2026-10-12真实自动化暴露：UE5.8的UPCGMetadata不允许属性名包含点号，直接使用Pcg.Layer.Name、Pcg.Spawn.MeshSetId等创建属性会失败。不能因此把公开Schema v1或项目配置的Pcg.*字段改名。

现由FGamePlatformPCGSchema::GetMetadataAttributeName只对已注册字段作存储编码：逻辑Pcg.Layer.Name对应存储Pcg_Layer_Name。所有平台节点创建、读取、写入元数据均经过这一边界；校验器把存储名重新匹配至逻辑注册表，保留原核心字段与类型验证，未知Pcg_或Pcg.字段都失败关闭。20个注册字段保持无碰撞，未知逻辑字段返回None而非开放任意映射。

新编写的原生PCG元数据操作应调用该方法；项目DataAsset/Template描述仍使用Pcg.*逻辑字段。若人工使用UE的原生属性选择器，应选择下划线存储名。没有对既有资产批量改写或迁移，也不将已失败的旧生成结果声称为有效。

回归包括真实PointArrayData的MeshSet逐点覆盖、核心类型与未知字段拒绝、20字段唯一映射。此次修复不证明全部地形/WorldPartition/碰撞生成和烘焙已人工验收。
