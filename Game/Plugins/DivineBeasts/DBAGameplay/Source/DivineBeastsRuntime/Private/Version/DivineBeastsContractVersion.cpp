#include "Version/DivineBeastsContractVersion.h"

#include "Version/DivineBeastsGeneratedCatalogAdapter.h"

namespace
{
    bool ParseSemVer(const FString& Version, int32& Major, int32& Minor, int32& Patch)
    {
        TArray<FString> Parts;
        Version.ParseIntoArray(Parts, TEXT("."), true);
        if (Parts.Num() != 3)
        {
            return false;
        }

        return LexTryParseString(Major, *Parts[0]) &&
               LexTryParseString(Minor, *Parts[1]) &&
               LexTryParseString(Patch, *Parts[2]) &&
               Major >= 0 && Minor >= 0 && Patch >= 0;
    }

    int32 CompareSemVer(const FString& A, const FString& B, bool& bValid)
    {
        int32 AMajor = 0, AMinor = 0, APatch = 0;
        int32 BMajor = 0, BMinor = 0, BPatch = 0;
        bValid =
            ParseSemVer(A, AMajor, AMinor, APatch) &&
            ParseSemVer(B, BMajor, BMinor, BPatch);
        if (!bValid)
        {
            return 0;
        }
        if (AMajor != BMajor) return AMajor < BMajor ? -1 : 1;
        if (AMinor != BMinor) return AMinor < BMinor ? -1 : 1;
        if (APatch != BPatch) return APatch < BPatch ? -1 : 1;
        return 0;
    }
}

FString FDivineBeastsContractVersion::GetCurrentVersion()
{
    return FDivineBeastsGeneratedCatalogAdapter::GetContractVersion();
}

int32 FDivineBeastsContractVersion::GetCatalogVersion()
{
    return FDivineBeastsGeneratedCatalogAdapter::GetCatalogVersion();
}

FString FDivineBeastsContractVersion::GetGeneratedRevision()
{
    return FDivineBeastsGeneratedCatalogAdapter::GetGeneratedRevision();
}

FDivineBeastsContractCompatibilitySummary
FDivineBeastsContractVersion::GetCompatibilitySummary()
{
    FDivineBeastsContractCompatibilitySummary Summary;
    Summary.CurrentVersion = GetCurrentVersion();
    Summary.CatalogVersion = GetCatalogVersion();
    Summary.GeneratedRevision = GetGeneratedRevision();
    Summary.ClientServerMinimumVersion =
        FDivineBeastsGeneratedCatalogAdapter::GetClientServerMinimumVersion();
    Summary.ClientServerMaximumExclusiveVersion =
        FDivineBeastsGeneratedCatalogAdapter::GetClientServerMaximumExclusiveVersion();
    Summary.ServerBackendMinimumVersion =
        FDivineBeastsGeneratedCatalogAdapter::GetServerBackendMinimumVersion();
    Summary.ServerBackendMaximumExclusiveVersion =
        FDivineBeastsGeneratedCatalogAdapter::GetServerBackendMaximumExclusiveVersion();
    return Summary;
}

bool FDivineBeastsContractVersion::IsClientServerCompatible(const FString& Version)
{
    return IsInRange(
        Version,
        FDivineBeastsGeneratedCatalogAdapter::GetClientServerMinimumVersion(),
        FDivineBeastsGeneratedCatalogAdapter::GetClientServerMaximumExclusiveVersion());
}

bool FDivineBeastsContractVersion::IsServerBackendCompatible(const FString& Version)
{
    return IsInRange(
        Version,
        FDivineBeastsGeneratedCatalogAdapter::GetServerBackendMinimumVersion(),
        FDivineBeastsGeneratedCatalogAdapter::GetServerBackendMaximumExclusiveVersion());
}

bool FDivineBeastsContractVersion::IsInRange(
    const FString& Version,
    const FString& MinimumInclusive,
    const FString& MaximumExclusive)
{
    bool bMinimumValid = false;
    const int32 MinimumComparison =
        CompareSemVer(Version, MinimumInclusive, bMinimumValid);
    if (!bMinimumValid || MinimumComparison < 0)
    {
        return false;
    }

    bool bMaximumValid = false;
    const int32 MaximumComparison =
        CompareSemVer(Version, MaximumExclusive, bMaximumValid);
    return bMaximumValid && MaximumComparison < 0;
}
