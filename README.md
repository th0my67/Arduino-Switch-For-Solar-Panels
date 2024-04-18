# Solar Panels / Inverter Arduino Switch

The objective of this project is to activate a switch when the solar panel inverter produces power exceeding a certain threshold. Specifically, I aim to activate my boiler when the power output exceeds 5000W. This setup is tailored to function with an [Arduino Nano 33IoT](https://docs.arduino.cc/hardware/nano-33-iot/), the [MKR ETH Shield](https://docs.arduino.cc/hardware/mkr-eth-shield/), and a SolarEdge inverter compatible with SolarEdge Monitoring API v2. It emits a 3.3V signal from pin 6 when the inverter surpasses 5000W, which can be utilized to trigger a switch.
Several Arduino libraries were utilized, primarily the [WifiNINA](https://www.arduino.cc/reference/en/libraries/wifinina/) library. Additionally, the project relies on the [SPI](https://www.arduino.cc/reference/en/language/functions/communication/spi/) library, the [Arduino Low Power](https://www.arduino.cc/reference/en/libraries/arduino-low-power/) library and the Arduino.h file provided within the IDE.

## Customization

You have the flexibility to adjust all HTTP-related parameters used for the request. The buffer sizes and their contents are also flexible. It is initially set to 5 and 11 characters but can be adjusted to accommodate your specific needs. Additionally, the switch pin, power limit, delay between requests and sleep duration can be easily modified.

## Debugging LED

For enhanced debugging convenience, the project includes blink patterns and commented serial prints. The following patterns are used:

- ON 4s ; OFF 4s                    : No connection with the Host
- ON 1s; OFF 4s                     : Waiting for the Next Request
- ON 0.1s; OFF 0.1s                 : Waiting for the Response

At night, following the "TimeToSleep" period, the onboard LED will remain off until the Arduino wakes up.

## Warning

**This project represents my initial venture into Arduino programming. As such, it may contain errors and should not be relied upon blindly.**

## License

This project is provided under the terms of the MIT License.
