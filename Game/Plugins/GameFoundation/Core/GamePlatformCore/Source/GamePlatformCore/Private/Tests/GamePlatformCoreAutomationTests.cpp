#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Types/GamePlatformId.h"
#include "Types/GamePlatformResult.h"
#include "Types/GamePlatformVersion.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformIdContractTest, "GamePlatform.Core.Identity",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformIdContractTest::RunTest(const FString& Parameters)
{
    FGamePlatformId Id;
    TestFalse(TEXT("默认身份无效"), Id.IsValid());
    TestTrue(TEXT("解析大写与最大逻辑版本"), FGamePlatformId::TryParse(TEXT("Platform.Test.Item@2147483647"), Id));
    TestEqual(TEXT("规范文本"), Id.ToString(), FString(TEXT("platform.test.item@2147483647")));
    FGamePlatformId Upper = Id;
    Upper.Namespace = TEXT("PLATFORM.TEST");
    Upper.Name = TEXT("ITEM");
    TestTrue(TEXT("编辑大写与规范值相等"), Upper == Id);
    TestEqual(TEXT("相等值哈希一致"), GetTypeHash(Upper), GetTypeHash(Id));
    TSet<FGamePlatformId> Identities;
    Identities.Add(Upper);
    TestTrue(TEXT("容器以规范值查找"), Identities.Contains(Id));
    TestTrue(TEXT("反射比较遵循规范相等性"), FGamePlatformId::StaticStruct()->CompareScriptStruct(&Upper, &Id, 0));
    TestTrue(TEXT("反射公开命名空间"), FindFProperty<FStrProperty>(FGamePlatformId::StaticStruct(), TEXT("Namespace")) != nullptr);
    TestTrue(TEXT("反射公开逻辑版本"), FindFProperty<FIntProperty>(FGamePlatformId::StaticStruct(), TEXT("LogicalVersion")) != nullptr);
    TestFalse(TEXT("逻辑版本溢出失败"), FGamePlatformId::TryParse(TEXT("platform.item@2147483648"), Id));
    TestTrue(TEXT("失败清空输出"), Id.Namespace.IsEmpty() && Id.Name.IsEmpty() && Id.LogicalVersion == 1);
    TestFalse(TEXT("失败输出不能重新使用"), Id.IsValid());
    Id.Namespace = TEXT("PLATFORM.ITEM@2");
    TestTrue(TEXT("输入可别名引用输出字段"), FGamePlatformId::TryParse(Id.Namespace, Id));
    TestEqual(TEXT("别名解析结果"), Id.ToString(), FString(TEXT("platform.item@2")));
    const TCHAR Embedded[] = {TEXT('a'), TEXT('.'), TEXT('b'), TEXT('@'), TEXT('1'), 0, TEXT('x')};
    TestFalse(TEXT("内嵌NUL不得截断成合法文本"), FGamePlatformId::TryParse(FString(UE_ARRAY_COUNT(Embedded), Embedded), Id));
    Id.Namespace = TEXT("a");
    Id.Name = TEXT("b.c");
    TestFalse(TEXT("可编辑字段不绕过名称校验"), Id.IsValid());
    TestTrue(TEXT("无效身份不生成字符串"), Id.ToString().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVersionContractTest, "GamePlatform.Core.Version",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVersionContractTest::RunTest(const FString& Parameters)
{
    FGamePlatformVersion Version;
    TestTrue(TEXT("零版本有效"), Version.IsValid());
    TestEqual(TEXT("零版本规范输出"), Version.ToString(), FString(TEXT("0.0.0")));
    TestTrue(TEXT("最大分量可解析"), FGamePlatformVersion::TryParse(TEXT("2147483647.2.3"), Version));
    FGamePlatformVersion Lower;
    TestTrue(TEXT("比较输入有效"), FGamePlatformVersion::TryParse(TEXT("1.2147483647.2147483647"), Lower));
    TestEqual(TEXT("按数值主版本比较"), Version.Compare(Lower), 1);
    TestFalse(TEXT("前导零拒绝"), FGamePlatformVersion::TryParse(TEXT("1.02.3"), Version));
    TestEqual(TEXT("失败重置全部分量"), Version.ToString(), FString(TEXT("0.0.0")));
    TestFalse(TEXT("补丁溢出拒绝"), FGamePlatformVersion::TryParse(TEXT("1.2.2147483648"), Version));
    Version.Minor = -1;
    TestFalse(TEXT("编辑非法值被校验"), Version.IsValid());
    TestTrue(TEXT("无效版本无规范文本"), Version.ToString().IsEmpty());
    TestTrue(TEXT("版本反射类型有补丁字段"), FindFProperty<FIntProperty>(FGamePlatformVersion::StaticStruct(), TEXT("Patch")) != nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformResultContractTest, "GamePlatform.Core.Result",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformResultContractTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("默认未执行不是成功"), FGamePlatformResult{}.IsSuccess());
    TestTrue(TEXT("显式成功"), FGamePlatformResult::Success().IsSuccess());
    const FGamePlatformResult Failure = FGamePlatformResult::Failure(NAME_None, TEXT("原始原因"));
    TestFalse(TEXT("缺少失败码仍为失败"), Failure.IsSuccess());
    TestEqual(TEXT("缺码诊断可机器识别"), Failure.Code, FName(TEXT("MissingFailureCode")));
    TestTrue(TEXT("中文原始诊断保留"), Failure.Message.Contains(TEXT("原始原因")));
    const FGamePlatformResult Provided = FGamePlatformResult::Failure(FName(TEXT("LoadFailed")), TEXT("加载失败"));
    TestEqual(TEXT("提供的诊断码保留"), Provided.Code, FName(TEXT("LoadFailed")));
    TestEqual(TEXT("提供的中文诊断保留"), Provided.Message, FString(TEXT("加载失败")));
    const FGamePlatformResult Cancelled = FGamePlatformResult::Cancelled(FString{});
    TestTrue(TEXT("取消有独立终态"), Cancelled.Status == EGamePlatformResultStatus::Cancelled);
    TestFalse(TEXT("取消不是成功"), Cancelled.IsSuccess());
    TestFalse(TEXT("取消补充说明"), Cancelled.Message.IsEmpty());
    const FGamePlatformResult Unsupported = FGamePlatformResult::Unsupported(NAME_None, FString{});
    TestTrue(TEXT("不支持有独立状态"), Unsupported.Status == EGamePlatformResultStatus::Unsupported);
    TestEqual(TEXT("不支持缺码诊断"), Unsupported.Code, FName(TEXT("MissingUnsupportedCode")));
    FGamePlatformResult Contradictory = FGamePlatformResult::Success();
    Contradictory.Code = FName(TEXT("UnexpectedError"));
    TestFalse(TEXT("手工矛盾成功不能通过"), Contradictory.IsSuccess());
    TestTrue(TEXT("结果状态进入反射"), StaticEnum<EGamePlatformResultStatus>() != nullptr);
    TestTrue(TEXT("结果说明进入反射"), FindFProperty<FStrProperty>(FGamePlatformResult::StaticStruct(), TEXT("Message")) != nullptr);
    return true;
}
#endif
