"""显式本地PIE运动证据夹具；只经Monolith运行，不保存地图/配置，不作联机验收。"""
import json
import os
import uuid
from pathlib import Path
import unreal as u

OUT = Path(u.Paths.project_dir()).resolve().parent / 'Saved/Validation/VillageWaterSystems20261031'
_probe = None


def spawn(world, cls, transform):
    library = u.get_default_object(u.GameplayStatics)
    actor = library.call_method('BeginDeferredActorSpawnFromClass', args=(
        world, cls, transform, u.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None))
    return library.call_method('FinishSpawningActor', args=(actor, transform))


def start(location=(800, 200, 150), exit_location=(6000, 0, 200)):
    """实际World循环推进：入水、下潜、平游、上浮、传送离水；从不强设Swimming。"""
    global _probe
    assert _probe is None, '已有在途探针，先结束'
    worlds = u.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds) == 1 and any(name in worlds[0].get_path_name() for name in ('L_GP_WaterReview.', 'L_Village_Start.'))
    world = worlds[0]
    pc = u.GameplayStatics.get_player_controller(world, 0)
    old_pawn = u.GameplayStatics.get_player_pawn(world, 0)
    pawn = spawn(world, u.load_class(None, '/Script/DivineBeastsAbilitiesRuntime.DivineBeastsGameplayCharacter'),
                 u.Transform(location=u.Vector(*location)))
    pc.possess(pawn)
    pawn.mesh.set_skeletal_mesh_asset(u.load_asset('/DBAContentPack_Common/Characters/KachujinGRosales/Kachujin_G_Rosales'))
    pawn.mesh.set_relative_location(u.Vector(0, 0, -90), False, False)
    pawn.mesh.set_relative_rotation(u.Rotator(pitch=0, yaw=-90, roll=0), False, False)
    pawn.mesh.set_anim_instance_class(u.load_class(None, '/DBAContentPack_Common/Characters/KachujinGRosales/Animations/ABP_DBA_Kachujin.ABP_DBA_Kachujin_C'))
    camera = spawn(world, u.CameraActor, u.Transform())
    pc.set_view_target_with_blend(camera, 0)
    camera.camera_component.set_field_of_view(75)
    state = {'world': world, 'pc': pc, 'old_pawn': old_pawn, 'pawn': pawn, 'camera': camera,
             'elapsed': 0., 'next_sample': 0., 'next_frame': 0., 'rows': [], 'exit': False, 'frames': [],
             'start_time': u.GameplayStatics.get_time_seconds(world), 'exit_location': exit_location,
             'output': OUT/('RuntimeProbe-'+uuid.uuid4().hex[:8])}
    _probe = state
    (state['output']/'Frames').mkdir(parents=True, exist_ok=True)
    state['handle'] = u.register_slate_pre_tick_callback(tick)
    print('WATER_RUNTIME_PROBE_STARTED', os.getpid())


def tick(delta):
    state = _probe
    if state is None:
        return
    try:
        if not u.EditorLevelLibrary.get_pie_worlds(False):
            finish('PIE ended')
            return
        state['elapsed'] = u.GameplayStatics.get_time_seconds(state['world']) - state['start_time']
        t = state['elapsed']
        pawn = state['pawn']
        movement = pawn.character_movement
        phase = 'entry'
        direction = u.Vector()
        if 2 <= t < 5:
            phase, direction = 'dive', u.Vector(0, 0, -1)
        elif 5 <= t < 8:
            phase, direction = 'forward', u.Vector(1, 0, 0)
        elif 8 <= t < 11:
            phase, direction = 'ascend', u.Vector(0, 0, 1)
        elif t >= 11:
            phase = 'exit'
            if not state['exit']:
                pawn.set_actor_location(u.Vector(*state['exit_location']), False, False)
                state['exit'] = True
        if movement.movement_mode == u.MovementMode.MOVE_SWIMMING:
            pawn.add_movement_input(direction, 1)
        location = pawn.get_actor_location()
        camera_location = location + u.Vector(-350, -300, 60)
        state['camera'].set_actor_location(camera_location, False, False)
        state['camera'].set_actor_rotation(u.MathLibrary.find_look_at_rotation(camera_location, location), False)
        if t >= state['next_sample']:
            state['next_sample'] = t + .1
            animation = pawn.mesh.get_anim_instance()
            state['rows'].append({'time': t, 'phase': phase, 'xyz': location.to_tuple(),
                                  'mode': str(movement.movement_mode),
                                  'physics': pawn.capsule_component.get_physics_volume().get_class().get_path_name(),
                                  'animSwimming': animation.get_editor_property('is_swimming'),
                                  'speed': pawn.get_velocity().to_tuple()})
        if t >= state['next_frame'] and t < 11:
            state['next_frame'] = t + .2
            filename = state['output']/'Frames'/f'Frame{len(state["frames"]):04d}.png'
            u.SystemLibrary.execute_console_command(state['world'], f'HighResShot 960x540 filename="{filename.as_posix()}"')
            state['frames'].append(str(filename))
        if t >= 13:
            finish('complete')
    except Exception as error:
        finish(str(error))
        raise


def finish(reason='cancelled'):
    global _probe
    state, _probe = _probe, None
    if not state:
        return
    u.unregister_slate_pre_tick_callback(state['handle'])
    rows = state['rows']
    swimming = [r for r in rows if 'MOVE_SWIMMING' in r['mode']]
    phase_z = {phase: [r['xyz'][2] for r in rows if r['phase'] == phase]
               for phase in ('dive', 'forward', 'ascend', 'exit')}
    checks = {
        'enteredByNativeBroadphase': bool(swimming) and all('GamePlatformWaterPhysicsVolume' in r['physics'] for r in swimming),
        'diveMovesDown': len(phase_z['dive']) > 1 and phase_z['dive'][-1] < phase_z['dive'][0]-200,
        'ascendMovesUp': len(phase_z['ascend']) > 1 and phase_z['ascend'][-1] > phase_z['ascend'][0]+200,
        'exitClearsSwimming': any('MOVE_SWIMMING' not in r['mode'] and not r['animSwimming'] for r in rows if r['phase'] == 'exit'),
        'animationReflectsSwimming': bool(swimming) and all(r['animSwimming'] for r in swimming[1:]),
    }
    report = {'pid': os.getpid(), 'scope': 'Standalone PIE fixture, actual Chaos/native movement, no forced movement mode',
              'reason': reason, 'checks': checks, 'passed': reason == 'complete' and all(checks.values()),
              'MaxFPS': u.SystemLibrary.get_console_variable_float_value('t.MaxFPS'),
              'VSync': u.SystemLibrary.get_console_variable_int_value('r.VSync'), 'rows': rows, 'framesRequested': state['frames'],
              'networkAdmissionVerified': False, 'humanAccepted': False}
    report['map'] = state['world'].get_path_name()
    (state['output']/'RuntimeSwimming.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    state['pc'].possess(state['old_pawn'])
    state['camera'].destroy_actor()
    state['pawn'].destroy_actor()
    print('WATER_RUNTIME_PROBE_FINISHED', json.dumps(checks), reason)
