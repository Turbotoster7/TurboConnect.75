from __future__ import annotations
import hashlib
import io
import json
import re
import secrets
import shutil
import subprocess
import threading
import os
import time
import uuid
import zipfile
from functools import wraps
from pathlib import Path
from typing import Callable, Optional

import traceback

from flask import (
    Flask,
    Response,
    abort,
    flash,
    get_flashed_messages,
    jsonify,
    redirect,
    render_template,
    request,
    send_file,
    session,
    stream_with_context,
    url_for,
)
from werkzeug.security import check_password_hash, generate_password_hash

# Lazy imports - jesli czegos brakuje na hostingu, panel WWW
# i tak musi sie zaladowac (zglosimy blad dopiero przy uzyciu funkcji).
try:
    import requests as _requests  # type: ignore
except Exception as _requests_err:  # pragma: no cover
    _requests = None
    _REQUESTS_IMPORT_ERROR = _requests_err
else:
    _REQUESTS_IMPORT_ERROR = None

try:
    import yt_dlp as _yt_dlp  # type: ignore
except Exception as _yt_err:  # pragma: no cover
    _yt_dlp = None
    _YT_DLP_IMPORT_ERROR = _yt_err
else:
    _YT_DLP_IMPORT_ERROR = None


def _require_requests():
    if _requests is None:
        raise RuntimeError(
            "Modul 'requests' nie jest zainstalowany na serwerze. "
            "Uruchom: pip install -r requirements.txt"
            + (f" (szczegoly: {_REQUESTS_IMPORT_ERROR})" if _REQUESTS_IMPORT_ERROR else "")
        )
    return _requests


def _require_yt_dlp():
    if _yt_dlp is None:
        raise RuntimeError(
            "Modul 'yt_dlp' nie jest zainstalowany na serwerze. "
            "Uruchom: pip install -r requirements.txt"
            + (f" (szczegoly: {_YT_DLP_IMPORT_ERROR})" if _YT_DLP_IMPORT_ERROR else "")
        )
    return _yt_dlp

app = Flask(__name__, static_folder="static", static_url_path="/static", template_folder="templates")
application = app

# Folder na muzyke i sciezka do Twojego FFmpeg
BASE_DIR = Path(__file__).resolve().parent
MUSIC_DIR = BASE_DIR / "music_library"
MUSIC_DIR.mkdir(exist_ok=True)
ZIPS_DIR = BASE_DIR / "zips"
ZIPS_DIR.mkdir(exist_ok=True)
JOBS_DIR = BASE_DIR / "jobs"
JOBS_DIR.mkdir(exist_ok=True)
PLAYLISTS_FILE = BASE_DIR / "playlists.json"
USERS_FILE = BASE_DIR / "users.json"
SECRET_FILE = BASE_DIR / "secret.key"

SPOTIFY_PLAYLIST_RE = re.compile(r"(?:open\.spotify\.com/(?:embed/)?playlist/|spotify:playlist:)([A-Za-z0-9]{16,32})")


def _load_secret_key() -> bytes:
    env = os.environ.get("TURBOCONNECT_SECRET")
    if env:
        return env.encode("utf-8")
    if SECRET_FILE.exists():
        data = SECRET_FILE.read_bytes().strip()
        if data:
            return data
    key = secrets.token_hex(32).encode("utf-8")
    try:
        SECRET_FILE.write_bytes(key)
        try:
            os.chmod(SECRET_FILE, 0o600)
        except OSError:
            pass
    except OSError:
        pass
    return key


def _load_users() -> dict:
    if USERS_FILE.exists():
        try:
            data = json.loads(USERS_FILE.read_text(encoding="utf-8"))
            if isinstance(data, dict) and isinstance(data.get("users"), dict) and data["users"]:
                return data
        except Exception:
            pass

    user = os.environ.get("TURBOCONNECT_USER", "admin").strip() or "admin"
    pwd = os.environ.get("TURBOCONNECT_PASS", "admin")
    data = {"users": {user: generate_password_hash(pwd)}}
    try:
        USERS_FILE.write_text(json.dumps(data, indent=2), encoding="utf-8")
        try:
            os.chmod(USERS_FILE, 0o600)
        except OSError:
            pass
    except OSError:
        pass
    print(f"[turboconnect] Utworzono domyslnego uzytkownika '{user}'. ZMIEN HASLO w {USERS_FILE} lub zmiennej TURBOCONNECT_PASS.")
    return data


def _verify_login(username: str, password: str) -> bool:
    data = _load_users()
    h = data.get("users", {}).get(username)
    if not h:
        return False
    try:
        return check_password_hash(h, password)
    except Exception:
        return False


def _is_logged_in() -> bool:
    return bool(session.get("tc_user"))


def login_required(view):
    """Wymaga zalogowanej sesji do widokow panelu WWW."""
    @wraps(view)
    def wrapper(*args, **kwargs):
        if not _is_logged_in():
            if request.accept_mimetypes.best == "application/json" or request.path.startswith("/api/"):
                return {"ok": False, "error": "auth required"}, 401
            return redirect(url_for("login", next=request.full_path if request.query_string else request.path))
        return view(*args, **kwargs)
    return wrapper


app.secret_key = _load_secret_key()
app.config.update(
    SESSION_COOKIE_HTTPONLY=True,
    SESSION_COOKIE_SAMESITE="Lax",
    PERMANENT_SESSION_LIFETIME=60 * 60 * 24 * 14,
)


@app.context_processor
def _inject_user():
    return {
        "current_user": session.get("tc_user"),
        "is_authenticated": _is_logged_in(),
    }


@app.errorhandler(500)
@app.errorhandler(Exception)
def _handle_internal_error(e):
    """Czytelna strona 500 zamiast generycznego komunikatu."""
    # Flask abort/redirect/HTTPException maja wlasne kody - puscmy dalej.
    from werkzeug.exceptions import HTTPException
    if isinstance(e, HTTPException):
        return e

    tb = traceback.format_exc()
    app.logger.error("500 on %s: %s\n%s", request.path, e, tb)
    show_tb = bool(os.environ.get("TURBOCONNECT_DEBUG"))
    html = (
        "<!doctype html><meta charset='utf-8'><title>500 - Turboconnect</title>"
        "<body style='background:#0b1026;color:#e0f7ff;font-family:monospace;padding:2rem;'>"
        "<h1 style='color:#ff6b6b;'>500 - cos sie wysypalo</h1>"
        f"<p><strong>{type(e).__name__}:</strong> {str(e)[:400]}</p>"
        "<p>Sprawdz logi serwera. Najczestsza przyczyna po aktualizacji: "
        "missing packages from <code>requirements.txt</code> "
        "(<code>pip install -r requirements.txt</code> w venv).</p>"
        "<p><a style='color:#00f0ff' href='/healthz'>/healthz</a> &nbsp;|&nbsp; "
        "<a style='color:#00f0ff' href='/login'>/login</a></p>"
        + (f"<pre style='color:#aabcdf;white-space:pre-wrap;'>{tb}</pre>" if show_tb else "")
        + "</body>"
    )
    return Response(html, status=500, mimetype="text/html; charset=utf-8")


@app.route("/healthz")
def healthz():
    """Diagnostyka: pokazuje co jest zainstalowane i czy katalogi sa zapisywalne."""
    info = {
        "ok": True,
        "python": os.sys.version.split()[0],
        "requests": getattr(_requests, "__version__", None) if _requests else None,
        "yt_dlp": getattr(getattr(_yt_dlp, "version", None), "__version__", None) if _yt_dlp else None,
        "ffmpeg": _get_ffmpeg_path(),
        "users_file": str(USERS_FILE),
        "users_file_exists": USERS_FILE.exists(),
        "spotify_client_credentials": bool(_spotify_client_credentials()),
        "secret_file_exists": SECRET_FILE.exists(),
        "music_dir_writable": os.access(MUSIC_DIR, os.W_OK),
        "zips_dir_writable": os.access(ZIPS_DIR, os.W_OK),
        "errors": {
            "requests": str(_REQUESTS_IMPORT_ERROR) if _REQUESTS_IMPORT_ERROR else None,
            "yt_dlp": str(_YT_DLP_IMPORT_ERROR) if _YT_DLP_IMPORT_ERROR else None,
        },
    }
    return info

def _get_ffmpeg_path():
    """Szuka FFmpeg najpierw w folderze aplikacji, potem w systemie."""
    local_ffmpeg = BASE_DIR / "ffmpeg"
    if local_ffmpeg.exists() and os.access(local_ffmpeg, os.X_OK):
        return str(local_ffmpeg)
    return shutil.which("ffmpeg")


def _load_playlists() -> dict:
    if not PLAYLISTS_FILE.exists():
        return {"playlists": []}
    try:
        return json.loads(PLAYLISTS_FILE.read_text(encoding="utf-8"))
    except Exception:
        return {"playlists": []}


def _save_playlists(data: dict) -> None:
    PLAYLISTS_FILE.write_text(
        json.dumps(data, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def _find_playlist(data: dict, playlist_id: str) -> dict | None:
    for p in data.get("playlists", []):
        if p.get("id") == playlist_id:
            return p
    return None

def _track_id_for_relpath(rel_posix: str) -> str:
    return hashlib.sha256(rel_posix.encode("utf-8")).hexdigest()[:16]

def _iter_audio():
    if not MUSIC_DIR.is_dir():
        return
    # Szukamy mp3 i m4a
    for ext in ("*.mp3", "*.m4a"):
        for path in sorted(MUSIC_DIR.rglob(ext)):
            if path.is_file():
                try:
                    rel = path.relative_to(MUSIC_DIR)
                    rel_posix = rel.as_posix()
                    yield path, rel_posix, _track_id_for_relpath(rel_posix)
                except ValueError:
                    continue

def _path_for_track_id(tid: str) -> Path | None:
    for path, _rel, i in _iter_audio():
        if i == tid:
            return path
    return None


def _all_tracks() -> list[dict]:
    tracks: list[dict] = []
    for path, rel_posix, tid in _iter_audio():
        mime = _sniff_audio_mimetype(path)
        suf = _stream_url_suffix(mime)
        tracks.append(
            {
                "id": tid,
                "title": path.stem.replace("_", " "),
                "filename": path.name,
                "rel": rel_posix,
                "path": path,
                "stream_url": f"/library/stream/{tid}{suf}",
                "download_url": f"/library/file/{tid}{suf}",
            }
        )
    return tracks

def _sniff_audio_mimetype(path: Path) -> str:
    try:
        with path.open("rb") as f:
            head = f.read(16)
    except OSError:
        head = b""
    if len(head) >= 8 and head[4:8] == b"ftyp":
        return "audio/mp4"
    if head[:3] == b"ID3" or (len(head) >= 2 and head[0] == 0xFF and (head[1] & 0xE0) == 0xE0):
        return "audio/mpeg"
    if head[:4] == b"RIFF" and len(head) >= 12 and head[8:12] == b"WAVE":
        return "audio/wav"
    return "application/octet-stream"

def _stream_url_suffix(mimetype: str) -> str:
    if mimetype == "audio/mp4": return ".m4a"
    if mimetype == "audio/mpeg": return ".mp3"
    return ".bin"

def _normalize_stream_track_id(raw: str) -> str:
    for ext in (".mp3", ".m4a", ".webm", ".bin", ".MP3", ".M4A"):
        if raw.endswith(ext):
            return raw[: -len(ext)]
    return raw

def _ascii_download_name(path: Path) -> str:
    name = path.name.replace('"', "").replace("\\", "")[:120]
    return name if re.fullmatch(r"[\x20-\x7e]+", name) else f"track{path.suffix}"

def _convert_m4a_library_to_mp3(delete_source: bool = False) -> tuple[int, int, list[str]]:
    converted, failed, messages = 0, 0, []
    ffmpeg_bin = _get_ffmpeg_path()

    if not ffmpeg_bin:
        return 0, 0, ["FFmpeg not found on server (including local binary)."]

    for src in sorted(MUSIC_DIR.rglob("*.m4a")):
        dst = src.with_suffix(".mp3")
        if dst.exists(): continue

        cmd = [ffmpeg_bin, "-y", "-i", str(src), "-vn", "-acodec", "libmp3lame", "-b:a", "128k", str(dst)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            converted += 1
            if delete_source: src.unlink()
        else:
            failed += 1
            messages.append(f"BLAD: {src.name}")
    return converted, failed, messages

def _clean_string(text: str) -> str:
    return re.sub(r'[^a-zA-Z0-9]', '', text).lower()


def _ydl_opts_for_library(target_dir: Path) -> dict:
    """Bazowa konfiguracja yt-dlp z opcjonalnym FFmpeg."""
    ffmpeg_bin = _get_ffmpeg_path()
    opts: dict = {
        "outtmpl": str(target_dir / "%(title)s.%(ext)s"),
        "noplaylist": True,
        "quiet": True,
        "no_warnings": True,
        "ignoreerrors": False,
        "default_search": "ytsearch1",
        "socket_timeout": 30,
        "retries": 2,
        "fragment_retries": 2,
    }
    if ffmpeg_bin:
        opts["ffmpeg_location"] = ffmpeg_bin
        opts["format"] = "bestaudio/best"
        opts["postprocessors"] = [{
            "key": "FFmpegExtractAudio",
            "preferredcodec": "mp3",
            "preferredquality": "128",
        }]
    else:
        opts["format"] = "bestaudio[ext=mp3]/best[ext=mp3]/bestaudio[acodec*=mp3]/best[acodec*=mp3]"
    return opts


def _download_track(query: str, target_dir: Path, deadline_s: float = 120.0) -> Path:
    """Pobiera jeden utwor przez yt-dlp do target_dir i zwraca sciezke do pliku."""
    target_dir.mkdir(parents=True, exist_ok=True)
    opts = _ydl_opts_for_library(target_dir)
    ytdl_mod = _require_yt_dlp()

    deadline = time.monotonic() + deadline_s

    def _check_deadline(d: dict) -> None:
        if time.monotonic() > deadline:
            raise ytdl_mod.utils.DownloadError("timeout pobierania utworu")

    opts["progress_hooks"] = [_check_deadline]

    try:
        with ytdl_mod.YoutubeDL(opts) as ydl:
            info = ydl.extract_info(f"ytsearch1:{query}", download=True)
    except ytdl_mod.utils.DownloadError as e:
        raise RuntimeError(str(e)) from e
    if not info:
        raise RuntimeError("brak wynikow")
    if "entries" in info and info["entries"]:
        info = info["entries"][0]

    candidate_paths: list[Path] = []
    rd = info.get("requested_downloads") or []
    for d in rd:
        fp = d.get("filepath") or d.get("filename")
        if fp:
            candidate_paths.append(Path(fp))
    if not candidate_paths:
        ydl_filename = info.get("_filename") or info.get("filename")
        if ydl_filename:
            candidate_paths.append(Path(ydl_filename))

    if _get_ffmpeg_path():
        for p in list(candidate_paths):
            mp3 = p.with_suffix(".mp3")
            if mp3.exists():
                candidate_paths.insert(0, mp3)

    for p in candidate_paths:
        if p.is_file():
            return p

    raise RuntimeError("nie znaleziono pobranego pliku")


def _spotify_playlist_id(url: str) -> str | None:
    m = SPOTIFY_PLAYLIST_RE.search(url or "")
    return m.group(1) if m else None


_SPOTIFY_UA = (
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36"
)

# Cache na token Client Credentials (1 godzina waznosci)
_SPOTIFY_CC_TOKEN: dict = {"token": None, "expires_at": 0.0}


def _spotify_client_credentials() -> tuple[str, str] | None:
    """Zwraca (client_id, client_secret) z env-ow albo None."""
    cid = (os.environ.get("SPOTIFY_CLIENT_ID") or os.environ.get("SPOTIPY_CLIENT_ID") or "").strip()
    csec = (os.environ.get("SPOTIFY_CLIENT_SECRET") or os.environ.get("SPOTIPY_CLIENT_SECRET") or "").strip()
    if cid and csec:
        return cid, csec
    return None


def _spotify_token_client_credentials(req) -> str:
    """Pobiera token przez oficjalny Client Credentials Flow.

    Wymaga zmiennych SPOTIFY_CLIENT_ID i SPOTIFY_CLIENT_SECRET
    (zalozenie aplikacji na https://developer.spotify.com/dashboard).
    """
    creds = _spotify_client_credentials()
    if not creds:
        raise RuntimeError(
            "missing SPOTIFY_CLIENT_ID / SPOTIFY_CLIENT_SECRET — "
            "zaloz darmowa aplikacje na https://developer.spotify.com/dashboard "
            "i wstaw klucze w panelu Pythona (Environment Variables)"
        )

    now = time.time()
    cache = _SPOTIFY_CC_TOKEN
    if cache.get("token") and cache.get("expires_at", 0) - 30 > now:
        return cache["token"]

    cid, csec = creds
    import base64
    basic = base64.b64encode(f"{cid}:{csec}".encode("utf-8")).decode("ascii")
    r = req.post(
        "https://accounts.spotify.com/api/token",
        headers={
            "Authorization": f"Basic {basic}",
            "Content-Type": "application/x-www-form-urlencoded",
            "User-Agent": _SPOTIFY_UA,
        },
        data={"grant_type": "client_credentials"},
        timeout=15,
    )
    if r.status_code != 200:
        raise RuntimeError(f"token endpoint zwrocil HTTP {r.status_code}: {r.text[:200]}")
    payload = r.json()
    tok = payload.get("access_token")
    if not tok:
        raise RuntimeError("token endpoint nie zwrocil access_token")
    cache["token"] = tok
    cache["expires_at"] = now + float(payload.get("expires_in", 3600))
    return tok


def _spotify_token_from_page(req, pid: str) -> str:
    """Pobiera anonimowy access token z glownej strony playlisty open.spotify.com.

    Spotify wstawia w HTML <script id="session"> z JSON-em zawierajacym 'accessToken'.
    To te same dane co loaduje web player.
    """
    url = f"https://open.spotify.com/playlist/{pid}"
    headers = {
        "User-Agent": _SPOTIFY_UA,
        "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language": "en-US,en;q=0.9",
    }
    r = req.get(url, headers=headers, timeout=20)
    if r.status_code != 200:
        raise RuntimeError(f"strona playlisty zwrocila HTTP {r.status_code}")

    html = r.text
    patterns = [
        r'<script[^>]+id="session"[^>]*>\s*(\{.*?\})\s*</script>',
        r'<script[^>]+id="config"[^>]*>\s*(\{.*?\})\s*</script>',
    ]
    for pat in patterns:
        m = re.search(pat, html, re.DOTALL)
        if not m:
            continue
        try:
            payload = json.loads(m.group(1))
        except json.JSONDecodeError:
            continue
        tok = payload.get("accessToken") or payload.get("access_token")
        if tok:
            return tok

    m = re.search(r'"accessToken"\s*:\s*"([A-Za-z0-9._\-]+)"', html)
    if m:
        return m.group(1)

    raise RuntimeError("nie znaleziono accessToken na stronie playlisty (Spotify zmienil format?)")


def _spotify_get_token(req, pid: str) -> tuple[str, str]:
    """Zwraca (token, source) - 'cc' lub 'page'."""
    if _spotify_client_credentials():
        return _spotify_token_client_credentials(req), "cc"
    return _spotify_token_from_page(req, pid), "page"


def _spotify_extract_tracks(items: list) -> list[dict]:
    out: list[dict] = []
    for item in items or []:
        t = (item or {}).get("track") or {}
        if not isinstance(t, dict):
            continue
        title = (t.get("name") or "").strip()
        artists = t.get("artists") or []
        artist = ", ".join(a.get("name", "") for a in artists if isinstance(a, dict)).strip()
        if title:
            out.append({"title": title, "artist": artist})
    return out


def _spotify_via_api(req, pid: str) -> tuple[str, list[dict]]:
    """Pobiera playliste przez oficjalne Spotify Web API.

    Spotify od 2024 r. drastycznie ograniczyl, co dostaje aplikacja
    z Client Credentials (czasem oddaje playliste z pusta tracks.items).
    Probujemy roznych URL-i + roznych marketow i odpadamy z konkretnym
    komunikatem (z tracks.total) gdy nic nie zwraca utworow.
    """
    token, _src = _spotify_get_token(req, pid)
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/json",
        "User-Agent": _SPOTIFY_UA,
    }

    market_env = (os.environ.get("SPOTIFY_MARKET") or "").strip()
    markets: list[str] = []
    for m in [market_env, "PL", "US", "GB", "from_token", ""]:
        if m not in markets:
            markets.append(m)

    attempts: list[str] = []
    last_name = "Spotify Playlist"
    last_total: int | None = None

    base = f"https://api.spotify.com/v1/playlists/{pid}"
    for market in markets:
        suffix = f"?market={market}" if market else ""
        url = base + suffix
        try:
            r = req.get(url, headers=headers, timeout=25)
        except Exception as e:
            attempts.append(f"GET {url}: {e}")
            continue

        if r.status_code != 200:
            attempts.append(f"GET {url} -> HTTP {r.status_code}: {r.text[:140]}")
            continue

        try:
            payload = r.json()
        except Exception as e:
            attempts.append(f"GET {url}: bad JSON: {e}")
            continue

        last_name = (payload.get("name") or last_name).strip()
        tracks_block = payload.get("tracks") or {}
        last_total = tracks_block.get("total")
        tracks = _spotify_extract_tracks(tracks_block.get("items") or [])

        next_url = tracks_block.get("next")
        while next_url and len(tracks) < (last_total or 1000):
            try:
                rr = req.get(next_url, headers=headers, timeout=25)
            except Exception as e:
                attempts.append(f"next {next_url}: {e}")
                break
            if rr.status_code != 200:
                attempts.append(f"next {next_url} -> HTTP {rr.status_code}")
                break
            page = rr.json()
            tracks.extend(_spotify_extract_tracks(page.get("items") or []))
            next_url = page.get("next")

        if tracks:
            return last_name, tracks

        attempts.append(
            f"GET {url} -> HTTP 200, name='{last_name}', tracks.total={last_total}, items=0"
        )

    for market in markets:
        suffix = f"?market={market}&limit=100" if market else "?limit=100"
        url = base + "/tracks" + suffix
        try:
            r = req.get(url, headers=headers, timeout=25)
        except Exception as e:
            attempts.append(f"GET {url}: {e}")
            continue

        if r.status_code != 200:
            attempts.append(f"GET {url} -> HTTP {r.status_code}: {r.text[:140]}")
            continue

        try:
            payload = r.json()
        except Exception as e:
            attempts.append(f"GET {url}: bad JSON: {e}")
            continue

        last_total = payload.get("total") if last_total is None else last_total
        tracks = _spotify_extract_tracks(payload.get("items") or [])
        next_url = payload.get("next")
        while next_url:
            try:
                rr = req.get(next_url, headers=headers, timeout=25)
            except Exception as e:
                attempts.append(f"next {next_url}: {e}")
                break
            if rr.status_code != 200:
                attempts.append(f"next {next_url} -> HTTP {rr.status_code}")
                break
            page = rr.json()
            tracks.extend(_spotify_extract_tracks(page.get("items") or []))
            next_url = page.get("next")

        if tracks:
            return last_name, tracks

        attempts.append(f"GET {url} -> HTTP 200 ale items=0 (total={last_total})")

    msg_total = f"Spotify reports {last_total} tracks" if last_total else "Spotify did not report track count"
    raise RuntimeError(
        f"API returned no tracks ({msg_total}). "
        f"Probowano markets={markets[:4]}. "
        f"Ostatnie odpowiedzi: {' | '.join(attempts[-3:])}"
    )


def _spotify_via_embed(req, pid: str) -> tuple[str, list[dict]]:
    """Fallback: parsuje HTML strony embed (starszy format z __NEXT_DATA__)."""
    headers = {
        "User-Agent": _SPOTIFY_UA,
        "Accept-Language": "en-US,en;q=0.9",
    }
    embed_url = f"https://open.spotify.com/embed/playlist/{pid}"
    resp = req.get(embed_url, headers=headers, timeout=20)
    resp.raise_for_status()
    html = resp.text

    m = re.search(r'<script id="__NEXT_DATA__"[^>]*>(.*?)</script>', html, re.DOTALL)
    if not m:
        raise RuntimeError("strona embed Spotify nie zawiera __NEXT_DATA__")

    try:
        data = json.loads(m.group(1))
    except json.JSONDecodeError as e:
        raise RuntimeError(f"nieprawidlowy JSON Spotify embed: {e}")

    name = "Spotify Playlist"
    tracks: list[dict] = []

    def harvest(track_list):
        for t in track_list:
            if not isinstance(t, dict):
                continue
            title = (t.get("title") or t.get("name") or "").strip()
            subtitle = t.get("subtitle")
            if not subtitle and isinstance(t.get("artists"), list):
                subtitle = ", ".join(a.get("name", "") for a in t["artists"] if isinstance(a, dict))
            subtitle = (subtitle or "").strip()
            if title:
                tracks.append({"title": title, "artist": subtitle})

    try:
        entity = data["props"]["pageProps"]["state"]["data"]["entity"]
        name = entity.get("name") or entity.get("title") or name
        if isinstance(entity.get("trackList"), list):
            harvest(entity["trackList"])
    except (KeyError, TypeError):
        pass

    if not tracks:
        def walk(node):
            if isinstance(node, dict):
                tl = node.get("trackList")
                if isinstance(tl, list):
                    harvest(tl)
                for v in node.values():
                    walk(v)
            elif isinstance(node, list):
                for v in node:
                    walk(v)
        walk(data)

    if not tracks:
        raise RuntimeError("embed nie zwrocil listy utworow (Spotify ladowal dynamicznie?)")

    return name, tracks


def _parse_spotify_playlist(url: str) -> tuple[str, list[dict]]:
    """Zwraca (nazwa_playlisty, [{'artist': str, 'title': str}]).

    Probuje po kolei:
      1) Spotify Web API z anonimowym tokenem z glownej strony playlisty,
      2) strona embed (fallback, dla starszych ukladow).
    """
    pid = _spotify_playlist_id(url)
    if not pid:
        raise ValueError("invalid Spotify playlist URL (expected 'open.spotify.com/playlist/<id>')")

    req = _require_requests()
    errors: list[str] = []
    has_cc = bool(_spotify_client_credentials())

    for label, fn in (("api", _spotify_via_api), ("embed", _spotify_via_embed)):
        try:
            name, tracks = fn(req, pid)
            return name, _dedup_tracks(tracks)
        except Exception as e:  # noqa: BLE001 - chcemy zlapac WSZYSTKO
            errors.append(f"{label}: {e}")

    extra = ""
    if not has_cc:
        extra = (
            " | Wskazowka: zaloz darmowa aplikacje na "
            "https://developer.spotify.com/dashboard i ustaw zmienne "
            "SPOTIFY_CLIENT_ID / SPOTIFY_CLIENT_SECRET w panelu hostingu (Environment Variables)."
        )
    raise ValueError(
        "Could not fetch playlist from Spotify. Tried: " + "; ".join(errors) + extra
    )


def _dedup_tracks(tracks: list[dict]) -> list[dict]:
    seen: set[tuple[str, str]] = set()
    unique: list[dict] = []
    for t in tracks:
        key = (t["title"].lower(), t.get("artist", "").lower())
        if key in seen:
            continue
        seen.add(key)
        unique.append(t)
    return unique


def _sanitize_zip_name(name: str) -> str:
    name = re.sub(r"[^A-Za-z0-9 _\-]+", " ", name).strip()
    name = re.sub(r"\s+", "_", name)
    return name[:60] or "spotify_playlist"

@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        username = (request.form.get("username") or "").strip()
        password = request.form.get("password") or ""
        if _verify_login(username, password):
            session.permanent = True
            session["tc_user"] = username
            nxt = request.form.get("next") or request.args.get("next")
            if nxt and nxt.startswith("/") and not nxt.startswith("//"):
                return redirect(nxt)
            return redirect(url_for("music_page"))
        flash("Invalid username or password.")
    if _is_logged_in():
        return redirect(url_for("music_page"))
    return render_template(
        "login.html",
        next=request.args.get("next", ""),
        errors=get_flashed_messages(),
    )


@app.route("/logout")
def logout():
    session.pop("tc_user", None)
    return redirect(url_for("login"))


@app.route("/")
@login_required
def index():
    return redirect(url_for("music_page"))

@app.route("/music")
@login_required
def music_page():
    q = request.args.get("q", "")
    qn = _clean_string(q)
    tracks = []
    ffmpeg_bin = _get_ffmpeg_path()
    status = "AKTYWNY" if ffmpeg_bin else "BRAK"
    playlists_data = _load_playlists()

    for t in _all_tracks():
        if not qn or qn in _clean_string(t["title"]):
            tracks.append(t)

    playlists = []
    for p in playlists_data.get("playlists", []):
        playlists.append(
            {"id": p.get("id"), "name": p.get("name", "Playlist"), "track_count": len(p.get("track_ids", []))}
        )
    return render_template("music.html", q_e=q, tracks=tracks, playlists=playlists, ffmpeg_status=status)


@app.route("/player/<track_id>")
@login_required
def player(track_id: str):
    tid = _normalize_stream_track_id(track_id)
    tracks = _all_tracks()
    idx = -1
    for i, t in enumerate(tracks):
        if t["id"] == tid:
            idx = i
            break
    if idx < 0:
        abort(404)
    current = tracks[idx]
    prev_url = f"/player/{tracks[idx - 1]['id']}" if idx > 0 else None
    next_url = f"/player/{tracks[idx + 1]['id']}" if idx + 1 < len(tracks) else None
    prefetch_1 = tracks[idx + 1]["stream_url"] if idx + 1 < len(tracks) else None
    prefetch_2 = tracks[idx + 2]["stream_url"] if idx + 2 < len(tracks) else None
    return render_template(
        "player.html",
        title=current["title"],
        stream_url=current["stream_url"],
        download_url=current["download_url"],
        prev_url=prev_url,
        next_url=next_url,
        prefetch_1=prefetch_1,
        prefetch_2=prefetch_2,
    )


@app.route("/playlists/create", methods=["POST"])
@login_required
def playlists_create():
    name = request.form.get("name", "").strip()
    if not name:
        return "Enter a playlist name.", 400
    data = _load_playlists()
    playlist_id = hashlib.sha1(f"{name}-{time.time()}".encode("utf-8")).hexdigest()[:12]
    data.setdefault("playlists", []).append(
        {"id": playlist_id, "name": name[:60], "track_ids": [], "created": int(time.time())}
    )
    _save_playlists(data)
    return redirect(url_for("music_page"))


@app.route("/playlists/delete/<playlist_id>")
@login_required
def playlists_delete(playlist_id: str):
    data = _load_playlists()
    before = len(data.get("playlists", []))
    data["playlists"] = [p for p in data.get("playlists", []) if p.get("id") != playlist_id]
    if len(data["playlists"]) != before:
        _save_playlists(data)
    return redirect(url_for("music_page"))


@app.route("/playlists/add_tracks", methods=["POST"])
@login_required
def playlists_add_tracks():
    playlist_id = request.form.get("playlist_id", "").strip()
    track_ids = request.form.getlist("track_ids")
    if not playlist_id:
        return "Select a playlist.", 400
    if not track_ids:
        return "Select at least one track.", 400

    available_ids = {t["id"] for t in _all_tracks()}
    data = _load_playlists()
    playlist = _find_playlist(data, playlist_id)
    if not playlist:
        return "Playlist does not exist.", 404
    existing = set(playlist.get("track_ids", []))
    for tid in track_ids:
        if tid in available_ids and tid not in existing:
            playlist.setdefault("track_ids", []).append(tid)
    _save_playlists(data)
    return redirect(url_for("playlist_view", playlist_id=playlist_id))


@app.route("/playlist/<playlist_id>")
@login_required
def playlist_view(playlist_id: str):
    data = _load_playlists()
    playlist = _find_playlist(data, playlist_id)
    if not playlist:
        return "No playlist found.", 404
    all_tracks = {t["id"]: t for t in _all_tracks()}
    tracks = [all_tracks[tid] for tid in playlist.get("track_ids", []) if tid in all_tracks]
    return render_template("playlist.html", name=playlist.get("name", "Playlist"), tracks=tracks)


@app.route("/api/download_selected", methods=["POST"])
@login_required
def api_download_selected():
    track_ids = request.form.getlist("track_ids")
    if not track_ids:
        return "No tracks selected.", 400
    all_tracks = {t["id"]: t for t in _all_tracks()}
    selected = [all_tracks[tid] for tid in track_ids if tid in all_tracks]
    if not selected:
        return "No valid tracks found.", 400

    mem = io.BytesIO()
    with zipfile.ZipFile(mem, mode="w", compression=zipfile.ZIP_STORED) as zf:
        for t in selected:
            zf.write(t["path"], arcname=t["filename"])
    mem.seek(0)
    ts = int(time.time())
    return send_file(
        mem,
        mimetype="application/zip",
        as_attachment=True,
        download_name=f"turboconnect-selected-{ts}.zip",
    )

@app.route("/api/fetch")
def api_fetch():
    query = request.args.get("q", "").strip()
    if not query:
        return "Name is required!", 400
    try:
        path = _download_track(query, MUSIC_DIR)
    except Exception as e:
        return f"BLAD: {e}", 500
    if request.args.get("format") == "json":
        return {"ok": True, "filename": path.name, "title": path.stem.replace("_", " ")}
    return f"Pobrano: {path.name}"


@app.route("/api/search_plain")
def api_search_plain():
    q = request.args.get("q", "")
    qn = _clean_string(q)

    # Optional client-side cap; default high enough that newly fetched tracks
    # actually show up. Clamp to a sane upper bound for the tiny S60 list.
    try:
        limit = int(request.args.get("limit", "60"))
    except ValueError:
        limit = 60
    limit = max(1, min(limit, 200))

    # Gather matches, then sort newest first so freshly downloaded tracks
    # appear at the top of the Nokia screen.
    matches: list[tuple[float, dict]] = []
    for t in _all_tracks():
        if qn and qn not in _clean_string(t["title"]):
            continue
        try:
            mtime = float(t["path"].stat().st_mtime)
        except OSError:
            mtime = 0.0
        matches.append((mtime, t))
    matches.sort(key=lambda item: item[0], reverse=True)

    lines: list[str] = []
    for _mtime, t in matches[:limit]:
        title = t["title"].replace("|", " ").replace("\r", " ").replace("\n", " ")
        filename = _ascii_download_name(t["path"]).replace("|", "_")
        lines.append(f"{title}|{filename}|{t['download_url']}")

    response = Response("\n".join(lines), mimetype="text/plain; charset=utf-8")
    # Prevent intermediate proxies/host from caching the listing while we are
    # actively adding tracks via /api/fetch.
    response.headers["Cache-Control"] = "no-store, no-cache, must-revalidate, max-age=0"
    response.headers["Pragma"] = "no-cache"
    response.headers["Expires"] = "0"
    return response


@app.route("/api/spotify_preview")
@login_required
def api_spotify_preview():
    """Zwraca liste utworow z playlisty Spotify (bez pobierania)."""
    url = request.args.get("url", "").strip()
    try:
        name, tracks = _parse_spotify_playlist(url)
    except Exception as e:
        return {"ok": False, "error": str(e)}, 400
    return {
        "ok": True,
        "name": name,
        "count": len(tracks),
        "tracks": tracks,
    }


# ---------------------------------------------------------------------------
# Background job system dla importu Spotify
# (shared hosting czesto zabija dlugie polaczenia HTTP - dlatego puszczamy
# robote w tle, a UI tylko odpytuje status co sekunde)
# ---------------------------------------------------------------------------

# Stan zadania trzymamy w pliku JSON, zeby kazdy worker Passengera widzial to samo.
# (Slownik w pamieci by nie wystarczyl - polling moze trafic do innego procesu
#  niz ten ktory uruchomil watek.)
_JOB_FILE_LOCK = threading.Lock()
_SPOTIFY_JOBS_TTL = 3600  # 1h


def _job_path(job_id: str) -> Path:
    safe = re.sub(r"[^A-Za-z0-9_-]+", "", job_id)[:64]
    if not safe:
        raise ValueError("bad job id")
    return JOBS_DIR / f"{safe}.json"


def _job_write(job: dict) -> None:
    p = _job_path(job["id"])
    tmp = p.with_suffix(".tmp")
    with _JOB_FILE_LOCK:
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(job, f, ensure_ascii=False)
        os.replace(tmp, p)


def _job_read(job_id: str) -> dict | None:
    try:
