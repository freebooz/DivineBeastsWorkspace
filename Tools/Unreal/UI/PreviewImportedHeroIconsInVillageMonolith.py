"""在真实新手村Standalone PIE中显示原生战斗HUD和只读图标审查层。

由Monolith执行；必须先获得明确只读预览授权。open从当前无外服连接的PIE
旅行至新手村并显式使用引擎GameModeBase；show只实例化已保存Widget Blueprint。
不创建服务器授权、账号、技能按钮或后台请求，不修改地图。capture使用原生
Shot SHOWUI截图，禁止Monolith widget截图Gamma路径。cleanup移除本工具UI与相机。
"""
from pathlib import Path
import json
import unreal

ROOT=Path(__file__).resolve().parents[3]
# 后续英雄批次可指定自己的证据目录；只允许写本工作空间内，不覆盖旧批次记录。
OUT=Path(globals().get('PREVIEW_OUTPUT',ROOT/'Saved/Validation/HeroSkillIconWorkflow20261019')).resolve()
MODE=globals().get('PREVIEW_MODE','status')
HERO=globals().get('PREVIEW_HERO','Ox')

def require(condition,message):
    if not condition:raise RuntimeError(message)

def world():
    """只允许Standalone；没有将连接的Village/MainArena实例，不进行联机准入。"""
    value=unreal.EditorLevelLibrary.get_game_world()
    require(value is not None and unreal.SystemLibrary.is_standalone(value),'只读预览必须为Standalone，拒绝外服连接')
    return value

def state():
    """暂态审查对象仅保留在本编辑器Python模块，停止PIE后不跨世界恢复。"""
    if not hasattr(unreal,'_sslm_hero_icon_preview'):
        unreal._sslm_hero_icon_preview={}
    return unreal._sslm_hero_icon_preview

def create_widget(value,path,controller):
    """通过真实UMG反射工厂构造用户控件，不伪造WidgetTree或Slate图像。"""
    cls=unreal.load_class(None,path+'.'+path.rsplit('/',1)[-1]+'_C')
    require(cls is not None,'预览或正式HUD类不可加载：'+path)
    widget=unreal.WidgetLibrary.get_default_object().call_method('Create',args=(value,cls,controller))
    require(widget is not None,'UMG工厂未创建控件')
    widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    return widget

def main():
    """开场、显示、截图和清理分开调用，布局与截图均由真实引擎帧完成。"""
    require(Path(unreal.Paths.get_project_file_path()).resolve()==ROOT/'Game/DivineBeastsArena.uproject','不是正式工程')
    require(OUT.is_relative_to(ROOT) and OUT.is_dir(),'证据目录必须是已存在的工作空间目录')
    if MODE=='cleanup':
        data=state()
        for key in ['review','hud']:
            widget=data.get(key)
            if unreal.SystemLibrary.is_valid(widget):widget.remove_from_parent()
        for widget,visibility in data.get('hiddenRoots',[]):
            if unreal.SystemLibrary.is_valid(widget):widget.set_visibility(visibility)
        camera=data.get('camera')
        if unreal.SystemLibrary.is_valid(camera):camera.destroy_actor()
        unreal._sslm_hero_icon_preview={}
        print('SSLM_ICON_REVIEW_CLEANED')
        return
    value=world()
    if MODE=='open':
        settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
        require(not settings.get_editor_property('bLaunchSeparateServer') and settings.get_editor_property('PlayNumberOfClients')==1,'不能创建额外服务器或客户端')
        (OUT/'OfflinePreviewGate.json').write_text(json.dumps({'standalone':True,'externalVillageTarget':None,'externalArenaTarget':None,'onlineAdmissionVerified':False,'reason':'用户明确选择只读美术预览；不连接战斗服务器'},ensure_ascii=False,indent=2),encoding='utf-8')
        unreal.GameplayStatics.open_level(value,unreal.Name('/DBAWorldPack_Village/Maps/L_Village_Start'),True,'game=/Script/Engine.GameModeBase')
        print('SSLM_ICON_REVIEW_VILLAGE_TRAVEL_REQUESTED')
        return
    require('L_Village_Start' in value.get_name(),'当前不是新手村真实PIE世界')
    mode=unreal.GameplayStatics.get_game_mode(value)
    require(mode and mode.get_class().get_name()=='GameModeBase','预览不能使用SSLM权威技能GameMode')
    controller=unreal.GameplayStatics.get_player_controller(value,0)
    require(controller is not None,'缺少本地预览控制器')
    data=state()
    if MODE=='show':
        require(HERO in ['Rat','Tiger','Snake','Ox','Dragon'],'未核验英雄')
        # Standalone仍会生成前端根布局；只在本次只读审查中暂时隐藏，结束恢复，不提交登录。
        if 'hiddenRoots' not in data:
            roots=unreal.WidgetLibrary.get_all_widgets_of_class(value,unreal.UserWidget,True)
            data['hiddenRoots']=[(widget,widget.get_visibility()) for widget in roots if widget.is_in_viewport()]
            for widget,visibility in data['hiddenRoots']:widget.set_visibility(unreal.SlateVisibility.COLLAPSED)
        if data.get('review'):data['review'].remove_from_parent()
        if not data.get('hud'):
            hud=create_widget(value,'/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_CombatHUD',controller)
            hud.add_to_viewport(9000)
            data['hud']=hud
        panel='/DBAHeroPack_'+HERO+'/UI/Review/WBP_DBA_'+HERO+'_SkillIconReviewV6'
        review=create_widget(value,panel,controller)
        review.add_to_viewport(10000)
        review.set_desired_size_in_viewport(unreal.Vector2D(1064,274))
        review.set_position_in_viewport(unreal.Vector2D(24,24),False)
        data['review']=review
        data['hero']=HERO
        # 用户要求图标进入正式技能栏：调用技能栏自己的唯一VM，不直接改Image或制造Grant。
        bar=data['hud'].get_editor_property('AbilityBar')
        require(bar.preview_hero_icons(unreal.Name('Hero.Zodiac.'+HERO)),'真实技能栏未受理英雄图标预览')
        if not data.get('camera'):
            starts=unreal.GameplayStatics.get_all_actors_of_class(value,unreal.PlayerStart)
            location=starts[0].get_actor_location() if starts else unreal.Vector(0,0,1000)
            location=location+unreal.Vector(-600,-600,500)
            pose=unreal.Transform(location=location,rotation=unreal.Rotator(pitch=-25,yaw=45,roll=0))
            # BlueprintInternalUseOnly工厂没有Python胶水，按已核对的原生UFunction签名调用。
            factory=unreal.GameplayStatics.get_default_object()
            scale_mode=unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT
            camera=factory.call_method('BeginDeferredActorSpawnFromClass',args=(value,unreal.CameraActor,pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,scale_mode))
            require(camera is not None,'只读预览相机创建失败')
            camera=factory.call_method('FinishSpawningActor',args=(camera,pose,scale_mode))
            controller.set_view_target_with_blend(camera,0.0)
            data['camera']=camera
        print('SSLM_ICON_REVIEW_VISIBLE '+HERO)
    elif MODE=='capture':
        review=data.get('review')
        require(review is not None and data.get('hero')==HERO,'尚未显示指定英雄')
        metrics=[]
        for size in [64,48]:
            for index in range(1,9):
                icon=review.get_editor_property('Icon%d_%d'%(size,index))
                geometry=unreal.SlateLibrary.get_local_size(icon.get_cached_geometry())
                require(abs(geometry.x-size)<0.01 and abs(geometry.y-size)<0.01,'控件实际布局尺寸错误')
                tint=icon.get_editor_property('color_and_opacity')
                require(all(abs(x-1)<0.001 for x in [tint.r,tint.g,tint.b,tint.a]),'原图存在额外乘色')
                texture=unreal.WidgetLibrary.get_brush_resource_as_texture2d(icon.get_editor_property('brush'))
                require(texture is not None,'原生Image画刷没有真实纹理')
                metrics.append({'name':icon.get_name(),'size':[geometry.x,geometry.y],'texture':texture.get_path_name(),'whiteTint':True})
        filename=OUT/(HERO+'_VillageHUD_ReadOnly.png')
        record={'hero':HERO,'world':value.get_path_name(),'gameMode':'GameModeBase','standalone':True,
            'hudClass':data['hud'].get_class().get_path_name(),'reviewClass':review.get_class().get_path_name(),
            'dpiScale':unreal.WidgetLayoutLibrary.get_viewport_scale(value),'icons':metrics,
            'abilityBarSlots':[{'slotId':str(slot.slot_id),'abilityId':str(slot.content_id),'enabled':slot.enabled if hasattr(slot,'enabled') else slot.get_editor_property('bEnabled')} for slot in data['hud'].get_editor_property('AbilityBar').get_ability_slots()],
            'screenshotRequested':str(filename),'screenshotWrittenVerified':False,'onlineAdmissionVerified':False}
        (OUT/(HERO+'_HUDPreview.json')).write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
        unreal.SystemLibrary.execute_console_command(value,'Shot '+str(filename).replace('\\','/')+' SHOWUI',controller)
        print('SSLM_ICON_REVIEW_SCREENSHOT_REQUESTED '+HERO)
    else:
        print(json.dumps({'world':value.get_path_name(),'standalone':True,'hero':data.get('hero')},ensure_ascii=False))

main()
