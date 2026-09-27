#include "ViewModels/GamePlatformViewModelBase.h"

void UGamePlatformViewModelBase::BeginPage()
{
    ++PageGeneration;
    bPageActive = true;
    MarkStateChanged();
}

void UGamePlatformViewModelBase::EndPage()
{
    bPageActive = false;
    ++PageGeneration;
}

void UGamePlatformViewModelBase::MarkStateChanged()
{
    ++Revision;
    OnViewStateChanged.Broadcast(Revision, PageGeneration);
}

bool UGamePlatformViewModelBase::IsCallbackCurrent(
    int32 ExpectedRevision,
    int32 ExpectedPageGeneration) const
{
    return bPageActive &&
           Revision == ExpectedRevision &&
           PageGeneration == ExpectedPageGeneration;
}
