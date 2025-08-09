#include <Philips5400.h>
#include <HardwareSerial.h>

// HardwareSerial instances for the display and mainboard UARTs
HardwareSerial displaySerial(1);
HardwareSerial mainboardSerial(2);

Philips5400 coffee(displaySerial, mainboardSerial);

void setup() {
  Serial.begin(115200);
  // Display <-> ESP UART on GPIO16(RX) and GPIO17(TX)
  displaySerial.begin(115200, SERIAL_8N1, 16, 17);
  // Mainboard <-> ESP UART on GPIO14(RX) and GPIO5(TX)
  mainboardSerial.begin(115200, SERIAL_8N1, 14, 5);

  coffee.onDrinkSelected([](uint8_t drink, uint8_t strength, uint8_t cups,
                            uint16_t volume, uint16_t milk) {
    Serial.print("Drink ");
    Serial.print(drink);
    Serial.print(" strength=");
    Serial.print(strength);
    Serial.print(" cups=");
    Serial.print(cups);
    Serial.print(" volume=");
    Serial.print(volume);
    Serial.print(" milk=");
    Serial.println(milk);
  });

  coffee.onStateChanged([](uint8_t id, const uint8_t *data, size_t len) {
    Serial.printf("State 0x%02X:", id);
    for (size_t i = 0; i < len; ++i) {
      Serial.printf(" %02X", data[i]);
    }
    Serial.println();
  });

  // Enable bypass so messages pass between display and mainboard
  coffee.enableBypass(true);
}

void loop() {
  coffee.loop();
}
