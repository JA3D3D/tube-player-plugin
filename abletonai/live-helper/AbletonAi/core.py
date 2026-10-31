# AbletonAi Live helper: the commands that change your Live set.
#
# Runs inside Live (as a control surface script). Every command here is called on Live's
# main thread by server.py; nothing in this file touches sockets or threads.
# Written for the Python 3.7 that Live 11 uses.

from __future__ import absolute_import, print_function, unicode_literals

import time

VERSION = "1.0.0"

# browser roots Claude can search, in the order a plain search walks them
ROOTS = ("instruments", "drums", "sounds", "audio_effects", "midi_effects", "plugins",
         "max_for_live", "user_library", "packs", "samples", "clips")
SEARCH_BUDGET_S = 1.2          # a search runs on Live's main thread: keep Live responsive
SEARCH_NODE_LIMIT = 8000
MAX_NOTES = 4000


class CommandError(Exception):
    pass


def _num(v, name, lo=None, hi=None):
    try:
        f = float(v)
    except (TypeError, ValueError):
        raise CommandError("{0} must be a number".format(name))
    if f != f:
        raise CommandError("{0} must be a number".format(name))
    if lo is not None and f < lo:
        raise CommandError("{0} must be at least {1}".format(name, lo))
    if hi is not None and f > hi:
        raise CommandError("{0} must be at most {1}".format(name, hi))
    return f


def _int(v, name, lo=None, hi=None):
    f = _num(v, name, lo, hi)
    if int(f) != f:
        raise CommandError("{0} must be a whole number".format(name))
    return int(f)


def _hex(c):
    try:
        return "#{0:06x}".format(int(c) & 0xFFFFFF)
    except Exception:
        return None


COLOURS = {"red": 0xFF3636, "orange": 0xFFA529, "yellow": 0xFFF034, "lime": 0x87FF67, "green": 0x3DC300,
           "teal": 0x19E9FF, "cyan": 0x5CE2FF, "blue": 0x1090F2, "indigo": 0x5480E4, "purple": 0x8D79EA,
           "violet": 0xB17BF4, "pink": 0xFF39D4, "magenta": 0xE553A0, "white": 0xFFFFFF, "gray": 0x9E9E9E,
           "grey": 0x9E9E9E, "brown": 0xA16A2A}


def _colour(v):
    if v is None:
        return None
    s = str(v).strip().lower()
    if s in COLOURS:
        return COLOURS[s]
    if s.startswith("#") and len(s) == 7:
        try:
            return int(s[1:], 16)
        except ValueError:
            pass
    raise CommandError("colour should be a name like 'pink' or a hex like '#ff2e93'")


class Commands(object):
    """One method per command; `run` dispatches by name."""

    def __init__(self, song, app, live_module=None, log=None):
        # song/app may be callables: Live swaps the Song object when you open another set
        self._song = song
        self._app = app
        self.Live = live_module
        self.log = log or (lambda m: None)
        self._items = {}            # uri -> browser item, from recent searches

    @property
    def song(self):
        return self._song() if callable(self._song) else self._song

    @property
    def app(self):
        return self._app() if callable(self._app) else self._app

    # ------------------------------------------------------------------ dispatch
    def run(self, cmd, args):
        if not isinstance(args, dict):
            args = {}
        fn = getattr(self, "cmd_" + str(cmd), None)
        if fn is None:
            raise CommandError("unknown command: {0}".format(cmd))
        return fn(args)

    def cmd_batch(self, a):
        out = []
        for c in a.get("commands") or []:
            try:
                out.append({"ok": True, "result": self.run(c.get("cmd"), c.get("args") or {})})
            except Exception as e:
                out.append({"ok": False, "error": str(e)})
        return out

    # ------------------------------------------------------------------ lookups
    def _track(self, a, key="track"):
        tracks = list(self.song.tracks)
        if key not in a:
            raise CommandError("which track? give its number ({0}, starting at 0)".format(key))
        i = _int(a[key], key, 0)
        if i >= len(tracks):
            raise CommandError("there is no track {0}; the set has {1} tracks".format(i, len(tracks)))
        return i, tracks[i]

    def _slot(self, track, a):
        slots = list(track.clip_slots)
        i = _int(a.get("slot", 0), "slot", 0)
        while i >= len(slots):          # add scenes so the slot exists
            self.song.create_scene(-1)
            slots = list(track.clip_slots)
        return i, slots[i]

    def _device(self, track, a):
        devs = list(track.devices)
        i = _int(a.get("device", 0), "device", 0)
        if i >= len(devs):
            raise CommandError("track '{0}' has {1} devices; there is no device {2}".format(track.name, len(devs), i))
        return i, devs[i]

    # ------------------------------------------------------------------ reading the set
    def cmd_ping(self, a):
        return {"helper": VERSION, "live": self._live_version()}

    def _live_version(self):
        try:
            return "{0}.{1}.{2}".format(self.app.get_major_version(), self.app.get_minor_version(), self.app.get_bugfix_version())
        except Exception:
            return "unknown"

    def _track_info(self, i, t, detail=True):
        info = {"index": i, "name": t.name, "type": "midi" if t.has_midi_input else "audio",
                "color": _hex(getattr(t, "color", 0))}
        for k in ("mute", "solo", "arm"):
            try:
                info[k] = bool(getattr(t, k))
            except Exception:
                pass
        try:
            v = t.mixer_device.volume
            info["volume"] = round(v.value, 3)
            info["volume_display"] = v.str_for_value(v.value)
        except Exception:
            pass
        info["devices"] = [{"index": j, "name": d.name, "class": getattr(d, "class_name", "")} for j, d in enumerate(t.devices)]
        if detail:
            clips = []
            for j, s in enumerate(t.clip_slots):
                if s.has_clip:
                    c = s.clip
                    clips.append({"slot": j, "name": c.name, "length": c.length, "midi": bool(getattr(c, "is_midi_clip", True))})
            info["clips"] = clips
        return info

    def cmd_get_session(self, a):
        s = self.song
        tracks = list(s.tracks)
        sel = None
        try:
            sel = tracks.index(s.view.selected_track)
        except Exception:
            pass
        return {
            "live": self._live_version(), "tempo": s.tempo,
            "time_signature": "{0}/{1}".format(s.signature_numerator, s.signature_denominator),
            "playing": bool(s.is_playing), "selected_track": sel,
            "tracks": [self._track_info(i, t) for i, t in enumerate(tracks)],
            "return_tracks": [t.name for t in s.return_tracks],
            "scenes": [{"index": i, "name": sc.name} for i, sc in enumerate(s.scenes)],
        }

    def cmd_get_track(self, a):
        i, t = self._track(a)
        return self._track_info(i, t)

    def cmd_get_clip_notes(self, a):
        ti, t = self._track(a)
        si, slot = self._slot(t, a)
        if not slot.has_clip:
            raise CommandError("track {0} slot {1} is empty".format(ti, si))
        c = slot.clip
        notes = []
        try:
            got = c.get_notes_extended(0, 128, 0.0, c.length)
            for n in got:
                notes.append({"pitch": n.pitch, "start": round(n.start_time, 4), "duration": round(n.duration, 4), "velocity": int(n.velocity)})
        except Exception:
            for n in c.get_notes(0.0, 0, c.length, 128):
                notes.append({"pitch": n[0], "start": round(n[1], 4), "duration": round(n[2], 4), "velocity": int(n[3])})
        notes.sort(key=lambda n: (n["start"], n["pitch"]))
        return {"name": c.name, "length": c.length, "notes": notes}

    # ------------------------------------------------------------------ song
    def cmd_set_tempo(self, a):
        self.song.tempo = _num(a.get("bpm"), "bpm", 20, 999)
        return {"tempo": self.song.tempo}

    def cmd_set_time_signature(self, a):
        self.song.signature_numerator = _int(a.get("numerator", 4), "numerator", 1, 99)
        self.song.signature_denominator = _int(a.get("denominator", 4), "denominator", 1, 16)
        return {"time_signature": "{0}/{1}".format(self.song.signature_numerator, self.song.signature_denominator)}

    def cmd_play(self, a):
        self.song.start_playing()
        return {"playing": True}

    def cmd_stop(self, a):
        self.song.stop_playing()
        return {"playing": False}

    def cmd_stop_all_clips(self, a):
        self.song.stop_all_clips()
        return {"stopped": True}

    def cmd_undo(self, a):
        n = _int(a.get("steps", 1), "steps", 1, 50)
        done = 0
        for _ in range(n):
            if not self.song.can_undo:
                break
            self.song.undo()
            done += 1
        return {"undone": done}

    # ------------------------------------------------------------------ tracks
    def cmd_create_track(self, a):
        kind = str(a.get("type", "midi")).lower()
        if kind not in ("midi", "audio"):
            raise CommandError("type must be 'midi' or 'audio'")
        n = len(list(self.song.tracks))
        index = _int(a.get("index", -1), "index", -1)
        if index > n:
            index = -1
        (self.song.create_midi_track if kind == "midi" else self.song.create_audio_track)(index)
        new_index = index if index >= 0 else len(list(self.song.tracks)) - 1
        t = list(self.song.tracks)[new_index]
        self._apply_track(t, a)
        return self._track_info(new_index, t, detail=False)

    def _apply_track(self, t, a):
        if a.get("name"):
            t.name = str(a["name"])[:60]
        if a.get("color") is not None:
            t.color = _colour(a.get("color"))
        for k in ("mute", "solo", "arm"):
            if k in a:
                try:
                    setattr(t, k, bool(a[k]))
                except Exception:
                    raise CommandError("can't change {0} on track '{1}'".format(k, t.name))
        if "volume" in a:
            p = t.mixer_device.volume
            p.value = max(p.min, min(p.max, _num(a["volume"], "volume")))
        if "pan" in a:
            p = t.mixer_device.panning
            p.value = max(-1.0, min(1.0, _num(a["pan"], "pan")))

    def cmd_set_track(self, a):
        i, t = self._track(a)
        self._apply_track(t, a)
        return self._track_info(i, t, detail=False)

    def cmd_select_track(self, a):
        i, t = self._track(a)
        self.song.view.selected_track = t
        return {"selected_track": i}

    # ------------------------------------------------------------------ scenes
    def cmd_create_scene(self, a):
        index = _int(a.get("index", -1), "index", -1)
        n = len(list(self.song.scenes))
        if index > n:
            index = -1
        self.song.create_scene(index)
        new_index = index if index >= 0 else len(list(self.song.scenes)) - 1
        if a.get("name"):
            list(self.song.scenes)[new_index].name = str(a["name"])[:60]
        return {"index": new_index}

    def cmd_set_scene(self, a):
        scenes = list(self.song.scenes)
        i = _int(a.get("scene"), "scene", 0, len(scenes) - 1)
        if a.get("name") is not None:
            scenes[i].name = str(a["name"])[:60]
        return {"index": i, "name": scenes[i].name}

    def cmd_fire_scene(self, a):
        scenes = list(self.song.scenes)
        i = _int(a.get("scene"), "scene", 0, len(scenes) - 1)
        scenes[i].fire()
        return {"fired": i}

    # ------------------------------------------------------------------ clips
    def _clean_notes(self, notes, length):
        if not isinstance(notes, list):
            raise CommandError("notes must be a list")
        if len(notes) > MAX_NOTES:
            raise CommandError("too many notes ({0}); split the part into several clips".format(len(notes)))
        out = []
        for k, n in enumerate(notes):
            if not isinstance(n, dict):
                raise CommandError("note {0} should be an object with pitch, start, duration, velocity".format(k))
            pitch = _int(n.get("pitch"), "note {0} pitch".format(k), 0, 127)
            start = _num(n.get("start", 0), "note {0} start".format(k), 0)
            dur = _num(n.get("duration", 0.25), "note {0} duration".format(k), 0.001)
            vel = int(round(_num(n.get("velocity", 100), "note {0} velocity".format(k), 1, 127)))
            if start >= length:
                continue            # outside the clip
            out.append((pitch, start, min(dur, length - start), vel, bool(n.get("mute", False))))
        return out

    def _write_notes(self, clip, notes):
        spec = None
        try:
            spec = self.Live.Clip.MidiNoteSpecification
        except Exception:
            spec = None
        if spec is not None and hasattr(clip, "add_new_notes"):
            clip.add_new_notes(tuple(spec(pitch=p, start_time=s, duration=d, velocity=v, mute=m) for (p, s, d, v, m) in notes))
        else:
            clip.set_notes(tuple(notes))

    def _clear_notes(self, clip):
        try:
            clip.remove_notes_extended(0, 128, 0.0, clip.length)
        except Exception:
            clip.remove_notes(0.0, 0, clip.length, 128)

    def cmd_write_clip(self, a):
        ti, t = self._track(a)
        if not t.has_midi_input:
            raise CommandError("track '{0}' is an audio track; MIDI clips go on MIDI tracks".format(t.name))
        si, slot = self._slot(t, a)
        length = _num(a.get("length", 4), "length (beats)", 0.25, 4096)
        notes = self._clean_notes(a.get("notes") or [], length)
        mode = str(a.get("mode", "replace")).lower()
        if slot.has_clip:
            if mode == "add":
                clip = slot.clip
            elif mode == "replace":
                slot.delete_clip()
                slot.create_clip(length)
                clip = slot.clip
            else:
                raise CommandError("slot {0} on '{1}' already has a clip".format(si, t.name))
        else:
            slot.create_clip(length)
            clip = slot.clip
        if a.get("name"):
            clip.name = str(a["name"])[:60]
        if a.get("color") is not None:
            try:
                clip.color = _colour(a.get("color"))
            except Exception:
                pass
        if notes:
            self._write_notes(clip, notes)
        return {"track": ti, "slot": si, "name": clip.name, "length": clip.length, "notes": len(notes)}

    def cmd_delete_clip(self, a):
        ti, t = self._track(a)
        si, slot = self._slot(t, a)
        if slot.has_clip:
            slot.delete_clip()
        return {"track": ti, "slot": si, "deleted": True}

    def cmd_fire_clip(self, a):
        ti, t = self._track(a)
        si, slot = self._slot(t, a)
        slot.fire()
        return {"track": ti, "slot": si}

    def cmd_arrange_clip(self, a):
        """Copy a Session clip into the Arrangement at a bar."""
        ti, t = self._track(a)
        si, slot = self._slot(t, a)
        if not slot.has_clip:
            raise CommandError("track {0} slot {1} is empty".format(ti, si))
        beats_per_bar = 4.0 * self.song.signature_numerator / self.song.signature_denominator
        if "bar" in a:
            beat = (_num(a["bar"], "bar", 1) - 1) * beats_per_bar
        else:
            beat = _num(a.get("beat", 0), "beat", 0)
        fn = getattr(t, "duplicate_clip_to_arrangement", None)
        if fn is None:
            raise CommandError("this version of Live can't place clips in the Arrangement from a script")
        fn(slot.clip, float(beat))
        return {"track": ti, "slot": si, "beat": beat}

    def cmd_show_view(self, a):
        view = {"arrangement": "Arranger", "session": "Session", "detail": "Detail/Clip"}.get(str(a.get("view", "session")).lower(), "Session")
        self.app.view.show_view(view)
        return {"view": view}

    # ------------------------------------------------------------------ browser: instruments, effects, presets
    def _root(self, name):
        b = self.app.browser
        try:
            return getattr(b, name)
        except Exception:
            return None

    def cmd_search_browser(self, a):
        q = str(a.get("query", "")).strip().lower()
        if not q:
            raise CommandError("what should I search for?")
        roots = a.get("category")
        roots = [roots] if isinstance(roots, str) and roots else list(ROOTS)
        limit = _int(a.get("limit", 15), "limit", 1, 50)
        words = [w for w in q.replace("-", " ").split() if w]
        found, seen, t0 = [], 0, time.time()
        for rname in roots:
            root = self._root(rname)
            if root is None:
                continue
            stack = [(c, rname + "/" + c.name, 1) for c in root.children]
            while stack and seen < SEARCH_NODE_LIMIT and time.time() - t0 < SEARCH_BUDGET_S:
                item, path, depth = stack.pop(0)
                seen += 1
                name = item.name
                base = name.rsplit(".", 1)[0].lower() if "." in name[-5:] else name.lower()
                score = 0
                if base == q:
                    score = 100
                elif base.startswith(q):
                    score = 70
                elif all(w in base for w in words):
                    score = 50
                elif all(w in path.lower() for w in words):
                    score = 20
                if score and item.is_loadable:
                    uri = getattr(item, "uri", path)
                    self._items[uri] = item
                    found.append({"name": name, "path": path, "uri": uri, "score": score - 0.01 * depth,
                                  "category": rname, "device": bool(getattr(item, "is_device", False))})
                if item.is_folder and depth < 6:
                    stack.extend((c, path + "/" + c.name, depth + 1) for c in item.children)
        found.sort(key=lambda f: (-f["score"], len(f["path"])))
        for f in found:
            f.pop("score", None)
        return {"results": found[:limit], "searched": seen, "complete": seen < SEARCH_NODE_LIMIT and time.time() - t0 < SEARCH_BUDGET_S}

    def cmd_list_browser(self, a):
        path = str(a.get("path", "")).strip("/")
        if not path:
            return {"folders": [r for r in ROOTS if self._root(r) is not None]}
        parts = path.split("/")
        node = self._root(parts[0])
        if node is None:
            raise CommandError("unknown browser section '{0}'".format(parts[0]))
        for p in parts[1:]:
            nxt = None
            for c in node.children:
                if c.name == p:
                    nxt = c
                    break
            if nxt is None:
                raise CommandError("'{0}' not found in {1}".format(p, path))
            node = nxt
        items = []
        for c in node.children:
            uri = getattr(c, "uri", path + "/" + c.name)
            if c.is_loadable:
                self._items[uri] = c
            items.append({"name": c.name, "folder": bool(c.is_folder), "loadable": bool(c.is_loadable), "uri": uri})
            if len(items) >= 200:
                break
        return {"path": path, "items": items}

    def _find_item(self, a):
        uri = a.get("uri")
        if uri and uri in self._items:
            return self._items[uri]
        path = a.get("path")
        if path:
            parts = str(path).strip("/").split("/")
            node = self._root(parts[0])
            for p in parts[1:]:
                if node is None:
                    break
                node = next((c for c in node.children if c.name == p), None)
            if node is not None and node.is_loadable:
                return node
        q = a.get("name") or a.get("query")
        if q:
            res = self.cmd_search_browser({"query": q, "category": a.get("category"), "limit": 1})["results"]
            if res:
                return self._items.get(res[0]["uri"])
        raise CommandError("couldn't find that in Live's browser; search for it first")

    def cmd_load_device(self, a):
        ti, t = self._track(a)
        item = self._find_item(a)
        self.song.view.selected_track = t
        devs = list(t.devices)
        if devs and a.get("position", "end") == "end":
            try:
                self.song.view.select_device(devs[-1])      # effects go after the last device
            except Exception:
                pass
        self.app.browser.load_item(item)
        return {"track": ti, "loaded": item.name, "devices": [d.name for d in t.devices]}

    def cmd_get_drum_pads(self, a):
        """The filled pads of a Drum Rack: which note plays which sound."""
        ti, t = self._track(a)
        devs = list(t.devices)
        if "device" in a:
            di, d = self._device(t, a)
        else:
            di, d = next(((i, x) for i, x in enumerate(devs) if getattr(x, "can_have_drum_pads", False)), (None, None))
            if d is None:
                raise CommandError("track '{0}' has no Drum Rack".format(t.name))
        if not getattr(d, "can_have_drum_pads", False):
            raise CommandError("'{0}' is not a Drum Rack".format(d.name))
        pads = []
        for p in d.drum_pads:
            try:
                if len(list(p.chains)):
                    pads.append({"note": p.note, "name": p.name})
            except Exception:
                pass
        return {"track": ti, "device": di, "name": d.name, "pads": pads}

    # ------------------------------------------------------------------ device parameters
    def _param_info(self, i, p):
        info = {"index": i, "name": p.name, "value": round(p.value, 4), "min": p.min, "max": p.max}
        try:
            info["display"] = p.str_for_value(p.value)
        except Exception:
            pass
        if getattr(p, "is_quantized", False):
            try:
                info["options"] = list(p.value_items)
            except Exception:
                pass
        return info

    def cmd_get_device_params(self, a):
        ti, t = self._track(a)
        di, d = self._device(t, a)
        params = [self._param_info(i, p) for i, p in enumerate(d.parameters)]
        f = str(a.get("filter", "")).lower()
        if f:
            params = [p for p in params if f in p["name"].lower()]
        return {"track": ti, "device": di, "name": d.name, "params": params[:200]}

    def cmd_set_device_params(self, a):
        ti, t = self._track(a)
        di, d = self._device(t, a)
        plist = list(d.parameters)
        results = []
        for ch in a.get("params") or []:
            p = None
            if "index" in ch:
                k = _int(ch["index"], "param index", 0)
                p = plist[k] if k < len(plist) else None
            else:
                want = str(ch.get("name", "")).lower()
                p = next((x for x in plist if x.name.lower() == want), None) or next((x for x in plist if want and want in x.name.lower()), None)
            if p is None:
                results.append({"param": ch.get("name", ch.get("index")), "ok": False, "error": "no such parameter"})
                continue
            try:
                if "option" in ch and getattr(p, "is_quantized", False):
                    opts = [str(o).lower() for o in p.value_items]
                    v = opts.index(str(ch["option"]).lower())
                else:
                    v = _num(ch.get("value"), p.name)
                p.value = max(p.min, min(p.max, v))
                r = {"param": p.name, "ok": True, "value": round(p.value, 4)}
                try:
                    r["display"] = p.str_for_value(p.value)
                except Exception:
                    pass
                results.append(r)
            except Exception as e:
                results.append({"param": p.name, "ok": False, "error": str(e)})
        return {"track": ti, "device": di, "name": d.name, "results": results}
