// 平台Editor证据完整性内部算法；不拥有项目运行或跨进程注册表，仅在游戏线程按固定根读文件。
#pragma once
#include "CoreMinimal.h"
class UGamePlatformReviewCase;
class UGamePlatformReviewReport;
namespace GamePlatform::Review
{
    /** 固定项目Saved/RunGUID/Review下解析相对证据路径，拒绝逃逸、链接和未实现的链接审计。 */
    bool ResolveEvidencePath(const FString& RunId, const FString& RelativePath, FString& OutPath, FString& OutError);
    /** 64MiB是外部文件读取防滥用上限，不是GPU/游戏性能预算；超限/变化/哈希失败返回空。 */
    FString HashEvidenceFile(const FString& Path);
    /** 用例字段按固定顺序和长度编码，指纹随目的、步骤、预期及类型变化。 */
    FString ContractFingerprint(const UGamePlatformReviewCase& ReviewCase);
    /** 核对合同/版本/运行和实际证据，未完整记录不能标Passed。 */
    bool ValidateEvidence(const UGamePlatformReviewCase& ReviewCase, const UGamePlatformReviewReport& Report, FString& OutError);
    /** 列表是type:path，校验实际文件和已导入摘要；Passed要求全部必需类型。 */
    bool ValidateEvidenceReferences(const UGamePlatformReviewCase& ReviewCase, const UGamePlatformReviewReport& Report,
        const TArray<FString>& References, bool bRequireAllTypes, FString& OutError);
}
