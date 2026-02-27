// Geobot Pages site (static)
// - Loads mod.json, about.md, changelog.md from the repo
// - Download Latest Build points to latest GitHub Release asset (prefers .geode)
// - "Latest update preview" shows release notes + latest changelog section

const OWNER = "Benno111";
const REPO  = "Geobot";

const API_REPO = `https://api.github.com/repos/${OWNER}/${REPO}`;
const RAW_BASE = `https://raw.githubusercontent.com/${OWNER}/${REPO}/main`;

const el = (id) => document.getElementById(id);

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({
    "&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#039;"
  }[c]));
}

// Tiny Markdown (headings, lists, paragraphs, inline code + links)
function mdToHtml(md) {
  const lines = String(md).replace(/\r/g, "").split("\n");
  let html = "";
  let inList = false;

  const closeList = () => { if (inList) { html += "</ul>"; inList = false; } };

  for (const raw of lines) {
    const line = raw.trimRight();

    if (!line.trim()) { closeList(); continue; }

    if (line.startsWith("### ")) { closeList(); html += `<h3>${escapeHtml(line.slice(4))}</h3>`; continue; }
    if (line.startsWith("## "))  { closeList(); html += `<h2>${escapeHtml(line.slice(3))}</h2>`; continue; }
    if (line.startsWith("# "))   { closeList(); html += `<h1>${escapeHtml(line.slice(2))}</h1>`; continue; }

    if (line.startsWith("* ") || line.startsWith("- ")) {
      if (!inList) { html += "<ul>"; inList = true; }
      html += `<li>${inlineMd(line.slice(2))}</li>`;
      continue;
    }

    closeList();
    html += `<p>${inlineMd(line)}</p>`;
  }

  closeList();
  return html;
}

function inlineMd(s) {
  let out = escapeHtml(s);
  // inline code
  out = out.replace(/`([^`]+)`/g, "<code>$1</code>");
  // links [text](url)
  out = out.replace(/\[([^\]]+)\]\((https?:\/\/[^\s)]+)\)/g, '<a href="$2" target="_blank" rel="noreferrer">$1</a>');
  return out;
}

async function fetchText(url) {
  const r = await fetch(url, { cache: "no-store" });
  if (!r.ok) throw new Error(`${r.status} ${r.statusText}`);
  return await r.text();
}

async function fetchJson(url) {
  const r = await fetch(url, { cache: "no-store" });
  if (!r.ok) throw new Error(`${r.status} ${r.statusText}`);
  return await r.json();
}

function setHref(id, url) {
  const a = el(id);
  if (a) a.href = url;
}

function setText(id, value) {
  const n = el(id);
  if (n) n.textContent = value;
}

function setHtml(id, value) {
  const n = el(id);
  if (n) n.innerHTML = value;
}

function pickLogo() {
  const img = el("brandLogo");
  if (!img) return;

  const candidates = [
    `${RAW_BASE}/logo.png`,
    `./assets/logo.png`,
  ];

  let i = 0;
  const tryNext = () => {
    if (i >= candidates.length) {
      img.style.display = "none";
      return;
    }
    img.src = candidates[i++];
  };

  img.onerror = tryNext;
  tryNext();
}

function firstMarkdownSection(md) {
  // Grab from first heading down to before next same-or-higher heading
  const text = String(md).replace(/\r/g, "");
  const lines = text.split("\n");

  // Find first heading (# or ##)
  let start = -1;
  let startLevel = 0;
  for (let i = 0; i < lines.length; i++) {
    const m = lines[i].match(/^(#{1,3})\s+/);
    if (m) { start = i; startLevel = m[1].length; break; }
  }
  if (start === -1) return text.trim().slice(0, 1200);

  let end = lines.length;
  for (let i = start + 1; i < lines.length; i++) {
    const m = lines[i].match(/^(#{1,3})\s+/);
    if (m && m[1].length <= startLevel) { end = i; break; }
  }
  return lines.slice(start, end).join("\n").trim();
}

async function main() {
  setText("year", new Date().getFullYear());
  pickLogo();

  const repoUrl = `https://github.com/${OWNER}/${REPO}`;
  setHref("footerRepo", repoUrl);
  setText("footerRepo", repoUrl.replace("https://", ""));
  setHref("btnSource", repoUrl);
  setHref("btnIssues", `${repoUrl}/issues`);

  // Load mod.json
  let mod = null;
  try {
    mod = await fetchJson(`${RAW_BASE}/mod.json`);
    setText("modName", mod.name || "Geobot");
    setText("heroTitle", mod.name || "Geobot");
    setText("modVersion", mod.version ? `v${mod.version}` : "version ?");
    setText("modId", mod.id || "");
    setText("installId", mod.id || "");
    setText("modDesc", mod.description || "");

    setText("geodeVer", mod.geode || "—");

    const gd = mod.gd || {};
    const gdStr = Object.entries(gd).map(([k,v]) => `${k}: ${v}`).join(" • ");
    setText("gdVers", gdStr || "—");

    // lightweight badge
    const gdCompat = gdStr ? `GD ${gdStr}` : "GD compatible";
    setText("gdCompat", gdCompat);

    setText("developer", mod.developer || "—");
    const repo = mod.repository || repoUrl;
    setHref("btnSource", repo);
    setHref("footerRepo", repo);
    setText("footerRepo", repo.replace("https://", ""));

    const issuesUrl = (mod.issues && mod.issues.url) ? mod.issues.url : `${repo}/issues`;
    setHref("btnIssues", issuesUrl);
  } catch (e) {
    console.warn("mod.json load failed:", e);
  }

  // Load About + Changelog full
  let changelogMd = "";
  try {
    const aboutMd = await fetchText(`${RAW_BASE}/about.md`);
    setHtml("aboutHtml", mdToHtml(aboutMd));
  } catch (e) {
    setHtml("aboutHtml", `<p>Could not load <code>about.md</code>: ${escapeHtml(e.message)}</p>`);
  }

  try {
    changelogMd = await fetchText(`${RAW_BASE}/changelog.md`);
    setHtml("changelogHtml", mdToHtml(changelogMd));
    setHtml("latestChangelogEntry", mdToHtml(firstMarkdownSection(changelogMd)));
  } catch (e) {
    setHtml("changelogHtml", `<p>Could not load <code>changelog.md</code>: ${escapeHtml(e.message)}</p>`);
    setHtml("latestChangelogEntry", `<p>Could not load changelog.</p>`);
  }

  // Latest release (download + notes)
  let latestBuildUrl = null;
  let releaseUrl = null;

  try {
    const rel = await fetchJson(`${API_REPO}/releases/latest`);

    if (rel) {
      releaseUrl = rel.html_url || null;
      if (releaseUrl) setHref("btnReleasePage", releaseUrl);

      const assets = Array.isArray(rel.assets) ? rel.assets : [];
      if (assets.length) {
        const geodeAsset = assets.find(a => (a.name || "").toLowerCase().endsWith(".geode"));
        const chosen = geodeAsset || assets[0];
        latestBuildUrl = chosen.browser_download_url;

        setText("releaseHint", `Latest release: ${rel.tag_name} • asset: ${chosen.name}`);
      } else {
        setText("releaseHint", `Latest release: ${rel.tag_name} (no assets uploaded)`);
      }

      const body = (rel.body || "").trim();
      if (body) {
        setHtml("latestReleaseNotes", mdToHtml(body.length > 5000 ? body.slice(0, 5000) + "\n\n…(trimmed)" : body));
      } else {
        // fallback to changelog
        setHtml("latestReleaseNotes", changelogMd ? mdToHtml(firstMarkdownSection(changelogMd)) : "<p>No release notes available.</p>");
      }
    }
  } catch (e) {
    console.warn("No latest release:", e);
    setText("releaseHint", "No official release found — using changelog/source fallback.");
    setHtml("latestReleaseNotes", changelogMd ? mdToHtml(firstMarkdownSection(changelogMd)) : "<p>No release notes available.</p>");
    // hide release page button if we couldn't find it
    const b = el("btnReleasePage");
    if (b) b.style.display = "none";
  }

  if (latestBuildUrl) {
    setHref("btnDownloadLatest", latestBuildUrl);
  } else {
    // No release assets → hide the "Latest Build" button
    const btn = el("btnDownloadLatest");
    if (btn) btn.style.display = "none";
  }
}

main().catch((e) => {
  console.error(e);
  setText("releaseHint", `Site failed to load: ${e.message}`);
});
