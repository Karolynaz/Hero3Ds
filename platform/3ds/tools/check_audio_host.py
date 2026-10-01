#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Optional real-decoder host test (requires Python 3, clang, clang++, ffmpeg).

Downloads pinned BSD-licensed Tremor/libogg sources into a temporary directory.
Simulates libctru/NDSP, but decodes a real generated OGG through real Tremor.
Does not replace native ARM compilation or hardware audio verification.
"""
import argparse
import math
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import tarfile
import tempfile
import urllib.request
import wave

SOURCES = {
    "tremor": ("sezero/tremor", "18a673fc97d95dffff3ccdeea7d493b2a6ab4481"),
    "ogg": ("xiph/ogg", "06a5e0262cdc28aa4ae6797627a783b5010440f0"),
}


def run(arguments):
    subprocess.run([str(value) for value in arguments], check=True)


def fetch_source(work, name, repository, revision):
    archive = work / (name + ".tar.gz")
    url = f"https://codeload.github.com/{repository}/tar.gz/{revision}"
    request = urllib.request.Request(url, headers={"User-Agent": "Hero3DS-audio-tests"})
    with urllib.request.urlopen(request, timeout=60) as response, archive.open("wb") as output:
        shutil.copyfileobj(response, output)
    target = work / name
    target.mkdir()
    with tarfile.open(archive) as source:
        for member in source.getmembers():
            parts = PurePosixPath(member.name).parts
            if len(parts) <= 1:
                continue
            if member.name.startswith("/") or ".." in parts or not (member.isfile() or member.isdir()):
                raise RuntimeError("Unexpected archive member: " + member.name)
            destination = target.joinpath(*parts[1:])
            if member.isdir():
                destination.mkdir(parents=True, exist_ok=True)
            else:
                destination.parent.mkdir(parents=True, exist_ok=True)
                with source.extractfile(member) as data, destination.open("wb") as output:
                    shutil.copyfileobj(data, output)
    return target


def verify(work, clang, clangxx, ffmpeg):
    root = Path(__file__).resolve().parents[3]
    tremor = fetch_source(work, "tremor", *SOURCES["tremor"])
    ogg = fetch_source(work, "ogg", *SOURCES["ogg"])
    (ogg / "include/ogg/config_types.h").write_text("""#include <stdint.h>
typedef int16_t ogg_int16_t; typedef uint16_t ogg_uint16_t;
typedef int32_t ogg_int32_t; typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t; typedef uint64_t ogg_uint64_t;
""")
    include = work / "include"
    (include / "tremor").mkdir(parents=True)
    for header in ("ivorbisfile.h", "ivorbiscodec.h"):
        shutil.copy2(tremor / header, include / "tremor" / header)
    objects = []
    source_files = [ogg / "src/bitwise.c", ogg / "src/framing.c"]
    source_files += [tremor / (name + ".c") for name in
                    "mdct block window synthesis info floor1 floor0 vorbisfile res012 mapping0 registry codebook sharedbook".split()]
    for source in source_files:
        output = work / (source.stem + ".o")
        # Tremor's fixed-point arithmetic uses signed shifts. Suppress this
        # sanitizer category only for the third-party decoder, never our code.
        run([clang, "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize=shift",
             "-DVAR_ARRAYS", "-I" + str(ogg / "include"), "-I" + str(tremor),
             "-c", source, "-o", output])
        objects.append(output)
    wav = work / "sample.wav"
    with wave.open(str(wav), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(22050)
        for frame in range(22050):
            sample = int(8000 * math.sin(2 * math.pi * 440 * frame / 22050))
            output.writeframesraw(struct.pack("<hh", sample, sample))
    fixture = work / "sample.ogg"
    run([ffmpeg, "-hide_banner", "-loglevel", "error", "-i", wav,
         "-c:a", "vorbis", "-strict", "experimental", "-y", fixture])
    binary = work / "audio-music-test"
    run([clangxx, "-std=c++17", "-O1", "-g", "-fsanitize=address,undefined",
         "-DTARGET_NINTENDO_3DS", "-I" + str(root / "platform/3ds/tests/audio-mocks"),
         "-I" + str(include), "-I" + str(ogg / "include"), "-I" + str(root / "src/engine"),
         "-Wall", "-Wextra", "-Werror", root / "platform/3ds/tests/audio_music.cpp",
         *objects, "-o", binary])
    for _ in range(5):
        run([binary, fixture])
    print("PASS: real OGG decode, EOF/loop/resume, mute, failure cleanup, shutdown, 48 KiB PCM bound (5 sanitizer runs).")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", type=Path, help="Keep downloaded source, build objects and fixtures here.")
    args = parser.parse_args()
    programs = [shutil.which(name) for name in ("clang", "clang++", "ffmpeg")]
    if not all(programs):
        parser.error("Optional test dependencies missing: clang, clang++, and ffmpeg are required.")
    if args.work_dir:
        args.work_dir.mkdir(parents=True, exist_ok=True)
        verify(args.work_dir.resolve(), *programs)
    else:
        with tempfile.TemporaryDirectory(prefix="hero3ds-audio-") as directory:
            verify(Path(directory), *programs)


if __name__ == "__main__":
    main()
