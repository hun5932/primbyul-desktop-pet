#!/usr/bin/env python3
"""Build stable, shared Primbyul v1.5 animation assets.

The v1.4 keyframes remain the visual source of truth.  This builder:
* keeps a uniform scale inside each action,
* anchors stationary actions to the same paws/body point,
* centers large-motion actions without global zooming,
* gives every frame safe transparent padding, and
* exports individual 512 px PNGs shared by Windows and macOS.
"""

from __future__ import annotations

from io import BytesIO
from pathlib import Path
from typing import Iterable

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "assets" / "frames"
SOURCE = ROOT / "assets" / "source-keyframes"
ICONS = ROOT / "assets" / "icons"
APPEARANCES = ("adult", "puppy")
STATES = ("idle", "run", "wag", "play", "watch", "sit")
CELL = 512
SAFE_MARGIN = 28


def remove_far_artifacts(image: Image.Image) -> Image.Image:
    """Remove isolated matte flecks while preserving nearby fur wisps."""
    rgba = np.array(image, copy=True)
    mask = rgba[:, :, 3] > 12
    height, width = mask.shape
    visited = np.zeros_like(mask, dtype=bool)
    components: list[list[tuple[int, int]]] = []
    for y in range(height):
        for x in range(width):
            if not mask[y, x] or visited[y, x]:
                continue
            component: list[tuple[int, int]] = []
            stack = [(x, y)]
            visited[y, x] = True
            while stack:
                px, py = stack.pop()
                component.append((px, py))
                for nx, ny in (
                    (px - 1, py), (px + 1, py),
                    (px, py - 1), (px, py + 1),
                ):
                    if (
                        0 <= nx < width and 0 <= ny < height
                        and mask[ny, nx] and not visited[ny, nx]
                    ):
                        visited[ny, nx] = True
                        stack.append((nx, ny))
            components.append(component)
    if not components:
        return image
    main = max(components, key=len)
    xs = [point[0] for point in main]
    ys = [point[1] for point in main]
    margin = 16
    safe = (
        max(0, min(xs) - margin),
        max(0, min(ys) - margin),
        min(width - 1, max(xs) + margin),
        min(height - 1, max(ys) + margin),
    )
    for component in components:
        if component is main:
            continue
        if all(
            not (safe[0] <= x <= safe[2] and safe[1] <= y <= safe[3])
            for x, y in component
        ):
            for x, y in component:
                rgba[y, x] = 0
    # Also clear the very low-alpha halo left around a removed fleck.  The
    # 16-pixel expansion keeps legitimate antialiased fur around the subject.
    rgba[:safe[1], :, :] = 0
    rgba[safe[3] + 1:, :, :] = 0
    rgba[:, :safe[0], :] = 0
    rgba[:, safe[2] + 1:, :] = 0
    return Image.fromarray(rgba, "RGBA")


def alpha_bbox(image: Image.Image) -> tuple[int, int, int, int]:
    alpha = np.asarray(image.getchannel("A"))
    ys, xs = np.nonzero(alpha > 12)
    if len(xs) == 0:
        raise ValueError("empty animation frame")
    return int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1


def scaled_frame(image: Image.Image, scale: float) -> Image.Image:
    if abs(scale - 1.0) < 1e-6:
        return image.copy()
    width = max(1, round(image.width * scale))
    height = max(1, round(image.height * scale))
    return image.resize((width, height), Image.Resampling.LANCZOS)


def opaque_points(image: Image.Image) -> tuple[np.ndarray, np.ndarray]:
    alpha = np.asarray(image.getchannel("A"))
    return np.nonzero(alpha > 32)


def standing_anchor(image: Image.Image) -> tuple[float, float]:
    """Anchor standing poses at the visible paw line, excluding most tail mass."""
    ys, xs = opaque_points(image)
    ymax = int(ys.max())
    foot_mask = ys >= ymax - 18
    return float(xs[foot_mask].mean()), float(ymax)


def seated_anchor(image: Image.Image) -> tuple[float, float]:
    """Anchor seated poses at the torso median and ground line."""
    ys, xs = opaque_points(image)
    ymin, ymax = int(ys.min()), int(ys.max())
    body_mask = (ys >= ymin + 0.30 * (ymax - ymin)) & (
        ys <= ymin + 0.78 * (ymax - ymin)
    )
    return float(np.median(xs[body_mask])), float(ymax)


def centered_anchor(image: Image.Image) -> tuple[float, float]:
    left, top, right, bottom = alpha_bbox(image)
    return (left + right) / 2.0, (top + bottom) / 2.0


def feasible_target(
    images: Iterable[Image.Image],
    anchors: Iterable[tuple[float, float]],
    preferred: tuple[float, float],
) -> tuple[float, float]:
    """Clamp one common target so every translated frame stays inside the cell."""
    x_low = float("-inf")
    x_high = float("inf")
    y_low = float("-inf")
    y_high = float("inf")
    for image, (anchor_x, anchor_y) in zip(images, anchors):
        left, top, right, bottom = alpha_bbox(image)
        x_low = max(x_low, SAFE_MARGIN + anchor_x - left)
        x_high = min(x_high, CELL - SAFE_MARGIN + anchor_x - right)
        y_low = max(y_low, SAFE_MARGIN + anchor_y - top)
        y_high = min(y_high, CELL - SAFE_MARGIN + anchor_y - bottom)
    target_x = min(max(preferred[0], x_low), x_high)
    target_y = min(max(preferred[1], y_low), y_high)
    return target_x, target_y


def place(
    image: Image.Image,
    anchor: tuple[float, float],
    target: tuple[float, float],
) -> Image.Image:
    canvas = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    x = round(target[0] - anchor[0])
    y = round(target[1] - anchor[1])
    canvas.alpha_composite(image, (x, y))
    return canvas


def save_png(image: Image.Image, path: Path) -> None:
    """Encode completely in memory, then commit one complete PNG payload."""
    payload = BytesIO()
    image.save(payload, format="PNG", compress_level=6)
    path.write_bytes(payload.getvalue())


def source_frames(appearance: str, state: str) -> list[Image.Image]:
    directory = SOURCE / appearance / state
    return [
        remove_far_artifacts(
            Image.open(directory / f"key-{index}.png").convert("RGBA")
        )
        for index in range(4)
    ]


def build_state(appearance: str, state: str) -> None:
    originals = source_frames(appearance, state)
    bboxes = [alpha_bbox(image) for image in originals]
    max_width = max(right - left for left, _, right, _ in bboxes)
    max_height = max(bottom - top for _, top, _, bottom in bboxes)
    scale = min(1.0, (CELL - 2 * SAFE_MARGIN) / max(max_width, max_height))

    # Play frames touched a v1.4 cell boundary.  A little extra common scale
    # creates real padding without changing proportions between keyframes.
    if state == "play":
        scale = min(scale, 0.90)

    images = [scaled_frame(image, scale) for image in originals]
    if state in {"idle", "wag", "watch"}:
        anchors = [standing_anchor(image) for image in images]
        preferred = (CELL * 0.58, CELL - SAFE_MARGIN)
    elif state == "sit":
        anchors = [seated_anchor(image) for image in images]
        preferred = (CELL * 0.48, CELL - SAFE_MARGIN)
    else:
        anchors = [centered_anchor(image) for image in images]
        preferred = (CELL / 2, CELL / 2)

    target = feasible_target(images, anchors, preferred)
    destination = OUTPUT / appearance / state
    destination.mkdir(parents=True, exist_ok=True)
    for index, (image, anchor) in enumerate(zip(images, anchors)):
        frame = place(image, anchor, target)
        save_png(frame, destination / f"{index}.png")


def build_mirrored_run(appearance: str) -> None:
    """macOS uses pre-mirrored PNGs so it never resamples a live frame."""
    source = OUTPUT / appearance / "run"
    destination = OUTPUT / appearance / "run-left"
    destination.mkdir(parents=True, exist_ok=True)
    for index in range(4):
        frame = Image.open(source / f"{index}.png").convert("RGBA")
        save_png(
            frame.transpose(Image.Transpose.FLIP_LEFT_RIGHT),
            destination / f"{index}.png",
        )


def build_icon() -> None:
    source = Image.open(
        ICONS / "primbyul-icon-master.png"
    ).convert("RGBA")
    bbox = alpha_bbox(source)
    subject = source.crop(bbox)
    subject.thumbnail((900, 900), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    x = (1024 - subject.width) // 2
    y = max(40, (1024 - subject.height) // 2)
    canvas.alpha_composite(subject, (x, y))
    save_png(canvas, ICONS / "primbyul-icon-1024.png")
    canvas.save(
        ICONS / "Primbyul.ico",
        format="ICO",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64),
               (128, 128), (256, 256)],
    )
    status = canvas.resize((64, 64), Image.Resampling.LANCZOS)
    save_png(status, ICONS / "primbyul-status-icon.png")


def validate() -> None:
    for appearance in APPEARANCES:
        for state in STATES:
            for index in range(4):
                path = OUTPUT / appearance / state / f"{index}.png"
                image = Image.open(path).convert("RGBA")
                if image.size != (CELL, CELL):
                    raise ValueError(f"wrong size: {path}")
                alpha = np.asarray(image.getchannel("A"))
                if alpha[0, :].max() or alpha[-1, :].max():
                    raise ValueError(f"vertical clipping: {path}")
                if alpha[:, 0].max() or alpha[:, -1].max():
                    raise ValueError(f"horizontal clipping: {path}")


def main() -> None:
    for appearance in APPEARANCES:
        for state in STATES:
            build_state(appearance, state)
        build_mirrored_run(appearance)
    build_icon()
    validate()
    frame_count = len(list(OUTPUT.rglob("*.png")))
    print(f"Built {frame_count} animation frames")


if __name__ == "__main__":
    main()
