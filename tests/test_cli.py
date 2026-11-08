#!/usr/bin/env python3
"""Integration tests with independently calculated timing and malformed inputs."""
import pathlib
import shutil
import subprocess
import sys
import tempfile

EXE = pathlib.Path(sys.argv[1]).resolve()
DEMO = pathlib.Path(__file__).resolve().parents[1] / 'examples/pdf_demo'
count = 0
def run(changes=None, error=None):
    global count
    with tempfile.TemporaryDirectory() as tmp:
        case = pathlib.Path(tmp) / 'case'
        shutil.copytree(DEMO, case)
        for name, edit in (changes or {}).items():
            p = case / name
            p.write_text(edit(p.read_text()))
        output = pathlib.Path(tmp) / 'out.structure'
        p = subprocess.run([str(EXE), str(case), str(output), "--no-opt"], capture_output=True, text=True)
        if error:
            assert p.returncode != 0 and error in p.stderr, (p.stdout, p.stderr)
            assert not output.exists()
        else:
            assert p.returncode == 0, p.stderr
            import csv
            with pathlib.Path(str(output)+'.metrics.csv').open() as f:
                data=list(csv.DictReader(f))
            baseline=data[0]
            assert abs(float(baseline['area'])-2.7)<1e-12
            assert float(baseline['setup_tns']) == 0
            assert abs(float(baseline['hold_tns'])+.022)<1e-12
            assert abs(float(baseline['hold_wns'])+.0188)<1e-12
            shutil.copyfile(output, case/'clk_tree.structure')
            out2 = pathlib.Path(tmp)/'out2.structure'
            again = subprocess.run([str(EXE), str(case), str(out2), "--no-opt"], capture_output=True, text=True)
            assert again.returncode == 0
            assert out2.read_bytes() == output.read_bytes()
        count += 1

run()
run({'clk_tree.structure': lambda s: '\ufeff'+s.replace('    ','\t').replace('\n','\r\n')})
run({'clk_tree.structure': lambda s: s.replace('FF_12','FF_37')}, 'duplicate component')
run({'clk_tree.structure': lambda s: s.replace('[1]','[2]',1)}, 'cannot skip levels')
run({'clk_tree.structure': lambda s: s.replace('REALBUF_X8','UNKNOWN',1)}, 'unknown buffer type')
run({'clk_tree.structure': lambda s: s.replace('[3] FF_12','[4] FF_12')}, 'SINK cannot have children')
run({'clk_tree.structure': lambda s: s+' [2] BUF_EMPTY (REALBUF_X2)\n'}, 'unconnected output')
run({'clk_tree.structure': lambda s: s+' [1] DIRECT (FIFO) (SINK)\n'}, 'driven by a buffer')
run({'buf.lib': lambda s: s.replace('0.006 0.0098 0.0152 0.0247 0.0372','0.006')}, 'fanout exceeds corner 1')
run({'buf.lib': lambda s: s.replace('SIZE 0.5 BY 0.5','SIZE 0.5 BY 0.5\n SIZE 0.5 BY 0.5')}, 'duplicate field')
run({'buf.lib': lambda s: s.rstrip()[:-1]}, 'unclosed cell')
run({'buf.lib': lambda s: s.replace('0.051','nan')}, 'finite nonnegative')
run({'SS_delay.rpt': lambda s: s.replace('FF_5','MISSING')}, 'not a known SINK')
run({'SS_delay.rpt': lambda s: s.replace('FF_5','BUF_0')}, 'not a known SINK')
run({'SS_delay.rpt': lambda s: s.replace('Path2','Path1')}, 'duplicate path')
run({'SS_delay.rpt': lambda s: s.replace('0.162','-1')}, 'finite nonnegative')
run({'SS_delay.rpt': lambda s: s+'garbage\n'}, 'invalid report row')
run({'SS_delay.rpt': lambda s: s.replace('0.3','0',1)}, 'must be positive')
print(f'PASS: {count} cases (including timing oracle and round-trip checks)')
