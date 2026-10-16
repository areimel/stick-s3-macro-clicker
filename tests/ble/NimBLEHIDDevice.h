#pragma once
#include "NimBLEDevice.h"
constexpr int HID_KEYBOARD = 0x03c1;
struct NimBLEHIDDevice {
  inline static NimBLEHIDDevice* last = nullptr;
  NimBLECharacteristic input;
  NimBLECharacteristic output;
  NimBLEService service;
  uint8_t battery = 100;
  explicit NimBLEHIDDevice(NimBLEServer*) { last = this; }
  NimBLECharacteristic* getInputReport(uint8_t) { return &input; }
  NimBLECharacteristic* getOutputReport(uint8_t) { return &output; }
  void setManufacturer(const char* m) { manufacturer = m; }
  void setPnp(uint8_t, uint16_t, uint16_t, uint16_t) {}
  void setHidInfo(uint8_t, uint8_t) {}
  std::vector<uint8_t> reportMap;
  std::string manufacturer;
  void setReportMap(uint8_t* p, size_t n) { reportMap.assign(p, p + n); }
  void setBatteryLevel(uint8_t level, bool = false) { battery = level; }
  NimBLEService* getHidService() { return &service; }
};
