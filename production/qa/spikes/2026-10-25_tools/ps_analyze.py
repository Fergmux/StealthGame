"""S2 analyzer: PlayerState properties received by each client, split own PS vs other players' PS.
Usage: python -I ps_analyze.py <log>"""
import re, sys, collections
log = sys.argv[1]
NAMES = {7:'Owner',10:'Score',11:'PlayerId',12:'CompressedPing',13:'bIsSpectator',14:'bOnlySpectator',15:'bIsABot',16:'bIsInactive',17:'bFromPreviousLevel',18:'StartTime',19:'UniqueId',20:'PlayerNamePrivate',21:'Team',22:'LifeState',23:'NpcKills',24:'LastDefenceActivationTime',0:'bReplicateMovement',1:'bHidden',2:'bTearOff',3:'bCanBeDamaged',4:'RemoteRole',5:'AttachmentReplication',6:'ReplicatedMovement',8:'Role',9:'Instigator'}
re_actor = re.compile(r'LogNetTraffic: (?:      Channel Actor|      Actor) \S+ /\S+/UEDPIE_(\d)_[^:]*:PersistentLevel\.(\S+?):$')
re_owner = re.compile(r'ReceiveProperties: Owner=(\S+)')
re_parent = re.compile(r'ReceiveProperties_r: Parent=(\d+), Cmd')
re_ps = re.compile(r'\[SpikeDump\] pie=(\d) PS (\S+?)\{\d+\} .* owner=(\S+) pawn=')
recv = collections.defaultdict(collections.Counter); own = {}
cur = None; cls = None
for line in open(log, encoding='utf-8', errors='replace'):
    line = line.rstrip()
    m = re_ps.search(line)
    if m and m.group(1) != '0':
        if m.group(3) != 'null': own[(int(m.group(1)), m.group(2))] = True
        continue
    m = re_actor.search(line)
    if m: cur = (int(m.group(1)), m.group(2)); cls = None; continue
    m = re_owner.search(line)
    if m: cls = m.group(1).split('.')[-1]; continue
    m = re_parent.search(line)
    if m and cur and cls == 'StealthPlayerState' and cur[0] in (1, 2):
        recv[cur][int(m.group(1))] += 1
agg = {'own': collections.Counter(), 'other': collections.Counter()}
for key, c in recv.items():
    agg['own' if own.get(key) else 'other'].update(c)
for k in ('other', 'own'):
    print('%s PlayerState (both clients): %s' % (k, ', '.join('%s x%d' % (NAMES.get(p, p), n) for p, n in sorted(agg[k].items()))))
