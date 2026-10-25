// Render the logo SVGs to PNGs (transparent backgrounds). usage: node brand/render_logo.js brand/out
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
const OUT = path.resolve(process.argv[2] || 'brand/out');
const svgDir = path.join(OUT, 'svg'), pngDir = path.join(OUT, 'png');
fs.mkdirSync(pngDir, { recursive: true });

const jobs = [
  ['icon.svg', 'icon-1024.png', 1024, 1024], ['icon.svg', 'icon-512.png', 512, 512], ['icon.svg', 'icon-256.png', 256, 256],
  ['icon.svg', 'icon-128.png', 128, 128], ['icon-64.svg', 'icon-64.png', 64, 64], ['icon-32.svg', 'icon-32.png', 32, 32],
  ['icon-16.svg', 'icon-16.png', 16, 16], ['icon-macos.svg', 'icon-macos-1024.png', 1024, 1024],
  ['mark-white.svg', 'mark-white-512.png', 512, 512], ['mark-black.svg', 'mark-black-512.png', 512, 512],
  ['mark-pink.svg', 'mark-pink-512.png', 512, 512],
  ['logo-dark.svg', 'logo-dark.png', null, 256], ['logo-light.svg', 'logo-light.png', null, 256],
  ['wordmark-dark.svg', 'wordmark-dark.png', null, 160],
];

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  for (const [src, dst, w0, h] of jobs) {
    const text = fs.readFileSync(path.join(svgDir, src), 'utf8');
    const m = /width="([\d.]+)" height="([\d.]+)"/.exec(text);
    const w = w0 || Math.round(h * Number(m[1]) / Number(m[2]));
    const sized = text.replace(/width="[\d.]+" height="[\d.]+"/, `width="${w}" height="${h}"`);
    await page.setViewportSize({ width: w, height: h });
    await page.setContent(`<html><body style="margin:0;background:transparent">${sized}</body></html>`);
    await page.screenshot({ path: path.join(pngDir, dst), omitBackground: true, clip: { x: 0, y: 0, width: w, height: h } });
  }
  await browser.close();
  console.log('rendered', jobs.length, 'PNGs to', pngDir);
})();
