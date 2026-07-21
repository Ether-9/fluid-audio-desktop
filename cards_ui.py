from PySide6.QtWidgets import QFrame, QVBoxLayout, QLabel, QPushButton
from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QPixmap, QEnterEvent

class MediaCardWidget(QFrame):
    clicked = Signal(str)  # Emits track path or unique ID when played

    def __init__(self, title: str, artist: str, cover_path: str, media_id: str, parent=None):
        super().__init__(parent)
        self.media_id = media_id
        
        # Track path attribute explicitly required to resolve online streams vs local files.
        self.track_path = media_id  
        
        self.setFixedSize(160, 220)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setObjectName("MediaCard")
        
        # Modern UI Glassmorphic Design Style
        self.setStyleSheet("""
            #MediaCard {
                background-color: rgba(35, 35, 35, 0.45);
                border: 1px solid rgba(255, 255, 255, 0.08);
                border-radius: 12px;
            }
            #MediaCard:hover {
                background-color: rgba(55, 55, 55, 0.6);
                border: 1px solid rgba(255, 255, 255, 0.15);
            }
            #PlayOverlay {
                background-color: #60CDFF;
                border-radius: 20px;
                border: none;
            }
            #PlayOverlay:hover {
                background-color: #0078D4;
                transform: scale(1.1);
            }
        """)

        # Layout Setup
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(6)

        # Image Container (Layers absolute position overlays safely)
        self.img_container = QFrame(self)
        self.img_container.setFixedSize(140, 140)
        self.img_container.setStyleSheet("background: transparent; border-radius: 8px;")
        
        self.img_label = QLabel(self.img_container)
        self.img_label.setGeometry(0, 0, 140, 140)
        self.img_label.setScaledContents(True)
        self.img_label.setStyleSheet("border-radius: 8px;")
        
        # Media Player & Solfeggio Specific Visual Art Renderer
        pixmap = QPixmap(cover_path)
        if pixmap.isNull():
            # Generate ambient color blocks paired to solfeggio hertz bands
            if "174" in title: grad = "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #8b0000, stop:1 #2b0000);"
            elif "285" in title: grad = "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #d35400, stop:1 #2c3e50);"
            elif "396" in title: grad = "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #f39c12, stop:1 #d35400);"
            elif "417" in title: grad = "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #f1c40f, stop:1 #f39c12);"
            elif "432" in title: grad = "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2ecc71, stop:1 #27ae60);"
            elif "528" in title: grad = "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1abc9c, stop:1 #16a085);"
            elif "639" in title: grad = "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3498db, stop:1 #2980b9);"
            elif "741" in title: grad = "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #9b59b6, stop:1 #8e44ad);"
            elif "852" in title: grad = "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7f8c8d, stop:1 #1c2833);"
            elif "963" in title: grad = "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #8e44ad, stop:1 #2c3e50);"
            else: grad = "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2c3e50, stop:1 #000000);"
            
            self.img_label.setStyleSheet(grad + " border-radius: 8px;")
            self.overlay_title = QLabel(title.split(" ")[-2] if len(title.split(" ")) > 1 else title, self.img_container)
            self.overlay_title.setAlignment(Qt.AlignmentFlag.AlignCenter)
            self.overlay_title.setStyleSheet("color: white; font-size: 22px; font-weight: bold; background: transparent;")
            self.overlay_title.setGeometry(0, 0, 140, 140)
        else:
            self.img_label.setPixmap(pixmap)

        # Hover Play Overlay Button
        self.play_btn = QPushButton("▶", self.img_container)
        self.play_btn.setObjectName("PlayOverlay")
        self.play_btn.setGeometry(50, 50, 40, 40)
        self.play_btn.setStyleSheet("color: black; font-size: 16px; font-weight: bold;")
        self.play_btn.hide()
        self.play_btn.clicked.connect(lambda: self.clicked.emit(self.media_id))

        layout.addWidget(self.img_container)

        # Metadata Labels
        self.title_label = QLabel(title, self)
        self.title_label.setStyleSheet("color: #FFFFFF; font-size: 13px; font-weight: 600; border: none;")
        self.title_label.setWordWrap(False)
        layout.addWidget(self.title_label)

        self.artist_lbl = QLabel(artist, self)
        self.artist_lbl.setStyleSheet("color: rgba(255, 255, 255, 0.6); font-size: 11px; border: none;")
        layout.addWidget(self.artist_lbl)

    def enterEvent(self, event: QEnterEvent):
        self.play_btn.show()
        self.play_btn.raise_()
        super().enterEvent(event)

    def leaveEvent(self, event):
        self.play_btn.hide()
        super().leaveEvent(event)