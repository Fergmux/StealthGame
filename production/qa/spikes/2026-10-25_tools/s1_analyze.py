"""S1/S14 analyzer: attribute LogRepProperties parent indices to actors and connections.
Usage: python -I s1_analyze.py <log> <startMarkerTag> <endMarkerTag>
Receive side (clients): '      Actor <full>:' / 'Channel Actor <full>:' set the actor; 'ReceiveProperties: Owner=<class>' sets layout class;
'ReceiveProperties_r: Parent=N' records N.  Send side (server): 'Replicate <name>, bNetInitial: a, bNetOwner: b' sets actor+owner flag;
'SendProperties: Owner=<class>' sets layout; 'SendProperties_r: Parent=N' records N.
"""
import re, sys, collections
log, start_tag, end_tag = sys.argv[1], sys.argv[2], sys.argv[3]
LINK = {7: 'Owner', 12: 'PlayerState', 13: 'Controller'}
PAWN_CLASS = 'BP_CrowdCharacter_C'
names = {}
recv = collections.defaultdict(lambda: collections.Counter())      # (pie, actor) -> Counter(parent)
send = collections.defaultdict(lambda: collections.Counter())      # (actor, owner) -> Counter(parent)
local = collections.defaultdict(set)                                # pie -> client actor names ever local
dumpkind = {}                                                        # (pie, actor) -> 'player'/'npc' guesses from server guid map
srv_guid_kind = collections.defaultdict(set)                         # guid -> kinds seen on server
cli_guid = {}                                                        # (pie, actor) -> guid
active = (start_tag == '-')
cur_actor = None; cur_class = None; cur_pie = None
s_actor = None; s_owner = None; s_class = None
re_actor = re.compile(r'LogNetTraffic: (?:      Channel Actor|      Actor) \S+ /\S+/UEDPIE_(\d)_[^:]*:PersistentLevel\.(\S+?):$')
re_recv_owner = re.compile(r'ReceiveProperties: Owner=(\S+)')
re_recv_parent = re.compile(r'ReceiveProperties_r: Parent=(\d+), Cmd')
re_rep = re.compile(r'LogNetTraffic: Replicate (\S+), bNetInitial: (\d), bNetOwner: (\d)')
re_send_owner = re.compile(r'SendProperties: Owner=(\S+),')
re_send_parent = re.compile(r'SendProperties_r: Parent=(\d+), Cmd')
re_dump = re.compile(r'\[SpikeDump\] pie=(\d) PAWN (\S+?)\{(\d+)\} local=(\d) .*PlayerState=(\S+) Controller')
re_tag = re.compile(r'\[SpikeDump\] ==== tag=(\S+) ')
for line in open(log, encoding='utf-8', errors='replace'):
    line = line.rstrip()
    m = re_tag.search(line)
    if m:
        if m.group(1) == start_tag: active = True
        if m.group(1) == end_tag and active is True: active = 'ending'
    m = re_dump.search(line)
    if m and active:
        pie, name, guid, loc, ps = int(m.group(1)), m.group(2), m.group(3), m.group(4), m.group(5)
        if pie == 0:
            srv_guid_kind[guid].add('player' if ps != 'null' else 'npc')
        else:
            cli_guid[(pie, name)] = guid
            if loc == '1': local[pie].add(name)
        continue
    if active is not True: 
        if active == 'ending' and '[SpikeDump] pie=2 SUMMARY' in line: break
        continue
    m = re_actor.search(line)
    if m:
        cur_pie, cur_actor = int(m.group(1)), m.group(2); cur_class = None; continue
    m = re_recv_owner.search(line)
    if m:
        cur_class = m.group(1).split('.')[-1]; continue
    m = re_recv_parent.search(line)
    if m and cur_actor and cur_class == PAWN_CLASS and cur_pie in (1, 2):
        recv[(cur_pie, cur_actor)][int(m.group(1))] += 1; continue
    m = re_rep.search(line)
    if m:
        s_actor, s_owner = m.group(1), m.group(3); s_class = None; continue
    m = re_send_owner.search(line)
    if m:
        s_class = m.group(1).split('.')[-1]; continue
    m = re_send_parent.search(line)
    if m and s_actor and s_class == PAWN_CLASS:
        send[(s_actor, s_owner)][int(m.group(1))] += 1
# Report
print('== SEND side (server): link parents per pawn channel by bNetOwner')
tot = collections.Counter()
for (actor, owner), c in sorted(send.items()):
    for p in LINK:
        if c[p]: tot[(owner, LINK[p])] += c[p]
for k, v in sorted(tot.items()): print('  bNetOwner=%s %-11s sends=%d' % (k[0], k[1], v))
print('  pawn channels with any send: owner=1 %d, owner=0 %d' % (sum(1 for k in send if k[1]=='1'), sum(1 for k in send if k[1]=='0')))
print('== RECEIVE side (clients): link parents on pawns this client never controlled (non-owner receipt)')
leaks = collections.Counter(); own = collections.Counter(); per_kind_sets = collections.defaultdict(set)
for (pie, actor), c in sorted(recv.items()):
    is_local = actor in local[pie]
    guid = cli_guid.get((pie, actor))
    kinds = srv_guid_kind.get(guid, set())
    kind = '/'.join(sorted(kinds)) or '?'
    if not is_local:
        per_kind_sets[kind] |= set(c)
    for p in LINK:
        if c[p]:
            (own if is_local else leaks)[(pie, kind, LINK[p])] += c[p]
for k, v in sorted(leaks.items()): print('  NON-OWNER pie=%d kind=%s %s received %d times' % (k[0], k[1], k[2], v))
if not leaks: print('  none')
for k, v in sorted(own.items()): print('  owner pie=%d kind=%s %s received %d times' % (k[0], k[1], k[2], v))
print('== Non-owner received parent-index sets by server-truth kind (union over pawns):')
for kind, s in sorted(per_kind_sets.items()): print('  %-12s %s' % (kind, sorted(s)))
print('  pawns analysed on clients: %d' % len(recv))
