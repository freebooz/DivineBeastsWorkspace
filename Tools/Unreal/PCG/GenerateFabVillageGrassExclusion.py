"""从正式地图只读快照生成花草排除遮罩；不得用历史地图采样。"""
import argparse
import hashlib
import json
import math
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]
MAP = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap'
SOURCE = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/FabGrass'
SIZE = 2048


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('survey', type=Path)
    args = parser.parse_args()
    data = json.loads(args.survey.read_text(encoding='utf-8'))
    assert args.survey.resolve().is_relative_to((ROOT / 'Saved/Validation/FabVillageGrass').resolve())
    assert hashlib.sha256(MAP.read_bytes()).hexdigest() == data['mapSha256'], '地图已变更，必须重新采样'
    ox, oy, width, height = data['landscapeBounds']
    assert width > 0 and math.isclose(width, height) and len(data['spatialMasks']) > 0
    out = SOURCE / 'VillageGrassExclusion.png'
    meta = SOURCE / 'VillageGrassExclusion.json'
    assert not out.exists() and not meta.exists(), '目标源文件已存在，拒绝覆盖'
    image = Image.new('L', (SIZE, SIZE), 0)
    draw = ImageDraw.Draw(image)
    scale = SIZE / width
    def point(p): return (round((p[0] - ox)*scale), round((p[1] - oy)*scale))
    def stroke(points, radius):
        px = max(1, math.ceil(radius*scale))
        pts = [point(p) for p in points]
        if len(pts) > 1: draw.line(pts, fill=255, width=2*px+1, joint='curve')
        for x, y in pts: draw.ellipse((x-px, y-px, x+px, y+px), fill=255)
    # 水采用实时样条的真实湖面及变宽河道，避免使用较大的Water.Lake矩形近似。
    draw.polygon([point(p) for p in data['lakePolygon']], fill=255)
    stroke(data['lakePolygon'] + data['lakePolygon'][:1], 150.)
    river = data['riverPoints']
    for a, b in zip(river, river[1:]): stroke([a, b], max(a[3], b[3])/2 + 150.)
    included = []
    for mask in data['spatialMasks']:
        if mask['domain'] in ('Water.Lake', 'Water.River'): continue
        vertices = mask['vertices']
        assert len(vertices) >= 2
        if mask['closed'] and mask['fill']:
            assert len(vertices) >= 3
            draw.polygon([point(p) for p in vertices], fill=255)
        if mask['halfWidth'] > 0:
            stroke(vertices + (vertices[:1] if mask['closed'] else []), mask['halfWidth'])
        included.append(mask['domain'])
    SOURCE.mkdir(parents=True, exist_ok=True)
    image.save(out)
    record = {'mapSha256': data['mapSha256'], 'bounds': data['landscapeBounds'],
              'size': SIZE, 'survey': str(args.survey.resolve()), 'domains': included,
              'pngSha256': hashlib.sha256(out.read_bytes()).hexdigest(), 'whiteMeansExcluded': True}
    meta.write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
    print('FAB_GRASS_EXCLUSION_GENERATED', out, 'sha256', record['pngSha256'])


if __name__ == '__main__': main()
