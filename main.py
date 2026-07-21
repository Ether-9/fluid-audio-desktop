# main.py - Entry point for Fluid Audio
import sys
import os

# Add current directory to path
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from mainwindow import MainWindow
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import Qt

if __name__ == "__main__":
    # Enable high DPI scaling
    QApplication.setHighDpiScaleFactorRoundingPolicy(
        Qt.HighDpiScaleFactorRoundingPolicy.PassThrough
    )
    
    app = QApplication(sys.argv)
    app.setApplicationName("Fluid Audio")
    app.setOrganizationName("Fluid Audio")
    
    window = MainWindow()
    window.show()
    
    sys.exit(app.exec())
