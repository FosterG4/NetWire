import os
import sys
from pathlib import Path
from PyQt6.QtGui import QIcon, QPixmap, QPainter
from PyQt6.QtSvg import QSvgRenderer
from PyQt6.QtCore import Qt, QSize

def convert_svg_to_png(svg_path, png_path, size=24):
    """Convert an SVG file to a PNG file with the specified size."""
    # Create a transparent pixmap
    pixmap = QPixmap(size, size)
    pixmap.fill(Qt.GlobalColor.transparent)
    
    # Create a painter and render the SVG onto the pixmap
    painter = QPainter(pixmap)
    renderer = QSvgRenderer(svg_path)
    renderer.render(painter)
    painter.end()
    
    # Save the pixmap as a PNG
    pixmap.save(png_path, "PNG")

def main():
    # Create output directory if it doesn't exist
    output_dir = Path("resources/icons/png")
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Get all SVG files in the icons directory
    svg_files = list(Path("resources/icons").glob("*.svg"))
    
    # Convert each SVG to PNG
    for svg_file in svg_files:
        png_file = output_dir / f"{svg_file.stem}.png"
        print(f"Converting {svg_file} to {png_file}")
        convert_svg_to_png(str(svg_file), str(png_file))
    
    print("Conversion complete!")

if __name__ == "__main__":
    # Initialize QApplication (required for QPixmap)
    from PyQt6.QtWidgets import QApplication
    app = QApplication(sys.argv)
    
    main()
