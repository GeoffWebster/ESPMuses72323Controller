/*
  The MIT License (MIT)

  Copyright (c) 2016 Christoffer Hjalmarsson

  Permission is hereby granted, free of charge, to any person obtaining a copy of
  this software and associated documentation files (the "Software"), to deal in
  the Software without restriction, including without limitation the rights to
  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
  the Software, and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "Muses72323.h"
#include <SPI.h>

typedef Muses72323 Self;

// pass through some types into global scope
using data_t = Self::data_t;
using volume_t = Self::volume_t;
using pin_t = Self::pin_t;

// control select addresses, chip address (low 4) ignored
static const data_t s_control_attenuation_l = 0b0000000000010000;
static const data_t s_control_attenuation_r = 0b0000000000010100;
static const data_t s_control_gain          = 0b0000000000001000;
static const data_t s_control_states        = 0b0000001000001100;

// control state bits
static const data_t s_state_soft_step         = 4;
static const data_t s_state_bit_zero_crossing = 8;
static const data_t s_state_external_clock    = 9;
static const data_t s_state_bit_gain          = 15;

pin_t _latch;
// Muses72323 max clock freq=1MHz, set for 800KHz
static const SPISettings s_muses_spi_settings(800000, MSBFIRST, SPI_MODE0);

static inline data_t volume_to_attenuation(volume_t volume)
{
  volume_t tmp ;
  tmp = 4096  + (-volume * 128) ;
  // volume to attenuation data conversion:
  // #=====================================#
  // |    0.0 dB | in: [  0] -> 0b000100000 |
  // | -117.5 dB | in: [447] -> 0b111011111 |
  // #=====================================#
  return static_cast<data_t>(tmp);
}
/*
  static inline data_t volume_to_gain(volume_t gain)
  {
  // volume to gain data conversion:
  // #===================================#
  // |     0 dB | in: [ 0] -> 0b00000000 |
  // | +31.5 dB | in: [63] -> 0b01111111 |
  // #===================================#
  return static_cast<data_t>(min(gain, 63));
  }
*/

Self::Muses72323(address_t chip_address, pin_t latch):
  chip_address(chip_address & 0b0000000000000011),
  states(0) {
  _latch = latch;
}

void Self::begin() {
  pinMode(_latch, OUTPUT);
  digitalWrite(_latch, HIGH);
  SPI.begin();
  //  SPI.setBitOrder(MSBFIRST);
  //  SPI.setDataMode(SPI_MODE2);
  //  initialize SPI:
  //  SPI.setClockDivider(SPI_CLOCK_DIV64);
}

void Self::setVolume(volume_t lch, volume_t rch) {
  transfer(s_control_attenuation_l, volume_to_attenuation(lch));
  transfer(s_control_attenuation_r, volume_to_attenuation(rch));
}

void Self::setGain() {
  transfer(s_control_gain, gain);
}

void Self::mute() {
  transfer(s_control_attenuation_l, 0);
  transfer(s_control_attenuation_r, 0);
}

void Self::setExternalClock(bool enabled) {
  // 0 external, 1 internal
  bitWrite(states, s_state_external_clock, !enabled);
  transfer(s_control_states, states);
}

void Self::setZeroCrossingOn(bool enabled) {
  // 0 is enabled, 1 is disabled
  bitWrite(gain, s_state_bit_zero_crossing, !enabled);
  transfer(s_control_gain, gain);
}

void Self::setLinkChannels(bool enabled) {
  // 1 is enabled (linked channels), 0 is disabled
  bitWrite(gain, s_state_bit_gain, enabled);
  transfer(s_control_gain, gain);
}

void Self::transfer(address_t address, data_t data) {
  word tmp ;
  tmp = address | chip_address ;
  tmp = tmp | data ;

  
    // for debug
    /*
    for (int i = 15; i>=0;i--) {
     Serial.print(bitRead(chip_address,i));
    if (i==7) Serial.print(" ");
    }
    Serial.print("\t");

    for (int i = 15; i>=0;i--) {
    Serial.print(bitRead(address,i));
    if (i==7) Serial.print(" ");
    }
    Serial.print("\t");

    for (int i = 15; i>=0;i--) {
    Serial.print(bitRead(data,i));
    if (i==7) Serial.print(" ");
    }
    Serial.print("\t");

    for (int i = 15; i>=0;i--)  {
    Serial.print(bitRead(tmp,i));
    if (i==7) Serial.print(" ");
    }
    Serial.print("\n");
  */

  SPI.beginTransaction(s_muses_spi_settings);
  digitalWrite(_latch, LOW);

  SPI.transfer(highByte(tmp));
  SPI.transfer(lowByte(tmp));

  digitalWrite(_latch, HIGH);
  SPI.endTransaction();
}
