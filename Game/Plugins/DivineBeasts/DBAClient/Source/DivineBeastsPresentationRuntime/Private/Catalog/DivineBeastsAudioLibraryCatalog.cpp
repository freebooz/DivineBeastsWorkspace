// SSLM第二批客户端音频资源库目录：只提供语义→Definition身份映射，不在项目层播放或加载音源。
// 按UI/Footsteps/Creatures/Spells/Ambience等类别按需构建，独立原子预载，避免新手村之外加载全部声音。
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "Identity/DivineBeastsProjectCatalog.h"

FGamePlatformPresentationCatalogFragment FDivineBeastsPresentationProjectCatalog::BuildAudioLibraryFragment(FName Category)
{
    FGamePlatformPresentationCatalogFragment Fragment;
    Fragment.FragmentId = FName(*(FString(TEXT("DBA.Audio.Library.")) + Category.ToString()));
    Fragment.Revision = 1;
    Fragment.Scope = EGamePlatformPresentationCatalogScope::Project;
    Fragment.OwnerScopeId = FDivineBeastsProjectCatalog::GetProjectId();
    Fragment.LifecycleScope = EGamePlatformPresentationContextScope::World;
    struct FEntry
    {
        const TCHAR* Category;
        const TCHAR* ClipId;
        const TCHAR* SemanticTag;
        const TCHAR* DefinitionId;
    };
    // 对照SSLM_AudioLibraryExpansion_20261018.json（32项）生成；严禁映射任意资产路径。
    static const FEntry Entries[] =
    {
        { TEXT("Footsteps"), TEXT("Footstep.LeavesA"), TEXT("Presentation.SSLM.AudioLibrary.Footstep.LeavesA"), TEXT("presentation.dba.audiolibrary.footstep.leavesa@1") },
        { TEXT("Footsteps"), TEXT("Footstep.LeavesB"), TEXT("Presentation.SSLM.AudioLibrary.Footstep.LeavesB"), TEXT("presentation.dba.audiolibrary.footstep.leavesb@1") },
        { TEXT("Footsteps"), TEXT("Footstep.Gravel"), TEXT("Presentation.SSLM.AudioLibrary.Footstep.Gravel"), TEXT("presentation.dba.audiolibrary.footstep.gravel@1") },
        { TEXT("Footsteps"), TEXT("Footstep.Mud"), TEXT("Presentation.SSLM.AudioLibrary.Footstep.Mud"), TEXT("presentation.dba.audiolibrary.footstep.mud@1") },
        { TEXT("Footsteps"), TEXT("Footstep.Stone"), TEXT("Presentation.SSLM.AudioLibrary.Footstep.Stone"), TEXT("presentation.dba.audiolibrary.footstep.stone@1") },
        { TEXT("Footsteps"), TEXT("Footstep.Wood"), TEXT("Presentation.SSLM.AudioLibrary.Footstep.Wood"), TEXT("presentation.dba.audiolibrary.footstep.wood@1") },
        { TEXT("Water"), TEXT("Water.SplashA"), TEXT("Presentation.SSLM.AudioLibrary.Water.SplashA"), TEXT("presentation.dba.audiolibrary.water.splasha@1") },
        { TEXT("Water"), TEXT("Water.SplashB"), TEXT("Presentation.SSLM.AudioLibrary.Water.SplashB"), TEXT("presentation.dba.audiolibrary.water.splashb@1") },
        { TEXT("Ambience"), TEXT("Ambience.WaterStreamA"), TEXT("Presentation.SSLM.AudioLibrary.Ambience.WaterStreamA"), TEXT("presentation.dba.audiolibrary.ambience.waterstreama@1") },
        { TEXT("Ambience"), TEXT("Ambience.WaterStreamB"), TEXT("Presentation.SSLM.AudioLibrary.Ambience.WaterStreamB"), TEXT("presentation.dba.audiolibrary.ambience.waterstreamb@1") },
        { TEXT("World"), TEXT("World.WoodDoorClose"), TEXT("Presentation.SSLM.AudioLibrary.World.WoodDoorClose"), TEXT("presentation.dba.audiolibrary.world.wooddoorclose@1") },
        { TEXT("Combat"), TEXT("Combat.WoodHit"), TEXT("Presentation.SSLM.AudioLibrary.Combat.WoodHit"), TEXT("presentation.dba.audiolibrary.combat.woodhit@1") },
        { TEXT("Combat"), TEXT("Combat.MetalHit"), TEXT("Presentation.SSLM.AudioLibrary.Combat.MetalHit"), TEXT("presentation.dba.audiolibrary.combat.metalhit@1") },
        { TEXT("World"), TEXT("World.WoodBreak"), TEXT("Presentation.SSLM.AudioLibrary.World.WoodBreak"), TEXT("presentation.dba.audiolibrary.world.woodbreak@1") },
        { TEXT("Combat"), TEXT("Combat.SwordClashA"), TEXT("Presentation.SSLM.AudioLibrary.Combat.SwordClashA"), TEXT("presentation.dba.audiolibrary.combat.swordclasha@1") },
        { TEXT("Combat"), TEXT("Combat.SwordClashB"), TEXT("Presentation.SSLM.AudioLibrary.Combat.SwordClashB"), TEXT("presentation.dba.audiolibrary.combat.swordclashb@1") },
        { TEXT("Combat"), TEXT("Combat.SwishLight"), TEXT("Presentation.SSLM.AudioLibrary.Combat.SwishLight"), TEXT("presentation.dba.audiolibrary.combat.swishlight@1") },
        { TEXT("Combat"), TEXT("Combat.SwishHeavy"), TEXT("Presentation.SSLM.AudioLibrary.Combat.SwishHeavy"), TEXT("presentation.dba.audiolibrary.combat.swishheavy@1") },
        { TEXT("Creatures"), TEXT("Creature.Howl"), TEXT("Presentation.SSLM.AudioLibrary.Creature.Howl"), TEXT("presentation.dba.audiolibrary.creature.howl@1") },
        { TEXT("Creatures"), TEXT("Creature.Bark"), TEXT("Presentation.SSLM.AudioLibrary.Creature.Bark"), TEXT("presentation.dba.audiolibrary.creature.bark@1") },
        { TEXT("Creatures"), TEXT("Creature.Roar"), TEXT("Presentation.SSLM.AudioLibrary.Creature.Roar"), TEXT("presentation.dba.audiolibrary.creature.roar@1") },
        { TEXT("Creatures"), TEXT("Creature.Grunt"), TEXT("Presentation.SSLM.AudioLibrary.Creature.Grunt"), TEXT("presentation.dba.audiolibrary.creature.grunt@1") },
        { TEXT("UI"), TEXT("UI.BookFlip"), TEXT("Presentation.SSLM.AudioLibrary.UI.BookFlip"), TEXT("presentation.dba.audiolibrary.ui.bookflip@1") },
        { TEXT("World"), TEXT("World.DoorOpen"), TEXT("Presentation.SSLM.AudioLibrary.World.DoorOpen"), TEXT("presentation.dba.audiolibrary.world.dooropen@1") },
        { TEXT("UI"), TEXT("UI.Coins"), TEXT("Presentation.SSLM.AudioLibrary.UI.Coins"), TEXT("presentation.dba.audiolibrary.ui.coins@1") },
        { TEXT("Combat"), TEXT("Combat.KnifeSlice"), TEXT("Presentation.SSLM.AudioLibrary.Combat.KnifeSlice"), TEXT("presentation.dba.audiolibrary.combat.knifeslice@1") },
        { TEXT("Spells"), TEXT("Spell.ArcaneCast"), TEXT("Presentation.SSLM.AudioLibrary.Spell.ArcaneCast"), TEXT("presentation.dba.audiolibrary.spell.arcanecast@1") },
        { TEXT("UI"), TEXT("UI.CoinsHeavy"), TEXT("Presentation.SSLM.AudioLibrary.UI.CoinsHeavy"), TEXT("presentation.dba.audiolibrary.ui.coinsheavy@1") },
        { TEXT("Creatures"), TEXT("Creature.Wolfman"), TEXT("Presentation.SSLM.AudioLibrary.Creature.Wolfman"), TEXT("presentation.dba.audiolibrary.creature.wolfman@1") },
        { TEXT("Ambience"), TEXT("Ambience.Birds"), TEXT("Presentation.SSLM.AudioLibrary.Ambience.Birds"), TEXT("presentation.dba.audiolibrary.ambience.birds@1") },
        { TEXT("Spells"), TEXT("Spell.Healing"), TEXT("Presentation.SSLM.AudioLibrary.Spell.Healing"), TEXT("presentation.dba.audiolibrary.spell.healing@1") },
        { TEXT("Spells"), TEXT("Spell.Teleport"), TEXT("Presentation.SSLM.AudioLibrary.Spell.Teleport"), TEXT("presentation.dba.audiolibrary.spell.teleport@1") },
    };
    for (const FEntry& Audio : Entries)
    {
        if (Category != FName(Audio.Category)) continue;
        const FGameplayTag Semantic = FGameplayTag::RequestGameplayTag(FName(Audio.SemanticTag), false);
        if (!Semantic.IsValid())
        {
            // 必要标签不完整时整个分类失败关闭，不允许半套目录发布。
            Fragment.Entries.Reset();
            return Fragment;
        }
        FGamePlatformPresentationCatalogEntry Entry;
        Entry.EntryId = FName(*(FString(TEXT("DBA.Audio.Library.")) + Audio.ClipId));
        Entry.SemanticTag = Semantic;
        Entry.ContextQuery.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
        Entry.ProviderChannel = TEXT("SFX");
        Entry.DefinitionId = FName(Audio.DefinitionId);
        Entry.Scope = EGamePlatformPresentationCatalogScope::Project;
        Entry.ContentRevision = TEXT("1");
        Fragment.Entries.Add(MoveTemp(Entry));
    }
    return Fragment;
}
