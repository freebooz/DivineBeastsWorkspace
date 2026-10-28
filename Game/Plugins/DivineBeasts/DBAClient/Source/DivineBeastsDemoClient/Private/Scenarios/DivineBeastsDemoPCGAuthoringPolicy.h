// 项目私有作者命名边界；平台只负责中性资产名与固定开发目录。
#pragma once
#include "Types/GamePlatformResult.h"
namespace DivineBeasts::Demo {
inline FGamePlatformResult ValidateManualRealizedName(FName Name){
 const FString Text=Name.ToString(),Prefix=TEXT("PCG_DBA_Manual");
 bool Valid=Text.StartsWith(Prefix,ESearchCase::CaseSensitive)&&Text.Len()>Prefix.Len()&&Text.Len()<=96;
 for(TCHAR C:Text)Valid&=(C>='A'&&C<='Z')||(C>='a'&&C<='z')||(C>='0'&&C<='9')||C=='_';
 return Valid?FGamePlatformResult::Success():FGamePlatformResult::Failure(TEXT("Demo.PCG.OutputScope"),TEXT("本页输出名必须为PCG_DBA_Manual前缀及非空ASCII字母数字下划线后缀；大小写精确匹配，拒绝路径。"));
}
}
