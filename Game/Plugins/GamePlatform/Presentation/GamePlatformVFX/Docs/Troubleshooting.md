# Troubleshooting（故障排查）

CatalogMiss：先检查Catalog Fragment是否注册、Semantic/DefinitionId、ContextTags、Platform/Quality过滤条件。CatalogAmbiguous：检查Specificity/Scope/Priority是否仍完全同级，禁止用数组顺序规避。

DefinitionLoadFailed：检查软引用Cook、GamePlatformData加载、Parameter Schema、平台/质量变体及PreloadAssets。跨World Handle失败属于预期保护，应由新World重新请求表现。

UE验证显示未执行：检查UE_ROOT、Build.bat、UnrealEditor-Cmd.exe、Review/Test资产和真实Cook工件路径，不得手工把状态改成通过。