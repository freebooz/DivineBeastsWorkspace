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
param([Parameter(Mandatory)][string]$ContainerCsv, [string]$ReportPath, [switch]$DevelopmentSkills)
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
        'DBAContentPack_Common/Content/Mannequins/DBA/Meshes/SKM_Quinn_Simple.uasset',
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
    if ($DevelopmentSkills) { $required += 'DivineBeastsArena/Content/Development/DivineBeasts/Abilities/DT_DBA_Zodiac_DevBalance.uasset' }
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
