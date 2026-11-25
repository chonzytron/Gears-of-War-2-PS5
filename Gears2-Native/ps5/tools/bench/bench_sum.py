import sys, re, glob, statistics
d, name = sys.argv[1], sys.argv[2]
log = open(glob.glob(d + '/gow2_*.log')[0], errors='replace').read().splitlines()
fps = []; busy = []
for l in log:
    m = re.search(r'ps5 diag (\d+)s: frames \d+ \(([\d.]+)/s.*?gpu busy (\d+)%.*?slow frames (\d+) \(over 20 ms; (\d+) over 40', l)
    if m and int(m.group(1)) >= 50:
        fps.append(float(m.group(2))); busy.append(int(m.group(3)))
gpu = [l.split('ps5 gpu: ', 1)[1] for l in log if 'ps5 gpu: ' in l]
cp = [l.split('ps5 cp: ', 1)[1] for l in log if "ps5 cp: a frame's command processor" in l]
print(f'== {name}: fps (50 s on) mean {statistics.mean(fps):.1f} min {min(fps):.1f} | gpu busy mean {statistics.mean(busy):.0f}%' if fps else f'== {name}: no diag')
if gpu: print('  gpu:', gpu[-1][:330])
if cp: print('  cp:', cp[-1][:260])
