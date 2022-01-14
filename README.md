![image](https://user-images.githubusercontent.com/24932503/149502430-167afb52-9581-4bd2-91ea-95554dee556e.png)

![image](https://user-images.githubusercontent.com/24932503/149523128-88a3ba10-fae4-42b5-9a51-8e91c416be5e.png)

![image](https://user-images.githubusercontent.com/24932503/149524095-bb87f742-4247-4c63-9d16-11ac8429fe6a.png)  ![image](https://user-images.githubusercontent.com/24932503/149525171-e6ea7685-e2ad-4621-85b7-eb3820df8b34.png)




# SoftRF firmware build instructions

* [NodeMCU](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source#nodemcu)<br>
* [ESP32](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source#esp32)<br>
* [Raspberry Pi](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source#raspberry-pi)<br>
* [CC13XX](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source#cc13xx)<br>
* [STM32](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source#stm32)<br>
* [ASR650x](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source#asr650x)<br>
* [nRF52840](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source#nrf52840)<br>
* [SAMD21](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source#samd21)<br>

<br>

## NodeMCU

1. Follow [these official instructions](https://github.com/esp8266/Arduino#installing-with-boards-manager)
  to install Arduino IDE and latest Arduino ESP8266 Core
2. Become familiar with IDE and **NodeMCU** by building and uploading of a basic **Blink** sketch:<br>

    _File_ -> _Examples_ -> _ESP8266_ -> _Blink_ <br>

    then<br>

    _Sketch_ -> _Upload_

3. When you are done with the lesson, close your **Arduino** application
4. open ``<My Documents>`` (Windows) or ``<Home>`` (Linux) directory
5. create **Arduino** sub-directory
6. transfer full content of **SoftRF** and **libraries** GitHub folders into the sub-directory:

    [SoftRF](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source/SoftRF) &nbsp;&nbsp;**-->** ``<My Documents>``/Arduino/SoftRF <br>
    [libraries](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source/libraries) **-->** ``<My Documents>``/Arduino/libraries <br>

7. start **Arduino** application again
8. open **SoftRF** sketch from _File_ -> _Open_ menu
9. Select _Tools_ -> _Board_ -> _NodeMCU_ _1.0_
10. in _Tools_ -> _lwIP Variant_  -> _v2 Higher Bandwidth_
11. _Sketch_ -> _Upload_

<br>

## ESP32

1. Follow [these official instructions](https://github.com/espressif/arduino-esp32/blob/master/docs/arduino-ide/boards_manager.md)
  to install Arduino IDE and [latest **stable** Arduino ESP32 Core](https://github.com/espressif/arduino-esp32/releases/tag/1.0.5) (1.0.5)
2. Become familiar with IDE and **DoIt ESP32 DevKit** by building and uploading of a basic **Blink** sketch:<br>
```
int ledPin = 2; // use pin 14 for TTGO T-Beam rev.05 or higher   

void setup()
{
    pinMode(ledPin, OUTPUT);
    Serial.begin(115200);
}

void loop()
{
    Serial.println("Hello, world!");
    digitalWrite(ledPin, HIGH);
    delay(500);
    digitalWrite(ledPin, LOW);
    delay(500);
}
```

3. When you are done with the lesson, close your **Arduino** application
4. open ``<My Documents>`` (Windows) or ``<Home>`` (Linux) directory
5. create **Arduino** sub-directory
6. transfer full content of **SoftRF** and **libraries** GitHub folders into the sub-directory:

    [SoftRF](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source/SoftRF) &nbsp;&nbsp;**-->** ``<My Documents>``/Arduino/SoftRF <br>
    [libraries](https://github.com/lyusupov/SoftRF/tree/master/software/firmware/source/libraries) **-->** ``<My Documents>``/Arduino/libraries <br>

7. start **Arduino** application again
8. open **SoftRF** sketch from _File_ -> _Open_ menu
9. Select _Tools_ -> _Board_ ->  _ESP32_ _Dev_ _Module_
10. Select _Tools_ -> _Flash_ _Mode_ ->  _DIO_
11. Select _Tools_ -> _Flash_ _Size_ ->  _4MB_
12. Select _Tools_ -> _Partition_ _Scheme_ ->  _Minimal_ _SPIFFS_
13. Select _Tools_ -> _Flash_ _Frequency_ ->  _80MHz_
14. Select _Tools_ -> _CPU_ _Frequency_ ->  _80MHz_
15. Select _Tools_ -> _PSRAM_ ->  _Enabled_
16. _Sketch_ -> _Upload_

<br>

