// 平台层客户端群落纯算法回归：直接链接正式 Private 源码，不模拟 UE、渲染或资源服务。
// 本文件仅由原生验证入口编译；通过不代表 UE、GPU、Cook 或联机验收。
// 几何断言采用已知盒、L 型区域和球体的解析约束，验证输出，不复制行为算法。
#include "GamePlatformFaunaSimulation.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace GamePlatformFauna;

namespace
{
int Passed = 0;
int Failed = 0;
// 失败终止当前用例；主入口继续独立用例并最终返回非零退出码。
void Require(bool bCondition, const std::string& Message)
{
    if (!bCondition) { throw std::runtime_error(Message); }
}
bool Near(double A, double B, double Tolerance = 1.e-9)
{
    return std::abs(A - B) <= Tolerance;
}
double DistanceSquared(const FVector& A, const FVector& B)
{
    const double X = A.X - B.X, Y = A.Y - B.Y, Z = A.Z - B.Z;
    return X * X + Y * Y + Z * Z;
}
// 检验有限性及公开单位范围；鱼与蝶均须保持此不变量。
void CheckAgent(const FAgent& Agent, const FSettings& Settings)
{
    Require(std::isfinite(Agent.Position.X) && std::isfinite(Agent.Position.Y) && std::isfinite(Agent.Position.Z), "位置必须有限");
    Require(std::isfinite(Agent.Velocity.X) && std::isfinite(Agent.Velocity.Y) && std::isfinite(Agent.Velocity.Z), "速度必须有限");
    Require(std::abs(Agent.Position.X) <= Settings.HalfExtent.X - Settings.BodyRadius + 1.e-5, "越过 X 边界");
    Require(std::abs(Agent.Position.Y) <= Settings.HalfExtent.Y - Settings.BodyRadius + 1.e-5, "越过 Y 边界");
    Require(std::abs(Agent.Position.Z) <= Settings.HalfExtent.Z - Settings.BodyRadius + 1.e-5, "越过 Z 边界");
    Require(std::isfinite(Agent.AnimationPhase) && Agent.AnimationPhase >= 0.0 && Agent.AnimationPhase < 1.0, "动画相位必须归一化");
    Require(std::isfinite(Agent.FlightWeight) && Agent.FlightWeight >= 0.0 && Agent.FlightWeight <= 1.0, "飞行权重必须处于 0..1");
}
FSettings MakeSettings(EKind Kind, int Count)
{
    FSettings Settings;
    Settings.Kind = Kind; Settings.Count = Count; Settings.Seed = 17431;
    Settings.HalfExtent = {400.0, 400.0, 160.0};
    Settings.CruiseSpeed = Kind == EKind::Butterfly ? 130.0 : 220.0;
    Settings.BodyRadius = 3.0; Settings.NeighborRadius = 150.0;
    Settings.SeparationRadius = 28.0; Settings.MaxNeighbors = 12;
    return Settings;
}
void Initialize(FGamePlatformFaunaSimulation& Simulation, const FSettings& Settings,
    const std::vector<FFlower>& Flowers = {}, const std::vector<FObstacle>& Obstacles = {},
    const std::vector<FVector>& Polygon = {})
{
    std::string Error;
    Require(Simulation.Initialize(Settings, Flowers, Obstacles, Polygon, Error), "合法初始化失败：" + Error);
    Require(Error.empty(), "成功初始化不得残留错误");
}
template <typename Function>
void Run(const char* Name, Function Body)
{
    try { Body(); ++Passed; std::cout << "[PASS] " << Name << '\n'; }
    catch (const std::exception& Error) { ++Failed; std::cerr << "[FAIL] " << Name << ": " << Error.what() << '\n'; }
}
}

// 正常、失败、容量、刺激、确定性、邻域上界与持续运行；不存在假成功分支。
int main()
{
    Run("Counts_0_1_128_512", []
    {
        for (EKind Kind : {EKind::Butterfly, EKind::Fish})
            for (int Count : {0, 1, 128, 512})
            {
                auto Settings = MakeSettings(Kind, Count);
                FGamePlatformFaunaSimulation Simulation; Initialize(Simulation, Settings);
                for (int Step = 0; Step < 90; ++Step) { Simulation.Step(Settings.FixedStepSeconds); }
                Require(Simulation.GetAgents().size() == static_cast<std::size_t>(Count), "数量不应漂移");
                for (const auto& Agent : Simulation.GetAgents()) { CheckAgent(Agent, Settings); }
            }
    });
    Run("Atomic_InvalidConfiguration", []
    {
        FGamePlatformFaunaSimulation Simulation, Reference;
        auto Settings = MakeSettings(EKind::Fish, 16);
        Initialize(Simulation, Settings); Initialize(Reference, Settings);
        std::string Error; const auto Before = Simulation.GetAgents();
        for (int Count : {-1, 513})
        {
            auto Invalid = Settings; Invalid.Count = Count;
            Require(!Simulation.Initialize(Invalid, {}, {}, {}, Error) && !Error.empty(), "非法数量应失败");
        }
        auto Invalid = Settings; Invalid.HalfExtent.X = std::numeric_limits<double>::quiet_NaN();
        Require(!Simulation.Initialize(Invalid, {}, {}, {}, Error), "NaN 范围应失败");
        Invalid = Settings; Invalid.CruiseSpeed = std::numeric_limits<double>::infinity();
        Require(!Simulation.Initialize(Invalid, {}, {}, {}, Error), "无限速度应失败");
        Invalid = Settings; Invalid.MaxNeighbors = 33;
        Require(!Simulation.Initialize(Invalid, {}, {}, {}, Error), "邻居上限应受限");
        Invalid = Settings; Invalid.FixedStepSeconds = 0.0;
        Require(!Simulation.Initialize(Invalid, {}, {}, {}, Error), "零步长应失败");
        Require(!Simulation.Initialize(Settings, {{{0,0,0}, {0,0,0}, 1}}, {}, {}, Error), "零法线应失败");
        Require(!Simulation.Initialize(Settings, {}, {{{0,0,0}, -1.0}}, {}, Error), "负障碍半径应失败");
        Require(Simulation.GetAgents().size() == Before.size(), "失败不能替换旧数组");
        for (std::size_t Index = 0; Index < Before.size(); ++Index)
            Require(DistanceSquared(Simulation.GetAgents()[Index].Position, Before[Index].Position) == 0.0, "失败不能改变已有位置");
        Simulation.Step(Settings.FixedStepSeconds); Reference.Step(Settings.FixedStepSeconds);
        for (std::size_t Index = 0; Index < Before.size(); ++Index)
            Require(DistanceSquared(Simulation.GetAgents()[Index].Position, Reference.GetAgents()[Index].Position) == 0.0, "失败不能污染后续随机序列");
    });
    Run("DeterministicSeed_AndFixedStep", []
    {
        auto Settings = MakeSettings(EKind::Fish, 128);
        FGamePlatformFaunaSimulation A, B; Initialize(A, Settings); Initialize(B, Settings);
        for (int Step = 0; Step < 300; ++Step)
        {
            A.Step(Settings.FixedStepSeconds);
            B.Step(Settings.FixedStepSeconds * 0.5); B.Step(Settings.FixedStepSeconds * 0.5);
        }
        for (std::size_t Index = 0; Index < A.GetAgents().size(); ++Index)
            Require(DistanceSquared(A.GetAgents()[Index].Position, B.GetAgents()[Index].Position) < 1.e-15, "同种子及同固定步应重现");
        auto Changed = Settings; Changed.Seed += 1; Initialize(B, Changed);
        Require(DistanceSquared(A.GetAgents()[0].Position, B.GetAgents()[0].Position) > 1.e-4, "不同种子应形成不同布局");
    });
    Run("Butterfly_EmptyFlowers", []
    {
        auto Settings = MakeSettings(EKind::Butterfly, 24);
        FGamePlatformFaunaSimulation Simulation; Initialize(Simulation, Settings);
        const auto Initial = Simulation.GetAgents()[0].Position;
        for (int Step = 0; Step < 1800; ++Step)
        {
            Simulation.Step(Settings.FixedStepSeconds);
            for (const auto& Agent : Simulation.GetAgents())
            {
                CheckAgent(Agent, Settings);
                Require(Agent.FlowerIndex == -1 && Agent.Behavior != EBehavior::Perched, "无花点只能游飞");
            }
        }
        Require(DistanceSquared(Initial, Simulation.GetAgents()[0].Position) > 1.0, "无花点不能静止");
        Require(Simulation.GetStatistics().OccupiedFlowerSlots == 0, "空花点不能产生占位");
    });
    Run("Butterfly_Capacity_Perch_Takeoff", []
    {
        auto Settings = MakeSettings(EKind::Butterfly, 16);
        std::vector<FFlower> Flowers = {{{0,0,-40}, {0,0,1}, 1}};
        FGamePlatformFaunaSimulation Simulation; Initialize(Simulation, Settings, Flowers);
        bool bPerched = false, bTookOff = false;
        for (int Step = 0; Step < 3600; ++Step)
        {
            Simulation.Step(Settings.FixedStepSeconds); int Reserved = 0;
            for (const auto& Agent : Simulation.GetAgents())
            {
                CheckAgent(Agent, Settings);
                if (Agent.FlowerIndex >= 0) { ++Reserved; }
                if (Agent.Behavior == EBehavior::Perched)
                {
                    bPerched = true;
                    Require(Near(Agent.Position.Z, -40.0 + Settings.PerchOffset, 1.e-4), "停栖须落在花面法线方向");
                    Require(DistanceSquared(Agent.Velocity, {}) < 1.e-12, "停栖须静止");
                }
                if (bPerched && Agent.Behavior == EBehavior::TakingOff) { bTookOff = true; }
            }
            Require(Reserved <= 1 && Simulation.GetStatistics().OccupiedFlowerSlots <= 1, "单容量不得双占位");
            Require(static_cast<unsigned>(Reserved) == Simulation.GetStatistics().OccupiedFlowerSlots, "统计必须反映实际预约");
        }
        Require(bPerched, "必须真正到达停栖态"); Require(bTookOff, "停留完成必须释放并起飞");
    });
    Run("Butterfly_Disturbance_Release_Recovery", []
    {
        auto Settings = MakeSettings(EKind::Butterfly, 10);
        FGamePlatformFaunaSimulation Simulation; Initialize(Simulation, Settings, {{{0,0,0}, {0,0,1}, 2}});
        for (int Step = 0; Step < 1500; ++Step) { Simulation.Step(Settings.FixedStepSeconds); }
        std::string Error; Require(Simulation.SetDisturbance({}, 2000, 1, 1, Error), "合法刺激应受理");
        Require(Simulation.GetStatistics().OccupiedFlowerSlots == 0, "受惊调用须立即释放预约");
        for (const auto& Agent : Simulation.GetAgents())
            Require(Agent.FlowerIndex == -1 && Agent.Behavior == EBehavior::Fleeing, "受惊态不能残留花点");
        bool bRecovered = false;
        for (int Step = 0; Step < 1800; ++Step)
        {
            Simulation.Step(Settings.FixedStepSeconds);
            for (const auto& Agent : Simulation.GetAgents())
            {
                CheckAgent(Agent, Settings); bRecovered = bRecovered || Agent.Behavior == EBehavior::Perched;
            }
        }
        Require(bRecovered, "刺激消失后应恢复停栖");
    });
    Run("Fish_SpatialNeighbors_Bounded", []
    {
        auto Settings = MakeSettings(EKind::Fish, 512); Settings.MaxNeighbors = 8;
        FGamePlatformFaunaSimulation Simulation; Initialize(Simulation, Settings);
        std::uint64_t ObservedChecks = 0;
        for (int Step = 0; Step < 600; ++Step)
        {
            Simulation.Step(Settings.FixedStepSeconds);
            const auto& Statistics = Simulation.GetStatistics(); ObservedChecks += Statistics.NeighborChecks;
            Require(Statistics.NeighborChecks <= static_cast<std::uint64_t>(Settings.Count * 32), "空间候选检查应受每体预算限制");
            for (const auto& Agent : Simulation.GetAgents()) { CheckAgent(Agent, Settings); }
        }
        Require(ObservedChecks > 0, "鱼群必须实际查询邻居");
    });
    Run("Fish_ConcaveWater_AndIsland", []
    {
        auto Settings = MakeSettings(EKind::Fish, 128);
        const std::vector<FVector> Polygon = {{-400,-400,0},{400,-400,0},{400,0,0},{0,0,0},{0,400,0},{-400,400,0}};
        const FObstacle Island = {{-180,-120,0}, 90};
        FGamePlatformFaunaSimulation Simulation; Initialize(Simulation, Settings, {}, {Island}, Polygon);
        for (int Step = 0; Step < 2400; ++Step)
        {
            Simulation.Step(Settings.FixedStepSeconds);
            for (const auto& Agent : Simulation.GetAgents())
            {
                CheckAgent(Agent, Settings);
                Require(!(Agent.Position.X > 0.0 && Agent.Position.Y > 0.0), "越出 L 型水域");
                Require(DistanceSquared(Agent.Position, Island.Center) >= std::pow(Island.Radius + Settings.BodyRadius - 1.e-5, 2), "进入岛球体");
            }
        }
    });
    Run("Fish_HighSpeed_NoObstacleTunneling", []
    {
        auto Settings = MakeSettings(EKind::Fish, 8); Settings.CruiseSpeed = 15000; Settings.FixedStepSeconds = 0.1;
        const FObstacle Island = {{0,0,0}, 120};
        FGamePlatformFaunaSimulation Simulation; Initialize(Simulation, Settings, {}, {Island});
        for (int Step = 0; Step < 900; ++Step)
        {
            const auto Previous = Simulation.GetAgents(); Simulation.Step(Settings.FixedStepSeconds);
            for (std::size_t Index = 0; Index < Previous.size(); ++Index)
            {
                const auto& A = Previous[Index].Position; const auto& B = Simulation.GetAgents()[Index].Position;
                const FVector Delta = {B.X-A.X, B.Y-A.Y, B.Z-A.Z}; const double LengthSquared = DistanceSquared(A, B);
                const double T = LengthSquared > 0 ? std::fmax(0.0, std::fmin(1.0, -(A.X*Delta.X+A.Y*Delta.Y+A.Z*Delta.Z)/LengthSquared)) : 0;
                const FVector Closest = {A.X+T*Delta.X, A.Y+T*Delta.Y, A.Z+T*Delta.Z};
                Require(DistanceSquared(Closest, {}) >= std::pow(Island.Radius + Settings.BodyRadius - 1.e-4, 2), "单步路径穿过岛体");
            }
        }
    });
    Run("InvalidPolygon_AndNoLegalSpace", []
    {
        auto Settings = MakeSettings(EKind::Fish, 12);
        FGamePlatformFaunaSimulation Simulation; Initialize(Simulation, Settings);
        const auto Initial = Simulation.GetAgents()[0].Position; std::string Error;
        Require(!Simulation.Initialize(Settings, {}, {}, {{-200,-200,0},{200,200,0},{-200,200,0},{200,-200,0}}, Error), "自交多边形应失败");
        Require(!Simulation.Initialize(Settings, {}, {{{0,0,0},1000}}, {}, Error), "全覆盖障碍应初始化失败");
        Require(DistanceSquared(Initial, Simulation.GetAgents()[0].Position) == 0.0, "几何失败应保留旧群落");
    });
    Run("InvalidDelta_AndDisturbance_AreAtomic", []
    {
        auto Settings = MakeSettings(EKind::Butterfly, 8);
        FGamePlatformFaunaSimulation Simulation, Reference; Initialize(Simulation, Settings); Initialize(Reference, Settings);
        const auto Before = Simulation.GetAgents()[0].Position;
        Simulation.Step(-1); Simulation.Step(std::numeric_limits<double>::infinity()); Simulation.Step(std::numeric_limits<double>::quiet_NaN());
        Require(DistanceSquared(Before, Simulation.GetAgents()[0].Position) == 0.0, "非法时间必须无副作用");
        std::string Error;
        Require(Simulation.SetDisturbance({}, 2000, 0.8, 1, Error), "有效刺激应成功");
        Require(Reference.SetDisturbance({}, 2000, 0.8, 1, Error), "参考刺激应成功");
        Require(!Simulation.SetDisturbance({}, -1, 0.8, 1, Error), "非法刺激半径应失败");
        Require(!Simulation.SetDisturbance({}, 1, std::numeric_limits<double>::quiet_NaN(), 1, Error), "非有限强度应失败");
        for (int Step = 0; Step < 90; ++Step) { Simulation.Step(Settings.FixedStepSeconds); Reference.Step(Settings.FixedStepSeconds); }
        Require(DistanceSquared(Simulation.GetAgents()[0].Position, Reference.GetAgents()[0].Position) == 0.0, "失败刺激不能覆盖已有刺激");
    });
    Run("LongRun_Finite_AndReset", []
    {
        auto Settings = MakeSettings(EKind::Fish, 128);
        FGamePlatformFaunaSimulation Simulation; Initialize(Simulation, Settings); std::string Error;
        for (int Step = 0; Step < 18000; ++Step)
        {
            if (Step % 1800 == 0) { Require(Simulation.SetDisturbance({}, 350, 1, 0.8, Error), "长稳刺激失败"); }
            Simulation.Step(Settings.FixedStepSeconds);
            for (const auto& Agent : Simulation.GetAgents()) { CheckAgent(Agent, Settings); }
        }
        for (const auto& Agent : Simulation.GetAgents()) { Require(Agent.Behavior == EBehavior::Swimming, "鱼应恢复游动态"); }
        Simulation.Step(1000000);
        Require(Simulation.GetStatistics().FixedSteps <= 8, "超长帧不能触发无界补帧");
        Simulation.Reset();
        Require(Simulation.GetAgents().empty() && Simulation.GetStatistics().OccupiedFlowerSlots == 0, "重置应清空群落与预约");
        Simulation.Step(1); Require(Simulation.GetAgents().empty(), "重置后不得复活");
    });
    Run("TinyGeometry_StillRejectsCoveredHabitat", []
    {
        auto Settings = MakeSettings(EKind::Fish, 1);
        Settings.HalfExtent = {0.0002,0.0002,0.0002}; Settings.BodyRadius = 0.00001;
        Settings.PerchOffset = 0.0; // 此回归仅验证极小尺度几何，不让未使用的停栖配置提前拒绝。
        Settings.CruiseSpeed = 0.0001; Settings.NeighborRadius = 0.0001; Settings.SeparationRadius = 0.00002;
        FGamePlatformFaunaSimulation Simulation; std::string Error;
        Require(!Simulation.Initialize(Settings, {}, {{{0,0,0},0.0005}}, {}, Error), "绝对容差不能使小尺度全覆盖障碍失效");
    });
    Run("Fish_NeighborLimitChangesActualSteering", []
    {
        auto One = MakeSettings(EKind::Fish, 128); One.MaxNeighbors = 1;
        auto Many = One; Many.MaxNeighbors = 32;
        FGamePlatformFaunaSimulation A, B; Initialize(A,One); Initialize(B,Many);
        for (std::size_t I = 0; I < A.GetAgents().size(); ++I)
            Require(DistanceSquared(A.GetAgents()[I].Position,B.GetAgents()[I].Position) == 0.0, "邻域上限不应改变出生布局");
        A.Step(One.FixedStepSeconds); B.Step(Many.FixedStepSeconds);
        bool bDifferent = false;
        for (std::size_t I = 0; I < A.GetAgents().size(); ++I)
            bDifferent = bDifferent || DistanceSquared(A.GetAgents()[I].Velocity,B.GetAgents()[I].Velocity) > 1.e-8;
        Require(bDifferent && A.GetStatistics().NeighborLinks > 0 && B.GetStatistics().NeighborLinks > 0, "实际邻居必须影响速度而非只累计统计");
    });
    Run("Butterfly_DisabledFlower_TiltedNormal_Reset", []
    {
        auto Settings = MakeSettings(EKind::Butterfly, 8);
        FGamePlatformFaunaSimulation Simulation;
        Initialize(Simulation,Settings,{{{0,0,0},{0,0,1},0},{{50,-30,0},{0,1,1},1}});
        bool bPerched = false;
        for (int Step = 0; Step < 1800 && !bPerched; ++Step)
        {
            Simulation.Step(Settings.FixedStepSeconds);
            for (const auto& Agent : Simulation.GetAgents())
            {
                Require(Agent.FlowerIndex != 0, "零容量花点不得预约");
                if (Agent.Behavior == EBehavior::Perched)
                {
                    bPerched = true;
                    Require(Near(Agent.UpVector.Y,std::sqrt(0.5)) && Near(Agent.UpVector.Z,std::sqrt(0.5)), "非单位花面法线应归一化输出");
                    Require(Near(Agent.Position.Y,-30+Settings.PerchOffset*std::sqrt(0.5),1.e-5), "停栖中心须沿倾斜法线偏移");
                }
            }
        }
        Require(bPerched && Simulation.GetStatistics().OccupiedFlowerSlots == 1, "需要真实占位后验证重置");
        Simulation.Reset();
        Require(Simulation.GetAgents().empty() && Simulation.GetStatistics().OccupiedFlowerSlots == 0, "重置必须释放已占花点");
    });
    Run("Butterfly_FootContactOffset", []
    {
        // 正式蝶模型的脚底位于局部-Z 0.72cm；BodyRadius=7cm仅用于约束，不能作为离花高度。
        auto Settings = MakeSettings(EKind::Butterfly, 8);
        Settings.BodyRadius = 7.0;
        for (const FVector Normal : {FVector{0,0,1}, FVector{0,1,1}})
        {
            const double NormalLength = std::sqrt(DistanceSquared(Normal,{}));
            const FVector UnitNormal = {Normal.X/NormalLength,Normal.Y/NormalLength,Normal.Z/NormalLength};
            const FVector FlowerPosition = {50,-30,-40};
            FGamePlatformFaunaSimulation Simulation;
            Initialize(Simulation,Settings,{{FlowerPosition,Normal,1}});
            bool bPerched = false;
            for (int Step = 0; Step < 1950 && !bPerched; ++Step)
            {
                Simulation.Step(Settings.FixedStepSeconds);
                for (const auto& Agent : Simulation.GetAgents())
                {
                    CheckAgent(Agent,Settings);
                    if (Agent.Behavior != EBehavior::Perched) continue;
                    bPerched = true;
                    const FVector Foot = {Agent.Position.X-UnitNormal.X*0.72,
                        Agent.Position.Y-UnitNormal.Y*0.72,Agent.Position.Z-UnitNormal.Z*0.72};
                    Require(DistanceSquared(Foot,FlowerPosition) < 1.e-10,
                        "停栖时真实模型脚底应贴花面，不能按7cm约束球半径悬空");
                    Require(DistanceSquared(Agent.UpVector,UnitNormal) < 1.e-12,
                        "脚底偏移与停栖姿态必须采用同一单位花面法线");
                }
            }
            Require(bPerched,"必须经过生产状态机真实到达停栖，才能验证脚底接触");
        }
    });
    Run("PerchOffset_Validation_AndBodyClearance", []
    {
        auto Settings = MakeSettings(EKind::Butterfly,8);
        Settings.BodyRadius = 7.0;
        FGamePlatformFaunaSimulation Simulation;
        Initialize(Simulation,Settings);
        const FVector PreviousPosition = Simulation.GetAgents()[0].Position;
        std::string Error;
        for (const double InvalidOffset : {-0.01, 7.01,
            std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        {
            auto Invalid = Settings; Invalid.PerchOffset = InvalidOffset;
            Require(!Simulation.Initialize(Invalid,{}, {}, {},Error) && !Error.empty(),
                "非有限、负数或超过完整约束球的停栖偏移必须拒绝");
            Require(DistanceSquared(Simulation.GetAgents()[0].Position,PreviousPosition) == 0.0,
                "拒绝脚底偏移时不得破坏当前合法群落");
        }
        for (const double ValidOffset : {0.0,0.25,Settings.BodyRadius})
        {
            auto Valid = Settings; Valid.PerchOffset = ValidOffset;
            Initialize(Simulation,Valid,{{{0,0,0},{0,0,1},1}});
        }
        // 中心距离障碍仅10.72cm；半径7cm个体对半径10cm障碍仍需17cm净距。
        Require(!Simulation.Initialize(Settings,{{{0,0,10},{0,0,1},1}},
            {{{0,0,0},10}}, {}, Error),"减小脚底偏移不能缩小个体障碍净空");
        // 地板上的花不能让7cm约束球以仅0.72cm中心高度越过栖息盒下边界。
        Require(!Simulation.Initialize(Settings,{{{0,0,-160},{0,0,1},1}},
            {}, {}, Error),"花站合法性必须继续包含完整BodyRadius盒边界余量");
    });
    std::cout << "FaunaNative: " << Passed << " passed, " << Failed << " failed\n";
    return Failed == 0 ? 0 : 1;
}
