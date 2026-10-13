// 项目第三层物品本地化表现回退：不下沉GamePlatform，不计算任何权威道具规则。
#include "Screens/Inventory/DivineBeastsInventoryPresentationCatalog.h"

#define LOCTEXT_NAMESPACE "DivineBeastsInventoryItems"

FDivineBeastsInventoryPresentation FDivineBeastsInventoryPresentationCatalog::Find(
	FName DefinitionId)
{
	// 后续图标必须由Monolith实际导入对应T_DBA_Item_*资源后方可显示；
	// 这些软路径绝不触发同步加载，也不能当作已经生成资产的证据。
	if (DefinitionId == TEXT("item.herb.peach"))
	{
		return {LOCTEXT("PeachHerbName", "桃灵草"), LOCTEXT("PeachHerbDescription", "生长于桃花林间的温润草药，可用于村中采集和制作。"),
			FSoftObjectPath(TEXT("/DBAUIPack_Core/UI/Inventory/Icons/T_DBA_Item_PeachHerb.T_DBA_Item_PeachHerb"))};
	}
	if (DefinitionId == TEXT("item.herb.mugwort"))
	{
		return {LOCTEXT("MugwortName", "艾叶"), LOCTEXT("MugwortDescription", "新手村田边采得的草本叶片，适合基础制作。"),
			FSoftObjectPath(TEXT("/DBAUIPack_Core/UI/Inventory/Icons/T_DBA_Item_Mugwort.T_DBA_Item_Mugwort"))};
	}
	if (DefinitionId == TEXT("item.material.lotus"))
	{
		return {LOCTEXT("LotusName", "湖心莲"), LOCTEXT("LotusDescription", "湖心三岛附近采集的清香莲瓣，用于任务与制作。"),
			FSoftObjectPath(TEXT("/DBAUIPack_Core/UI/Inventory/Icons/T_DBA_Item_Lotus.T_DBA_Item_Lotus"))};
	}
	if (DefinitionId == TEXT("item.material.stone"))
	{
		return {LOCTEXT("StoneName", "青岩矿石"), LOCTEXT("StoneDescription", "山脚裸岩中获得的矿石原料，带有天然青灰色纹理。"),
			FSoftObjectPath(TEXT("/DBAUIPack_Core/UI/Inventory/Icons/T_DBA_Item_GreenStone.T_DBA_Item_GreenStone"))};
	}
	if (DefinitionId == TEXT("item.material.peachwood"))
	{
		return {LOCTEXT("PeachwoodName", "桃木"), LOCTEXT("PeachwoodDescription", "来自桃林的木料，可用于村庄器具与基础建造。"),
			FSoftObjectPath(TEXT("/DBAUIPack_Core/UI/Inventory/Icons/T_DBA_Item_Peachwood.T_DBA_Item_Peachwood"))};
	}
	if (DefinitionId == TEXT("item.consumable.herbal_potion"))
	{
		return {LOCTEXT("HerbalPotionName", "草本药露"), LOCTEXT("HerbalPotionDescription", "装在玉色瓷瓶中的调配药露；实际使用效果由服务器确认。"),
			FSoftObjectPath(TEXT("/DBAUIPack_Core/UI/Inventory/Icons/T_DBA_Item_HerbalPotion.T_DBA_Item_HerbalPotion"))};
	}
	if (DefinitionId == TEXT("item.quest.village_badge"))
	{
		return {LOCTEXT("VillageBadgeName", "桃源村徽"), LOCTEXT("VillageBadgeDescription", "新手村完成村务后获赠的身份信物，不可普通消费。"),
			FSoftObjectPath(TEXT("/DBAUIPack_Core/UI/Inventory/Icons/T_DBA_Item_VillageBadge.T_DBA_Item_VillageBadge"))};
	}
	if (DefinitionId == TEXT("item.material.silkworm_silk"))
	{
		return {LOCTEXT("SilkName", "桑蚕丝"), LOCTEXT("SilkDescription", "桑林采集与养蚕获得的细丝材料，用于服饰和工艺制作。"),
			FSoftObjectPath(TEXT("/DBAUIPack_Core/UI/Inventory/Icons/T_DBA_Item_SilkwormSilk.T_DBA_Item_SilkwormSilk"))};
	}
	// 不认识服务器定义时不泄露后台原始编码到常规UI；诊断可通过审计工具查询快照。
	return {LOCTEXT("UnknownName", "未识别物品"),
		LOCTEXT("UnknownDescription", "缺少对应的项目显示定义，物品数量仍由服务器管理。"),
		FSoftObjectPath()};
}

#undef LOCTEXT_NAMESPACE
