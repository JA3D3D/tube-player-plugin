// Render the AbletonAi icon: node render_icon.js (from this folder)
const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const svg = fs.readFileSync(__dirname + '/icon.svg', 'utf8');
  const inner = svg.replace(/^<svg[^>]*>/, '').replace(/<\/svg>\s*$/, '');
  const mac = `<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024"><defs><filter id="sh" x="-20%" y="-20%" width="140%" height="140%"><feDropShadow dx="0" dy="10" stdDeviation="14" flood-opacity=".35"/></filter></defs><g filter="url(#sh)"><svg x="100" y="100" width="824" height="824" viewBox="0 0 240 240">${inner}</svg></g></svg>`;
  const b = await chromium.launch(); const p = await b.newPage();
  for (const [name, content, size] of [['icon-1024.png', svg, 1024], ['icon-256.png', svg, 256], ['icon-macos-1024.png', mac, 1024]]) {
    await p.setViewportSize({ width: size, height: size });
    await p.setContent(`<html><body style="margin:0;background:transparent">${content.replace(/width="1024" height="1024"/, `width="${size}" height="${size}"`)}</body></html>`);
    await p.screenshot({ path: __dirname + '/' + name, omitBackground: true, clip: { x: 0, y: 0, width: size, height: size } });
  }
  await b.close(); console.log('icons rendered');
})();
