rows=[("AssassinationRangeCm",2,1,2),("DefenceCooldownSec",2,1,1),("DefenceDurationSec",2,1,1),("NpcCount",1,0,0),
("NpcKillStrikeLimit",1,1,0),("ObjectiveInteractSec",2,1,1),("TargetRoundMinutes",2,0,0),("TeamSize",1,1,0)]
rows.sort(key=lambda r:r[0].encode())
h=14695981039346656037
def upd(b):
    global h
    for x in b:
        h^=x; h=(h*1099511628211)&0xFFFFFFFFFFFFFFFF
for k,t,p,l in rows:
    upd(bytes([len(k)])+k.encode()+bytes([t,p,l,0,0]))
print(hex(h))
