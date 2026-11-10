#!/usr/bin/env python3
"""Run all complete testcase directories; measure whole process including I/O."""
import argparse
import csv
import pathlib
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('executable', type=pathlib.Path)
p.add_argument('testcase_root', type=pathlib.Path)
p.add_argument('output_root', type=pathlib.Path)
p.add_argument('--expected-count', type=int, default=5)
p.add_argument('--alpha', type=float, default=.4)
p.add_argument('--beta', type=float, default=.3)
p.add_argument('--gamma', type=float, default=.3)
p.add_argument('--seconds', type=float, default=540)
p.add_argument('--fixed-area', type=float, default=0)
a = p.parse_args()
exe = a.executable.resolve()
root = a.testcase_root.resolve()
out = a.output_root.resolve()
# Never place generated data inside the input tree.
if root == out or root in out.parents:
    p.error('output_root must be outside testcase_root')
cases = sorted({f.parent for f in root.rglob('clk_tree.structure')
                if all((f.parent/n).is_file() for n in ('buf.lib','SS_delay.rpt','FF_delay.rpt'))})
if not cases:
    p.error('no complete testcase directories found')
if len(cases) != a.expected_count:
    print(f'NOTICE: found {len(cases)} cases; expected {a.expected_count}')
out.mkdir(parents=True, exist_ok=True)
rows = []
for case in cases:
    name = str(case.relative_to(root))
    dest = out / (case.relative_to(root) if case != root else pathlib.Path('root_case'))
    dest.mkdir(parents=True, exist_ok=True)
    output = dest/'modified_clk_tree.structure'
    sidecar = pathlib.Path(str(output)+'.metrics.csv')
    # Do not report stale output as this run's success.
    output.unlink(missing_ok=True)
    sidecar.unlink(missing_ok=True)
    cmd = [str(exe),str(case),str(output),'--alpha',str(a.alpha),'--beta',str(a.beta),
           '--gamma',str(a.gamma),'--seconds',str(a.seconds),'--fixed-area',str(a.fixed_area)]
    start = time.monotonic()
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
        elapsed = time.monotonic()-start
        (dest/'run.log').write_text(result.stdout+'\n'+result.stderr)
        status = 'OK' if result.returncode == 0 and elapsed < 600 else 'ERROR'
    except subprocess.TimeoutExpired:
        elapsed = time.monotonic()-start
        (dest/'run.log').write_text('TIMEOUT: exceeded 600 seconds\n')
        status = 'TIMEOUT'
    record = {'testcase':name,'status':status,'wall_seconds':elapsed}
    if status == 'OK' and sidecar.exists():
        with sidecar.open() as f:
            data = list(csv.DictReader(f))
        before, after = data
        for k in ('setup_wns','setup_tns','hold_wns','hold_tns','area'):
            record['original_'+k] = before[k]
            record['optimized_'+k] = after[k]
        for k in ('local_score','spec_score_defined','legal','positive_score'):
            record[k] = after[k]
        # Reparse exported tree and check independently of in-memory IDs.
        try:
            check = subprocess.run([str(exe),str(case),str(output),'--check',str(output)],
                                   capture_output=True,text=True,timeout=600)
            (dest/'check.log').write_text(check.stdout+'\n'+check.stderr)
            check_failed = check.returncode != 0
        except subprocess.TimeoutExpired:
            (dest/'check.log').write_text('CHECK_TIMEOUT\n')
            check_failed = True
        if check_failed:
            record['status'] = 'CHECK_FAILED'
        elif after['positive_score'] != '1':
            record['status'] = 'NONPOSITIVE'
        elif after['spec_score_defined'] != '1':
            record['status'] = 'LOCAL_SCORE_ONLY'
    elif status == 'OK':
        record['status'] = 'MISSING_METRICS'
    rows.append(record)
    print(name,record['status'],f'{elapsed:.3f}s',flush=True)
fields = list(dict.fromkeys(k for r in rows for k in r))
with (out/'summary.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
passed = len(cases)==a.expected_count and all(r['status']=='OK' for r in rows)
print('All expected cases legal, positive SPEC-formula score, under 600s:',passed)
raise SystemExit(0 if passed else 1)
