document.addEventListener("DOMContentLoaded", () => {
    initMobileMenu();
    initFetchForm();
    initSpotifyForm();
    initBulkButtons();
    initConsoleClear();
    initLiveSearch();
    initMiniPlayer();
    initToolsToggle();
});

function initMobileMenu() {
    const menuBtn = document.getElementById("menuToggle");
    const navLinks = document.getElementById("navMenu");
    if (!menuBtn || !navLinks) return;
    menuBtn.addEventListener("click", () => {
        navLinks.classList.toggle("active");
        const icon = menuBtn.querySelector("i");
        if (icon) {
            icon.classList.toggle("fa-bars");
            icon.classList.toggle("fa-times");
        }
    });
}

function getConsole() {
    return {
        log: document.getElementById("consoleLog"),
        status: document.getElementById("consoleStatus"),
        actions: document.getElementById("consoleActions"),
        downloadLink: document.getElementById("consoleDownload"),
    };
}

function setStatus(state, text) {
    const { status } = getConsole();
    if (!status) return;
    status.classList.remove("idle", "busy", "done", "err");
    status.classList.add(state);
    status.textContent = text;
}

function clearConsole(initialText) {
    const { log, actions } = getConsole();
    if (log) log.innerHTML = "";
    if (actions) actions.hidden = true;
    if (initialText) addLog(initialText, "info");
}

function addLog(text, kind) {
    const { log } = getConsole();
    if (!log) return;
    const line = document.createElement("div");
    line.className = "log-line" + (kind ? " " + kind : "");
    line.textContent = text;
    log.appendChild(line);
    log.scrollTop = log.scrollHeight;
}

function initConsoleClear() {
    const btn = document.getElementById("consoleClear");
    if (!btn) return;
    btn.addEventListener("click", () => {
        clearConsole("> gotowy do pracy.");
        setStatus("idle", "IDLE");
    });
}

function initFetchForm() {
    const form = document.getElementById("fetchForm");
    if (!form) return;
    const input = form.querySelector('input[name="q"]');
    const btn = form.querySelector("button[type=submit]");

    form.addEventListener("submit", async (e) => {
        e.preventDefault();
        const q = (input.value || "").trim();
        if (!q) {
            input.focus();
            return;
        }
        clearConsole(`> POBIERAM: ${q}`);
        setStatus("busy", "FETCH...");
        btn.disabled = true;
        try {
            const r = await fetch(`/api/fetch?format=json&q=${encodeURIComponent(q)}`, { cache: "no-store" });
            const text = await r.text();
            let data = null;
            try { data = JSON.parse(text); } catch (_) {}
            if (r.ok && data && data.ok) {
                addLog(`OK: ${data.filename}`, "ok");
                addLog("Odśwież stronę, żeby zobaczyć utwór w bibliotece.", "info");
                setStatus("done", "OK");
            } else {
                addLog(`BŁĄD: ${text || "nieznany"}`, "err");
                setStatus("err", "FAIL");
            }
        } catch (err) {
            addLog(`BŁĄD KOMUNIKACJI: ${err}`, "err");
            setStatus("err", "FAIL");
        } finally {
            btn.disabled = false;
            input.value = "";
        }
    });
}

function triggerDownload(url) {
    try {
        const a = document.createElement("a");
        a.href = url;
        a.download = "";
        a.style.display = "none";
        document.body.appendChild(a);
        a.click();
        setTimeout(() => a.remove(), 1000);
    } catch (_) {
        window.location.href = url;
    }
}

function showZipReady(zipUrl, autoDownload) {
    const { actions, downloadLink } = getConsole();
    if (downloadLink) downloadLink.href = zipUrl;
    if (actions) actions.hidden = false;
    setStatus("done", "ZIP READY");
    addLog(`Pobierz ZIP: ${zipUrl}`, "ok");
    if (autoDownload) {
        addLog("Pobieram automatycznie...", "info");
        triggerDownload(zipUrl);
    }
}

async function pollSpotifyJob(jobId) {
    let since = 0;
    let triedFallback = false;

    while (true) {
        let data;
        try {
            const r = await fetch(`/api/spotify_status/${encodeURIComponent(jobId)}?since=${since}`, { cache: "no-store" });
            if (!r.ok) {
                addLog(`BŁĄD HTTP ${r.status} przy odpytywaniu statusu`, "err");
                setStatus("err", "FAIL");
                return;
            }
            data = await r.json();
        } catch (err) {
            addLog(`BŁĄD KOMUNIKACJI: ${err}`, "err");
            setStatus("err", "FAIL");
            return;
        }

        if (!data || !data.ok) {
            addLog(`BŁĄD: ${(data && data.error) || "nieznany"}`, "err");
            setStatus("err", "FAIL");
            return;
        }

        if (Array.isArray(data.logs)) {
            for (const entry of data.logs) {
                addLog(entry.t, entry.k || null);
            }
            since = data.logs_total || since + data.logs.length;
        }

        if (data.state === "done") {
            if (data.zip_url) {
                showZipReady(data.zip_url, true);
            } else if (!triedFallback) {
                triedFallback = true;
                try {
                    const fb = await fetch("/api/spotify_last_zip", { cache: "no-store" });
                    if (fb.ok) {
                        const fbd = await fb.json();
                        if (fbd && fbd.ok && fbd.url) {
                            showZipReady(fbd.url, true);
                            return;
                        }
                    }
                } catch (_) {}
                setStatus("err", "DONE WITHOUT ZIP");
                addLog("Zadanie zakonczone, ale brak ZIP-a.", "err");
            }
            return;
        }

        if (data.state === "error") {
            setStatus("err", "FAIL");
            return;
        }

        await new Promise((r) => setTimeout(r, 1200));
    }
}

function initSpotifyForm() {
    const form = document.getElementById("spotifyForm");
    if (!form) return;
    const urlInput = form.querySelector('input[name="url"]');
    const limitInput = form.querySelector('input[name="limit"]');
    const saveInput = form.querySelector('input[name="save"]');
    const btn = form.querySelector("button[type=submit]");

    form.addEventListener("submit", async (e) => {
        e.preventDefault();
        const url = (urlInput.value || "").trim();
        if (!url) {
            urlInput.focus();
            return;
        }
        const limit = parseInt(limitInput.value, 10) || 40;
        const save = saveInput && saveInput.checked ? "1" : "0";

        clearConsole(`> IMPORT SPOTIFY: ${url}`);
        addLog(`> limit=${limit}, zapis do biblioteki=${save === "1" ? "tak" : "nie"}`, "info");
        addLog("Uruchamiam zadanie w tle...", "info");
        setStatus("busy", "RUNNING...");
        btn.disabled = true;

        let jobId = null;
        try {
            const body = new FormData();
            body.append("url", url);
            body.append("limit", String(limit));
            body.append("save", save);
            const r = await fetch("/api/spotify_start", { method: "POST", body, cache: "no-store" });
            if (!r.ok) {
                const t = await r.text();
                addLog(`BŁĄD HTTP ${r.status}: ${t || r.statusText}`, "err");
                setStatus("err", "FAIL");
                return;
            }
            const data = await r.json();
            if (!data || !data.ok || !data.job_id) {
                addLog(`BŁĄD: ${(data && data.error) || "nie udalo sie wystartowac"}`, "err");
                setStatus("err", "FAIL");
                return;
            }
            jobId = data.job_id;
            addLog(`Job ID: ${jobId}`, "info");
        } catch (err) {
            addLog(`BŁĄD KOMUNIKACJI: ${err}`, "err");
            setStatus("err", "FAIL");
            return;
        } finally {
            btn.disabled = false;
        }

        await pollSpotifyJob(jobId);
    });
}

function initBulkButtons() {
    const all = document.getElementById("bulkAll");
    const none = document.getElementById("bulkNone");
    const form = document.getElementById("libForm");
    if (!form) return;
    const visible = () => Array.from(form.querySelectorAll('input[type="checkbox"][name="track_ids"]'))
        .filter((b) => {
            const row = b.closest(".track-row");
            return !row || !row.classList.contains("hidden-by-filter");
        });
    if (all) all.addEventListener("click", () => visible().forEach((b) => (b.checked = true)));
    if (none) none.addEventListener("click", () => visible().forEach((b) => (b.checked = false)));
}

/* =========================================
   Live search w bibliotece (filtruje wiersze na biezaco).
   ========================================= */
function initLiveSearch() {
    const input = document.getElementById("search-q");
    const clearBtn = document.getElementById("searchClear");
    const list = document.getElementById("trackList");
    const counter = document.getElementById("searchCounter");
    const empty = document.getElementById("emptyFilter");
    if (!input || !list) return;

    const rows = Array.from(list.querySelectorAll(".track-row"));
    const total = rows.length;

    const normalize = (s) => (s || "")
        .toString()
        .toLowerCase()
        .normalize("NFKD")
        .replace(/[\u0300-\u036f]/g, "");

    function apply() {
        const q = normalize(input.value.trim());
        let shown = 0;
        for (const row of rows) {
            const hay = normalize(row.dataset.search || row.textContent);
            const match = !q || hay.includes(q);
            row.classList.toggle("hidden-by-filter", !match);
            if (match) shown++;
        }
        if (counter) {
            counter.textContent = q
                ? `${shown} z ${total} pasuje do „${input.value.trim()}”`
                : `Łącznie utworów: ${total}`;
        }
        if (empty) empty.hidden = shown > 0;
    }

    input.addEventListener("input", apply);
    if (clearBtn) {
        clearBtn.addEventListener("click", () => {
            input.value = "";
            apply();
            input.focus();
        });
    }

    document.addEventListener("keydown", (e) => {
        if (e.key === "/" && document.activeElement !== input
            && !["INPUT", "TEXTAREA", "SELECT"].includes((document.activeElement || {}).tagName || "")) {
            e.preventDefault();
            input.focus();
            input.select();
        } else if (e.key === "Escape" && document.activeElement === input) {
            input.value = "";
            apply();
            input.blur();
        }
    });

    apply();
}

/* =========================================
   Mini-player: klikasz play przy utworze, gra inline + kolejka.
   ========================================= */
function initMiniPlayer() {
    const list = document.getElementById("trackList");
    const mp = document.getElementById("miniPlayer");
    const audio = document.getElementById("mpAudio");
    const titleEl = document.getElementById("mpTitle");
    const metaEl = document.getElementById("mpMeta");
    const playBtn = document.getElementById("mpPlay");
    const prevBtn = document.getElementById("mpPrev");
    const nextBtn = document.getElementById("mpNext");
    const closeBtn = document.getElementById("mpClose");
    if (!list || !mp || !audio) return;

    audio.controls = true;

    const allRows = () => Array.from(list.querySelectorAll(".track-row"));
    const visibleRows = () => allRows().filter((r) => !r.classList.contains("hidden-by-filter"));

    let currentRow = null;

    function show() {
        mp.hidden = false;
        document.body.classList.add("has-mp");
    }
    function setPlayingRow(row) {
        for (const r of allRows()) r.classList.remove("playing");
        if (row) row.classList.add("playing");
        currentRow = row;
    }
    function setIcon(playing) {
        const ic = playBtn.querySelector("i");
        if (!ic) return;
        ic.classList.toggle("fa-play", !playing);
        ic.classList.toggle("fa-pause", playing);
    }
    function playRow(row) {
        if (!row) return;
        const url = row.dataset.streamUrl;
        const title = row.dataset.title || "(bez tytulu)";
        const filename = row.dataset.filename || "";
        if (!url) return;
        show();
        setPlayingRow(row);
        titleEl.textContent = title;
        metaEl.textContent = filename;
        audio.src = url;
        audio.play().catch(() => { /* user gesture wymagany? nie - tu jest klik */ });
    }
    function neighbour(delta) {
        const rows = visibleRows();
        if (!rows.length) return null;
        if (!currentRow) return rows[0];
        let idx = rows.indexOf(currentRow);
        if (idx < 0) return rows[0];
        idx = (idx + delta + rows.length) % rows.length;
        return rows[idx];
    }

    list.addEventListener("click", (e) => {
        const btn = e.target.closest(".track-play-btn");
        if (!btn) return;
        const row = btn.closest(".track-row");
        if (!row) return;
        if (row === currentRow && !audio.paused) {
            audio.pause();
        } else if (row === currentRow && audio.paused && audio.src) {
            audio.play().catch(() => {});
        } else {
            playRow(row);
        }
    });

    playBtn.addEventListener("click", () => {
        if (!currentRow) {
            playRow(visibleRows()[0]);
            return;
        }
        if (audio.paused) audio.play().catch(() => {});
        else audio.pause();
    });
    prevBtn.addEventListener("click", () => playRow(neighbour(-1)));
    nextBtn.addEventListener("click", () => playRow(neighbour(1)));
    closeBtn.addEventListener("click", () => {
        audio.pause();
        audio.removeAttribute("src");
        audio.load();
        mp.hidden = true;
        document.body.classList.remove("has-mp");
        setPlayingRow(null);
    });

    audio.addEventListener("play", () => setIcon(true));
    audio.addEventListener("pause", () => setIcon(false));
    audio.addEventListener("ended", () => {
        const nxt = neighbour(1);
        if (nxt && nxt !== currentRow) playRow(nxt);
    });

    document.addEventListener("keydown", (e) => {
        const inField = ["INPUT", "TEXTAREA", "SELECT"].includes((document.activeElement || {}).tagName || "");
        if (inField) return;
        if (e.code === "Space") {
            if (!currentRow && !audio.src) return;
            e.preventDefault();
            if (audio.paused) audio.play().catch(() => {});
            else audio.pause();
        } else if (e.key === "n") {
            playRow(neighbour(1));
        } else if (e.key === "p") {
            playRow(neighbour(-1));
        }
    });
}

/* =========================================
   Rozwijacz "NARZĘDZIA" - chowa pobieranie / spotify / konsole / playlisty.
   ========================================= */
function initToolsToggle() {
    const btn = document.getElementById("toolsToggle");
    const area = document.getElementById("toolsArea");
    if (!btn || !area) return;

    function setOpen(open, opts) {
        opts = opts || {};
        btn.setAttribute("aria-expanded", open ? "true" : "false");
        area.setAttribute("aria-hidden", open ? "false" : "true");
        area.classList.toggle("is-open", open);
        if (open && opts.scroll) {
            requestAnimationFrame(() => {
                area.scrollIntoView({ behavior: "smooth", block: "start" });
            });
        }
    }

    btn.addEventListener("click", () => {
        const isOpen = btn.getAttribute("aria-expanded") === "true";
        setOpen(!isOpen);
    });

    // Linki / przyciski oznaczone [data-tools-open] automatycznie otwieraja sekcje.
    document.querySelectorAll("[data-tools-open]").forEach((el) => {
        el.addEventListener("click", () => setOpen(true, { scroll: true }));
    });

    // Submit w formularzach wewnątrz - nawet jak ktoś klikie hotlinkiem, otwiera.
    ["fetchForm", "spotifyForm"].forEach((id) => {
        const form = document.getElementById(id);
        if (!form) return;
        form.addEventListener("submit", () => setOpen(true));
    });

    // Jak URL ma hash do schowanej sekcji (np. #tc-spotify) - rozwin po wejsciu.
    const hash = (location.hash || "").toLowerCase();
    if (["#tc-fetch", "#tc-spotify"].includes(hash)) {
        setOpen(true, { scroll: true });
    }
}
