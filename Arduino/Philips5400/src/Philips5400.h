#pragma once

#include <Arduino.h>
#include <functional>

class Philips5400 {
 public:
  Philips5400(Stream &display, Stream &mainboard);

  void loop();

  void enableBypass(bool en);
  bool bypassEnabled() const;

  using DrinkCallback = std::function<void(uint8_t drink, uint8_t strength,
                                           uint8_t cups, uint16_t volume,
                                           uint16_t milk)>;
  using StateCallback =
      std::function<void(uint8_t id, const uint8_t *data, size_t len)>;

  void onDrinkSelected(DrinkCallback cb) { drink_cb_ = std::move(cb); }
  void onStateChanged(StateCallback cb) { state_cb_ = std::move(cb); }

 private:
  Stream &display_;
  Stream &mainboard_;
  bool bypass_ = false;

  uint8_t main_cnt_ = 0;
  uint8_t disp_cnt_ = 0;
  uint8_t old_disp_cnt_ = 0;

  DrinkCallback drink_cb_;
  StateCallback state_cb_;

  static const uint8_t PREAMBLE[3];
  static constexpr uint8_t POSTAMBLE = 0x55;

  static const uint8_t CoffePattern[14][4];

  uint32_t crc_ = 0xffffffff;
  void startCRC(uint8_t val);
  void addCRC(uint8_t val);
  uint32_t endCRC();
  uint32_t calcCRC(const uint8_t *data, uint8_t size);
  void writeWithCRC(Stream &out, const uint8_t *data, uint8_t size);

  void sendPacket9x(uint8_t *data, uint8_t size);
  void sendPacketFF(uint8_t *data, uint8_t size);

  void decodeAndPublish(const uint8_t *recipe);
};

