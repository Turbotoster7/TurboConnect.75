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
