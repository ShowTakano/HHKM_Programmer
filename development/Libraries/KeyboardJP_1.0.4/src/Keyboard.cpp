/*
  Keyboard.cpp

  Modified by Earle F. Philhower, III <earlephilhower@yahoo.com>
  Main Arduino Library Copyright (c) 2015, Arduino LLC
  Original code (pre-library): Copyright (c) 2011, Peter Barrett

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
// 2022.08.30 V1.0.3a Support JP Keyboaard by B.T.O
// 2022.12.28 V1.0.4 Bug Fix by B.T.O
#include "KeyboardJP.h"
#include "KeyboardLayout.h"

#if defined(ARDUINO_ARCH_RP2040)
#define SERIAL_PORT_MONITOR Serial              // V0.1
//#warning "(ARDUINO_ARCH_RP2040))"
#define _USING_HID
#include <RP2040USB.h>

#include "tusb.h"
#include "class/hid/hid_device.h"

// Weak function override to add our descriptor to the TinyUSB list
void __USBInstallKeyboard() { /* noop */ }
#endif
#if defined(_USING_HID)
//================================================================================
//================================================================================
//  Keyboard

#if !defined (ARDUINO_ARCH_RP2040)
static const uint8_t _hidReportDescriptor[] PROGMEM = {

    //  Keyboard
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)  // 47
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x85, 0x02,                    //   REPORT_ID (2)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)

    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)

    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)

    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0xFE,                    //   LOGICAL_MAXIMUM (115)  V1.0.3a change 0x73 to 0xfe
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)

    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0xFE,                    //   USAGE_MAXIMUM (Keyboard Application) V1.0.3a
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0,                          // END_COLLECTION
};
#endif

Keyboard_::Keyboard_(void) 
{
#if   defined(ARDUINO_ARCH_RP2040)
	bzero(&_keyReport, sizeof(_keyReport));
#else
	static HIDSubDescriptor node(_hidReportDescriptor, sizeof(_hidReportDescriptor));
	HID().AppendDescriptor(&node);
#endif
	_asciimap = KeyboardLayout_en_US;
}

void Keyboard_::begin(const uint16_t *layout)		// V1.0.3a
{
	_asciimap = layout;
}

void Keyboard_::end(void)
{
}

void Keyboard_::sendReport(KeyReport* keys)
{
#if   defined(ARDUINO_ARCH_RP2040)
    CoreMutex m(&__usb_mutex);
    tud_task();
    if (tud_hid_ready()) {
        tud_hid_keyboard_report(__USBGetKeyboardReportID(), keys->modifiers, keys->keys);
    }
    tud_task();
#else
	HID().SendReport(2,keys,sizeof(KeyReport));
#endif
}
#ifndef ARDUINO_ARCH_RP2040
uint8_t USBPutChar(uint8_t c);
#endif

// press() adds the specified key (printing, non-printing, or modifier)
// to the persistent key report and sends the report.  Because of the way
// USB HID works, the host acts like the key remains pressed until we
// call release(), releaseAll(), or otherwise clear the report and resend.
size_t Keyboard_::press(uint8_t k)
{
	uint8_t i;
	uint16_t c;					// V1.0.3a

	if (k >= 136) {    			// V1.0.4 it's a non-printing key (not modifier)
		k -= 136;				// V1.0.4
	} else if (k >= 128) {      // V1.0.4 it's a modifier key
		_keyReport.modifiers |= (1 << (k - 128));	// V1.0.3a
		k = 0;
	} else {				// it's a printing key
		c = pgm_read_word(_asciimap + k);	// V1.0.3a
		//SERIAL_PORT_MONITOR.print(c, HEX);
		//SERIAL_PORT_MONITOR.print(" : ");

		k = uint8_t(c) & 0xff;				// V1.0.3a
		if (!c) {
			setWriteError();
			return 0;
		}
		if ((c & ALT_GR) == ALT_GR) {
			_keyReport.modifiers |= 0x40;   // AltGr = right Alt
			//k &= 0x3F;					// V1.0.3a
		} else if ((c & SHIFT) == SHIFT) {
			_keyReport.modifiers |= 0x02;	// the left shift modifier
			//k &= 0x7F;					// V1.0.3a for JP Keyboard
		}
		if (k == ISO_REPLACEMENT) {			// V1.0.3a
			k = ISO_KEY;					// V1.0.3a
		}
	}

	// Add k to the key report only if it's not already present
	// and if there is an empty slot.
	if (_keyReport.keys[0] != k && _keyReport.keys[1] != k &&
		_keyReport.keys[2] != k && _keyReport.keys[3] != k &&
		_keyReport.keys[4] != k && _keyReport.keys[5] != k) {

		for (i=0; i<6; i++) {
			if (_keyReport.keys[i] == 0x00) {
				_keyReport.keys[i] = k;
				//_PORT_MONITOR.print(_keyReport.keys[i], HEX);
				//SERIAL_PORT_MONITOR.print(" ");
				break;
			}
		}
		//_PORT_MONITOR.println();
		if (i == 6) {
			setWriteError();
			return 0;
		}
	}

	sendReport(&_keyReport);
	return 1;
}

// release() takes the specified key out of the persistent key report and
// sends the report.  This tells the OS the key is no longer pressed and that
// it shouldn't be repeated any more.
size_t Keyboard_::release(uint8_t k)
{
	uint8_t i;
	uint16_t c;				// V1.0.3a

	if (k >= 136) {    		// V1.0.4 it's a non-printing key (not modifier)
		k -= 136;			// V1.0.4
	} else if (k >= 128) {  // V1.0.4 it's a modifier key
		_keyReport.modifiers &= ~(1 << (k - 128));
		k = 0;
	} else {				// it's a printing key
		c = pgm_read_word(_asciimap + k);
		k = uint8_t(c) & 0xff;				// V1.0.3a
		if (!c) {
			return 0;
		}
		if ((c & ALT_GR) == ALT_GR) {
			_keyReport.modifiers &= ~(0x40);   // AltGr = right Alt
		} else if ((c & SHIFT) == SHIFT) {
			_keyReport.modifiers &= ~(0x02);	// the left shift modifier
		}
		if (k == ISO_REPLACEMENT) {
			k = ISO_KEY;
		}
	}

	// Test the key report to see if k is present.  Clear it if it exists.
	// Check all positions in case the key is present more than once (which it shouldn't be)
	for (i=0; i<6; i++) {
		if (0 != k && _keyReport.keys[i] == k) {
			_keyReport.keys[i] = 0x00;
		}
	}

	sendReport(&_keyReport);
	return 1;
}

void Keyboard_::releaseAll(void)
{
	_keyReport.keys[0] = 0;
	_keyReport.keys[1] = 0;
	_keyReport.keys[2] = 0;
	_keyReport.keys[3] = 0;
	_keyReport.keys[4] = 0;
	_keyReport.keys[5] = 0;
	_keyReport.modifiers = 0;
	sendReport(&_keyReport);
}

size_t Keyboard_::write(uint8_t c)
{
	uint8_t p = press(c);  // Keydown
	delay(10);
	release(c);            // Keyup
	delay(10);
	return p;              // just return the result of press() since release() almost always returns 1
}

size_t Keyboard_::write(const uint8_t *buffer, size_t size) {
	size_t n = 0;
	while (size--) {
		if (*buffer != '\r') {
			if (write(*buffer)) {
				n++;
			} else {
				break;
			}
		}
		buffer++;
	}
	return n;
}

size_t Keyboard_::pressRaw(uint8_t k) {
  uint8_t  i;
  
  if(!k){
    return 0;
  }

  // Add k to the key report only if it's not already present
  // and if there is an empty slot.
  if (_keyReport.keys[0] != k && _keyReport.keys[1] != k && 
    _keyReport.keys[2] != k && _keyReport.keys[3] != k &&
    _keyReport.keys[4] != k && _keyReport.keys[5] != k) {
    
    for (i=0; i<6; i++) {
      if (_keyReport.keys[i] == 0x00) {
        _keyReport.keys[i] = k;
        break;
      }
    }
    if (i == 6) {
      // setWriteError();
      return 0;
    }  
  }
  sendReport(&_keyReport);
  delay(10);
  return 1;
}

size_t Keyboard_::releaseRaw(uint8_t k) {
  uint8_t i;
  if (!k) {
    return 0;
  }

  // Test the key report to see if k is present.  Clear it if it exists.
  // Check all positions in case the key is present more than once (which it shouldn't be)
  for (i=0; i<6; i++) {
    if (0 != k && _keyReport.keys[i] == k) {
      _keyReport.keys[i] = 0x00;
    }
  }

  sendReport(&_keyReport);
  delay(10);
  return 1;
}

size_t Keyboard_::writeRaw(uint8_t c)
{  
  uint8_t p = pressRaw(c);  // Keydown
  releaseRaw(c);            // Keyup
  return p;                 // just return the result of press() since release() almost always returns 1
}

Keyboard_ Keyboard;

#endif
