# Dependencies（依赖规则）

当前代码模块依赖：Core、CoreUObject、Engine、GameplayTags、Niagara、DeveloperSettings。

禁止基础层反向引用：

- `MobaCommon/*`
- `DivineBeasts/*`
- `DBAHeroPack_*`
- `DBAWorldPack_*`
- `DBASkinPack_*`

注意：独立源码包不知道项目真实 Asset Mount Point，因此 `GamePlatformVFXDependencyValidator` 只提供接入骨架。进入真实仓库后必须使用 AssetRegistry 对实际挂载点执行依赖审计。
