#!/usr/bin/env python3
"""Build the placement study with KiCad's bundled Python; never edits the routed baseline."""
import json
from pathlib import Path
import pcbnew as p

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'hardware/kicad/carrier.kicad_pcb'
OUT = ROOT / 'hardware/kicad/placement-refinement'

def vec(x,y): return p.VECTOR2I(p.FromMM(x),p.FromMM(y))
def pos(item):
    v=item.GetPosition()
    return [round(p.ToMM(v.x),5),round(p.ToMM(v.y),5)]

def build():
    b=p.LoadBoard(str(BASE))
    moves=json.loads((OUT/'placement.json').read_text())
    fps={f.GetReference():f for f in b.GetFootprints()}
    changes=[]
    for ref,(x,y,angle) in moves.items():
        f=fps[ref]
        changes.append({'ref':ref,'value':f.GetValue(),'before':pos(f)+[f.GetOrientationDegrees()],'after':[x,y,angle]})
        f.SetPosition(vec(x,y)); f.SetOrientationDegrees(angle)
        # Moved references are placed afresh; all values remain on the fabrication layer.
        f.Reference().SetPosition(vec(x,y-2.0)); f.Reference().SetTextAngle(p.EDA_ANGLE(0))
        f.Reference().SetLayer(p.F_Fab)
    for ref in ['C18','C20','R5','R6','C26']:
        fps[ref].Reference().SetLayer(p.F_Fab)
    for item in fps['TP5'].GraphicalItems():
        if isinstance(item,p.PCB_TEXT): item.SetLayer(p.F_Fab)
    # Preserve only unchanged local converter routes and their thermal vias.
    core_nets = {'CHG_SW','CHG_BTST','CHG_PMID','BOOST_SW','BOOST_BOOT',
                 'BOOST_VCC','BOOST_FSW','BOOST_SS','BOOST_COMP','BOOST_ILIM','LOAD_CT'}
    kept=0
    original_tracks = list(b.GetTracks())
    for t in original_tracks:
        thermal = isinstance(t,p.PCB_VIA) and t.GetNetname()=='GND' and any(
            (pos(t)[0]-pos(fps[r])[0])**2+(pos(t)[1]-pos(fps[r])[1])**2 < 1.5**2
            for r in ['U4','U6','U7'])
        if thermal: kept+=1
        elif t.GetNetname() not in core_nets: b.RemoveNative(t)
    assert kept==17,kept
    nets={n.GetNetname():n for n in b.GetNetsByNetcode().values()}
    def point(v):
        if isinstance(v,str):
            ref,num=v.split('.')
            return next(pad.GetPosition() for pad in fps[ref].Pads() if pad.GetNumber()==num)
        return vec(*v)
    def route(net,points,width=.2):
        for a,c in zip(points,points[1:]):
            t=p.PCB_TRACK(b);t.SetStart(point(a));t.SetEnd(point(c))
            t.SetWidth(p.FromMM(width));t.SetLayer(p.F_Cu);t.SetNet(nets[net]);b.Add(t)
    def ground(points,at):
        route('GND',points+[at],.18)
        v=p.PCB_VIA(b);v.SetPosition(vec(*at));v.SetWidth(p.FromMM(.6));v.SetDrill(p.FromMM(.3))
        v.SetViaType(p.VIATYPE_THROUGH);v.SetLayerPair(p.F_Cu,p.B_Cu);v.SetNet(nets['GND']);b.Add(v)
    route('CHG_IND_G',['R32.2','Q3.1'])
    route('FULL_OFF_B',['R35.2',(46.0625,84.5125),'Q5.1'])
    route('CHG_VSET',['U4.12',(67.25,77.4),(68.925,79.075),'R7.1'])
    route('VBUS_DET',['U2.4','R1.2'])
    route('ILIM_SW',['Q1.3',(61.1125,79.65),(62.1875,80.725),'R4.2'])
    route('CHG_ILIM',['U4.8',(65.25,77.4),'R5.1','R4.1'])
    route('CHG_ICHG',['U4.10',(66.25,77.65),(66.72,78.12),'R6.1'])
    route('BAT_TS',['U4.11',(66.75,77.5),(67.6,78.35),(67.6,81.625),'R8.2'])
    route('USB_3V3',['U1.5','C2.1'])
    route('USB_VBUS',['C32.1',(89.475,60.675),(88.7,61.45),'U1.1'])
    route('VBAT',['C27.1',(56.7625,59),'U8.2','U8.3'])
    ground(['U2.10'],(80.3,64.8)); route('GND',['U2.10','U2.11'],.18)
    ground(['U10.7'],(62.3,57.85)); ground(['U10.9'],(64.2,58.95))
    ground(['J1.A1',(83.25,87.75)],(82.5,87.0))
    ground(['J1.A12',(89.75,87.75)],(90.5,87.0))
    for z in b.Zones():
        if not z.GetIsRuleArea(): z.UnFill()
    b.GetTitleBlock().SetTitle('Flybee — locality placement study — UNROUTED')
    candidate=OUT/'carrier-locality.kicad_pcb'
    p.SaveBoard(str(candidate),b)
    # The study is one directory below the original project and shares its models.
    candidate.write_text(candidate.read_text().replace('${KIPRJMOD}/lib/','${KIPRJMOD}/../lib/'))
    (OUT/'placement-changes.json').write_text(json.dumps(changes,indent=2)+'\n')
    # Same rules as the baseline; the plane-reference constraint is checked by audit_placement.py.
    (OUT/'carrier-locality.kicad_pro').write_bytes(BASE.with_suffix('.kicad_pro').read_bytes())
    (OUT/'fp-lib-table').write_text((BASE.parent/'fp-lib-table').read_text().replace('${KIPRJMOD}/lib/','${KIPRJMOD}/../lib/'))
    print('Moved',len(changes),'footprints; retained',kept,'thermal vias.')

if __name__=='__main__': build()
