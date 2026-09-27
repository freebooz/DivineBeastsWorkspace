# SocketsAndAttachment（挂点与附着）

SocketName、RelativeTransform 和 MaterialOverrides 都来自 ClientOnly Visual Definition，客户端不能把 SocketName 作为权威 Equip 参数提交服务器。

加载完成后验证 Avatar SkeletalMesh（角色骨骼网格）存在目标 Socket，再创建/复用 StaticMeshComponent 并 AttachToComponent（附着到组件）。

Socket 缺失只造成视觉降级，不撤销后端长期装备或服务器 Gameplay Grant。真实 Development Skeleton 上的 Weapon_R 等 Socket 尚未通过 Unreal Editor 验证。