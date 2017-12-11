/*********************************************************************
 This is firmware for Adafruits nRF52 Feather, 
 it serves as an I2C -> BLE keyboard bridge

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
*********************************************************************/
#include <bluefruit.h>
#include <Wire.h>

BLEDis bledis;
BLEHidAdafruit blehid;

// I2C settings
#define I2CADDR 4

// Global variables used to store the current keys
uint8_t modifiers = 0x00;
volatile uint8_t key_buffer[6];
uint8_t pressed_keys = 0;

bool buffer_changed = false;

uint16_t consumer_val = 0;
bool consumer_high_byte_received = 0;

enum key_type
{
  KEY = 0,
  MODIFIER,
  MEDIA
} type = KEY;
bool key_release = 0;

// Status byte, stores the current status / error states
volatile uint8_t keeb_status = 0b00000000;
/* Table of bits and their meaning:
  BIT | meaning
   7  | 
   6  | 
   5  | 
   4  | 
   3  | 
   2  | Key already released
   1  | Key already pressed
   0  | ROLL OVER, too many keys pressed, key_presses ignored
*/

void setup() 
{
  Wire.begin(I2CADDR);
  Wire.onReceive(receiveEvent); // register event
  Wire.onRequest(requestedData);

  Bluefruit.begin();
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BlueKeebV1");

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather 52");
  bledis.begin();

  /* Start BLE HID
   * Note: Apple requires BLE device must have min connection interval >= 20m
   * ( The smaller the connection interval the faster we could send data).
   * However for HID and MIDI device, Apple could accept min connection interval 
   * up to 11.25 ms. Therefore BLEHidAdafruit::begin() will try to set the min and max
   * connection interval to 11.25  ms and 15 ms respectively for best performance.
   */
  blehid.begin();

  /* Set connection interval (min, max) to your perferred value.
   * Note: It is already set by BLEHidAdafruit::begin() to 11.25ms - 15ms
   * min = 9*1.25=11.25 ms, max = 12*1.25= 15 ms 
   */
  /* Bluefruit.setConnInterval(9, 12); */

  // Set up and start advertising
  startAdv();
}

void startAdv(void)
{  
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  
  // Include BLE HID service
  Bluefruit.Advertising.addService(blehid);

  // There is enough room for the dev name in the advertising packet
  Bluefruit.Advertising.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

void receiveEvent(int numBytes)
{
  while(Wire.available())
  {
    uint8_t b = Wire.read();

    // Any byte with 0xF* is considered a type changer, to signal if a modifer, normal key or media key is being sent.
    // Additionally bit 3 signals a key_release is to be signaled.
    if((b & 0xF0) == 0xF0)
    {
      type = b & 0x07;
      key_release = (b & bit(3)) >> 3;
      continue;
    }

    // BLE HID is 6KRO, if we are holding 6 keys, set status bit and ignore.
    if(!key_release && pressed_keys >5 && type == KEY)
    {
      bitSet(keeb_status, 0);
    }
    else
    {
      set_key(b, !key_release);
    }
  }

  if(buffer_changed)
  {
    buffer_changed = false;
    blehid.keyboardReport(modifiers, key_buffer);
  }
}

void set_key(uint8_t key, bool press)
{
  // If a modifier is being set, do so
  if(type == MODIFIER)
  {
    bitWrite(modifiers, key, press);
    buffer_changed = true;
  }
  else if(type == MEDIA)
  {
    if(press)
    {
      consumer_val = (consumer_val << 8) | key;
      if(consumer_byte_received) // If We've already received 1 byte of the consumer val
      {
        consumer_high_byte_received = false;
        blehid.consumerKeyPress(consumer_val);
      }
      else // If we haven't received a byte already, remember that we just did
      {
        consumer_high_byte_received = true;
      }
    }
    else
    {
      blehid.consumerKeyPress(0);
    }
  }
  else
  {
    // Check the buffer if the key is set.
    bool is_set = false;
    for(uint8_t i = 0; i<6; i++)
    {
      if(key_buffer[i] == key)
      {
        is_set = true;
        // Do the apropriate checks/actions depending on press
        if(press)
        {
          bitSet(keeb_status, 1); // Set the flag in status
        }
        else
        {
          // Shuffle all higher codes down, and clear byte 5
          for(uint8_t k = i; k<5; k++)
          {
            key_buffer[k] = key_buffer[k+1];
          }
          key_buffer[5] = 0x00;
          pressed_keys--;
          buffer_changed = true;
        }
        break;
      }
    }

    // If the key isn't already set, and we want to set it, set it.
    if(!is_set && press)
    {
      bitClear(keeb_status, 0);
      key_buffer[pressed_keys++] = key;
      buffer_changed = true;
    }
    else if(!is_set && !press)
    {
      bitSet(keeb_status, 2);
    }
  }  
}

void requestedData(void)
{
  Wire.write(keeb_status);
  // Some bits are considered single warnings, and are cleared after being sent
  keeb_status &= 0xF9;
}

void loop() 
{
  waitForEvent();  
}

/**
 * RTOS Idle callback is automatically invoked by FreeRTOS
 * when there are no active threads. E.g when loop() calls delay() and
 * there is no bluetooth or hw event. This is the ideal place to handle
 * background data.
 * 
 * NOTE: FreeRTOS is configured as tickless idle mode. After this callback
 * is executed, if there is time, freeRTOS kernel will go into low power mode.
 * Therefore waitForEvent() should not be called in this callback.
 * http://www.freertos.org/low-power-tickless-rtos.html
 * 
 * WARNING: This function MUST NOT call any blocking FreeRTOS API 
 * such as delay(), xSemaphoreTake() etc ... for more information
 * http://www.freertos.org/a00016.html
 */
void rtos_idle_callback(void)
{
  // Don't call any other FreeRTOS blocking API()
  // Perform background task(s) here
}
