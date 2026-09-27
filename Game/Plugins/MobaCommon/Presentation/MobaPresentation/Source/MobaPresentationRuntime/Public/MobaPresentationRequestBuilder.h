#pragma once

#include "GamePlatformPresentationTypes.h"
#include "Types/MobaPresentationTypes.h"

/** FMobaPresentationRequestBuilder（MOBA到平台表现请求构建器）。 */
class MOBAPRESENTATIONRUNTIME_API FMobaPresentationRequestBuilder
{
public:
    static FGamePlatformPresentationRequest Build(
        const FMobaPresentationAdaptedFact& Fact,
        int32 RequestGeneration);
};
