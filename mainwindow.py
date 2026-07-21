import os
import sys
import ctypes
import random
import json
import math
import struct
import keyring
import time
import wave
import urllib.request
import threading
import uuid
import base64
from cryptography.fernet import Fernet
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC


from PySide6.QtCore import QTimer
from PySide6.QtWidgets import (
    QMainWindow, QWidget, QHBoxLayout, QVBoxLayout, QPushButton, 
    QLabel, QSlider, QScrollArea, QGridLayout, QComboBox, QFrame, 
    QApplication, QFileDialog, QListWidget, QListWidgetItem, QMessageBox,
    QLineEdit, QMenu, QInputDialog, QTabWidget, QGroupBox, QFormLayout,
    QCheckBox, QAbstractItemView
)
from PySide6.QtCore import Qt, QTimer, QPoint, QSettings
from PySide6.QtGui import QColor, QPainter, QFontMetrics, QShortcut, QKeySequence, QPixmap, QAction

# Import custom modern UI components and the C++ Audio Engine
from fading_stack import FadingSlidingStackedWidget
from cards_ui import MediaCardWidget
from native_audio import AudioEngine

SERVICE_NAME = "FluidAudio_SystemCore"
ACCOUNT_NAME = "InstallTimestamp"

def get_trial_start_time() -> float:
    """Retrieves or sets the initial installation timestamp in OS Keyring."""
    stored_val = keyring.get_password(SERVICE_NAME, ACCOUNT_NAME)
    
    if stored_val is None:
        # First launch: record current timestamp in system keyring
        now = str(time.time())
        keyring.set_password(SERVICE_NAME, ACCOUNT_NAME, now)
        return float(now)
    
    try:
        return float(stored_val)
    except ValueError:
        # Value was tampered with
        return 0.0

class HardwareBoundTrialManager:
    def __init__(self):
        # Hidden path inside OS AppData rather than user home (~/)
        appdata_dir = os.getenv('APPDATA') or os.path.expanduser('~/.config')
        self.config_dir = os.path.join(appdata_dir, "FluidAudio", "System")
        os.makedirs(self.config_dir, exist_ok=True)
        self.config_path = os.path.join(self.config_dir, "sys_cache.dat")
        
        self.fernet = Fernet(self._generate_hwid_key())

    def _generate_hwid_key(self) -> bytes:
        """Derives a deterministic encryption key bound to this specific PC hardware."""
        raw_hwid = str(uuid.getnode()).encode('utf-8')  # Unique MAC/Hardware ID
        salt = b"FluidAudio_Static_Salt_2026"           # Private salt
        
        kdf = PBKDF2HMAC(
            algorithm=hashes.SHA256(),
            length=32,
            salt=salt,
            iterations=100000,
        )
        return base64.urlsafe_b64encode(kdf.derive(raw_hwid))

    def get_first_launch_time(self) -> float:
        """Reads encrypted trial time or creates a new hardware-locked record."""
        if not os.path.exists(self.config_path):
            first_launch = time.time()
            self._save_trial_data(first_launch)
            return first_launch

        try:
            with open(self.config_path, "rb") as f:
                encrypted_data = f.read()
            
            decrypted_bytes = self.fernet.decrypt(encrypted_data)
            payload = json.loads(decrypted_bytes.decode('utf-8'))
            return payload.get("install_time", 0.0)
            
        except Exception:
            # File was tampered with, corrupted, or copied from another machine
            print("[Warning] Trial file tampered with or transferred.")
            return 0.0 # Force trial expiration

    def _save_trial_data(self, timestamp: float):
        payload = json.dumps({"install_time": timestamp}).encode('utf-8')
        encrypted_data = self.fernet.encrypt(payload)
        with open(self.config_path, "wb") as f:
            f.write(encrypted_data)

def check_trial_tampering(hw_manager: HardwareBoundTrialManager) -> tuple[bool, float]:
    """Cross-references the OS Keyring date with the local file date."""
    keyring_time = get_trial_start_time()
    file_time = hw_manager.get_first_launch_time()

    # If the user deleted the local file but Keyring exists (or vice versa), mark as tampered
    if abs(keyring_time - file_time) > 5.0:  # Allow 5 seconds initial skew
        print("[Security] Mismatch detected between OS Keyring and local cache! Trial invalidated.")
        return True, 0.0 # Tampered!
        
    return False, keyring_time
    
import sys
from PySide6.QtWidgets import QApplication, QMainWindow, QLabel, QPushButton, QLineEdit, QVBoxLayout, QWidget, QMessageBox
from audio_engine import FluidAudioBackend

 
class ClickableSlider(QSlider):
    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            val = self.minimum() + ((self.maximum() - self.minimum()) * event.position().x()) / self.width()
            self.setValue(int(val))
            self.sliderPressed.emit()
            super().mousePressEvent(event)
        else:
            super().mousePressEvent(event)

    def mouseReleaseEvent(self, event):
        super().mouseReleaseEvent(event)


class MarqueeLabel(QLabel):
    def __init__(self, text="", parent=None):
        super().__init__(text, parent)
        self.full_text = text
        self.scroll_pos = 0
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.update_position)
        self.setAlignment(Qt.AlignmentFlag.AlignVCenter)
        self.setStyleSheet("background: transparent; color: white; font-size: 13px; font-weight: bold; border: none;")

    def setText(self, text):
        self.full_text = text
        self.scroll_pos = 0
        self.timer.stop()
        super().setText(text)
        
        fm = QFontMetrics(self.font())
        if fm.horizontalAdvance(self.full_text) > self.width():
            self.timer.start(30)

    def update_position(self):
        fm = QFontMetrics(self.font())
        text_width = fm.horizontalAdvance(self.full_text)
        if text_width > self.width():
            self.scroll_pos -= 1
            if self.scroll_pos < -text_width:
                self.scroll_pos = self.width()
            self.update()
        else:
            self.scroll_pos = 0

    def paintEvent(self, event):
        painter = QPainter(self)
        fm = QFontMetrics(self.font())
        text_width = fm.horizontalAdvance(self.full_text)
        
        if text_width > self.width():
            y = (self.height() + fm.ascent() - fm.descent()) // 2
            painter.drawText(self.scroll_pos, y, self.full_text)
        else:
            super().paintEvent(event)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Fluid Audio")
        self.resize(1180, 780)
        self.setMinimumSize(400, 300)
        self.backend = FluidAudioBackend()

        
                # Check trial
        is_trial_active, trial_msg = self.backend.check_trial_status()

        self.status_label = QLabel(f"Status: {trial_msg}")
        self.key_input = QLineEdit()
        self.key_input.setPlaceholderText("Enter Lemon Squeezy Key to Unlock Lifetime Access...")
        
        self.activate_btn = QPushButton("Activate License")
        self.activate_btn.clicked.connect(self.handle_activation)

        self.play_btn = QPushButton("Start Shifting Frequencies")
        self.play_btn.clicked.connect(self.play_music)

        layout = QVBoxLayout()
        layout.addWidget(self.status_label)
        layout.addWidget(self.key_input)
        layout.addWidget(self.activate_btn)
        layout.addWidget(self.play_btn)

        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)

        self.settings = QSettings("Fluid Audio", "AudioEngine")
        
        self.scanned_folders = []
        try:
            saved_folders = self.settings.value("scanned_music_folders", "[]")
            self.scanned_folders = json.loads(saved_folders)
        except Exception:
            self.scanned_folders = []

        self.setAcceptDrops(True)
        
        self.app_data_dir = os.path.join(os.environ['APPDATA'], 'FluidAudio')
        self.tracks_dir = os.path.join(self.app_data_dir, 'tracks')
        self.playlists_dir = os.path.join(self.app_data_dir, 'playlists')
        self.album_art_dir = os.path.join(self.app_data_dir, 'album_art')
        self.solfeggio_dir = os.path.join(self.app_data_dir, 'solfeggio_offline')
        self.history_file = os.path.join(self.app_data_dir, 'playback_history.txt')

        os.makedirs(self.app_data_dir, exist_ok=True)
        os.makedirs(self.tracks_dir, exist_ok=True)
        os.makedirs(self.playlists_dir, exist_ok=True)
        os.makedirs(self.album_art_dir, exist_ok=True)
        os.makedirs(self.solfeggio_dir, exist_ok=True)
        
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, True)
        self.apply_acrylic_effect()

        self.audio_engine = AudioEngine(self)
        
        # Enforce 432Hz default Solfeggio scale selection on boot up
        if hasattr(self.audio_engine, 'set_frequency'):
            self.audio_engine.set_frequency(432)

        if hasattr(self.audio_engine, 'enable_eq'):
            self.audio_engine.enable_eq(False)
        if hasattr(self.audio_engine, 'enable_crossfade'):
            self.audio_engine.enable_crossfade(False)
        if hasattr(self.audio_engine, 'enable_replaygain'):
            self.audio_engine.enable_replaygain(False)

        self.current_song_path = ""
        self.is_slider_pressed = False
        self.crossfade_triggered = False
        self.is_solfeggio_active = False
        
        self.library_tracks = [] 
        self.playlist_tracks = [] 
        self.play_queue_tracks = []  
        self.queue_playback_backup = []
        self.queue_loop_count = 0
        self.current_track_index = -1
        self.is_playing_from_queue = False  
        
        self.is_shuffle = False
        self.repeat_mode = 0  
        self.shuffle_played = set()
        
        self.lyrics_dict = {}
        self.lyrics_visible = False
        
        self.user_playlists = {}
        self.load_metadata_store()
        self.load_local_playlists()

        self.loaded_tabs = {}

        self.update_timer = QTimer(self)
        self.update_timer.setInterval(200)  
        self.update_timer.timeout.connect(self.update_playback_progress)

        self.setup_ui()
        self.connect_signals()
        self.setup_shortcuts()
        
        if self.scanned_folders:
            self.reload_all_saved_folders()

    def apply_acrylic_effect(self):
        try:
            hwnd = self.winId()
            import platform
            if platform.system() == "Windows" and int(platform.release().split('.')[0]) >= 10:
                dwm = ctypes.windll.dwmapi
                dark_mode = ctypes.c_int(1)
                dwm.DwmSetWindowAttribute(hwnd, 20, ctypes.byref(dark_mode), ctypes.sizeof(dark_mode))
                backdrop = ctypes.c_int(3)
                dwm.DwmSetWindowAttribute(hwnd, 38, ctypes.byref(backdrop), ctypes.sizeof(backdrop))
        except Exception as e:
            print(f"Native Window effects fallback: {e}")

    def load_metadata_store(self):
        self.metadata_store_path = os.path.join(self.app_data_dir, 'metadata_store.json')
        if os.path.exists(self.metadata_store_path):
            try:
                with open(self.metadata_store_path, 'r', encoding='utf-8') as f:
                    self.metadata_store = json.load(f)
            except Exception:
                self.metadata_store = {"recently_played": [], "recently_added": [], "play_counts": {}}
        else:
            self.metadata_store = {"recently_played": [], "recently_added": [], "play_counts": {}}

    def save_metadata_store(self):
        try:
            with open(self.metadata_store_path, 'w', encoding='utf-8') as f:
                json.dump(self.metadata_store, f, indent=4)
        except Exception as e:
            print(f"Error saving analytics metadata: {e}")

    def record_track_played_local(self, track_path):
        if not track_path or track_path.startswith("http"): return
        if "recently_played" not in self.metadata_store:
            self.metadata_store["recently_played"] = []
        if track_path in self.metadata_store["recently_played"]:
            self.metadata_store["recently_played"].remove(track_path)
        self.metadata_store["recently_played"].insert(0, track_path)
        self.metadata_store["recently_played"] = self.metadata_store["recently_played"][:30]
        
        counts = self.metadata_store.get("play_counts", {})
        counts[track_path] = counts.get(track_path, 0) + 1
        self.metadata_store["play_counts"] = counts
        self.save_metadata_store()
        
        if hasattr(self, 'history_tabs'):
            self.sync_history_list()

    def record_track_added_local(self, track_path):
        if "recently_added" not in self.metadata_store:
            self.metadata_store["recently_added"] = []
        if track_path not in self.metadata_store["recently_added"]:
            self.metadata_store["recently_added"].insert(0, track_path)
            self.metadata_store["recently_added"] = self.metadata_store["recently_added"][:30]

    def format_seconds_to_time(self, seconds):
        if not seconds or seconds < 0:
            return "00:00:00"
        seconds = int(seconds)
        h = seconds // 3600
        m = (seconds % 3600) // 60
        s = seconds % 60
        return f"{h:02d}:{m:02d}:{s:02d}"

    def update_playback_progress(self):
        if self.is_slider_pressed or not hasattr(self, 'audio_engine'): 
            return
            
        try:
            current_sec = self.audio_engine.get_position()
            duration = self.audio_engine.get_duration()
            
            if duration > 0:
                self.lbl_time_current.setText(self.format_seconds_to_time(current_sec))
                self.lbl_time_total.setText(self.format_seconds_to_time(duration))
                
                self.progress_slider.blockSignals(True)
                self.progress_slider.setValue(int((current_sec / duration) * 1000))
                self.progress_slider.blockSignals(False)

                if self.lyrics_visible and self.lyrics_dict:
                    target_idx = -1
                    sorted_times = sorted(self.lyrics_dict.keys())
                    for idx, t in enumerate(sorted_times):
                        if t <= current_sec:
                            target_idx = idx
                        else:
                            break
                    if target_idx != -1 and hasattr(self, 'lyrics_list_widget'):
                        if self.lyrics_list_widget.currentRow() != target_idx:
                            self.lyrics_list_widget.blockSignals(True)
                            self.lyrics_list_widget.setCurrentRow(target_idx)
                            item = self.lyrics_list_widget.item(target_idx)
                            if item:
                                for i in range(self.lyrics_list_widget.count()):
                                    it = self.lyrics_list_widget.item(i)
                                    it.setForeground(QColor("rgba(255, 255, 255, 0.5)"))
                                    font = it.font()
                                    font.setBold(False)
                                    font.setPointSize(12)
                                    it.setFont(font)
                                item.setForeground(QColor("#60CDFF"))
                                font = item.font()
                                font.setBold(True)
                                font.setPointSize(15)
                                item.setFont(font)
                                self.lyrics_list_widget.scrollToItem(item, QAbstractItemView.ScrollHint.PositionAtCenter)
                            self.lyrics_list_widget.blockSignals(False)

                if current_sec > 0.5:
                    time_remaining = duration - current_sec
                    if hasattr(self, 'chk_crossfade') and self.chk_crossfade.isChecked():
                        crossfade_time = self.slider_crossfade.value() if hasattr(self, 'slider_crossfade') else 5
                        if crossfade_time > 0 and time_remaining <= crossfade_time:
                            if not self.crossfade_triggered:
                                self.crossfade_triggered = True
                                self.play_next_track(crossfade=True)
                    else:
                        if time_remaining <= 0.4:
                            if not self.crossfade_triggered:
                                self.crossfade_triggered = True
                                self.play_next_track(crossfade=True)

        except Exception:
            pass

    def on_slider_pressed(self):
        self.is_slider_pressed = True

    def on_slider_released(self):
        if hasattr(self, 'audio_engine') and self.audio_engine.get_duration() > 0:
            percentage = self.progress_slider.value() / 1000.0
            target_sec = percentage * self.audio_engine.get_duration()
            if hasattr(self.audio_engine, 'seek'):
                self.audio_engine.seek(float(target_sec))
            self.lbl_time_current.setText(self.format_seconds_to_time(target_sec))
            
            # Use small single-shot window to seamlessly allow buffering mechanics without graphic stutter
            QTimer.singleShot(400, lambda: setattr(self, 'is_slider_pressed', False))
        else:
            self.is_slider_pressed = False

    def on_frequency_changed(self, index):
        if hasattr(self, 'freq_dropdown') and hasattr(self, 'audio_engine'):
            try:
                freq_text = self.freq_dropdown.currentText().split()[0]
                freq_val = int(freq_text)
                if hasattr(self.audio_engine, 'set_frequency'):
                    self.audio_engine.set_frequency(freq_val)
            except Exception as e:
                print(f"Error handling engine frequency assignment: {e}")

    def dragEnterEvent(self, event):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()

    def dropEvent(self, event):
        for url in event.mimeData().urls():
            file_path = url.toLocalFile()
            if file_path.lower().endswith(('.mp3', '.wav', '.flac', '.ogg')):
                self.is_playing_from_queue = False
                self.play_track_directly(file_path)
                break

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if self.width() < 800 and self.sidebar_expanded:
            self.toggle_sidebar()
        elif self.width() >= 800 and not self.sidebar_expanded:
            self.toggle_sidebar()
        self.update_control_buttons_style()
        
    def download_and_play(self, url: str, target_path: str):
        try:
            urllib.request.urlretrieve(url, target_path)
            QTimer.singleShot(0, lambda: self.play_track_directly(target_path))
        except Exception as e:
            print(f"Error fetching web stream: {e}")
    
    def generate_solfeggio_wav(self, filepath: str, freq_hz: int, duration_sec: int = 300, sample_rate: int = 44100):
        """Generates a clean local Solfeggio sine wave WAV file on demand."""
        os.makedirs(os.path.dirname(filepath), exist_ok=True)
        num_samples = sample_rate * duration_sec
        amplitude = 16000  # Safe 16-bit volume level
        
        with wave.open(filepath, 'w') as wav_file:
            wav_file.setnchannels(1)       # Mono
            wav_file.setsampwidth(2)       # 16-bit PCM
            wav_file.setframerate(sample_rate)
            
            frames = bytearray()
            for i in range(num_samples):
                t = float(i) / sample_rate
                sample = int(amplitude * math.sin(2.0 * math.pi * freq_hz * t))
                frames.extend(struct.pack('<h', sample))
                
            wav_file.writeframes(frames)
            
    def play_track_directly(self, track_path: str):        
        if not track_path:
            return
            
        if track_path.startswith("http"):
            safe_filename = "".join([c for c in os.path.basename(track_path) if c.isalnum() or c in "._-"])
            if not safe_filename.endswith(".mp3"):
                safe_filename += ".mp3"
            local_target = os.path.join(self.solfeggio_dir, safe_filename)
            
            if os.path.exists(local_target):
                track_path = local_target
            else:
                threading.Thread(target=self.download_and_play, args=(track_path, local_target), daemon=True).start()
                return
            
        if not os.path.exists(track_path):
            return
        
        
        try:
            self.current_song_path = track_path
            self.crossfade_triggered = False 
            
            is_solfeggio = "Solfeggio" in os.path.basename(track_path) or self.is_solfeggio_active
            self.is_solfeggio_active = is_solfeggio
            
            self.audio_engine.load_song(track_path)
            
            freq_target = int(self.freq_dropdown.currentText().split()[0])
            
            # Dynamically fetch target frequency from persistent dropdown selection context
            if hasattr(self, 'freq_dropdown'):
                freq_target = int(self.freq_dropdown.currentText().split()[0])
            else:
                freq_target = 432
                
            if hasattr(self.audio_engine, 'set_frequency'):
                self.audio_engine.set_frequency(freq_target)
            
            if hasattr(self, 'chk_eq_enable') and self.chk_eq_enable.isChecked():
                if hasattr(self.audio_engine, 'enable_eq'):
                    self.audio_engine.enable_eq(True)
                self.apply_eq_preset()
            
            if hasattr(self, 'chk_replay_gain') and self.chk_replay_gain.isChecked():
                if hasattr(self.audio_engine, 'enable_replaygain'):
                    self.audio_engine.enable_replaygain(True)
            
            if hasattr(self, 'chk_crossfade') and self.chk_crossfade.isChecked():
                if hasattr(self.audio_engine, 'enable_crossfade'):
                    self.audio_engine.enable_crossfade(True)
                if hasattr(self.audio_engine, 'set_crossfade_duration'):
                    self.audio_engine.set_crossfade_duration(self.slider_crossfade.value())
            
            duration_sec = self.audio_engine.get_duration()
            self.lbl_time_total.setText(self.format_seconds_to_time(duration_sec))
            self.lbl_time_current.setText(self.format_seconds_to_time(0))
            self.progress_slider.setValue(0)
            
            self.audio_engine.play()
            self.btn_play.setText("⏸")
            self.update_timer.start()

            track_filename = os.path.basename(track_path)
            track_title = os.path.splitext(track_filename)[0]
            artist_folder = os.path.basename(os.path.dirname(track_path))
            
            self.now_playing_title.setText(track_title)
            self.now_playing_artist.setText(artist_folder if artist_folder else "Local Library")
            self.setWindowTitle(f"Fluid Audio - {track_title}")
                
                    
            self.update_list_highlights()
            self.update_album_art_display(track_path)
            self.load_lyrics_for_track(track_path)
            
            self.add_to_history(track_path, freq_target)
            self.record_track_played_local(track_path)
            
        except Exception as e:
            print(f"Error starting playback: {e}")

    def update_list_highlights(self):
        # [MODIFIED]: Keep playing song on screen and highlighted persistently
        def apply_highlight(widget_name):
            if not hasattr(self, widget_name): return
            list_widget = getattr(self, widget_name)
            for i in range(list_widget.count()):
                item = list_widget.item(i)
                item_path = str(item.data(Qt.ItemDataRole.UserRole))
                if item_path == self.current_song_path:
                    item.setForeground(QColor("#60CDFF"))
                    font = item.font()
                    font.setBold(True)
                    item.setFont(font)
                    # Automatically scrolls to keep the active song visible on screen
                    list_widget.scrollToItem(item, QAbstractItemView.ScrollHint.PositionAtCenter)
                else:
                    item.setForeground(QColor("#E5E5E5"))
                    font = item.font()
                    font.setBold(False)
                    item.setFont(font)
                    
        apply_highlight('track_list_widget')
        apply_highlight('queue_list_widget')
        apply_highlight('playlist_tracks_display')

    def update_album_art_display(self, track_path: str):
        found_art = False
        if not track_path.startswith("http"):
            base_no_ext = os.path.splitext(os.path.basename(track_path))[0]
            track_dir = os.path.dirname(track_path)
            
            paths_to_check = [
                os.path.join(self.album_art_dir, f"{base_no_ext}.jpg"),
                os.path.join(self.album_art_dir, f"{base_no_ext}.png"),
                os.path.splitext(track_path)[0] + ".jpg",
                os.path.splitext(track_path)[0] + ".png",
                os.path.join(track_dir, "cover.jpg"),
                os.path.join(track_dir, "cover.png"),
                os.path.join(track_dir, "folder.jpg"),
                os.path.join(track_dir, "album.png")
            ]
            
            for path in paths_to_check:
                if os.path.exists(path):
                    pixmap = QPixmap(path)
                    if not pixmap.isNull():
                        self.album_art_lbl.setPixmap(pixmap.scaled(55, 55, Qt.AspectRatioMode.KeepAspectRatioByExpanding, Qt.TransformationMode.SmoothTransformation))
                        found_art = True
                        break

        if not found_art:
            fallback_pixmap = QPixmap(55, 55)
            fallback_pixmap.fill(QColor("#2d2d3a"))
            painter = QPainter(fallback_pixmap)
            painter.setPen(QColor("#60CDFF"))
            painter.drawText(fallback_pixmap.rect(), Qt.AlignmentFlag.AlignCenter, "🎵")
            painter.end()
            self.album_art_lbl.setPixmap(fallback_pixmap)

    def load_lyrics_for_track(self, track_path: str):
        self.lyrics_dict.clear()
        if hasattr(self, 'lyrics_list_widget'):
            self.lyrics_list_widget.clear()
            
        if track_path.startswith("http"):
            self.lyrics_list_widget.addItem(QListWidgetItem("Streaming Solfeggio Audio Lyrics Unavailable"))
            return

        base_name = os.path.splitext(os.path.basename(track_path))[0]
        track_dir = os.path.dirname(track_path)
        
        possible_paths = [
            os.path.splitext(track_path)[0] + ".lrc",
            os.path.join(track_dir, f"{base_name}.lrc"),
            os.path.splitext(track_path)[0] + ".txt"
        ]

        for path in possible_paths:
            if os.path.exists(path):
                try:
                    with open(path, "r", encoding="utf-8") as file:
                        if path.endswith(".txt"):
                            for idx, line in enumerate(file):
                                if line.strip():
                                    item = QListWidgetItem(line.strip())
                                    item.setData(Qt.ItemDataRole.UserRole, float(idx * 3))
                                    self.lyrics_list_widget.addItem(item)
                                    self.lyrics_dict[float(idx * 3)] = line.strip()
                            break
                            
                        for line in file:
                            if line.startswith("[") and "]" in line:
                                time_str = line[1:line.find("]")]
                                text = line[line.find("]")+1:].strip()
                                try:
                                    m, s = time_str.split(":")
                                    sec = int(m) * 60 + float(s)
                                    item = QListWidgetItem(text if text else "...")
                                    item.setData(Qt.ItemDataRole.UserRole, sec)
                                    self.lyrics_list_widget.addItem(item)
                                    self.lyrics_dict[sec] = text
                                except ValueError:
                                    continue
                    break
                except Exception:
                    pass
        if self.lyrics_list_widget.count() == 0:
            self.lyrics_list_widget.addItem(QListWidgetItem("No local synced LRC found for track"))

    def on_lyric_line_clicked(self, item):
        timestamp = item.data(Qt.ItemDataRole.UserRole)
        if timestamp is not None and hasattr(self, 'audio_engine'):
            self.audio_engine.seek(timestamp)

    def toggle_lyrics_view(self):
        self.lyrics_visible = not self.lyrics_visible
        self.lyrics_panel.setVisible(self.lyrics_visible)

    def setup_ui(self):
        central_widget = QWidget(self)
        central_widget.setObjectName("CentralShell")
        
        layout_manager = QHBoxLayout(central_widget)
        layout_manager.setContentsMargins(0, 0, 0, 0)
        layout_manager.setSpacing(0)

        # Sidebar Setup
        self.sidebar = QWidget()
        self.sidebar.setObjectName("Sidebar")
        self.sidebar.setFixedWidth(240)
        self.sidebar_expanded = True
        
        sidebar_layout = QVBoxLayout(self.sidebar)
        sidebar_layout.setContentsMargins(20, 40, 20, 20)
        sidebar_layout.setSpacing(8)

        sidebar_header = QHBoxLayout()
        self.btn_toggle_sidebar = QPushButton("☰")
        self.btn_toggle_sidebar.setFixedSize(30, 30)
        self.btn_toggle_sidebar.setStyleSheet("background: transparent; color: white; font-size: 16px; border: none;")
        self.btn_toggle_sidebar.clicked.connect(self.toggle_sidebar)
        
        self.logo = QLabel("Fluid Audio")
        self.logo.setStyleSheet("font-size: 18px; font-weight: bold; color: #FFFFFF;")
        
        sidebar_header.addWidget(self.btn_toggle_sidebar)
        sidebar_header.addWidget(self.logo)
        sidebar_header.addStretch()
        sidebar_layout.addLayout(sidebar_header)
        sidebar_layout.addSpacing(15)

        self.nav_btn_home = self.create_nav_button("🏠  Home", 0)
        self.nav_btn_library = self.create_nav_button("📚  Your Library", 1)
        self.nav_btn_queue = self.create_nav_button("📋  Queue", 2)
        self.nav_btn_playlists = self.create_nav_button("📁  Playlists", 3)
        self.nav_btn_settings = self.create_nav_button("⚙️  Settings", 4)
        self.nav_btn_history = self.create_nav_button("📜  Analytics & History", 5)
        
        self.nav_buttons = [
            self.nav_btn_home, self.nav_btn_library, self.nav_btn_queue,
            self.nav_btn_playlists, self.nav_btn_settings, self.nav_btn_history
        ]
        
        for btn in self.nav_buttons:
            sidebar_layout.addWidget(btn)
        sidebar_layout.addStretch()
        
        layout_manager.addWidget(self.sidebar)

        # Right Viewport Assembly
        right_panel = QWidget()
        right_panel_layout = QVBoxLayout(right_panel)
        right_panel_layout.setContentsMargins(0, 0, 0, 0)
        right_panel_layout.setSpacing(0)

        self.global_header = QWidget()
        self.global_header.setFixedHeight(60)
        self.global_header.setStyleSheet("background: transparent;")

        header_layout = QHBoxLayout(self.global_header)
        header_layout.setContentsMargins(35, 10, 30, 0)
        
        self.search_bar = QLineEdit()
        self.search_bar.setPlaceholderText("🔍  Search tracks, albums, artists...")
        self.search_bar.setFixedWidth(320)
        self.search_bar.setStyleSheet("""
            QLineEdit {
                background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.12);
                border-radius: 18px; padding: 6px 15px; color: #FFFFFF; font-size: 13px;
            }
            QLineEdit:focus { border-color: #60CDFF; background-color: rgba(255, 255, 255, 0.12); }
        """)
        self.search_bar.textChanged.connect(self.filter_universal_tracks)
        header_layout.addWidget(self.search_bar)
        header_layout.addStretch()
        
        # [MODIFIED]: Shifted the frequency dropdown list to the top right corner
        self.freq_dropdown = QComboBox()
        self.freq_dropdown.addItems(["174 Hz", "285 Hz", "396 Hz", "417 Hz", "432 Hz", "528 Hz", "639 Hz", "741 Hz", "852 Hz", "963 Hz"])
        self.freq_dropdown.setCurrentText("432 Hz")
        self.freq_dropdown.setToolTip("Global Solfeggio Scale Harmonic Shift Tuning Selection")
        self.freq_dropdown.setStyleSheet("""
            QComboBox { 
                background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15);
                border-radius: 6px; color: white; padding: 6px 12px; font-size: 13px; font-weight: bold; min-width: 90px;
            }
            QComboBox QAbstractItemView {
                background-color: #1a1a24; color: white; selection-background-color: #60CDFF; selection-color: black;
            }
        """)
        self.freq_dropdown.currentIndexChanged.connect(self.on_frequency_changed)
        header_layout.addWidget(self.freq_dropdown)
        
        right_panel_layout.addWidget(self.global_header)

        self.viewports_stack = FadingSlidingStackedWidget()
        self.viewports_stack.setObjectName("ViewportsStack")

        self.home_viewport = QWidget()
        self.library_viewport = QWidget()
        self.queue_viewport = QWidget()
        self.playlist_viewport = QWidget()
        self.settings_viewport = QWidget()
        self.history_viewport = QWidget()

        self.viewports_stack.addWidget(self.home_viewport)
        self.viewports_stack.addWidget(self.library_viewport)
        self.viewports_stack.addWidget(self.queue_viewport)
        self.viewports_stack.addWidget(self.playlist_viewport)
        self.viewports_stack.addWidget(self.settings_viewport)
        self.viewports_stack.addWidget(self.history_viewport)

        self.lyrics_panel = QWidget()
        self.lyrics_panel.setFixedHeight(150)
        self.lyrics_panel.setVisible(False)
        lyrics_layout = QVBoxLayout(self.lyrics_panel)
        lyrics_layout.setContentsMargins(10, 0, 10, 5)
        
        self.lyrics_list_widget = QListWidget()
        self.lyrics_list_widget.setStyleSheet("""
            QListWidget { background: rgba(0, 0, 0, 0.45); border: none; border-radius: 10px; color: rgba(255, 255, 255, 0.6); font-size: 13px; }
            QListWidget::item { text-align: center; padding: 4px; background: transparent; }
        """)
        self.lyrics_list_widget.itemClicked.connect(self.on_lyric_line_clicked)
        lyrics_layout.addWidget(self.lyrics_list_widget)
        right_panel_layout.addWidget(self.lyrics_panel)

        right_panel_layout.addWidget(self.viewports_stack)

        # Bottom Media Controls Block
        self.playback_bar = QWidget()
        self.playback_bar.setObjectName("PlaybackBar")
        self.playback_bar.setFixedHeight(95)
        
        playback_layout = QHBoxLayout(self.playback_bar)
        playback_layout.setContentsMargins(24, 12, 24, 12)
        
        self.album_art_lbl = QLabel()
        self.album_art_lbl.setFixedSize(55, 55)
        self.album_art_lbl.setStyleSheet("border-radius: 6px; background-color: rgba(255, 255, 255, 0.05); border: 1px solid rgba(255, 255, 255, 0.1);")
        
        metadata_layout = QVBoxLayout()
        metadata_layout.setSpacing(2)
        metadata_layout.setAlignment(Qt.AlignmentFlag.AlignVCenter)
        
        self.now_playing_title = MarqueeLabel("Choose a song...", self.playback_bar)
        self.now_playing_title.setFixedSize(200, 20)
        
        self.now_playing_artist = QLabel("No source loaded", self.playback_bar)
        self.now_playing_artist.setMaximumWidth(200)  
        self.now_playing_artist.setStyleSheet("color: rgba(255, 255, 255, 0.5); font-size: 11px; border: none; background: transparent;")
        
        metadata_layout.addWidget(self.now_playing_title)
        metadata_layout.addWidget(self.now_playing_artist)

        track_info_layout = QHBoxLayout()
        track_info_layout.setSpacing(12)
        track_info_layout.addWidget(self.album_art_lbl)
        track_info_layout.addLayout(metadata_layout)

        center_controls = QVBoxLayout()
        center_controls.setSpacing(4)
        
        btn_layout = QHBoxLayout()
        btn_layout.setSpacing(14)
        btn_layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        self.btn_shuffle = QPushButton("🔀")
        self.btn_prev = QPushButton("⏮")
        self.btn_play = QPushButton("▶")
        self.btn_next = QPushButton("⏭")
        self.btn_repeat = QPushButton("🔁")

        btn_layout.addWidget(self.btn_shuffle)
        btn_layout.addWidget(self.btn_prev)
        btn_layout.addWidget(self.btn_play)
        btn_layout.addWidget(self.btn_next)
        btn_layout.addWidget(self.btn_repeat)
        
        self.update_control_buttons_style()

        progress_layout = QHBoxLayout()
        progress_layout.setSpacing(10)
        
        self.lbl_time_current = QLabel("00:00:00")
        self.lbl_time_current.setStyleSheet("color: rgba(255,255,255,0.6); font-size: 11px; background: transparent;")
        
        self.progress_slider = ClickableSlider(Qt.Orientation.Horizontal)
        self.progress_slider.setObjectName("ProgressSlider")
        self.progress_slider.setRange(0, 1000)  
        self.progress_slider.setStyleSheet("""
            QSlider::groove:horizontal { height: 4px; background: rgba(255, 255, 255, 0.15); border-radius: 2px; }
            QSlider::sub-page:horizontal { background: #60CDFF; border-radius: 2px; }
            QSlider::handle:horizontal { background: #FFFFFF; border: 1px solid #555555; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }
            QSlider::handle:horizontal:hover { background: #60CDFF; width: 14px; height: 14px; border-radius: 7px; }
        """)

        self.lbl_time_total = QLabel("00:00:00")
        self.lbl_time_total.setStyleSheet("color: rgba(255,255,255,0.6); font-size: 11px; background: transparent;")

        progress_layout.addWidget(self.lbl_time_current)
        progress_layout.addWidget(self.progress_slider)
        progress_layout.addWidget(self.lbl_time_total)

        center_controls.addLayout(btn_layout)
        center_controls.addLayout(progress_layout)

        right_utilities = QHBoxLayout()
        right_utilities.setSpacing(10)
        right_utilities.setAlignment(Qt.AlignmentFlag.AlignRight)
        

        self.btn_lyrics = QPushButton("💬")
        self.btn_lyrics.setFixedSize(36, 36)
        self.btn_lyrics.setToolTip("Show Lark Lyric Engine Tracking")
        self.btn_lyrics.setStyleSheet("""
            QPushButton { background: transparent; border: none; color: #E5E5E5; font-size: 15px; }
            QPushButton:hover { color: #60CDFF; }
        """)
        self.btn_lyrics.clicked.connect(self.toggle_lyrics_view)
        right_utilities.addWidget(self.btn_lyrics)
        
        self.volume_icon = QLabel("🔊")
        self.volume_icon.setStyleSheet("color: rgba(255,255,255,0.8); background: transparent;")
        
        self.volume_slider = QSlider(Qt.Orientation.Horizontal)
        self.volume_slider.setFixedWidth(100)
        self.volume_slider.setRange(0, 100)
        self.volume_slider.setValue(80)
        self.volume_slider.setStyleSheet("""
            QSlider::groove:horizontal { height: 4px; background: rgba(255, 255, 255, 0.15); border-radius: 2px; }
            QSlider::sub-page:horizontal { background: #FFFFFF; border-radius: 2px; }
            QSlider::handle:horizontal { background: #FFFFFF; width: 10px; height: 10px; margin: -3px 0; border-radius: 5px; }
        """)

        right_utilities.addWidget(self.volume_icon)
        right_utilities.addWidget(self.volume_slider)

        playback_layout.addLayout(track_info_layout, 1)
        playback_layout.addLayout(center_controls, 2)
        playback_layout.addLayout(right_utilities, 1)

        right_panel_layout.addWidget(self.playback_bar)
        layout_manager.addWidget(right_panel)

        self.setCentralWidget(central_widget)

        self.setStyleSheet("""
            #CentralShell { background-color: rgba(20, 20, 20, 0.65); }
            #Sidebar { background-color: rgba(12, 12, 12, 0.45); border-right: 1px solid rgba(255, 255, 255, 0.05); }
            #PlaybackBar { background-color: rgba(22, 22, 22, 0.85); border-top: 1px solid rgba(255, 255, 255, 0.08); }
            QScrollArea, QWidget#ScrollContent { background: transparent; border: none; }
        """)

        self.switch_tab(0)

    def update_control_buttons_style(self):
        w = self.width()
        if w < 800:
            size, p_size, font, p_font = 36, 44, 16, 20
        elif w < 1000:
            size, p_size, font, p_font = 40, 48, 18, 22
        else:
            size, p_size, font, p_font = 46, 54, 22, 24

        shuff_bg = 'rgba(96, 205, 255, 0.15)' if self.is_shuffle else 'transparent'
        shuff_color = '#60CDFF' if self.is_shuffle else '#E5E5E5'
        self.btn_shuffle.setFixedSize(size, size)
        self.btn_shuffle.setStyleSheet(f"""
            QPushButton {{ 
                background-color: {shuff_bg}; 
                border: {'1px solid #60CDFF' if self.is_shuffle else 'none'}; 
                color: {shuff_color}; 
                font-size: {font}px; 
                border-radius: {size//2}px; 
            }} 
            QPushButton:hover {{ 
                background-color: rgba(255, 255, 255, 0.1); 
                color: #60CDFF; 
            }}
        """)
        self.btn_shuffle.setToolTip(f"Shuffle Mode: {'ON' if self.is_shuffle else 'OFF'}")

        rep_bg = 'rgba(96, 205, 255, 0.15)' if self.repeat_mode > 0 else 'transparent'
        rep_colors = {0: '#E5E5E5', 1: '#60CDFF', 2: '#60CDFF'}
        rep_tooltips = {0: "Repeat Mode: OFF", 1: "Repeat Mode: Track (One)", 2: "Repeat Mode: Context (All)"}
        self.btn_repeat.setFixedSize(size, size)
        self.btn_repeat.setStyleSheet(f"""
            QPushButton {{ 
                background-color: {rep_bg}; 
                border: {'1px solid #60CDFF' if self.repeat_mode > 0 else 'none'}; 
                color: {rep_colors[self.repeat_mode]}; 
                font-size: {font}px; 
                border-radius: {size//2}px; 
            }} 
            QPushButton:hover {{ 
                background-color: rgba(255, 255, 255, 0.1); 
                color: #60CDFF; 
            }}
        """)
        self.btn_repeat.setToolTip(rep_tooltips[self.repeat_mode])

        for btn in [self.btn_prev, self.btn_next]:
            btn.setFixedSize(size, size)
            btn.setStyleSheet(f"QPushButton {{ background: transparent; border: none; color: #E5E5E5; font-size: {font}px; border-radius: {size//2}px; }} QPushButton:hover {{ background-color: rgba(255, 255, 255, 0.1); color: #60CDFF; }}")

        self.btn_play.setFixedSize(p_size, p_size)
        self.btn_play.setStyleSheet(f"QPushButton {{ background-color: #FFFFFF; color: #000000; font-size: {p_font}px; border-radius: {p_size//2}px; border: none; font-weight: bold; }} QPushButton:hover {{ background-color: #60CDFF; color: #FFFFFF; }}")

    def setup_shortcuts(self):
        QShortcut(QKeySequence("Space"), self).activated.connect(self.toggle_play_state)
        QShortcut(QKeySequence("Ctrl+Right"), self).activated.connect(self.play_next_track)
        QShortcut(QKeySequence("Ctrl+Left"), self).activated.connect(self.play_prev_track)
        QShortcut(QKeySequence("Ctrl+S"), self).activated.connect(self.toggle_shuffle_mode)
        QShortcut(QKeySequence("Ctrl+R"), self).activated.connect(self.toggle_repeat_mode)
        QShortcut(QKeySequence("Ctrl+L"), self).activated.connect(self.toggle_lyrics_view)

    def toggle_sidebar(self):
        self.sidebar_expanded = not self.sidebar_expanded
        if self.sidebar_expanded:
            self.sidebar.setFixedWidth(240)
            self.logo.show()
            labels = ["🏠  Home", "📚  Your Library", "📋  Queue", "📁  Playlists", "⚙️  Settings", "📜  Analytics & History"]
            for idx, btn in enumerate(self.nav_buttons):
                btn.setText(labels[idx])
        else:
            self.sidebar.setFixedWidth(70)
            self.logo.hide()
            icons = ["🏠", "📚", "📋", "📁", "⚙️", "📜"]
            for idx, btn in enumerate(self.nav_buttons):
                btn.setText(icons[idx])
        self.switch_tab(self.viewports_stack.currentIndex())

    def create_nav_button(self, label: str, index: int) -> QPushButton:
        btn = QPushButton(label)
        btn.setObjectName(f"NavBtn_{index}")
        btn.setStyleSheet("""
            QPushButton { text-align: left; padding: 12px 18px; border-radius: 8px; color: rgba(255, 255, 255, 0.85); font-weight: 500; background-color: transparent; border: none; font-size: 13px; }
            QPushButton:hover { background-color: rgba(255, 255, 255, 0.06); }
        """)
        btn.clicked.connect(lambda: self.switch_tab(index))
        return btn

    def switch_tab(self, index: int):
        for idx, btn in enumerate(self.nav_buttons):
            if idx == index:
                if self.sidebar_expanded:
                    btn.setStyleSheet("text-align: left; padding: 12px 18px; border-radius: 8px; color: #60CDFF; background-color: rgba(255, 255, 255, 0.07); border-left: 3px solid #60CDFF; font-weight: bold; font-size: 13px;")
                else:
                    btn.setStyleSheet("text-align: center; padding: 12px 0px; border-radius: 8px; color: #60CDFF; background-color: rgba(255, 255, 255, 0.07); border-left: 3px solid #60CDFF; font-weight: bold; font-size: 16px;")
            else:
                if self.sidebar_expanded:
                    btn.setStyleSheet("text-align: left; padding: 12px 18px; border-radius: 8px; color: rgba(255, 255, 255, 0.8); background-color: transparent; border: none; font-size: 13px;")
                else:
                    btn.setStyleSheet("text-align: center; padding: 12px 0px; border-radius: 8px; color: rgba(255, 255, 255, 0.8); background-color: transparent; border: none; font-size: 16px;")

        self.lazy_load_tab(index)
        self.viewports_stack.setCurrentIndex(index)
        QTimer.singleShot(50, self.update_list_highlights)

    def connect_signals(self):
        self.btn_play.clicked.connect(self.toggle_play_state)
        self.progress_slider.sliderPressed.connect(self.on_slider_pressed)
        self.progress_slider.sliderReleased.connect(self.on_slider_released)
        self.btn_next.clicked.connect(lambda: self.play_next_track(crossfade=False))
        self.btn_prev.clicked.connect(self.play_prev_track)
        self.volume_slider.valueChanged.connect(self.on_volume_changed)
        self.btn_shuffle.clicked.connect(self.toggle_shuffle_mode)
        self.btn_repeat.clicked.connect(self.toggle_repeat_mode)

    def on_volume_changed(self, value):
        if hasattr(self, 'audio_engine'):
            self.audio_engine.set_volume(value / 100.0)

    def filter_universal_tracks(self, search_text):
        query = search_text.lower()
        if hasattr(self, 'track_list_widget'):
            for i in range(self.track_list_widget.count()):
                item = self.track_list_widget.item(i)
                item.setHidden(query not in item.text().lower())
                
        if hasattr(self, 'queue_list_widget'):
            for i in range(self.queue_list_widget.count()):
                item = self.queue_list_widget.item(i)
                item.setHidden(query not in item.text().lower())

    def sort_library_tracks(self):
        criteria = self.sort_dropdown.currentText() if hasattr(self, 'sort_dropdown') else "Title (A-Z)"
        
        if criteria == "Title (A-Z)":
            self.library_tracks.sort(key=lambda x: os.path.basename(x).lower())
        elif criteria == "Title (Z-A)":
            self.library_tracks.sort(key=lambda x: os.path.basename(x).lower(), reverse=True)
        elif criteria == "Artist Directory Location":
            self.library_tracks.sort(key=lambda x: os.path.dirname(x).lower())
        elif criteria == "Absolute File Path":
            self.library_tracks.sort(key=lambda x: x.lower())
            
        self.refresh_library_list_widget()

    def lazy_load_tab(self, index: int):
        if index in self.loaded_tabs:
            if index == 1: self.update_list_highlights()
            elif index == 2: self.sync_queue_list()
            elif index == 3: self.refresh_playlist_displays()
            elif index == 5: self.sync_history_list()
            return

        target_widget = self.viewports_stack.widget(index)
        layout = QVBoxLayout(target_widget)
        layout.setContentsMargins(35, 40, 35, 30)

        if index == 0:  
            title = QLabel("📡 Live Solfeggio Frequency Radio Streams", target_widget)
            title.setStyleSheet("font-size: 26px; font-weight: bold; color: #FFFFFF; margin-bottom: 2px;")
            layout.addWidget(title)

            subtitle = QLabel("Tune into continuous online Solfeggio streams for live meditation, acoustic healing, and harmonic focus.", target_widget)
            subtitle.setStyleSheet("color: rgba(255, 255, 255, 0.6); font-size: 13px; margin-bottom: 15px;")
            layout.addWidget(subtitle)

            self.live_status_frame = QFrame()
            self.live_status_frame.setStyleSheet("""
                QFrame {
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 rgba(96, 205, 255, 0.18), stop:1 rgba(20, 20, 30, 0.45));
                    border: 1px solid rgba(96, 205, 255, 0.35);
                    border-radius: 10px;
                    padding: 8px 16px;
                }
            """)
            status_layout = QHBoxLayout(self.live_status_frame)
            
            self.lbl_live_indicator = QLabel("🟢 READY TO STREAM")
            self.lbl_live_indicator.setStyleSheet("color: #60CDFF; font-weight: bold; font-size: 12px;")
            
            self.lbl_active_stream_info = QLabel("Select an online station below to begin live audio streaming")
            self.lbl_active_stream_info.setStyleSheet("color: white; font-size: 13px;")
            
            status_layout.addWidget(self.lbl_live_indicator)
            status_layout.addSpacing(15)
            status_layout.addWidget(self.lbl_active_stream_info)
            status_layout.addStretch()
            
            layout.addWidget(self.live_status_frame)
            layout.addSpacing(12)

            category_bar = QHBoxLayout()
            category_bar.setSpacing(10)
            
            categories = ["All Stations", "Physical Healing", "Transformation", "Spiritual & Divine"]
            for cat_name in categories:
                btn_cat = QPushButton(cat_name)
                btn_cat.setCursor(Qt.CursorShape.PointingHandCursor)
                btn_cat.setStyleSheet("""
                    QPushButton {
                        background-color: rgba(255, 255, 255, 0.08);
                        border: 1px solid rgba(255, 255, 255, 0.15);
                        border-radius: 14px;
                        padding: 6px 14px;
                        color: white;
                        font-size: 12px;
                        font-weight: 500;
                    }
                    QPushButton:hover {
                        background-color: rgba(96, 205, 255, 0.25);
                        border-color: #60CDFF;
                    }
                """)
                category_bar.addWidget(btn_cat)
            category_bar.addStretch()
            layout.addLayout(category_bar)
            layout.addSpacing(15)

            scroll = QScrollArea()
            scroll.setWidgetResizable(True)
            scroll_content = QWidget()
            scroll_content.setObjectName("ScrollContent")
            self.home_grid = QGridLayout(scroll_content)
            self.home_grid.setSpacing(20)

            all_solfeggio_streams = [
                ("Solfeggio 174 Hz", "Security & Grounding", "Physical Healing", "https://actions.google.com/sounds/v1/ambiences/sine_wave_174hz.mp3"),
                ("Solfeggio 285 Hz", "Quantum Cognition", "Physical Healing", "https://actions.google.com/sounds/v1/ambiences/sine_wave_285hz.mp3"),
                ("Solfeggio 396 Hz", "Liberating Guilt & Fear", "Physical Healing", "https://actions.google.com/sounds/v1/ambiences/sine_wave_396hz.mp3"),
                ("Solfeggio 417 Hz", "Undoing Situations & Change", "Transformation", "https://actions.google.com/sounds/v1/ambiences/sine_wave_417hz.mp3"),
                ("Solfeggio 432 Hz", "Cosmic Natural Harmony", "Transformation", "https://actions.google.com/sounds/v1/ambiences/sine_wave_432hz.mp3"),
                ("Solfeggio 528 Hz", "Transformation & DNA Repair", "Transformation", "https://actions.google.com/sounds/v1/ambiences/sine_wave_528hz.mp3"),
                ("Solfeggio 639 Hz", "Connecting & Relationships", "Transformation", "https://actions.google.com/sounds/v1/ambiences/sine_wave_639hz.mp3"),
                ("Solfeggio 741 Hz", "Expression & Solutions", "Spiritual & Divine", "https://actions.google.com/sounds/v1/ambiences/sine_wave_741hz.mp3"),
                ("Solfeggio 852 Hz", "Returning to Spiritual Order", "Spiritual & Divine", "https://actions.google.com/sounds/v1/ambiences/sine_wave_852hz.mp3"),
                ("Solfeggio 963 Hz", "Divine Consciousness", "Spiritual & Divine", "https://actions.google.com/sounds/v1/ambiences/sine_wave_963hz.mp3")
            ]

            def stream_online_station(name, desc, url):
                self.lbl_live_indicator.setText("🔴 LIVE STREAMING")
                self.lbl_active_stream_info.setText(f"Streaming Live: {name} — {desc}")
                
                self.is_playing_from_queue = False
                safe_name = "".join([c for c in name if c.isalpha() or c.isdigit() or c == ' ']).rstrip()
                local_path = os.path.join(self.solfeggio_dir, f"{safe_name}.wav")
                
                freq_val = 432
                parts = name.split()
                for p in parts:
                    if p.isdigit():
                        freq_val = int(p)
                        break

                if not os.path.exists(local_path):
                    self.lbl_active_stream_info.setText(f"Generating local {freq_val} Hz frequency tone...")
                    self.generate_solfeggio_wav(local_path, freq_hz=freq_val, duration_sec=600)

                self.play_track_directly(local_path)

            for i, data in enumerate(all_solfeggio_streams):
                name, desc, category, url = data
                
                card = MediaCardWidget(name, desc, category, url)
                card.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
                card.customContextMenuRequested.connect(lambda pos, d=data: self.show_solfeggio_context(pos, d))
                card.clicked.connect(lambda checked=False, n=name, d=desc, u=url: stream_online_station(n, d, u))
                
                self.home_grid.addWidget(card, i // 4, i % 4)

            scroll.setWidget(scroll_content)
            layout.addWidget(scroll)
            
        elif index == 1: 
            title = QLabel("Your Music Library", target_widget)
            title.setStyleSheet("font-size: 26px; font-weight: bold; color: white; margin-bottom: 10px;")
            layout.addWidget(title)
            
            lib_header = QHBoxLayout()
            btn_scan_folder = QPushButton("📁  Scan Folder")
            btn_scan_folder.setStyleSheet("""
                QPushButton { background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); color: white; border-radius: 6px; padding: 8px 15px; font-size: 13px; font-weight: bold; }
                QPushButton:hover { background-color: rgba(96, 205, 255, 0.2); border-color: #60CDFF; }
            """)
            btn_scan_folder.clicked.connect(self.scan_local_folder_dialog)
            lib_header.addWidget(btn_scan_folder)
            
            btn_add_selected_queue = QPushButton("📋 Add Selected to Queue")
            btn_add_selected_queue.setStyleSheet("""
                QPushButton { background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); color: white; border-radius: 6px; padding: 8px 15px; font-size: 13px; }
                QPushButton:hover { background-color: rgba(96, 205, 255, 0.2); border-color: #60CDFF; }
            """)
            btn_add_selected_queue.clicked.connect(self.add_selected_to_queue)
            lib_header.addWidget(btn_add_selected_queue)

            lib_header.addSpacing(15)
            self.sort_dropdown = QComboBox()
            self.sort_dropdown.addItems(["Title (A-Z)", "Title (Z-A)", "Artist Directory Location", "Absolute File Path"])
            self.sort_dropdown.setStyleSheet("""
                QComboBox { background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255,255,255,0.15); border-radius: 6px; color: white; padding: 6px; font-size: 12px; }
            """)
            self.sort_dropdown.currentIndexChanged.connect(self.sort_library_tracks)
            lib_header.addWidget(QLabel("Sort Preference:"))
            lib_header.addWidget(self.sort_dropdown)
            lib_header.addStretch()
            layout.addLayout(lib_header)

            self.track_list_widget = QListWidget()
            self.track_list_widget.setSelectionMode(QAbstractItemView.ExtendedSelection)
            self.track_list_widget.setStyleSheet("""
                QListWidget { background-color: rgba(255, 255, 255, 0.03); border: 1px solid rgba(255,255,255,0.05); border-radius: 8px; color: #E5E5E5; padding: 10px; font-size: 14px; margin-top: 15px; }
                QListWidget::item { padding: 12px; border-radius: 4px; }
                QListWidget::item:hover { background-color: rgba(255, 255, 255, 0.06); }
                QListWidget::item:selected { background-color: rgba(96, 205, 255, 0.15); color: #60CDFF; }
            """)
            self.track_list_widget.itemDoubleClicked.connect(self.on_library_item_double_clicked)
            self.track_list_widget.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
            self.track_list_widget.customContextMenuRequested.connect(self.show_track_context_menu)
            layout.addWidget(self.track_list_widget)
            
            if self.library_tracks:
                self.refresh_library_list_widget()
            self.update_list_highlights()

        elif index == 2:  
            title = QLabel("Play Queue", target_widget)
            title.setStyleSheet("font-size: 26px; font-weight: bold; color: white; margin-bottom: 5px;")
            layout.addWidget(title)
            
            self.queue_info_label = QLabel("Queue: 0 tracks | Approx Duration: 00:00:00")
            self.queue_info_label.setStyleSheet("color: rgba(255,255,255,0.6); font-size: 13px; font-weight: bold; margin-bottom: 5px;")
            layout.addWidget(self.queue_info_label)
            
            queue_action_layout = QHBoxLayout()
            btn_add_to_queue = QPushButton("➕ Add Songs")
            btn_add_to_queue.setStyleSheet("""
                QPushButton { background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255,255,255,0.15); color: white; border-radius: 6px; padding: 8px 15px; font-size: 13px; }
                QPushButton:hover { background-color: rgba(96, 205, 255, 0.15); border-color: #60CDFF; }
            """)
            btn_add_to_queue.clicked.connect(self.add_songs_to_queue_manually)
            queue_action_layout.addWidget(btn_add_to_queue)
            
            btn_remove_from_queue = QPushButton("❌ Remove Selected")
            btn_remove_from_queue.setStyleSheet("""
                QPushButton { background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255,255,255,0.15); color: white; border-radius: 6px; padding: 8px 15px; font-size: 13px; }
                QPushButton:hover { background-color: rgba(235, 75, 75, 0.15); border-color: #ff6b6b; }
            """)
            btn_remove_from_queue.clicked.connect(self.remove_selected_from_queue)
            queue_action_layout.addWidget(btn_remove_from_queue)
            
            btn_clear_queue = QPushButton("🧹 Clear All")
            btn_clear_queue.setStyleSheet("""
                QPushButton { background-color: rgba(235, 75, 75, 0.1); border: 1px solid rgba(235,75,75,0.25); color: #ff6b6b; border-radius: 6px; padding: 8px 15px; font-size: 13px; }
                QPushButton:hover { background-color: rgba(235, 75, 75, 0.25); }
            """)
            btn_clear_queue.clicked.connect(self.clear_entire_queue)
            queue_action_layout.addWidget(btn_clear_queue)

            btn_save_queue_pl = QPushButton("💾 Save as Playlist")
            btn_save_queue_pl.setStyleSheet("""
                QPushButton { background-color: rgba(96, 205, 255, 0.15); border: 1px solid #60CDFF; color: #60CDFF; border-radius: 6px; padding: 8px 15px; font-size: 13px; }
                QPushButton:hover { background-color: rgba(96, 205, 255, 0.3); }
            """)
            btn_save_queue_pl.clicked.connect(self.save_queue_as_playlist)
            queue_action_layout.addWidget(btn_save_queue_pl)
            
            queue_action_layout.addStretch()
            layout.addLayout(queue_action_layout)
            
            self.queue_list_widget = QListWidget()
            self.queue_list_widget.setSelectionMode(QAbstractItemView.ExtendedSelection)
            self.queue_list_widget.setDragDropMode(QAbstractItemView.InternalMove)
            self.queue_list_widget.setDragEnabled(True)
            self.queue_list_widget.setAcceptDrops(True)
            self.queue_list_widget.setDropIndicatorShown(True)
            
            self.queue_list_widget.setStyleSheet("""
                QListWidget { background-color: rgba(255, 255, 255, 0.03); border: 1px solid rgba(255,255,255,0.05); border-radius: 8px; color: #E5E5E5; padding: 10px; font-size: 14px; margin-top: 15px; }
                QListWidget::item { padding: 12px; border-radius: 4px; }
                QListWidget::item:hover { background-color: rgba(255, 255, 255, 0.06); }
                QListWidget::item:selected { background-color: rgba(96, 205, 255, 0.15); color: #60CDFF; }
            """)
            self.queue_list_widget.itemDoubleClicked.connect(self.on_queue_item_double_clicked)
            self.queue_list_widget.model().rowsMoved.connect(self.on_queue_rows_reordered_internally)
            layout.addWidget(self.queue_list_widget)
            self.sync_queue_list()

        elif index == 3:  
            title = QLabel("Playlists Manager", target_widget)
            title.setStyleSheet("font-size: 26px; font-weight: bold; color: white; margin-bottom: 5px;")
            layout.addWidget(title)
            
            playlist_desc = QLabel("Create custom playlists or generate dynamic sets matched to Solfeggio frequencies.", target_widget)
            playlist_desc.setStyleSheet("color: rgba(255,255,255,0.5); font-size: 13px; margin-bottom: 20px;")
            layout.addWidget(playlist_desc)
            
            pl_bar = QHBoxLayout()
            btn_create_pl = QPushButton("➕ Create Playlist")
            btn_create_pl.setStyleSheet("""
                QPushButton { background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); color: white; border-radius: 6px; padding: 8px 15px; font-size: 13px; font-weight: bold; }
                QPushButton:hover { background-color: rgba(96, 205, 255, 0.15); border-color: #60CDFF; }
            """)
            btn_create_pl.clicked.connect(self.create_new_playlist_ui)
            pl_bar.addWidget(btn_create_pl)
            
            btn_add_to_pl = QPushButton("➕ Add Songs")
            btn_add_to_pl.setStyleSheet("""
                QPushButton { background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); color: white; border-radius: 6px; padding: 8px 15px; font-size: 13px; }
                QPushButton:hover { background-color: rgba(96, 205, 255, 0.15); border-color: #60CDFF; }
            """)
            btn_add_to_pl.clicked.connect(self.add_songs_to_playlist_manually)
            pl_bar.addWidget(btn_add_to_pl)
            
            btn_remove_pl = QPushButton("❌ Remove Selected")
            btn_remove_pl.setStyleSheet("""
                QPushButton { background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); color: white; border-radius: 6px; padding: 8px 15px; font-size: 13px; }
                QPushButton:hover { background-color: rgba(235, 75, 75, 0.15); border-color: #ff6b6b; }
            """)
            btn_remove_pl.clicked.connect(self.remove_selected_from_playlist)
            pl_bar.addWidget(btn_remove_pl)
            
            btn_clear_pl = QPushButton("🧹 Clear Playlist")
            btn_clear_pl.setStyleSheet("""
                QPushButton { background-color: rgba(235, 75, 75, 0.1); border: 1px solid rgba(235,75,75,0.25); color: #ff6b6b; border-radius: 6px; padding: 8px 15px; font-size: 13px; }
                QPushButton:hover { background-color: rgba(235, 75, 75, 0.25); }
            """)
            btn_clear_pl.clicked.connect(self.clear_current_playlist)
            pl_bar.addWidget(btn_clear_pl)

            btn_generate_smart = QPushButton("🪄 Generate Smart Selection")
            btn_generate_smart.setStyleSheet("""
                QPushButton { background-color: rgba(96, 205, 255, 0.1); border: 1px solid rgba(96, 205, 255, 0.25); color: #60CDFF; border-radius: 6px; padding: 8px 15px; font-size: 13px; font-weight: bold; }
                QPushButton:hover { background-color: rgba(96, 205, 255, 0.25); }
            """)
            btn_generate_smart.clicked.connect(self.trigger_smart_playlist_generation)
            pl_bar.addWidget(btn_generate_smart)
            pl_bar.addStretch()
            layout.addLayout(pl_bar)

            playlists_split_layout = QHBoxLayout()
            playlists_split_layout.setSpacing(15)
            
            self.playlist_names_list = QListWidget()
            self.playlist_names_list.setFixedWidth(240)
            self.playlist_names_list.setStyleSheet("""
                QListWidget { background-color: rgba(255, 255, 255, 0.03); border: 1px solid rgba(255,255,255,0.05); border-radius: 8px; color: #E5E5E5; padding: 10px; font-size: 14px; }
                QListWidget::item { padding: 10px; border-radius: 4px; }
                QListWidget::item:selected { background-color: rgba(96, 205, 255, 0.15); color: #60CDFF; }
            """)
            self.playlist_names_list.itemClicked.connect(self.display_selected_playlist_tracks)
            playlists_split_layout.addWidget(self.playlist_names_list)
            
            self.playlist_tracks_display = QListWidget()
            self.playlist_tracks_display.setSelectionMode(QAbstractItemView.ExtendedSelection)
            self.playlist_tracks_display.setStyleSheet("""
                QListWidget { background-color: rgba(255, 255, 255, 0.03); border: 1px solid rgba(255,255,255,0.05); border-radius: 8px; color: #E5E5E5; padding: 10px; font-size: 14px; }
                QListWidget::item { padding: 10px; border-radius: 4px; }
                QListWidget::item:selected { background-color: rgba(96, 205, 255, 0.15); color: #60CDFF; }
            """)
            self.playlist_tracks_display.itemDoubleClicked.connect(self.play_track_from_playlist_viewport)
            playlists_split_layout.addWidget(self.playlist_tracks_display)
            
            layout.addLayout(playlists_split_layout)
            self.refresh_playlist_displays()

        elif index == 4:  
            title = QLabel("System Settings & Custom Controls", target_widget)
            title.setStyleSheet("font-size: 26px; font-weight: bold; color: white; margin-bottom: 20px;")
            layout.addWidget(title)

            settings_box = QScrollArea()
            settings_box.setWidgetResizable(True)
            settings_box_content = QWidget()
            settings_layout = QVBoxLayout(settings_box_content)
            settings_layout.setSpacing(20)

            replay_gain_group = QGroupBox("ReplayGain Audio Normalization")
            replay_gain_group.setStyleSheet("QGroupBox { color: #60CDFF; font-weight: bold; font-size: 14px; border: 1px solid rgba(255,255,255,0.08); padding-top: 15px; border-radius: 6px; }")
            replay_gain_layout = QHBoxLayout()
            self.chk_replay_gain = QCheckBox("Enable Audio Volume Level Normalization")
            self.chk_replay_gain.setStyleSheet("color: white;")
            self.chk_replay_gain.stateChanged.connect(self.toggle_replay_gain)
            replay_gain_layout.addWidget(self.chk_replay_gain)
            replay_gain_group.setLayout(replay_gain_layout)
            settings_layout.addWidget(replay_gain_group)

            crossfade_group = QGroupBox("Gapless Transition & Crossfade")
            crossfade_group.setStyleSheet("QGroupBox { color: #60CDFF; font-weight: bold; font-size: 14px; border: 1px solid rgba(255,255,255,0.08); padding-top: 15px; border-radius: 6px; }")
            cross_form = QFormLayout()
            self.chk_crossfade = QCheckBox("Enable Crossfade transitions between tracks")
            self.chk_crossfade.setStyleSheet("color: white;")
            self.chk_crossfade.stateChanged.connect(self.toggle_crossfade)
            
            self.slider_crossfade = QSlider(Qt.Orientation.Horizontal)
            self.slider_crossfade.setRange(0, 15)
            self.slider_crossfade.setValue(5)
            self.lbl_crossfade_val = QLabel("5 seconds")
            self.lbl_crossfade_val.setStyleSheet("color: white;")
            self.slider_crossfade.valueChanged.connect(lambda val: (self.lbl_crossfade_val.setText(f"{val} seconds"), self.update_crossfade_duration(val)))
            
            cross_form.addRow(self.chk_crossfade)
            cross_form.addRow("Transition Time Duration:", self.slider_crossfade)
            cross_form.addRow("Selected Duration Value:", self.lbl_crossfade_val)
            crossfade_group.setLayout(cross_form)
            settings_layout.addWidget(crossfade_group)

            eq_group = QGroupBox("Advanced 10-Band Graphic Equalizer")
            eq_group.setStyleSheet("QGroupBox { color: #60CDFF; font-weight: bold; font-size: 14px; border: 1px solid rgba(255,255,255,0.08); padding-top: 15px; border-radius: 6px; }")
            eq_vbox = QVBoxLayout()
            
            eq_top_bar = QHBoxLayout()
            self.chk_eq_enable = QCheckBox("Enable Hardware-Accelerated EQ Engine")
            self.chk_eq_enable.setStyleSheet("color: white;")
            self.chk_eq_enable.stateChanged.connect(self.toggle_eq)
            eq_top_bar.addWidget(self.chk_eq_enable)
            
            self.eq_presets = QComboBox()
            self.eq_presets.addItems(["Flat", "Vocal Booster", "Bass Boost", "Classical", "Pop", "Rock", "Techno", "Acoustic"])
            self.eq_presets.setStyleSheet("QComboBox { background-color: rgba(255,255,255,0.08); color: white; padding: 4px; }")
            self.eq_presets.currentIndexChanged.connect(self.apply_eq_preset)
            eq_top_bar.addWidget(QLabel("Preset Profile:"))
            eq_top_bar.addWidget(self.eq_presets)
            eq_vbox.addLayout(eq_top_bar)
            
            bands_hbox = QHBoxLayout()
            bands_freqs = ["31Hz", "62Hz", "125Hz", "250Hz", "500Hz", "1kHz", "2kHz", "4kHz", "8kHz", "16kHz"]
            self.eq_sliders = []
            for idx, b_freq in enumerate(bands_freqs):
                band_vbox = QVBoxLayout()
                b_slider = QSlider(Qt.Orientation.Vertical)
                b_slider.setRange(-12, 12)
                b_slider.setValue(0)
                b_slider.setFixedHeight(120)
                b_slider.valueChanged.connect(lambda val, i=idx: self.update_eq_band(i, val))
                b_lbl = QLabel(b_freq)
                b_lbl.setStyleSheet("font-size: 9px; color: #E5E5E5;")
                b_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
                band_vbox.addWidget(b_slider, 0, Qt.AlignmentFlag.AlignCenter)
                band_vbox.addWidget(b_lbl, 0, Qt.AlignmentFlag.AlignCenter)
                bands_hbox.addLayout(band_vbox)
                self.eq_sliders.append(b_slider)
                
            eq_vbox.addLayout(bands_hbox)
            eq_group.setLayout(eq_vbox)
            settings_layout.addWidget(eq_group)
            
            settings_layout.addStretch()
            settings_box.setWidget(settings_box_content)
            layout.addWidget(settings_box)

        elif index == 5: 
            title_layout = QHBoxLayout()
            title = QLabel("Playback Analytics Dashboard", target_widget)
            title.setStyleSheet("font-size: 26px; font-weight: bold; color: white;")
            title_layout.addWidget(title)
            title_layout.addStretch()
            
            btn_clear_history = QPushButton("🗑 Reset History Store")
            btn_clear_history.setStyleSheet("""
                QPushButton { background-color: rgba(235, 75, 75, 0.1); border: 1px solid rgba(235, 75, 75, 0.25); color: #ff6b6b; border-radius: 6px; padding: 8px 15px; font-size: 12px; font-weight: bold; }
                QPushButton:hover { background-color: rgba(235, 75, 75, 0.2); }
            """)
            btn_clear_history.clicked.connect(self.clear_playback_history)
            title_layout.addWidget(btn_clear_history)
            layout.addLayout(title_layout)

            self.history_tabs = QTabWidget()
            self.history_tabs.setStyleSheet("""
                QTabWidget::panel { background: rgba(255, 255, 255, 0.03); border: 1px solid rgba(255,255,255,0.05); border-radius: 8px; color: white; }
                QTabBar::tab { background: rgba(255,255,255,0.05); color: #E5E5E5; padding: 8px 16px; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 4px; }
                QTabBar::tab:selected { background: rgba(255, 255, 255, 0.1); color: #60CDFF; font-weight: bold; border-bottom: 2px solid #60CDFF; }
            """)
            
            self.recently_played_list = QListWidget()
            self.recently_added_list = QListWidget()
            self.most_played_list = QListWidget()
            
            for list_view in [self.recently_played_list, self.recently_added_list, self.most_played_list]:
                list_view.setStyleSheet("""
                    QListWidget { background: transparent; border: none; color: #E5E5E5; padding: 10px; font-size: 14px; }
                    QListWidget::item { padding: 10px; border-radius: 4px; }
                    QListWidget::item:hover { background-color: rgba(255, 255, 255, 0.06); }
                    QListWidget::item:selected { background-color: rgba(96, 205, 255, 0.15); color: #60CDFF; }
                """)
                list_view.itemDoubleClicked.connect(self.on_analytics_item_double_clicked)
                
            self.history_tabs.addTab(self.recently_played_list, "🕒 Recently Played (30)")
            self.history_tabs.addTab(self.recently_added_list, "✨ Recently Added (30)")
            self.history_tabs.addTab(self.most_played_list, "🔥 Most Played")
            
            layout.addWidget(self.history_tabs)
            self.sync_history_list()

        self.loaded_tabs[index] = True

    def show_solfeggio_context(self, pos, data):
        sender = self.sender()
        menu = QMenu(self)
        menu.setStyleSheet("""
            QMenu { background-color: #1a1a24; color: #ffffff; border: 1px solid rgba(255,255,255,0.1); border-radius: 6px; padding: 5px; }
            QMenu::item { padding: 6px 20px; border-radius: 4px; }
            QMenu::item:selected { background-color: #60CDFF; color: #000000; }
        """)
        
        down_act = QAction("⬇ Download for Offline Use", self)
        down_act.triggered.connect(lambda: self.download_solfeggio(data[3], data[0]))
        menu.addAction(down_act)
        menu.exec(sender.mapToGlobal(pos))
        
    def download_solfeggio(self, url, name):
        safe_name = "".join([c for c in name if c.isalpha() or c.isdigit() or c==' ']).rstrip()
        out_path = os.path.join(self.solfeggio_dir, f"{safe_name}.mp3")
        
        if os.path.exists(out_path):
            QMessageBox.information(self, "Exists", f"'{name}' is already downloaded in your offline folder.")
            return

        def task():
            try:
                urllib.request.urlretrieve(url, out_path)
                if out_path not in self.library_tracks:
                    self.library_tracks.append(out_path)
                    self.record_track_added_local(out_path)
            except Exception as e:
                print(f"Failed to download stream: {e}")

        QMessageBox.information(self, "Downloading", f"Downloading '{name}' in the background. It will be added to your library shortly.")
        threading.Thread(target=task, daemon=True).start()

    def toggle_eq(self, state):
        enabled = state == Qt.CheckState.Checked.value
        if hasattr(self, 'audio_engine') and hasattr(self.audio_engine, 'enable_eq'):
            self.audio_engine.enable_eq(enabled)
            
    def update_eq_band(self, index, value):
        if self.chk_eq_enable.isChecked() and hasattr(self, 'audio_engine') and hasattr(self.audio_engine, 'set_eq_band'):
            self.audio_engine.set_eq_band(index, value)

    def apply_eq_preset(self):
        presets = {
            "Flat": [0]*10,
            "Bass Boost": [6, 5, 4, 2, 0, 0, 0, 0, 0, 0],
            "Vocal Booster": [-2, -2, -1, 1, 3, 4, 3, 1, -1, -2],
            "Classical": [0, 0, 0, 0, 0, 0, -2, -4, -4, -6],
            "Pop": [-2, -1, 2, 4, 5, 4, 2, -1, -2, -3],
            "Rock": [5, 4, 3, 1, -1, -1, 2, 3, 4, 5],
            "Techno": [6, 5, 3, 0, -2, -2, 0, 3, 5, 6],
            "Acoustic": [3, 3, 2, 1, 0, 0, 1, 2, 2, 2]
        }
        profile = self.eq_presets.currentText()
        if profile in presets:
            vals = presets[profile]
            for i, slider in enumerate(self.eq_sliders):
                slider.blockSignals(True)
                slider.setValue(vals[i])
                slider.blockSignals(False)
                self.update_eq_band(i, vals[i])

    def toggle_crossfade(self, state):
        enabled = state == Qt.CheckState.Checked.value
        if hasattr(self, 'audio_engine') and hasattr(self.audio_engine, 'enable_crossfade'):
            self.audio_engine.enable_crossfade(enabled)

    def update_crossfade_duration(self, val):
        if self.chk_crossfade.isChecked() and hasattr(self, 'audio_engine') and hasattr(self.audio_engine, 'set_crossfade_duration'):
            self.audio_engine.set_crossfade_duration(val)

    def toggle_replay_gain(self, state):
        enabled = state == Qt.CheckState.Checked.value
        if hasattr(self, 'audio_engine') and hasattr(self.audio_engine, 'enable_replaygain'):
            self.audio_engine.enable_replaygain(enabled)

    def show_track_context_menu(self, pos: QPoint):
        selected_items = self.track_list_widget.selectedItems()
        checked_items = [self.track_list_widget.item(i) for i in range(self.track_list_widget.count()) if self.track_list_widget.item(i).checkState() == Qt.CheckState.Checked]
        items_to_use = checked_items if checked_items else selected_items
        if not items_to_use: return
            
        menu = QMenu(self)
        menu.setStyleSheet("""
            QMenu { background-color: #1a1a24; color: #ffffff; border: 1px solid rgba(255,255,255,0.1); border-radius: 6px; padding: 5px; }
            QMenu::item { padding: 6px 20px; border-radius: 4px; }
            QMenu::item:selected { background-color: #60CDFF; color: #000000; }
        """)
        
        if len(items_to_use) == 1:
            song_path = str(items_to_use[0].data(Qt.UserRole))
            play_act = QAction("▶ Play Now", self)
            play_act.triggered.connect(lambda: self.play_track_from_library_context(song_path))
            menu.addAction(play_act)
            
        play_next_act = QAction(f"⏭ Play Next ({len(items_to_use)} selected)", self)
        add_queue_act = QAction(f"📋 Add to Queue ({len(items_to_use)} selected)", self)
        
        add_to_playlist_submenu = menu.addMenu("📁 Add to Playlist")
        add_to_playlist_submenu.setStyleSheet(menu.styleSheet())
        
        play_next_act.triggered.connect(lambda: self.add_multiple_to_queue(items_to_use, to_front=True))
        add_queue_act.triggered.connect(lambda: self.add_multiple_to_queue(items_to_use, to_front=False))
        
        menu.addAction(play_next_act)
        menu.addAction(add_queue_act)
        
        if self.user_playlists:
            for playlist_name in self.user_playlists.keys():
                playlist_act = QAction(playlist_name, self)
                playlist_act.triggered.connect(lambda checked=False, name=playlist_name: self.add_selected_to_playlist(items_to_use, name))
                add_to_playlist_submenu.addAction(playlist_act)
        else:
            no_playlist_act = QAction("No Custom Playlists Found", self)
            no_playlist_act.setEnabled(False)
            add_to_playlist_submenu.addAction(no_playlist_act)
            
        menu.exec(self.track_list_widget.mapToGlobal(pos))

    def play_track_from_library_context(self, song_path):
        self.shuffle_played = set() 
        self.is_playing_from_queue = False
        if song_path in self.library_tracks:
            self.playlist_tracks = list(self.library_tracks)
            self.current_track_index = self.library_tracks.index(song_path)
        else:
            self.playlist_tracks = [song_path]
            self.current_track_index = 0
        self.play_track_directly(song_path)

    def add_multiple_to_queue(self, selected_items, to_front=False):
        for item in selected_items:
            path = str(item.data(Qt.UserRole))
            if path and (os.path.exists(path) or path.startswith("http")):
                if to_front:
                    self.play_queue_tracks.insert(max(0, self.current_track_index + 1), path)
                else:
                    self.play_queue_tracks.append(path)
                if hasattr(self.audio_engine, 'append_to_queue'):
                    self.audio_engine.append_to_queue(path)
        self.sync_queue_list()
        self.clear_all_library_checkboxes()
        

    def add_selected_to_queue(self):
        selected_items = self.track_list_widget.selectedItems()
        checked_items = [self.track_list_widget.item(i) for i in range(self.track_list_widget.count()) if self.track_list_widget.item(i).checkState() == Qt.CheckState.Checked]
        items_to_add = checked_items if checked_items else selected_items
        if not items_to_add:
            QMessageBox.warning(self, "Selection Empty", "Select or check tracks to queue first.")
            return
        self.add_multiple_to_queue(items_to_add, to_front=False)

    def add_selected_to_playlist(self, selected_items, playlist_name):
        added_count = 0
        for item in selected_items:
            path = str(item.data(Qt.UserRole))
            if playlist_name in self.user_playlists and path not in self.user_playlists[playlist_name]:
                self.user_playlists[playlist_name].append(path)
                added_count += 1
        if added_count > 0:
            self.save_playlist_to_disk(playlist_name)
            self.refresh_playlist_displays()
            self.clear_all_library_checkboxes()

    def clear_all_library_checkboxes(self):
        if hasattr(self, 'track_list_widget'):
            self.track_list_widget.blockSignals(True)
            for i in range(self.track_list_widget.count()):
                self.track_list_widget.item(i).setCheckState(Qt.CheckState.Unchecked)
            self.track_list_widget.blockSignals(False)

    def load_local_playlists(self):
        if not os.path.exists(self.playlists_dir): return
        for file in os.listdir(self.playlists_dir):
            if file.endswith(".txt"):
                playlist_name = os.path.splitext(file)[0]
                try:
                    with open(os.path.join(self.playlists_dir, file), "r", encoding="utf-8") as f:
                        paths = [line.strip() for line in f if os.path.exists(line.strip()) or line.strip().startswith("http")]
                    self.user_playlists[playlist_name] = paths
                except Exception as e:
                    print(f"Error loading playlist {playlist_name}: {e}")

    def save_playlist_to_disk(self, playlist_name):
        os.makedirs(self.playlists_dir, exist_ok=True)
        file_path = os.path.join(self.playlists_dir, f"{playlist_name}.txt")
        try:
            with open(file_path, "w", encoding="utf-8") as f:
                for track_path in self.user_playlists.get(playlist_name, []):
                    f.write(f"{track_path}\n")
        except Exception as e:
            print(f"Error saving playlist to disk: {e}")

    def create_new_playlist_ui(self):
        name, ok = QInputDialog.getText(self, "Create Playlist", "Enter custom playlist name:")
        if ok and name.strip():
            clean_name = name.strip()
            if clean_name not in self.user_playlists:
                self.user_playlists[clean_name] = []
                self.save_playlist_to_disk(clean_name)
                self.refresh_playlist_displays()

    def add_songs_to_playlist_manually(self):
        current_list = self.playlist_names_list.currentItem()
        if not current_list: return
        p_name = current_list.text().replace("📁 ", "")
        files, _ = QFileDialog.getOpenFileNames(self, "Select Music Files", "", "Audio Files (*.mp3 *.wav *.flac *.ogg)")
        if files:
            added = 0
            for file_path in files:
                if file_path not in self.user_playlists[p_name]:
                    self.user_playlists[p_name].append(file_path)
                    added += 1
            if added:
                self.save_playlist_to_disk(p_name)
                self.display_selected_playlist_tracks(current_list)

    def remove_selected_from_playlist(self):
        current_list = self.playlist_names_list.currentItem()
        if not current_list: return
        p_name = current_list.text().replace("📁 ", "")
        selected_items = self.playlist_tracks_display.selectedItems()
        if not selected_items: return
        for item in selected_items:
            path = str(item.data(Qt.UserRole))
            if path in self.user_playlists[p_name]:
                self.user_playlists[p_name].remove(path)
        self.save_playlist_to_disk(p_name)
        self.display_selected_playlist_tracks(current_list)

    def clear_current_playlist(self):
        current_list = self.playlist_names_list.currentItem()
        if not current_list: return
        p_name = current_list.text().replace("📁 ", "")
        reply = QMessageBox.question(self, "Clear Playlist", f"Clear songs from '{p_name}'?", QMessageBox.Yes | QMessageBox.No)
        if reply == QMessageBox.Yes:
            self.user_playlists[p_name] = []
            self.save_playlist_to_disk(p_name)
            self.display_selected_playlist_tracks(current_list)

    def trigger_smart_playlist_generation(self):
        if not self.library_tracks: return
        smart_name = f"Smart Mix {random.randint(1000, 9999)}"
        count = min(15, len(self.library_tracks))
        self.user_playlists[smart_name] = random.sample(self.library_tracks, count)
        self.save_playlist_to_disk(smart_name)
        self.refresh_playlist_displays()

    def refresh_playlist_displays(self):
        if not hasattr(self, 'playlist_names_list'): return
        self.playlist_names_list.clear()
        for p_name in self.user_playlists.keys():
            self.playlist_names_list.addItem(f"📁 {p_name}")

    def reload_all_saved_folders(self):
        self.library_tracks.clear()
        for folder in self.scanned_folders:
            if os.path.exists(folder):
                for root, _, files in os.walk(folder):
                    for file in files:
                        if file.lower().endswith(('.mp3', '.wav', '.flac', '.ogg')):
                            path = os.path.join(root, file)
                            if path not in self.library_tracks:
                                self.library_tracks.append(path)
                                self.record_track_added_local(path)
        self.sort_library_tracks()

    def scan_local_folder_dialog(self):
        folder = QFileDialog.getExistingDirectory(self, "Select Music Directory to Scan")
        if folder:
            if folder not in self.scanned_folders:
                self.scanned_folders.append(folder)
                self.settings.setValue("scanned_music_folders", json.dumps(self.scanned_folders))
            self.reload_all_saved_folders()
            self.refresh_library_list_widget()

    def refresh_library_list_widget(self):
        if not hasattr(self, 'track_list_widget'): return
        self.track_list_widget.clear()
        for track in self.library_tracks:
            item = QListWidgetItem(os.path.basename(track))
            item.setData(Qt.UserRole, track)
            item.setCheckState(Qt.CheckState.Unchecked)
            self.track_list_widget.addItem(item)
        self.update_list_highlights()

    def on_library_item_double_clicked(self, item):
        path = item.data(Qt.UserRole)
        self.play_track_from_library_context(path)

    def on_queue_item_double_clicked(self, item):
        # [MODIFIED] Clicking an item in queue automatically jumps there and removes items prior
        track = item.data(Qt.ItemDataRole.UserRole)
        idx = self.play_queue_tracks.index(track)
        self.play_queue_tracks = self.play_queue_tracks[idx:]
        self.sync_queue_list()
        self.play_next_track(crossfade=False)

    def on_queue_rows_reordered_internally(self, parent, start, end, destination, row):
        new_queue = []
        for i in range(self.queue_list_widget.count()):
            new_queue.append(self.queue_list_widget.item(i).data(Qt.UserRole))
        self.play_queue_tracks = new_queue
        if hasattr(self.audio_engine, 'clear_queue'):
            self.audio_engine.clear_queue()
            for path in self.play_queue_tracks:
                if hasattr(self.audio_engine, 'append_to_queue'):
                    self.audio_engine.append_to_queue(path)

    def display_selected_playlist_tracks(self, item):
        if not item or not hasattr(self, 'playlist_tracks_display'): return
        p_name = item.text().replace("📁 ", "")
        self.playlist_tracks_display.clear()
        tracks = self.user_playlists.get(p_name, [])
        for track in tracks:
            it = QListWidgetItem(os.path.basename(track))
            it.setData(Qt.UserRole, track)
            self.playlist_tracks_display.addItem(it)
        self.update_list_highlights()

    def play_track_from_playlist_viewport(self, item):
        path = item.data(Qt.UserRole)
        current_pl_item = self.playlist_names_list.currentItem()
        if current_pl_item:
            p_name = current_pl_item.text().replace("📁 ", "")
            self.playlist_tracks = list(self.user_playlists.get(p_name, []))
            self.current_track_index = self.playlist_tracks.index(path)
            self.is_playing_from_queue = False
            self.play_track_directly(path)

    def sync_queue_list(self):
        if not hasattr(self, 'queue_list_widget'): return
        self.queue_list_widget.clear()
        for track in self.play_queue_tracks:
            item = QListWidgetItem(os.path.basename(track))
            item.setData(Qt.UserRole, track)
            self.queue_list_widget.addItem(item)
        
        total_tracks = len(self.play_queue_tracks)
        approx_sec = total_tracks * 215 
        self.queue_info_label.setText(f"Queue: {total_tracks} tracks | Approx Duration: {self.format_seconds_to_time(approx_sec)}")
        self.update_list_highlights()

    def add_to_queue(self, track_path):
        self.play_queue_tracks.append(track_path)
        # [MODIFIED]: Resets looping metric when new songs populate the queue
        self.queue_loop_count = 0
        self.queue_playback_backup = list(self.play_queue_tracks)
        self.sync_queue_list()
    
    def add_songs_to_queue_manually(self):
        files, _ = QFileDialog.getOpenFileNames(self, "Select Audio Files", "", "Audio Files (*.mp3 *.wav *.flac *.ogg)")
        if files:
            self.play_queue_tracks.extend(files)
            # [MODIFIED]: Fresh tracking initialized 
            self.queue_loop_count = 0
            self.queue_playback_backup = list(self.play_queue_tracks)
            self.sync_queue_list()

    def remove_selected_from_queue(self):
        if not hasattr(self, 'queue_list_widget'): return
        selected = self.queue_list_widget.selectedItems()
        if not selected: return
        for item in selected:
            path = item.data(Qt.UserRole)
            if path in self.play_queue_tracks:
                self.play_queue_tracks.remove(path)
        if hasattr(self.audio_engine, 'clear_queue'):
            self.audio_engine.clear_queue()
            for path in self.play_queue_tracks:
                if hasattr(self.audio_engine, 'append_to_queue'):
                    self.audio_engine.append_to_queue(path)
        self.sync_queue_list()

    def clear_entire_queue(self):
        self.play_queue_tracks.clear()
        if hasattr(self.audio_engine, 'clear_queue'):
            self.audio_engine.clear_queue()
        self.sync_queue_list()

    def save_queue_as_playlist(self):
        if not self.play_queue_tracks: return
        name, ok = QInputDialog.getText(self, "Save Queue Config As Playlist", "Playlist Target Title Name:")
        if ok and name.strip():
            clean_name = name.strip()
            self.user_playlists[clean_name] = list(self.play_queue_tracks)
            self.save_playlist_to_disk(clean_name)
            self.refresh_playlist_displays()

    def toggle_play_state(self):
        if not self.current_song_path:
            if self.library_tracks:
                self.play_track_from_library_context(self.library_tracks[0])
            return
            
        if self.btn_play.text() == "▶":
            if hasattr(self, 'audio_engine') and hasattr(self.audio_engine, 'play'):
                self.audio_engine.play()
            self.btn_play.setText("⏸")
            self.update_timer.start()
        else:
            if hasattr(self, 'audio_engine') and hasattr(self.audio_engine, 'pause'):
                self.audio_engine.pause()
            self.btn_play.setText("▶")
            self.update_timer.stop()

    def play_next_track(self, crossfade=False):
        # [MODIFIED]: In any tab, prioritize the queue if songs are added
        if self.play_queue_tracks:
            # Pop song to play
            next_track = self.play_queue_tracks.pop(0)
            
            # [MODIFIED]: Auto-remove from the UI list in the queue tab
            if hasattr(self, 'queue_list_widget') and self.queue_list_widget.count() > 0:
                self.queue_list_widget.takeItem(0)
                
            self.is_playing_from_queue = True
            self.play_track_directly(next_track)
            
            # [MODIFIED]: Queue repetition logic
            if not self.play_queue_tracks:
                # Infinite loop handling
                if self.repeat_mode == 2: 
                    self.play_queue_tracks = list(self.queue_playback_backup)
                    self.sync_queue_list()
                # 4-time threshold handling
                elif self.queue_loop_count < 4: 
                    self.play_queue_tracks = list(self.queue_playback_backup)
                    self.queue_loop_count += 1
                    self.sync_queue_list()
                else:
                    # Stops naturally if limits are exhausted
                    pass
            return
            
        # Standard logic when queue is empty
        current_list = self.library_tracks
        if hasattr(self, 'current_song_path') and self.current_song_path in self.playlist_tracks:
            current_list = self.playlist_tracks

        if not current_list:
            return

        if self.current_song_path in current_list:
            current_index = current_list.index(self.current_song_path)
        else:
            current_index = -1

        if self.repeat_mode == 1:
            self.play_track_directly(self.current_song_path)
            return

        if self.is_shuffle:
            available_indices = [i for i in range(len(current_list)) if i != current_index]
            if not available_indices:
                next_index = current_index
            else:
                next_index = random.choice(available_indices)
        else:
            next_index = (current_index + 1) % len(current_list)

        if not self.is_shuffle and self.repeat_mode == 0 and next_index == 0 and current_index == len(current_list) - 1:
            self.audio_engine.stop()
            self.btn_play.setText("▶")
            return

        self.play_track_directly(current_list[next_index])

    def play_prev_track(self):
        tracks_source = self.play_queue_tracks if self.is_playing_from_queue else self.playlist_tracks
        if not tracks_source: return
        
        if self.current_track_index - 1 >= 0:
            self.current_track_index -= 1
        else:
            if self.repeat_mode == 2:
                self.current_track_index = len(tracks_source) - 1
            else:
                self.current_track_index = 0
        self.play_track_directly(tracks_source[self.current_track_index])

    def toggle_shuffle_mode(self):
        self.is_shuffle = not self.is_shuffle
        self.shuffle_played.clear()
        self.update_control_buttons_style()

    def toggle_repeat_mode(self):
        self.repeat_mode = (self.repeat_mode + 1) % 3
        self.update_control_buttons_style()

    def add_to_history(self, track_path, freq):
        try:
            with open(self.history_file, "a", encoding="utf-8") as f:
                f.write(f"{track_path}|{freq}\n")
        except Exception as e:
            print(f"Error logging local history record line: {e}")

    def clear_playback_history(self):
        if os.path.exists(self.history_file):
            try:
                os.remove(self.history_file)
            except Exception: pass
        self.metadata_store["recently_played"] = []
        self.metadata_store["play_counts"] = {}
        self.save_metadata_store()
        self.sync_history_list()

    def sync_history_list(self):
        if not hasattr(self, 'recently_played_list'): return
        self.recently_played_list.clear()
        self.recently_added_list.clear()
        self.most_played_list.clear()
        
        for path in self.metadata_store.get("recently_played", []):
            item = QListWidgetItem(os.path.basename(path))
            item.setData(Qt.UserRole, path)
            self.recently_played_list.addItem(item)
            
        for path in self.metadata_store.get("recently_added", []):
            item = QListWidgetItem(os.path.basename(path))
            item.setData(Qt.UserRole, path)
            self.recently_added_list.addItem(item)
            
        counts = self.metadata_store.get("play_counts", {})
        sorted_counts = sorted(counts.items(), key=lambda x: x[1], reverse=True)
        for path, count in sorted_counts[:30]:
            item = QListWidgetItem(f"[{count}x] {os.path.basename(path)}")
            item.setData(Qt.UserRole, path)
            self.most_played_list.addItem(item)

    def on_analytics_item_double_clicked(self, item):
        path = item.data(Qt.UserRole)
        self.play_track_from_library_context(path)
        
    def handle_activation(self):
        key = self.key_input.text().strip()
        if self.backend.activate_lemon_squeezy_license(key):
            QMessageBox.information(self, "Success", "Full version permanently unlocked!")
            self.status_label.setText("Status: Premium Unlocked")
        else:
            QMessageBox.critical(self, "Error", "Invalid or used license key.")

    def play_music(self):
        if self.backend.engine.can_play_audio():
            self.backend.process_music()
        else:
            QMessageBox.warning(self, "Trial Expired", "Your 7-day trial has ended. Please purchase a license to continue listening.")




if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())