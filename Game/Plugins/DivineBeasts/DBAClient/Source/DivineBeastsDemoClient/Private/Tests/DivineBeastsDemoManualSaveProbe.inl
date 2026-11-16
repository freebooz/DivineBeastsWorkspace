// 仅从同模块私有实现内包含，复用真实私有状态/Provider；不参与生产构建。
#if WITH_DEV_AUTOMATION_TESTS
// 仅测试构建编入；调用上面同一清理实现，不复制IO/存储规则。
bool VerifyManualSaveCleanupForTests()
{
    class FService final:public IGamePlatformSaveService
    {
    public:int32 UnregisterCount=0;
        FGamePlatformResult RegisterProvider(const TSharedRef<IGamePlatformSaveProvider>&) override{return FGamePlatformResult::Success();}
        FGamePlatformResult UnregisterProvider(FName) override{++UnregisterCount;return FGamePlatformResult::Success();}
        FGamePlatformSaveRequestHandle SaveRecordAsync(const FGamePlatformSaveRecord&,FGamePlatformSaveCallback,FGamePlatformResult& R) override{R=FGamePlatformResult::Cancelled(TEXT("测试服务拒绝IO"));return {};}
        FGamePlatformSaveRequestHandle LoadRecordAsync(const FGamePlatformSaveKey&,FGamePlatformSaveCallback,FGamePlatformResult& R) override{R=FGamePlatformResult::Cancelled(TEXT("测试服务拒绝IO"));return {};}
        FGamePlatformSaveRequestHandle DeleteRecordAsync(const FGamePlatformSaveKey&,FGamePlatformSaveCallback,FGamePlatformResult& R) override{R=FGamePlatformResult::Cancelled(TEXT("测试服务拒绝IO"));return {};}
        FGamePlatformSaveDiagnostics GetDiagnostics() const override{return {};}
    } Service;
    auto State=MakeShared<FManualSaveState>();State->OwnedKeys.AddDefaulted();State->bProviderRegistered=true;
    int32 Missing=0;State->RequestStop([&](const auto&,const auto& R){if(!R.IsSuccess())++Missing;});
    const bool Retained=Missing==1 && State->OwnedKeys.Num()==1 && State->bProviderRegistered && !State->bClosed;
    State->OwnedKeys.Reset(); // 模拟前一轮删除已得到确认，只剩本Provider；不声称本测试执行磁盘IO。
    State->TestService=&Service;State->CleanupResult=ManualError(TEXT("Demo.PreviousAttempt"),TEXT("旧清理失败"));
    int32 Success=0;State->RequestStop([&](const auto&,const auto& R){if(R.IsSuccess())++Success;});
    return Retained && Success==1 && State->bClosed && State->CleanupResult.IsSuccess() && Service.UnregisterCount==1;
}
#endif
