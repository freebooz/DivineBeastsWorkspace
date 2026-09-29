#include "Resolution/GamePlatformSettingsResolver.h"

#include "Validation/GamePlatformSettingsValidation.h"

namespace
{
    void GetPriority(
        const EGamePlatformSettingRuntimeScope Runtime,
        TArray<EGamePlatformSettingLayer>& OutLayers)
    {
        OutLayers.Reset();

        if (Runtime == EGamePlatformSettingRuntimeScope::Server)
        {
            OutLayers = {
                EGamePlatformSettingLayer::PlatformDefault,
                EGamePlatformSettingLayer::ProjectDefault,
                EGamePlatformSettingLayer::ProviderDefault,
                EGamePlatformSettingLayer::ServerDefault,
                EGamePlatformSettingLayer::Deployment,
                EGamePlatformSettingLayer::Environment,
                EGamePlatformSettingLayer::CommandLine,
                EGamePlatformSettingLayer::Session
            };
            return;
        }

        OutLayers = {
            EGamePlatformSettingLayer::PlatformDefault,
            EGamePlatformSettingLayer::ProjectDefault,
            EGamePlatformSettingLayer::ProviderDefault,
            EGamePlatformSettingLayer::User,
            EGamePlatformSettingLayer::Session
        };
    }
}

FGamePlatformResult FGamePlatformSettingsResolver::Resolve(
    const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
    const FGamePlatformSettingsLayers& Layers,
    const EGamePlatformSettingRuntimeScope CurrentRuntime,
    const FGamePlatformSettingsSnapshot& Previous,
    const EGamePlatformSettingsChangeReason Reason,
    const int32 SchemaVersion,
    const bool bDirty,
    FGamePlatformSettingsSnapshot& OutSnapshot,
    FGamePlatformSettingsChangeSet& OutChanges)
{
    TArray<EGamePlatformSettingLayer> Priority;
    GetPriority(CurrentRuntime, Priority);

    FGamePlatformSettingsSnapshot Candidate;
    Candidate.SchemaVersion = SchemaVersion;
    Candidate.Revision = Previous.Revision;
    Candidate.bDirty = bDirty;
    Candidate.bSaveInFlight = Previous.bSaveInFlight;
    Candidate.LastResult = FGamePlatformResult::Success();

    TArray<FName> SettingIds;
    Descriptors.GetKeys(SettingIds);
    SettingIds.Sort(FNameLexicalLess());

    for (const FName SettingId : SettingIds)
    {
        const FGamePlatformSettingDescriptor& Descriptor =
            Descriptors.FindChecked(SettingId);

        FGamePlatformSettingValue Resolved = Descriptor.DefaultValue;
        EGamePlatformSettingLayer Source = Descriptor.DefaultLayer;

        for (const EGamePlatformSettingLayer Layer : Priority)
        {
            const TMap<FName, FGamePlatformSettingValue>* Values =
                Layers.Find(Layer);
            if (!Values)
            {
                continue;
            }

            const FGamePlatformSettingValue* CandidateValue =
                Values->Find(SettingId);
            if (!CandidateValue)
            {
                continue;
            }

            if (!FGamePlatformSettingsValidation::IsLayerAllowed(
                    Descriptor, Layer, CurrentRuntime))
            {
                return FGamePlatformResult::Failure(
                    TEXT("SettingsLayerNotAllowed"),
                    FString::Printf(
                        TEXT("设置 %s 出现在当前端侧不允许的配置层。"),
                        *SettingId.ToString()));
            }

            const FGamePlatformResult Validation =
                FGamePlatformSettingsValidation::ValidateValue(
                    Descriptor, *CandidateValue);
            if (!Validation.IsSuccess())
            {
                return FGamePlatformResult::Failure(
                    TEXT("SettingsLayerValueInvalid"),
                    FString::Printf(
                        TEXT("设置 %s 的覆盖值未通过校验：%s"),
                        *SettingId.ToString(),
                        *Validation.Message));
            }

            Resolved = *CandidateValue;
            Source = Layer;
        }

        FGamePlatformResolvedSetting Item;
        Item.SettingId = SettingId;
        Item.Value = Resolved;
        Item.SourceLayer = Source;

        const FGamePlatformResolvedSetting* Old =
            Previous.Values.Find(SettingId);
        const bool bChanged =
            !Old || !Old->Value.Equals(Resolved) ||
            Old->SourceLayer != Source;

        Item.Revision =
            bChanged
                ? (Old ? Old->Revision + 1 : 1)
                : Old->Revision;

        Candidate.Values.Add(SettingId, Item);

        if (bChanged)
        {
            FGamePlatformSettingChange Change;
            Change.SettingId = SettingId;
            Change.Category = Descriptor.Category;
            Change.OldValue = Old ? Old->Value : Descriptor.DefaultValue;
            Change.NewValue = Resolved;
            OutChanges.Changes.Add(MoveTemp(Change));
        }
    }

    // Provider移除时，旧Snapshot中不再存在的设置也进入ChangeSet，便于消费者清理缓存。
    for (const TPair<FName, FGamePlatformResolvedSetting>& OldPair : Previous.Values)
    {
        if (!Candidate.Values.Contains(OldPair.Key))
        {
            FGamePlatformSettingChange Change;
            Change.SettingId = OldPair.Key;
            Change.OldValue = OldPair.Value.Value;
            Change.NewValue = FGamePlatformSettingValue();
            OutChanges.Changes.Add(MoveTemp(Change));
        }
    }

    if (!OutChanges.Changes.IsEmpty())
    {
        ++Candidate.Revision;
    }

    OutChanges.Reason = Reason;
    OutChanges.Revision = Candidate.Revision;
    OutSnapshot = MoveTemp(Candidate);
    return FGamePlatformResult::Success();
}
