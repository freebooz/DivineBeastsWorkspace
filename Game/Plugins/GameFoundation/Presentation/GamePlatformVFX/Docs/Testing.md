# Testing（测试规范）

自动化测试源码放在真实模块 `Private/Tests`，不是插件根 `Tests`。

建议 CI：

1. Editor Target 编译。
2. Client Target 编译。
3. Server Target 编译并确认不装配 `GamePlatformVFXClient`。
4. `UnrealEditor-Cmd -run=DataValidation`。
5. Client Clean Cook + Stage。
6. Server Clean Cook + Stage。
7. 使用 AssetRegistry / IoStore 清单审计服务器没有纯 VFX 内容。
8. PIE 多实例：切图、取消加载、重复 Stop、Catalog 注销。
