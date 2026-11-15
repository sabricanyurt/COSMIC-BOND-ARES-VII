// COSMIC BOND // Sabri Can YURT & İrem Merve YILMAZ — NASA Space Apps Challenge 2026 1st Place Winner

/*
  J.A.R.V.I.S. MARK VII  -  ESP32 ARC REACTOR COMMAND DISPLAY
  -----------------------------------------------------------
  Donanım : ESP32-WROOM-32 + 1.44" 128x128 ILI9163C TFT (SPI) + 12 LED WS2812B halka
  Kütüphane: TFT_ILI9163C (sumotoy), Adafruit GFX Library, Adafruit NeoPixel
  Not     : MAX30100 kullanılmaz. POWER / TEMP / ONLINE / SYS vb. değerler
            GERÇEK ÖLÇÜM DEĞİLDİR; yalnızca statik görsel durum göstergeleridir.

  PIN TABLOSU
    TFT SCK  -> GPIO18      TFT SDA (MOSI) -> GPIO23
    TFT A0/DC-> GPIO21      TFT RESET      -> GPIO22
    TFT CS   -> GPIO5       TFT LED/BL     -> 3V3 (veya modülün gerektirdiği şekilde)
    LED DIN  -> GPIO25 (330-470 ohm seri direnç, tercihen 74AHCT125 üzerinden)

  KONTROL
    BOOT düğmesi (GPIO0) veya Seri Monitör (115200): 'a' = ACTIVE, 's' = STANDBY
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <Adafruit_NeoPixel.h>
#include <Fonts/TomThumb.h>   // 3x5 piksel mini font (Adafruit GFX ile birlikte gelir)

// =====================================================================
//  AYARLANABİLİR PARAMETRELER  (hız / parlaklık / renk burada değişir)
// =====================================================================
// --- Pinler ---
#define TFT_SCK_PIN   18
#define TFT_MOSI_PIN  23
#define TFT_DC_PIN    21
#define TFT_RST_PIN   22
#define TFT_CS_PIN    5
#define LED_PIN       25
#define LED_COUNT     12
#define BUTTON_PIN    0          // ESP32 devkit BOOT düğmesi. Kullanmak istemezseniz -1 yapın.

// --- Ekran ayarları ---
#define TFT_ROTATION  0          // 0..3  (yön yanlışsa değiştirin)
#define TFT_SWAP_RB   0          // Kırmızı <-> mavi yer değiştirmişse 1 yapın
#define SHOW_COLOR_TEST 0        // 1: açılışta kırmızı/yeşil/mavi test çubukları (renk sırası kontrolü)

// --- Zamanlama (ms) ---
#define TFT_FRAME_MS      33     // TFT animasyon aralığı (~30 FPS). Büyütürseniz CPU yükü düşer.
#define LED_FRAME_MS      16     // LED animasyon aralığı (~60 FPS)

// --- Animasyon hızları ---
#define SPEED_ACTIVE      1.00f  // ACTIVE modunda genel hız çarpanı
#define SPEED_STANDBY     0.22f  // STANDBY modunda genel hız çarpanı
#define LED_HEAD_LEDS_PER_SEC 6.0f   // Enerji segmentinin hızı (LED/saniye). 6 = saniyede yarım tur
#define LED_TRAIL_LEDS    5.5f   // Segmentin kuyruk uzunluğu (LED)
#define BEAT_HZ           0.80f  // Çekirdek "kalp atışı" frekansı
#define MID_REV_PER_SEC   0.30f  // Orta halkanın ters yönde dönüş hızı (tur/sn)
#define FLARE_PERIOD_S    1.70f  // Beyaz-mavi parlama aralığı (sn)
#define STANDBY_BREATH_MS 4200   // STANDBY nefes periyodu (ms)

// --- Parlaklık ---
#define LED_BRIGHTNESS    90     // 0..255 (USB'den beslerken 60-90 önerilir)

// --- Renkler (R,G,B 0..255) ---
#define COL_RGB_CYAN      0,225,255
#define COL_RGB_DIM       0,60,115
#define COL_RGB_ORANGE    255,130,0

// --- Açılış animasyonu ---
#define BOOT_RING_STEP_MS 30     // Reaktör halkalarının büyüme hızı
#define BOOT_HUD_STEP_MS  110    // HUD parçalarının belirme aralığı
#define BOOT_LED_LAP_MS   650    // Açılışta LED dolaşma turu süresi
#define BOOT_FLASH_MS     650    // Son beyaz-mavi parlama süresi

// =====================================================================
//  RENK YARDIMCILARI
// =====================================================================
constexpr uint16_t C565(uint8_t r, uint8_t g, uint8_t b) {
#if TFT_SWAP_RB
  return ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
#else
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
#endif
}
const uint16_t COL_BG     = 0x0000;
const uint16_t COL_CYAN   = C565(COL_RGB_CYAN);
const uint16_t COL_DIM    = C565(COL_RGB_DIM);
const uint16_t COL_ORANGE = C565(COL_RGB_ORANGE);
const uint16_t COL_FRAME  = C565(0, 120, 200);
const uint16_t COL_BRIGHT = C565(150, 245, 255);
const uint16_t COL_WHT    = C565(235, 250, 255);
const uint16_t COL_GLOW   = C565(0, 70, 170);
const uint16_t COL_WAVE   = C565(60, 220, 255);

struct RGB { uint8_t r, g, b; };
static inline uint8_t u8(float v) { return v < 0 ? 0 : (v > 255 ? 255 : (uint8_t)(v + 0.5f)); }
static inline RGB mk(float r, float g, float b) { RGB c = { u8(r), u8(g), u8(b) }; return c; }
static inline uint16_t toTFT(RGB c) { return C565(c.r, c.g, c.b); }

// Seviye b (0..1) için mavi -> camgöbeği -> beyaz-mavi parlama, tabandan başlayarak
static RGB glowMix(RGB base, float b) {
  float w = b * b; w = w * w * w;                       // b^6 : beyaz sıcak uç
  return mk(base.r * (1 - b) + 190 * w,
            base.g * (1 - b) + 150 * b + 105 * w,
            base.b * (1 - b) + 255 * b);
}

// =====================================================================
//  GLOBAL NESNELER VE DURUM
// =====================================================================
TFT_ILI9163C tft(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

enum ReactorMode { MODE_BOOT, MODE_ACTIVE, MODE_STANDBY };
static ReactorMode mode = MODE_BOOT;

// Ortak sistem saati: hem LED hem TFT bu iki değerden beslenir
static float animPhase = 0;       // saniye cinsinden, mod hızıyla ölçeklenmiş
static float speedCur = 1.0f, speedTarget = 1.0f;
static float gHead = 0;           // enerji segmenti konumu (0..LED_COUNT)
static float gBeat = 0;           // 0..1 çekirdek atışı
static uint32_t lastLoopMs = 0, lastTftMs = 0, lastLedMs = 0;

// =====================================================================
//  GEOMETRİ
// =====================================================================
static const int CX = 64, CY = 64;           // reaktör merkezi
static const int RADAR_CX = 113, RADAR_CY = 52;
static const int WAVE_CY = 118, WAVE_BARS = 11, WAVE_MAX = 7;

// Animasyonlu "hücre" sistemi: sadece rengi değişen hücre yeniden çizilir
enum Layer : uint8_t { L_OUTER, L_MID, L_ORANGE, L_INNER, L_RADAR, L_STATUS, L_BAR, L_HDR };
struct Cell { uint8_t x, y, w, h, layer, k; };
static const int MAX_CELLS = 140;
static Cell cells[MAX_CELLS];
static uint32_t lastCol[MAX_CELLS];
static int nCells = 0;

static void addCell(int x, int y, int w, int h, uint8_t layer, uint8_t k) {
  if (nCells < MAX_CELLS && x >= 0 && y >= 0 && x + w <= 128 && y + h <= 128) {
    Cell c = { (uint8_t)x, (uint8_t)y, (uint8_t)w, (uint8_t)h, layer, k };
    cells[nCells++] = c;
  }
}
static void addRing(uint8_t layer, int n, float cx, float cy, float r, int w, int h, float startDeg) {
  for (int k = 0; k < n; k++) {
    float a = (startDeg + 360.0f * k / n) * DEG_TO_RAD;
    addCell(lroundf(cx + r * sinf(a) - w / 2.0f), lroundf(cy - r * cosf(a) - h / 2.0f), w, h, layer, k);
  }
}
static void buildCells() {
  nCells = 0;
  addRing(L_OUTER,  48, CX, CY, 31.0f, 2, 2, 0);     // dış tarama halkası
  addRing(L_MID,    24, CX, CY, 25.0f, 4, 4, 0);     // orta zırh segmentleri
  addRing(L_ORANGE,  8, CX, CY, 20.0f, 2, 2, 22.5f); // turuncu enerji noktaları
  addRing(L_INNER,  12, CX, CY, 14.0f, 3, 3, 0);     // 12 LED ile birebir eşleşen iç halka
  addRing(L_RADAR,  10, RADAR_CX, RADAR_CY, 5.0f, 2, 2, 0);
  for (int j = 0; j < 4; j++) addCell(104, 80 + 6 * j - 3 + 0, 3, 3, L_STATUS, j);
  for (int j = 0; j < 10; j++) addCell(6, 39 + 3 * (9 - j), 16, 2, L_BAR, j);
  addCell(5, 13, 3, 3, L_HDR, 0);
  addCell(120, 13, 3, 3, L_HDR, 1);
  for (int i = 0; i < MAX_CELLS; i++) lastCol[i] = 0xFFFFFFFF;
}

// =====================================================================
//  ÇİZİM YARDIMCILARI
// =====================================================================
static inline void px(int x, int y, uint16_t c) {
  if (x >= 0 && y >= 0 && x < 128 && y < 128) tft.drawPixel(x, y, c);
}

// 3x5 mini font. y = taban çizgisi. Font verisi doğrudan GFXfont yapısından okunur.
static int tinyText(int x, int baseY, const char* s, uint16_t col) {
  const GFXfont* f = &TomThumb;
  for (; *s; s++) {
    uint8_t c = (uint8_t)*s;
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < f->first || c > f->last) { x += 4; continue; }
    const GFXglyph* g = &f->glyph[c - f->first];
    const uint8_t* bm = f->bitmap + g->bitmapOffset;
    uint8_t bits = 0, bit = 0;
    for (uint8_t yy = 0; yy < g->height; yy++)
      for (uint8_t xx = 0; xx < g->width; xx++) {
        if (!(bit++ & 7)) bits = *bm++;
        if (bits & 0x80) px(x + g->xOffset + xx, baseY + g->yOffset + yy, col);
        bits <<= 1;
      }
    x += g->xAdvance;
  }
  return x;
}

// Varsayılan 6x8 fontla harf aralığı kontrollü yazı (arka plan şeffaf)
static void spacedText(int x, int y, const char* s, uint16_t col, uint8_t size, int pitch) {
  for (; *s; s++, x += pitch) tft.drawChar(x, y, *s, col, col, size);
}

static void chamfer(int x, int y, int w, int h, int c, uint16_t col) {
  tft.drawFastHLine(x + c, y, w - 2 * c, col);
  tft.drawFastHLine(x + c, y + h - 1, w - 2 * c, col);
  tft.drawFastVLine(x, y + c, h - 2 * c, col);
  tft.drawFastVLine(x + w - 1, y + c, h - 2 * c, col);
  tft.drawLine(x, y + c, x + c, y, col);
  tft.drawLine(x + w - 1 - c, y, x + w - 1, y + c, col);
  tft.drawLine(x, y + h - 1 - c, x + c, y + h - 1, col);
  tft.drawLine(x + w - 1 - c, y + h - 1, x + w - 1, y + h - 1 - c, col);
}
// Parlak köşe vurguları
static void accentCorners(int x, int y, int w, int h, int len, uint16_t col) {
  tft.drawFastHLine(x + 3, y, len, col);
  tft.drawFastHLine(x + w - 3 - len, y + h - 1, len, col);
}

// =====================================================================
//  DURUM / MOD METNİ
// =====================================================================
static void drawModeText() {
  tft.fillRect(103, 36, 22, 7, COL_BG);
  if (mode == MODE_STANDBY) tinyText(104, 41, "IDLE", COL_ORANGE);
  else                      tinyText(104, 41, "ACTIVE", COL_BRIGHT);
}

// =====================================================================
//  STATİK HUD  (parça parça çizilir; açılışta animasyonu bloke etmez)
// =====================================================================
static const int HUD_PARTS = 6;

static void drawStaticHUD(int part) {
  switch (part) {
    case 0: {  // Dış çerçeve köşe braketleri + kenar işaretleri
      const int L = 9;
      tft.drawFastHLine(0, 0, L, COL_FRAME);   tft.drawFastVLine(0, 0, L, COL_FRAME);
      tft.drawFastHLine(128 - L, 0, L, COL_FRAME);  tft.drawFastVLine(127, 0, L, COL_FRAME);
      tft.drawFastHLine(0, 127, L, COL_FRAME); tft.drawFastVLine(0, 128 - L, L, COL_FRAME);
      tft.drawFastHLine(128 - L, 127, L, COL_FRAME); tft.drawFastVLine(127, 128 - L, L, COL_FRAME);
      for (int y = 40; y < 90; y += 10) { px(0, y, COL_DIM); px(127, y, COL_DIM); }
      break;
    }
    case 1: {  // Başlık
      chamfer(14, 1, 100, 24, 4, COL_FRAME);
      accentCorners(14, 1, 100, 24, 8, COL_BRIGHT);
      tft.drawFastHLine(17, 24, 4, COL_ORANGE);  tft.drawFastHLine(107, 24, 4, COL_ORANGE);
      spacedText(18, 6, "J.A.R.V.I.S.", COL_GLOW, 1, 8);     // gölge
      spacedText(17, 5, "J.A.R.V.I.S.", COL_WHT, 1, 8);
      spacedText(34, 15, "MARK VII", COL_CYAN, 1, 8);
      tinyText(3, 10, "SYS", COL_CYAN);                       // statik etiketler
      tinyText(116, 10, "ESP", COL_CYAN);
      break;
    }
    case 2: {  // Sol paneller: CORE + TEMP  (değerler statiktir)
      chamfer(1, 27, 26, 55, 4, COL_FRAME);
      accentCorners(1, 27, 26, 55, 6, COL_BRIGHT);
      tinyText(5, 34, "CORE", COL_CYAN);
      tft.drawRect(4, 37, 20, 33, COL_DIM);
      tinyText(5, 78, "100%", COL_BRIGHT);

      chamfer(1, 84, 26, 18, 3, COL_FRAME);
      tinyText(5, 90, "TEMP", COL_CYAN);
      spacedText(5, 92, "24", COL_BRIGHT, 1, 6);              // statik görsel değer
      tft.drawRect(17, 92, 2, 2, COL_BRIGHT);
      spacedText(20, 92, "C", COL_BRIGHT, 1, 6);
      break;
    }
    case 3: {  // Sağ paneller: MODE + radar + STATUS
      chamfer(101, 27, 26, 37, 4, COL_FRAME);
      accentCorners(101, 27, 26, 37, 6, COL_BRIGHT);
      tinyText(104, 33, "MODE", COL_CYAN);
      tft.drawFastHLine(103, 35, 22, COL_DIM);
      drawModeText();
      tft.drawCircle(RADAR_CX, RADAR_CY, 9, COL_DIM);
      tft.drawCircle(RADAR_CX, RADAR_CY, 3, COL_DIM);
      tft.fillRect(RADAR_CX - 1, RADAR_CY - 1, 2, 2, COL_BRIGHT);

      chamfer(101, 66, 26, 36, 4, COL_FRAME);
      tinyText(104, 73, "STATUS", COL_CYAN);
      tft.drawFastHLine(103, 75, 22, COL_DIM);
      tinyText(110, 80, "SYS",  COL_BRIGHT);
      tinyText(110, 86, "SENS", COL_BRIGHT);
      tinyText(110, 92, "DISP", COL_BRIGHT);
      tinyText(110, 98, "PWR",  COL_BRIGHT);
      break;
    }
    case 4: {  // Alt panel: CORE STATUS / ONLINE + ses dalgası merkez noktaları
      chamfer(1, 103, 126, 24, 5, COL_FRAME);
      accentCorners(1, 103, 126, 24, 10, COL_BRIGHT);
      tinyText(42, 110, "CORE STATUS", COL_CYAN);
      tft.drawFastHLine(8, 107, 28, COL_DIM);  tft.drawFastHLine(92, 107, 28, COL_DIM);
      spacedText(30, 113, "ONLINE", COL_GLOW, 2, 12);         // gölge
      spacedText(29, 112, "ONLINE", COL_WHT, 2, 12);
      for (int i = 0; i < WAVE_BARS; i++) { px(6 + 2 * i, WAVE_CY, COL_WAVE); px(101 + 2 * i, WAVE_CY, COL_WAVE); }
      break;
    }
    case 5: {  // Merkez reaktörün statik katmanları
      // Sekiz adet kol (spoke)
      for (int k = 0; k < 8; k++) {
        float a = k * 45.0f * DEG_TO_RAD;
        tft.drawLine(CX + lroundf(11 * sinf(a)), CY - lroundf(11 * cosf(a)),
                     CX + lroundf(28 * sinf(a)), CY - lroundf(28 * cosf(a)), C565(0, 70, 130));
      }
      tft.drawCircle(CX, CY, 10, C565(0, 130, 220));
      tft.drawCircle(CX, CY, 17, C565(0, 50, 100));
      tft.drawCircle(CX, CY, 29, C565(0, 50, 100));
      tft.drawCircle(CX, CY, 34, C565(0, 80, 150));
      // 30 derecelik tik işaretleri (eksenler hariç)
      for (int k = 0; k < 12; k++) {
        if (k % 3 == 0) continue;
        float a = k * 30.0f * DEG_TO_RAD;
        tft.drawLine(CX + lroundf(34 * sinf(a)), CY - lroundf(34 * cosf(a)),
                     CX + lroundf(36 * sinf(a)), CY - lroundf(36 * cosf(a)), COL_FRAME);
      }
      // Hedefleme üçgenleri (4 eksen)
      tft.fillTriangle(61, 26, 67, 26, 64, 30, COL_CYAN);
      tft.fillTriangle(61, 101, 67, 101, 64, 98, COL_CYAN);
      tft.fillTriangle(28, 61, 28, 67, 31, 64, COL_CYAN);
      tft.fillTriangle(100, 61, 100, 67, 97, 64, COL_CYAN);
      break;
    }
  }
}

// =====================================================================
//  LED RENGİ  (LED halkası VE TFT iç halkası aynı fonksiyonu kullanır)
// =====================================================================
static uint32_t bootT0 = 0;
static uint32_t bootFlashT0 = 0;
static bool bootFlashing = false;

static RGB bootLedPixel(int i, uint32_t now) {
  uint32_t t = now - bootT0;
  RGB c = { 0, 0, 0 };
  if (bootFlashing) {                         // son beyaz-mavi parlama
    float u = (float)(now - bootFlashT0) / BOOT_FLASH_MS; if (u > 1) u = 1;
    float lv = 1.0f - u;
    return mk(30 + 190 * lv, 100 + 130 * lv, 255);
  }
  float head = fmodf((float)t / BOOT_LED_LAP_MS * LED_COUNT, LED_COUNT);
  float dist = head - i; if (dist < 0) dist += LED_COUNT;
  float b = (dist < 4.0f) ? (1.0f - dist / 4.0f) : 0;
  if (t > (uint32_t)((i + 1) * BOOT_LED_LAP_MS / LED_COUNT)) c = mk(0, 25, 110);   // yanan LED'ler kalır
  return glowMix(c, b * b);
}

static RGB ledColor(int i, uint32_t now) {
  if (mode == MODE_BOOT) return bootLedPixel(i, now);

  if (mode == MODE_STANDBY) {                 // nefes alan mavi halka
    float ph = (float)(now % STANDBY_BREATH_MS) / STANDBY_BREATH_MS - i * 0.012f;
    float br = 0.5f - 0.5f * cosf(TWO_PI * ph);
    br = 0.04f + 0.96f * br * br;
    float w = br * br * br;
    return mk(40 * w, 40 + 70 * br, 255 * br);
  }

  // MARK VII ACTIVE
  float dist = gHead - i; if (dist < 0) dist += LED_COUNT;
  float b = 0;
  if (dist < LED_TRAIL_LEDS) { b = 1.0f - dist / LED_TRAIL_LEDS; b *= b; }
  else if (dist > LED_COUNT - 1) b = dist - (LED_COUNT - 1);          // başın hemen önü (yumuşatma)

  bool orange = (i == 3 || i == 9);
  float amp = 0.55f + 0.45f * gBeat;                                     // ritmik parlaklık
  RGB base = mk(0, 6 * amp, 34 * amp * (orange ? 0.3f : 1.0f));         // koyu mavi taban
  RGB c = glowMix(base, b);
  float k = 0.75f + 0.25f * gBeat;
  c = mk(c.r * k, c.g * k, c.b * k);

  float add_r = 0, add_g = 0, add_b = 0;
  // Aralıklı beyaz-mavi parlama
  float fp = animPhase / FLARE_PERIOD_S;
  int slot = (int)fp; float ph = fp - slot;
  if (ph < 0.12f && i == (slot * 5 + 3) % LED_COUNT) {
    float lv = 1.0f - ph / 0.12f;
    add_r += 140 * lv; add_g += 180 * lv; add_b += 255 * lv;
  }
  // Kontrollü turuncu vurgu (yalnızca 2 LED, yavaş nabız)
  if (orange) {
    float ol = 0.4f + 0.6f * (0.5f + 0.5f * sinf(TWO_PI * animPhase * 0.35f + (i == 9 ? PI : 0)));
    add_r += 95 * ol * (1 - b); add_g += 26 * ol * (1 - b);
  }
  return mk(c.r + add_r, c.g + add_g, c.b + add_b);
}

// =====================================================================
//  TFT HÜCRE RENKLERİ
// =====================================================================
static inline float quant(float v, int levels) { return floorf(v * levels + 0.5f) / levels; }

static uint16_t cellColor(const Cell& c) {
  const bool act = (mode != MODE_STANDBY);
  switch (c.layer) {
    case L_OUTER: {                                   // LED başıyla aynı konumda ilerleyen yay
      float headCell = gHead / LED_COUNT * 48.0f;
      float d = headCell - c.k; if (d < 0) d += 48;
      float b = 0;
      if (d < 16) { b = 1.0f - d / 16.0f; b *= b; }
      else if (d > 47) b = d - 47;
      return toTFT(glowMix(mk(0, 26, 66), quant(b, 12)));
    }
    case L_MID: {                                     // ters yönde dönen dalga
      float s = 0.5f + 0.5f * sinf(TWO_PI * (2.0f * c.k / 24.0f + animPhase * MID_REV_PER_SEC));
      float b = quant(s * s, 6);
      float m = act ? 1.0f : 0.6f;
      return toTFT(mk((0 + 30 * b) * m, (34 + 140 * b) * m, (84 + 170 * b) * m));
    }
    case L_ORANGE: {                                  // turuncu enerji noktaları
      float s = 0.5f + 0.5f * sinf(TWO_PI * animPhase * 0.5f + c.k * 0.9f);
      float b = quant(0.3f + 0.7f * s, 6) * (act ? 1.0f : 0.5f);
      return toTFT(mk(255 * b, 120 * b, 0));
    }
    case L_INNER: {                                   // 12 fiziksel LED ile birebir
      RGB l = ledColor(c.k, millis());
      return toTFT(mk(quant(l.r / 255.0f, 10) * 255, quant(l.g / 255.0f, 10) * 255, quant(l.b / 255.0f, 10) * 255));
    }
    case L_RADAR: {
      float h = fmodf(animPhase * 0.9f * 10.0f, 10.0f);
      float d = h - c.k; if (d < 0) d += 10;
      float b = (d < 7) ? (1.0f - d / 7.0f) : 0;
      return toTFT(glowMix(mk(0, 25, 55), quant(b * b, 8)));
    }
    case L_STATUS: {                                  // statik durum LED'leri (yalnızca görsel)
      float s = quant(0.6f + 0.4f * sinf(TWO_PI * animPhase * 0.5f + c.k * 1.2f), 6);
      return toTFT(mk(0, 255 * s, 200 * s));
    }
    case L_BAR: {                                     // CORE barı: yukarı akan parlaklık dalgası
      float s = quant(0.65f + 0.35f * sinf(TWO_PI * (animPhase * 0.8f - c.k / 10.0f)), 6);
      return toTFT(mk(90 * s, 240 * s, 255 * s));
    }
    default: {                                        // L_HDR: başlık LED'leri
      float s = (c.k == 0) ? quant(0.7f + 0.3f * sinf(TWO_PI * animPhase * 0.4f), 6)
                           : ((fmodf(animPhase, 1.4f) < 0.7f) ? 1.0f : 0.25f);
      return toTFT(mk(0, 255 * s, 215 * s));
    }
  }
}

// =====================================================================
//  ÇEKİRDEK VE SES DALGALARI
// =====================================================================
static int8_t coreLevelDrawn = -1;
static int8_t waveH[2][11];

static void drawCore(int level) {          // level 0..6
  float t = level / 6.0f;
  tft.fillCircle(CX, CY, 8, toTFT(mk(0, 55 + 55 * t, 140 + 115 * t)));
  tft.fillCircle(CX, CY, 5, toTFT(mk(30 + 80 * t, 140 + 80 * t, 230 + 25 * t)));
  tft.fillCircle(CX, CY, 2, toTFT(mk(190 + 65 * t, 235 + 20 * t, 255)));
}

static void updateWaves() {
  float amp = (mode == MODE_STANDBY) ? 0.30f : 1.0f;
  for (int side = 0; side < 2; side++) {
    for (int j = 0; j < WAVE_BARS; j++) {
      int o = (side == 0) ? j : (WAVE_BARS - 1 - j);            // 0 = dış kenar, N-1 = merkeze yakın
      int x = (side == 0) ? (6 + 2 * j) : (101 + 2 * j);
      float env = 0.2f + 0.8f * o / (float)(WAVE_BARS - 1);
      float v = 0.55f * sinf(TWO_PI * animPhase * 1.1f + o * 0.9f)
              + 0.45f * sinf(TWO_PI * animPhase * 1.9f - o * 1.7f + side);
      v = 0.5f + 0.5f * v;
      int h = (int)lroundf(1 + env * amp * v * (WAVE_MAX - 1));
      int old = waveH[side][j];
      if (h == old) continue;
      uint16_t col = (h > old) ? COL_WAVE : COL_BG;
      int lo = min(h, old) + 1, hi = max(h, old);
      tft.fillRect(x, WAVE_CY - hi, 1, hi - lo + 1, col);
      tft.fillRect(x, WAVE_CY + lo, 1, hi - lo + 1, col);
      waveH[side][j] = h;
    }
  }
}

static void drawReactor() {                // hücre tabanlı reaktör katmanları
  for (int i = 0; i < nCells; i++) {
    uint16_t col = cellColor(cells[i]);
    if (lastCol[i] != col) {
      lastCol[i] = col;
      tft.fillRect(cells[i].x, cells[i].y, cells[i].w, cells[i].h, col);
    }
  }
  float amp = (mode == MODE_STANDBY) ? 0.35f : 1.0f;
  int lv = (int)lroundf((0.35f + 0.65f * gBeat) * amp * 6.0f);
  if (mode == MODE_BOOT && bootFlashing) lv = 6;
  if (lv != coreLevelDrawn) { coreLevelDrawn = lv; drawCore(lv); }
}

static void drawSystemPanels() { updateWaves(); }

static void updateTFTAnimation() {
  drawReactor();
  drawSystemPanels();
}

// =====================================================================
//  LED GÜNCELLEME
// =====================================================================
static void updateLEDRing(uint32_t now) {
  for (int i = 0; i < LED_COUNT; i++) {
    RGB c = ledColor(i, now);
    strip.setPixelColor(i, strip.Color(c.r, c.g, c.b));
  }
  strip.show();
}

// =====================================================================
//  MOD YÖNETİMİ
// =====================================================================
static void setReactorMode(ReactorMode m) {
  mode = m;
  speedTarget = (m == MODE_STANDBY) ? SPEED_STANDBY : SPEED_ACTIVE;
  if (m != MODE_BOOT) drawModeText();
  Serial.println(m == MODE_ACTIVE ? "MODE: MARK VII ACTIVE" : (m == MODE_STANDBY ? "MODE: STANDBY" : "MODE: BOOT"));
}

// =====================================================================
//  AÇILIŞ ANİMASYONU (millis tabanlı durum makinesi, delay yok)
// =====================================================================
enum BootStage { B_RINGS, B_HUD, B_FLASH };
static BootStage bootStage = B_RINGS;
static int bootR = 2, bootPart = 0;
static uint32_t bootNext = 0;

static void runBootAnimation(uint32_t now) {
  if (now - lastLedMs >= LED_FRAME_MS) { lastLedMs = now; updateLEDRing(now); }
  if (now < bootNext) return;

  switch (bootStage) {
    case B_RINGS: {                          // merkezden dışa büyüyen enerji halkaları
      float t = bootR / 34.0f;
      tft.drawCircle(CX, CY, bootR, toTFT(mk(0, 60 + 160 * t, 140 + 115 * t)));
      if (bootR % 6 == 2) tft.drawCircle(CX, CY, bootR - 1, toTFT(mk(180 * (1 - t), 220, 255)));
      bootR++;
      bootNext = now + BOOT_RING_STEP_MS;
      if (bootR > 34) { bootStage = B_HUD; bootPart = 0; bootNext = now + 150; tft.fillScreen(COL_BG); }
      break;
    }
    case B_HUD: {                            // HUD parça parça belirir
      drawStaticHUD(bootPart++);
      bootNext = now + BOOT_HUD_STEP_MS;
      if (bootPart >= HUD_PARTS) {
        bootStage = B_FLASH; bootFlashing = true; bootFlashT0 = now;
        for (int i = 0; i < MAX_CELLS; i++) lastCol[i] = 0xFFFFFFFF;
        coreLevelDrawn = -1;
        memset(waveH, 0, sizeof(waveH));
      }
      break;
    }
    case B_FLASH: {
      updateTFTAnimation();
      bootNext = now + TFT_FRAME_MS;
      if (now - bootFlashT0 >= BOOT_FLASH_MS) {
        bootFlashing = false;
        setReactorMode(MODE_ACTIVE);          // varsayılan: MARK VII ACTIVE
      }
      break;
    }
  }
}

// =====================================================================
//  GİRİŞ (düğme + seri)
// =====================================================================
static void handleInput(uint32_t now) {
  static bool lastBtn = true; static uint32_t lastChange = 0;
  if (mode == MODE_BOOT) return;
  if (BUTTON_PIN >= 0) {
    bool b = digitalRead(BUTTON_PIN);
    if (b != lastBtn && now - lastChange > 60) {
      lastChange = now; lastBtn = b;
      if (!b) setReactorMode(mode == MODE_ACTIVE ? MODE_STANDBY : MODE_ACTIVE);
    }
  }
  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'a' || c == 'A') setReactorMode(MODE_ACTIVE);
    if (c == 's' || c == 'S') setReactorMode(MODE_STANDBY);
  }
}

// =====================================================================
//  SETUP / LOOP
// =====================================================================
static void updateClock(uint32_t now) {
  float dt = (now - lastLoopMs) * 0.001f;
  lastLoopMs = now;
  if (dt > 0.1f) dt = 0.1f;
  speedCur += (speedTarget - speedCur) * 0.06f;           // modlar arası yumuşak geçiş
  animPhase += dt * speedCur;
  gHead = fmodf(animPhase * LED_HEAD_LEDS_PER_SEC, (float)LED_COUNT);
  float s = 0.5f + 0.5f * sinf(TWO_PI * animPhase * BEAT_HZ);
  gBeat = s * s;
}

void setup() {
  Serial.begin(115200);
  if (BUTTON_PIN >= 0) pinMode(BUTTON_PIN, INPUT_PULLUP);

  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.clear();
  strip.show();

  SPI.begin(TFT_SCK_PIN, -1, TFT_MOSI_PIN, -1);           // VSPI: SCK=18, MOSI=23 (MISO kullanılmıyor)
  tft.begin();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(COL_BG);

#if SHOW_COLOR_TEST
  tft.fillRect(0, 0, 43, 128, C565(255, 0, 0));
  tft.fillRect(43, 0, 42, 128, C565(0, 255, 0));
  tft.fillRect(85, 0, 43, 128, C565(0, 0, 255));
  delay(2000);                                            // yalnızca test için
  tft.fillScreen(COL_BG);
#endif

  buildCells();
  memset(waveH, 0, sizeof(waveH));
  bootT0 = millis();
  lastLoopMs = bootT0;
  bootNext = bootT0 + 200;
  mode = MODE_BOOT;
  speedCur = speedTarget = SPEED_ACTIVE;
  Serial.println("J.A.R.V.I.S. MARK VII - boot");
}

void loop() {
  uint32_t now = millis();
  updateClock(now);
  handleInput(now);

  if (mode == MODE_BOOT) {
    runBootAnimation(now);
    return;
  }
  if (now - lastLedMs >= LED_FRAME_MS) { lastLedMs = now; updateLEDRing(now); }
  if (now - lastTftMs >= TFT_FRAME_MS) { lastTftMs = now; updateTFTAnimation(); }
}
