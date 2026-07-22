import os
import sys
import time
import json
import os
import requests
import uuid

# Dynamically add the build directory to path so python can find the compiled native binary module
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

try:
    import native_audio
    # Now Engine is available as native_audio.Engine
    Engine = native_audio.Engine
except ImportError as e:
    raise ImportError(
        "Could not load the native C++ audio module. "
        "Make sure you compiled native_audio using CMake and your compiler.\n"
        f"Detail: {e}"
    )
except AttributeError as e:
    raise ImportError(
        "Could not load the Engine class from native_audio.\n"
        f"Detail: {e}"
    )
WORKER_URL = "https://fluid-audio-trial-checker.kawukijoshua19.workers.dev/"

class FluidAudioBackend:
    def __init__(self):
        # Your existing initialization...
        pass

    def get_hardware_id(self) -> str:
        """Returns a deterministic unique hardware ID for this PC."""
        return str(uuid.getnode())

    def check_server_trial_status(self) -> tuple[bool, str]:
        """Queries the Cloudflare Worker to verify 7-day trial validity."""
        hwid = self.get_hardware_id()
        
        try:
            response = requests.post(
                WORKER_URL,
                json={"hwid": hwid},
                timeout=4  # 4-second timeout to keep UI responsive
            )
            
            if response.status_code == 200:
                data = response.json()
                is_valid = data.get("valid", False)
                days_left = data.get("days_remaining", 0)
                
                if is_valid:
                    return True, f"Trial Active ({days_left} days left)"
                else:
                    return False, "Trial Expired"
                    
        except requests.exceptions.RequestException:
            # Handle offline scenario (see fallback recommendation)
            print("[Network] Unable to connect to trial verification server.")
            
        return False, "Connection Error"
    
class FluidAudioBackend:
    def __init__(self):
        self.engine = Engine()
        self.secret_hash = "FLUID_AUDIO_SECURE_HASH_2026"
        self.config_path = os.path.expanduser("~/.fluid_audio_data.json")
        
        # Initialize trial status
        self.check_trial_status()

    def get_first_launch_time(self):
        """Stores or reads the initial install time."""
        if not os.path.exists(self.config_path):
            data = {"install_time": time.time(), "license_key": None}
            with open(self.config_path, "w") as f:
                json.dump(data, f)
            return data["install_time"]
        else:
            with open(self.config_path, "r") as f:
                data = json.load(f)
                return data.get("install_time", time.time())

    def check_trial_status(self):
        """Determines if the 7-day trial is still valid."""
        install_time = self.get_first_launch_time()
        seven_days_in_seconds = 7 * 24 * 60 * 60
        time_elapsed = time.time() - install_time

        if time_elapsed < seven_days_in_seconds:
            self.engine.set_trial_status(True)
            days_left = int((seven_days_in_seconds - time_elapsed) / 86400) + 1
            return True, f"Trial Active ({days_left} days left)"
        else:
            self.engine.set_trial_status(False)
            return False, "Trial Expired"

    def activate_lemon_squeezy_license(self, license_key):
        """Verifies license with Lemon Squeezy API."""
        url = "https://api.lemonsqueezy.com/v1/licenses/activate"
        payload = {"license_key": license_key, "instance_name": "Fluid_Audio_Desktop"}
        
        try:
            res = requests.post(url, data=payload, headers={"Accept": "application/json"})
            if res.status_code == 200 and res.json().get("activated"):
                self.engine.unlock_premium(self.secret_hash)
                # Save key locally so user doesn't enter it again
                self.save_license_locally(license_key)
                return True
            return False
        except Exception:
            return False

    def save_license_locally(self, key):
        if os.path.exists(self.config_path):
            with open(self.config_path, "r") as f:
                data = json.load(f)
            data["license_key"] = key
            with open(self.config_path, "w") as f:
                json.dump(data, f)

    def process_music(self):
        self.engine.processAudio()

class AudioEngine:
    """
    High-performance wrapper over native C++ AudioEngine (miniaudio + SoundTouch).
    Loads 2-hour long streams instantly and shifts pitch with zero stutter.
    """
    def __init__(self, parent=None):
        self._player = native_audio.AudioEngine()
        self.is_playing = False

    def load_song(self, file_path):
        safe_path = str(os.path.abspath(file_path)) if not file_path.startswith("http") else file_path
        
        if not file_path.startswith("http") and not os.path.exists(safe_path):
            raise FileNotFoundError(f"Audio file not found: {safe_path}")
            
        self._player.load_song(safe_path)

    def play(self):
        self._player.play()
        self.is_playing = True

    def pause(self):
        self._player.pause()
        self.is_playing = False

    def stop(self):
        self._player.stop()
        self.is_playing = False

    def set_frequency(self, frequency: float):
        """
        Updates the resampling pitch shift ratio instantly, 
        leaving song speed entirely untouched.
        """
        self._player.set_frequency(int(frequency))

    def get_duration(self) -> float:
        try:
            return self._player.get_duration()
        except Exception:
            return 0.0

    def get_position(self) -> float:
        try:
            return self._player.get_position()
        except Exception:
            return 0.0

    def set_seeking(self, seconds: float):
        self._player.set_seeking(seconds)

    def seek(self, seconds: float):
        self.set_seeking(seconds)
        
    def set_volume(self, volume: float):
        if hasattr(self._player, 'set_volume'):
            self._player.set_volume(volume)
            
    def enable_eq(self, enabled: bool):
        if hasattr(self._player, 'enable_eq'):
            self._player.enable_eq(enabled)
            
    def set_eq_band(self, index: int, value: float):
        if hasattr(self._player, 'set_eq_band'):
            self._player.set_eq_band(index, value)

    def enable_replaygain(self, enabled: bool):
        if hasattr(self._player, 'enable_replaygain'):
            self._player.enable_replaygain(enabled)
            
    def enable_crossfade(self, enabled: bool):
        if hasattr(self._player, 'enable_crossfade'):
            self._player.enable_crossfade(enabled)
            
    def set_crossfade_duration(self, duration: int):
        if hasattr(self._player, 'set_crossfade_duration'):
            self._player.set_crossfade_duration(duration)

    def append_to_queue(self, path: str):
        if hasattr(self._player, 'append_to_queue'):
            self._player.append_to_queue(path)