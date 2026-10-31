"""Run the real helper server against a fake Live set (for the plugin's tests).

usage: python3 fake_helper_server.py <info dir> [seconds]
Writes <info dir>/helper.json, answers requests, exits after [seconds] (default 60).
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "AbletonAi"))
sys.path.insert(0, HERE)
import core  # noqa: E402
import server  # noqa: E402
from fake_live import App, FakeLive, Song  # noqa: E402

song = Song()
srv = server.Server(core.Commands(song, App(song), FakeLive), info_dir=sys.argv[1], first_port=47480)
if not srv.start():
    sys.exit(1)
print("ready", srv.port, flush=True)
end = time.time() + float(sys.argv[2] if len(sys.argv) > 2 else 60)
while time.time() < end and not os.path.exists(os.path.join(sys.argv[1], "stop")):
    srv.process()
    time.sleep(0.01)
srv.stop()
