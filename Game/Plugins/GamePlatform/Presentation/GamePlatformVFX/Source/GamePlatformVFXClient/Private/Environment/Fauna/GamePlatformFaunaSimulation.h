// 平台层客户端环境群落：无 UE、网络、Actor、渲染或资源加载依赖的正式 C++17 行为机制。
// 由既有 GamePlatformVFX 世界实例独占并在同一线程调用；本类不创建第二套播放器或服务。
// 输入与输出均相对栖息区中心，以厘米、厘米/秒、秒计；装配层负责世界坐标与 Niagara 传输。
// 不承诺跨编译器/跨平台位级确定性，不持有权威玩法状态；Reset 随实例终止释放全部预约与刺激。
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace GamePlatformFauna
{
// 局部三维向量；坐标单位厘米，作为方向/法线时为无量纲。
struct FVector
{
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
};

// 群落内容类型；不包含任何项目身份、MOBA 规则或资产路径。
enum class EKind : std::uint8_t { Butterfly, Fish };
// 蝴蝶完整停栖状态机与鱼群游动/受惊状态；只影响本地表现。
enum class EBehavior : std::uint8_t { Flying, Approaching, Perched, TakingOff, Fleeing, Swimming };

// 每群配置。非法输入原子拒绝，保留当前合法群落；下面数值界限用于防溢出和限定计算成本。
struct FSettings
{
    EKind Kind = EKind::Butterfly;          // 蝴蝶或鱼群。
    int Count = 32;                       // 个体数，0..512；0 表示有效空群落。
    std::uint32_t Seed = 1;               // 局部伪随机种子；0 也是合法种子。
    FVector HalfExtent = {600, 600, 300};  // 栖息盒半范围，各轴 > BodyRadius，且 <= 1e8 厘米。
    double CruiseSpeed = 120.0;           // 巡航速度，0 < 值 <= 1e7 厘米/秒。
    double BodyRadius = 3.0;              // 约束球半径，1e-5..1e8 厘米。
    double NeighborRadius = 160.0;        // 鱼邻域半径，0 < 值 <= 1e8 厘米。
    double SeparationRadius = 25.0;       // 鱼分离半径，0 < 值 <= NeighborRadius。
    int MaxNeighbors = 16;                // 每体每固定步接受邻居上限，1..32。
    double FixedStepSeconds = 1.0 / 30.0; // 固定步长，1e-5..0.25 秒；每次 Step 最多执行 8 步。
};

// 花面中心及朝外法线；容量 0..512，0 表示禁用。最多 256 花点，总容量 <=4096。
// 槽位在花面切平面排布；处于障碍或边界外的槽位不予预约。
struct FFlower
{
    FVector Position;
    FVector Normal = {0, 0, 1};
    int Capacity = 1;
};

// 静态球体障碍；最多 256 个，半径 0 < 值 <= 1e8 厘米，中心各轴绝对值 <=1e8 厘米。
struct FObstacle
{
    FVector Center;
    double Radius = 50.0;
};

// 只读个体快照；引用有效期截止下一次非 const 调用。无资源所有权或网络身份。
struct FAgent
{
    FVector Position;                    // 栖息区局部中心位置，厘米。
    FVector Velocity;                    // 局部速度，厘米/秒；停栖时为零。
    double AnimationPhase = 0.0;          // 周期相位 [0,1)，由适配层映射扑翼/摆尾。
    double FlightWeight = 1.0;            // 扑翼姿态混合权重 [0,1]，停栖逐渐归零。
    EBehavior Behavior = EBehavior::Flying;
    int FlowerIndex = -1;                // 当前预约的花点索引；-1 表示未占位。
    FVector UpVector = {0, 0, 1};         // 单位上向量；停栖按花面法线，其他状态为局部 Z 上。
};

// CPU 行为诊断；不代表 GPU 或游戏帧耗时。
struct FStatistics
{
    std::uint64_t NeighborChecks = 0;     // 最近一次有效 Step 各子步实际候选距离检查总数。
    std::uint64_t NeighborLinks = 0;      // 最近一次有效 Step 接受的邻居总数。
    std::uint32_t OccupiedFlowerSlots = 0;// 当前预约总数，包括接近和停栖。
    std::uint32_t FixedSteps = 0;         // 最近一次有效 Step 实际子步数，0..8。
    std::uint64_t TotalFixedSteps = 0;    // Initialize/Reset 后累计固定步数。
    double DroppedSeconds = 0.0;          // 因 8 子步上限丢弃的累计时间，秒；不补偿为快速移动。
};

class FGamePlatformFaunaSimulation
{
public:
    // 校验配置、简单 XY 多边形及出生点，在候选实例完整成功后替换当前实例。
    // 多边形为空使用盒体；鱼额外受多边形内部及 BodyRadius 岸线距离约束；蝶忽略水域。
    // 位置有界采样失败返回 false 和中文错误；不把采样失败伪称为数学无解。
    bool Initialize(const FSettings& InSettings, const std::vector<FFlower>& InFlowers,
        const std::vector<FObstacle>& InObstacles, const std::vector<FVector>& WaterPolygon,
        std::string& OutError);

    // 累积正且有限的时间并执行固定步；非法/零时间无副作用，长帧最多 8 步。
    // 鱼同一步从同一位置/速度快照计算，用空间网格与有限候选预算避免全体两两扫描。
    void Step(double DeltaSeconds);

    // 替换一个临时局部刺激：半径 (0,1e8] 厘米，强度 [0,1]，持续 (0,3600] 秒。
    // 强度 0 清除刺激；非法输入不覆盖已有刺激。有效刺激立即释放范围内蝴蝶的预约。
    bool SetDisturbance(const FVector& Center, double Radius, double Strength,
        double DurationSeconds, std::string& OutError);

    // 清空个体、预约、随机序列、诊断和刺激；之后 Step 不会重新生成个体。
    void Reset();
    const std::vector<FAgent>& GetAgents() const { return Agents; }
    const FStatistics& GetStatistics() const { return Statistics; }

private:
    // 与个体同索引的内部记忆；每体独立随机流避免查询顺序改变其他个体的随机决策。
    struct FAgentMemory
    {
        std::uint32_t RandomState = 1;
        FVector Goal;
        double StateSeconds = 0.0;         // 当前状态剩余时间，秒。
        double DecisionSeconds = 0.0;      // 下次游飞/择花决策等待，秒。
        double AnimationRate = 1.0;        // 周期/秒；每体初始化后固定。
        int FlowerSlot = -1;              // 花点内预约槽位；-1 为无。
    };
    // 空间网格只做按键查找，从不依赖哈希表枚举顺序。
    struct FCell
    {
        std::int64_t X = 0, Y = 0, Z = 0;
        bool operator==(const FCell& Other) const { return X == Other.X && Y == Other.Y && Z == Other.Z; }
    };
    struct FCellHash { std::size_t operator()(const FCell& Cell) const; };
    struct FDisturbance
    {
        FVector Center;
        double Radius = 0.0;
        double Strength = 0.0;
        double RemainingSeconds = 0.0;
    };

    // 全部容器由本群落实例独占；没有全局可变状态或外部 UObject 指针。
    FSettings Settings;
    std::vector<FFlower> Flowers;
    std::vector<FObstacle> Obstacles;
    std::vector<FVector> Polygon;
    std::vector<FAgent> Agents;
    std::vector<FAgent> NextAgents;         // 鱼双缓冲；当前步不读已更新邻居。
    std::vector<FAgentMemory> Memory;
    std::vector<std::vector<int>> FlowerOwners; // 槽位 -> 个体索引，-1 为空；只有本实例能写。
    std::unordered_map<FCell, std::vector<int>, FCellHash> Grid;
    FStatistics Statistics;
    FDisturbance Disturbance;
    double AccumulatorSeconds = 0.0;
    double GridCellSize = 1.0;
    bool bInitialized = false;

    bool Validate(std::string& OutError) const;
    bool IsPositionLegal(const FVector& Position) const;
    bool IsSweepLegal(const FVector& From, const FVector& To) const;
    bool SampleLegalPosition(std::uint32_t& RandomState, FVector& OutPosition, const FVector* Around = nullptr) const;
    FVector ConstrainMotion(const FVector& From, const FVector& Displacement) const;
    FVector BoundarySteering(const FVector& Position) const;
    FVector FlowerSlotPosition(int FlowerIndex, int Slot) const;
    bool ReserveFlower(int AgentIndex);
    void ReleaseFlower(int AgentIndex);
    void ApplyDisturbance(int AgentIndex);
    void StepFixed();
    void StepButterflies();
    void StepFish();
    FCell CellFor(const FVector& Position) const;
};
}
