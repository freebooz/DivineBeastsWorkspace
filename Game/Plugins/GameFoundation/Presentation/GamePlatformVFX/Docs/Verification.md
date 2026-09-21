# Verification（当前交付验证）

## 已完成

- 插件目录和文件生成。
- `.uplugin` JSON 语法检查。
- `Build.cs`、头文件和实现文件配对检查。
- 核心纵向链代码实现。
- 关键 UE5.8 API 已按官方文档核对：UWorldSubsystem、UAssetManager/FStreamableManager、UPrimaryDataAsset、UNiagaraFunctionLibrary、UNiagaraComponent、Data Validation。

## 未执行

当前运行环境没有：

- UE5.8 Engine / UnrealBuildTool
- 《神兽联盟》实际 `.uproject`
- `GamePlatformData` 真实 API
- `GamePlatformPresentation` 真实 Provider Interface
- 真实 Niagara `.uasset`

因此不能声明：

- UBT 编译通过
- UHT 反射通过
- Editor/Client/Server Target 构建通过
- Cook/Stage 通过
- 多 PIE 自动化测试通过
- 生产验收通过

## 接入工程后的第一步

先在真实工程中编译 `GamePlatformVFXEditor`，修正任何由项目锁定 UE5.8 分支产生的 API 微差；随后接入 `GamePlatformData` 与 `GamePlatformPresentation` 的真实接口，再进行资产和 Cook 验证。
