// 三路音量只由SFX领域持久化，通知现有World执行器和页面；无Tick、Timer或第二套播放机制。
#include "Subsystems/GamePlatformSFXUserSettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

namespace
{
const TCHAR* AudioSettingsSection = TEXT("GamePlatformSFX.UserVolumes");
float ReadGain(const FConfigFile& File, const TCHAR* Key)
{
    float Value = 1.0f;
    File.GetFloat(AudioSettingsSection, Key, Value);
    return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 1.0f;
}
}

IGamePlatformSFXUserSettingsService* IGamePlatformSFXUserSettingsService::Get(UGameInstance& GameInstance)
{
    check(IsInGameThread());
    return GameInstance.GetSubsystem<UGamePlatformSFXUserSettingsSubsystem>();
}

bool UGamePlatformSFXUserSettingsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const auto* GI = Cast<UGameInstance>(Outer);
    return GI && !GI->IsDedicatedServerInstance() && !IsRunningCommandlet() &&
        (!GI->GetWorld() || GI->GetWorld()->GetNetMode() != NM_DedicatedServer);
}
void UGamePlatformSFXUserSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bClosing = false;
    LoadVolumes(BuildStorageFilename(GIsEditor));
}
FString UGamePlatformSFXUserSettingsSubsystem::BuildStorageFilename(bool bForEditor) const
{
    if (bForEditor)
        return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EditorAudioUserSettings"), EditorStorageId.ToString(EGuidFormats::Digits),
            TEXT("GamePlatformAudioUserSettings.ini"));
    const FString Directory = GGameUserSettingsIni.IsEmpty() ? FPaths::GeneratedConfigDir() : FPaths::GetPath(GGameUserSettingsIni);
    return FPaths::Combine(Directory, TEXT("GamePlatformAudioUserSettings.ini"));
}
void UGamePlatformSFXUserSettingsSubsystem::Deinitialize()
{
    const auto Result = CommitVolumes();
    if (!Result.IsSuccess()) UE_LOG(LogTemp, Warning, TEXT("[SFX] Audio settings final save failed: %s"), *Result.Code.ToString());
    bClosing = true;
    Changed.Clear();
    Super::Deinitialize();
}
void UGamePlatformSFXUserSettingsSubsystem::LoadVolumes(const FString& Filename)
{
    StorageFilename = Filename;
    FConfigFile File;
    File.Read(Filename);
    Snapshot = {};
    Snapshot.Effects = ReadGain(File, TEXT("Effects"));
    Snapshot.Ambient = ReadGain(File, TEXT("Ambient"));
    Snapshot.Music = ReadGain(File, TEXT("Music"));
    bDirty = false;
}
bool UGamePlatformSFXUserSettingsSubsystem::SaveVolumes(const FGamePlatformSFXUserVolumes& Candidate) const
{
    if (StorageFilename.IsEmpty()) return false;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(StorageFilename), true);
    FConfigFile File;
    File.SetFloat(AudioSettingsSection, TEXT("Effects"), Candidate.Effects);
    File.SetFloat(AudioSettingsSection, TEXT("Ambient"), Candidate.Ambient);
    File.SetFloat(AudioSettingsSection, TEXT("Music"), Candidate.Music);
    FString Text;
    File.WriteToString(Text);
    // 专属本地文件不走远端Config镜像；显式返回真实文件写入结果，不能把全局NoSave跳过误当保存成功。
    return FFileHelper::SaveStringToFile(Text, *StorageFilename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
FGamePlatformSFXUserVolumes UGamePlatformSFXUserSettingsSubsystem::GetSnapshot() const
{
    check(IsInGameThread());
    return Snapshot;
}
FGamePlatformResult UGamePlatformSFXUserSettingsSubsystem::SetVolume(EGamePlatformSFXSoundGroup Group, float Value)
{
    check(IsInGameThread());
    if (bClosing || bPublishing)
        return FGamePlatformResult::Failure(TEXT("SFX.UserSettings.Unavailable"), TEXT("音量设置正在通知或关闭，请稍后重试。"));
    if (!FMath::IsFinite(Value) || Value < 0.0f || Value > 1.0f)
        return FGamePlatformResult::Failure(TEXT("SFX.UserSettings.InvalidVolume"), TEXT("音量必须介于0%与100%之间。"));
    auto Candidate = Snapshot;
    switch (Group)
    {
    case EGamePlatformSFXSoundGroup::Effects: Candidate.Effects = Value; break;
    case EGamePlatformSFXSoundGroup::Ambient: Candidate.Ambient = Value; break;
    case EGamePlatformSFXSoundGroup::Music: Candidate.Music = Value; break;
    default: return FGamePlatformResult::Failure(TEXT("SFX.UserSettings.InvalidGroup"), TEXT("未知音频分组。"));
    }
    if (Snapshot.GetGain(Group) == Value) return FGamePlatformResult::Success();
    Candidate.Revision = Snapshot.Revision + 1;
    Snapshot = Candidate;
    bDirty = true;
    const TGuardValue<bool> Publishing(bPublishing, true);
    Changed.Broadcast(Candidate);
    return FGamePlatformResult::Success();
}
FGamePlatformResult UGamePlatformSFXUserSettingsSubsystem::CommitVolumes()
{
    check(IsInGameThread());
    if (!bDirty) return FGamePlatformResult::Success();
    if (!SaveVolumes(Snapshot))
        return FGamePlatformResult::Failure(TEXT("SFX.UserSettings.SaveFailed"), TEXT("音量已生效，但本地保存失败，请重试。"));
    bDirty = false;
    return FGamePlatformResult::Success();
}
FDelegateHandle UGamePlatformSFXUserSettingsSubsystem::AddChangedHandler(const FGamePlatformSFXUserVolumesChanged::FDelegate& Handler)
{
    check(IsInGameThread());
    return bClosing ? FDelegateHandle() : Changed.Add(Handler);
}
void UGamePlatformSFXUserSettingsSubsystem::RemoveChangedHandler(FDelegateHandle Handle)
{
    check(IsInGameThread());
    Changed.Remove(Handle);
}
