#!/usr/bin/env python3
"""Unit tests for the pure helpers in daemon/media_windows.py (no WinRT needed).

Run: python -m pytest daemon/tests/test_windows_media.py -x -q
"""
import io
import json

from PIL import Image

from daemon.media_windows import (
    CHUNK_HEADER,
    NowPlaying,
    art_chunks,
    art_id_for,
    encode_meta,
    make_cover_jpeg,
    meta_payload,
)


def _np(**kw):
    base = dict(app="Spotify.exe", title="Instrucciones", artist="Arde Bogotá",
                album="MCGS", playing=True, position=47, duration=206, thumbnail=b"x")
    base.update(kw)
    return NowPlaying(**base)


def test_meta_payload_nothing_playing():
    assert meta_payload(None) == {"m": 0}


def test_meta_payload_fields_and_truncation():
    p = meta_payload(_np(title="x" * 200))
    assert p["m"] == 1 and p["p"] == 1 and p["pos"] == 47 and p["dur"] == 206
    assert len(p["t"]) == 64
    assert p["a"] == "Arde Bogotá"


def test_meta_payload_clips_multibyte_text_on_char_boundary():
    p = meta_payload(_np(artist="á" * 100))   # 2 bytes each in UTF-8
    assert len(p["a"].encode()) <= 135
    p["a"].encode().decode()                    # still valid UTF-8


def test_encode_meta_fits_budget_trimming_album_first():
    p = meta_payload(_np(album="x" * 64, artist="y" * 64, title="z" * 64))
    data = encode_meta(p, 200)
    assert len(data) <= 200
    out = json.loads(data)
    assert out["t"] == "z" * 64            # title kept whole while album/artist absorb the cut
    assert len(out["al"]) < 64


def test_encode_meta_keeps_accents_raw():
    assert "Bogotá".encode() in encode_meta(meta_payload(_np()), 500)


def test_art_id_follows_cover_image_and_zero_without_cover():
    assert art_id_for(_np(thumbnail=None)) == 0
    a, b = art_id_for(_np()), art_id_for(_np(thumbnail=b"otra"))
    assert a != 0 and b != 0 and a != b
    assert art_id_for(_np(title="Otra", position=100)) == a  # same album art → same id


def test_art_chunks_reassemble():
    data = bytes(range(256)) * 40
    chunks = art_chunks(7, data, 231)
    out = bytearray()
    for c in chunks:
        art_id, off, total = CHUNK_HEADER.unpack_from(c)
        assert art_id == 7 and total == len(data) and off == len(out)
        out += c[CHUNK_HEADER.size:]
    assert bytes(out) == data


def test_make_cover_jpeg_square_baseline():
    src = io.BytesIO()
    Image.new("RGBA", (300, 200), (200, 30, 30, 255)).save(src, "PNG")
    jpeg = make_cover_jpeg(src.getvalue(), size=120)
    img = Image.open(io.BytesIO(jpeg))
    assert img.format == "JPEG" and img.size == (120, 120)
    assert not img.info.get("progressive")
