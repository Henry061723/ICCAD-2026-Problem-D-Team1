#!/usr/bin/env python3
import csv
import pathlib
import subprocess
import sys
import tempfile
exe=pathlib.Path(sys.argv[1]).resolve()
project=pathlib.Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(project/'tests/test_cli.py'),str(exe)],check=True)
case=project/'examples/pdf_demo'
with tempfile.TemporaryDirectory() as tmp:
    out=pathlib.Path(tmp)/'modified_clk_tree.structure'
    def run(*args):
        return subprocess.run([str(exe),str(case),str(out),*args],capture_output=True,text=True)
    r=run();assert r.returncode==0,r.stderr
    with pathlib.Path(str(out)+'.metrics.csv').open() as f:
        before,after=list(csv.DictReader(f))
    assert float(after['local_score'])>0
    assert float(after['setup_tns'])==0
    assert float(after['hold_tns'])>=float(before['hold_tns'])
    assert after['spec_score_defined']=='0' # demo initially has no setup violation
    assert run('--check',str(out)).returncode==0
    assert run('--alpha','.5').returncode!=0
    assert run('--seconds','601').returncode!=0
    assert run('--passes','1.5').returncode!=0
    assert run('--max-trials','0').returncode==0
    with pathlib.Path(str(out)+'.metrics.csv').open() as f:
        _,after=list(csv.DictReader(f))
    assert float(after['local_score'])==0
    assert run('--fixed-area','10','--no-opt').returncode==0
    with pathlib.Path(str(out)+'.metrics.csv').open() as f:
        before,_=list(csv.DictReader(f))
    assert abs(float(before['area'])-12.7)<1e-12
print('PASS: optimizer round-trip, CLI limits, zero policy, fixed area')
