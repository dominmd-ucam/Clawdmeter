"""Now-playing source for the Música screen — Windows SMTC.

Reads the current media session (Spotify, browser, VLC...) through Windows'
System Media Transport Controls, so no Spotify API key is needed. Also turns
the cover into a small baseline JPEG and splits it into BLE-sized chunks, and
forwards prev / play-pause / next commands from the board to the session.

Run standalone to check what Windows reports (writes cover.jpg next to cwd):
    python -m daemon.media_windows
    python -m daemon.media_windows next
"""

import asyncio
import hashlib
import io
import json
import struct
import sys
from dataclasses import dataclass

from PIL import Image

COVER_SIZE = 200        # px, square — the firmware draws it 1:1 (2.16" layout)
COVER_QUALITY = 80      # JPEG quality; ~5-15 KB at 200x200
TEXT_MAX = 64           # chars per text field (keeps the meta JSON well under 512 B)
TEXT_MAX_BYTES = 135    # UTF-8 bytes per field — firmware MusicData buffers are 136
CHUNK_HEADER = struct.Struct("<HII")  # art_id, offset, total

CMD_PREV = 0x01
CMD_PLAYPAUSE = 0x02
CMD_NEXT = 0x03

PLAYBACK_PLAYING = 4    # GlobalSystemMediaTransportControlsSessionPlaybackStatus.PLAYING


@dataclass
class NowPlaying:
    app: str
    title: str
    artist: str
    album: str
    playing: bool
    position: int       # seconds
    duration: int       # seconds
    thumbnail: bytes | None


def art_id_for(np: NowPlaying) -> int:
    """16-bit id of the cover image (same album → same id, no resend); 0 = no cover."""
    if not np.thumbnail:
        return 0
    h = int.from_bytes(hashlib.sha1(np.thumbnail).digest()[:2], "little")
    return h or 1


def _clip(s: str) -> str:
    """At most TEXT_MAX chars and TEXT_MAX_BYTES of UTF-8 (firmware buffer size),
    cut on a character boundary so the board never gets half a multibyte char."""
    s = s[:TEXT_MAX]
    while len(s.encode()) > TEXT_MAX_BYTES:
        s = s[:-1]
    return s


def meta_payload(np: NowPlaying | None) -> dict:
    """Compact JSON the firmware's music screen consumes. {"m": 0} = nothing playing."""
    if np is None:
        return {"m": 0}
    return {
        "m": 1,
        "t": _clip(np.title),
        "a": _clip(np.artist),
        "al": _clip(np.album),
        "p": 1 if np.playing else 0,
        "pos": np.position,
        "dur": np.duration,
        "art": art_id_for(np),
    }


def encode_meta(payload: dict, max_bytes: int) -> bytes:
    """UTF-8 JSON that fits one BLE write: trims album, then artist, then title."""
    p = dict(payload)
    while True:
        data = json.dumps(p, separators=(",", ":"), ensure_ascii=False).encode()
        if len(data) <= max_bytes:
            return data
        for key in ("al", "a", "t"):
            if p.get(key):
                p[key] = p[key][:-max(1, (len(data) - max_bytes) // 2)]
                break
        else:
            return data  # nothing left to trim; the caller's write will fail loudly


def make_cover_jpeg(raw: bytes, size: int = COVER_SIZE, quality: int = COVER_QUALITY) -> bytes:
    """Square-crop, resize and re-encode as baseline JPEG (what TJpgDec accepts)."""
    img = Image.open(io.BytesIO(raw)).convert("RGB")
    w, h = img.size
    side = min(w, h)
    img = img.crop(((w - side) // 2, (h - side) // 2, (w + side) // 2, (h + side) // 2))
    img = img.resize((size, size), Image.LANCZOS)
    out = io.BytesIO()
    img.save(out, "JPEG", quality=quality, optimize=True, progressive=False)
    return out.getvalue()


def art_chunks(art_id: int, jpeg: bytes, chunk_payload: int) -> list[bytes]:
    """Split a JPEG into writes of [art_id u16][offset u32][total u32][data...]."""
    total = len(jpeg)
    return [
        CHUNK_HEADER.pack(art_id, off, total) + jpeg[off:off + chunk_payload]
        for off in range(0, total, chunk_payload)
    ]


# ---------------------------------------------------------------------------
# WinRT side — imported lazily so the pure helpers above stay testable anywhere.
# ---------------------------------------------------------------------------

async def _manager():
    from winrt.windows.media.control import (
        GlobalSystemMediaTransportControlsSessionManager as MediaManager,
    )
    return await MediaManager.request_async()


async def _read_thumbnail(ref) -> bytes:
    from winrt.windows.storage.streams import Buffer, DataReader, InputStreamOptions
    stream = await ref.open_read_async()
    buf = Buffer(stream.size)
    await stream.read_async(buf, stream.size, InputStreamOptions.READ_AHEAD)
    data = bytearray(buf.length)
    DataReader.from_buffer(buf).read_bytes(data)
    return bytes(data)


async def read_now_playing(with_thumbnail: bool = True) -> NowPlaying | None:
    session = (await _manager()).get_current_session()
    if session is None:
        return None
    props = await session.try_get_media_properties_async()
    if not props.title:
        return None
    timeline = session.get_timeline_properties()
    thumb = None
    if with_thumbnail and props.thumbnail is not None:
        try:
            thumb = await _read_thumbnail(props.thumbnail)
        except OSError:
            thumb = None
    return NowPlaying(
        app=session.source_app_user_model_id,
        title=props.title,
        artist=props.artist,
        album=props.album_title,
        playing=session.get_playback_info().playback_status == PLAYBACK_PLAYING,
        position=int(timeline.position.total_seconds()),
        duration=int(timeline.end_time.total_seconds()),
        thumbnail=thumb,
    )


async def send_command(cmd: int) -> bool:
    session = (await _manager()).get_current_session()
    if session is None:
        return False
    if cmd == CMD_PREV:
        return await session.try_skip_previous_async()
    if cmd == CMD_PLAYPAUSE:
        return await session.try_toggle_play_pause_async()
    if cmd == CMD_NEXT:
        return await session.try_skip_next_async()
    return False


async def _main(argv: list[str]) -> None:
    cmds = {"prev": CMD_PREV, "play": CMD_PLAYPAUSE, "next": CMD_NEXT}
    if argv and argv[0] in cmds:
        print("Comando:", argv[0], "->", await send_command(cmds[argv[0]]))
        await asyncio.sleep(1.0)
    np = await read_now_playing()
    if np is None:
        print("Nada reproduciendose")
        return
    print(meta_payload(np))
    if np.thumbnail:
        jpeg = make_cover_jpeg(np.thumbnail)
        with open("cover.jpg", "wb") as f:
            f.write(jpeg)
        n = len(art_chunks(1, jpeg, 244 - 3 - CHUNK_HEADER.size))
        print(f"Caratula: {len(jpeg)} bytes JPEG, {n} trozos BLE -> cover.jpg")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    asyncio.run(_main(sys.argv[1:]))
