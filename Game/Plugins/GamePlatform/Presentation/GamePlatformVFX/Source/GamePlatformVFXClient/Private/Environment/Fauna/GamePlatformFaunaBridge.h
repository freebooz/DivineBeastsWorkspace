// UE值合同与真实标准C++算法之间的唯一转换；生产与原生测试使用同一算法源文件。
#pragma once
#include "Environment/GamePlatformFaunaConfig.h"
#include "Environment/Fauna/GamePlatformFaunaSimulation.h"

namespace GamePlatformFaunaBridge
{
inline bool Initialize(GamePlatformFauna::FGamePlatformFaunaSimulation& Simulation,
    const FGamePlatformFaunaConfig& Config, FString& OutError)
{
    if (Config.Kind!=EGamePlatformFaunaKind::Butterfly && Config.Kind!=EGamePlatformFaunaKind::Fish)
    { OutError=TEXT("不支持的群落运动模型。"); return false; }
    if (Config.Flowers.Num()>256 || Config.Obstacles.Num()>128 || Config.WaterPolygon.Num()>128 ||
        Config.Seed<0 || Config.HalfExtent.GetAbsMax()>100000.0)
    { OutError=TEXT("群落花点/障碍/水域顶点或局部范围超过配置上限。"); return false; }
    GamePlatformFauna::FSettings Settings;
    Settings.Kind=Config.Kind==EGamePlatformFaunaKind::Fish ? GamePlatformFauna::EKind::Fish : GamePlatformFauna::EKind::Butterfly;
    Settings.Count=Config.Count; Settings.Seed=Config.Seed;
    Settings.HalfExtent={Config.HalfExtent.X,Config.HalfExtent.Y,Config.HalfExtent.Z};
    Settings.CruiseSpeed=Config.CruiseSpeed; Settings.BodyRadius=Config.BodyRadius;
    Settings.PerchOffset=Config.PerchOffsetCm;
    Settings.NeighborRadius=Config.NeighborRadius; Settings.SeparationRadius=Config.SeparationRadius;
    Settings.MaxNeighbors=Config.MaxNeighbors; Settings.FixedStepSeconds=1.0/30.0;
    std::vector<GamePlatformFauna::FFlower> Flowers;
    std::vector<GamePlatformFauna::FObstacle> Obstacles;
    std::vector<GamePlatformFauna::FVector> Polygon;
    for (const auto& P:Config.Flowers) Flowers.push_back({{P.Position.X,P.Position.Y,P.Position.Z},{P.Normal.X,P.Normal.Y,P.Normal.Z},P.Capacity});
    for (const auto& P:Config.Obstacles) Obstacles.push_back({{P.Center.X,P.Center.Y,P.Center.Z},P.Radius});
    for (const auto& P:Config.WaterPolygon) Polygon.push_back({P.X,P.Y,0.0});
    std::string Error;
    if (!Simulation.Initialize(Settings,Flowers,Obstacles,Polygon,Error))
    { OutError=TEXT("群落配置无效：")+FString(UTF8_TO_TCHAR(Error.c_str())); return false; }
    OutError.Reset(); return true;
}
}
