import sys, glob, statistics
from PIL import Image
d=sys.argv[1]
fs=sorted(glob.glob(d+'/capture_f*.bmp'))
vals=[]
for f in fs:
    try: im=Image.open(f).convert('RGB').resize((320,180))
    except Exception: continue
    w=sum(1 for r,g,b in im.get_flattened_data() if r>=215 and g>=215 and b>=215) if hasattr(im,'get_flattened_data') else sum(1 for r,g,b in im.getdata() if r>=215 and g>=215 and b>=215)
    vals.append((f.split('/')[-1],w))
spikes=[]
for i,(f,w) in enumerate(vals):
    nb=[v for j,(_,v) in enumerate(vals) if j!=i and abs(j-i)<=3]
    if w>statistics.median(nb)+150: spikes.append((f,w,int(statistics.median(nb))))
print(d, len(vals), 'spikes:', [s for s in spikes if len(sys.argv)>2 or int(s[0][9:15])>1100])
