// 仅由Flow适配实现包含；测试直调同一Execute/Cleanup，服务为真实平台Flow，未复制句柄规则。
bool VerifyManualFlowOwnershipForTests(FAutomationTestBase& Test)
{
    TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformApplicationFlowSubsystem> Service(NewObject<UGamePlatformApplicationFlowSubsystem>(GI.Get()));
    FSubsystemCollection<UGameInstanceSubsystem> Collection;Service->Initialize(Collection);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if(!Test.TestNotNull(TEXT("句柄测试独立World"),World)){Service->Deinitialize();return false;}
    auto Setup=[&](FManualFlowScenario& F)
    {
        F.Host=GI.Get();F.Flow=Service.Get();F.Node.Reset(NewObject<UDivineBeastsDemoManualFlowNode>(GI.Get()));
        F.Context.Token.RunId=FGuid::NewGuid();F.Context.Token.WorldId=FGuid::NewGuid();F.Context.Token.Generation=1;
        TArray<FName> Names,Ops;TArray<EDivineBeastsDemoParameterType> Types;
        GetPlatformManualSignature(TEXT("GamePlatformApplicationFlow.01"),Names,Types,Ops);
        for(int32 I=0;I<Names.Num();++I)
        {
            auto& P=F.Context.Scenario.Parameters.AddDefaulted_GetRef();P.ParameterId=Names[I];P.DefaultValue.Type=Types[I];
            if(Names[I]==TEXT("TimeoutSeconds"))P.DefaultValue.NumberValue=30;
            if(Names[I]==TEXT("WaitForUser"))P.DefaultValue.bBooleanValue=true;
            if(Names[I]==TEXT("Outcome"))P.DefaultValue.IdentifierValue=TEXT("Accept");
        }
    };
    auto Command=[&](FManualFlowScenario& F,FName Op)
    {
        FManualValues Values;for(const auto& P:F.Context.Scenario.Parameters)Values.Add(P.DefaultValue);
        FGamePlatformResult R;F.ExecuteService(Op,FManualArguments(F.Context,Values),[&](const auto&,const auto& Result){R=Result;});return R;
    };
    auto Stop=[&](FManualFlowScenario& F){FGamePlatformResult R;F.Stop([&](const auto&,const auto& Result){R=Result;});return R;};
    {
        FManualFlowScenario F(*World,TEXT("GamePlatformApplicationFlow.01"));Setup(F);
        Test.TestTrue(TEXT("显式撤销路径：Register"),Command(F,TEXT("RegisterFactory")).IsSuccess());
        Test.TestTrue(TEXT("显式撤销路径：Configure"),Command(F,TEXT("Configure")).IsSuccess());
        Test.TestTrue(TEXT("显式撤销路径：Start"),Command(F,TEXT("Start")).IsSuccess());
        Test.TestTrue(TEXT("实际运行先取消至终态"),Command(F,TEXT("Cancel")).IsSuccess());
        Test.TestTrue(TEXT("显式Unregister成功"),Command(F,TEXT("UnregisterFactory")).IsSuccess());
        Test.TestFalse(TEXT("成功撤销清字段"),F.Factory.IsValid());
        Test.TestTrue(TEXT("显式撤销后Stop不重复撤销旧工厂"),Stop(F).IsSuccess());
    }
    FGuid OriginalRegistration;
    {
        FManualFlowScenario F(*World,TEXT("GamePlatformApplicationFlow.01"));Setup(F);
        Test.TestTrue(TEXT("下个案例可重新注册"),Command(F,TEXT("RegisterFactory")).IsSuccess());
        OriginalRegistration=F.Factory.RegistrationId;
        Test.TestFalse(TEXT("重复Register明确失败"),Command(F,TEXT("RegisterFactory")).IsSuccess());
        Test.TestTrue(TEXT("重复失败保留原工厂句柄"),F.Factory.RegistrationId==OriginalRegistration);
        Test.TestTrue(TEXT("重复注册后Stop撤销真实原注册"),Stop(F).IsSuccess());
    }
    {
        FManualFlowScenario F(*World,TEXT("GamePlatformApplicationFlow.01"));Setup(F);
        Test.TestTrue(TEXT("再次新案例无遗留工厂"),Command(F,TEXT("RegisterFactory")).IsSuccess());
        Test.TestTrue(TEXT("新注册身份不复用"),F.Factory.RegistrationId!=OriginalRegistration);
        Test.TestTrue(TEXT("运行句柄：Configure"),Command(F,TEXT("Configure")).IsSuccess());
        Test.TestTrue(TEXT("运行句柄：首次Start"),Command(F,TEXT("Start")).IsSuccess());
        const auto Owned=F.Handle;const auto Factory=F.Factory;
        Test.TestFalse(TEXT("运行中Unregister失败"),Command(F,TEXT("UnregisterFactory")).IsSuccess());
        Test.TestTrue(TEXT("撤销失败保留原工厂"),F.Factory.RegistrationId==Factory.RegistrationId);
        Test.TestFalse(TEXT("重复Start失败"),Command(F,TEXT("Start")).IsSuccess());
        Test.TestTrue(TEXT("重复Start保留原Run"),F.Handle.ScopeId==Owned.ScopeId && F.Handle.RunId==Owned.RunId);
        // 只越过适配就绪守卫以实测Native FlowBusy；不签发或伪称取得Data租约。
        F.DataState->bReady=true;
        Test.TestFalse(TEXT("已有本运行时StartFlow实际拒绝"),Command(F,TEXT("StartFlow")).IsSuccess());
        Test.TestFalse(TEXT("重复StartFlow实际拒绝"),Command(F,TEXT("StartFlow")).IsSuccess());
        Test.TestTrue(TEXT("被拒资产Start不覆盖本运行句柄"),F.Handle.ScopeId==Owned.ScopeId && F.Handle.RunId==Owned.RunId);
        Test.TestTrue(TEXT("重复Start/StartFlow后Stop仍可取消原Run并撤销工厂"),Stop(F).IsSuccess());
        Test.TestTrue(TEXT("实际唯一Flow已Cancelled"),Service->GetSnapshot().State==EGamePlatformFlowState::Cancelled);
    }
    Service->Deinitialize();World->DestroyWorld(false);return !Test.HasAnyErrors();
}


/** 必须已有已保存ManualFlow资源；真实GI集合/Data租约/两次资产请求，不模拟资产就绪。 */
class FManualFlowAssetOwnershipCommand final:public IAutomationLatentCommand
{
public:
    explicit FManualFlowAssetOwnershipCommand(FAutomationTestBase& InTest):Test(InTest),Started(FPlatformTime::Seconds()){}
    ~FManualFlowAssetOwnershipCommand() override{Cleanup();}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>30){Test.AddError(TEXT("Fix2真实Flow租约回归超时，不能计成功。"));Cleanup();return true;}
        if(Phase==0)
        {
            if(!GEngine){Test.AddError(TEXT("需要真实Engine宿主。"));return true;}
            const FString Allowed=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),TEXT("../Saved/Validation/PluginManualUI20261027")));
            if(!FDivineBeastsPluginDemoMode::IsEnabled() || !FParse::Param(FCommandLine::Get(),TEXT("GamePlatformManualFixture")) || !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()),Allowed))
            {Test.AddError(TEXT("真实GI集合回归须显式DemoMode+ManualFixture及工程验证根UserDir，拒绝在普通宿主初始化项目流程。"));return true;}
            SavedToken=FNetDelegates::OnReceivedNetworkEncryptionToken;SavedAck=FNetDelegates::OnReceivedNetworkEncryptionAck;SavedFailure=FNetDelegates::OnReceivedNetworkEncryptionFailure;
            GI.Reset(NewObject<UGameInstance>(GEngine));GI->Init();bInitialized=true;
            auto* Flow=GI->GetSubsystem<UGamePlatformApplicationFlowSubsystem>();Data=IGamePlatformDataService::Get(*GI);
            if(!Flow || !Data){Test.AddError(TEXT("真实GI集合缺Flow/Data。"));Cleanup();return true;}
            // 真实GI还拥有目录等其他服务租约；统计所有请求状态，避免目录Loading→Ready改变Active数。
            const auto Baseline=Data->GetDiagnostics();
            InitialRequestCount=Baseline.PendingRequests+Baseline.ActiveLeases+Baseline.TerminalRequests;
            Test.AddInfo(FString::Printf(TEXT("Flow回归Data请求基线=%d（含同GI其他所有者）"),InitialRequestCount));
            World=UWorld::CreateWorld(EWorldType::Game,false);if(!World){Test.AddError(TEXT("无法创建测试World。"));Cleanup();return true;}World->SetGameInstance(GI.Get());
            Fixture=MakeUnique<FManualFlowScenario>(*World,TEXT("GamePlatformApplicationFlow.01"));
            Fixture->Host=GI.Get();Fixture->Flow=Flow;Fixture->Context.Token.RunId=FGuid::NewGuid();Fixture->Context.Token.WorldId=FGuid::NewGuid();Fixture->Context.Token.Generation=1;
            Fixture->DataState->GI=GI.Get();Fixture->DataState->World=World;
            if(!Command(TEXT("RegisterFactory")).IsSuccess()){Test.AddError(TEXT("真实测试工厂注册失败。"));Cleanup();return true;}
            Acquire();Phase=1;return false;
        }
        if(!bAcquired)return false;
        if(!Acquired.IsSuccess()){Test.AddError(TEXT("ManualFlow真实资产/扫描未就绪：")+Acquired.Message);Cleanup();return true;}
        Fixture->DataState->Lease=ReadyLease;ReadyLease={};Fixture->DataState->bReady=true;
        if(Phase==1)
        {
            FirstLease=Fixture->DataState->Lease;
            Test.TestTrue(TEXT("首次真实StartFlow成功"),Command(TEXT("StartFlow")).IsSuccess());
            Owned=Fixture->Handle;
            if(!Owned.IsValid()){Cleanup();return true;}
            Test.TestFalse(TEXT("真实就绪租约成功转交唯一执行器"),Fixture->DataState->Lease.IsValid());
            Acquire();Phase=2;return false;
        }
        const auto SecondLease=Fixture->DataState->Lease;
        Test.TestFalse(TEXT("活动资产运行中重复StartFlow失败"),Command(TEXT("StartFlow")).IsSuccess());
        Test.TestFalse(TEXT("再次重复StartFlow仍失败"),Command(TEXT("StartFlow")).IsSuccess());
        Test.TestTrue(TEXT("失败未覆盖原资产运行句柄"),Fixture->Handle.ScopeId==Owned.ScopeId && Fixture->Handle.RunId==Owned.RunId);
        Test.TestTrue(TEXT("失败不转移第二份真实租约"),Fixture->DataState->Lease.LeaseId==SecondLease.LeaseId);
        FGamePlatformResult Stopped;Fixture->Stop([&](const auto&,const auto& R){Stopped=R;});
        Test.TestTrue(TEXT("重复资产StartFlow后Stop清理真实运行/两份租约/工厂"),Stopped.IsSuccess());
        Test.TestTrue(TEXT("执行器接管的第一份完整租约已释放"),Data->GetLeaseState(FirstLease)==EGamePlatformDataRequestState::Released);
        Test.TestTrue(TEXT("失败未转交的第二份完整租约已释放"),Data->GetLeaseState(SecondLease)==EGamePlatformDataRequestState::Released);
        const auto Remaining=Data->GetDiagnostics();
        Test.TestEqual(TEXT("只释放本Flow请求，同GI其他所有者请求数保持基线"),
            Remaining.PendingRequests+Remaining.ActiveLeases+Remaining.TerminalRequests,InitialRequestCount);
        auto* Flow=GI->GetSubsystem<UGamePlatformApplicationFlowSubsystem>();FGamePlatformResult R;
        const auto Next=Flow->RegisterNodeFactory(TEXT("ManualWaitExecutor"),[](UGameInstance& Owner){return NewObject<UDivineBeastsDemoManualFlowNode>(&Owner);},R);
        Test.TestTrue(TEXT("下一案例可重新注册工厂"),Next.IsValid() && R.IsSuccess());
        if(Next.IsValid())Flow->UnregisterNodeFactory(Next,R);
        Cleanup();return true;
    }
private:
    FGamePlatformResult Command(FName Op)
    {FManualValues Empty;FGamePlatformResult R;Fixture->ExecuteService(Op,FManualArguments(Fixture->Context,Empty),[&](const auto&,const auto& Value){R=Value;});return R;}
    void Acquire()
    {
        bAcquired=false;FGamePlatformId Id;FGamePlatformId::TryCreate(TEXT("divinebeasts.manual.flow"),TEXT("interaction"),1,Id);
        const FPrimaryAssetId Primary(UGamePlatformPrimaryDataAsset::DefinitionAssetType(),FName(*Id.ToString()));
        FGamePlatformResult Accepted;TWeakPtr<int32> Weak(Alive);
        ReadyLease=Data->AcquireDefinition(Primary,UGamePlatformFlowDefinition::StaticClass(),{},EGamePlatformDataLifetime::Instance,GI.Get(),
            [this,Weak](const auto& L,const auto& R){if(!Weak.IsValid())return;const auto& E=ReadyLease;
                if(L.ScopeId!=E.ScopeId || L.LeaseId!=E.LeaseId || L.Generation!=E.Generation || L.IssuerProof!=E.IssuerProof || L.DefinitionId!=E.DefinitionId || L.Bundles!=E.Bundles || L.ResourcePaths!=E.ResourcePaths)
                {Acquired=ManualError(TEXT("Test.FlowLeaseMismatch"),TEXT("真实租约全身份不匹配。"));bAcquired=true;return;}
                Acquired=R;bAcquired=true;},Accepted);
        if(!Accepted.IsSuccess()){Acquired=Accepted;bAcquired=true;}
    }
    void Cleanup()
    {
        Alive.Reset();
        if(Fixture){FGamePlatformResult R;Fixture->Stop([&](const auto&,const auto& Result){R=Result;});if(!R.IsSuccess())Test.AddError(TEXT("Flow回归清理失败：")+R.Message);Fixture.Reset();}
        if(Data && ReadyLease.IsValid()){Data->ReleaseDefinition(ReadyLease);ReadyLease={};}
        if(World){World->DestroyWorld(false);World=nullptr;}
        if(bInitialized){GI->Shutdown();bInitialized=false;FNetDelegates::OnReceivedNetworkEncryptionToken=SavedToken;FNetDelegates::OnReceivedNetworkEncryptionAck=SavedAck;FNetDelegates::OnReceivedNetworkEncryptionFailure=SavedFailure;}
        Data=nullptr;GI.Reset();
    }
    FAutomationTestBase& Test;double Started;int32 Phase=0;bool bInitialized=false,bAcquired=false;
    TStrongObjectPtr<UGameInstance> GI;UWorld* World=nullptr;IGamePlatformDataService* Data=nullptr;
    TUniquePtr<FManualFlowScenario> Fixture;FGamePlatformDataLease ReadyLease,FirstLease;FGamePlatformFlowHandle Owned;FGamePlatformResult Acquired;
    int32 InitialRequestCount=0;
    TSharedPtr<int32> Alive=MakeShared<int32>(0);
    decltype(FNetDelegates::OnReceivedNetworkEncryptionToken) SavedToken;
    decltype(FNetDelegates::OnReceivedNetworkEncryptionAck) SavedAck;
    decltype(FNetDelegates::OnReceivedNetworkEncryptionFailure) SavedFailure;
};
void QueueManualFlowAssetOwnershipForTests(FAutomationTestBase& Test)
{FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FManualFlowAssetOwnershipCommand>(Test));}
