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

#ifndef INCLUDED_MUSES_72323
#define INCLUDED_MUSES_72323

#include <Arduino.h>

class Muses72323 {
  public:
    // contextual data types
    typedef uint8_t pin_t;
    typedef uint16_t data_t;
    typedef int volume_t;
    typedef uint16_t address_t;

    // specify a connection over an address using three output pins
    Muses72323(address_t chip_address, pin_t latch);

    // set the pins in their correct states
    void begin();

    // set the volume using the following formula:
    // (-0.25 * volume) db
    // audio level goes from -111.75 to 0.0 dB
    // input goes from -447 to 0
    void setVolume(volume_t left, volume_t right);

    // gain is disabled, this function sets the settings in the gain address
    void setGain();

    void mute();
    // must be set to false if no external clock is connected
    void setExternalClock(bool enabled);

    // enable or disable zero crossing
    void setZeroCrossingOn(bool enabled);

    // if set to true left and right will not be treated independently
    // to set attenuation with linked channels just set the left channel
    void setLinkChannels(bool enabled);

  private:
    void transfer(address_t address, data_t data);

    // for multiple chips on the same bus line
    address_t chip_address;
    data_t states ;
    data_t gain ;
};

#endif // INCLUDED_MUSES_72323
