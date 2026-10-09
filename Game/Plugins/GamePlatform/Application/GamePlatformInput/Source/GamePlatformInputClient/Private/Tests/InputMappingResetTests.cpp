// 输入自有行回归：瞬态原生UserSettings登记两功能映射，重置A不应改变B；不生成正式资产或写用户存档。
#include "Profiles/InputMappingReset.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputOwnedRowsResetTest, "GamePlatform.Input.Profile.OwnedRowsReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInputOwnedRowsResetTest::RunTest(const FString&)
{
    // ULocalPlayer声明Within=Engine，瞬态夹具也必须使用真实Engine外层，不能以默认Package绕过引擎对象前提。
    if (!GEngine) { AddError(TEXT("输入映射重置夹具需要有效Engine外层")); return false; }
    auto* Settings = NewObject<UEnhancedInputUserSettings>(); Settings->Initialize(NewObject<ULocalPlayer>(GEngine));
    auto* Context = NewObject<UInputMappingContext>();
    // 通过原生反射属性设置瞬态Action的Mappable契约，避免派生生产类型仅用于测试访问受保护字段。
    auto* Property = FindFProperty<FObjectPropertyBase>(UInputAction::StaticClass(), TEXT("PlayerMappableKeySettings"));
    if (!Property) { AddError(TEXT("引擎Mappable属性不可访问")); return false; }
    for (const auto Row : {FName(TEXT("Owned")), FName(TEXT("External"))})
    {
        auto* Action = NewObject<UInputAction>(Context); auto* Mappable = NewObject<UPlayerMappableKeySettings>(Action); Mappable->Name = Row;
        Property->SetObjectPropertyValue_InContainer(Action, Mappable); Context->MapKey(Action, EKeys::A);
    }
    TestTrue(TEXT("原生映射登记"), Settings->RegisterInputMappingContext(Context));
    for (const auto Row : {FName(TEXT("Owned")), FName(TEXT("External"))})
    {
        FMapPlayerKeyArgs Args; Args.MappingName = Row; Args.Slot = EPlayerMappableKeySlot::First; Args.NewKey = EKeys::B;
        FGameplayTagContainer Errors; Settings->MapPlayerKey(Args, Errors); TestTrue(TEXT("夹具重绑已受理"), Errors.IsEmpty());
    }
    TestTrue(TEXT("仅自有行重置成功"), ResetGamePlatformProfileMappings(*Settings, {FName(TEXT("Owned"))}, NAME_None).IsSuccess());
    const auto* Owned = Settings->FindCurrentMappingForSlot(TEXT("Owned"), EPlayerMappableKeySlot::First);
    const auto* External = Settings->FindCurrentMappingForSlot(TEXT("External"), EPlayerMappableKeySlot::First);
    if (!Owned || !External) { AddError(TEXT("原生登记行未产生有效映射")); return false; }
    TestEqual(TEXT("自有行恢复默认"), Owned->GetCurrentKey(), EKeys::A);
    TestEqual(TEXT("外部行保留自定义"), External->GetCurrentKey(), EKeys::B);
    TestFalse(TEXT("显式外部行亦拒绝"), ResetGamePlatformProfileMappings(*Settings, {FName(TEXT("Owned"))}, TEXT("External")).IsSuccess());
    return true;
}
#endif
