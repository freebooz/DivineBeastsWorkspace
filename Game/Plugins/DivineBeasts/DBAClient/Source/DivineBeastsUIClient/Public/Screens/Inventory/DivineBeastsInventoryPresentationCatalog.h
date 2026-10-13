#pragma once

#include "CoreMinimal.h"
// FSoftObjectPath（软对象路径）为具名成员，必须包含完整类型定义而非仅前置声明。
#include "UObject/SoftObjectPath.h"

/**
 * FDivineBeastsInventoryPresentation（项目第三层物品中文表现）。
 * 仅决定前端可见的名称、描述与未来正式图标身份；
 * 不包含数量、权限、堆叠规则和效果，不依赖Go后端实现。
 */
struct FDivineBeastsInventoryPresentation
{
	FText DisplayName;
	FText Description;
	FSoftObjectPath IconAssetPath;
};

/**
 * FDivineBeastsInventoryPresentationCatalog（项目物品表现回退表）。
 * 在正式项目DataAsset和资产目录接通前，提供固定本地化条目；
 * 未知物品只显示安全占位名称，不根据客户端显示内容授予道具。
 */
class FDivineBeastsInventoryPresentationCatalog final
{
public:
	/** 根据服务器ItemDefinitionId（物品定义稳定编号）寻找项目中文名称和说明。 */
	static FDivineBeastsInventoryPresentation Find(FName DefinitionId);
};
