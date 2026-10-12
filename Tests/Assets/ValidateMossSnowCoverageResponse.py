#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""平台苔藓与雪覆盖遮罩回归，用于验证数值边界；不代替真实UE渲染。"""
import math

def clamp(x):
    return max(0.0,min(1.0,x))

def moss_mask(macro,microheight,slope,influence=1.0,local=.82):
    """宏观团簇与微高度、苔藓影响、坡面共同计算。"""
    return clamp(clamp(macro)*clamp(microheight)*(.4+.6*clamp(slope))*
                 clamp(influence)*clamp(local))

def snow_mask(macro,slope,snow,slope_power=2.6,edge=.22):
    """积雪的连续软阈值和顶点坡度，雪量零时严格无雪。"""
    snow=clamp(snow)
    threshold=.5-.4*snow
    return clamp(snow*clamp((macro-threshold)/edge)*
                 math.pow(clamp(slope),slope_power))

def check():
    """分别检查晴雪端点、坡面与苔藓恢复，避免无意改变天气协议。"""
    for coverage in (.1,.3,.5,.8,1.):
        for slope in (0.,.25,.5,1.):
            assert snow_mask(coverage,slope,0.)==0.
            values=[snow_mask(coverage,slope,x) for x in (0.,.23,.5,.85,1.)]
            assert all(a<=b+1e-6 for a,b in zip(values,values[1:]))
            assert moss_mask(coverage,.7,slope,influence=0.)==0.

    assert snow_mask(.8,1.,.85)>snow_mask(.8,.5,.85)
    assert snow_mask(.8,0.,1.)==0.
    assert moss_mask(.8,.9,1.)>moss_mask(.8,.9,0.)
    assert moss_mask(.8,.9,1.)>0.
    print("MOSS_SNOW_COVERAGE_RESPONSE_PASS")

if __name__=="__main__":
    check()
