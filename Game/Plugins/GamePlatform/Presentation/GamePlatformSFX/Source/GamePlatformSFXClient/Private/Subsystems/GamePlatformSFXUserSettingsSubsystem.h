// 音频设置唯一所有者：仅拥有本地持久值和变化事件，不播放音频、不依赖项目层或显示设置。
#pragma once
#include "Interfaces/IGamePlatformSFXUserSettingsService.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamePlatformSFXUserSettingsSubsystem.generated.h"

UCLASS()
class UGamePlatformSFXUserSettingsSubsystem final : public UGameInstanceSubsystem, public IGamePlatformSFXUserSettingsService
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual FGamePlatformSFXUserVolumes GetSnapshot() const override;
    virtual FGamePlatformResult SetVolume(EGamePlatformSFXSoundGroup Group, float Value) override;
    virtual FGamePlatformResult CommitVolumes() override;
    virtual FDelegateHandle AddChangedHandler(const FGamePlatformSFXUserVolumesChanged::FDelegate& Handler) override;
    virtual void RemoveChangedHandler(FDelegateHandle Handle) override;
private:
    friend class FGamePlatformSFXUserSettingsTest;
    friend class FGamePlatformSFXEditorStorageIsolationTest;
    /** Editor/PIE每GI独立路径；普通客户端继续使用设备持久路径，不让测试覆盖真实用户音量。 */
    FString BuildStorageFilename(bool bForEditor) const;
    /** 专属INI，避免显示设置UGameUserSettings保存覆盖音频文件；测试可注入独立临时文件。 */
    void LoadVolumes(const FString& Filename);
    bool SaveVolumes(const FGamePlatformSFXUserVolumes& Candidate) const;
    FString StorageFilename;
    /** 非反射成员每个Subsystem实例单独生成；该Subsystem在所属GI内唯一，不跨进程复用。 */
    FGuid EditorStorageId = FGuid::NewGuid();
    FGamePlatformSFXUserVolumes Snapshot;
    FGamePlatformSFXUserVolumesChanged Changed;
    bool bClosing = false;
    bool bPublishing = false;
    bool bDirty = false;
};
