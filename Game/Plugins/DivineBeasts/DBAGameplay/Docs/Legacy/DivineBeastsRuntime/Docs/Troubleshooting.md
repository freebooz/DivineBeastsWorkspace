# Troubleshooting（故障排查）

Runtime Build提示Generated Catalog缺失：先执行Build/Contracts/Generate-DivineBeastsContracts.ps1。

DivineBeastsContracts提示静态库缺失：按报错中的Platform/Architecture/Configuration/Compiler/UE5.8路径运行Build-DivineBeastsStaticLibrary.ps1。当前Runner没有MSVC时会明确返回“未执行”。

ProjectContext无效：依次检查GameId/ProjectId、ServerRole、Experience映射；ArenaMode只能与MainArena + Experience.MainArena.Main组合。

clean regenerate失败：禁止直接修Generated，先修改Shared真源或生成器，再重新生成。

UE编译错误不能用PowerShell静态验证替代；以锁定UE5.8/UBT实际API为准。
