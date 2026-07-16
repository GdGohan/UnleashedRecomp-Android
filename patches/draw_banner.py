#!/usr/bin/env python3
"""
draw_banner.py

Pega uma imagem especifica e escreve nela:
  - um TITULO centralizado horizontalmente, colado no topo
  - um bloco de NOTAS alinhado a esquerda, centralizado verticalmente na imagem

Usa uma fonte .otf (ou .ttf) fornecida pelo usuario.

Uso:
    python3 draw_banner.py \
        --image banner.png \
        --font MinhaFonte.otf \
        --title "UnleashedRecomp v1.2.3" \
        --notes "Fixed crash on boot\nImproved shader cache\nAndroid support added" \
        --output banner_final.png

Cada "\n" (ou linha real) dentro de --notes vira uma linha do bloco de notas.
Linhas muito longas sao quebradas automaticamente para caber na largura da imagem.

Pensado para ser chamado dentro do seu workflow (.yml) no passo de release,
por exemplo logo apos "Select random banner":

    - name: Draw release banner
      run: |
        python3 patches/draw_banner.py \
          --image "${{ env.BANNER_PATH }}" \
          --font patches/fonts/MinhaFonte.otf \
          --title "${{ env.RELEASE_TAG }}" \
          --notes "${{ github.event.inputs.release_notes }}" \
          --output "${{ env.BANNER_PATH }}"
"""

import argparse
import sys
import textwrap
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Escreve titulo e notas em uma imagem usando uma fonte .otf/.ttf")
    p.add_argument("--image", required=True, help="Caminho da imagem de entrada")
    p.add_argument("--font", required=True, help="Caminho da fonte .otf ou .ttf")
    p.add_argument("--title", default="", help="Texto do titulo (centralizado, topo)")
    p.add_argument("--notes", default="", help="Texto das notas (aceita \\n para quebrar linha)")
    p.add_argument("--output", required=True, help="Caminho da imagem de saida")

    p.add_argument("--title-size", type=int, default=None, help="Tamanho da fonte do titulo (default: proporcional a imagem)")
    p.add_argument("--notes-size", type=int, default=None, help="Tamanho da fonte das notas (default: proporcional a imagem)")

    p.add_argument("--title-color", default="#FFFFFF", help="Cor do titulo (default: branco)")
    p.add_argument("--notes-color", default="#FFFFFF", help="Cor das notas (default: branco)")

    p.add_argument("--title-margin-top", type=int, default=None, help="Distancia do topo ate o titulo em px (default: proporcional)")
    p.add_argument("--notes-margin-left", type=int, default=None, help="Distancia da borda esquerda ate as notas em px (default: proporcional)")
    p.add_argument("--notes-margin-right", type=int, default=None, help="Margem direita usada para quebra de linha das notas (default: proporcional)")
    p.add_argument("--line-spacing", type=int, default=8, help="Espaco extra entre linhas das notas, em px")

    p.add_argument("--shadow", action="store_true", help="Adiciona uma sombra leve atras do texto para melhorar legibilidade")

    return p.parse_args()


def load_font(font_path: str, size: int) -> ImageFont.FreeTypeFont:
    try:
        return ImageFont.truetype(font_path, size)
    except OSError as exc:
        sys.exit(f"::error::Nao foi possivel carregar a fonte '{font_path}': {exc}")


def text_size(draw: ImageDraw.ImageDraw, text: str, font: ImageFont.FreeTypeFont) -> tuple[int, int]:
    bbox = draw.textbbox((0, 0), text, font=font)
    return bbox[2] - bbox[0], bbox[3] - bbox[1]


def wrap_notes(draw: ImageDraw.ImageDraw, notes: str, font: ImageFont.FreeTypeFont, max_width: int) -> list[str]:
    """Quebra as notas em linhas que cabem em max_width, preservando quebras manuais."""
    raw_lines = notes.replace("\\n", "\n").splitlines() or [""]
    wrapped: list[str] = []

    for raw_line in raw_lines:
        if raw_line.strip() == "":
            wrapped.append("")
            continue

        words = raw_line.split()
        current = ""
        for word in words:
            candidate = f"{current} {word}".strip()
            w, _ = text_size(draw, candidate, font)
            if w <= max_width or not current:
                current = candidate
            else:
                wrapped.append(current)
                current = word
        if current:
            wrapped.append(current)

    return wrapped


def draw_text_with_optional_shadow(
    draw: ImageDraw.ImageDraw,
    xy: tuple[float, float],
    text: str,
    font: ImageFont.FreeTypeFont,
    fill: str,
    shadow: bool,
) -> None:
    if shadow and text:
        x, y = xy
        offset = max(1, font.size // 40)
        draw.text((x + offset, y + offset), text, font=font, fill="#000000AA")
    draw.text(xy, text, font=font, fill=fill)


def main() -> None:
    args = parse_args()

    image_path = Path(args.image)
    if not image_path.is_file():
        sys.exit(f"::error::Imagem nao encontrada: {image_path}")

    image = Image.open(image_path).convert("RGBA")
    draw = ImageDraw.Draw(image)
    width, height = image.size

    title_size = args.title_size or max(18, width // 20)
    notes_size = args.notes_size or max(14, width // 34)
    title_margin_top = args.title_margin_top if args.title_margin_top is not None else height // 20
    notes_margin_left = args.notes_margin_left if args.notes_margin_left is not None else width // 20
    notes_margin_right = args.notes_margin_right if args.notes_margin_right is not None else width // 20

    title_font = load_font(args.font, title_size)
    notes_font = load_font(args.font, notes_size)

    # --- Titulo: centralizado horizontalmente, colado no topo ---
    if args.title.strip():
        title_w, title_h = text_size(draw, args.title, title_font)
        title_x = (width - title_w) / 2
        title_y = title_margin_top
        draw_text_with_optional_shadow(
            draw, (title_x, title_y), args.title, title_font, args.title_color, args.shadow
        )

    # --- Notas: alinhadas a esquerda, bloco centralizado verticalmente ---
    if args.notes.strip():
        max_notes_width = width - notes_margin_left - notes_margin_right
        lines = wrap_notes(draw, args.notes, notes_font, max_notes_width)

        line_heights = []
        for line in lines:
            _, h = text_size(draw, line if line else " ", notes_font)
            line_heights.append(h)

        total_height = sum(line_heights) + args.line_spacing * (len(lines) - 1 if lines else 0)
        start_y = (height - total_height) / 2

        y = start_y
        for line, h in zip(lines, line_heights):
            draw_text_with_optional_shadow(
                draw, (notes_margin_left, y), line, notes_font, args.notes_color, args.shadow
            )
            y += h + args.line_spacing

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    if output_path.suffix.lower() in (".jpg", ".jpeg"):
        image.convert("RGB").save(output_path, quality=95)
    else:
        image.save(output_path)

    print(f"Banner salvo em: {output_path}")


if __name__ == "__main__":
    main()
