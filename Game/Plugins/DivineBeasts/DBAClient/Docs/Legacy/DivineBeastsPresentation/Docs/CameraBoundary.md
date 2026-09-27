# CameraBoundary（相机边界）

相机算法归GamePlatformCamera（游戏平台相机）。

本插件不调用APlayerCameraManager、不实现Shake/Blend/LockOn算法、不持有Camera Animation资产。项目Catalog未来可以提供Camera通道逻辑DefinitionId，实际执行由Camera Provider负责。

QualityTier和LocalPlayerRelation只是表现提示，不改变Gameplay。
