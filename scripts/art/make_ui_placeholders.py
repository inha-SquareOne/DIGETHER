"""프런트엔드 UI 자리표시 텍스처(PNG)를 만든다. 아트가 나오면 같은 이름의 PNG로 바꾸고 다시 가져온다.

실행(시스템 Python + Pillow):
  python scripts/art/make_ui_placeholders.py

출력: DIGETHER/SourceArt/Frontend/UI/*.png
- 회색 채움 + 흰 테두리의 둥근 사각형. 버튼·패널 스타일의 Tint 색을 곱해 쓰므로
  테두리는 Tint 색 그대로, 채움은 더 어둡게 보인다. 9-slice(Box) 여백은 0.25.
"""

from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 64
SCALE = 4  # 가장자리를 부드럽게 하려고 크게 그린 뒤 줄인다.
OUT_DIR = Path(__file__).resolve().parents[2] / "DIGETHER" / "SourceArt" / "Frontend" / "UI"

# 이름: (모서리 반경, 테두리 두께, 채움 밝기 0~255, 채움 불투명도 0~255)
TEXTURES = {
    "T_UI_Button": (10, 2, 72, 255),
    "T_UI_Panel": (12, 2, 40, 232),
    "T_UI_Row": (8, 1, 64, 242),
    "T_UI_Frame": (8, 2, 0, 0),
}


def draw(radius: int, border: int, fill_value: int, fill_alpha: int) -> Image.Image:
    big = SIZE * SCALE
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    painter = ImageDraw.Draw(image)
    box = (0, 0, big - 1, big - 1)
    painter.rounded_rectangle(box, radius=radius * SCALE, fill=(255, 255, 255, 255))
    inset = border * SCALE
    inner = (inset, inset, big - 1 - inset, big - 1 - inset)
    inner_radius = max(0, (radius - border) * SCALE)
    painter.rounded_rectangle(inner, radius=inner_radius, fill=(fill_value, fill_value, fill_value, fill_alpha))
    return image.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, params in TEXTURES.items():
        path = OUT_DIR / f"{name}.png"
        draw(*params).save(path)
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
