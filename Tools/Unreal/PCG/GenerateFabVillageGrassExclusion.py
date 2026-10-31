"""从正式地图只读快照生成花草排除遮罩；不得用历史地图采样。"""
import argparse
import hashlib
import json
import math
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[3]
MAP = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap'
SOURCE = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/FabGrass'
SIZE = 2048


def render_mask(data, size=SIZE):
    """出生区域不屏蔽无碰撞装饰草；其余空间限制仍独立生效。"""
    ox, oy, width, height = data['landscapeBounds']
    assert width > 0 and math.isclose(width, height) and len(data['spatialMasks']) > 0
    image = Image.new('L', (size, size), 0)
    draw = ImageDraw.Draw(image)
    scale = size / width
    def point(p): return (round((p[0] - ox)*scale), round((p[1] - oy)*scale))
    def stroke(points, radius):
        px = max(1, math.ceil(radius*scale))
        pts = [point(p) for p in points]
        if len(pts) > 1: draw.line(pts, fill=255, width=2*px+1, joint='curve')
        for x, y in pts: draw.ellipse((x-px, y-px, x+px, y+px), fill=255)
    # 湖的平面轮廓可能投影到远高于水面的山地；仅排除水面附近及以下的地形。
    # 采样格扩一格并在缺失范围内保守排除，避免在岸线或水中漏出花草。
    heights = data['heights']
    nx, ny = data['nx'], data['ny']
    assert len(heights) == ny and all(len(row) == nx for row in heights)
    assert data['step'] > 0
    wet = Image.new('L', (nx, ny))
    wet.putdata([255 if z <= data['waterZ'] + 150. else 0 for row in heights for z in row])
    wet = wet.filter(ImageFilter.MaxFilter(3))
    sx, sy = data['origin']
    wet = wet.transform((size, size), Image.Transform.AFFINE,
                        (width / size / data['step'], 0, (ox - sx) / data['step'],
                         0, height / size / data['step'], (oy - sy) / data['step']),
                        resample=Image.Resampling.NEAREST, fillcolor=255)
    draw.polygon([point(p) for p in data['lakePolygon']], fill=255)
    stroke(data['lakePolygon'] + data['lakePolygon'][:1], 150.)
    image = ImageChops.multiply(image, wet)
    draw = ImageDraw.Draw(image)
    # 河道仍使用变宽样条，保留原有排除范围。
    river = data['riverPoints']
    for a, b in zip(river, river[1:]): stroke([a, b], max(a[3], b[3])/2 + 150.)
    included = []
    for mask in data['spatialMasks']:
        if mask['domain'] in ('Water.Lake', 'Water.River', 'Gameplay.Spawn'): continue
        vertices = mask['vertices']
        assert len(vertices) >= 2
        if mask['closed'] and mask['fill']:
            assert len(vertices) >= 3
            draw.polygon([point(p) for p in vertices], fill=255)
        if mask['halfWidth'] > 0:
            stroke(vertices + (vertices[:1] if mask['closed'] else []), mask['halfWidth'])
        included.append(mask['domain'])
    return image, included


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('survey', type=Path)
    parser.add_argument('--output-dir', type=Path, default=SOURCE)
    args = parser.parse_args()
    data = json.loads(args.survey.read_text(encoding='utf-8'))
    assert args.survey.resolve().is_relative_to((ROOT / 'Saved/Validation/FabVillageGrass').resolve())
    assert hashlib.sha256(MAP.read_bytes()).hexdigest() == data['mapSha256'], '地图已变更，必须重新采样'
    destination = args.output_dir.resolve()
    assert destination == SOURCE.resolve() or destination.is_relative_to((ROOT / 'Saved/Validation/FabVillageGrass').resolve()), '候选输出必须位于工程内'
    out = destination / 'VillageGrassExclusion.png'
    meta = destination / 'VillageGrassExclusion.json'
    assert not out.exists() and not meta.exists(), '目标源文件已存在，拒绝覆盖'
    image, included = render_mask(data)
    destination.mkdir(parents=True, exist_ok=True)
    image.save(out)
    record = {'mapSha256': data['mapSha256'], 'bounds': data['landscapeBounds'],
              'size': SIZE, 'survey': str(args.survey.resolve()), 'domains': included,
              'pngSha256': hashlib.sha256(out.read_bytes()).hexdigest(), 'whiteMeansExcluded': True}
    meta.write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
    print('FAB_GRASS_EXCLUSION_GENERATED', out, 'sha256', record['pngSha256'])


if __name__ == '__main__': main()
