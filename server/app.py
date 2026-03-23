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
