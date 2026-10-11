// GamePlatformVFX 内部正式 CPU 群落行为；实现合同见同目录头文件。
// 仅计算本地表现状态，不加载资源、不生成 Actor、不参与服务器权威玩法。
// 采用有界固定步、每体随机流、花点独占预约、鱼邻域双缓冲及连续扫掠约束。
#include "GamePlatformFaunaSimulation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace GamePlatformFauna
{
namespace
{
constexpr double Pi = 3.14159265358979323846;
// 相对平方距离容差随几何尺度缩放，避免小球半径被固定绝对容差整体吞掉。
constexpr double GeometryRelativeTolerance = 1.e-12;
constexpr double CoordinateLimit = 1.e8;
constexpr int MaximumSubsteps = 8;

// 向量运算保持独立于 UE 类型，便于同一生产源在原生编译器和模块内验证。
FVector Add(const FVector& A, const FVector& B) { return {A.X+B.X, A.Y+B.Y, A.Z+B.Z}; }
FVector Subtract(const FVector& A, const FVector& B) { return {A.X-B.X, A.Y-B.Y, A.Z-B.Z}; }
FVector Scale(const FVector& A, double Value) { return {A.X*Value, A.Y*Value, A.Z*Value}; }
double Dot(const FVector& A, const FVector& B) { return A.X*B.X+A.Y*B.Y+A.Z*B.Z; }
double LengthSquared(const FVector& A) { return Dot(A, A); }
double Length(const FVector& A) { return std::sqrt(LengthSquared(A)); }
FVector Normalize(const FVector& A, const FVector& Fallback = {1,0,0})
{
    const double Magnitude = Length(A);
    return Magnitude > 1.e-12 ? Scale(A, 1.0 / Magnitude) : Fallback;
}
FVector Cross(const FVector& A, const FVector& B)
{
    return {A.Y*B.Z-A.Z*B.Y, A.Z*B.X-A.X*B.Z, A.X*B.Y-A.Y*B.X};
}
FVector LimitLength(const FVector& A, double Maximum)
{
    const double Magnitude = Length(A);
    return Magnitude > Maximum ? Scale(A, Maximum / Magnitude) : A;
}
bool FiniteVector(const FVector& A)
{
    return std::isfinite(A.X) && std::isfinite(A.Y) && std::isfinite(A.Z);
}
bool BoundedVector(const FVector& A)
{
    return FiniteVector(A) && std::abs(A.X) <= CoordinateLimit && std::abs(A.Y) <= CoordinateLimit && std::abs(A.Z) <= CoordinateLimit;
}
bool PositiveFinite(double Value, double Maximum = CoordinateLimit)
{
    return std::isfinite(Value) && Value > 0.0 && Value <= Maximum;
}

// 固定整型混合与 xorshift 避免共享随机源；不声称浮点三角函数跨平台位级相同。
std::uint32_t Mix(std::uint32_t Value)
{
    Value ^= Value >> 16; Value *= 0x7feb352du; Value ^= Value >> 15;
    Value *= 0x846ca68bu; Value ^= Value >> 16;
    return Value != 0 ? Value : 0x9e3779b9u;
}
double Random01(std::uint32_t& State)
{
    State ^= State << 13; State ^= State >> 17; State ^= State << 5;
    return static_cast<double>(State >> 8) / 16777216.0;
}
FVector RandomDirection(std::uint32_t& State)
{
    const double Z = Random01(State) * 2.0 - 1.0;
    const double Angle = Random01(State) * 2.0 * Pi;
    const double Radial = std::sqrt(std::max(0.0, 1.0 - Z*Z));
    return {Radial*std::cos(Angle), Radial*std::sin(Angle), Z};
}
FVector ClosestPoint(const FVector& Point, const FVector& A, const FVector& B)
{
    const FVector Edge = Subtract(B, A);
    const double Denominator = LengthSquared(Edge);
    const double T = Denominator > 1.e-20 ? std::clamp(Dot(Subtract(Point, A), Edge) / Denominator, 0.0, 1.0) : 0.0;
    return Add(A, Scale(Edge, T));
}
FVector XY(const FVector& Value) { return {Value.X, Value.Y, 0.0}; }
double CrossXY(const FVector& A, const FVector& B, const FVector& C)
{
    return (B.X-A.X)*(C.Y-A.Y) - (B.Y-A.Y)*(C.X-A.X);
}
// 包含共线接触，用于拒绝自交多边形及连续路径穿岸；相邻顶点由调用方排除。
bool SegmentsIntersectXY(const FVector& A, const FVector& B, const FVector& C, const FVector& D)
{
    const double AB_C = CrossXY(A, B, C), AB_D = CrossXY(A, B, D);
    const double CD_A = CrossXY(C, D, A), CD_B = CrossXY(C, D, B);
    if (((AB_C > 0 && AB_D < 0) || (AB_C < 0 && AB_D > 0)) &&
        ((CD_A > 0 && CD_B < 0) || (CD_A < 0 && CD_B > 0))) { return true; }
    const auto OnSegment = [](const FVector& Point, const FVector& Start, const FVector& End)
    {
        return std::abs(CrossXY(Start, End, Point)) <= 1.e-8 &&
            Point.X >= std::min(Start.X, End.X)-1.e-8 && Point.X <= std::max(Start.X, End.X)+1.e-8 &&
            Point.Y >= std::min(Start.Y, End.Y)-1.e-8 && Point.Y <= std::max(Start.Y, End.Y)+1.e-8;
    };
    return OnSegment(C,A,B) || OnSegment(D,A,B) || OnSegment(A,C,D) || OnSegment(B,C,D);
}
double SegmentDistanceSquaredXY(const FVector& A, const FVector& B, const FVector& C, const FVector& D)
{
    if (SegmentsIntersectXY(A,B,C,D)) { return 0.0; }
    const FVector A2 = XY(A), B2 = XY(B), C2 = XY(C), D2 = XY(D);
    return std::min({LengthSquared(Subtract(A2, ClosestPoint(A2,C2,D2))),
        LengthSquared(Subtract(B2, ClosestPoint(B2,C2,D2))),
        LengthSquared(Subtract(C2, ClosestPoint(C2,A2,B2))),
        LengthSquared(Subtract(D2, ClosestPoint(D2,A2,B2)))});
}
// 绕向无关的奇偶规则；岸线安全距离由独立检查处理。
bool InsidePolygonXY(const FVector& Point, const std::vector<FVector>& Vertices)
{
    bool bInside = false;
    for (std::size_t I = 0, J = Vertices.size()-1; I < Vertices.size(); J = I++)
    {
        const auto& A = Vertices[I]; const auto& B = Vertices[J];
        if ((A.Y > Point.Y) != (B.Y > Point.Y) &&
            Point.X < (B.X-A.X)*(Point.Y-A.Y)/(B.Y-A.Y)+A.X) { bInside = !bInside; }
    }
    return bInside;
}
}

std::size_t FGamePlatformFaunaSimulation::FCellHash::operator()(const FCell& Cell) const
{
    const auto X = static_cast<std::uint64_t>(Cell.X), Y = static_cast<std::uint64_t>(Cell.Y), Z = static_cast<std::uint64_t>(Cell.Z);
    return static_cast<std::size_t>((X*0x9e3779b185ebca87ull) ^ (Y*0xc2b2ae3d27d4eb4full) ^ (Z*0x165667b19e3779f9ull));
}

bool FGamePlatformFaunaSimulation::Validate(std::string& OutError) const
{
    const auto Fail = [&OutError](const char* Message) { OutError = Message; return false; };
    if (Settings.Kind != EKind::Butterfly && Settings.Kind != EKind::Fish) { return Fail("未知群落种类。"); }
    if (Settings.Count < 0 || Settings.Count > 512) { return Fail("群落数量必须在 0..512。"); }
    if (!BoundedVector(Settings.HalfExtent) || !PositiveFinite(Settings.BodyRadius) || Settings.BodyRadius < 1.e-5 ||
        Settings.HalfExtent.X <= Settings.BodyRadius+1.e-5 || Settings.HalfExtent.Y <= Settings.BodyRadius+1.e-5 ||
        Settings.HalfExtent.Z <= Settings.BodyRadius+1.e-5) { return Fail("栖息半范围必须有限且大于个体半径。"); }
    if (!PositiveFinite(Settings.CruiseSpeed,1.e7) || !PositiveFinite(Settings.NeighborRadius) ||
        !PositiveFinite(Settings.SeparationRadius) || Settings.SeparationRadius > Settings.NeighborRadius) { return Fail("速度与邻域参数必须为合法正有限数。"); }
    if (Settings.MaxNeighbors < 1 || Settings.MaxNeighbors > 32 ||
        !PositiveFinite(Settings.FixedStepSeconds,0.25) || Settings.FixedStepSeconds < 1.e-5) { return Fail("邻居上限或固定步长超出约束。"); }
    if (Flowers.size() > 256 || Obstacles.size() > 256 || Polygon.size() > 128) { return Fail("花点、障碍或水域顶点超过有界处理容量。"); }
    for (const auto& Obstacle : Obstacles)
        if (!BoundedVector(Obstacle.Center) || !PositiveFinite(Obstacle.Radius)) { return Fail("障碍球中心或半径无效。"); }
    if (!Polygon.empty())
    {
        if (Polygon.size() < 3) { return Fail("水域至少需要三个不同顶点。"); }
        double TwiceArea = 0.0;
        for (std::size_t I = 0; I < Polygon.size(); ++I)
        {
            const auto& A = Polygon[I]; const auto& B = Polygon[(I+1)%Polygon.size()];
            if (!BoundedVector(A) || std::abs(A.X) > Settings.HalfExtent.X || std::abs(A.Y) > Settings.HalfExtent.Y ||
                LengthSquared(Subtract(XY(A),XY(B))) <= 1.e-12) { return Fail("水域顶点必须有限、位于栖息盒内且无零长度边。"); }
            TwiceArea += A.X*B.Y - B.X*A.Y;
            for (std::size_t J = I+1; J < Polygon.size(); ++J)
            {
                if (J == I+1 || (I == 0 && J+1 == Polygon.size())) { continue; }
                if (SegmentsIntersectXY(A,B,Polygon[J],Polygon[(J+1)%Polygon.size()])) { return Fail("水域必须为不自交的简单多边形。"); }
            }
        }
        if (std::abs(TwiceArea) <= 1.e-6) { return Fail("水域多边形面积为零。"); }
    }
    int TotalCapacity = 0;
    for (const auto& Flower : Flowers)
    {
        if (!BoundedVector(Flower.Position) || !BoundedVector(Flower.Normal) || LengthSquared(Flower.Normal) <= 1.e-12 ||
            Flower.Capacity < 0 || Flower.Capacity > 512) { return Fail("花点位置、法线或容量无效。"); }
        TotalCapacity += Flower.Capacity;
        if (TotalCapacity > 4096) { return Fail("花点总容量超过 4096。"); }
        if (Flower.Capacity > 0 && !IsPositionLegal(Add(Flower.Position, Scale(Normalize(Flower.Normal), Settings.BodyRadius))))
            { return Fail("花点停栖中心位于边界或障碍之外。"); }
    }
    return true;
}

bool FGamePlatformFaunaSimulation::Initialize(const FSettings& InSettings, const std::vector<FFlower>& InFlowers,
    const std::vector<FObstacle>& InObstacles, const std::vector<FVector>& WaterPolygon, std::string& OutError)
{
    // 先拒绝超限集合再复制，避免非法输入造成不必要的大容量分配。
    if (InFlowers.size() > 256 || InObstacles.size() > 256 || WaterPolygon.size() > 128)
        { OutError = "花点、障碍或水域顶点超过有界处理容量。"; return false; }
    // 所有校验、采样和容量分配只修改候选；包括失败随机采样在内均不污染旧实例。
    FGamePlatformFaunaSimulation Candidate;
    Candidate.Settings = InSettings; Candidate.Flowers = InFlowers;
    Candidate.Obstacles = InObstacles; Candidate.Polygon = WaterPolygon;
    if (!Candidate.Validate(OutError)) { return false; }
    for (auto& Flower : Candidate.Flowers) { Flower.Normal = Normalize(Flower.Normal); }
    for (const auto& Flower : Candidate.Flowers) { Candidate.FlowerOwners.emplace_back(static_cast<std::size_t>(Flower.Capacity), -1); }
    Candidate.Agents.resize(static_cast<std::size_t>(InSettings.Count));
    Candidate.NextAgents.resize(Candidate.Agents.size());
    Candidate.Memory.resize(Candidate.Agents.size());
    Candidate.Grid.reserve(Candidate.Agents.size());
    Candidate.GridCellSize = std::max({InSettings.NeighborRadius, InSettings.BodyRadius*2.0,
        std::max({InSettings.HalfExtent.X,InSettings.HalfExtent.Y,InSettings.HalfExtent.Z})*1.e-6,1.e-3});
    FVector SchoolCenter;
    std::uint32_t CenterState = Mix(InSettings.Seed ^ 0x4f1bbcdcu);
    if (InSettings.Count > 0 && !Candidate.SampleLegalPosition(CenterState,SchoolCenter))
        { OutError = "在有界采样预算内找不到合法出生位置。"; return false; }
    for (int I = 0; I < InSettings.Count; ++I)
    {
        auto& Agent = Candidate.Agents[static_cast<std::size_t>(I)];
        auto& State = Candidate.Memory[static_cast<std::size_t>(I)];
        State.RandomState = Mix(InSettings.Seed ^ Mix(static_cast<std::uint32_t>(I)+1u));
        const FVector* Around = InSettings.Kind == EKind::Fish ? &SchoolCenter : nullptr;
        if (!Candidate.SampleLegalPosition(State.RandomState,Agent.Position,Around))
            { OutError = "在有界采样预算内无法为所有个体找到合法出生位置。"; return false; }
        State.Goal = Agent.Position;
        Candidate.SampleLegalPosition(State.RandomState,State.Goal);
        FVector Direction = RandomDirection(State.RandomState);
        if (InSettings.Kind == EKind::Fish) { Direction = Normalize(Add(Scale(Direction,0.35),{1,0,0})); }
        Agent.Velocity = Scale(Direction,InSettings.CruiseSpeed);
        Agent.Behavior = InSettings.Kind == EKind::Fish ? EBehavior::Swimming : EBehavior::Flying;
        Agent.AnimationPhase = Random01(State.RandomState);
        State.AnimationRate = (InSettings.Kind == EKind::Fish ? 1.5 : 5.0) * (0.8+Random01(State.RandomState)*0.4);
        State.DecisionSeconds = Random01(State.RandomState)*1.5;
    }
    Candidate.bInitialized = true;
    *this = std::move(Candidate);
    OutError.clear();
    return true;
}

bool FGamePlatformFaunaSimulation::IsPositionLegal(const FVector& Position) const
{
    const auto Exceeds = [](double Value, double Limit)
    {
        const double Tolerance = std::max(std::abs(Value),std::abs(Limit))*std::numeric_limits<double>::epsilon()*8.0;
        return std::abs(Value) > Limit+Tolerance;
    };
    if (!FiniteVector(Position) || Exceeds(Position.X,Settings.HalfExtent.X-Settings.BodyRadius) ||
        Exceeds(Position.Y,Settings.HalfExtent.Y-Settings.BodyRadius) ||
        Exceeds(Position.Z,Settings.HalfExtent.Z-Settings.BodyRadius)) { return false; }
    for (const auto& Obstacle : Obstacles)
    {
        const double Radius = Obstacle.Radius + Settings.BodyRadius;
        if (LengthSquared(Subtract(Position,Obstacle.Center)) < Radius*Radius*(1.0-GeometryRelativeTolerance)) { return false; }
    }
    if (Settings.Kind == EKind::Fish && !Polygon.empty())
    {
        if (!InsidePolygonXY(Position,Polygon)) { return false; }
        for (std::size_t I = 0; I < Polygon.size(); ++I)
            if (LengthSquared(Subtract(XY(Position),ClosestPoint(XY(Position),XY(Polygon[I]),XY(Polygon[(I+1)%Polygon.size()])))) <
                Settings.BodyRadius*Settings.BodyRadius*(1.0-GeometryRelativeTolerance)) { return false; }
    }
    return true;
}

bool FGamePlatformFaunaSimulation::IsSweepLegal(const FVector& From, const FVector& To) const
{
    if (!IsPositionLegal(To)) { return false; }
    for (const auto& Obstacle : Obstacles)
    {
        const double Radius = Obstacle.Radius + Settings.BodyRadius;
        if (LengthSquared(Subtract(Obstacle.Center,ClosestPoint(Obstacle.Center,From,To))) < Radius*Radius*(1.0-GeometryRelativeTolerance)) { return false; }
    }
    if (Settings.Kind == EKind::Fish && !Polygon.empty())
        for (std::size_t I = 0; I < Polygon.size(); ++I)
            if (SegmentDistanceSquaredXY(From,To,Polygon[I],Polygon[(I+1)%Polygon.size()]) <
                Settings.BodyRadius*Settings.BodyRadius*(1.0-GeometryRelativeTolerance)) { return false; }
    return true;
}

bool FGamePlatformFaunaSimulation::SampleLegalPosition(std::uint32_t& RandomState, FVector& OutPosition, const FVector* Around) const
{
    const FVector Limit = {Settings.HalfExtent.X-Settings.BodyRadius,Settings.HalfExtent.Y-Settings.BodyRadius,Settings.HalfExtent.Z-Settings.BodyRadius};
    for (int Attempt = 0; Attempt < 512; ++Attempt)
    {
        FVector Position;
        if (Around && Attempt < 192)
            { Position = Add(*Around,Scale(RandomDirection(RandomState),Settings.NeighborRadius*0.45*std::cbrt(Random01(RandomState)))); }
        else
            { Position = {(Random01(RandomState)*2-1)*Limit.X,(Random01(RandomState)*2-1)*Limit.Y,(Random01(RandomState)*2-1)*Limit.Z}; }
        if (IsPositionLegal(Position)) { OutPosition = Position; return true; }
    }
    // 有界网格兜底提高狭窄空间命中机会；失败保留输出且明确交给初始化/决策调用方。
    for (int Z = 0; Z < 5; ++Z) for (int Y = 0; Y < 7; ++Y) for (int X = 0; X < 7; ++X)
    {
        const FVector Position = {(static_cast<double>(X)/3.0-1.0)*Limit.X*0.95,
            (static_cast<double>(Y)/3.0-1.0)*Limit.Y*0.95,(static_cast<double>(Z)/2.0-1.0)*Limit.Z*0.95};
        if (IsPositionLegal(Position)) { OutPosition = Position; return true; }
    }
    return false;
}

FVector FGamePlatformFaunaSimulation::ConstrainMotion(const FVector& From, const FVector& Displacement) const
{
    const auto ClampBox = [this](const FVector& Position)
    {
        return FVector{std::clamp(Position.X,-Settings.HalfExtent.X+Settings.BodyRadius,Settings.HalfExtent.X-Settings.BodyRadius),
            std::clamp(Position.Y,-Settings.HalfExtent.Y+Settings.BodyRadius,Settings.HalfExtent.Y-Settings.BodyRadius),
            std::clamp(Position.Z,-Settings.HalfExtent.Z+Settings.BodyRadius,Settings.HalfExtent.Z-Settings.BodyRadius)};
    };
    const FVector Desired = ClampBox(Add(From,Displacement));
    if (IsSweepLegal(From,Desired)) { return Desired; }
    // 尝试切向滑动仍检查整条路径；不能以合法终点掩盖高速穿岛或跨过凹水域缺口。
    const std::array<FVector,6> Alternatives = {{
        {Displacement.X,Displacement.Y,0},{Displacement.X,0,Displacement.Z},{0,Displacement.Y,Displacement.Z},
        {Displacement.X,0,0},{0,Displacement.Y,0},{0,0,Displacement.Z}}};
    FVector Best = From; double BestDistance = 0.0;
    for (const auto& Alternative : Alternatives)
    {
        const FVector Candidate = ClampBox(Add(From,Alternative));
        const double Distance = LengthSquared(Subtract(Candidate,From));
        if (Distance > BestDistance && IsSweepLegal(From,Candidate)) { Best = Candidate; BestDistance = Distance; }
    }
    if (BestDistance > 1.e-10) { return Best; }
    // 可行前缀的单调二分，而非只检查中点位置；前缀一旦碰撞，后续都不可接受。
    double Low = 0.0, High = 1.0;
    const FVector Delta = Subtract(Desired,From);
    for (int Iteration = 0; Iteration < 20; ++Iteration)
    {
        const double Middle = (Low+High)*0.5;
        if (IsSweepLegal(From,Add(From,Scale(Delta,Middle)))) { Low = Middle; }
        else { High = Middle; }
    }
    return Add(From,Scale(Delta,Low));
}

FVector FGamePlatformFaunaSimulation::BoundarySteering(const FVector& Position) const
{
    FVector Force;
    const double Margin = std::max(Settings.BodyRadius*4.0,Settings.CruiseSpeed*0.6);
    const auto AxisForce = [Margin](double PositionAxis, double Limit)
    {
        const double Distance = Limit-std::abs(PositionAxis);
        return Distance < Margin ? (PositionAxis < 0 ? 1.0 : -1.0)*(1.0-std::max(0.0,Distance)/Margin) : 0.0;
    };
    Force = {AxisForce(Position.X,Settings.HalfExtent.X-Settings.BodyRadius),
        AxisForce(Position.Y,Settings.HalfExtent.Y-Settings.BodyRadius),
        AxisForce(Position.Z,Settings.HalfExtent.Z-Settings.BodyRadius)};
    for (const auto& Obstacle : Obstacles)
    {
        const FVector Away = Subtract(Position,Obstacle.Center);
        const double Gap = Length(Away)-Obstacle.Radius-Settings.BodyRadius;
        if (Gap < Margin) { Force = Add(Force,Scale(Normalize(Away),1.5*(1.0-std::clamp(Gap/Margin,0.0,1.0)))); }
    }
    if (Settings.Kind == EKind::Fish && !Polygon.empty())
        for (std::size_t I = 0; I < Polygon.size(); ++I)
        {
            const FVector Closest = ClosestPoint(XY(Position),XY(Polygon[I]),XY(Polygon[(I+1)%Polygon.size()]));
            const FVector Away = Subtract(XY(Position),Closest);
            const double Gap = Length(Away)-Settings.BodyRadius;
            if (Gap < Margin) { Force = Add(Force,Scale(Normalize(Away),1.0-std::clamp(Gap/Margin,0.0,1.0))); }
        }
    return Force;
}

FVector FGamePlatformFaunaSimulation::FlowerSlotPosition(int FlowerIndex, int Slot) const
{
    const auto& Flower = Flowers[static_cast<std::size_t>(FlowerIndex)];
    const FVector Base = Add(Flower.Position,Scale(Flower.Normal,Settings.BodyRadius));
    if (Flower.Capacity <= 1) { return Base; }
    const int Columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(Flower.Capacity))));
    const int Rows = (Flower.Capacity+Columns-1)/Columns;
    const FVector Tangent = Normalize(Cross(Flower.Normal,std::abs(Flower.Normal.Z)<0.9 ? FVector{0,0,1} : FVector{0,1,0}));
    const FVector Bitangent = Cross(Flower.Normal,Tangent);
    const double Spacing = Settings.BodyRadius*2.2;
    return Add(Base,Add(Scale(Tangent,(Slot%Columns-(Columns-1)*0.5)*Spacing),
        Scale(Bitangent,(Slot/Columns-(Rows-1)*0.5)*Spacing)));
}

bool FGamePlatformFaunaSimulation::ReserveFlower(int AgentIndex)
{
    auto& Agent = Agents[static_cast<std::size_t>(AgentIndex)];
    auto& State = Memory[static_cast<std::size_t>(AgentIndex)];
    int BestFlower = -1, BestSlot = -1;
    double BestDistance = std::numeric_limits<double>::max();
    for (std::size_t F = 0; F < FlowerOwners.size(); ++F)
        for (std::size_t Slot = 0; Slot < FlowerOwners[F].size(); ++Slot)
        {
            if (FlowerOwners[F][Slot] >= 0) { continue; }
            const FVector Target = FlowerSlotPosition(static_cast<int>(F),static_cast<int>(Slot));
            const double Distance = LengthSquared(Subtract(Target,Agent.Position));
            if (Distance < BestDistance && IsPositionLegal(Target))
                { BestDistance = Distance; BestFlower = static_cast<int>(F); BestSlot = static_cast<int>(Slot); }
        }
    if (BestFlower < 0) { return false; }
    FlowerOwners[static_cast<std::size_t>(BestFlower)][static_cast<std::size_t>(BestSlot)] = AgentIndex;
    Agent.FlowerIndex = BestFlower; State.FlowerSlot = BestSlot; ++Statistics.OccupiedFlowerSlots;
    State.Goal = FlowerSlotPosition(BestFlower,BestSlot);
    Agent.Behavior = EBehavior::Approaching;
    State.StateSeconds = 8.0 + std::sqrt(BestDistance)/Settings.CruiseSpeed*4.0;
    return true;
}

void FGamePlatformFaunaSimulation::ReleaseFlower(int AgentIndex)
{
    auto& Agent = Agents[static_cast<std::size_t>(AgentIndex)];
    auto& State = Memory[static_cast<std::size_t>(AgentIndex)];
    if (Agent.FlowerIndex >= 0 && State.FlowerSlot >= 0)
    {
        auto& Owner = FlowerOwners[static_cast<std::size_t>(Agent.FlowerIndex)][static_cast<std::size_t>(State.FlowerSlot)];
        if (Owner == AgentIndex) { Owner = -1; if (Statistics.OccupiedFlowerSlots > 0) { --Statistics.OccupiedFlowerSlots; } }
    }
    Agent.FlowerIndex = -1; State.FlowerSlot = -1;
}

void FGamePlatformFaunaSimulation::ApplyDisturbance(int AgentIndex)
{
    auto& Agent = Agents[static_cast<std::size_t>(AgentIndex)];
    if (Disturbance.RemainingSeconds <= 0.0 || Disturbance.Strength <= 0.0 ||
        LengthSquared(Subtract(Agent.Position,Disturbance.Center)) > Disturbance.Radius*Disturbance.Radius) { return; }
    ReleaseFlower(AgentIndex);
    Agent.Behavior = EBehavior::Fleeing;
    Memory[static_cast<std::size_t>(AgentIndex)].StateSeconds = 0.75;
}

bool FGamePlatformFaunaSimulation::SetDisturbance(const FVector& Center, double Radius, double Strength,
    double DurationSeconds, std::string& OutError)
{
    if (!bInitialized || !BoundedVector(Center) || !PositiveFinite(Radius) ||
        !std::isfinite(Strength) || Strength < 0.0 || Strength > 1.0 || !PositiveFinite(DurationSeconds,3600.0))
        { OutError = "刺激要求有效群落、有限中心、正半径、0..1 强度和 0..3600 秒内正持续时间。"; return false; }
    Disturbance = {Center,Radius,Strength,Strength > 0.0 ? DurationSeconds : 0.0};
    for (int I = 0; I < Settings.Count; ++I) { ApplyDisturbance(I); }
    OutError.clear();
    return true;
}

void FGamePlatformFaunaSimulation::Step(double DeltaSeconds)
{
    if (!bInitialized || !std::isfinite(DeltaSeconds) || DeltaSeconds <= 0.0) { return; }
    Statistics.NeighborChecks = 0; Statistics.NeighborLinks = 0; Statistics.FixedSteps = 0;
    const double MaximumCatchup = Settings.FixedStepSeconds*MaximumSubsteps;
    const double Accepted = std::min(DeltaSeconds,MaximumCatchup);
    // 累计统计饱和，避免调用者反复传入极端有限时间使诊断本身溢出。
    Statistics.DroppedSeconds = std::min(std::numeric_limits<double>::max()/2.0,
        Statistics.DroppedSeconds + std::min(DeltaSeconds-Accepted,std::numeric_limits<double>::max()/4.0));
    AccumulatorSeconds += Accepted;
    while (AccumulatorSeconds+Settings.FixedStepSeconds*1.e-9 >= Settings.FixedStepSeconds && Statistics.FixedSteps < MaximumSubsteps)
    {
        StepFixed(); AccumulatorSeconds = std::max(0.0,AccumulatorSeconds-Settings.FixedStepSeconds);
        ++Statistics.FixedSteps; ++Statistics.TotalFixedSteps;
    }
}

void FGamePlatformFaunaSimulation::StepFixed()
{
    for (int I = 0; I < Settings.Count; ++I) { ApplyDisturbance(I); }
    if (Settings.Kind == EKind::Butterfly) { StepButterflies(); }
    else { StepFish(); }
    Disturbance.RemainingSeconds = std::max(0.0,Disturbance.RemainingSeconds-Settings.FixedStepSeconds);
}

void FGamePlatformFaunaSimulation::StepButterflies()
{
    const double Delta = Settings.FixedStepSeconds;
    for (int I = 0; I < Settings.Count; ++I)
    {
        auto& Agent = Agents[static_cast<std::size_t>(I)];
        auto& State = Memory[static_cast<std::size_t>(I)];
        State.StateSeconds -= Delta; State.DecisionSeconds -= Delta;
        if (Agent.Behavior == EBehavior::Fleeing && State.StateSeconds <= 0.0)
            { Agent.Behavior = EBehavior::Flying; State.DecisionSeconds = 1.5; if (!SampleLegalPosition(State.RandomState,State.Goal)) { State.Goal = Agent.Position; } }
        if (Agent.Behavior == EBehavior::Perched)
        {
            Agent.Position = FlowerSlotPosition(Agent.FlowerIndex,State.FlowerSlot);
            Agent.Velocity = {};
            Agent.UpVector = Flowers[static_cast<std::size_t>(Agent.FlowerIndex)].Normal;
            if (State.StateSeconds <= 0.0)
            {
                const FVector Normal = Agent.UpVector;
                ReleaseFlower(I); Agent.Behavior = EBehavior::TakingOff; State.StateSeconds = 0.85;
                State.Goal = Add(Agent.Position,Scale(Normal,std::max(40.0,Settings.CruiseSpeed*0.65)));
                Agent.Velocity = Scale(Normal,Settings.CruiseSpeed*0.5);
            }
        }
        if (Agent.Behavior == EBehavior::Approaching && State.StateSeconds <= 0.0)
            { ReleaseFlower(I); Agent.Behavior = EBehavior::Flying; State.DecisionSeconds = 2.0; if (!SampleLegalPosition(State.RandomState,State.Goal)) { State.Goal = Agent.Position; } }
        if (Agent.Behavior == EBehavior::TakingOff && State.StateSeconds <= 0.0)
            { Agent.Behavior = EBehavior::Flying; State.DecisionSeconds = 1.0; if (!SampleLegalPosition(State.RandomState,State.Goal)) { State.Goal = Agent.Position; } }
        if (Agent.Behavior == EBehavior::Flying && State.DecisionSeconds <= 0.0)
        {
            if (!ReserveFlower(I)) { if (!SampleLegalPosition(State.RandomState,State.Goal)) { State.Goal = Agent.Position; } }
            State.DecisionSeconds = 2.0+Random01(State.RandomState)*2.0;
        }
        if (Agent.Behavior != EBehavior::Perched)
        {
            Agent.UpVector = {0,0,1};
            FVector DesiredVelocity;
            if (Agent.Behavior == EBehavior::Fleeing)
            {
                const FVector Away = Normalize(Subtract(Agent.Position,Disturbance.Center),RandomDirection(State.RandomState));
                DesiredVelocity = Scale(Normalize(Add(Away,{0,0,0.35})),Settings.CruiseSpeed*(1.0+Disturbance.Strength));
                DesiredVelocity = Add(DesiredVelocity,Scale(BoundarySteering(Agent.Position),Settings.CruiseSpeed*2.0));
            }
            else
            {
                const FVector Toward = Subtract(State.Goal,Agent.Position);
                const double Distance = Length(Toward);
                const double Speed = Agent.Behavior == EBehavior::Approaching ? std::min(Settings.CruiseSpeed,Distance*2.5) : Settings.CruiseSpeed;
                DesiredVelocity = Scale(Normalize(Toward),Speed);
                // 接近花点使用阻尼直达，游飞则加入低幅度连续摆动；避免末段随机力永远无法停栖。
                if (Agent.Behavior == EBehavior::Flying)
                {
                    const double Phase = Agent.AnimationPhase*2.0*Pi;
                    DesiredVelocity = Add(DesiredVelocity,{std::sin(Phase)*Speed*0.12,std::cos(Phase)*Speed*0.12,std::sin(Phase*0.5)*Speed*0.15});
                    DesiredVelocity = Add(DesiredVelocity,Scale(BoundarySteering(Agent.Position),Settings.CruiseSpeed*2.0));
                }
            }
            Agent.Velocity = Add(Agent.Velocity,LimitLength(Subtract(DesiredVelocity,Agent.Velocity),Settings.CruiseSpeed*5.0*Delta));
            Agent.Velocity = LimitLength(Agent.Velocity,Settings.CruiseSpeed*2.5);
            const FVector Previous = Agent.Position;
            Agent.Position = ConstrainMotion(Previous,Scale(Agent.Velocity,Delta));
            if (LengthSquared(Subtract(Agent.Position,Previous)) < 1.e-10 && Agent.Behavior == EBehavior::Flying)
                { State.DecisionSeconds = 0.0; }
            if (Agent.Behavior == EBehavior::Approaching && LengthSquared(Subtract(Agent.Position,State.Goal)) <= std::pow(std::max(0.5,Settings.BodyRadius*0.5),2) &&
                IsSweepLegal(Agent.Position,State.Goal))
            {
                Agent.Position = State.Goal; Agent.Velocity = {}; Agent.Behavior = EBehavior::Perched;
                Agent.UpVector = Flowers[static_cast<std::size_t>(Agent.FlowerIndex)].Normal;
                State.StateSeconds = 2.5+Random01(State.RandomState)*4.0;
            }
        }
        Agent.FlightWeight = std::clamp(Agent.FlightWeight+(Agent.Behavior == EBehavior::Perched ? -1.0 : 1.0)*Delta*4.0,0.0,1.0);
        Agent.AnimationPhase = std::fmod(Agent.AnimationPhase+Delta*State.AnimationRate*(Agent.Behavior == EBehavior::Perched ? 0.08 : 1.0),1.0);
    }
}

FGamePlatformFaunaSimulation::FCell FGamePlatformFaunaSimulation::CellFor(const FVector& Position) const
{
    return {static_cast<std::int64_t>(std::floor(Position.X/GridCellSize)),
        static_cast<std::int64_t>(std::floor(Position.Y/GridCellSize)),static_cast<std::int64_t>(std::floor(Position.Z/GridCellSize))};
}

void FGamePlatformFaunaSimulation::StepFish()
{
    Grid.clear();
    for (int I = 0; I < Settings.Count; ++I) { Grid[CellFor(Agents[static_cast<std::size_t>(I)].Position)].push_back(I); }
    const double Delta = Settings.FixedStepSeconds;
    const double NeighborSquared = Settings.NeighborRadius*Settings.NeighborRadius;
    const double SeparationSquared = Settings.SeparationRadius*Settings.SeparationRadius;
    const int CandidateBudget = std::max(32,Settings.MaxNeighbors*4);
    for (int I = 0; I < Settings.Count; ++I)
    {
        const auto& Agent = Agents[static_cast<std::size_t>(I)];
        auto& Next = NextAgents[static_cast<std::size_t>(I)]; Next = Agent;
        auto& State = Memory[static_cast<std::size_t>(I)];
        State.StateSeconds -= Delta; State.DecisionSeconds -= Delta;
        if (Next.Behavior == EBehavior::Fleeing && State.StateSeconds <= 0.0) { Next.Behavior = EBehavior::Swimming; }
        if (State.DecisionSeconds <= 0.0 || LengthSquared(Subtract(State.Goal,Agent.Position)) < Settings.NeighborRadius*Settings.NeighborRadius*0.09)
        {
            if (!SampleLegalPosition(State.RandomState,State.Goal)) { State.Goal = Agent.Position; }
            State.DecisionSeconds = 3.0+Random01(State.RandomState)*3.0;
        }
        FVector Center, Alignment, Separation;
        int NeighborCount = 0, Checks = 0;
        const FCell Cell = CellFor(Agent.Position);
        const int CellOffset = static_cast<int>((static_cast<std::uint64_t>(I)+Statistics.TotalFixedSteps)%27u);
        // 27 邻近网格单元均覆盖邻域半径；旋转起点缓解高密度桶内的固定索引偏置。
        for (int Visit = 0; Visit < 27 && Checks < CandidateBudget && NeighborCount < Settings.MaxNeighbors; ++Visit)
        {
            const int Index = (Visit+CellOffset)%27;
            const FCell Nearby = {Cell.X+Index%3-1,Cell.Y+(Index/3)%3-1,Cell.Z+Index/9-1};
            const auto Found = Grid.find(Nearby);
            if (Found == Grid.end()) { continue; }
            const auto& Members = Found->second;
            const std::size_t Offset = (static_cast<std::size_t>(I)+static_cast<std::size_t>(Statistics.TotalFixedSteps))%Members.size();
            for (std::size_t VisitMember = 0; VisitMember < Members.size() && Checks < CandidateBudget && NeighborCount < Settings.MaxNeighbors; ++VisitMember)
            {
                const int OtherIndex = Members[(VisitMember+Offset)%Members.size()];
                if (OtherIndex == I) { continue; }
                ++Checks;
                const auto& Other = Agents[static_cast<std::size_t>(OtherIndex)];
                const FVector Away = Subtract(Agent.Position,Other.Position);
                const double DistanceSquared = LengthSquared(Away);
                if (DistanceSquared > NeighborSquared) { continue; }
                ++NeighborCount; Center = Add(Center,Other.Position); Alignment = Add(Alignment,Other.Velocity);
                if (DistanceSquared < SeparationSquared)
                {
                    FVector Direction = Normalize(Away,{I < OtherIndex ? -1.0 : 1.0,0,0});
                    Separation = Add(Separation,Scale(Direction,1.0-std::sqrt(DistanceSquared)/Settings.SeparationRadius));
                }
            }
        }
        Statistics.NeighborChecks += static_cast<std::uint64_t>(Checks);
        Statistics.NeighborLinks += static_cast<std::uint64_t>(NeighborCount);
        FVector Steering = Scale(Normalize(Subtract(State.Goal,Agent.Position)),0.65);
        if (NeighborCount > 0)
        {
            Steering = Add(Steering,Scale(Normalize(Alignment),1.2));
            Steering = Add(Steering,Scale(Normalize(Subtract(Scale(Center,1.0/NeighborCount),Agent.Position)),0.65));
            Steering = Add(Steering,Scale(Separation,2.2));
        }
        double Speed = Settings.CruiseSpeed;
        if (Next.Behavior == EBehavior::Fleeing)
        {
            Steering = Add(Steering,Scale(Normalize(Subtract(Agent.Position,Disturbance.Center)),4.0));
            Speed *= 1.0+Disturbance.Strength;
        }
        Steering = Add(Steering,Scale(BoundarySteering(Agent.Position),3.0));
        const FVector DesiredVelocity = Scale(Normalize(Steering,Normalize(Agent.Velocity)),Speed);
        Next.Velocity = Add(Agent.Velocity,LimitLength(Subtract(DesiredVelocity,Agent.Velocity),Settings.CruiseSpeed*3.0*Delta));
        Next.Velocity = LimitLength(Next.Velocity,Settings.CruiseSpeed*2.0);
        Next.Position = ConstrainMotion(Agent.Position,Scale(Next.Velocity,Delta));
        const FVector ActualDelta = Subtract(Next.Position,Agent.Position);
        // 碰壁后采用实际切向方向；完全受阻则反向离开，避免永远在墙上原地摆尾。
        if (LengthSquared(ActualDelta) < 1.e-10)
            { Next.Velocity = Scale(Normalize(Add(BoundarySteering(Agent.Position),Scale(Normalize(Agent.Velocity),-1.0))),Speed*0.5); State.DecisionSeconds = 0.0; }
        else if (LengthSquared(Subtract(ActualDelta,Scale(Next.Velocity,Delta))) > 1.e-6)
            { Next.Velocity = Scale(Normalize(ActualDelta),Speed*0.75); }
        Next.AnimationPhase = std::fmod(Agent.AnimationPhase+Delta*State.AnimationRate*(Next.Behavior == EBehavior::Fleeing ? 1.5 : 1.0),1.0);
        Next.FlightWeight = 1.0; Next.UpVector = {0,0,1}; Next.FlowerIndex = -1;
    }
    Agents.swap(NextAgents);
}

void FGamePlatformFaunaSimulation::Reset()
{
    *this = FGamePlatformFaunaSimulation{};
}
}
