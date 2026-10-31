// AbletonAi: what Claude knows and what it can do in Live.
// Loaded by ui.html (inlined at build time). Plain browser JavaScript.
(function (root) {
  'use strict';

  var MODELS = {
    fast: { id: 'claude-haiku-5-5', label: 'Fast' },
    balanced: { id: 'claude-sonnet-5-5', label: 'Balanced' },
    best: { id: 'claude-opus-5-5', label: 'Best' }
  };

  var SYSTEM = [
    'You are AbletonAi, a music producer working inside the user\'s Ableton Live set through tools.',
    'You write MIDI, build tracks with Live\'s own instruments and effects, dial in effect settings, and sketch parts or whole songs.',
    'The user makes bass music and electronic music (tearout, riddim, brostep, melodic dubstep, future bass, colour bass, deep dubstep, drum & bass, midtempo, techno, melodic techno) but help with any style.',
    '',
    'HOW THE SET WORKS',
    '- Tracks, clip slots and scenes are numbered from 0. Clip length and note times are in beats (quarter notes): in 4/4 a bar is 4 beats, a 16th note is 0.25.',
    '- MIDI pitch numbers: Ableton calls MIDI 60 "C3" (so C1 = 36, C2 = 48, C4 = 72). Bass usually sits around 24-48, chords 48-72, leads 60-84.',
    '- Drum Racks: after loading a kit, call get_drum_pads to see which note plays which sound, and program those notes. Live\'s kits usually put the kick on 36, snare 38, clap 39, closed hat 42, open hat 46, but check.',
    '- Live 11 Suite instruments: Operator, Wavetable, Analog, Simpler, Sampler, Drum Rack, Collision, Electric, Tension, Impulse. Presets live in the "sounds" section (Bass, Pad, Lead, Keys, Synth, Pluck, Brass...), drum kits in "drums".',
    '- Audio effects include: Auto Filter, Auto Pan, Beat Repeat, Channel EQ, Chorus-Ensemble, Compressor, Corpus, Delay, Drum Buss, Dynamic Tube, Echo, EQ Eight, Erosion, Frequency Shifter, Gate, Glue Compressor, Grain Delay, Hybrid Reverb, Limiter, Multiband Dynamics (OTT-style), Overdrive, Pedal, Phaser-Flanger, Redux, Resonators, Reverb, Saturator, Spectral Resonator, Spectral Time, Utility, Vinyl Distortion, Vocoder. MIDI effects: Arpeggiator, Chord, Note Length, Pitch, Random, Scale, Velocity.',
    '- Device parameter values are Live\'s raw values between each parameter\'s min and max. Read them with get_device_params (use a filter), set them with set_device_params, and check the "display" text that comes back (for example "-6.0 dB" or "1/8"). For switches and menus use "option".',
    '- A track\'s volume of 0.85 is 0 dB.',
    '',
    'HOW TO WORK',
    '- Start each new request with get_session unless you just looked.',
    '- Never delete or overwrite the user\'s own clips or tracks. Put new parts on new tracks or in empty slots. Only replace clips you made earlier in this chat, or when the user asks.',
    '- "The selected track" means selected_track from get_session.',
    '- For a part: create a MIDI track named for its role, load a fitting instrument or preset (search_browser, then load_device with the uri), write the whole clip in one write_clip call, give clips and tracks clear names and colours.',
    '- For an effect or sound request: load the devices on the right track, then set the important parameters to musical values and say what you set.',
    '- For a full song or a big section: pick tempo, key and structure from the request (only ask if something essential is missing), make one scene per section (Intro, Build, Drop, Break, Build 2, Drop 2, Outro...), name the scenes, create the tracks, and write a clip for each track in each section with real variation between sections. Keep loops 4-16 bars. When done, say how to play it (launch the scenes in order) and offer to lay it out in the Arrangement with arrange_clip.',
    '- Write real, musical parts: groove, ghost notes and velocity changes on drums, bass that locks with the kick, voice-led chords, call and response. Stay in the key.',
    '- Be efficient: few searches, no repeated calls, complete clips in one call.',
    '- If a tool says the Live helper is not connected, stop and tell the user how to switch it on: Live > Settings > Link, Tempo & MIDI > Control Surface > AbletonAi (Input and Output: None).',
    '- Keep replies short and friendly: what you made, where it is, one idea for next. No long theory unless asked. The user can undo your changes in Live with Cmd+Z.'
  ].join('\n');

  var NOTE = {
    type: 'object',
    properties: {
      pitch: { type: 'integer', minimum: 0, maximum: 127, description: 'MIDI note number (60 = C3)' },
      start: { type: 'number', minimum: 0, description: 'start in beats from the clip start' },
      duration: { type: 'number', exclusiveMinimum: 0, description: 'length in beats' },
      velocity: { type: 'integer', minimum: 1, maximum: 127 }
    },
    required: ['pitch', 'start', 'duration']
  };
  var TRACK = { type: 'integer', minimum: 0, description: 'track number, from 0' };
  var SLOT = { type: 'integer', minimum: 0, description: 'clip slot (scene) number, from 0' };

  function tool(name, description, props, required) {
    return { name: name, description: description, input_schema: { type: 'object', properties: props || {}, required: required || [] } };
  }

  var TOOLS = [
    tool('get_session', 'Look at the Live set: tempo, time signature, tracks (type, devices, clips), scenes, the selected track.'),
    tool('get_track', 'Details of one track: devices and clips.', { track: TRACK }, ['track']),
    tool('set_tempo', 'Set the song tempo.', { bpm: { type: 'number', minimum: 20, maximum: 999 } }, ['bpm']),
    tool('set_time_signature', 'Set the time signature.', { numerator: { type: 'integer' }, denominator: { type: 'integer' } }, ['numerator', 'denominator']),
    tool('create_track', 'Add a track. Returns its index.', {
      type: { type: 'string', enum: ['midi', 'audio'] }, name: { type: 'string' },
      color: { type: 'string', description: 'a name (red, orange, yellow, green, cyan, blue, purple, pink, gray) or #rrggbb' },
      index: { type: 'integer', description: 'position, -1 (default) for the end' }
    }, ['type', 'name']),
    tool('set_track', 'Rename, recolour or mix a track.', {
      track: TRACK, name: { type: 'string' }, color: { type: 'string' },
      volume: { type: 'number', minimum: 0, maximum: 1, description: '0.85 = 0 dB' },
      pan: { type: 'number', minimum: -1, maximum: 1 }, mute: { type: 'boolean' }, solo: { type: 'boolean' }, arm: { type: 'boolean' }
    }, ['track']),
    tool('select_track', 'Select a track in Live.', { track: TRACK }, ['track']),
    tool('create_scene', 'Add a scene (a row of clip slots), for example one per song section.', { name: { type: 'string' }, index: { type: 'integer' } }),
    tool('set_scene', 'Rename a scene.', { scene: { type: 'integer', minimum: 0 }, name: { type: 'string' } }, ['scene', 'name']),
    tool('write_clip', 'Write a MIDI clip into a slot on a MIDI track (creates scenes if needed). mode "replace" (default) replaces a clip already in the slot, "add" adds notes to it.', {
      track: TRACK, slot: SLOT, length: { type: 'number', description: 'clip length in beats (4 per bar in 4/4)' },
      name: { type: 'string' }, color: { type: 'string' }, mode: { type: 'string', enum: ['replace', 'add'] },
      notes: { type: 'array', items: NOTE }
    }, ['track', 'slot', 'length', 'notes']),
    tool('get_clip_notes', 'Read the notes of a MIDI clip.', { track: TRACK, slot: SLOT }, ['track', 'slot']),
    tool('delete_clip', 'Delete a clip (only ones you made, or when the user asks).', { track: TRACK, slot: SLOT }, ['track', 'slot']),
    tool('search_browser', 'Search Live\'s browser for instruments, presets, drum kits, effects or plugins. Returns uris to load.', {
      query: { type: 'string' },
      category: { type: 'string', enum: ['instruments', 'drums', 'sounds', 'audio_effects', 'midi_effects', 'plugins', 'max_for_live', 'user_library', 'packs', 'samples'] },
      limit: { type: 'integer', minimum: 1, maximum: 30 }
    }, ['query']),
    tool('list_browser', 'List a folder of Live\'s browser, e.g. "sounds/Bass". Empty path lists the sections.', { path: { type: 'string' } }),
    tool('load_device', 'Load an instrument, preset, kit or effect onto a track (effects go after the existing devices; an instrument replaces the current one). Use a uri from search_browser.', {
      track: TRACK, uri: { type: 'string' }, path: { type: 'string' }, name: { type: 'string', description: 'search and load the best match' }
    }, ['track']),
    tool('get_drum_pads', 'Which note plays which sound in a track\'s Drum Rack.', { track: TRACK, device: { type: 'integer' } }, ['track']),
    tool('get_device_params', 'Read a device\'s parameters (name, value, min, max, display, options).', {
      track: TRACK, device: { type: 'integer', minimum: 0, description: 'device number on the track, from 0' },
      filter: { type: 'string', description: 'only parameters whose name contains this' }
    }, ['track', 'device']),
    tool('set_device_params', 'Set device parameters by name (or index). Use value for numbers, option for switches/menus.', {
      track: TRACK, device: { type: 'integer', minimum: 0 },
      params: { type: 'array', items: { type: 'object', properties: { name: { type: 'string' }, index: { type: 'integer' }, value: { type: 'number' }, option: { type: 'string' } } } }
    }, ['track', 'device', 'params']),
    tool('fire_clip', 'Launch a clip.', { track: TRACK, slot: SLOT }, ['track', 'slot']),
    tool('fire_scene', 'Launch a scene.', { scene: { type: 'integer', minimum: 0 } }, ['scene']),
    tool('play', 'Start Live\'s transport.'),
    tool('stop', 'Stop Live\'s transport.'),
    tool('stop_all_clips', 'Stop all playing clips.'),
    tool('arrange_clip', 'Copy a Session clip into the Arrangement at a bar (bar 1 is the start).', { track: TRACK, slot: SLOT, bar: { type: 'number', minimum: 1 } }, ['track', 'slot', 'bar']),
    tool('show_view', 'Switch Live to the Session or Arrangement view.', { view: { type: 'string', enum: ['session', 'arrangement'] } }, ['view'])
  ];
  // cache the long, unchanging part of every request
  TOOLS[TOOLS.length - 1].cache_control = { type: 'ephemeral' };

  // ------------------------------------------------------------------ plain-English labels for the activity list
  function q(s) { return '“' + String(s) + '”'; }
  function bars(beats) { var b = beats / 4; return (b === Math.round(b) ? b : b.toFixed(2)) + (b === 1 ? ' bar' : ' bars'); }
  function describe(name, input, result) {
    var a = input || {}, r = (result && result.result) || {};
    switch (name) {
      case 'get_session': return 'Looked at your set';
      case 'get_track': return 'Looked at track ' + (a.track + 1);
      case 'set_tempo': return 'Tempo → ' + a.bpm + ' BPM';
      case 'set_time_signature': return 'Time signature → ' + a.numerator + '/' + a.denominator;
      case 'create_track': return 'New ' + (a.type === 'audio' ? 'audio' : 'MIDI') + ' track ' + q(a.name || 'Track') + (r.index != null ? ' (track ' + (r.index + 1) + ')' : '');
      case 'set_track': return 'Updated track ' + (a.track + 1) + (a.name ? ' ' + q(a.name) : '');
      case 'select_track': return 'Selected track ' + (a.track + 1);
      case 'create_scene': return 'New scene' + (a.name ? ' ' + q(a.name) : '');
      case 'set_scene': return 'Named scene ' + (a.scene + 1) + ' ' + q(a.name);
      case 'write_clip': {
        var n = (a.notes || []).length;
        return 'Wrote ' + (a.name ? q(a.name) : 'a clip') + ': ' + bars(a.length || 4) + ', ' + n + (n === 1 ? ' note' : ' notes') + ' on track ' + (a.track + 1) + ', slot ' + (a.slot + 1);
      }
      case 'get_clip_notes': return 'Read the clip on track ' + (a.track + 1) + ', slot ' + (a.slot + 1);
      case 'delete_clip': return 'Deleted the clip on track ' + (a.track + 1) + ', slot ' + (a.slot + 1);
      case 'search_browser': return 'Searched the browser for ' + q(a.query);
      case 'list_browser': return 'Looked in ' + (a.path || 'the browser');
      case 'load_device': return 'Loaded ' + q(r.loaded || a.name || (a.path || a.uri || 'a device').split('/').pop()) + ' on track ' + (a.track + 1);
      case 'get_drum_pads': return 'Checked the drum pads on track ' + (a.track + 1);
      case 'get_device_params': return 'Read the settings of device ' + (a.device + 1) + ' on track ' + (a.track + 1);
      case 'set_device_params': {
        var parts = (r.results || []).filter(function (x) { return x.ok; }).map(function (x) { return x.param + (x.display ? ' ' + x.display : ''); });
        return 'Set ' + (r.name || 'device') + (parts.length ? ': ' + parts.slice(0, 4).join(', ') + (parts.length > 4 ? '…' : '') : '');
      }
      case 'fire_clip': return 'Launched track ' + (a.track + 1) + ', slot ' + (a.slot + 1);
      case 'fire_scene': return 'Launched scene ' + (a.scene + 1);
      case 'play': return 'Started playback';
      case 'stop': return 'Stopped playback';
      case 'stop_all_clips': return 'Stopped all clips';
      case 'arrange_clip': return 'Placed track ' + (a.track + 1) + ', slot ' + (a.slot + 1) + ' in the Arrangement at bar ' + a.bar;
      case 'show_view': return 'Switched to ' + a.view + ' view';
      default: return name;
    }
  }
  // tools that only read: shown quietly in the activity list
  var QUIET = { get_session: 1, get_track: 1, get_clip_notes: 1, search_browser: 1, list_browser: 1, get_drum_pads: 1, get_device_params: 1 };

  root.AbletonAiAgent = { MODELS: MODELS, SYSTEM: SYSTEM, TOOLS: TOOLS, describe: describe, QUIET: QUIET };
})(this);
