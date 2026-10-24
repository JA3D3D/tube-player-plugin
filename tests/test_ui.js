// The plugin's page (Resources/ui.html) in Chromium, with a stand-in for the plugin (window.__mockNative)
// and for YouTube's player. usage: node tests/test_ui.js [shotsDir]
const { chromium } = require('playwright');
const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.resolve(__dirname, '..');
const SHOTS = path.resolve(process.argv[2] || path.join(ROOT, 'build-shots'));
fs.mkdirSync(SHOTS, { recursive: true });
const HTML = fs.readFileSync(path.join(ROOT, 'Resources/ui.html'));
const FAKE_YT = fs.readFileSync(path.join(__dirname, 'fake_yt.js'), 'utf8');
let fails = 0;
const ok = (c, m) => { if (!c) { fails++; console.log('FAIL', m); } else console.log('ok  ', m); };
const sleep = ms => new Promise(r => setTimeout(r, ms));

(async () => {
  const srv = http.createServer((q, r) => { r.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' }); r.end(HTML); });
  await new Promise(r => srv.listen(0, '127.0.0.1', r));
  const url = 'http://127.0.0.1:' + srv.address().port + '/tok123';
  const browser = await chromium.launch();
  const ctx = await browser.newContext({ viewport: { width: 980, height: 620 } });
  await ctx.route('https://www.youtube.com/iframe_api', r => r.fulfill({ status: 200, contentType: 'text/javascript', body: FAKE_YT }));
  const errors = [];

  async function open(initMsg, opts = {}) {
    const page = await ctx.newPage();
    page.on('pageerror', e => errors.push(e.message));
    if (opts.viewport) await page.setViewportSize(opts.viewport);
    await page.addInitScript((clip) => {
      window.__sent = [];
      window.__clip = clip;
      window.__mockNative = m => {
        window.__sent.push(m);
        if (m.type === 'paste') setTimeout(() => TP.recv({ type: 'clipboard', text: window.__clip, load: m.load }), 10);
      };
    }, opts.clipboard || '');
    await page.goto(url);
    await page.waitForFunction(() => window.__sent.some(m => m.type === 'ready'));
    if (initMsg) await page.evaluate(m => TP.recv(m), initMsg);
    return page;
  }
  const sent = (page, type) => page.evaluate(t => window.__sent.filter(m => m.type === t), type);
  const lastSent = async (page, type) => (await sent(page, type)).pop();
  const yt = (page, expr) => page.evaluate(new Function('return (' + expr + ')'));
  const baseInit = { type: 'init', params: { play: false, volume: 100, speed: 1, loop: false, follow: false }, state: {}, recent: [], host: { playing: false } };

  // ---------------------------------------------------------------- empty plugin
  let p = await open(baseInit);
  ok(await p.isVisible('#empty') && /Paste a YouTube link/.test(await p.textContent('#empty')), 'new plugin: says to paste a link');
  await p.screenshot({ path: path.join(SHOTS, 'empty.png') });

  // Paste & Load reads the clipboard through the plugin
  await p.evaluate(() => { window.__clip = 'https://www.youtube.com/watch?v=dQw4w9WgXcQ&t=1m5s'; });
  await p.click('#pasteLoad');
  await p.waitForFunction(() => window.__yt && window.__yt.state === 1, null, { timeout: 4000 });
  const opened = await lastSent(p, 'opened');
  ok(opened && opened.id === 'dQw4w9WgXcQ' && opened.start === 65, 'Paste & Load: opens the clipboard link at 1:05 and tells the plugin');
  ok((await yt(p, '__yt.o.playerVars.origin')) === new URL(url).origin, 'player told its page address (avoids Error 153)');
  await sleep(400);
  const st = await lastSent(p, 'status');
  ok(st && st.state === 'playing' && st.t >= 65 && st.dur === 212 && st.title === 'How to make a tearout growl in Operator', 'status goes to the plugin: playing, time, length, title');
  ok((await p.textContent('#title')) === 'How to make a tearout growl in Operator', 'title shown');
  ok(await p.evaluate(() => document.getElementById('pp').classList.contains('on')), 'Play button shows pause while playing');

  // transport buttons
  await p.click('#pp'); await sleep(150);
  ok((await yt(p, '__yt.state')) === 2 && (await lastSent(p, 'setParam')).name === 'play' && (await lastSent(p, 'setParam')).value === 0, 'Pause: pauses and sets the Play parameter (so Live can record it)');
  const t0 = await yt(p, '__yt.t');
  await p.click('#fwd'); await sleep(80);
  ok(Math.abs((await yt(p, '__yt.t')) - t0 - 10) < 0.3, '+10 s');
  await p.click('#back'); await p.click('#back'); await sleep(80);
  ok(Math.abs((await yt(p, '__yt.t')) - t0 + 10) < 0.3, '-10 s twice');
  await p.selectOption('#speed', '0.75'); await sleep(80);
  ok((await yt(p, '__yt.rate')) === 0.75 && (await lastSent(p, 'setParam')).name === 'speed' && (await lastSent(p, 'setParam')).value === 0.75, 'speed 0.75x, parameter set');
  await p.evaluate(() => { const v = document.getElementById('vol'); v.value = 35; v.dispatchEvent(new Event('input')); v.dispatchEvent(new Event('change')); });
  ok((await lastSent(p, 'setParam')).name === 'volume' && (await lastSent(p, 'setParam')).value === 35, 'volume slider sets the Volume parameter');

  // loop
  await p.evaluate(() => { __yt.t = 80; }); await p.click('#setA');
  await p.evaluate(() => { __yt.t = 92; }); await p.click('#setB');
  const lp = await lastSent(p, 'loop');
  ok(lp.a === 80 && lp.b === 92 && (await p.getAttribute('#loopBtn', 'aria-pressed')) === 'true', 'Set A / Set B: loop 1:20 to 1:32 switches on');
  ok((await p.textContent('#loopInfo')).replace(/\s+/g, ' ') === 'A 1:20 B 1:32', 'loop points shown');
  await p.click('#pp'); await p.evaluate(() => { __yt.t = 92.5; }); await sleep(300);
  ok((await yt(p, '__yt.t')) < 82, 'loop jumps back to A');
  await p.click('#loopBtn');
  ok((await p.getAttribute('#loopBtn', 'aria-pressed')) === 'false', 'Loop off');
  await p.screenshot({ path: path.join(SHOTS, 'playing.png') });

  // from Live: automation / MIDI mapping
  await p.evaluate(() => TP.recv({ type: 'param', name: 'play', value: false })); await sleep(80);
  ok((await yt(p, '__yt.state')) === 2, 'Play parameter off (automation) pauses');
  await p.evaluate(() => TP.recv({ type: 'param', name: 'speed', value: 1.5 })); await sleep(50);
  ok((await yt(p, '__yt.rate')) === 1.5 && (await p.inputValue('#speed')) === '1.5', 'Speed parameter from Live');
  await p.evaluate(() => TP.recv({ type: 'param', name: 'volume', value: 60 }));
  ok((await p.inputValue('#vol')) === '60', 'Volume parameter from Live');
  const before = (await sent(p, 'setParam')).length;
  await p.evaluate(() => TP.recv({ type: 'param', name: 'loop', value: true })); await sleep(50);
  ok((await p.getAttribute('#loopBtn', 'aria-pressed')) === 'true' && (await sent(p, 'setParam')).length === before, 'Loop parameter from Live (no echo)');

  // Follow Live
  await p.click('#followBtn');
  ok((await lastSent(p, 'setParam')).name === 'follow' && (await lastSent(p, 'setParam')).value === 1, 'Follow Live switched on');
  await p.evaluate(() => TP.recv({ type: 'host', playing: true })); await sleep(80);
  ok((await yt(p, '__yt.state')) === 1, 'Live plays -> video plays');
  await p.evaluate(() => TP.recv({ type: 'host', playing: false })); await sleep(80);
  ok((await yt(p, '__yt.state')) === 2, 'Live stops -> video pauses');

  // links
  await p.fill('#link', 'https://vimeo.com/123'); await p.press('#link', 'Enter'); await sleep(50);
  ok(!(await p.isHidden('#toast')) && /isn't a YouTube link/.test(await p.textContent('#toast')), 'non-YouTube link refused with a message');
  await p.fill('#link', 'youtu.be/9bZkp7q19f0'); await p.press('#link', 'Enter'); await sleep(300);
  ok((await yt(p, '__yt.vid')) === '9bZkp7q19f0' && (await lastSent(p, 'opened')).id === '9bZkp7q19f0', 'typing a link and pressing Return loads it');
  ok((await lastSent(p, 'loop')).a === -1 && (await p.getAttribute('#loopBtn', 'aria-pressed')) === 'false', 'new video clears the loop');

  // recent list, with resume
  await p.evaluate(() => TP.recv({ type: 'recent', items: [{ id: 'dQw4w9WgXcQ', list: '', title: 'How to make a tearout growl in Operator', t: 130, dur: 212 }, { id: '9bZkp7q19f0', list: '', title: 'Riddim drums in 10 minutes', t: 0, dur: 212 }] }));
  ok((await p.textContent('#recentCount')) === '2', 'recent count in the top bar');
  await p.click('#recentBtn');
  ok(await p.isVisible('#recentPop'), 'recent list opens');
  await p.screenshot({ path: path.join(SHOTS, 'recent.png') });
  await p.click('#recentList .item >> nth=0'); await sleep(300);
  ok((await yt(p, '__yt.vid')) === 'dQw4w9WgXcQ' && (await yt(p, '__yt.t')) >= 130 && (await lastSent(p, 'opened')).start === 130, 'picking a recent video carries on where you stopped (2:10)');
  ok(await p.isHidden('#recentPop'), 'list closes');

  // errors
  await p.evaluate(() => __ytError(150)); await sleep(50);
  ok(await p.isVisible('#error') && /only allows this video on YouTube/.test(await p.textContent('#error')), 'embedding blocked: explains and offers Open on YouTube');
  await p.click('#errOpen');
  ok(/^https:\/\/www\.youtube\.com\/watch\?v=dQw4w9WgXcQ/.test((await lastSent(p, 'open')).url), 'Open on YouTube asks the plugin to open the browser');
  await p.screenshot({ path: path.join(SHOTS, 'error.png') });

  // Restart
  await p.click('#restartBtn');
  ok(await p.isVisible('#restartModal'), 'Restart asks first');
  await p.screenshot({ path: path.join(SHOTS, 'restart.png') });
  await p.click('#rCancel');
  ok(await p.isHidden('#restartModal') && !(await lastSent(p, 'restart')), 'Cancel does nothing');
  await p.click('#restartBtn'); await p.check('#forgetRecent'); await p.click('#rGo');
  const rs = await lastSent(p, 'restart');
  ok(rs && rs.forgetRecent === true && await p.isVisible('#busy'), 'Restart sends the request (with forget recent) and shows Restarting');

  // ---------------------------------------------------------------- reopening a saved set
  const p2 = await open({ type: 'init', params: { play: true, volume: 70, speed: 0.5, loop: true, follow: false },
    state: { id: 'dQw4w9WgXcQ', list: '', title: 'How to make a tearout growl in Operator', t: 100, dur: 212, a: 95, b: 105 }, recent: [], host: { playing: false } });
  await p2.waitForFunction(() => window.__yt && window.__yt.state === 1, null, { timeout: 4000 });
  ok((await yt(p2, '__yt.t')) >= 100 && (await yt(p2, '__yt.rate')) === 0.5, 'saved set reopens at 1:40, 0.5x, playing');
  ok((await p2.getAttribute('#loopBtn', 'aria-pressed')) === 'true' && (await p2.inputValue('#vol')) === '70', 'loop and volume restored');
  // Follow Live on, Live stopped: stays paused
  const p3 = await open({ type: 'init', params: { play: true, volume: 100, speed: 1, loop: false, follow: true },
    state: { id: 'dQw4w9WgXcQ', list: '', title: '', t: 20, dur: 212 }, recent: [], host: { playing: false } });
  await sleep(500);
  ok((await yt(p3, '__yt.state')) !== 1, 'with Follow Live on and Live stopped, the video waits');

  // ---------------------------------------------------------------- clipboard without a link
  const p4 = await open(baseInit, { clipboard: 'hello there' });
  await p4.click('#pasteLoad'); await sleep(80);
  ok(/doesn't hold a YouTube link/.test(await p4.textContent('#toast')), 'clipboard without a link: says so');
  await p4.click('#pp');
  ok(/Load a video first/.test(await p4.textContent('#toast')), 'Play with nothing loaded: says so');

  // ---------------------------------------------------------------- window sizes
  for (const [w, h] of [[560, 360], [980, 620], [1600, 1000]]) {
    const q = await open(baseInit, { viewport: { width: w, height: h } });
    await q.evaluate(() => TP.recv({ type: 'recent', items: [] }));
    const m = await q.evaluate(() => {
      const f = document.getElementById('frame').getBoundingClientRect(), s = document.getElementById('stage').getBoundingClientRect();
      return { sw: document.documentElement.scrollWidth, sh: document.documentElement.scrollHeight, fw: f.width, fh: f.height, inside: f.left >= s.left && f.right <= s.right + 1 && f.top >= s.top && f.bottom <= s.bottom + 1 };
    });
    ok(m.sw <= w && m.sh <= h && m.inside && Math.abs(m.fw / m.fh - 16 / 9) < 0.02 && m.fw > 100, w + 'x' + h + ': everything fits, video stays 16:9 (' + Math.round(m.fw) + 'x' + Math.round(m.fh) + ')');
    await q.screenshot({ path: path.join(SHOTS, 'size-' + w + 'x' + h + '.png') });
    await q.close();
  }

  ok(errors.length === 0, 'no script errors' + (errors.length ? ': ' + errors.join(' | ') : ''));
  await browser.close(); srv.close();
  console.log(fails ? fails + ' FAILURES' : 'ALL UI TESTS PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('FAIL exception', e); process.exit(1); });
