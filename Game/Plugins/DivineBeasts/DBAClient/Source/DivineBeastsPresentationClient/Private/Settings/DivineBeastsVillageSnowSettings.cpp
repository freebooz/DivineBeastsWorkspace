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
    GroundAlbedoTexture = FSoftObjectPath(TEXT(
        "/DBAPresentationPack_Core/Textures/Terrain/Grass/T_DBA_Village_Grass_BaseColor.T_DBA_Village_Grass_BaseColor"));
    GroundNormalTexture = FSoftObjectPath(TEXT(
        "/DBAPresentationPack_Core/Textures/Terrain/Grass/T_DBA_Village_Grass_Normal.T_DBA_Village_Grass_Normal"));
    GroundORMTexture = FSoftObjectPath(TEXT(
        "/DBAPresentationPack_Core/Textures/Terrain/Grass/T_DBA_Village_Grass_ORM.T_DBA_Village_Grass_ORM"));
    // 项目层只保存稳定的纯客户端MaterialInstance路径，真实加载交给平台Data的世界租约。
    MossSnowRockMaterial = FSoftObjectPath(TEXT(
        "/DBAPresentationPack_Core/Materials/Environment/MI_DBA_MossSnow_Cliff.MI_DBA_MossSnow_Cliff"));
}
