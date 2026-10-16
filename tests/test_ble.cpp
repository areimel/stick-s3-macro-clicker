// Compile the production transport against controllable BLE/clock substitutes.
#include "BleKeyboard.h"
#include <Arduino.h>
#include <algorithm>
#include <cassert>
#include <iostream>

struct Fixture {
  MacroKeyboard keyboard;
  NimBLEConnInfo peer;
  explicit Fixture(uint16_t keystrokeMs = 25) {
    fakeMillis = 100;
    keyboard.begin("Test macro", keystrokeMs);
  }
  ~Fixture() { delete NimBLEHIDDevice::last; }
  NimBLECharacteristic& input() { return NimBLEHIDDevice::last->input; }
  void connect() { NimBLEDevice::server.callbacks->onConnect(&NimBLEDevice::server, peer); }
  void encrypt(bool value = true) {
    peer.encrypted = value;
    NimBLEDevice::server.callbacks->onAuthenticationComplete(peer);
  }
  void subscribe(uint16_t value = 1) { input().callbacks->onSubscribe(&input(), peer, value); }
  void disconnect() {
    NimBLEDevice::server.callbacks->onDisconnect(&NimBLEDevice::server, peer, 0);
  }
  void tick(uint32_t t) { fakeMillis = t; keyboard.update(t); }
  void ready() { connect(); encrypt(); subscribe(); tick(fakeMillis); }
  void expect(size_t index, uint8_t mods, uint8_t usage) {
    assert(index < input().reports.size());
    assert(input().reports[index] == std::vector<uint8_t>({mods, 0, usage, 0, 0, 0, 0, 0}));
    assert(input().handles[index] == peer.handle);
  }
  size_t count() { return input().reports.size(); }
};

int main() {
  {  // security setup, descriptor, readiness gating, baseline, press/hold/gap
    Fixture f;
    assert(NimBLEDevice::bonding && NimBLEDevice::secureConnections);
    assert(NimBLEDevice::server.autoAdvertise && NimBLEDevice::advertising.started);
    assert(NimBLEHIDDevice::last->manufacturer == "Stick-S3 Macro");
    const std::vector<uint8_t>& map = NimBLEHIDDevice::last->reportMap;
    const std::vector<uint8_t> keyArray = {0x15, 0x00, 0x26, 0xE7, 0x00,
                                           0x05, 0x07, 0x19, 0x00, 0x29, 0xE7, 0x81, 0x00, 0xC0};
    assert(map.size() >= keyArray.size());
    assert(std::equal(keyArray.begin(), keyArray.end(), map.end() - keyArray.size()));
    assert(!f.keyboard.connected() && !f.keyboard.ready() && f.keyboard.idle());
    assert(!f.keyboard.send(0, 0x4f));
    f.connect();
    assert(f.keyboard.connected() && !f.keyboard.ready());
    f.subscribe();
    assert(!f.keyboard.ready());
    assert(!f.keyboard.send(0, 0x4f));
    f.encrypt();
    assert(f.keyboard.ready());
    f.tick(100);
    assert(f.count() == 1);
    f.expect(0, 0, 0);  // baseline
    f.tick(110);
    assert(f.count() == 1);  // baseline only once
    assert(f.keyboard.send(0x09, 0x06));
    assert(!f.keyboard.idle());
    assert(f.count() == 1);  // notify happens in update()
    assert(!f.keyboard.send(0, 0x07));  // busy
    f.tick(110);
    f.expect(1, 0x09, 0x06);  // modifier byte present
    f.tick(134);
    assert(f.count() == 2);
    f.tick(135);
    f.expect(2, 0, 0);  // explicit release after hold
    assert(!f.keyboard.idle());
    f.tick(159);
    assert(!f.keyboard.idle() && !f.keyboard.send(0, 0x07));
    f.tick(160);
    assert(f.keyboard.idle());
    assert(f.keyboard.send(0, 0x07));
    f.tick(160);
    f.expect(3, 0, 0x07);
  }
  {  // invalid strokes and modifier-only chord
    Fixture f;
    f.ready();
    assert(!f.keyboard.send(0, 0));
    assert(!f.keyboard.send(0, 0xE8));
    assert(!f.keyboard.send(0x01, 0xE8));
    assert(f.keyboard.idle());
    assert(f.keyboard.send(0x01, 0xE7));  // max usage allowed
    f.tick(100);
    f.expect(1, 0x01, 0xE7);
    f.tick(125); f.tick(150);
    assert(f.keyboard.idle());
    assert(f.keyboard.send(0x0A, 0));  // modifier-only chord
    f.tick(150);
    f.expect(3, 0x0A, 0);
  }
  {  // configurable keystrokeMs
    Fixture f(40);
    f.ready();
    assert(f.keyboard.send(0, 0x04));
    f.tick(100);
    f.tick(139);
    assert(f.count() == 2);
    f.tick(140);
    f.expect(2, 0, 0);
    f.tick(179);
    assert(!f.keyboard.idle());
    f.tick(180);
    assert(f.keyboard.idle());
  }
  {  // failed press retries; failed release retries and never advances
    Fixture f;
    f.ready();
    f.input().failCount = 1;
    assert(f.keyboard.send(0, 0x04));
    f.tick(100);
    assert(f.count() == 1 && !f.keyboard.idle());
    f.tick(101);
    f.expect(1, 0, 0x04);  // press succeeded on retry
    f.input().failCount = 2;
    f.tick(126);  // 101 + 25
    f.tick(127);
    assert(f.count() == 2);
    assert(!f.keyboard.idle() && !f.keyboard.send(0, 0x05));
    f.tick(128);
    f.expect(2, 0, 0);
    f.tick(152);
    assert(!f.keyboard.idle());
    f.tick(153);
    assert(f.keyboard.idle());
  }
  {  // reconnect between accept and send drops the stroke
    Fixture f;
    f.ready();
    assert(f.keyboard.send(0, 0x04));
    f.disconnect();
    assert(!f.keyboard.ready() && !f.keyboard.send(0, 0x05));
    // Full reconnect between loop iterations: generation still clears work.
    f.connect(); f.encrypt(); f.subscribe(); f.tick(125);
    assert(f.count() == 2);
    f.expect(1, 0, 0);  // only the new baseline, no stale press
    assert(f.keyboard.idle());
    f.tick(200);
    assert(f.count() == 2);
    // Resubscribe also invalidates an accepted stroke.
    assert(f.keyboard.send(0, 0x04));
    f.subscribe(0);
    f.subscribe(1);
    f.tick(201);
    assert(f.count() == 3);
    f.expect(2, 0, 0);
    assert(f.keyboard.idle());
  }
  {  // reconnect while key held: stale release is not replayed on new link
    Fixture f;
    f.ready();
    assert(f.keyboard.send(0x02, 0x04));
    f.tick(100);
    assert(f.count() == 2);
    f.disconnect();
    f.connect(); f.encrypt(); f.subscribe(); f.tick(130);
    assert(f.count() == 3);
    f.expect(2, 0, 0);  // baseline neutral report
    f.tick(300);
    assert(f.count() == 3 && f.keyboard.idle());
  }
  {  // releaseAll mid-press and before press
    Fixture f;
    f.ready();
    assert(f.keyboard.send(0x01, 0x06));
    f.tick(100);
    f.expect(1, 0x01, 0x06);
    fakeMillis = 105;
    f.keyboard.releaseAll();
    f.expect(2, 0, 0);  // released immediately, before hold elapsed
    assert(!f.keyboard.idle());
    f.tick(130);
    assert(f.keyboard.idle());
    // Accepted but unsent stroke is dropped.
    assert(f.keyboard.send(0, 0x04));
    f.keyboard.releaseAll();
    assert(f.keyboard.idle());
    f.tick(200);
    assert(f.count() == 3);
    // Failed immediate release is retried on the next update without waiting.
    assert(f.keyboard.send(0, 0x04));
    f.tick(200);
    f.expect(3, 0, 0x04);
    f.input().failCount = 1;
    f.keyboard.releaseAll();
    assert(f.count() == 4 && !f.keyboard.idle());
    f.tick(201);
    f.expect(4, 0, 0);
    // Nothing to do when idle.
    f.tick(300);
    f.keyboard.releaseAll();
    assert(f.count() == 5);
  }
  {  // clearBonds releases, invalidates, disconnects
    Fixture f;
    f.ready();
    assert(f.keyboard.send(0, 0x04));
    f.tick(100);
    f.keyboard.clearBonds();
    f.expect(2, 0, 0);
    assert(!f.keyboard.ready() && f.keyboard.idle());
    assert(NimBLEDevice::bondsDeleted);
    assert(NimBLEDevice::server.disconnected.back() == f.peer.handle);
    f.tick(125);
    assert(f.count() == 3);
  }
  {  // unencrypted link is dropped; battery clamps
    Fixture f;
    f.connect();
    f.encrypt(false);
    assert(!f.keyboard.ready());
    assert(NimBLEDevice::server.disconnected.back() == f.peer.handle);
    f.keyboard.setBatteryLevel(255);
    assert(NimBLEHIDDevice::last->battery == 100);
    f.keyboard.setBatteryLevel(42);
    assert(NimBLEHIDDevice::last->battery == 42);
  }
  {  // millis wraparound for hold and gap
    Fixture f;
    f.ready();
    fakeMillis = UINT32_MAX - 10;
    assert(f.keyboard.send(0, 0x04));
    f.tick(fakeMillis);
    assert(f.count() == 2);
    f.tick(13);  // 24 ms later
    assert(f.count() == 2);
    f.tick(14);
    f.expect(2, 0, 0);  // release deadline survives wraparound
    f.tick(38);
    assert(!f.keyboard.idle());
    f.tick(39);
    assert(f.keyboard.idle());
  }
  std::cout << "BLE transport tests passed\n";
}
