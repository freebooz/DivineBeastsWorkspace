// 神兽联盟项目草地及雪纹理软映射；资源身份归原所有者，实际加载交给平台Data。
// 对应客户端配置只存软路径，不产生对Surface插件资产的服务器Cook硬依赖。
#include "Settings/DivineBeastsVillageSnowSettings.h"

UDivineBeastsVillageSnowSettings::UDivineBeastsVillageSnowSettings()
{
    SnowCoverageTexture = FSoftObjectPath(TEXT(
        "/GamePlatformSurface/Textures/Snow/T_GP_Snow_CoverageNoise.T_GP_Snow_CoverageNoise"));
    SnowAlbedoTexture = FSoftObjectPath(TEXT(
        "/GamePlatformSurface/Textures/Weather/T_GP_Weather_SnowAlbedo.T_GP_Weather_SnowAlbedo"));
    SnowNormalTexture = FSoftObjectPath(TEXT(
        "/GamePlatformSurface/Textures/Weather/T_GP_Weather_SnowNormal.T_GP_Weather_SnowNormal"));
    SnowORMTexture = FSoftObjectPath(TEXT(
        "/GamePlatformSurface/Textures/Weather/T_GP_Weather_SnowORM.T_GP_Weather_SnowORM"));
    SnowHeightTexture = FSoftObjectPath(TEXT(
        "/GamePlatformSurface/Textures/Snow/T_GP_Snow_MicroHeight.T_GP_Snow_MicroHeight"));
    // 2026-10-18用户授权将V3.0正式用于Village：仅客户端通过既有世界租约装配，不修改共享Landscape资产。
    // V3草地色是同一Poly Haven CC0扫描BaseColor的派生，Normal/ORM必须取该源扫描对应版本，
    // 防止颜色与微表面来自两套不同扫描而出现不真实的光照/高光。原V3母材质和审核地图继续独立。
    GroundAlbedoTexture = FSoftObjectPath(TEXT(
        "/DBAPresentationPack_Core/Textures/Terrain/Production10/T_DBA_Terrain_Grass_BaseColor_V3.T_DBA_Terrain_Grass_BaseColor_V3"));
    GroundNormalTexture = FSoftObjectPath(TEXT(
        "/DBAWorldPack_Village/Terrain/Textures/Grass/T_DBA_Terrain_Grass_Normal.T_DBA_Terrain_Grass_Normal"));
    GroundORMTexture = FSoftObjectPath(TEXT(
        "/DBAWorldPack_Village/Terrain/Textures/Grass/T_DBA_Terrain_Grass_ORM.T_DBA_Terrain_Grass_ORM"));
    // 项目层只保存稳定的纯客户端MaterialInstance路径，真实加载交给平台Data的世界租约。
    MossSnowRockMaterial = FSoftObjectPath(TEXT(
        "/DBAPresentationPack_Core/Materials/Environment/MI_DBA_MossSnow_Cliff.MI_DBA_MossSnow_Cliff"));
}
