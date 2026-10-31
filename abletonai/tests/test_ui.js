// AbletonAi's screen in Chromium with stand-ins for the plugin (window.__mockNative), for Live
// (a tiny fake set behind the helper calls) and for Claude's API (scripted streaming replies).
// usage: node abletonai/tests/test_ui.js [shotsDir]
const { chromium } = require('playwright');
const http = require('http'), fs = require('fs'), path = require('path');
const RES = path.resolve(__dirname, '..', 'Resources');
const SHOTS = path.resolve(process.argv[2] || path.join(__dirname, '..', 'build-shots'));
fs.mkdirSync(SHOTS, { recursive: true });
let fails = 0;
const ok = (c, m) => { if (!c) { fails++; console.log('FAIL', m); } else console.log('ok  ', m); };
const sleep = ms => new Promise(r => setTimeout(r, ms));

// ---------------------------------------------------------------- scripted Claude replies as SSE
function sse(blocks, stop = 'end_turn', usage = { input_tokens: 1200, output_tokens: 300, cache_read_input_tokens: 900 }) {
  const ev = [];
  const push = (type, data) => ev.push('event: ' + type + '\ndata: ' + JSON.stringify(Object.assign({ type }, data)) + '\n\n');
  push('message_start', { message: { id: 'msg', type: 'message', role: 'assistant', content: [], model: 'm', usage: { input_tokens: usage.input_tokens, output_tokens: 1, cache_read_input_tokens: usage.cache_read_input_tokens } } });
  push('ping', {});
  blocks.forEach((b, i) => {
    if (b.text !== undefined) {
      push('content_block_start', { index: i, content_block: { type: 'text', text: '' } });
      for (const part of b.text.match(/.{1,7}/gs) || []) push('content_block_delta', { index: i, delta: { type: 'text_delta', text: part } });
    } else {
      push('content_block_start', { index: i, content_block: { type: 'tool_use', id: b.id, name: b.name, input: {} } });
      const js = JSON.stringify(b.input);
      for (let k = 0; k < js.length; k += 20) push('content_block_delta', { index: i, delta: { type: 'input_json_delta', partial_json: js.slice(k, k + 20) } });
    }
    push('content_block_stop', { index: i });
  });
  push('message_delta', { delta: { stop_reason: stop }, usage: { output_tokens: usage.output_tokens } });
  push('message_stop', {});
  return ev.join('');
}

const DRUMS = [{ pitch: 36, start: 0, duration: 0.25, velocity: 120 }, { pitch: 38, start: 2, duration: 0.25, velocity: 110 },
  { pitch: 42, start: 0.5, duration: 0.25, velocity: 70 }, { pitch: 36, start: 3.5, duration: 0.25, velocity: 90 }];
const BASS = Array.from({ length: 16 }, (_, i) => ({ pitch: 29 + (i % 4 === 3 ? 12 : 0), start: i * 0.5, duration: 0.4, velocity: 100 + (i % 3) * 8 }));

(async () => {
  const srv = http.createServer((q, r) => {
    const f = q.url.startsWith('/agent.js') ? 'agent.js' : 'ui.html';
    r.writeHead(200, { 'Content-Type': f.endsWith('.js') ? 'text/javascript' : 'text/html; charset=utf-8' });
    r.end(fs.readFileSync(path.join(RES, f)));
  });
  await new Promise(r => srv.listen(0, '127.0.0.1', r));
  const url = 'http://127.0.0.1:' + srv.address().port + '/page';
  const browser = await chromium.launch();
  const errors = [];

  async function open(opts = {}) {
    const ctx = await browser.newContext({ viewport: opts.viewport || { width: 1000, height: 720 } });
    const api = { calls: [], replies: (opts.replies || []).slice(), status: opts.apiStatus || 200 };
    await ctx.route('https://api.anthropic.com/**', async route => {
      const req = route.request();
      const cors = { 'access-control-allow-origin': '*', 'access-control-allow-headers': '*', 'access-control-allow-methods': 'POST' };
      if (req.method() === 'OPTIONS') return route.fulfill({ status: 204, headers: cors });
      api.calls.push({ headers: req.headers(), body: JSON.parse(req.postData()) });
      if (api.status !== 200) return route.fulfill({ status: api.status, headers: cors, contentType: 'application/json', body: JSON.stringify({ type: 'error', error: { type: 'authentication_error', message: 'invalid x-api-key' } }) });
      const next = api.replies.shift() || sse([{ text: 'Done.' }]);
      if (opts.slow) await sleep(opts.slow);
      return route.fulfill({ status: 200, headers: cors, contentType: 'text/event-stream', body: next });
    });
    const page = await ctx.newPage();
    page.on('pageerror', e => errors.push(e.message));
    await page.addInitScript(() => {
      // a tiny Live set behind the helper
      const live = { tempo: 120, tracks: [{ name: '1-MIDI', type: 'midi', clips: {} }], selected: 0 };
      window.__live = live; window.__sent = []; window.__helperCalls = [];
      window.__helperOn = true;
      window.__mockNative = m => {
        window.__sent.push(m);
        if (m.type !== 'helper') return;
        window.__helperCalls.push(m);
        let res;
        if (!window.__helperOn) res = { ok: false, error: 'The Live helper is not connected' };
        else if (m.cmd === 'get_session') res = { ok: true, result: { tempo: live.tempo, tracks: live.tracks.map((t, i) => ({ index: i, name: t.name, type: t.type })), selected_track: live.selected } };
        else if (m.cmd === 'set_tempo') { live.tempo = m.args.bpm; res = { ok: true, result: { tempo: live.tempo } }; }
        else if (m.cmd === 'create_track') { live.tracks.push({ name: m.args.name, type: m.args.type, clips: {} }); res = { ok: true, result: { index: live.tracks.length - 1, name: m.args.name } }; }
        else if (m.cmd === 'write_clip') {
          const t = live.tracks[m.args.track];
          if (!t || t.type !== 'midi') res = { ok: false, error: 'track ' + m.args.track + ' is not a MIDI track' };
          else { t.clips[m.args.slot] = m.args; res = { ok: true, result: { track: m.args.track, slot: m.args.slot, notes: m.args.notes.length } }; }
        } else if (m.cmd === 'load_device') res = { ok: true, result: { loaded: 'Wavetable', devices: ['Wavetable'] } };
        else if (m.cmd === 'set_device_params') res = { ok: true, result: { name: 'Echo', results: [{ param: 'Dry/Wet', ok: true, display: '25 %' }, { param: 'Feedback', ok: true, display: '60 %' }] } };
        else res = { ok: false, error: 'unknown command: ' + m.cmd };
        setTimeout(() => AI.recv(Object.assign({ type: 'helperResult', reqId: m.reqId }, res)), 20);
      };
    });
    await page.goto(url);
    await page.waitForFunction(() => window.__sent.some(m => m.type === 'ready'));
    if (opts.init !== false) await page.evaluate(i => AI.recv(i), Object.assign({ type: 'init', settings: { apiKey: 'sk-ant-test-123', model: 'balanced' }, helper: { connected: true, live: '11.3.35' }, chat: {} }, opts.init || {}));
    return { page, api, ctx };
  }
  const sentOf = (page, type) => page.evaluate(t => window.__sent.filter(m => m.type === t), type);

  // ---------------------------------------------------------------- first run: setup cards
  let { page, ctx } = await open({ init: { settings: {}, helper: { connected: false } } });
  ok(await page.isVisible('#cardKey') && !(await page.getAttribute('#cardKey', 'class')).includes('done'), 'first run: asks for an API key');
  ok((await page.textContent('#helperText')) === 'Live helper off' && (await page.getAttribute('#helperPill', 'class')).includes('off'), 'helper shown as off');
  ok((await page.$$('.chip')).length === 5, 'example prompts shown');
  await page.screenshot({ path: path.join(SHOTS, 'first-run.png') });
  await page.click('#cardHelperBtn');
  ok(await page.isVisible('#helperHelp') && /Control Surface/.test(await page.textContent('#helperHelp')), 'helper help explains the Control Surface step');
  await page.screenshot({ path: path.join(SHOTS, 'helper-help.png') });
  await page.click('#helpClose');
  await page.fill('#input', 'make a bassline'); await page.press('#input', 'Enter');
  ok(await page.isVisible('#settings'), 'sending without a key opens Settings');
  await page.fill('#apiKey', 'hello'); await page.click('#setSave');
  ok(/doesn.t look like a Claude API key/.test(await page.textContent('#toast')), 'a non-key is refused');
  await page.fill('#apiKey', 'sk-ant-abc'); await page.selectOption('#modelDefault', 'best'); await page.click('#setSave');
  const saved = (await sentOf(page, 'saveSettings')).pop();
  ok(saved.apiKey === 'sk-ant-abc' && saved.model === 'best', 'key and model saved through the plugin');
  ok((await page.getAttribute('#cardKey', 'class')).includes('done') && (await page.inputValue('#model')) === 'best', 'setup card ticks off');
  await page.evaluate(() => AI.recv({ type: 'status', helper: { connected: true, live: '11.3.35' } }));
  ok((await page.textContent('#helperText')) === 'Live 11.3.35 connected', 'helper turns green when Live connects');
  await ctx.close();

  // ---------------------------------------------------------------- a full request: Claude uses tools in Live
  let api;
  const run = await open({ replies: [
    sse([{ text: 'On it. ' }, { id: 't1', name: 'get_session', input: {} }], 'tool_use'),
    sse([{ id: 't2', name: 'set_tempo', input: { bpm: 150 } }, { id: 't3', name: 'create_track', input: { type: 'midi', name: 'Drums', color: 'red' } },
      { id: 't4', name: 'create_track', input: { type: 'midi', name: 'Bass', color: 'pink' } }], 'tool_use'),
    sse([{ id: 't5', name: 'write_clip', input: { track: 1, slot: 0, length: 4, name: 'Riddim drums', notes: DRUMS } },
      { id: 't6', name: 'write_clip', input: { track: 2, slot: 0, length: 8, name: 'Riddim bass', notes: BASS } },
      { id: 't7', name: 'write_clip', input: { track: 5, slot: 9, length: 4, notes: [{ pitch: 60, start: 0, duration: 1 }] } }], 'tool_use'),
    sse([{ text: 'Made a **150 BPM** riddim groove:\n- Drums on track 2\n- Bass on track 3\n\nLaunch scene 1 to hear it.' }]),
  ] });
  page = run.page; api = run.api; ctx = run.ctx;
  await page.click('.chip >> nth=0');
  ok((await page.inputValue('#input')).startsWith('Riddim bassline'), 'an example prompt fills the box');
  await page.fill('#input', 'make a riddim groove at 150');
  await page.press('#input', 'Enter');
  await page.waitForFunction(() => !document.getElementById('send').hidden, null, { timeout: 15000 });
  const c0 = api.calls[0];
  ok(c0.headers['x-api-key'] === 'sk-ant-test-123' && c0.headers['anthropic-version'] === '2023-06-01' && c0.headers['anthropic-dangerous-direct-browser-access'] === 'true', 'calls Claude with the key and the right headers');
  ok(c0.body.model === 'claude-sonnet-5-5' && c0.body.stream === true && c0.body.max_tokens >= 8000, 'Balanced = Claude Sonnet, streaming');
  ok(c0.body.tools.length >= 20 && c0.body.tools.some(t => t.name === 'write_clip') && c0.body.tools[c0.body.tools.length - 1].cache_control, 'sends Live tools, cached');
  ok(c0.body.system[0].cache_control && /Ableton calls MIDI 60 "C3"/.test(c0.body.system[0].text), 'system prompt with production rules, cached');
  ok(c0.body.messages.length === 1 && c0.body.messages[0].content === 'make a riddim groove at 150', 'first call carries the request');
  ok(api.calls.length === 4, 'four rounds: look, set up, write, answer (' + api.calls.length + ')');
  const c1 = api.calls[1].body.messages;
  const tr = c1[c1.length - 1].content[0];
  ok(tr.type === 'tool_result' && tr.tool_use_id === 't1' && JSON.parse(tr.content).tempo === 120, 'Live\'s answer goes back to Claude as a tool result');
  const c3 = api.calls[3].body.messages, bad = c3[c3.length - 1].content.find(x => x.tool_use_id === 't7');
  ok(bad.is_error === true && /not a MIDI track/.test(bad.content), 'a failed action is reported to Claude as an error');
  const calls = await page.evaluate(() => window.__helperCalls.map(c => c.cmd));
  ok(calls.join(',') === 'get_session,set_tempo,create_track,create_track,write_clip,write_clip,write_clip', 'actions run in Live in order: ' + calls.join(','));
  const live = await page.evaluate(() => window.__live);
  ok(live.tempo === 150 && live.tracks.length === 3 && live.tracks[2].clips[0].notes.length === 16, 'the fake set now has the tempo, tracks and clips');
  const labels = await page.$$eval('.act .label', els => els.map(e => e.textContent));
  ok(labels.includes('Tempo → 150 BPM') && labels.some(l => /New MIDI track “Bass”/.test(l)) && labels.some(l => /Wrote “Riddim bass”: 2 bars, 16 notes on track 3, slot 1/.test(l)), 'activity list in plain words');
  ok((await page.$$('.act canvas')).length === 2, 'piano-roll previews for the written clips');
  ok((await page.$$('.act .ic.err')).length === 1 && /not a MIDI track/.test(await page.textContent('.act div.err')), 'the failed action is marked');
  const txt = await page.textContent('.msg.ai .body');
  ok(/On it\./.test(txt) && /Launch scene 1/.test(txt) && (await page.innerHTML('.msg.ai')).includes('<strong>150 BPM</strong>'), 'Claude\'s text streamed in, with formatting');
  ok(/Last reply: .*in .*cached.*out/.test(await page.textContent('#usage')), 'token use shown');
  const chat = (await sentOf(page, 'saveChat')).pop().chat;
  ok(chat.history.length === 2 && /Changes made in Live: .*Tempo/.test(chat.history[1].content) && chat.transcript.length === 2, 'conversation saved with the Live set, compactly');
  await page.screenshot({ path: path.join(SHOTS, 'conversation.png') });

  // follow-up message carries the short history
  api.replies.push(sse([{ text: 'Sure.' }]));
  await page.fill('#input', 'make it darker'); await page.press('#input', 'Enter');
  await page.waitForFunction(() => document.querySelectorAll('.msg.ai').length === 2 && !document.getElementById('send').hidden);
  const m5 = api.calls[api.calls.length - 1].body.messages;
  ok(m5.length === 3 && m5[0].content === 'make a riddim groove at 150' && typeof m5[1].content === 'string' && m5[2].content === 'make it darker', 'follow-ups send the short history, not every tool step');
  await ctx.close();

  // ---------------------------------------------------------------- reopening a set restores the chat
  ({ page, ctx } = await open({ init: { chat: chat } }));
  ok((await page.$$('.msg.user')).length === 1 && (await page.$$('.msg.ai .act')).length >= 5 && await page.isHidden('#welcome'), 'a saved conversation comes back when the set reopens');
  await page.click('#newChat');
  ok((await page.$$('.msg')).length === 0 && await page.isVisible('#welcome') && (await sentOf(page, 'saveChat')).pop().chat.transcript.length === 0, 'New chat clears it');
  await ctx.close();

  // ---------------------------------------------------------------- errors and stopping
  ({ page, ctx } = await open({ apiStatus: 401 }));
  await page.fill('#input', 'hi'); await page.press('#input', 'Enter');
  await page.waitForSelector('.banner');
  ok(/API key was rejected/.test(await page.textContent('.banner')), 'a bad key is explained');
  await page.click('.banner [data-open=settings]');
  ok(await page.isVisible('#settings'), 'and Settings is one click away');
  await ctx.close();

  ({ page, ctx } = await open({ replies: [sse([{ id: 'x1', name: 'get_session', input: {} }], 'tool_use'), sse([{ text: 'The helper is off: switch it on in Live\'s settings.' }])] }));
  await page.evaluate(() => { window.__helperOn = false; });
  await page.fill('#input', 'make a bassline'); await page.press('#input', 'Enter');
  await page.waitForFunction(() => document.querySelectorAll('.msg.ai .act .ic.err').length === 1 && !document.getElementById('send').hidden);
  ok(true, 'with the helper off, the action fails and Claude explains');
  await ctx.close();

  const slow = await open({ slow: 1500, replies: [sse([{ text: 'This will take a while' }])] });
  await slow.page.fill('#input', 'write a whole album'); await slow.page.press('#input', 'Enter');
  await sleep(300);
  ok(await slow.page.isVisible('#stop') && await slow.page.isHidden('#send'), 'Stop shows while working');
  await slow.page.click('#stop');
  await slow.page.waitForFunction(() => !document.getElementById('send').hidden);
  ok(/Stopped/.test(await slow.page.textContent('.msg.ai')), 'Stop ends the request');
  await slow.ctx.close();

  // ---------------------------------------------------------------- window sizes
  for (const [w, h] of [[560, 380], [1000, 720], [1600, 1000]]) {
    const o = await open({ viewport: { width: w, height: h }, init: { settings: {}, helper: { connected: false } } });
    const m = await o.page.evaluate(() => ({ sw: document.documentElement.scrollWidth, sh: document.documentElement.scrollHeight, comp: document.querySelector('.composer').getBoundingClientRect().bottom }));
    ok(m.sw <= w && m.sh <= h && m.comp <= h + 1, w + 'x' + h + ': fits, message box visible');
    await o.page.screenshot({ path: path.join(SHOTS, 'size-' + w + 'x' + h + '.png') });
    await o.ctx.close();
  }

  ok(errors.length === 0, 'no script errors' + (errors.length ? ': ' + errors.join(' | ') : ''));
  await browser.close(); srv.close();
  console.log(fails ? fails + ' FAILURES' : 'ALL UI TESTS PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('FAIL exception', e); process.exit(1); });
