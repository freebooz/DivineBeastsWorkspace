// 中立目录排序生产策略回归：精确语义不能被高作用域父语义覆盖，近父级优先远父级。
#if defined(GAMEPLATFORM_PRESENTATION_NATIVE_TEST)
#include "../Resolution/GamePlatformPresentationCatalogScore.h"
#include <iostream>
int main()
{
    FGamePlatformPresentationCatalogScore exact; exact.SemanticRank=2; exact.Scope=2;
    FGamePlatformPresentationCatalogScore parent; parent.SemanticRank=1; parent.Scope=3;
    if (!exact.IsBetterThan(parent)) { std::cerr << "精确语义被高作用域父语义覆盖\n"; return 1; }
    FGamePlatformPresentationCatalogScore nearParent=parent; nearParent.SemanticDistance=1; nearParent.Scope=0;
    FGamePlatformPresentationCatalogScore farParent=parent; farParent.SemanticDistance=2;
    if (!nearParent.IsBetterThan(farParent)) return 2;
    FGamePlatformPresentationCatalogScore highScope=exact; highScope.Scope=3;
    if (!highScope.IsBetterThan(exact)) return 3;
    return 0;
}
#endif
