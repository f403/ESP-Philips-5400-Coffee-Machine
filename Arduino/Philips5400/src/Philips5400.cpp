#include "Philips5400.h"

// Patterns: {B0,B3,B4,max cups}
const uint8_t Philips5400::CoffePattern[14][4] = {
    {0,1,1,2}, // Espresso
    {0,1,2,1}, // Coffee to go
    {0,2,2,2}, // Black Coffee
    {0,2,2,2}, // Lungo
    {0,2,2,2}, // Caffe Crema
    {0,2,2,2}, // Ristretto
    {1,2,3,2}, // Americano
    {2,2,1,2}, // Coffee With Milk
    {2,2,2,2}, // Latte
    {3,2,2,2}, // Milk Coffee to go
    {3,2,2,2}, // Latte Macchiato
    {3,2,3,2}, // Cappuccino
    {4,0,0,1}, // Milk foam
    {5,1,0,1}  // Hot water
};

const uint8_t Philips5400::PREAMBLE[3] = {0xAA, 0xAA, 0xAA};

Philips5400::Philips5400(Stream &display, Stream &mainboard)
    : display_(display), mainboard_(mainboard) {}

void Philips5400::enableBypass(bool en) { bypass_ = en; }
bool Philips5400::bypassEnabled() const { return bypass_; }

// CRC helpers --------------------------------------------------------------
static const uint8_t BitReverse[256] = {
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

void Philips5400::startCRC(uint8_t val) {
  crc_ = 0xffffffff;
  addCRC(val);
}

void Philips5400::addCRC(uint8_t val) {
  ((uint8_t*)(&crc_))[3] ^= BitReverse[val];
  for (uint8_t j = 0; j < 8; j++) {
    if (crc_ & 0x80000000) {
      crc_ = (uint32_t)((crc_ << 1) ^ 0x04C11DB7);
    } else {
      crc_ <<= 1;
    }
  }
}

uint32_t Philips5400::endCRC() {
  uint32_t t32 = 0;
  ((uint8_t*)(&t32))[0] = BitReverse[((uint8_t*)(&crc_))[3]];
  ((uint8_t*)(&t32))[1] = BitReverse[((uint8_t*)(&crc_))[2]];
  ((uint8_t*)(&t32))[2] = BitReverse[((uint8_t*)(&crc_))[1]];
  ((uint8_t*)(&t32))[3] = BitReverse[((uint8_t*)(&crc_))[0]];
  crc_ = t32 ^ 0xffffffff;
  return crc_;
}

uint32_t Philips5400::calcCRC(const uint8_t *data, uint8_t size) {
  startCRC(data[0]);
  for (uint8_t i = 1; i < size; i++) addCRC(data[i]);
  return endCRC();
}

void Philips5400::writeWithCRC(Stream &out, const uint8_t *data, uint8_t size) {
  uint32_t cs = calcCRC(data, size);
  out.write(PREAMBLE, 3);
  out.write(data, size);
  out.write((uint8_t *)&cs, 4);
  out.write(POSTAMBLE);
}

// Packet helpers -----------------------------------------------------------
void Philips5400::sendPacket9x(uint8_t *data, uint8_t size) {
  if (disp_cnt_ < data[1] || data[1] == 0) disp_cnt_ = data[1];
  static uint8_t cnt93 = 0;
  static uint8_t cnt91 = 0;
  static uint8_t cnt90 = 0;
  if (data[0] == 0x93) {
    static uint8_t old = 0xFF;
    if (old != data[1]) {
      old = data[1];
      cnt93 = main_cnt_;
    }
    data[1] = cnt93;
  } else if (data[0] == 0x91) {
    static uint8_t old = 0xFF;
    static uint8_t old_main_cnt = 0xFF;
    if (old != data[1] && old_main_cnt != main_cnt_) {
      old = data[1];
      old_main_cnt = main_cnt_;
      cnt91 = main_cnt_;
      if (cnt91 == cnt93) {
        if (data[3] == 0x03)
          cnt91 = cnt93 + 2;
        else if (data[3] == 0x10)
          cnt91 = cnt93 + 1;
      }
    }
    data[1] = cnt91;
  } else if (data[0] == 0x90) {
    static uint8_t old = 0xFF;
    if (old != data[1]) {
      old = data[1];
      cnt90 = cnt93 + 1;
    }
    data[1] = cnt90;
  }
  writeWithCRC(mainboard_, data, size);
}

void Philips5400::sendPacketFF(uint8_t *data, uint8_t size) {
  main_cnt_ = data[1];
  if (old_disp_cnt_ != disp_cnt_ + 1) {
    old_disp_cnt_++;
  } else if (data[1] == 0) {
    old_disp_cnt_ = 0;
    disp_cnt_ = 0;
  }
  data[1] = old_disp_cnt_;
  data[3] = data[1];
  writeWithCRC(display_, data, size);
}

// Decoding -----------------------------------------------------------
void Philips5400::decodeAndPublish(const uint8_t *rec) {
  if (!drink_cb_) return;
  uint8_t type = 0;
  uint8_t bean = rec[1];
  uint8_t cups = rec[2];
  uint16_t vol = 0;
  uint16_t milk = 0;
  while (type < 14) {
    if (rec[0] == CoffePattern[type][0] && rec[3] == CoffePattern[type][1] &&
        rec[4] == CoffePattern[type][2])
      break;
    type++;
  }
  if (type < 14) {
    if (rec[0] == 1) {
      vol = rec[9] * 0x100 + rec[8] + rec[7] * 0x100 + rec[6];
    } else {
      vol = rec[7] * 0x100 + rec[6];
      if (rec[5] == 2) milk = rec[9] * 0x100 + rec[8];
    }
    drink_cb_(type, bean, cups, vol, milk);
  }
}

// Main loop -----------------------------------------------------------
void Philips5400::loop() {
  // From display to mainboard
  static uint8_t buffer_board[40];
  static uint8_t count_board = 0;
  while (display_.available()) {
    uint8_t temp = display_.read();
    if ((count_board > 2) || (temp == 0xAA)) {
      buffer_board[count_board++] = temp;
      if (count_board >= sizeof(buffer_board)) count_board = 0;
    } else {
      count_board = 0;
    }
    if (temp == 0x55 && count_board > 5 &&
        ((buffer_board[5] + 11) <= count_board)) {
      uint8_t *data = buffer_board + 3;
      uint8_t size = count_board - 8;
      if (bypass_ && ((buffer_board[3] & 0xF0) == 0x90)) {
        if (buffer_board[3] == 0x90) decodeAndPublish(buffer_board + 6);
        sendPacket9x(data, size);
      } else {
        mainboard_.write(buffer_board, count_board);
      }
      count_board = 0;
      break;
    }
  }

  // From mainboard to display
  static uint8_t buffer_displ[40];
  static uint8_t count_displ = 0;
  while (mainboard_.available()) {
    uint8_t temp = mainboard_.read();
    if ((count_displ > 2) || (temp == 0xAA)) {
      buffer_displ[count_displ++] = temp;
      if (count_displ >= sizeof(buffer_displ)) count_displ = 0;
    } else {
      count_displ = 0;
    }
    if (temp == 0x55 && count_displ > 5 &&
        ((buffer_displ[5] + 11) <= count_displ)) {
      uint8_t *data = buffer_displ + 3;
      uint8_t size = count_displ - 8;
      uint8_t id = buffer_displ[3];
      if (bypass_ && id == 0xFF) {
        sendPacketFF(data, size);
      } else {
        display_.write(buffer_displ, count_displ);
      }
      if (state_cb_ && (id == 0xB0 || id == 0xB5)) {
        state_cb_(id, data, size);
      }
      count_displ = 0;
      break;
    }
  }
}

