#include "Types/GamePlatformVFXCommonParameters.h"

namespace
{
    const FName PrimaryColorName(TEXT("User.PrimaryColor"));
    const FName SecondaryColorName(TEXT("User.SecondaryColor"));
    const FName CoreColorName(TEXT("User.CoreColor"));
    const FName IntensityName(TEXT("User.Intensity"));
    const FName DurationName(TEXT("User.Duration"));
    const FName RadiusName(TEXT("User.Radius"));
    const FName LengthName(TEXT("User.Length"));
    const FName WidthName(TEXT("User.Width"));
    const FName SpeedName(TEXT("User.Speed"));
    const FName SeedName(TEXT("User.Seed"));
    const FName SourcePositionName(TEXT("User.SourcePosition"));
    const FName TargetPositionName(TEXT("User.TargetPosition"));
    const FName DirectionName(TEXT("User.Direction"));
    const FName ScaleName(TEXT("User.Scale"));
}

namespace GamePlatformVFXCommonParameters
{
    const FName& PrimaryColor() { return PrimaryColorName; }
    const FName& SecondaryColor() { return SecondaryColorName; }
    const FName& CoreColor() { return CoreColorName; }
    const FName& Intensity() { return IntensityName; }
    const FName& Duration() { return DurationName; }
    const FName& Radius() { return RadiusName; }
    const FName& Length() { return LengthName; }
    const FName& Width() { return WidthName; }
    const FName& Speed() { return SpeedName; }
    const FName& Seed() { return SeedName; }
    const FName& SourcePosition() { return SourcePositionName; }
    const FName& TargetPosition() { return TargetPositionName; }
    const FName& Direction() { return DirectionName; }
    const FName& Scale() { return ScaleName; }
}
