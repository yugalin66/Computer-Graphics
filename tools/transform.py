#!/usr/bin/env python3
"""Convert a video to H.264/AAC MP4 using FFmpeg.

Examples:
    python3 tools/transform.py --input hw1/demo.webm
    python3 tools/transform.py --input hw1/demo.webm --output hw1/demo.mp4

Requires FFmpeg on PATH, or the optional imageio-ffmpeg Python package.
"""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def find_ffmpeg(override):
    if override:
        executable = shutil.which(override)
        if executable:
            return executable
        raise ValueError(f"找不到指定的 FFmpeg：{override}")
    executable = shutil.which("ffmpeg")
    if executable:
        return executable
    # Optional local dependencies keep the system Python environment unchanged.
    local_dependencies = Path(__file__).resolve().parent / ".deps"
    if local_dependencies.is_dir():
        sys.path.insert(0, str(local_dependencies))
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except (ImportError, RuntimeError) as error:
        raise ValueError(
            "找不到 FFmpeg。請安裝 ffmpeg，或執行 "
            "python3 -m pip install imageio-ffmpeg，或用 --ffmpeg 指定執行檔。"
        ) from error


def main():
    parser = argparse.ArgumentParser(description="將 WebM 影片轉成 MP4，保留原始影片。")
    parser.add_argument("--input", required=True, type=Path, help="輸入影片路徑")
    parser.add_argument("--output", type=Path, help="輸出路徑；預設為輸入路徑換成 .mp4")
    parser.add_argument("--overwrite", action="store_true", help="允許覆蓋已有的輸出檔")
    parser.add_argument("--ffmpeg", help="指定 FFmpeg 執行檔路徑")
    args = parser.parse_args()
    source = args.input.expanduser().resolve()
    destination = (args.output.expanduser() if args.output else source.with_suffix(".mp4")).resolve()
    temporary = None
    try:
        if not source.is_file():
            raise ValueError(f"輸入檔案不存在：{source}")
        if source == destination or (destination.exists() and os.path.samefile(source, destination)):
            raise ValueError("輸入和輸出不能是同一個檔案。")
        if destination.suffix.lower() != ".mp4":
            raise ValueError("輸出檔案的副檔名必須是 .mp4。")
        if destination.exists() and not args.overwrite:
            raise ValueError(f"輸出檔案已存在：{destination}；若要覆蓋，請加上 --overwrite。")
        if destination.exists() and not destination.is_file():
            raise ValueError(f"輸出路徑不是檔案：{destination}")
        ffmpeg = find_ffmpeg(args.ffmpeg)
        destination.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(prefix=".transform-", suffix=".mp4", dir=destination.parent, delete=False) as stream:
            temporary = Path(stream.name)
        print(f"轉換：{source}\n輸出：{destination}", flush=True)
        subprocess.run([
            ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
            "-i", str(source), "-map", "0:v:0", "-map", "0:a?",
            # H.264 yuv420p needs even dimensions; padding preserves the image.
            "-vf", "pad=ceil(iw/2)*2:ceil(ih/2)*2",
            "-c:v", "libx264", "-crf", "18", "-preset", "medium",
            "-pix_fmt", "yuv420p", "-fps_mode", "vfr",
            "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart",
            str(temporary),
        ], check=True)
        if not args.overwrite and destination.exists():
            raise ValueError("轉換期間輸出檔案已被建立，未覆蓋該檔案。")
        os.replace(temporary, destination)
        temporary = None
        print(f"完成：{destination}")
        return 0
    except KeyboardInterrupt:
        print("\n已取消轉換。", file=sys.stderr)
        return 130
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"轉換失敗：{error}", file=sys.stderr)
        return 1
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


if __name__ == "__main__":
    sys.exit(main())
