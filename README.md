# BlueKeeb
This software is meant to be used with the [Adafruit nRF52 feather](https://www.adafruit.com/product/3406)
and is meant to connect to any keyboards over I2C, converting that keyboard
into a bluetooth keyboard.

# I2C protocol for keyboard developers
The keyboard should work as master, and connect to the BlueKeeb.

## Key types
BlueKeeb starts assuming any bytes sent to it will be normal key_down events. 
If that isn't the case, and instead a media key, a modifer or a key_down event
is being sent, the keyboard should send a type change byte first.

 Bit  | Function 
:----:|----------
7:4| Has to be set to 1 to indicate a Type change byte
3  | If set indicates that any further keys send, are to be released
2  | Reserved for future use
1:0| The type of keycode sent after the type change

Types are currently:

Value | Type
:----:|------
0 | Normal key
1 | Modifier
2 | Media
3 | unused

Where media technically is just "consumer page" of the HID standard

## Releasing keys
Releasing keys are done by setting the release bit in the type change byte
and then sending your keycode


## Sending normal keys
If the correct type is set, send the HID bytes you wish to press / release.
Keep in mind that BlueKeeb is 6KRO, keep track yourself, or check the status byte
to see if ROLL_OVER has been hit.

## Modifiers
If the correct type is set, send the value corrosponding to the modifer

 Value | Modifier
:---:|----------
0 | left control
1 | left shift
2 | left alt
3 | left GUI (Win/Apple/Meta key)
4 | right control
5 | right shift
6 | right alt
7 | right GUI

## Media
If the correct type is set, send the value corrosponding to the media key

Only common functions are listed here, for a full consumer page, consult [the HID standard](http://www.usb.org/developers/hidpage/Hut1_12v2.pdf)

Value | Function
:----:|---------
0x30 | Power
0x31 | Reset
0x32 | Sleep
0xB0 | Play
0xB1 | Pause
0xB3 | Fast Forward
0xB4 | Rewind
0xB5 | Scan Next Track
0xB6 | Scan Previous Track
0xB7 | Stop
0xCD | Play/Pause
0xE2 | Mute
0xE9 | Volume+
0xEA | Volume-

## Status
The keyboard can check the status of the BlueKeeb any time it wants, but it makes
the most sense to check after sending a key.

Simply send an I2C request for 1 byte, and BlueKeeb will send it's status byte

BIT | Meaning | Note
:---:|-------|-----
7 | Unused |
6 | Unused |
5 | Unused |
4 | Unused |
3 | Unused |
2 | Key already released | Bit reset when status byte is read
1 | Key already pressed | Bit reset when status byte is read
0 | ROLL OVER, too many keys pressed, key_presses ignored | Bit is reset when key wasn't ignored