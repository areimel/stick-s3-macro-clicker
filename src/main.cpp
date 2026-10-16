#include <Arduino.h>
#include <M5Unified.h>
#include "BleKeyboard.h"
#include "Controller.h"
#include "MacroPlayer.h"
#include "ProfileStore.h"
#include "Ui.h"
#include "generated_config.h"

namespace {
using namespace MacroConfig;
MacroKeyboard keyboard;
MacroPlayer player;
ProfileStore store;
Controller controller(DEBOUNCE_MS, DOUBLE_CLICK_MS, LONG_PRESS_MS, PROFILE_COUNT);
Ui ui(M5.Display);
UiModel shown;
int battery = -1;
bool charging = false;
uint32_t lastBattery = 0;
uint32_t lastScreen = 0;
LinkState previousLink = LinkState::SEARCHING;
uint32_t lastGeneration = 0;

const char* const kButtonNames[] = {"A", "B"};
const char* const kGestureNames[] = {"single", "double", "long"};

SlotMask maskFor(uint8_t profile) {
  SlotMask mask = {};
  for (uint8_t b = 0; b < BUTTON_COUNT; ++b)
    for (uint8_t g = 0; g < GESTURE_COUNT; ++g)
      mask.assigned[b][g] = PROFILES[profile].slots[b][g].count > 0;
  return mask;
}

LinkState linkState() {
  return keyboard.ready() ? LinkState::SECURE
         : keyboard.connected() ? LinkState::HANDSHAKE
                                : LinkState::SEARCHING;
}

const char* linkName(LinkState link) {
  return link == LinkState::SECURE ? "READY" : link == LinkState::HANDSHAKE ? "CONNECTING" : "ADVERTISING";
}

void stopPlayback(const char* reason) {
  if (!player.active()) return;
  player.abort();
  keyboard.releaseAll();
  Serial.printf("Macro aborted: %s\n", reason);
}

void readBattery() {
  battery = M5.Power.getBatteryLevel();
  if (battery > 100) battery = 100;
  charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  if (battery >= 0) keyboard.setBatteryLevel(battery);
  Serial.printf("Battery: %d%%, %dmV, charging=%d\n", battery,
                M5.Power.getBatteryVoltage(), charging);
}

void startupBondReset() {
  // Only enter this maintenance action when both application buttons are held
  // at boot. No macro input is processed until they are released.
  M5.update();
  if (!M5.BtnA.isPressed() || !M5.BtnB.isPressed()) return;
  const uint32_t started = millis();
  while (M5.BtnA.isPressed() && M5.BtnB.isPressed()) {
    const uint32_t elapsed = millis() - started;
    if (elapsed >= 3000) {
      keyboard.clearBonds();
      Serial.println("Bluetooth bonds cleared by startup A+B hold");
      ui.bondReset(0, true);
      while (M5.BtnA.isPressed() || M5.BtnB.isPressed()) {
        M5.update();
        delay(10);
      }
      break;
    }
    ui.bondReset(3000 - elapsed, false);
    delay(20);
    M5.update();
  }
}

void handle(const ControllerEvent& ev, uint32_t now) {
  switch (ev.type) {
    case ControllerEvent::PLAY: {
      const Macro& macro = PROFILES[ev.profile].slots[ev.button][ev.gesture];
      const bool started = player.start(macro, now);
      Serial.printf("Macro %s %s %s (%u strokes): %s\n", kButtonNames[ev.button],
                    kGestureNames[ev.gesture], macro.label, macro.count,
                    started ? "started" : "discarded");
      break;
    }
    case ControllerEvent::ABORT:
      stopPlayback("button press");
      break;
    case ControllerEvent::PICKER_OPENED:
      stopPlayback("profile picker");
      Serial.println("Profile picker opened");
      break;
    case ControllerEvent::PICKER_SELECTED:
      controller.setActiveProfile(ev.profile, maskFor(ev.profile));
      store.save(ev.profile);
      Serial.printf("Profile %u selected: %s\n", ev.profile + 1, PROFILES[ev.profile].name);
      break;
    case ControllerEvent::PICKER_CANCELLED:
      Serial.println("Profile picker cancelled");
      break;
    default:
      break;
  }
}

UiModel model() {
  UiModel m;
  m.screen = player.active() ? Screen::TRANSMIT
             : controller.mode() == Mode::PICKER ? Screen::PICKER
                                                 : Screen::HOME;
  m.link = linkState();
  m.battery = battery;
  m.charging = charging;
  m.deviceName = DEVICE_NAME;
  m.profileIndex = controller.activeProfile();
  m.profileCount = PROFILE_COUNT;
  m.profiles = PROFILES;
  m.highlighted = controller.highlighted();
  if (player.active() && player.macro()) {
    m.macroLabel = player.macro()->label;
    m.sent = player.sent();
    m.total = player.total();
  }
  return m;
}

void refreshScreen(uint32_t now) {
  const UiModel next = model();
  const uint32_t since = now - lastScreen;
  // Throttle progress-only redraws so drawing never paces the keystrokes.
  const bool progressOnly = next.screen == Screen::TRANSMIT && shown.screen == Screen::TRANSMIT;
  // Unlinked screens carry a blinking status dot, so refresh them faster.
  const uint32_t idleRefresh = next.link == LinkState::SECURE ? 1000 : 250;
  if ((next != shown && (!progressOnly || since >= 100)) || since >= idleRefresh) {
    ui.render(next);
    shown = next;
    lastScreen = now;
  }
  if (next.link != previousLink) {
    Serial.printf("Bluetooth state: %s\n", linkName(next.link));
    previousLink = next.link;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.output_power = false;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(BRIGHTNESS);
  // Controller owns debounce, so M5Unified only supplies current button state.
  M5.BtnA.setDebounceThresh(0);
  M5.BtnB.setDebounceThresh(0);
  ui.begin();
  Serial.printf("Stick-S3 Macro Clicker boot; board=%d (expected %d), flash=%u, PSRAM=%u, profiles=%u\n",
                int(M5.getBoard()), int(m5::board_t::board_M5StickS3),
                ESP.getFlashChipSize(), ESP.getPsramSize(), PROFILE_COUNT);
  if (M5.getBoard() != m5::board_t::board_M5StickS3) {
    Serial.println("ERROR: expected StickS3; keyboard disabled");
    ui.fatal("UNEXPECTED BOARD. EXPECTED M5STACK STICK-S3. CHECK SERIAL.");
    while (true) delay(1000);
  }
  keyboard.begin(DEVICE_NAME, KEYSTROKE_MS);
  ui.boot(DEVICE_NAME);
  startupBondReset();
  const uint8_t profile = store.load(PROFILE_COUNT);
  Serial.printf("Active profile %u: %s\n", profile + 1, PROFILES[profile].name);
  M5.update();
  controller.begin(M5.BtnA.isPressed(), M5.BtnB.isPressed(), millis(), profile, maskFor(profile));
  readBattery();
  refreshScreen(millis());
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  const bool ready = keyboard.ready();
  const uint32_t generation = keyboard.linkGeneration();
  // Input during disconnection is discarded and a macro never resumes later.
  // Any link change (even a quick resubscribe) may have dropped a stroke.
  if (!ready) stopPlayback("link lost");
  else if (generation != lastGeneration) stopPlayback("link changed");
  lastGeneration = generation;
  handle(controller.update(M5.BtnA.isPressed(), M5.BtnB.isPressed(), now, ready, player.active()), now);
  Stroke stroke;
  if (player.next(now, keyboard.idle(), stroke) && !keyboard.send(stroke.mods, stroke.usage)) {
    stopPlayback("transport rejected stroke");
  }
  keyboard.update(now);
  if (uint32_t(now - lastBattery) >= 30000) {
    lastBattery = now;
    readBattery();
  }
  refreshScreen(now);
  delay(1);
}
