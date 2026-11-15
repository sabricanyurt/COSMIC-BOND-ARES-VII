# -*- coding: utf-8 -*-
# COSMIC BOND // Sabri Can YURT & İrem Merve YILMAZ — NASA Space Apps Challenge 2026 1st Place Winner

"""
COSMIC BOND — EVA-CUFF HARDWARE EMULATOR & SIMULATOR
NASA Space Apps Challenge 2026 - Interplanetary Survival Guide: Martian Map

Faithfully replicates the ESP32 firmware (sketch.ino), pinouts, ILI9341 display pages,
bi-directional REST synchronization, physical button interrupts, acoustic alerts,
and fail-safe offline mission caching.
"""

import time
import json
import urllib.request
import urllib.error
import math

class EVACuffSimulator:
    def __init__(self, server_url="http://127.0.0.1:5000", device_id="EVA-CUFF-01"):
        self.server_url = server_url
        self.device_id = device_id
        
        # Hardware Pins Emulation
        self.PIN_BTN_ACK = 14
        self.PIN_BTN_NEXT = 27
        self.PIN_BTN_EMG = 32
        
        self.PIN_LED_NOM = 21
        self.PIN_LED_CAUT = 22
        self.PIN_LED_CRIT = 15
        self.PIN_BUZZER = 13
        
        # State Machine
        self.state = "NOMINAL"  # NOMINAL, CAUTION, WARNING, CRITICAL, EMERGENCY
        self.current_page = 0   # 0: Tactical HUD, 1: Nav Corridor, 2: Commands, 3: Cache
        self.comm_link_online = False
        self.network_forced_offline = False
        self.ack_pending = False
        self.emergency_active = False
        
        # Astronaut Vitals & Physical Telemetry
        self.heart_rate_bpm = 78
        self.o2_percent = 91.0
        self.suit_temp_c = 19.8
        self.suit_pressure_kpa = 29.46
        self.battery_percent = 98.4
        self.local_gravity_ms2 = 3.71
        self.local_gravity_g = 0.378
        self.gravity_provenance = "MODELLED (Planetary Gravity Model: 3.71 m/s² / 0.378g)"
        
        # Navigation Corridor
        self.target_name = "REFUGE ALPHA (CURIOSITY)"
        self.target_distance_m = 184.0
        self.target_bearing_deg = 127.0
        self.local_slope_deg = 3.2
        self.route_code = "SMART-B"
        self.action_directive = "ADVANCE ON BEARING 127°"
        self.why_route = "SLOPE REDUCED 8.4° -> 3.9°, RAD BUFFER +120m"
        
        # Active Mission Command
        self.active_command = None
        
        # Fail-Safe Offline Mission Cache
        self.offline_cache = {
            "refuge_name": "REFUGE ALPHA (CURIOSITY ROVER)",
            "refuge_coords": [-5.3850, 137.8380],
            "cached_distance_m": 492.0,
            "cached_bearing_deg": 127.0,
            "cached_route": "SMART-B",
            "checksum_crc32": "0x8F4A2C",
            "is_valid": True
        }
        
        # LED & Buzzer States
        self.leds = {"GREEN": True, "YELLOW": False, "RED": False}
        self.buzzer_tone_hz = 0
        self.mission_start_time = time.time()
        self.boot_diagnostics = []

        # Phase 11: Bi-Directional Hardware-in-the-Loop Twin State
        self.tft_page = 0  # 0: RADAR, 1: 5LAYER_NASA, 2: HORIZON_POV, 3: ORBITAL_DSN
        self.suit_pitch_deg = 4.2
        self.suit_roll_deg = -1.8
        self.accel_g = 0.378
        self.suit_stability_index = 95.0
        self.slip_alert = False
        self.slip_hazard_code = None
        self.joystick_x = 0.0
        self.joystick_y = 0.0
        self.joystick_sel = False
        self.last_vox_command = "STANDBY"
        self.in_defilade = False
        self.neopixel_ring_16 = self.compute_neopixel_ring_16(
            target_bearing_deg=self.target_bearing_deg, hwi=0.84, safety_score=84, in_defilade=False
        )["leds"]

    def run_boot_sequence(self):
        """Simulates aerospace flight computer Power-On Self Test (POST)."""
        self.boot_diagnostics = [
            ("ESP32 DUAL-CORE 240MHz", "OK"),
            ("ILI9341 SPI DISPLAY 320x240", "OK"),
            ("16-LED NEOPIXEL COMPASS RING", "OK"),
            ("MPU6050 6-AXIS IMU (I2C)", "OK"),
            ("ANALOG JOYSTICK + VOX MIC", "OK"),
            ("HAPTIC / ACOUSTIC TRANSDUCER", "OK"),
            ("PHYSICAL INPUT MATRIX (4-BTN)", "OK"),
            ("PLANETARY GRAVITY MODEL", "3.71 m/s² (0.378 G) [MODELLED]"),
            ("OFFLINE MISSION CACHE", "VALID (CRC-32: 0x8F4A2C)"),
            ("MISSION CALLSIGN", "EVA-01 READY")
        ]
        self.state = "NOMINAL"
        self.update_indicators()
        return self.boot_diagnostics

    def update_indicators(self):
        """Updates LEDs and buzzer depending on state."""
        if self.state == "NOMINAL":
            self.leds = {"GREEN": True, "YELLOW": False, "RED": False}
            self.buzzer_tone_hz = 0
        elif self.state == "CAUTION":
            self.leds = {"GREEN": False, "YELLOW": True, "RED": False}
            self.buzzer_tone_hz = 800
        elif self.state == "WARNING":
            self.leds = {"GREEN": False, "YELLOW": True, "RED": False}
            self.buzzer_tone_hz = 1000
        elif self.state in ["CRITICAL", "EMERGENCY"]:
            self.leds = {"GREEN": False, "YELLOW": False, "RED": True}
            self.buzzer_tone_hz = 2200 if self.emergency_active else 1400

    def press_btn_ack(self):
        """Simulates pressing BTN 1 (ACK / SILENCE ALERT)."""
        self.ack_pending = False
        cmd_id = self.active_command.get("command_id") if self.active_command else None
        
        if self.state in ["CAUTION", "WARNING"] and not self.emergency_active:
            self.state = "NOMINAL"
        self.update_indicators()

        if not self.network_forced_offline:
            try:
                payload = json.dumps({"device_id": self.device_id, "command_id": cmd_id}).encode("utf-8")
                req = urllib.request.Request(
                    f"{self.server_url}/api/eva-cuff/ack",
                    data=payload,
                    headers={"Content-Type": "application/json"}
                )
                with urllib.request.urlopen(req, timeout=2) as resp:
                    return json.loads(resp.read().decode("utf-8"))
            except Exception as e:
                return {"status": "LOCAL_ACK_ONLY", "error": str(e)}
        return {"status": "LOCAL_ACK_ONLY", "offline": True}

    def press_btn_next(self):
        """Simulates pressing BTN 2 (CYCLE PAGE)."""
        self.current_page = (self.current_page + 1) % 4
        return self.current_page

    def press_btn_emergency(self):
        """Simulates pressing BTN 3 (EMERGENCY ABORT)."""
        self.emergency_active = True
        self.state = "EMERGENCY"
        self.action_directive = "RETURN TO REFUGE ALPHA IMMEDIATELY"
        self.update_indicators()

        if not self.network_forced_offline:
            try:
                payload = json.dumps({
                    "device_id": self.device_id,
                    "reason": "ASTRONAUT_PHYSICAL_ABORT"
                }).encode("utf-8")
                req = urllib.request.Request(
                    f"{self.server_url}/api/eva-cuff/emergency",
                    data=payload,
                    headers={"Content-Type": "application/json"}
                )
                with urllib.request.urlopen(req, timeout=2) as resp:
                    data = json.loads(resp.read().decode("utf-8"))
                    self.target_distance_m = data.get("distance_m", self.target_distance_m)
                    self.target_bearing_deg = data.get("bearing_deg", self.target_bearing_deg)
                    return data
            except Exception as e:
                # Fallback to local offline cache immediately
                self.target_distance_m = self.offline_cache["cached_distance_m"]
                self.target_bearing_deg = self.offline_cache["cached_bearing_deg"]
                return {"status": "EMERGENCY_LOCAL_FAILSAFE", "error": str(e)}
        return {"status": "EMERGENCY_LOCAL_FAILSAFE", "offline": True}

    def sync_telemetry_uplink(self):
        """Sends 1Hz astronaut biometrics and device status to Mission Control."""
        if self.network_forced_offline:
            self.comm_link_online = False
            return {"status": "OFFLINE", "cached": True}

        try:
            payload = json.dumps({
                "device_id": self.device_id,
                "heart_rate_bpm": self.heart_rate_bpm,
                "o2_percent": self.o2_percent,
                "suit_temp_c": self.suit_temp_c,
                "suit_pressure_kpa": self.suit_pressure_kpa,
                "battery_percent": self.battery_percent,
                "device_status": self.state,
                "emergency_active": self.emergency_active
            }).encode("utf-8")
            
            req = urllib.request.Request(
                f"{self.server_url}/api/eva-cuff/telemetry",
                data=payload,
                headers={"Content-Type": "application/json"}
            )
            with urllib.request.urlopen(req, timeout=2) as resp:
                data = json.loads(resp.read().decode("utf-8"))
                self.comm_link_online = True
                
                # Update target and gravity
                if "target" in data:
                    self.target_distance_m = data["target"].get("distance_m", self.target_distance_m)
                    self.target_bearing_deg = data["target"].get("bearing_deg", self.target_bearing_deg)
                    self.offline_cache["cached_distance_m"] = self.target_distance_m
                    self.offline_cache["cached_bearing_deg"] = self.target_bearing_deg
                
                if "local_gravity" in data:
                    self.local_gravity_ms2 = data["local_gravity"].get("gravity_ms2", self.local_gravity_ms2)
                    self.local_gravity_g = data["local_gravity"].get("gravity_g", self.local_gravity_g)
                
                return data
        except Exception:
            self.comm_link_online = False
            return {"status": "OFFLINE", "cached": True}

    def poll_command_downlink(self):
        """Polls Mission Control DSS for incoming tactical directives."""
        if self.network_forced_offline:
            self.comm_link_online = False
            return {"has_command": False, "offline": True}

        try:
            req = urllib.request.Request(f"{self.server_url}/api/eva-cuff/command")
            with urllib.request.urlopen(req, timeout=2) as resp:
                data = json.loads(resp.read().decode("utf-8"))
                self.comm_link_online = True
                
                if data.get("has_command", False):
                    cmd = data["command"]
                    self.active_command = cmd
                    self.route_code = cmd.get("route_code", self.route_code)
                    self.target_distance_m = cmd.get("distance_m", self.target_distance_m)
                    self.target_bearing_deg = cmd.get("bearing_deg", self.target_bearing_deg)
                    self.why_route = cmd.get("why_route", self.why_route)
                    self.ack_pending = cmd.get("requires_ack", False)
                    
                    if cmd.get("priority") == "CRITICAL":
                        self.state = "CRITICAL"
                    elif cmd.get("priority") == "HIGH":
                        self.state = "WARNING"
                    self.update_indicators()
                return data
        except Exception:
            self.comm_link_online = False
            return {"has_command": False, "offline": True}

    def set_network_offline(self, offline=True):
        """Toggles network severance to test fail-safe offline mission caching."""
        self.network_forced_offline = offline
        self.comm_link_online = not offline

    def render_hud_text(self):
        """Renders an ASCII text-mode flight computer screen matching the ILI9341 layout."""
        elapsed = int(time.time() - self.mission_start_time)
        hh, mm, ss = elapsed // 3600, (elapsed % 3600) // 60, elapsed % 60
        link_str = "[LINK: OK]" if self.comm_link_online else "[COMM LOST - OFFLINE]"
        
        lines = []
        lines.append("╔════════════════════════════════════════════════════════════════════════════╗")
        lines.append(f"║ COSMIC BOND EVA-CUFF v3.0 | T+{hh:02d}:{mm:02d}:{ss:02d} | {link_str:28} ║")
        lines.append("╠════════════════════════════════════════════════════════════════════════════╣")
        
        if self.current_page == 0:
            lines.append(f"║ [PAGE 1/4: TACTICAL HUD]       STATUS: {self.state:15} (LED:{self.get_led_str():8}) ║")
            lines.append("╟────────────────────────────────────────────────────────────────────────────╢")
            lines.append(f"║ O2 RESERVE: {self.o2_percent:5.1f}% [DRAIN: -0.18%/m] │ HEART RATE: {self.heart_rate_bpm:3d} BPM [{self.get_load_str():10}]   ║")
            lines.append(f"║ SUIT PRESS: {self.suit_pressure_kpa:5.2f} kPa             │ SUIT TEMP : {self.suit_temp_c:5.1f}°C                   ║")
            lines.append("╟────────────────────────────────────────────────────────────────────────────╢")
            lines.append(f"║ TARGET    : {self.target_name:32} [ROUTE: {self.route_code:8}] ║")
            lines.append(f"║ GUIDANCE  : DIST: {self.target_distance_m:5.1f}m │ BEARING: {self.target_bearing_deg:5.1f}° │ SLOPE: {self.local_slope_deg:4.1f}°      ║")
            lines.append(f"║ ACTION    : {self.action_directive:58} ║")
            lines.append("╟────────────────────────────────────────────────────────────────────────────╢")
            lines.append(f"║ LOCAL GRAVITY: {self.local_gravity_ms2:4.2f} m/s² ({self.local_gravity_g:5.3f} G) [{self.gravity_provenance[:30]}] ║")
        
        elif self.current_page == 1:
            lines.append("║ [PAGE 2/4: TERRAIN NAV CORRIDOR & BEARING GUIDANCE]                        ║")
            lines.append("╟────────────────────────────────────────────────────────────────────────────╢")
            lines.append(f"║ BEARING TO TARGET: {self.target_bearing_deg:5.1f}°         │ DISTANCE REMAINING: {self.target_distance_m:5.1f}m    ║")
            lines.append(f"║ LOCAL SURFACE GRADIENT: {self.local_slope_deg:4.1f}°     │ TERRAIN CLASS: SAFE BASALT REGOLITH    ║")
            lines.append("╟────────────────────────────────────────────────────────────────────────────╢")
            lines.append(f"║ ROUTE COMPARISON : {self.route_code} vs DIRECT (+169m detour, 0 RAD crossings)     ║")
            lines.append("║ PASSABILITY INDEX: 94.2% OPTIMAL (METABOLIC EXERTION MINIMIZED BY ~34%)    ║")
        
        elif self.current_page == 2:
            lines.append("║ [PAGE 3/4: MISSION DISPATCH & LIFE SUPPORT DIRECTIVES]                     ║")
            lines.append("╟────────────────────────────────────────────────────────────────────────────╢")
            if self.active_command:
                lines.append(f"║ ACTIVE DIRECTIVE: {self.active_command.get('title', 'NONE'):35} [{self.active_command.get('priority', 'NORM'):8}] ║")
                lines.append(f"║ MESSAGE         : {self.active_command.get('message', ''):54} ║")
                lines.append(f"║ WHY ROUTE CHOSEN: {self.why_route:54} ║")
                lines.append(f"║ ACK STATUS      : {'[PRESS BTN 1 TO ACKNOWLEDGE]' if self.ack_pending else 'ACKNOWLEDGED BY CREW':54} ║")
            else:
                lines.append("║ ACTIVE DIRECTIVE: NO PENDING DIRECTIVES                                    ║")
                lines.append("║ All mission directives acknowledged. Autonomous DSS monitoring surface.    ║")
        
        elif self.current_page == 3:
            lines.append("║ [PAGE 4/4: SYSTEM HEALTH & OFFLINE FAIL-SAFE CACHE]                        ║")
            lines.append("╟────────────────────────────────────────────────────────────────────────────╢")
            lines.append(f"║ COMM LINK STATUS  : {'ONLINE (5 GHz REST)' if self.comm_link_online else 'OFFLINE [FAILSAFE ACTIVE]':51} ║")
            lines.append(f"║ CACHED REFUGE     : {self.offline_cache['refuge_name']:51} ║")
            lines.append(f"║ CACHED VECTOR     : {self.offline_cache['cached_distance_m']:.1f}m BEARING {self.offline_cache['cached_bearing_deg']:.1f}° [ROUTE: {self.offline_cache['cached_route']}]          ║")
            lines.append(f"║ CACHE INTEGRITY   : CRC-32 VALID ({self.offline_cache['checksum_crc32']})                                ║")
            lines.append(f"║ BATTERY INTEGRITY : {self.battery_percent:4.1f}% (6.8 HOURS REMAINING)                           ║")
        
        lines.append("╠════════════════════════════════════════════════════════════════════════════╣")
        lines.append("║ [BTN 1: ACK/CONFIRM]   │   [BTN 2: NEXT PAGE]   │   [BTN 3: EMERGENCY]     ║")
        lines.append("╚════════════════════════════════════════════════════════════════════════════╝")
        return "\n".join(lines)

    def get_led_str(self):
        if self.leds["RED"]:
            return "RED"
        elif self.leds["YELLOW"]:
            return "AMBER"
        return "GREEN"

    def get_load_str(self):
        if self.heart_rate_bpm > 110:
            return "CRITICAL"
        elif self.heart_rate_bpm > 95:
            return "ELEVATED"
        return "NOMINAL"

    def compute_neopixel_ring_16(self, target_bearing_deg=None, hwi=0.84, safety_score=84, in_defilade=False):
        """
        Phase 11: Builds the 16-LED WS2812B NeoPixel compass & HWI ring state (22.5 deg per LED).
        LED index 0 marks True North (0 deg), pointer_led_index points to active science/shelter bearing.
        """
        bearing = float(self.target_bearing_deg if target_bearing_deg is None else target_bearing_deg) % 360.0
        pointer_idx = int(round(bearing / 22.5)) % 16
        if hwi < 0.45 or safety_score < 45:
            bg_hex = "#ef4444"
        elif hwi < 0.70 or safety_score < 70:
            bg_hex = "#f59e0b"
        else:
            bg_hex = "#10b981"

        leds = []
        for idx in range(16):
            angle_deg = idx * 22.5
            if idx == pointer_idx:
                color_hex = "#00ff8c" if in_defilade else "#00f2fe"
                role = "DEFILADE_BEACON" if in_defilade else "TARGET_HEADING_POINTER"
            elif idx == 0:
                color_hex = "#ffffff"
                role = "TRUE_NORTH_REF"
            else:
                color_hex = bg_hex
                role = "HWI_CORRIDOR_RING"
            leds.append({
                "index": idx,
                "angle_deg": angle_deg,
                "color_hex": color_hex,
                "role": role,
                "active": (idx == pointer_idx or idx == 0)
            })
        self.neopixel_ring_16 = leds
        return {
            "led_count": 16,
            "degrees_per_led": 22.5,
            "target_bearing_deg": round(bearing, 1),
            "pointer_led_index": pointer_idx,
            "north_led_index": 0,
            "in_defilade": bool(in_defilade),
            "ring_status_color": bg_hex,
            "leds": leds
        }

    def evaluate_mpu6050_imu(self, suit_pitch_deg, suit_roll_deg, terrain_slope_deg=4.2, terrain_aspect_deg=90.0):
        """
        Phase 11: Fuses real MOLA terrain slope/aspect with simulated MPU6050 6-axis IMU suit pitch/roll.
        Triggers SLIP_OR_FALL_HAZARD_DETECTED when |pitch| > 26 deg or |roll| > 24 deg.
        """
        pitch = float(suit_pitch_deg)
        roll = float(suit_roll_deg)
        slope = float(terrain_slope_deg)
        self.suit_pitch_deg = round(pitch, 2)
        self.suit_roll_deg = round(roll, 2)
        combined_tilt = (pitch ** 2 + roll ** 2) ** 0.5
        stability = max(0.0, min(100.0, 100.0 - (combined_tilt * 1.9 + max(0.0, slope - 5.0) * 1.3)))
        self.suit_stability_index = round(stability, 1)

        slip_detected = abs(pitch) > 26.0 or abs(roll) > 24.0
        self.slip_alert = slip_detected
        self.slip_hazard_code = "SLIP_OR_FALL_HAZARD_DETECTED" if slip_detected else None
        if slip_detected:
            self.state = "CRITICAL"
            self.update_indicators()
        elif self.state == "CRITICAL" and not self.emergency_active:
            self.state = "NOMINAL"
            self.update_indicators()

        return {
            "suit_pitch_deg": self.suit_pitch_deg,
            "suit_roll_deg": self.suit_roll_deg,
            "terrain_slope_deg": round(slope, 2),
            "terrain_aspect_deg": round(float(terrain_aspect_deg), 1),
            "accel_g": self.accel_g,
            "suit_stability_index": self.suit_stability_index,
            "slip_alert": slip_detected,
            "hazard_event": self.slip_hazard_code,
            "rescue_recommendation": (
                "DISPATCH CO-ROBOTIC ROVER + PAIRED BUDDY STABILIZATION ASSIST"
                if slip_detected else "SUIT BALANCE NOMINAL FOR 0.378G MARTIAN LOCOMOTION"
            )
        }

    def sync_hardware_twin(self, payload=None):
        """Phase 11: Syncs local simulator with /api/cuff/hardware-twin backend endpoint."""
        url = f"{self.server_url}/api/cuff/hardware-twin"
        try:
            if payload is None:
                req = urllib.request.Request(url, method="GET")
            else:
                body = json.dumps(payload).encode("utf-8")
                req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"}, method="POST")
            with urllib.request.urlopen(req, timeout=4) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except Exception as e:
            return {"status": "ERROR", "error": str(e)}

if __name__ == "__main__":
    cuff = EVACuffSimulator()
    print("Executing EVA-CUFF Boot Sequence:")
    for comp, res in cuff.run_boot_sequence():
        print(f"  {comp:30} ... {res}")
    
    print("\nSimulating Initial Telemetry Sync:")
    telem = cuff.sync_telemetry_uplink()
    print("  Sync result:", telem.get("status"))
    
    print("\nInitial Tactical HUD Display:")
    print(cuff.render_hud_text())

