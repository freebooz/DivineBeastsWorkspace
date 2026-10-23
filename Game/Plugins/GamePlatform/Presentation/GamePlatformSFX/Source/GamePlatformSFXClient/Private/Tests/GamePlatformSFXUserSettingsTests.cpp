// 平台三路音量回归：临时INI不读写用户文件；真实组件只核对增益，不冒充扬声器试听。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformSFXUserSettingsSubsystem.h"
#include "Subsystems/GamePlatformSFXWorldSubsystem.h"
#include "Components/AudioComponent.h"
#include "Definitions/GamePlatformSFXDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSFXUserSettingsTest, "GamePlatform.SFX.UserSettings.PersistenceAndMixing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSFXUserSettingsTest::RunTest(const FString&)
{
    const FString Filename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), FGuid::NewGuid().ToString(), TEXT("Audio.ini"));
    TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformSFXUserSettingsSubsystem> Settings(NewObject<UGamePlatformSFXUserSettingsSubsystem>(GI.Get()));
    Settings->LoadVolumes(Filename);
    float InitialMusic = 1.f;
    GConfig->GetFloat(TEXT("GamePlatformSFX.UserVolumeDefaults"), TEXT("Music"), InitialMusic, GGameIni);
    TestEqual(TEXT("首次无文件采用项目默认音乐音量"), Settings->GetSnapshot().Music, InitialMusic);
    const int32 ExpectedChanges = 2 + (InitialMusic == 0.f ? 0 : 1);
    int32 Notifications = 0;
    const auto Subscription = Settings->AddChangedHandler(FGamePlatformSFXUserVolumesChanged::FDelegate::CreateLambda(
        [&](const auto&) { ++Notifications; }));
    TestFalse(TEXT("拒绝NaN"), Settings->SetVolume(EGamePlatformSFXSoundGroup::Effects, std::numeric_limits<float>::quiet_NaN()).IsSuccess());
    TestFalse(TEXT("拒绝负数"), Settings->SetVolume(EGamePlatformSFXSoundGroup::Ambient, -0.01f).IsSuccess());
    TestFalse(TEXT("拒绝超过100%"), Settings->SetVolume(EGamePlatformSFXSoundGroup::Music, 1.01f).IsSuccess());
    TestFalse(TEXT("拒绝未知分组"), Settings->SetVolume(static_cast<EGamePlatformSFXSoundGroup>(255), 0.5f).IsSuccess());
    TestEqual(TEXT("拒绝命令不发布事件"), Notifications, 0);
    TestTrue(TEXT("效果音量保存"), Settings->SetVolume(EGamePlatformSFXSoundGroup::Effects, 0.25f).IsSuccess());
    TestTrue(TEXT("环境音量保存"), Settings->SetVolume(EGamePlatformSFXSoundGroup::Ambient, 0.5f).IsSuccess());
    TestTrue(TEXT("音乐静音保存"), Settings->SetVolume(EGamePlatformSFXSoundGroup::Music, 0.0f).IsSuccess());
    TestEqual(TEXT("只通知真实变化"), Notifications, ExpectedChanges);
    Settings->SetVolume(EGamePlatformSFXSoundGroup::Music, 0.0f);
    TestEqual(TEXT("重复值无事件"), Notifications, ExpectedChanges);
    TestTrue(TEXT("一次捕获结束将三路值写盘"), Settings->CommitVolumes().IsSuccess());
    TStrongObjectPtr<UGamePlatformSFXUserSettingsSubsystem> Restored(NewObject<UGamePlatformSFXUserSettingsSubsystem>(GI.Get()));
    Restored->LoadVolumes(Filename);
    TestEqual(TEXT("重建服务恢复效果音"), Restored->GetSnapshot().Effects, 0.25f);
    TestEqual(TEXT("重建服务恢复环境音"), Restored->GetSnapshot().Ambient, 0.5f);
    TestEqual(TEXT("重建服务恢复音乐静音"), Restored->GetSnapshot().Music, 0.0f);
    Settings->RemoveChangedHandler(Subscription);
    Settings->SetVolume(EGamePlatformSFXSoundGroup::Effects, 1.0f);
    TestEqual(TEXT("解绑不再通知"), Notifications, 3);
    Settings->StorageFilename.Reset();
    TestFalse(TEXT("无法保存时返回失败"), Settings->CommitVolumes().IsSuccess());
    TestEqual(TEXT("保存失败不撤销已生效音量"), Settings->GetSnapshot().Effects, 1.0f);
    Settings->StorageFilename = Filename;
    TestTrue(TEXT("失败可重试保存"), Settings->CommitVolumes().IsSuccess());
    bool bReentrantRejected = false;
    const auto Reentrant = Settings->AddChangedHandler(FGamePlatformSFXUserVolumesChanged::FDelegate::CreateLambda(
        [&](const auto&) { bReentrantRejected = !Settings->SetVolume(EGamePlatformSFXSoundGroup::Effects, 0.4f).IsSuccess(); }));
    Settings->SetVolume(EGamePlatformSFXSoundGroup::Effects, 0.9f);
    TestTrue(TEXT("通知中同步重入不能乱序覆盖后续观察者"), bReentrantRejected);
    Settings->RemoveChangedHandler(Reentrant);
    TestEqual(TEXT("旧Definition兼容默认效果音分组"), GetDefault<UGamePlatformSFXDefinition>()->GetSoundGroup(), EGamePlatformSFXSoundGroup::Effects);

    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    auto* Player = NewObject<UGamePlatformSFXWorldSubsystem>(World.Get());
    auto* Effect = NewObject<UAudioComponent>(World.Get());
    auto* Ambient = NewObject<UAudioComponent>(World.Get());
    auto* Music = NewObject<UAudioComponent>(World.Get());
    const auto AddInstance = [&](UAudioComponent* Component, EGamePlatformSFXSoundGroup Group, float Base)
    {
        FGamePlatformSFXHandle Handle; Handle.Id = FGuid::NewGuid(); Handle.Generation = 1; Handle.World = World.Get();
        UGamePlatformSFXWorldSubsystem::FActiveInstance Active; Active.Handle = Handle; Active.Component = Component;
        Active.SoundGroup = Group; Active.BaseVolume = Base;
        Player->ActiveInstances.Add(Handle.Id, Active);
        return Handle;
    };
    const auto EffectHandle = AddInstance(Effect, EGamePlatformSFXSoundGroup::Effects, 0.8f);
    AddInstance(Ambient, EGamePlatformSFXSoundGroup::Ambient, 0.6f);
    AddInstance(Music, EGamePlatformSFXSoundGroup::Music, 0.4f);
    Player->HandleUserVolumesChanged(Restored->GetSnapshot());
    TestEqual(TEXT("当前效果音乘一次"), Effect->VolumeMultiplier, 0.2f);
    TestEqual(TEXT("当前环境音独立"), Ambient->VolumeMultiplier, 0.3f);
    TestEqual(TEXT("当前音乐静音"), Music->VolumeMultiplier, 0.0f);
    auto Volumes = Restored->GetSnapshot(); Volumes.Music = 1.0f;
    Player->HandleUserVolumesChanged(Volumes);
    TestEqual(TEXT("音乐解除静音恢复原始基础音量"), Music->VolumeMultiplier, 0.4f);
    Player->HandleUserVolumesChanged(Volumes);
    TestEqual(TEXT("重复混音不累乘"), Effect->VolumeMultiplier, 0.2f);
    Player->SetVolumeMultiplier(EffectHandle, 0.4f);
    TestEqual(TEXT("实例改基础音量仍乘用户增益"), Effect->VolumeMultiplier, 0.1f);
    TestEqual(TEXT("后续新声音使用当前音乐增益"), Player->GetMixedVolume(EGamePlatformSFXSoundGroup::Music, 0.7f), 0.7f);
    TStrongObjectPtr<UWorld> OtherWorld(UWorld::CreateWorld(EWorldType::Game, false));
    auto* OtherPlayer = NewObject<UGamePlatformSFXWorldSubsystem>(OtherWorld.Get());
    auto* OtherMusic = NewObject<UAudioComponent>(OtherWorld.Get());
    UGamePlatformSFXWorldSubsystem::FActiveInstance OtherActive;
    OtherActive.Handle.Id = FGuid::NewGuid(); OtherActive.Handle.Generation = 1; OtherActive.Handle.World = OtherWorld.Get();
    OtherActive.Component = OtherMusic; OtherActive.SoundGroup = EGamePlatformSFXSoundGroup::Music; OtherActive.BaseVolume = 0.7f;
    OtherPlayer->ActiveInstances.Add(OtherActive.Handle.Id, OtherActive);
    const auto FirstWorldSubscription = Settings->AddChangedHandler(FGamePlatformSFXUserVolumesChanged::FDelegate::CreateUObject(
        Player, &UGamePlatformSFXWorldSubsystem::HandleUserVolumesChanged));
    const auto OtherWorldSubscription = Settings->AddChangedHandler(FGamePlatformSFXUserVolumesChanged::FDelegate::CreateUObject(
        OtherPlayer, &UGamePlatformSFXWorldSubsystem::HandleUserVolumesChanged));
    Settings->SetVolume(EGamePlatformSFXSoundGroup::Music, 1.0f);
    TestEqual(TEXT("同GI第一世界接音量通知"), Music->VolumeMultiplier, 0.4f);
    TestEqual(TEXT("同GI另一世界独立基础增益"), OtherMusic->VolumeMultiplier, 0.7f);
    Settings->RemoveChangedHandler(FirstWorldSubscription);
    Settings->SetVolume(EGamePlatformSFXSoundGroup::Music, 0.0f);
    TestEqual(TEXT("第一世界解绑不再受通知"), Music->VolumeMultiplier, 0.4f);
    TestEqual(TEXT("解绑第一世界不影响另一世界"), OtherMusic->VolumeMultiplier, 0.0f);
    Settings->RemoveChangedHandler(OtherWorldSubscription);
    Player->bClosing = true; Volumes.Music = 0.0f; Player->HandleUserVolumesChanged(Volumes);
    TestEqual(TEXT("世界关闭拒绝迟到混音通知"), Music->VolumeMultiplier, 0.4f);
    Player->ActiveInstances.Reset(); World->DestroyWorld(false);
    OtherPlayer->ActiveInstances.Reset(); OtherWorld->DestroyWorld(false);
    IFileManager::Get().Delete(*Filename);
    return true;
}

/** 两个真实GI的Editor存储路径由生产策略生成；交错保存/重读真实文件，不触碰正式用户设置。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSFXEditorStorageIsolationTest, "GamePlatform.SFX.UserSettings.EditorStorageIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSFXEditorStorageIsolationTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> FirstGI(NewObject<UGameInstance>());
    TStrongObjectPtr<UGameInstance> SecondGI(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformSFXUserSettingsSubsystem> First(NewObject<UGamePlatformSFXUserSettingsSubsystem>(FirstGI.Get()));
    TStrongObjectPtr<UGamePlatformSFXUserSettingsSubsystem> Second(NewObject<UGamePlatformSFXUserSettingsSubsystem>(SecondGI.Get()));
    const FString FirstFilename = First->BuildStorageFilename(true);
    const FString SecondFilename = Second->BuildStorageFilename(true);
    const FString ClientFilename = First->BuildStorageFilename(false);
    const FString EditorDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EditorAudioUserSettings")));
    TestNotEqual(TEXT("两个GI使用不同音频文件"), FirstFilename, SecondFilename);
    TestEqual(TEXT("同GI路径在其服务生命周期稳定"), First->BuildStorageFilename(true), FirstFilename);
    TestTrue(TEXT("第一GI文件局限Editor专属目录"), FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(FirstFilename), EditorDirectory));
    TestTrue(TEXT("第二GI文件局限Editor专属目录"), FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(SecondFilename), EditorDirectory));
    TestEqual(TEXT("正式客户端仍共用原固定配置身份"), Second->BuildStorageFilename(false), ClientFilename);
    TestNotEqual(TEXT("Editor不使用真实客户端文件"), FirstFilename, ClientFilename);
    // 即使生产路径策略回归，测试也必须在任何写入前停止，不能为暴露缺陷而修改真实用户文件。
    if (!TestTrue(TEXT("写入前确认两个测试路径完全隔离"), FirstFilename != SecondFilename &&
        FirstFilename != ClientFilename && SecondFilename != ClientFilename &&
        FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(FirstFilename), EditorDirectory) &&
        FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(SecondFilename), EditorDirectory))) return false;
    const bool bClientFileExisted = IFileManager::Get().FileExists(*ClientFilename);
    TArray<uint8> ClientBytesBefore;
    if (bClientFileExisted && !TestTrue(TEXT("读取正式音量文件用于只读比较"), FFileHelper::LoadFileToArray(ClientBytesBefore, *ClientFilename))) return false;

    First->LoadVolumes(FirstFilename);
    Second->LoadVolumes(SecondFilename);
    TestTrue(TEXT("第一GI更新效果音"), First->SetVolume(EGamePlatformSFXSoundGroup::Effects, 0.2f).IsSuccess());
    TestTrue(TEXT("第一GI首次落盘"), First->CommitVolumes().IsSuccess());
    TestTrue(TEXT("第二GI更新音乐"), Second->SetVolume(EGamePlatformSFXSoundGroup::Music, 0.7f).IsSuccess());
    TestTrue(TEXT("第二GI首次落盘"), Second->CommitVolumes().IsSuccess());
    TestTrue(TEXT("第一GI再次更新音乐"), First->SetVolume(EGamePlatformSFXSoundGroup::Music, 0.3f).IsSuccess());
    TestTrue(TEXT("第一GI交错落盘"), First->CommitVolumes().IsSuccess());
    TestTrue(TEXT("第二GI再次更新效果音"), Second->SetVolume(EGamePlatformSFXSoundGroup::Effects, 0.9f).IsSuccess());
    TestTrue(TEXT("第二GI交错落盘"), Second->CommitVolumes().IsSuccess());
    First->LoadVolumes(FirstFilename);
    Second->LoadVolumes(SecondFilename);
    TestEqual(TEXT("第一GI保留自己效果音"), First->GetSnapshot().Effects, 0.2f);
    TestEqual(TEXT("第一GI保留自己音乐"), First->GetSnapshot().Music, 0.3f);
    TestEqual(TEXT("第二GI保留自己效果音"), Second->GetSnapshot().Effects, 0.9f);
    TestEqual(TEXT("第二GI保留自己音乐"), Second->GetSnapshot().Music, 0.7f);
    TestEqual(TEXT("正式文件存在性未改变"), IFileManager::Get().FileExists(*ClientFilename), bClientFileExisted);
    if (bClientFileExisted)
    {
        TArray<uint8> ClientBytesAfter;
        TestTrue(TEXT("正式音量文件仍可只读"), FFileHelper::LoadFileToArray(ClientBytesAfter, *ClientFilename));
        TestTrue(TEXT("正式音量文件原始字节未改变"), ClientBytesAfter == ClientBytesBefore);
    }
    IFileManager::Get().Delete(*FirstFilename);
    IFileManager::Get().Delete(*SecondFilename);
    return true;
}
#endif
