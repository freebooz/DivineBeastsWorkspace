#include "VFX/DivineBeastsHeroVFXProfile.h"

#include "Identity/DivineBeastsProjectCatalog.h"

namespace
{
    /** 项目本地视觉语言元数据；不承担 HeroDefinitionId（英雄定义编号）真源职责。 */
    struct FZodiacVFXLanguageMetadata
    {
        const TCHAR* HeroSuffix;
        const TCHAR* HeroToken;
        const TCHAR* Shape;
        const TCHAR* Motion;
        const TCHAR* Energy;
        const TCHAR* Material;
        const TCHAR* Signature;
        const TCHAR* Impact;
        const TCHAR* Dissipation;
    };

    static const FZodiacVFXLanguageMetadata GZodiacVFXMetadata[] =
    {
        { TEXT(".Rat"), TEXT("Rat"), TEXT("DBA.VFX.Shape.Rat.SharpCrescent"), TEXT("DBA.VFX.Motion.Rat.DashAfterimage"), TEXT("DBA.VFX.Energy.Rat.ShadowSilver"), TEXT("DBA.VFX.Material.Rat.DarkMetalMist"), TEXT("DBA.VFX.Motif.Rat.FangSlash"), TEXT("DBA.VFX.Impact.Rat.CrossCut"), TEXT("DBA.VFX.Dissipation.Rat.FastCollapse") },
        { TEXT(".Ox"), TEXT("Ox"), TEXT("DBA.VFX.Shape.Ox.BroadHornRing"), TEXT("DBA.VFX.Motion.Ox.ChargeHeavyPulse"), TEXT("DBA.VFX.Energy.Ox.EarthForce"), TEXT("DBA.VFX.Material.Ox.RockDust"), TEXT("DBA.VFX.Motif.Ox.HornShockwave"), TEXT("DBA.VFX.Impact.Ox.GroundFracture"), TEXT("DBA.VFX.Dissipation.Ox.DustSettle") },
        { TEXT(".Tiger"), TEXT("Tiger"), TEXT("DBA.VFX.Shape.Tiger.ClawDiagonal"), TEXT("DBA.VFX.Motion.Tiger.PounceBurst"), TEXT("DBA.VFX.Energy.Tiger.WhiteGold"), TEXT("DBA.VFX.Material.Tiger.EnergyStripe"), TEXT("DBA.VFX.Motif.Tiger.TigerClaw"), TEXT("DBA.VFX.Impact.Tiger.RoarBurst"), TEXT("DBA.VFX.Dissipation.Tiger.StripeScatter") },
        { TEXT(".Rabbit"), TEXT("Rabbit"), TEXT("DBA.VFX.Shape.Rabbit.MoonArcRing"), TEXT("DBA.VFX.Motion.Rabbit.LeapFloat"), TEXT("DBA.VFX.Energy.Rabbit.JadeMoon"), TEXT("DBA.VFX.Material.Rabbit.TranslucentJade"), TEXT("DBA.VFX.Motif.Rabbit.JadeMoonArc"), TEXT("DBA.VFX.Impact.Rabbit.SoftRing"), TEXT("DBA.VFX.Dissipation.Rabbit.JadePetalRise") },
        { TEXT(".Dragon"), TEXT("Dragon"), TEXT("DBA.VFX.Shape.Dragon.SpiralScale"), TEXT("DBA.VFX.Motion.Dragon.OrbitDiveAscend"), TEXT("DBA.VFX.Energy.Dragon.AzureCloud"), TEXT("DBA.VFX.Material.Dragon.ScaleCloudEnergy"), TEXT("DBA.VFX.Motif.Dragon.DragonHeadCloud"), TEXT("DBA.VFX.Impact.Dragon.CloudThunderBurst"), TEXT("DBA.VFX.Dissipation.Dragon.CloudSpiralFade") },
        { TEXT(".Snake"), TEXT("Snake"), TEXT("DBA.VFX.Shape.Snake.SCurveCoil"), TEXT("DBA.VFX.Motion.Snake.WindingTracking"), TEXT("DBA.VFX.Energy.Snake.MysticScale"), TEXT("DBA.VFX.Material.Snake.IridescentScale"), TEXT("DBA.VFX.Motif.Snake.SerpentCurve"), TEXT("DBA.VFX.Impact.Snake.CoilBurst"), TEXT("DBA.VFX.Dissipation.Snake.ScaleFade") },
        { TEXT(".Horse"), TEXT("Horse"), TEXT("DBA.VFX.Shape.Horse.LineHoofLightning"), TEXT("DBA.VFX.Motion.Horse.SprintCharge"), TEXT("DBA.VFX.Energy.Horse.Thunder"), TEXT("DBA.VFX.Material.Horse.ElectricArc"), TEXT("DBA.VFX.Motif.Horse.ThunderHoof"), TEXT("DBA.VFX.Impact.Horse.LightningRing"), TEXT("DBA.VFX.Dissipation.Horse.ArcGrounding") },
        { TEXT(".Goat"), TEXT("Goat"), TEXT("DBA.VFX.Shape.Goat.HaloHornDome"), TEXT("DBA.VFX.Motion.Goat.ExpandRise"), TEXT("DBA.VFX.Energy.Goat.JadeGuardian"), TEXT("DBA.VFX.Material.Goat.TranslucentShield"), TEXT("DBA.VFX.Motif.Goat.JadeHornRune"), TEXT("DBA.VFX.Impact.Goat.ShieldRipple"), TEXT("DBA.VFX.Dissipation.Goat.JadeShardFade") },
        { TEXT(".Monkey"), TEXT("Monkey"), TEXT("DBA.VFX.Shape.Monkey.StaffArcAfterimage"), TEXT("DBA.VFX.Motion.Monkey.TrickMultiDash"), TEXT("DBA.VFX.Energy.Monkey.GoldenStroke"), TEXT("DBA.VFX.Material.Monkey.EnergyInkSmoke"), TEXT("DBA.VFX.Motif.Monkey.StaffArc"), TEXT("DBA.VFX.Impact.Monkey.MultiHitPop"), TEXT("DBA.VFX.Dissipation.Monkey.SmokePop") },
        { TEXT(".Rooster"), TEXT("Rooster"), TEXT("DBA.VFX.Shape.Rooster.FanFeatherWave"), TEXT("DBA.VFX.Motion.Rooster.SpreadPulse"), TEXT("DBA.VFX.Energy.Rooster.GoldenDawn"), TEXT("DBA.VFX.Material.Rooster.FeatherLight"), TEXT("DBA.VFX.Motif.Rooster.GoldenFeather"), TEXT("DBA.VFX.Impact.Rooster.SonicRing"), TEXT("DBA.VFX.Dissipation.Rooster.FeatherScatter") },
        { TEXT(".Dog"), TEXT("Dog"), TEXT("DBA.VFX.Shape.Dog.MarkPawGuardianRing"), TEXT("DBA.VFX.Motion.Dog.TrackLeap"), TEXT("DBA.VFX.Energy.Dog.SkyGuardian"), TEXT("DBA.VFX.Material.Dog.SpiritLight"), TEXT("DBA.VFX.Motif.Dog.TrackingMark"), TEXT("DBA.VFX.Impact.Dog.GuardianBiteBurst"), TEXT("DBA.VFX.Dissipation.Dog.TrailMarkFade") },
        { TEXT(".Boar"), TEXT("Boar"), TEXT("DBA.VFX.Shape.Boar.WideArcPressureRing"), TEXT("DBA.VFX.Motion.Boar.RamBurstSpread"), TEXT("DBA.VFX.Energy.Boar.HeavyForce"), TEXT("DBA.VFX.Material.Boar.DustPressure"), TEXT("DBA.VFX.Motif.Boar.ManePressureWave"), TEXT("DBA.VFX.Impact.Boar.WideShockwave"), TEXT("DBA.VFX.Dissipation.Boar.HeavyDustFade") }
    };

    const FZodiacVFXLanguageMetadata* FindMetadata(const FName HeroDefinitionId)
    {
        const FString HeroId = HeroDefinitionId.ToString();
        for (const FZodiacVFXLanguageMetadata& Metadata : GZodiacVFXMetadata)
        {
            if (HeroId.EndsWith(Metadata.HeroSuffix, ESearchCase::CaseSensitive))
            {
                return &Metadata;
            }
        }
        return nullptr;
    }

    FDivineBeastsHeroVFXProfile BuildProfile(
        const FName HeroDefinitionId,
        const FZodiacVFXLanguageMetadata& Metadata)
    {
        FDivineBeastsHeroVFXProfile Profile;
        Profile.HeroDefinitionId = HeroDefinitionId;
        Profile.ProfileId = FName(*FString::Printf(
            TEXT("Presentation.VFX.Hero.Zodiac.%s.Default"),
            Metadata.HeroToken));
        Profile.ShapeLanguageId = FName(Metadata.Shape);
        Profile.MotionLanguageId = FName(Metadata.Motion);
        Profile.EnergyLanguageId = FName(Metadata.Energy);
        Profile.MaterialLanguageId = FName(Metadata.Material);
        Profile.SignatureMotifId = FName(Metadata.Signature);
        Profile.ImpactLanguageId = FName(Metadata.Impact);
        Profile.DissipationLanguageId = FName(Metadata.Dissipation);
        return Profile;
    }

    const TArray<FDivineBeastsHeroVFXProfile>& Profiles()
    {
        static const TArray<FDivineBeastsHeroVFXProfile> Value = []
        {
            TArray<FDivineBeastsHeroVFXProfile> Result;
            const TArray<FName>& HeroIds = FDivineBeastsProjectCatalog::GetHeroDefinitionIds();
            Result.Reserve(HeroIds.Num());

            for (const FName HeroId : HeroIds)
            {
                if (const FZodiacVFXLanguageMetadata* Metadata = FindMetadata(HeroId))
                {
                    Result.Add(BuildProfile(HeroId, *Metadata));
                }
            }
            return Result;
        }();
        return Value;
    }
}

bool FDivineBeastsHeroVFXProfile::IsValid() const
{
    return !HeroDefinitionId.IsNone()
        && !ProfileId.IsNone()
        && !ShapeLanguageId.IsNone()
        && !MotionLanguageId.IsNone()
        && !EnergyLanguageId.IsNone()
        && !MaterialLanguageId.IsNone()
        && !SignatureMotifId.IsNone()
        && !ImpactLanguageId.IsNone()
        && !DissipationLanguageId.IsNone();
}

const TArray<FDivineBeastsHeroVFXProfile>&
FDivineBeastsHeroVFXProfileCatalog::GetProfiles()
{
    return Profiles();
}

const FDivineBeastsHeroVFXProfile*
FDivineBeastsHeroVFXProfileCatalog::Find(const FName HeroDefinitionId)
{
    return Profiles().FindByPredicate(
        [HeroDefinitionId](const FDivineBeastsHeroVFXProfile& Profile)
        {
            return Profile.HeroDefinitionId == HeroDefinitionId;
        });
}

FName FDivineBeastsHeroVFXProfileCatalog::GetDefaultProfileId(
    const FName HeroDefinitionId)
{
    const FDivineBeastsHeroVFXProfile* Profile = Find(HeroDefinitionId);
    return Profile ? Profile->ProfileId : NAME_None;
}
