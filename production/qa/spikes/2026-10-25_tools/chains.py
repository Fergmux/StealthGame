import re,math,collections,sys
B=collections.defaultdict(dict); P=collections.defaultdict(dict); order=collections.defaultdict(list)
for l in open(sys.argv[1]):
    m=re.match(r'mesh=(\S+) i=(\d+) name=(\S+) parent=(\S+) cs=([-\d.]+),([-\d.]+),([-\d.]+)',l)
    if not m: continue
    mesh,n,p=m.group(1),m.group(3),m.group(4)
    if n in B[mesh]: continue
    B[mesh][n]=tuple(map(float,m.groups()[4:])); P[mesh][n]=p; order[mesh].append(n)
d=math.dist
chains=[('thigh','thigh_l','calf_l'),('shin','calf_l','foot_l'),('foot(ankle->ball)','foot_l','ball_l'),
('spine(pelvis->neck_01)','pelvis','neck_01'),('clavicle->upperarm','clavicle_l','upperarm_l'),('upperarm','upperarm_l','lowerarm_l'),
('forearm','lowerarm_l','hand_l'),('hand(wrist->middle_03)','hand_l','middle_03_l'),('neck(neck_01->head)','neck_01','head')]
def pathlen(mesh,a,b):
    tot=0;n=b
    while n!=a:
        p=P[mesh].get(n,'-')
        if p=='-': return None
        tot+=d(B[mesh][n],B[mesh][p]); n=p
    return tot
ref=B.get('SKM_UEFN_Mannequin',{})
for mesh in B:
    names=order[mesh]
    print('==',mesh,'bones',len(names),'first',names[:3],'head z %.2f'%B[mesh].get('head',(0,0,0))[2])
    for c,a,b in chains:
        if a in B[mesh] and b in B[mesh]:
            pl=pathlen(mesh,a,b)
            print('  %-26s straight %7.2f  path %s'%(c,d(B[mesh][a],B[mesh][b]),('%7.2f'%pl) if pl else '-'))
    if ref and mesh!='SKM_UEFN_Mannequin':
        extra=[n for n in names if n not in ref]
        print('  bones not in SKM_UEFN_Mannequin:',extra[:10])
