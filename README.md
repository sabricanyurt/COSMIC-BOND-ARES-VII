# COSMIC BOND (ARES-VII) — Martian Tactical EVA & Autonomous Navigation HUD

**COSMIC BOND // Sabri Can YURT & İrem Merve YILMAZ — NASA Space Apps Challenge 2026 1st Place Winner**

> **English User Manual & Technical Architecture Guide**

---

## 1. Mission Overview
**COSMIC BOND (ARES-VII)** is an integrated Human-Systems & Planetary Surface Decision Support System designed for Mars EVA (Extravehicular Activity) operations across **Jezero Crater**, **Gale Crater / Mount Sharp Boxwork Ridge**, and **Mawrth Vallis**. The platform fuses real-time NASA Mars Trek orbital imagery layers, numerical MOLA elevation models, physiological EVA suit telemetry, subsurface GPR stratigraphy, and hardware-in-the-loop ESP32 wrist-cuff actuation.

---

## 2. Key Technical Capabilities
- **Mars-Radius Calibrated A* Safe Corridor Engine (`true_grid_astar.py`, `ares_assets/js/map.js`):**
  - Uses Mars planetary sphere Haversine geometry (`R = 3,390,000 m`) so all distances, ETAs, slope gradients, and hazard clearances reflect Martian surface physics rather than Earth-radius map defaults.
  - Dynamically compares **Route A (Direct / Ridge)** vs **Route B (Contour / Safe Defilade Valley)** under suit fatigue, HWI (Human Walkability Index), solar flare CME surges, and dynamic slope/radiation costs, outputting explainable **XAI Rationale** in real time.
- **NASA Multi-Layer Science & Optical Map Drawers:**
  - Interactive top drawers for **Optical Sensors & Sector Switcher** (`MDIM21 Viking`, `MOLA Hillshade`, `THEMIS-IR Thermal`, `MRO CTX 6m`, `MRO HiRISE 0.25m`, FLIR/NVG/SAR shaders), **Science Layer Fusion**, **Solar Time & Seasonal $L_s$ Ephemeris**, and **C4ISR Unit Lock & Swarm Control**.
- **Ergonomic Collapsible HUD (`ALPHA-CMD` Layout) & Live `EN / TR` Switch:**
  - Segmented left & right tabs keep 100% of telemetry modules accessible without screen clutter.
  - Dedicated collapse handles on the left (`◀`), right (`▶`), top map ribbon (`▲`), bottom module dock (`▼ MODÜL DOCK GİZLE`), plus a single-click **`⛶ FULL MAP / TAM HARİTA`** Zen toggle.
  - Instant zero-reload **`🌐 EN / TR`** bilingual toggle persisted in `localStorage`.
- **Subsurface, AI Co-Pilot & Co-Robotic Modules:**
  - **NASA RIMFAX GPR** 0–40 m subsurface echogram simulator (`ares_assets/js/telemetry.js`).
  - **Monte Carlo Stress Tester:** 1,000 stochastic Mars anomaly runs computing survival probability, worst-case $O_2$ reserve, choke points, and bailout shelter azimuths.
  - **3D MOLA Topographic Mesh & 3D Mars Globe Orbitals (`ares_assets/js/lidar3d.js`).**
  - **ESP32 J.A.R.V.I.S. Mark VII Hardware Twin (`sketch.ino`, `eva_cuff_sim.py`):** 128×128 ILI9163C TFT + 12-LED WS2812B Plasma Ring bi-directional wrist cuff simulator.

---

## 3. Installation & Launch

### Prerequisites
- Python 3.10+
- Modern Chromium/Chrome browser

### Setup
```bash
pip install -r requirements.txt
py app.py
```
Then open **`http://127.0.0.1:5000`** in your browser.

---

## 4. Controls & Shortcuts
| Key | Action | Key | Action |
|-----|--------|-----|--------|
| `E` | Lock A* Closest Safe Shelter Corridor | `K` | Trigger Dust Storm Simulation |
| `Z` | Run 1,000x Monte Carlo / Pre-EVA Rehearsal | `R` | Trigger Solar Flare / Radiation Surge |
| `1–3` | Switch Optical / MOLA / THEMIS Layers | `L` | Dispatch Rover Medevac Escort |
| `4–6` | FLIR IR / NVG Green / SAR Shader Modes | `B` | Open 3D LiDAR Topography Scanner |
| `C` | Close All Floating Drawers & Pop-Ups | `F` | Toggle Fullscreen Mode |

---

## 5. Repository Layout
- `app.py` — Flask REST API backend, NASA Mars Trek proxies, telemetry state machine, DSS route & elevation endpoints.
- `terrain_grid_engine.py` — Numerical MOLA DEM terrain cell classifier & slope walkability matrix.
- `true_grid_astar.py` — Multi-objective Martian terrain A* corridor solver.
- `eva_cuff_sim.py` — Hardware state bridge for the ESP32 EVA Wrist Cuff.
- `sketch.ino` — Embedded C++ firmware for ESP32-WROOM-32 + ILI9163C + 12x WS2812B NeoPixel ring.
- `index.html`, `ares_assets/css/hud.css`, `ares_assets/js/` — Tactical Mission Control web frontend.
- `TR_MASTER_BACKUP/` — Preserved Turkish master UI snapshot.
