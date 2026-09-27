#!/usr/bin/env python3
"""Audit actual filled bottom copper and render the KiCad placement study.

Run with KiCad's bundled Python after native DRC has refilled/saved the candidate.
The reference-plane check samples centre lines every 0.025 mm against the
bottom GND polygon eroded by trace half-width + the required 1 mm margin.
It is a geometry check, not a field solver or a substitute for native DRC.
"""
import collections, hashlib, json, math
from pathlib import Path
import pcbnew as p
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'hardware/kicad/carrier.kicad_pcb'
OUT=ROOT/'hardware/kicad/placement-refinement'
CAND=OUT/'carrier-locality.kicad_pcb'
MARGIN=1.0

def xy(v):return [p.ToMM(v.x),p.ToMM(v.y)]
def plane(b):
    zs=[z for z in b.Zones() if not z.GetIsRuleArea() and z.GetLayer()==p.B_Cu and z.GetNetname()=='GND']
    assert len(zs)==1
    s=p.SHAPE_POLY_SET(zs[0].GetFilledPolysList(p.B_Cu))
    s.Unfracture()  # Reconstruct holes before offsetting; fractured cut lines are not voids.
    return s

def metrics(b,check_plane=False):
    tracks=[t for t in b.GetTracks() if not isinstance(t,p.PCB_VIA)]
    vias=[t for t in b.GetTracks() if isinstance(t,p.PCB_VIA)]
    r={'vias_by_net':dict(sorted(collections.Counter(t.GetNetname() for t in vias).items())),
       'track_length_by_layer_mm':{b.GetLayerName(layer):round(sum(t.GetLength()/1e6 for t in tracks if t.GetLayer()==layer),3) for layer in [p.F_Cu,p.B_Cu]},
       'net_track_length_mm':{n:round(sum(t.GetLength()/1e6 for t in tracks if t.GetNetname()==n),3) for n in set(t.GetNetname() for t in tracks)}}
    if not check_plane:return r
    ground=plane(b); safe={}; bad=[]; checked=0
    for t in tracks:
        if t.GetLayer()!=p.F_Cu or t.GetNetname()=='GND':continue
        checked+=1;w=t.GetWidth()
        if w not in safe:
            s=p.SHAPE_POLY_SET(ground)
            s.Deflate(p.FromMM(MARGIN)+w//2+p.FromMM(.005),p.CORNER_STRATEGY_ROUND_ALL_CORNERS,p.FromMM(.001))
            safe[w]=s
        a=xy(t.GetStart());c=xy(t.GetEnd());length=math.dist(a,c)
        n=max(1,math.ceil(length/.025)); fail=[]
        for i in range(n+1):
            q=[a[j]+(c[j]-a[j])*i/n for j in [0,1]]
            if not safe[w].Contains(p.VECTOR2I(p.FromMM(q[0]),p.FromMM(q[1]))):fail.append(q)
        if fail:bad.append({'net':t.GetNetname(),'start_mm':a,'end_mm':c,'first_failure_mm':fail[0],'failed_samples':len(fail),'samples':n+1})
    r['ground_reference_audit']={'minimum_trace_edge_margin_mm':MARGIN,'sampling_step_mm':.025,'conservative_offset_mm':.005,'top_non_ground_segments_checked':checked,'failed_segments':bad,'bottom_ground_area_mm2':round(ground.Area()/1e12,2)}
    return r

def membership(b):
    return sorted((f.GetReference(),v.GetNumber(),v.GetNetname()) for f in b.GetFootprints() for v in f.Pads())

def picture(b):
    scale=24; y0=51; top=84
    im=Image.new('RGB',(105*scale+40,44*scale+top+40),'#101b24');d=ImageDraw.Draw(im)
    fontpath='/System/Library/Fonts/Supplemental/Arial.ttf'
    font=ImageFont.truetype(fontpath,16);small=ImageFont.truetype(fontpath,13);big=ImageFont.truetype(fontpath,24)
    def coord(v):return (20+v[0]*scale,top+(v[1]-y0)*scale)
    def polygon(chain,color):
        pts=[coord(xy(chain.CPoint(i))) for i in range(chain.PointCount())]
        if len(pts)>=3:d.polygon(pts,fill=color)
    g=plane(b)
    for i in range(g.OutlineCount()):
        polygon(g.COutline(i),'#183c3d')
        for j in range(g.HoleCount(i)):polygon(g.CHole(i,j),'#101b24')
    # Copper is shown as filled; red routes are the retained/trial top traces only.
    for t in b.GetTracks():
        if isinstance(t,p.PCB_VIA):
            x,y=coord(xy(t.GetPosition()));r=7
            d.ellipse([x-r,y-r,x+r,y+r],fill='#7ad7cc');d.ellipse([x-3,y-3,x+3,y+3],fill='#101b24')
        else:d.line([coord(xy(t.GetStart())),coord(xy(t.GetEnd()))],fill='#ff7d6a' if t.GetNetname()!='GND' else '#7ad7cc',width=max(2,round(t.GetWidth()/1e6*scale)))
    changed=json.loads((OUT/'placement.json').read_text())
    for f in b.GetFootprints():
        for v in f.Pads():
            if not v.IsOnLayer(p.F_Cu):continue
            poly=v.GetEffectivePolygon(p.F_Cu)
            col='#68baa9' if v.GetNetname()=='GND' else '#e8b976'
            for i in range(poly.OutlineCount()):polygon(poly.COutline(i),col)
            drill=v.GetDrillSize()
            if drill.x:
                x,y=coord(xy(v.GetPosition()));rx=p.ToMM(drill.x)*scale/2;ry=p.ToMM(drill.y)*scale/2
                d.ellipse([x-rx,y-ry,x+rx,y+ry],fill='#101b24')
        bounds=[]
        for item in f.GraphicalItems():
            if isinstance(item,p.PCB_SHAPE) and item.GetLayer()==p.F_CrtYd:
                bb=item.GetBoundingBox();bounds.extend([(p.ToMM(bb.GetLeft()),p.ToMM(bb.GetTop())),(p.ToMM(bb.GetRight()),p.ToMM(bb.GetBottom()))])
        if bounds:
            bx=[min(v[0] for v in bounds),min(v[1] for v in bounds),max(v[0] for v in bounds),max(v[1] for v in bounds)]
            if bx[3]<y0:continue
            col='#67c5ff' if f.GetReference() in changed else '#557777'
            d.rectangle([coord(bx[:2]),coord(bx[2:])],outline=col,width=1)
            x,y=coord([bx[0],bx[1]])
            d.text((x,y-16),f.GetReference(),font=font,fill=col)
    d.rectangle([0,0,im.width,top-8],fill='#101b24')
    d.text((20,12),'Flybee | locality placement revision',font=big,fill='white')
    d.text((20,46),'Blue outlines: moved/rotated parts   |   Red: partial top routing   |   Teal: actual bottom GND fill   |   Routing incomplete',font=font,fill='#b6ced9')
    im.save(OUT/'placement.png')

b=p.LoadBoard(str(BASE));c=p.LoadBoard(str(CAND))
assert membership(b)==membership(c),'Pad/net membership changed'
bf={f.GetReference():f for f in b.GetFootprints()};cf={f.GetReference():f for f in c.GetFootprints()}
fixed=[r for r in bf if r.startswith(('J','SW','MH'))]
for r in fixed:
    assert xy(bf[r].GetPosition())==xy(cf[r].GetPosition()) and bf[r].GetOrientationDegrees()==cf[r].GetOrientationDegrees(),r
complete=['CHG_IND_G','FULL_OFF_B','CHG_VSET','VBUS_DET','ILIM_SW']
drc=json.loads((OUT/'drc-placement.json').read_text())
for n in complete:
    assert not any('['+n+']' in i['description'] for v in drc['unconnected_items'] for i in v['items']),n
result={'source_sha256':hashlib.sha256(BASE.read_bytes()).hexdigest(),'candidate_sha256':hashlib.sha256(CAND.read_bytes()).hexdigest(),
        'stage':'placement with partial top routing; NOT a completed routed board','pad_net_membership_unchanged':True,'fixed_connectors_controls_and_mounting_footprints':fixed,
        'baseline':metrics(b),'candidate':metrics(c,True),'complete_trial_nets':complete,
        'drc_violation_counts':dict(collections.Counter(v['type'] for v in drc['violations'])),'unconnected_items':len(drc['unconnected_items'])}
(OUT/'audit.json').write_text(json.dumps(result,indent=2)+'\n')
picture(c)
print(json.dumps({k:result[k] for k in ['pad_net_membership_unchanged','drc_violation_counts','unconnected_items']},indent=2))
print(json.dumps(result['candidate']['ground_reference_audit'],indent=2))
