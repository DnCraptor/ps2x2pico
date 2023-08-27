/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 No0ne (https://github.com/No0ne)
 *           (c) 2023 Dustin Hoffman
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"
#include "ps2phy.h"
ps2phy ms_phy;

#define MS_TYPE_STANDARD  0x00
#define MS_TYPE_WHEEL_3   0x03
#define MS_TYPE_WHEEL_5   0x04

#define MS_MODE_IDLE      0
#define MS_MODE_STREAMING 1

#define MS_INPUT_CMD      0
#define MS_INPUT_SET_RATE 1

u8 ms_type = MS_TYPE_STANDARD;
u8 ms_mode = MS_MODE_IDLE;
u8 ms_input_mode = MS_INPUT_CMD;
u8 ms_rate = 100;
u32 ms_magic_seq = 0x00;

void ms_send(u8 byte) {
  if(DEBUG) printf("%02x ", byte);
  queue_try_add(&ms_phy.qbytes, &byte);
}

void ms_usb_mount(u8 dev_addr, u8 instance) {
  //ms_send(0xaa);
}

void ms_usb_umount(u8 dev_addr, u8 instance) {
  
}

void ms_usb_receive(u8 const* report) {
  if(DEBUG) printf("%02x %02x %02x %02x\n", report[0], report[1], report[2], report[3]);
  
  if(ms_mode == MS_MODE_STREAMING) {
    u8 s = (report[0] & 7) + 8;
    u8 x = report[1] & 0x7f;
    u8 y = report[2] & 0x7f;
    u8 z = report[3] & 7;
    
    if(report[1] >> 7) {
      s += 0x10;
      x += 0x80;
    }
    
    if(report[2] >> 7) {
      y = 0x80 - y;
    } else if(y) {
      s += 0x20;
      y = 0x100 - y;
    }
    
    ms_send(s);
    ms_send(x);
    ms_send(y);
    
    if (ms_type == MS_TYPE_WHEEL_3 || ms_type == MS_TYPE_WHEEL_5) {
      if(report[3] >> 7) {
        z = 0x8 - z;
      } else if(z) {
        z = 0x10 - z;
      }
    
      if (ms_type == MS_TYPE_WHEEL_5) {
        if (report[0] & 0x8) {
          z += 0x10;
        }
    
        if (report[0] & 0x10) {
          z += 0x20;
        }
      }
    
      ms_send(z);
    }
  }
}

void ms_receive(u8 byte, u8 prev_byte) {
  if(DEBUG) printf("!%02x ", byte);
  
  if(ms_input_mode == MS_INPUT_SET_RATE) {
    ms_rate = byte;  // TODO... need to actually honor the sample rate!
    ms_input_mode = MS_INPUT_CMD;
    ms_send(0xfa);

    ms_magic_seq = (ms_magic_seq << 8) | byte;
    if(ms_type == MS_TYPE_STANDARD && ms_magic_seq == 0xc86450) {
      ms_type = MS_TYPE_WHEEL_3;
    } else if (ms_type == MS_TYPE_WHEEL_3 && ms_magic_seq == 0xc8c850) {
      ms_type = MS_TYPE_WHEEL_5;
    }
    if(DEBUG) printf("  MS magic seq: %06x type: %d\n", ms_magic_seq, ms_type);
    return;
  }
  
  if(byte != 0xf3) {
    ms_magic_seq = 0x00;
  }

  switch(byte) {
    case 0xff: // Reset
      ms_type = MS_TYPE_STANDARD;
      ms_mode = MS_MODE_IDLE;
      ms_rate = 100;
      
      ms_send(0xfa);
      ms_send(0xaa);
      ms_send(ms_type);
    return;

    case 0xf6: // Set Defaults
      ms_type = MS_TYPE_STANDARD;
      ms_rate = 100;
    case 0xf5: // Disable Data Reporting
    case 0xea: // Set Stream Mode
      ms_mode = MS_MODE_IDLE;
      ms_send(0xfa);
    return;

    case 0xf4: // Enable Data Reporting
      ms_mode = MS_MODE_STREAMING;
      ms_send(0xfa);
    return;

    case 0xf3: // Set Sample Rate
      ms_input_mode = MS_INPUT_SET_RATE;
      ms_send(0xfa);
    return;

    case 0xf2: // Get Device ID
      ms_send(0xfa);
      ms_send(ms_type);
    return;

    case 0xe9: // Status Request
      ms_send(0xfa);
      ms_send(0x00); // Bit6: Mode, Bit 5: Enable, Bit 4: Scaling, Bits[2,1,0] = Buttons[L,M,R]
      ms_send(0x02); // Resolution
      ms_send(ms_rate); // Sample Rate
    return;
  
  // TODO: Implement (more of) these?
  //    case 0xf0: // Set Remote Mode
  //    case 0xee: // Set Wrap Mode
  //    case 0xec: // Reset Wrap Mode
  //    case 0xeb: // Read Data
  //    case 0xe8: // Set Resolution
  //    case 0xe7: // Set Scaling 2:1
  //    case 0xe6: // Set Scaling 1:1
  }
  
  ms_send(0xfa);
}

void ms_task() {
  ps2phy_task(&ms_phy);
}

void ms_init() {
  ps2phy_init(&ms_phy, pio0, MSDAT, &ms_receive);
}
