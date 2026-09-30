"""Create the desktop icon from the same teal ƒ mark used in the UI."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


root = Path(__file__).resolve().parents[1]
icon_dir = root / "app" / "build"
icon_dir.mkdir(parents=True, exist_ok=True)
image = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((32, 32, 480, 480), radius=106, fill="#38c2b1")
font_file = Path("C:/Windows/Fonts/georgiai.ttf")
if not font_file.exists():
    font_file = Path("C:/Windows/Fonts/ariali.ttf")
font = ImageFont.truetype(str(font_file), 370)
bbox = draw.textbbox((0, 0), "ƒ", font=font)
width, height = bbox[2] - bbox[0], bbox[3] - bbox[1]
draw.text(((512 - width) / 2 - bbox[0] - 7, (512 - height) / 2 - bbox[1] - 3), "ƒ", font=font, fill="#102335")
image.save(icon_dir / "icon.png")
image.save(icon_dir / "icon.ico", format="ICO", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
