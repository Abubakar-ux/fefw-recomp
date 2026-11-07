import { getVersion } from '@tauri-apps/api/app';
import { icon } from '../util.js';

export async function render(el) {
  let version = '';
  try {
    version = await getVersion();
  } catch {
    version = 'dev';
  }
  el.innerHTML = `
    <div class="page">
      <div class="hero">
        <h1>About</h1>
        <p class="dim">Launcher for fefw-recomp, a static recompilation of Fire Emblem: Fortune's Weave (Nintendo Switch 2) for Windows.</p>
      </div>
      <div class="card">
        <h2>${icon('info')}fefw recomp launcher</h2>
        <dl class="kv">
          <dt>Version</dt><dd>${version}</dd>
          <dt>Licence</dt><dd>GPL-3.0-or-later</dd>
          <dt>Built with</dt><dd>Tauri 2, Lucide icons (ISC)</dd>
          <dt>Credits</dt><dd>See README.md: nxrecomp, pokopia-recomp and the other projects this builds on.</dd>
        </dl>
      </div>
      <div class="banner warn">
        ${icon('triangle-alert')}
        <div><strong>No game files included.</strong> You need your own legally dumped copy of the game. This project contains no game code or data and is not affiliated with or endorsed by Nintendo or Intelligent Systems. Fire Emblem and Nintendo Switch 2 are trademarks of Nintendo; they are used here only to say which game this works with.</div>
      </div>
    </div>`;
}
