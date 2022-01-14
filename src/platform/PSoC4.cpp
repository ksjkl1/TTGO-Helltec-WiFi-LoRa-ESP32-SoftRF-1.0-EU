/*
 * Platform_PSoC4.cpp
 * Copyright (C) 2020-2022 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(__ASR6501__) || defined(ARDUINO_ARCH_ASR650X)

#include <SPI.h>
#include <Wire.h>

#include "../system/SoC.h"
#include "../driver/RF.h"
#include "../driver/EEPROM.h"
#include "../driver/LED.h"
#include "../driver/OLED.h"
#include "../driver/Baro.h"
#include "../driver/Sound.h"
#include "../driver/Battery.h"
#include "../protocol/data/NMEA.h"
#include "../protocol/data/GDL90.h"
#include "../protocol/data/D1090.h"

#include <innerWdt.h>
#if defined(__ASR6501__)
#include <lorawan_port.h>
#endif /* __ASR6501__ */

// SX1262 pin mapping
lmic_pinmap lmic_pins = {
    .nss = SOC_GPIO_PIN_SS,
    .txe = LMIC_UNUSED_PIN,
    .rxe = LMIC_UNUSED_PIN,
    .rst = SOC_GPIO_PIN_RST,
    .dio = {LMIC_UNUSED_PIN, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},
    .busy = SOC_GPIO_PIN_BUSY,
    .tcxo = LMIC_UNUSED_PIN,
};

#if !defined(EXCLUDE_LED_RING)
// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
CubeCell_NeoPixel strip = CubeCell_NeoPixel(PIX_NUM, SOC_GPIO_PIN_LED,
                              NEO_GRB + NEO_KHZ800);
#endif /* EXCLUDE_LED_RING */

char UDPpacketBuffer[4]; // Dummy definition to satisfy build sequence

static struct rst_info reset_info = {
  .reason = REASON_DEFAULT_RST,
};

static uint32_t bootCount __attribute__ ((section (".noinit")));

typedef enum
{
  PSOC4_LOW_POWER,
  PSOC4_ACTIVE
} PSoC4_states_t;

static PSoC4_states_t PSoC4_state = PSOC4_ACTIVE;

#if defined(BAT_MON_DISABLE)
static bool PSoC4_bat_mon_disable = false;
#endif

static int PSoC4_probe_pin(uint32_t pin, uint32_t mode)
{
  return 0;
}

static void PSoC4_SerialWakeup()
{
  if (hw_info.model == SOFTRF_MODEL_MINI &&
      PSoC4_state   == PSOC4_LOW_POWER) {
    detachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_CONS_RX));
    PSoC4_state = PSOC4_ACTIVE;
    CySoftwareReset();
  }
}

#if SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN
static void PSoC4_User_Key_Wakeup()
{
  if (hw_info.model == SOFTRF_MODEL_MINI &&
      PSoC4_state   == PSOC4_LOW_POWER) {

    delay(10);

    if (digitalRead(SOC_GPIO_PIN_BUTTON) == LOW) {
      detachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_BUTTON));
      PSoC4_state = PSOC4_ACTIVE;

      while (digitalRead(SOC_GPIO_PIN_BUTTON) == LOW);
      delay(100);
      CySoftwareReset();
    }
  }
}
#endif /* SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN */

static void PSoC4_setup()
{
  uint32 reset_reason = CySysGetResetReason(0);

  switch (reset_reason)
  {
    case CY_SYS_RESET_WDT:
      reset_info.reason = REASON_WDT_RST; break;
    case CY_SYS_RESET_PROTFAULT:
      reset_info.reason = REASON_EXCEPTION_RST; break;

    case CY_SYS_RESET_SW:
    default:
      reset_info.reason = REASON_DEFAULT_RST; break;
  }

  pinMode(SOC_GPIO_PIN_OLED_PWR, OUTPUT);
  digitalWrite(SOC_GPIO_PIN_OLED_PWR, LOW);

  pinMode(SOC_GPIO_PIN_OLED_RST, OUTPUT);
  digitalWrite(SOC_GPIO_PIN_OLED_RST, HIGH);

  delay(200);

  /* SSD1306 I2C OLED probing */
  Wire.begin();
  Wire.beginTransmission(SSD1306_OLED_I2C_ADDR);
  if (Wire.endTransmission() == 0) {
    hw_info.model = SOFTRF_MODEL_MINI;
  }

  pinMode(SOC_GPIO_PIN_OLED_RST, ANALOG);
  pinMode(SOC_GPIO_PIN_OLED_PWR, ANALOG);

  if (hw_info.model == SOFTRF_MODEL_MINI) {
    pinMode(SOC_GPIO_PIN_OLED_PWR, OUTPUT);
    digitalWrite(SOC_GPIO_PIN_OLED_PWR, LOW);

    pinMode(SOC_GPIO_PIN_OLED_RST, OUTPUT);
    digitalWrite(SOC_GPIO_PIN_OLED_RST, HIGH);

    pinMode(SOC_GPIO_PIN_GNSS_PWR, OUTPUT);
    digitalWrite(SOC_GPIO_PIN_GNSS_PWR, LOW);
  }
}

static void PSoC4_post_init()
{
  if (hw_info.model == SOFTRF_MODEL_MINI) {
    Serial.println();
    Serial.println(F("CubeCell-GPS Power-on Self Test"));
    Serial.println();
    Serial.flush();

    Serial.println(F("Built-in components:"));

    Serial.print(F("RADIO   : "));
    Serial.println(hw_info.rf      == RF_IC_SX1262        ? F("PASS") : F("FAIL"));
    Serial.print(F("GNSS    : "));
    Serial.println(hw_info.gnss    != GNSS_MODULE_NONE    ? F("PASS") : F("FAIL"));
    Serial.print(F("DISPLAY : "));
    Serial.println(hw_info.display == DISPLAY_OLED_HELTEC ? F("PASS") : F("FAIL"));

    Serial.println();
    Serial.println(F("External components:"));
    Serial.print(F("BMx280  : "));
    Serial.println(hw_info.baro    == BARO_MODULE_BMP280 ? F("PASS") : F("N/A"));

    Serial.println();
    Serial.println(F("Power-on Self Test is completed."));
    Serial.println();
    Serial.flush();
  }
#if defined(USE_OLED)
  OLED_info1();
#endif /* USE_OLED */
}

static void PSoC4_loop()
{
  if (PSoC4_state == PSOC4_LOW_POWER) {
    lowPowerHandler();
  }
}

static void PSoC4_fini(int reason)
{
  if (hw_info.model == SOFTRF_MODEL_MINI) {

    digitalWrite(SOC_GPIO_PIN_GNSS_PWR, HIGH);
    pinMode(SOC_GPIO_PIN_GNSS_PWR,  ANALOG);

    swSer.end();

    delay(2000);

    digitalWrite(SOC_GPIO_PIN_OLED_RST, LOW);
    delay(200);
    pinMode(SOC_GPIO_PIN_OLED_RST,  ANALOG);

    digitalWrite(SOC_GPIO_PIN_OLED_PWR, HIGH);
    pinMode(SOC_GPIO_PIN_OLED_PWR,  ANALOG);

    pinMode(SOC_GPIO_PIN_SS,        ANALOG);
    pinMode(SOC_GPIO_PIN_RST,       ANALOG);
    pinMode(SOC_GPIO_PIN_BUSY,      ANALOG);

    pinMode(SOC_GPIO_PIN_BATTERY,   ANALOG);

//    settings->nmea_l = false;
//    settings->nmea_s = false;

    Serial.end();

    switch (reason)
    {
#if SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN
    case SOFTRF_SHUTDOWN_BUTTON:
    case SOFTRF_SHUTDOWN_LOWBAT:
      pinMode(SOC_GPIO_PIN_BUTTON,  INPUT);
      attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_BUTTON),
                      PSoC4_User_Key_Wakeup, FALLING);
      break;
#endif /* SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN */
    case SOFTRF_SHUTDOWN_NMEA:
      pinMode(SOC_GPIO_PIN_CONS_RX, INPUT);
      attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_CONS_RX),
                      PSoC4_SerialWakeup, FALLING);
      break;
    default:
      break;
    }
  }

  PSoC4_state = PSOC4_LOW_POWER;
//  CyHalt(0);
}

static void PSoC4_reset()
{
  CySoftwareReset();
}

static uint32_t PSoC4_getChipId()
{
#if !defined(SOFTRF_ADDRESS)
  uint32_t id = getID();

  return DevID_Mapper(id);
#else
  return (SOFTRF_ADDRESS & 0xFFFFFFFFU );
#endif
}

static void* PSoC4_getResetInfoPtr()
{
  return (void *) &reset_info;
}

static String PSoC4_getResetInfo()
{
  switch (reset_info.reason)
  {
    default                     : return F("No reset information available");
  }
}

static String PSoC4_getResetReason()
{
  switch (reset_info.reason)
  {
    case REASON_DEFAULT_RST       : return F("DEFAULT");
    case REASON_WDT_RST           : return F("WDT");
    case REASON_EXCEPTION_RST     : return F("EXCEPTION");
    case REASON_SOFT_WDT_RST      : return F("SOFT_WDT");
    case REASON_SOFT_RESTART      : return F("SOFT_RESTART");
    case REASON_DEEP_SLEEP_AWAKE  : return F("DEEP_SLEEP_AWAKE");
    case REASON_EXT_SYS_RST       : return F("EXT_SYS");
    default                       : return F("NO_MEAN");
  }
}

extern "C" void * _sbrk_r (int);
extern "C" void * _sbrk   (int);
extern int _end;

static uint32_t PSoC4_getFreeHeap()
{
//  uint8 *heapend = (uint8 *) _sbrk_r(0);
//  return CYDEV_HEAP_SIZE - (heapend - (uint8 *) &_end);

  char top;
  return &top - reinterpret_cast<char*>(_sbrk(0));
}

static long PSoC4_random(long howsmall, long howBig)
{
  if(howsmall >= howBig) {
      return howsmall;
  }
  long diff = howBig - howsmall;

  return random(diff) + howsmall;
}

static void PSoC4_Sound_test(int var)
{

}

static void PSoC4_Sound_tone(int hz, uint8_t volume)
{
  /* TBD */
}

static void PSoC4_WiFi_set_param(int ndx, int value)
{
  /* NONE */
}

static void PSoC4_WiFi_transmit_UDP(int port, byte *buf, size_t size)
{
  /* NONE */
}

static bool PSoC4_EEPROM_begin(size_t size)
{
  EEPROM.begin(size);

  return true;
}

static void PSoC4_EEPROM_extension(int cmd)
{
  if (cmd == EEPROM_EXT_LOAD) {
    if (settings->mode != SOFTRF_MODE_NORMAL
#if !defined(EXCLUDE_TEST_MODE)
        &&
        settings->mode != SOFTRF_MODE_TXRX_TEST
#endif /* EXCLUDE_TEST_MODE */
        ) {
      settings->mode = SOFTRF_MODE_NORMAL;
    }

    if (settings->nmea_out != NMEA_OFF) {
      settings->nmea_out = NMEA_UART;
    }
    if (settings->gdl90 != GDL90_OFF) {
      settings->gdl90 = GDL90_UART;
    }
    if (settings->d1090 != D1090_OFF) {
      settings->d1090 = D1090_UART;
    }
  }
}

static void PSoC4_SPI_begin()
{
  SPI.begin();
}

static void PSoC4_swSer_begin(unsigned long baud)
{
  swSer.begin(baud);
}

static void PSoC4_swSer_enableRx(boolean arg)
{
  /* NONE */
}

static byte PSoC4_Display_setup()
{
  byte rval = DISPLAY_NONE;

#if defined(USE_OLED)
  rval = OLED_setup();
#endif /* USE_OLED */

  return rval;
}

static void PSoC4_Display_loop()
{
#if defined(USE_OLED)
  OLED_loop();
#endif /* USE_OLED */
}

static void PSoC4_Display_fini(int reason)
{
#if defined(USE_OLED)
  OLED_fini(reason);
#endif /* USE_OLED */
}

static void PSoC4_Battery_setup()
{
#if defined(BAT_MON_DISABLE)
  if (hw_info.model == SOFTRF_MODEL_MINI) {
    pinMode(SOC_GPIO_PIN_BMON_DIS, INPUT_PULLDOWN);

    delay(100);

    PSoC4_bat_mon_disable = (digitalRead(SOC_GPIO_PIN_BMON_DIS) == HIGH ?
                             true : false);
    pinMode(SOC_GPIO_PIN_BMON_DIS, ANALOG);
  }
#endif
}

static float PSoC4_Battery_param(uint8_t param)
{
  float rval, voltage;

  switch (param)
  {
  case BATTERY_PARAM_THRESHOLD:
    rval = hw_info.model == SOFTRF_MODEL_MINI ? BATTERY_THRESHOLD_LIPO   :
                                                BATTERY_THRESHOLD_NIMHX2;
    break;

  case BATTERY_PARAM_CUTOFF:
    rval = hw_info.model == SOFTRF_MODEL_MINI ? BATTERY_CUTOFF_LIPO   :
                                                BATTERY_CUTOFF_NIMHX2;
    break;

  case BATTERY_PARAM_CHARGE:
    voltage = Battery_voltage();
    if (voltage < Battery_cutoff())
      return 0;

    if (voltage > 4.2)
      return 100;

    if (voltage < 3.6) {
      voltage -= 3.3;
      return (voltage * 100) / 3;
    }

    voltage -= 3.6;
    rval = 10 + (voltage * 150 );
    break;

  case BATTERY_PARAM_VOLTAGE:
  default:

  #if defined(BAT_MON_DISABLE)
    if (PSoC4_bat_mon_disable) {
      rval = 0;
    } else
  #endif
    {
      uint16_t mV = 0;

      if (hw_info.model == SOFTRF_MODEL_MINI &&
          PSoC4_state   == PSOC4_ACTIVE) {
        /* GPIO7 is shared between USER_KEY and VBAT_ADC_CTL functions */
        int user_key_state = digitalRead(SOC_GPIO_PIN_BUTTON);

        /* if the key is not pressed down - activate VBAT_ADC_CTL */
        if (user_key_state == HIGH) {
          pinMode(VBAT_ADC_CTL,OUTPUT);
          digitalWrite(VBAT_ADC_CTL,LOW);
        }

#if !defined(ARDUINO_ARCH_ASR650X)
        /* Heltec CubeCell Arduino Core 1.2.0 or less */
        mV = analogRead(SOC_GPIO_PIN_BATTERY);
#else
        mV = analogReadmV(SOC_GPIO_PIN_BATTERY);
#endif

        /* restore previous state of VBAT_ADC_CTL pin */
        if (user_key_state == HIGH) {
          /*
           * CubeCell-GPS has external 10K VDD pullup resistor
           * connected to GPIO7 (USER_KEY / VBAT_ADC_CTL) pin
           */
          pinMode(VBAT_ADC_CTL, INPUT);
        }
      }

      rval = mV * SOC_ADC_VOLTAGE_DIV / 1000.0;
    }
    break;
  }

  return rval;
}

void PSoC4_GNSS_PPS_Interrupt_handler() {
  PPS_TimeMarker = millis();
}

static unsigned long PSoC4_get_PPS_TimeMarker() {
  return PPS_TimeMarker;
}

static bool PSoC4_Baro_setup() {
  return true;
}

static void PSoC4_UATSerial_begin(unsigned long baud)
{

}

static void PSoC4_UATModule_restart()
{

}

static void PSoC4_WDT_setup()
{
  /* Enable the WDT.
  * The wdt about every 1.4 seconds generates an interruption,
  * Two unserviced interrupts lead to a system reset(i.e. at the third match).
  * The max feed time shoud be 2.8 seconds.
  * autoFeed = false: do not auto feed wdt.
  * autoFeed = true : it auto feed the wdt in every interrupt.
  */

  /* Enable the WDT, autofeed	*/
  innerWdtEnable(true);
}

static void PSoC4_WDT_fini()
{
  CySysWdtDisable();
}

#if SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN
#include <AceButton.h>
using namespace ace_button;

AceButton button_1(SOC_GPIO_PIN_BUTTON);

// The event handler for the button.
void handleEvent(AceButton* button, uint8_t eventType,
    uint8_t buttonState) {

  switch (eventType) {
    case AceButton::kEventClicked:
    case AceButton::kEventReleased:
#if defined(USE_OLED)
      if (button == &button_1) {
        OLED_Next_Page();
      }
#endif
      break;
    case AceButton::kEventDoubleClicked:
      break;
    case AceButton::kEventLongPressed:
      if (button == &button_1) {
        shutdown(SOFTRF_SHUTDOWN_BUTTON);
//      Serial.println(F("This will never be printed."));
      }
      break;
  }
}

/* Callbacks for push button interrupt */
void onPageButtonEvent() {
  button_1.check();
}
#endif /* SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN */

static void PSoC4_Button_setup()
{
#if SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN
  if (hw_info.model == SOFTRF_MODEL_MINI) {
    int button_pin = SOC_GPIO_PIN_BUTTON;

    // Button(s) uses external pull up resistor.
    pinMode(button_pin, INPUT);

    button_1.init(button_pin);

    // Configure the ButtonConfig with the event handler, and enable all higher
    // level events.
    ButtonConfig* PageButtonConfig = button_1.getButtonConfig();
    PageButtonConfig->setEventHandler(handleEvent);
    PageButtonConfig->setFeature(ButtonConfig::kFeatureClick);
//    PageButtonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
    PageButtonConfig->setFeature(ButtonConfig::kFeatureLongPress);
    PageButtonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterClick);
//    PageButtonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);
//    PageButtonConfig->setFeature(
//                      ButtonConfig::kFeatureSuppressClickBeforeDoubleClick);
//  PageButtonConfig->setDebounceDelay(15);
    PageButtonConfig->setClickDelay(600);
//    PageButtonConfig->setDoubleClickDelay(1500);
    PageButtonConfig->setLongPressDelay(2000);

//  attachInterrupt(digitalPinToInterrupt(button_pin), onPageButtonEvent, CHANGE );
  }
#endif /* SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN */
}

static void PSoC4_Button_loop()
{
#if SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN
  if (hw_info.model == SOFTRF_MODEL_MINI) {
    button_1.check();
  }
#endif /* SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN */
}

static void PSoC4_Button_fini()
{
#if SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN
  if (hw_info.model == SOFTRF_MODEL_MINI) {
//  detachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_BUTTON));
    while (digitalRead(SOC_GPIO_PIN_BUTTON) == LOW);
    pinMode(SOC_GPIO_PIN_BUTTON, ANALOG);
  }
#endif /* SOC_GPIO_PIN_BUTTON != SOC_UNUSED_PIN */
}

#include "RingBuffer.h"

#define UART1_TX_FIFO_SIZE (MAX_TRACKING_OBJECTS * 65 + 75 + 75 + 42 + 20)

RingBuffer<uint8_t, UART1_TX_FIFO_SIZE> UART_TX_FIFO =
                                    RingBuffer<uint8_t, UART1_TX_FIFO_SIZE>();

static void PSoC4_UART_loop()
{
  while (SerialOutput.availableForWrite() > 0) {
//  while (UART_1_SpiUartGetTxBufferSize() < UART_1_TX_BUFFER_SIZE) {
    if (UART_TX_FIFO.empty()) {
      break;
    }
    SerialOutput.write(UART_TX_FIFO.read());
//    UART_1_UartPutChar(UART_TX_FIFO.read());
  }
}

static size_t PSoC4_UART_write(const uint8_t *buffer, size_t size)
{
  size_t written;

  for (written=0; written < size; written++) {
    if (!UART_TX_FIFO.full()) {
      UART_TX_FIFO.write(buffer[written]);
    } else {
      break;
    }
  }
  return written;
}

IODev_ops_t PSoC4_UART_ops = {
  "PSoC4 UART",
  NULL,
  PSoC4_UART_loop,
  NULL,
  NULL,
  NULL,
  PSoC4_UART_write
};

const SoC_ops_t PSoC4_ops = {
  SOC_PSOC4,
  "PSoC4",
  PSoC4_setup,
  PSoC4_post_init,
  PSoC4_loop,
  PSoC4_fini,
  PSoC4_reset,
  PSoC4_getChipId,
  PSoC4_getResetInfoPtr,
  PSoC4_getResetInfo,
  PSoC4_getResetReason,
  PSoC4_getFreeHeap,
  PSoC4_random,
  PSoC4_Sound_test,
  PSoC4_Sound_tone,
  NULL,
  PSoC4_WiFi_set_param,
  PSoC4_WiFi_transmit_UDP,
  NULL,
  NULL,
  NULL,
  PSoC4_EEPROM_begin,
  PSoC4_EEPROM_extension,
  PSoC4_SPI_begin,
  PSoC4_swSer_begin,
  PSoC4_swSer_enableRx,
  NULL, /* PSoC4 has no built-in Bluetooth */
  NULL, /* PSoC4 has no built-in USB */
  &PSoC4_UART_ops,
  PSoC4_Display_setup,
  PSoC4_Display_loop,
  PSoC4_Display_fini,
  PSoC4_Battery_setup,
  PSoC4_Battery_param,
  PSoC4_GNSS_PPS_Interrupt_handler,
  PSoC4_get_PPS_TimeMarker,
  PSoC4_Baro_setup,
  PSoC4_UATSerial_begin,
  PSoC4_UATModule_restart,
  PSoC4_WDT_setup,
  PSoC4_WDT_fini,
  PSoC4_Button_setup,
  PSoC4_Button_loop,
  PSoC4_Button_fini,
  NULL
};

#endif /* __ASR6501__ || ARDUINO_ARCH_ASR650X */
