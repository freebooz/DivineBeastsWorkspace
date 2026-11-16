// 仅从同模块私有实现内包含，复用真实私有状态/Provider；不参与生产构建。
#if WITH_DEV_AUTOMATION_TESTS
bool VerifyManualServerOwnershipForTests()
{
    auto A=MakeShared<FManualControlProvider>();auto B=MakeShared<FManualControlProvider>();
    FGamePlatformServerInstanceInfo I;I.GameId=TEXT("ManualGame");I.GameServerId=FGuid::NewGuid().ToString();I.ServerBootId=FGuid::NewGuid().ToString();
    I.ServerRoleId=TEXT("Village");I.ExperienceId=TEXT("Village.Training");I.WorldId=TEXT("ManualWorld");I.RegionId=TEXT("ManualRegion");
    I.BuildVersion=TEXT("manual");I.ProtocolVersion=1;I.PublicEndpoint=TEXT("127.0.0.1:7777");I.Capacity=2;
    auto Foreign=I;Foreign.GameServerId=FGuid::NewGuid().ToString();if(!A->Bind(I) || !B->Bind(Foreign))return false;
    int32 Own=0,Rejected=0;A->RegisterInstance(I,[&](bool Ok,FName){if(Ok)++Own;});
    A->RegisterInstance(Foreign,[&](bool Ok,FName){if(!Ok)++Rejected;});
    if(A->Pending.Num()!=1 || Rejected!=1 || !B->Pending.IsEmpty())return false;
    FManualControlRequest Request;if(A->Take(TEXT("Heartbeat"),1,Request) || !A->Take(TEXT("Register"),1,Request))return false;
    auto C=MoveTemp(Request.Completion);C(true,NAME_None);A->Close();
    A->RegisterInstance(I,[&](bool Ok,FName){if(!Ok)++Rejected;});
    auto Admission=MakeShared<FManualAdmissionProvider>();Admission->Expected.GameServerId=I.GameServerId;
    Admission->Expected.ServerBootId=I.ServerBootId;Admission->Expected.WorldId=I.WorldId;Admission->Expected.ExperienceId=I.ExperienceId;
    Admission->Expected.ProtocolVersion=TEXT("1.0.0");Admission->Expected.ServerStartGeneration=1;Admission->AllowedOperation=FGuid::NewGuid();
    FGamePlatformServerAdmissionProof Proof;Proof.OperationId=FGuid::NewGuid();
    Admission->ValidateAdmission(Admission->Expected,FGuid::NewGuid(),1,MoveTemp(Proof),[&](const auto& R){if(!R.bSucceeded)++Rejected;});
    Admission->Close();return Own==1 && Rejected==3 && A->Pending.IsEmpty() && Admission->Pending.IsEmpty();
}
#endif
