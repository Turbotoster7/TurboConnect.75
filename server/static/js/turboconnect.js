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
