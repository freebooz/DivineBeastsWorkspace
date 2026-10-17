#requires -Version 7.0
<#
.SYNOPSIS
只读检查锁定引擎导出的客户端IoStore清单中是否包含前端运行必需的软引用资产。
.DESCRIPTION
输入为UnrealPak -ListContainer -Csv生成的真实CSV；逐项要求WindowsClient的非空ExportBundleData。
覆盖RootLayout、登录/角色页面、动态卡片、应用流程和十二生肖外观入口；不以源码文件存在代替包内存在。
退出0表示包内容门禁通过，1表示缺失或清单无效；不代表登录、材质效果或新手村联机通过。
#>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ContainerCsv, [string]$ReportPath, [switch]$DevelopmentSkills,[switch]$DevelopmentArena)
$ErrorActionPreference = 'Stop'
try {
    # 引擎CSV头和值带空格；按实际列名裁剪后读取，避免把有效清单误判为空。
    $rows = @(Import-Csv -LiteralPath $ContainerCsv | ForEach-Object {
        $row = @{}
        foreach ($property in $_.PSObject.Properties) { $row[$property.Name.Trim()] = ([string]$property.Value).Trim() }
        $row
    })
    if ($rows.Count -eq 0 -or !$rows[0].ContainsKey('Filename') -or !$rows[0].ContainsKey('ChunkType')) {
        throw '清单为空或不是引擎IoStore格式。'
    }
    $required = @(
        'DBAUIPack_Core/Content/UI/Root/WBP_DBA_UI_RootLayout.uasset',
        'DBAUIPack_Core/Content/UI/Screens/WBP_DBA_UI_Login.uasset',
        'DBAUIPack_Core/Content/UI/Screens/WBP_DBA_UI_CharacterCreate.uasset',
        'DBAUIPack_Core/Content/UI/Screens/WBP_DBA_UI_CharacterSelect.uasset',
        # ESC系统层真实菜单与设备设置必须Cook，避免软目录解析成功而实际页面缺失。
        'DBAUIPack_Core/Content/UI/Screens/WBP_DBA_UI_SystemMenu.uasset',
        'DBAUIPack_Core/Content/UI/Screens/WBP_DBA_UI_Settings.uasset',
        # 加载与恢复是真实流程必要页面，缺少时连接失败会变成不可操作的黑屏。
        'DBAUIPack_Core/Content/UI/Screens/WBP_DBA_UI_LoadingTravel.uasset',
        'DBAUIPack_Core/Content/UI/Screens/WBP_DBA_UI_ErrorReconnect.uasset',
        'DBAUIPack_Core/Content/UI/Components/WBP_DBA_HeroChoice.uasset',
        # 生肖图像按钮的正式图集是必要资源，缺失时不允许把文字回退冒充图像交付。
        'DBAUIPack_Core/Content/UI/Textures/Zodiac/T_DBA_ZodiacChoiceAtlas_V1.uasset',
        'DBAUIPack_Core/Content/UI/Components/WBP_DBA_CharacterChoice.uasset',
        'DBAClient/Content/Definitions/DA_DivineBeastsApplicationFlow.uasset',
        'DBAUIPack_Core/Content/UI/Textures/DBA_MythicLogin.uasset',
        'DBAUIPack_Core/Content/UI/Textures/DBA_MythicLogo.uasset',
        # 定稿自然主题的真实贴图闭包；界面文字仍由控件渲染，旧资产仅保留兼容身份。
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureMenuButton.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NaturePanelFrame.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureLoginBackdrop.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureHUDFrame.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureAbilityDock.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureMinimapFrame.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureMenuReference.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureAbilityReference.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureMinimapReference.uasset',
        # 登录原稿面板、取样材质与透明点击样式必须进入实际发布包；哈希一致不替代此检查。
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureLoginPanelReference.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NatureLoginPanelReference.uasset',
        'DBAUIPack_Core/Content/UI/Styles/BP_DBA_ButtonStyle_LoginOverlay.uasset',
        # 原稿头像框与纯内容变体；两个肖像实例都由同一本地角色事件驱动，属性来自真实ASC。
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NaturePlayerFrameReference.uasset',
        # 独立轮廓仅控制外部Alpha，保持原稿石材/藤蔓暗部不透明。
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureAbilityReferenceMatte.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureMinimapReferenceMatte.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NaturePlayerFrameReferenceMatte.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NatureMenuReferenceMatte.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NatureAbilityIconPalette.uasset',
        # 统一面板及原生返回动作的实际运行闭包；样式跨页面共用，不以规划任务/背包空页冒充交付。
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NaturePanelReference.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Nature/T_DBA_NaturePanelReferenceMatte.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NaturePanelNineSlice.uasset',
        'DBAUIPack_Core/Content/UI/Styles/BP_DBA_BorderStyle_Panel.uasset',
        'DBAUIPack_Core/Content/UI/Components/WBP_DBA_UI_StandardPanel.uasset',
        'GamePlatformUI/Content/Input/DT_GP_UIActions.uasset',
        'GamePlatformUI/Content/Input/BP_GP_UIInputData.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NaturePlayerFrameReference.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NaturePlayerFrameHealthFillReference.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NaturePlayerFrameMomentumFillReference.uasset',
        'DBAUIPack_Core/Content/UI/Components/WBP_DBA_UI_PlayerFramePortrait.uasset',
        'DBAUIPack_Core/Content/UI/Components/WBP_DBA_UI_PlayerFrameStatus.uasset',
        'DBAUIPack_Core/Content/UI/Components/WBP_DBA_UI_PlayerFrameHealthBar.uasset',
        'DBAUIPack_Core/Content/UI/Components/WBP_DBA_UI_PlayerFrameMomentumBar.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NatureMenuReference.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NatureAbilityReference.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NatureMinimapReference.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NaturePanelReference.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NatureSkillFrameReference.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NatureHealthFillReference.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NatureMomentumFillReference.uasset',
        'DBAUIPack_Core/Content/UI/Materials/Nature/M_DBA_NatureUltimateFrameReference.uasset',
        'DBAUIPack_Core/Content/UI/Styles/BP_DBA_TextStyle_Menu.uasset',
        'DBAUIPack_Core/Content/UI/Components/WBP_DBA_UI_MomentumBar.uasset',
        'DBAContentPack_Common/Content/Mannequins/DBA/Animations/AS_DBA_PreviewIdle.uasset',
        'DBAContentPack_Common/Content/Mannequins/DBA/Animations/ABP_DBA_PreviewIdle.uasset',
        # 世界移动是独立速度驱动ABP，缺任一依赖会退化为静止或空模型；预览Idle不作为行走替代。
        'DBAContentPack_Common/Content/Mannequins/DBA/Animations/ABP_DBA_WorldLocomotion.uasset',
        'DBAContentPack_Common/Content/Mannequins/DBA/Animations/BS_DBA_WorldLocomotion.uasset',
        'DBAContentPack_Common/Content/Mannequins/DBA/Animations/WorldLocomotion/MM_Idle.uasset',
        'DBAContentPack_Common/Content/Mannequins/DBA/Animations/WorldLocomotion/MF_Unarmed_Walk_Fwd.uasset',
        'DBAContentPack_Common/Content/Mannequins/DBA/Animations/WorldLocomotion/MF_Unarmed_Jog_Fwd.uasset',
        'DBAContentPack_Common/Content/Mannequins/DBA/Animations/WorldLocomotion/MM_Fall_Loop.uasset',
        'DBAContentPack_Common/Content/Mannequins/DBA/Animations/WorldLocomotion/MM_Jump.uasset',
        'DBAContentPack_Common/Content/Mannequins/DBA/Animations/WorldLocomotion/MM_Land.uasset',
        'DBAUIPack_Core/Content/UI/Maps/T_DBA_Village_Minimap.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Primal/T_DBA_PrimalLoginBackdrop.uasset',
        'DBAUIPack_Core/Content/UI/Textures/Primal/T_DBA_ShangBronzePanel.uasset',
        # 真实蒙皮必须连同完整UE5骨架交付；旧68骨架同名兼容身份不能代替依赖。
        'DBAContentPack_Common/Content/Mannequins/UE5/Meshes/SK_Mannequin.uasset',
        'DBAContentPack_Common/Content/Mannequins/DBA/Meshes/SKM_Manny_Simple.uasset',
        # 十二生肖已替换Manny/Quinn占位模型；要求真实共用模型和动画蓝图进入容器。
        'DBAContentPack_Common/Content/Characters/KachujinGRosales/Kachujin_G_Rosales.uasset',
        'DBAContentPack_Common/Content/Characters/KachujinGRosales/Animations/ABP_DBA_Kachujin.uasset',
        'DBAContentPack_Common/Content/Characters/KachujinGRosales/Animations/BS_DBA_Kachujin_Locomotion.uasset',
        # 网络世界定义必须进入实际IoStore，不以编辑器磁盘文件代替交付。
        'DBAWorldPack_Village/Content/Definitions/DA_DBA_Pawn_WorldCharacter.uasset',
        'DBAWorldPack_Village/Content/Definitions/DA_DBA_Experience_Village_Tutorial.uasset'
    )
    # 角色选择和创建的四种自然飘落资源通过软Definition及Niagara引用装载，
    # 不能只验证Editor本地文件。容器必须交付编译后Niagara系统、项目外观与工作室地面锚点。
    # UE5.8 NiagaraScript.cpp::IsEditorOnly将独立模块脚本判为编辑器资产；
    # 空中/地面模块编入系统执行数据，不能要求NM_GP_*源脚本单独进入运行容器。
    $required += @(
        'GamePlatformVFX/Content/Environment/FallingFoliage/Materials/M_GP_FallingFoliage_Master.uasset',
        'GamePlatformVFX/Content/Environment/FallingFoliage/EffectTypes/NET_GP_FallingFoliage_Ambient.uasset',
        'GamePlatformVFX/Content/Environment/FallingFoliage/Niagara/NS_GP_FallingFoliage_Base.uasset',
        'DBAFrontEndPack/Content/CharacterPreview/Environment/Blueprints/BP_DBA_FoliageGroundAnchor.uasset',
        'DBAFrontEndPack/Content/Maps/L_DBA_CharacterStudio.umap'
    )
    $foliageContentRoot='DBAPresentationPack_Core/Content/VFX/Environment/FallingFoliage'
    foreach ($style in @('Peach','Maple','Bamboo','Ginkgo')) {
        $required += "$foliageContentRoot/Definitions/DA_DBA_VFX_FallingFoliage_$style.uasset"
        $required += "$foliageContentRoot/Niagara/NS_DBA_FallingFoliage_$style.uasset"
        $required += "$foliageContentRoot/Materials/MI_DBA_Foliage_$style.uasset"
        $required += "$foliageContentRoot/Textures/T_DBA_Foliage_${style}_Albedo.uasset"
        $required += "$foliageContentRoot/Textures/T_DBA_Foliage_${style}_Normal.uasset"
    }
    foreach ($hero in @('Rat','Ox','Tiger','Rabbit','Dragon','Snake','Horse','Goat','Monkey','Rooster','Dog','Boar')) {
        $required += "DBAHeroPack_$hero/Content/Characters/DA_Appearance_Zodiac_$hero.uasset"
        $required += "DBAGameplay/Content/Definitions/DA_Hero_Zodiac_$hero.uasset"
        # 前端创建介绍是独立只读显示资产；不等于引入开发Gameplay、批准或授予候选技能。
        $required += "DBAUIPack_Core/Content/UI/Profiles/CharacterCreation/DA_DBA_CharacterCreation_$hero.uasset"
        $required += "DBAHeroPack_$hero/Content/UI/Portraits/T_DBA_${hero}_Portrait.uasset"
        if ($DevelopmentSkills) {
            # 开发候选只核对用户批准的真实资产闭包；不将数据交付等同于全部机制实现。
            $root = 'DivineBeastsArena/Content/Development/DivineBeasts/Abilities'
            $required += "$root/DA_DBA_${hero}_DevAbilitySet.uasset"
            $required += "$root/UI/Profiles/DA_DBA_${hero}_DevUIProfile.uasset"
            foreach ($slot in @('BasicAttack','Active01','Active02','Passive','Ultimate')) {
                $tail = if ($hero -eq 'Rat' -and $slot -eq 'BasicAttack') { 'Primary' } else { $slot }
                $native = if ($hero -eq 'Rat' -and $slot -eq 'BasicAttack') { 'Native' } else { '' }
                $required += "$root/DA_DBA_${hero}_Dev$tail.uasset"
                $required += "$root/GA_DBA_${hero}_Dev$tail$native.uasset"
                if ($slot -in @('Active01','Active02','Ultimate')) {
                    $required += "$root/GE_DBA_${hero}_Dev${slot}Cost.uasset"
                    $required += "$root/GE_DBA_${hero}_Dev${slot}Cooldown.uasset"
                }
            }
        }
    }
    if ($DevelopmentSkills) {
        # 村庄开发包同时含三名NPC与混合怪群；表现不进入专服，但必须在客户端容器。
        $required += 'DBAContentPack_Common/Content/Characters/NPC/Arissa/Arissa.uasset'
        $required += 'DBAContentPack_Common/Content/Characters/NPC/Arissa/Animations/ABP_DBA_Arissa.uasset'
        $required += 'DBAWorldPack_Village/Content/Creatures/VampireALusth/Vampire_A_Lusth.uasset'
        $required += 'DBAWorldPack_Village/Content/Creatures/VampireALusth/Animations/ABP_DBA_Vampire.uasset'
        $required += 'DBAWorldPack_Village/Content/Creatures/VampireALusth/Animations/AM_DBA_Vampire_Attack.uasset'
        # 魔法定义/GA/成本/冷却与三图标、六VFX都必须在实际容器，不能以本机Editor存在放行。
        $required += 'DivineBeastsArena/Content/Development/DivineBeasts/Abilities/DT_DBA_Zodiac_DevBalance.uasset'
        $magic='DivineBeastsArena/Content/Development/DivineBeasts/Abilities/MagicTests'
        $required += "$magic/DT_DBA_DevMagicBalance.uasset"
        foreach($hero in @('Rat','Ox','Tiger','Rabbit','Dragon','Snake','Horse','Goat','Monkey','Rooster','Dog','Boar')) {
            foreach($spell in @('Fireball','Frostbolt','ArcaneMissiles')) {
                foreach($prefix in @('DA','GA')) { $required += "$magic/${prefix}_DBA_${hero}_Dev$spell.uasset" }
                foreach($suffix in @('Cost','Cooldown')) { $required += "$magic/GE_DBA_${hero}_Dev${spell}$suffix.uasset" }
            }
        }
        foreach($spell in @('Fireball','Frostbolt','ArcaneMissiles')) {
            $required += "DBAUIPack_Core/Content/UI/Abilities/MagicTests/T_DBA_Dev$spell.uasset"
            foreach($stage in @('Flight','Impact')) {
                $required += "DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Niagara/NS_DBA_Dev${spell}$stage.uasset"
                $required += "DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Definitions/DA_DBA_VFX_Dev${spell}$stage.uasset"
            }
        }
    }
    if($DevelopmentArena){
        # 同一实际容器必须含竞技地图、三份定义、全部组队/竞技页面及公用行/玉石材质，缺一即拒绝发布。
        $required+='DBAWorldPack_MainArena/Content/Maps/L_DBA_MainArena.umap'
        foreach($name in @('World_MainArena','Experience_MainArena','Pawn_ArenaCharacter')){$required+="DBAWorldPack_MainArena/Content/Definitions/DA_DBA_$name.uasset"}
        foreach($name in @('PartyPanel','PartyInviteModal','PartyReadyCheckModal','PartyApplications','PartyListEntry')){$required+="DBAUIPack_Core/Content/UI/Social/WBP_DBA_$name.uasset"}
        foreach($name in @('Matchmaking','MatchFoundReady','HeroSelection','Scoreboard','PostMatch')){$required+="DBAArena/Content/UI/Screens/WBP_DBA_UI_Arena_$name.uasset"}
        $required+='DBAArena/Content/UI/HUD/WBP_DBA_UI_ArenaHUD.uasset'
        $required+='DBAUIPack_Core/Content/UI/Materials/Social/M_DBA_PartyJade_V1.uasset'
    }
    # 软路径在C++目录中构造，地图引用扫描不能发现；要求实际导出数据而非配置或仅包名。
    $missing = @($required | Where-Object {
        $suffix = '/' + $_
        @($rows | Where-Object { $_.Filename.Replace('\','/').EndsWith($suffix, [StringComparison]::OrdinalIgnoreCase) -and
            $_.ChunkType -eq 'ExportBundleData' -and $_.Platform -eq 'WindowsClient' -and [long]$_.Size -gt 0 }).Count -eq 0
    })
    $report = [ordered]@{ Passed=($missing.Count -eq 0); RequiredCount=$required.Count; Missing=$missing; ContainerCsv=[IO.Path]::GetFullPath($ContainerCsv) }
    $json = $report | ConvertTo-Json -Depth 5
    if ($ReportPath) { $json | Set-Content -LiteralPath $ReportPath -Encoding utf8 }
    $json
    if ($missing.Count -gt 0) { exit 1 }
    exit 0
} catch { Write-Error $_; exit 1 }
