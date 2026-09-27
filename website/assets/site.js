(() => {
  const repository = 'Vvoidddd/Axiom';
  const developmentRaw = `https://raw.githubusercontent.com/${repository}/daily`;
  const api = `https://api.github.com/repos/${repository}`;
  const refreshKey = Math.floor(Date.now() / 300000);
  const byId = (id) => document.getElementById(id);
  const setText = (id, value) => { const node = byId(id); if (node) node.textContent = value; };
  const fetchText = async (url) => { const response = await fetch(url, { cache: 'no-store' }); if (!response.ok) throw new Error(`${response.status} ${url}`); return response.text(); };
  const fetchJson = async (url) => { const response = await fetch(url, { headers: { Accept: 'application/vnd.github+json' }, cache: 'no-store' }); if (!response.ok) throw new Error(`${response.status} ${url}`); return response.json(); };

  function parseRoadmap(markdown) {
    const lines = markdown.split(/\r?\n/), phases = [];
    let current = null, allDone = 0, allTasks = 0;
    for (const line of lines) {
      const heading = line.match(/^##\s+(Phase\s+([^:—]+)(?::\s*)?[^—]*?)(?:\s+—\s+(Completed))?\s*$/i);
      if (heading) {
        current = { title: heading[1].trim(), number: heading[2].trim(), completedLabel: Boolean(heading[3]), done: 0, total: 0 };
        phases.push(current);
        continue;
      }
      const task = line.match(/^- \[([ xX])\]\s+(.+)/);
      if (!task) continue;
      const done = task[1].toLowerCase() === 'x';
      allTasks++;
      if (done) allDone++;
      if (current) { current.total++; if (done) current.done++; }
    }
    return { phases: phases.filter((phase) => phase.total > 0 || phase.completedLabel), done: allDone, total: allTasks };
  }

  function renderRoadmap(markdown) {
    const data = parseRoadmap(markdown), list = byId('roadmap-list');
    if (!list || !data.phases.length) throw new Error('No roadmap phases found');
    list.replaceChildren();
    let activeAssigned = false;
    data.phases.forEach((phase) => {
      const complete = phase.completedLabel || (phase.total > 0 && phase.done === phase.total);
      const active = !complete && !activeAssigned;
      if (active) activeAssigned = true;
      const row = document.createElement('article');
      row.className = `roadmap-row${complete ? ' complete' : ''}${active ? ' active' : ''}`;
      const number = document.createElement('code'); number.textContent = phase.number.toUpperCase();
      const copy = document.createElement('div'), title = document.createElement('h3'), detail = document.createElement('p');
      title.textContent = phase.title.replace(/\s+—\s+Completed$/i, '');
      detail.textContent = phase.total ? `${phase.done} of ${phase.total} tracked tasks complete` : 'Completion recorded in project log';
      copy.append(title, detail);
      const progress = document.createElement('div'), value = document.createElement('strong'), state = document.createElement('small');
      progress.className = 'phase-progress';
      value.textContent = `${phase.total ? Math.round((phase.done / phase.total) * 100) : (complete ? 100 : 0)}%`;
      state.textContent = complete ? 'complete' : active ? 'active' : 'planned';
      progress.append(value, state); row.append(number, copy, progress); list.append(row);
    });
    const percent = data.total ? Math.round((data.done / data.total) * 100) : 0;
    setText('roadmap-percent', `${percent}%`);
    setText('roadmap-count', `${data.done} / ${data.total} tasks complete`);
    byId('roadmap-progress').style.width = `${percent}%`;
    setText('roadmap-source', `Source: daily/TODO.md · refreshed ${new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}`);
  }

  async function syncVersion() {
    const source = await fetchText(`${developmentRaw}/src/initramfs.c?v=${refreshKey}`);
    const match = source.match(/Axiom\s+([0-9]+(?:\.[0-9]+){1,2})\s+x86_64/);
    if (!match) throw new Error('Kernel version not found');
    setText('kernel-version', match[1]);
  }
  async function syncRelease() {
    const release = await fetchJson(`${api}/releases/latest`);
    setText('release-version', release.tag_name || 'unversioned');
    const iso = Array.isArray(release.assets) && release.assets.find((asset) => /\.iso$/i.test(asset.name));
    const link = byId('iso-download');
    if (link && iso) { link.href = iso.browser_download_url; link.textContent = `Download ${iso.name}`; }
  }
  async function syncCommit() {
    const commit = await fetchJson(`${api}/commits/daily`), date = new Date(commit.commit.author.date);
    setText('commit-sha', commit.sha.slice(0, 7));
    setText('commit-message', commit.commit.message.split('\n')[0]);
    setText('commit-date', `${date.toLocaleDateString(undefined, { year: 'numeric', month: 'short', day: 'numeric' })} · ${commit.commit.author.name}`);
    const link = byId('commit-link'); if (link) link.href = commit.html_url;
  }
  async function syncRoadmap() { renderRoadmap(await fetchText(`${developmentRaw}/TODO.md?v=${refreshKey}`)); }
  async function syncProject() {
    const results = await Promise.allSettled([syncVersion(), syncRelease(), syncCommit(), syncRoadmap()]);
    const failures = results.filter((result) => result.status === 'rejected').length, state = byId('sync-state');
    if (!state) return;
    if (!failures) { state.textContent = 'Synced from GitHub · daily source + stable release'; state.className = 'sync-state ok'; }
    else {
      state.textContent = `${results.length - failures}/${results.length} live sources available · links still work`; state.className = 'sync-state partial';
      if (results[0].status === 'rejected') setText('kernel-version', 'daily branch');
      if (results[1].status === 'rejected') setText('release-version', 'latest release');
      if (results[2].status === 'rejected') { setText('commit-sha', 'daily'); setText('commit-message', 'Open commit history on GitHub'); }
      if (results[3].status === 'rejected') byId('roadmap-list').innerHTML = '<div class="roadmap-loading">Live roadmap unavailable. Open daily/TODO.md on GitHub for the current plan.</div>';
    }
  }

  const copyButton = document.querySelector('[data-copy]');
  if (copyButton && navigator.clipboard) copyButton.addEventListener('click', async () => {
    const original = copyButton.textContent;
    try { await navigator.clipboard.writeText(copyButton.dataset.copy); copyButton.textContent = 'copied'; }
    catch { copyButton.textContent = 'select text'; }
    window.setTimeout(() => { copyButton.textContent = original; }, 1500);
  });
  syncProject();
})();
