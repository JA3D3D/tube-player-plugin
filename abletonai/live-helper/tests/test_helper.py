"""Tests for the AbletonAi Live helper against a stand-in for Live's API.

usage: python3 abletonai/live-helper/tests/test_helper.py
"""
import json
import os
import socket
import stat
import sys
import tempfile
import threading
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "AbletonAi"))
sys.path.insert(0, HERE)
import core  # noqa: E402
import server  # noqa: E402


from fake_live import *  # noqa: E402,F401,F403


# ---------------------------------------------------------------- tests
class CommandsTest(unittest.TestCase):
    def setUp(self):
        self.song = Song()
        self.app = App(self.song)
        self.c = core.Commands(lambda: self.song, lambda: self.app, FakeLive)

    def run_(self, cmd, **a):
        return self.c.run(cmd, a)

    def test_ping_and_session(self):
        self.assertEqual(self.run_("ping")["live"], "11.3.35")
        s = self.run_("get_session")
        self.assertEqual(s["tempo"], 120.0)
        self.assertEqual([t["type"] for t in s["tracks"]], ["midi", "audio"])
        self.assertEqual(s["return_tracks"], ["A-Reverb"])
        self.assertEqual(len(s["scenes"]), 4)

    def test_tempo(self):
        self.assertEqual(self.run_("set_tempo", bpm=150)["tempo"], 150)
        with self.assertRaises(core.CommandError):
            self.run_("set_tempo", bpm=5)
        with self.assertRaises(core.CommandError):
            self.run_("set_tempo", bpm="fast")

    def test_create_track(self):
        r = self.run_("create_track", type="midi", name="Sub Bass", color="pink")
        self.assertEqual((r["index"], r["name"], r["type"], r["color"]), (2, "Sub Bass", "midi", "#ff39d4"))
        r = self.run_("create_track", type="audio", index=0, name="Vox")
        self.assertEqual((r["index"], self.song.tracks[0].name), (0, "Vox"))
        with self.assertRaises(core.CommandError):
            self.run_("create_track", type="video")
        with self.assertRaises(core.CommandError):
            self.run_("create_track", color="plaid")

    def test_write_and_read_clip(self):
        notes = [{"pitch": 36, "start": 0, "duration": 0.5, "velocity": 120},
                 {"pitch": 38, "start": 1, "duration": 0.25},
                 {"pitch": 42, "start": 9, "duration": 1}]       # outside a 8-beat clip: dropped
        r = self.run_("write_clip", track=0, slot=1, length=8, name="Drop drums", notes=notes)
        self.assertEqual((r["notes"], r["length"], r["name"]), (2, 8.0, "Drop drums"))
        got = self.run_("get_clip_notes", track=0, slot=1)
        self.assertEqual([(n["pitch"], n["start"], n["velocity"]) for n in got["notes"]], [(36, 0.0, 120), (38, 1.0, 100)])
        # replace (default) swaps the clip; add keeps it
        self.run_("write_clip", track=0, slot=1, length=4, notes=[{"pitch": 60, "start": 0, "duration": 1}])
        self.assertEqual(len(self.run_("get_clip_notes", track=0, slot=1)["notes"]), 1)
        self.run_("write_clip", track=0, slot=1, length=4, mode="add", notes=[{"pitch": 64, "start": 1, "duration": 1}])
        self.assertEqual(len(self.run_("get_clip_notes", track=0, slot=1)["notes"]), 2)
        with self.assertRaises(core.CommandError):
            self.run_("write_clip", track=0, slot=1, mode="keep", notes=[])
        # notes clipped to the clip's end
        self.run_("write_clip", track=0, slot=2, length=4, notes=[{"pitch": 60, "start": 3, "duration": 4}])
        self.assertEqual(self.run_("get_clip_notes", track=0, slot=2)["notes"][0]["duration"], 1.0)

    def test_clip_errors(self):
        with self.assertRaises(core.CommandError) as e:
            self.run_("write_clip", track=1, slot=0, notes=[])
        self.assertIn("audio track", str(e.exception))
        with self.assertRaises(core.CommandError):
            self.run_("write_clip", track=9, slot=0, notes=[])
        with self.assertRaises(core.CommandError):
            self.run_("write_clip", track=0, slot=0, notes=[{"pitch": 200, "start": 0, "duration": 1}])
        with self.assertRaises(core.CommandError):
            self.run_("write_clip", track=0, slot=0, notes=[{"pitch": 60, "start": 0, "duration": 0}])

    def test_slot_beyond_scenes_adds_scenes(self):
        self.run_("write_clip", track=0, slot=6, length=4, notes=[{"pitch": 60, "start": 0, "duration": 1}])
        self.assertEqual(len(self.song.scenes), 7)
        self.assertTrue(self.song.tracks[0].clip_slots[6].has_clip)

    def test_browser_search_and_load(self):
        r = self.run_("search_browser", query="echo")
        self.assertEqual(r["results"][0]["name"], "Echo")
        self.assertEqual(r["results"][0]["path"], "audio_effects/Echo")
        r = self.run_("search_browser", query="reese", category="sounds")
        self.assertEqual(r["results"][0]["path"], "sounds/Bass/Reese Bass.adv")
        self.assertEqual(self.run_("search_browser", query="kazoo")["results"], [])
        # load an instrument, then two effects: effects go after the last device
        self.run_("load_device", track=0, uri=self.run_("search_browser", query="wavetable")["results"][0]["uri"])
        self.run_("load_device", track=0, name="Saturator")
        r = self.run_("load_device", track=0, path="audio_effects/Echo")
        self.assertEqual(r["devices"], ["Wavetable", "Saturator", "Echo"])
        self.assertIs(self.song.view.selected_track, self.song.tracks[0])
        with self.assertRaises(core.CommandError):
            self.run_("load_device", track=0, name="Kazoo")

    def test_list_browser(self):
        self.assertIn("audio_effects", self.run_("list_browser")["folders"])
        items = self.run_("list_browser", path="sounds/Bass")["items"]
        self.assertEqual([i["name"] for i in items], ["Reese Bass.adv", "Deep Sub.adv"])
        r = self.run_("load_device", track=0, uri=items[1]["uri"])
        self.assertEqual(r["loaded"], "Deep Sub.adv")

    def test_device_params(self):
        self.run_("load_device", track=0, name="Echo")
        p = self.run_("get_device_params", track=0, device=0)
        self.assertEqual([x["name"] for x in p["params"]], ["Device On", "Dry/Wet", "Sync", "Feedback"])
        self.assertEqual(p["params"][2]["options"], ["Off", "On"])
        self.assertEqual(len(self.run_("get_device_params", track=0, device=0, filter="feed")["params"]), 1)
        r = self.run_("set_device_params", track=0, device=0, params=[
            {"name": "dry/wet", "value": 0.25}, {"name": "Sync", "option": "On"},
            {"name": "Feedback", "value": 5}, {"name": "Nope", "value": 1}])["results"]
        self.assertEqual([x["ok"] for x in r], [True, True, True, False])
        d = self.song.tracks[0].devices[0]
        self.assertEqual((d.parameters[1].value, d.parameters[2].value, d.parameters[3].value), (0.25, 1, 1.0))
        with self.assertRaises(core.CommandError):
            self.run_("get_device_params", track=0, device=3)

    def test_mixer_scenes_transport(self):
        self.run_("set_track", track=0, volume=0.7, pan=-0.3, mute=True, name="Lead")
        t = self.song.tracks[0]
        self.assertEqual((t.mixer_device.volume.value, t.mixer_device.panning.value, t.mute, t.name), (0.7, -0.3, True, "Lead"))
        self.assertEqual(self.run_("create_scene", name="Drop")["index"], 4)
        self.assertEqual(self.song.scenes[4].name, "Drop")
        self.run_("fire_scene", scene=4)
        self.assertTrue(self.song.scenes[4].fired)
        self.run_("play")
        self.assertTrue(self.song.is_playing)
        self.run_("stop")
        self.assertFalse(self.song.is_playing)
        self.assertEqual(self.run_("undo", steps=5)["undone"], 3)

    def test_drum_pads(self):
        self.song.tracks[0].devices.append(Device("Glue"))
        self.song.tracks[0].devices.append(DrumRack("909 Core Kit"))
        r = self.run_("get_drum_pads", track=0)
        self.assertEqual((r["device"], [p["note"] for p in r["pads"]]), (1, [36, 38, 42, 46]))
        self.assertEqual(r["pads"][0]["name"], "Kick 909")
        with self.assertRaises(core.CommandError):
            self.run_("get_drum_pads", track=0, device=0)
        with self.assertRaises(core.CommandError):
            self.run_("get_drum_pads", track=1)

    def test_arrange(self):
        self.run_("write_clip", track=0, slot=0, length=16, name="Drop", notes=[])
        self.run_("arrange_clip", track=0, slot=0, bar=17)
        self.assertEqual(self.song.tracks[0].arranged, [("Drop", 64.0)])
        with self.assertRaises(core.CommandError):
            self.run_("arrange_clip", track=0, slot=3, bar=1)

    def test_batch_and_unknown(self):
        r = self.run_("batch", commands=[{"cmd": "set_tempo", "args": {"bpm": 140}}, {"cmd": "nope"}])
        self.assertEqual([x["ok"] for x in r], [True, False])
        with self.assertRaises(core.CommandError):
            self.run_("format_hard_drive")

    def test_new_set_is_followed(self):
        self.song = Song()           # Live opened another set
        self.song.tempo = 99
        self.assertEqual(self.run_("get_session")["tempo"], 99)


class ServerTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.song = Song()
        self.cmds = core.Commands(self.song, App(self.song), FakeLive)
        self.srv = server.Server(self.cmds, info_dir=self.dir, first_port=47460)
        self.assertTrue(self.srv.start())
        self.stop = False
        # stand-in for Live calling update_display
        self.pump = threading.Thread(target=self._pump)
        self.pump.daemon = True
        self.pump.start()

    def _pump(self):
        while not self.stop:
            self.srv.process()
            time.sleep(0.02)

    def tearDown(self):
        self.stop = True
        self.srv.stop()

    def ask(self, sock, obj):
        sock.sendall((json.dumps(obj) + "\n").encode())
        buf = b""
        while b"\n" not in buf:
            buf += sock.recv(65536)
        return json.loads(buf.split(b"\n")[0])

    def test_round_trip(self):
        info_path = os.path.join(self.dir, "helper.json")
        with open(info_path) as f:
            info = json.load(f)
        self.assertEqual(info["port"], self.srv.port)
        self.assertEqual(stat.S_IMODE(os.stat(info_path).st_mode), 0o600)
        s = socket.create_connection(("127.0.0.1", info["port"]), timeout=5)
        r = self.ask(s, {"id": 1, "token": "nope", "cmd": "ping"})
        self.assertEqual((r["ok"], r["error"]), (False, "wrong token"))
        r = self.ask(s, {"id": 2, "token": info["token"], "cmd": "ping"})
        self.assertEqual((r["id"], r["ok"], r["result"]["helper"]), (2, True, core.VERSION))
        r = self.ask(s, {"id": 3, "token": info["token"], "cmd": "set_tempo", "args": {"bpm": 1}})
        self.assertFalse(r["ok"])
        self.assertIn("at least", r["error"])
        s.sendall(b"not json\n")
        r = self.ask(s, {"id": 4, "token": info["token"], "cmd": "create_track", "args": {"name": "Bass"}})
        # the bad line answers first, then the real request
        self.assertTrue(r["error"] == "bad request" or r["ok"])
        s.close()

    def test_second_helper_takes_next_port_and_cleanup(self):
        other = server.Server(self.cmds, info_dir=tempfile.mkdtemp(), first_port=47460)
        self.assertTrue(other.start())
        self.assertEqual(other.port, self.srv.port + 1)
        other.stop()
        self.srv.stop()
        self.assertFalse(os.path.exists(os.path.join(self.dir, "helper.json")))


if __name__ == "__main__":
    unittest.main(verbosity=2)
