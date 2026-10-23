#include "GamePlatformSurfaceMaterialAuthoringLibrary.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpression.h"

bool UGamePlatformSurfaceMaterialAuthoringLibrary::ConnectDisplacement(UMaterialExpression* Expression)
{
    check(IsInGameThread());
    return IsValid(Expression) &&
        UMaterialEditingLibrary::ConnectMaterialProperty(Expression, FString(), MP_Displacement);
}
