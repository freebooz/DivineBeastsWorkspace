#pragma once
// 第三层本地世界操控策略；仅保存当前鼠标按键，不拥有Pawn、资源、授权或跨世界状态。
// 控制器在游戏线程调用；失焦、界面暂停、旅行及换Pawn必须Reset，不能从物理按住状态重建旧拖动。
struct FDivineBeastsWorldControlState
{
    unsigned char MouseButtons = 0;
    explicit FDivineBeastsWorldControlState(unsigned char Buttons = 0) : MouseButtons(Buttons & 3) {}
    /** 当次命中路径必须既包含本游戏实际SViewport，又以该SViewport为叶子；任意UI叶子均保留点击。 */
    static bool CanBeginViewportDrag(bool bPathContainsGameViewport, bool bLeafIsGameViewport)
    {
        return bPathContainsGameViewport && bLeafIsGameViewport;
    }
    /** 返回本控制器应增/减的锁数量（-1/0/1）；不读取或清零其他调用方的引擎输入锁。 */
    static int UpdateOwnedInputLock(bool bNeedsLock, bool& bOwnsLock)
    {
        if (bNeedsLock == bOwnsLock) return 0;
        bOwnsLock = bNeedsLock;
        return bNeedsLock ? 1 : -1;
    }
    /** Right区分左右键；Down是本次已由视口接收的事件，UI消费的事件不得传入。 */
    void SetMouseButton(bool Right, bool Down)
    {
        const unsigned char Bit = Right ? 2 : 1;
        MouseButtons = Down ? (MouseButtons | Bit) : (MouseButtons & ~Bit);
    }
    bool IsDragging() const { return MouseButtons != 0; }
    bool IsFacingCamera() const { return (MouseButtons & 2) != 0; }
    /** 两键前进优先于W/S，输出保持[-1,1]；无自动奔跑的隐式锁定状态。 */
    float ResolveForward(float KeyboardAxis) const { return MouseButtons == 3 ? 1.f : KeyboardAxis; }
    /** RMB将A/D从转身改为横移；Q/E始终横移，相加后钳制到归一化轴范围。 */
    float ResolveTurn(float KeyboardAxis) const { return IsFacingCamera() ? 0.f : KeyboardAxis; }
    float ResolveStrafe(float StrafeAxis, float TurnAxis) const
    {
        const float Value = StrafeAxis + (IsFacingCamera() ? TurnAxis : 0.f);
        return Value < -1.f ? -1.f : (Value > 1.f ? 1.f : Value);
    }
    void Reset() { MouseButtons = 0; }
    /** 左键短按且累计原始鼠标路程不超过6像素才算选择；长按/往返拖动/非有限输入均拒绝。
     * 使用累计路程而不是末点距离，避免拖回原点后被误判为单击。时间单位秒，最大0.35秒。 */
    static bool IsSelectionClick(bool bPending, double HeldSeconds, float TravelPixels)
    {
        return bPending && HeldSeconds >= 0.0 && HeldSeconds <= 0.35 &&
            TravelPixels >= 0.f && TravelPixels <= 6.f;
    }
};
