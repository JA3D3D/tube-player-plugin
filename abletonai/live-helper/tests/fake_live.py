"""A small stand-in for Live's Python API, used by the helper tests and the plugin tests."""
import collections


class Param(object):
    def __init__(self, name, value=0.0, lo=0.0, hi=1.0, items=None):
        self.name, self._v, self.min, self.max = name, value, lo, hi
        self.is_quantized = items is not None
        self.value_items = items or []

    @property
    def value(self):
        return self._v

    @value.setter
    def value(self, v):
        if v < self.min or v > self.max:
            raise ValueError("out of range")
        self._v = v

    def str_for_value(self, v):
        return self.value_items[int(v)] if self.is_quantized else "{0:.2f}".format(v)


class Device(object):
    def __init__(self, name, params=None):
        self.name, self.class_name = name, name.replace(" ", "")
        self.parameters = params or [Param("Device On", 1, 0, 1)]


class Pad(object):
    def __init__(self, note, name, filled):
        self.note, self.name, self.chains = note, name, [object()] if filled else []


class DrumRack(Device):
    def __init__(self, name):
        Device.__init__(self, name)
        self.can_have_drum_pads = True
        names = {36: "Kick 909", 38: "Snare 909", 42: "Hihat Closed", 46: "Hihat Open"}
        self.drum_pads = [Pad(n, names.get(n, ""), n in names) for n in range(128)]


NoteSpec = collections.namedtuple("NoteSpec", "pitch start_time duration velocity mute")


class Clip(object):
    def __init__(self, length):
        self.length, self.name, self.color, self.is_midi_clip = float(length), "", 0, True
        self.notes = []

    def add_new_notes(self, specs):
        self.notes.extend(specs)

    def get_notes_extended(self, from_pitch, pitch_span, from_time, time_span):
        return list(self.notes)

    def remove_notes_extended(self, *a):
        self.notes = []


class Slot(object):
    def __init__(self):
        self.clip, self.fired = None, False

    @property
    def has_clip(self):
        return self.clip is not None

    def create_clip(self, length):
        assert self.clip is None, "slot not empty"
        self.clip = Clip(length)

    def delete_clip(self):
        self.clip = None

    def fire(self):
        self.fired = True


class Mixer(object):
    def __init__(self):
        self.volume = Param("Track Volume", 0.85)
        self.panning = Param("Track Panning", 0.0, -1.0, 1.0)


class Track(object):
    def __init__(self, name, midi, scenes):
        self.name, self.has_midi_input, self.color = name, midi, 0x999999
        self.mute = self.solo = self.arm = False
        self.devices, self.mixer_device = [], Mixer()
        self.clip_slots = [Slot() for _ in range(scenes)]
        self.arranged = []

    def duplicate_clip_to_arrangement(self, clip, beat):
        self.arranged.append((clip.name, beat))


class Scene(object):
    def __init__(self, name=""):
        self.name, self.fired = name, False

    def fire(self):
        self.fired = True


class SongView(object):
    def __init__(self):
        self.selected_track, self.selected_device = None, None

    def select_device(self, d):
        self.selected_device = d


class Song(object):
    def __init__(self):
        self.tempo, self.signature_numerator, self.signature_denominator = 120.0, 4, 4
        self.is_playing, self.view = False, SongView()
        self.scenes = [Scene() for _ in range(4)]
        self.tracks = [Track("1-MIDI", True, 4), Track("2-Audio", False, 4)]
        self.return_tracks = [Track("A-Reverb", False, 0)]
        self.undos = 3

    def create_midi_track(self, i):
        t = Track("MIDI", True, len(self.scenes))
        self.tracks.insert(i if i >= 0 else len(self.tracks), t)

    def create_audio_track(self, i):
        t = Track("Audio", False, len(self.scenes))
        self.tracks.insert(i if i >= 0 else len(self.tracks), t)

    def create_scene(self, i):
        self.scenes.insert(i if i >= 0 else len(self.scenes), Scene())
        for t in self.tracks:
            t.clip_slots.insert(i if i >= 0 else len(t.clip_slots), Slot())

    def start_playing(self):
        self.is_playing = True

    def stop_playing(self):
        self.is_playing = False

    def stop_all_clips(self):
        pass

    @property
    def can_undo(self):
        return self.undos > 0

    def undo(self):
        self.undos -= 1


class Item(object):
    def __init__(self, name, children=(), loadable=True, device=False):
        self.name, self.children, self.is_loadable = name, list(children), loadable
        self.is_folder, self.is_device, self.uri = bool(children), device, "query:" + name

    def walk(self):
        yield self
        for c in self.children:
            for x in c.walk():
                yield x


class Browser(object):
    def __init__(self, song):
        self.song = song
        self.instruments = Item("Instruments", [Item("Wavetable", [Item("Bass", [Item("Basic Sub.adv")], loadable=False)], device=True),
                                                Item("Operator", device=True)], loadable=False)
        self.audio_effects = Item("Audio Effects", [Item("Echo", device=True), Item("EQ Eight", device=True),
                                                    Item("Saturator", device=True), Item("Reverb", device=True)], loadable=False)
        self.midi_effects = Item("MIDI Effects", [Item("Arpeggiator", device=True)], loadable=False)
        self.drums = Item("Drums", [Item("909 Core Kit.adg"), Item("Kit-Core Tearout.adg")], loadable=False)
        self.sounds = Item("Sounds", [Item("Bass", [Item("Reese Bass.adv"), Item("Deep Sub.adv")], loadable=False)], loadable=False)
        self.loaded = []

    def load_item(self, item):
        t = self.song.view.selected_track
        self.loaded.append((t.name, item.name))
        dev = Device(item.name.rsplit(".", 1)[0], [Param("Device On", 1, 0, 1), Param("Dry/Wet", 0.5),
                                                  Param("Sync", 0, 0, 1, ["Off", "On"]), Param("Feedback", 0.3)])
        sel = self.song.view.selected_device
        if sel in t.devices:
            t.devices.insert(t.devices.index(sel) + 1, dev)
        else:
            t.devices.append(dev)


class AppView(object):
    def __init__(self):
        self.shown = None

    def show_view(self, v):
        self.shown = v


class App(object):
    def __init__(self, song):
        self.browser, self.view = Browser(song), AppView()

    def get_major_version(self):
        return 11

    def get_minor_version(self):
        return 3

    def get_bugfix_version(self):
        return 35


class FakeLive(object):
    class Clip(object):
        MidiNoteSpecification = staticmethod(lambda **k: NoteSpec(**k))


