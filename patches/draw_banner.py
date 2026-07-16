#!/usr/bin/env python3
"""
draw_banner.py

Pega uma imagem base e escreve nela:
  - um TITULO centralizado horizontalmente, colado no topo
  - um bloco de NOTAS alinhado a esquerda, centralizado verticalmente
    na area disponivel abaixo do titulo

Usa uma fonte .otf (ou .ttf) fornecida pelo usuario.

COMPORTAMENTO DE OVERFLOW:
  1. As notas sao quebradas em linhas para caber na largura da imagem.
  2. Se o numero de linhas passar de --max-lines-before-shrink (default 10),
     a fonte das notas eh reduzida UMA UNICA VEZ (fator --shrink-factor,
     nunca menor que --min-notes-size) e as linhas sao recalculadas.
  3. Se, mesmo depois do shrink, as linhas ainda nao couberem na altura
     disponivel da imagem, o conteudo eh dividido em PAGINAS e uma imagem
     e salva para cada pagina (banner_1.png, banner_2.png, ...).
     Se so existir uma pagina, o nome de saida original eh mantido.

Uso:
    python3 draw_banner.py \
        --image banner.png \
        --font MinhaFonte.otf \
        --title "UnleashedRecomp v1.2.3" \
        --notes "Fixed crash on boot\nImproved shader cache\nAndroid support added" \
        --output banner_final.png

Cada "\n" (ou linha real) dentro de --notes vira uma linha do bloco de notas.
Linhas muito longas sao quebradas automaticamente para caber na largura da imagem.

O script imprime, no final, um caminho por linha com todas as imagens geradas
(1 ou mais), para o workflow poder capturar e usar todas.
"""

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Escreve titulo e notas em uma imagem usando uma fonte .otf/.ttf")
    p.add_argument("--image", required=True, help="Caminho da imagem de entrada")
    p.add_argument("--font", required=True, help="Caminho da fonte .otf ou .ttf")
    p.add_argument("--title", default="", help="Texto do titulo (centralizado, topo)")
    p.add_argument("--notes", default="", help="Texto das notas (aceita \\n para quebrar linha)")
    p.add_argument("--output", required=True, help="Caminho da imagem de saida (base, sem sufixo de pagina)")

    p.add_argument("--title-size", type=int, default=None, help="Tamanho da fonte do titulo (default: proporcional a imagem)")
    p.add_argument("--notes-size", type=int, default=None, help="Tamanho da fonte das notas (default: proporcional a imagem)")

    p.add_argument("--title-color", default="#FFFFFF", help="Cor do titulo (default: branco)")
    p.add_argument("--notes-color", default="#FFFFFF", help="Cor das notas (default: branco)")

    p.add_argument("--title-margin-top", type=int, default=None, help="Distancia do topo ate o titulo em px (default: proporcional)")
    p.add_argument("--notes-margin-left", type=int, default=None, help="Distancia da borda esquerda ate as notas em px (default: proporcional)")
    p.add_argument("--notes-margin-right", type=int, default=None, help="Margem direita usada para quebra de linha das notas (default: proporcional)")
    p.add_argument("--notes-margin-bottom", type=int, default=None, help="Margem inferior reservada para as notas em px (default: proporcional)")
    p.add_argument("--line-spacing", type=int, default=8, help="Espaco extra entre linhas das notas, em px")

    p.add_argument("--max-lines-before-shrink", type=int, default=10, help="Se o numero de linhas passar disso, reduz a fonte das notas uma vez (default: 10)")
    p.add_argument("--shrink-factor", type=float, default=0.75, help="Fator de reducao aplicado uma unica vez a fonte das notas (default: 0.75)")
    p.add_argument("--min-notes-size", type=int, default=10, help="Tamanho minimo permitido para a fonte das notas apos o shrink (default: 10)")

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


def line_heights_for(draw: ImageDraw.ImageDraw, lines: list[str], font: ImageFont.FreeTypeFont) -> list[int]:
    return [text_size(draw, line if line else " ", font)[1] for line in lines]


def block_height(heights: list[int], line_spacing: int) -> int:
    if not heights:
        return 0
    return sum(heights) + line_spacing * (len(heights) - 1)


def paginate_lines(
    lines: list[str],
    heights: list[int],
    available_height: int,
    line_spacing: int,
) -> list[list[str]]:
    """Divide as linhas em paginas que cabem em available_height cada."""
    pages: list[list[str]] = []
    current_lines: list[str] = []
    current_height = 0

    for line, h in zip(lines, heights):
        added = h if not current_lines else h + line_spacing
        if current_lines and current_height + added > available_height:
            pages.append(current_lines)
            current_lines = [line]
            current_height = h
        else:
            current_lines.append(line)
            current_height += added

    if current_lines:
        pages.append(current_lines)

    return pages or [[]]


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


def render_page(
    base_image: Image.Image,
    title_text: str,
    title_font: ImageFont.FreeTypeFont,
    title_color: str,
    title_margin_top: int,
    notes_lines: list[str],
    notes_font: ImageFont.FreeTypeFont,
    notes_color: str,
    notes_margin_left: int,
    notes_area_top: int,
    notes_area_bottom: int,
    line_spacing: int,
    shadow: bool,
) -> Image.Image:
    image = base_image.copy()
    draw = ImageDraw.Draw(image)
    width, _ = image.size

    if title_text.strip():
        title_w, _ = text_size(draw, title_text, title_font)
        title_x = (width - title_w) / 2
        draw_text_with_optional_shadow(
            draw, (title_x, title_margin_top), title_text, title_font, title_color, shadow
        )

    if notes_lines:
        heights = line_heights_for(draw, notes_lines, notes_font)
        total_height = block_height(heights, line_spacing)
        available = notes_area_bottom - notes_area_top
        start_y = notes_area_top + max(0, (available - total_height) / 2)

        y = start_y
        for line, h in zip(notes_lines, heights):
            draw_text_with_optional_shadow(
                draw, (notes_margin_left, y), line, notes_font, notes_color, shadow
            )
            y += h + line_spacing

    return image


def save_image(image: Image.Image, output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if output_path.suffix.lower() in (".jpg", ".jpeg"):
        image.convert("RGB").save(output_path, quality=95)
    else:
        image.save(output_path)


def main() -> None:
    args = parse_args()

    image_path = Path(args.image)
    if not image_path.is_file():
        sys.exit(f"::error::Imagem nao encontrada: {image_path}")

    base_image = Image.open(image_path).convert("RGBA")
    measure_draw = ImageDraw.Draw(base_image.copy())
    width, height = base_image.size

    title_size = args.title_size or max(18, width // 20)
    notes_size = args.notes_size or max(14, width // 34)
    title_margin_top = args.title_margin_top if args.title_margin_top is not None else height // 20
    notes_margin_left = args.notes_margin_left if args.notes_margin_left is not None else width // 20
    notes_margin_right = args.notes_margin_right if args.notes_margin_right is not None else width // 20
    notes_margin_bottom = args.notes_margin_bottom if args.notes_margin_bottom is not None else height // 20

    title_font = load_font(args.font, title_size)

    # --- Area reservada para o titulo, para as notas nao sobreporem ---
    if args.title.strip():
        _, title_h = text_size(measure_draw, args.title, title_font)
        notes_area_top = title_margin_top + title_h + title_margin_top
    else:
        notes_area_top = height // 20

    notes_area_bottom = height - notes_margin_bottom
    available_height = max(0, notes_area_bottom - notes_area_top)
    max_notes_width = width - notes_margin_left - notes_margin_right

    output_paths: list[Path] = []

    if args.notes.strip():
        notes_font = load_font(args.font, notes_size)
        lines = wrap_notes(measure_draw, args.notes, notes_font, max_notes_width)

        # --- Passo 1: se passou do limite de linhas, reduz a fonte UMA VEZ ---
        if len(lines) > args.max_lines_before_shrink:
            shrunk_size = max(args.min_notes_size, int(notes_size * args.shrink_factor))
            if shrunk_size != notes_size:
                notes_size = shrunk_size
                notes_font = load_font(args.font, notes_size)
                lines = wrap_notes(measure_draw, args.notes, notes_font, max_notes_width)

        heights = line_heights_for(measure_draw, lines, notes_font)

        # --- Passo 2: se ainda nao couber na altura disponivel, pagina ---
        if block_height(heights, args.line_spacing) <= available_height:
            pages = [lines]
        else:
            pages = paginate_lines(lines, heights, available_height, args.line_spacing)

        num_pages = len(pages)
        output_path = Path(args.output)

        for i, page_lines in enumerate(pages, start=1):
            page_title = args.title
            if num_pages > 1 and args.title.strip():
                page_title = f"{args.title} ({i}/{num_pages})"

            page_image = render_page(
                base_image=base_image,
                title_text=page_title,
                title_font=title_font,
                title_color=args.title_color,
                title_margin_top=title_margin_top,
                notes_lines=page_lines,
                notes_font=notes_font,
                notes_color=args.notes_color,
                notes_margin_left=notes_margin_left,
                notes_area_top=notes_area_top,
                notes_area_bottom=notes_area_bottom,
                line_spacing=args.line_spacing,
                shadow=args.shadow,
            )

            if num_pages == 1:
                page_output = output_path
            else:
                page_output = output_path.with_name(f"{output_path.stem}_{i}{output_path.suffix}")

            save_image(page_image, page_output)
            output_paths.append(page_output)
    else:
        # Sem notas: so desenha o titulo (se houver) numa unica imagem
        page_image = render_page(
            base_image=base_image,
            title_text=args.title,
            title_font=title_font,
            title_color=args.title_color,
            title_margin_top=title_margin_top,
            notes_lines=[],
            notes_font=load_font(args.font, notes_size),
            notes_color=args.notes_color,
            notes_margin_left=notes_margin_left,
            notes_area_top=notes_area_top,
            notes_area_bottom=notes_area_bottom,
            line_spacing=args.line_spacing,
            shadow=args.shadow,
        )
        output_path = Path(args.output)
        save_image(page_image, output_path)
        output_paths.append(output_path)

    for p in output_paths:
        print(p)


if __name__ == "__main__":
    main()
