# Independent F-2 golden computation for story tdc-045 (not the C++ under test).
import struct
def fnv(b):
    h=14695981039346656037
    for x in b:
        h^=x; h=(h*1099511628211)&0xFFFFFFFFFFFFFFFF
    return h
def canon(t,v):
    if t==0: return bytes([1 if v else 0])
    if t in (1,3): return struct.pack('<q',int(v))
    if t==2:
        f=struct.unpack('<f',struct.pack('<f',v))[0]
        if f==0.0: f=0.0
        return struct.pack('<f',f)
    if t==4:
        p=v.lower().encode('ascii'); return struct.pack('<H',len(p))+p
def stream(ver,entries):
    out=struct.pack('<I',ver)
    for k,t,v,pub in sorted(entries,key=lambda e:e[0].encode()):
        out+=bytes([len(k)])+k.encode()+bytes([t])+canon(t,v)
    return out
E=[("bFixtureFlag",0,True,True),("FixtureCount",1,42,True),("FixtureDelaySec",2,2.5,False),("FixtureMode",3,3,True),
   ("FixtureMotionAsset",4,"/Game/StealthGame/AI/DA_CrowdMotionProfile_Default.DA_CrowdMotionProfile_Default",False)]
print("TuningHash 0x%016x"%fnv(stream(7,E)))
print("ClientHash 0x%016x"%fnv(stream(7,[e for e in E if e[3]])))
print("stream", stream(7,E).hex())
print("TeamSize", (bytes([8])+b"TeamSize"+bytes([1])+canon(1,5)).hex())
print("DefenceDurationSec", (bytes([18])+b"DefenceDurationSec"+bytes([2])+canon(2,4.0)).hex())
