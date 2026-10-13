// SSLM项目开发运行证据写入；仅本次RunId下追加文件，签收仍归唯一平台Editor审核入口。
#pragma once
#include "Contracts/DivineBeastsDemoRunTypes.h"
namespace DivineBeasts::Demo
{
    /** 游戏线程写脱敏值快照/日志/索引，成功返回真实索引路径；失败不覆盖旧文件/人工记录。
     * 二进制验证默认false，只有后续测试运行器的独立构建证明可以补证，不凭当前Git推断。
     */
    bool WriteRunEvidence(const FDivineBeastsDemoRunEvidence& Evidence, FString& OutIndexPath, FString& OutError);
}
