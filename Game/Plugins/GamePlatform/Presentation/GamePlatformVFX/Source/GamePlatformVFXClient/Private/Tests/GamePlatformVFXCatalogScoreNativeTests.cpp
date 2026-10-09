// 本文件属于GamePlatform平台层 GamePlatformVFX，负责回归用例；夹具仅测试作用域，不伪造生产资源成功。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// 只验证生产兼容排序的P13不变量；不启动UE、不代表资产解析或播放验收。
#if defined(GAMEPLATFORM_VFX_NATIVE_TEST)
#include "../Resolution/GamePlatformVFXCatalogScore.h"
#include <iostream>
int main()
{
    FGamePlatformVFXCatalogScore Project; Project.SemanticTier=3; Project.Scope=2;
    FGamePlatformVFXCatalogScore Pack=Project; Pack.Scope=3;
    FGamePlatformVFXCatalogScore Platform=Project; Platform.Scope=0; Platform.Specificity=6; Platform.ContextTier=2;
    if (!Pack.IsBetterThan(Platform)) { std::cerr << "F03: scope must precede context specificity\n"; return 1; }
    FGamePlatformVFXCatalogScore Parent=Pack; Parent.SemanticTier=2;
    if (!Project.IsBetterThan(Parent)) { std::cerr << "exact semantic must precede parent scope\n"; return 2; }
    auto Specific=Pack; Specific.Specificity=1;
    if (!Specific.IsBetterThan(Pack)) return 3;
    auto Equal=Specific;
    if (!(Equal==Specific) || Equal.IsBetterThan(Specific)) return 4;
    std::cout << "VFX P13 score invariants passed\n";
}
#endif
