from flask import Flask
import spotipy
from spotipy.oauth2 import SpotifyOAuth

app = Flask(__name__)

sp = spotipy.Spotify(auth_manager=SpotifyOAuth(
    client_id="TWOJ_ID",
    client_secret="TWOJ_SECRET",
    redirect_uri="http://localhost:8888",
    scope="user-modify-playback-state"
))

@app.route('/next')
def next_track():
    sp.next_track()
    return "Następny utwór!"

@app.route('/play')
def play():
    sp.start_playback()
    return "Gramy!"

@app.route('/pause')
def pause():
    sp.pause_playback()
    return "Pauza!"


@app.route("/search")
def search():
    sp.searc


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)