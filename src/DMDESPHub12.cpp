/*
 * Copyright (C) 2012 Southern Storm Software, Pty Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.

--------------------------------------------------------------------------------------

CATATAN :

 * > Library DMDESP Updated 26 April 2020 - bonny@elektronmart.com
 * > HJS589(DMD porting for ESP8266 by dmk007) 
 * > DMD (https://github.com/rweather/arduino-projects/tree/master/libraries/DMD)

 * Penggunaan : Copy Folder DMDESP ke Arduino Library
  
 * JANGAN LUPA PESAN HEADER DI ATAS.

 * Silahkan untuk dipergunakan untuk kepentingan Ibadah, Edukasi dan Komersil. Jangan lupa
   jika menggunakan library ini jangan dihapus headernya dan jika ada perbaikan atau
   pengembangan lanjutan ANDA WAJIB untuk MEMBAGIKAN KEMBALI hasilnya ke PUBLIK (GNU 
   General Public License).

 * JANGAN PELIT ILMU.....!!!

--------------------------------------------------------------------------------------*/

/*
 * DMDESPHub12 v1.1.0 Flicker-free - FrameSync - modification notice
 *
 * DMDESPHub12 preserves the original DMDESP drawing/framebuffer
 * model and the notices above. Changes in this branch include:
 * - ESP8266 Timer0 deterministic refresh.
 * - Direct SPI1 FIFO scan output.
 * - Frame-boundary-synchronized double-buffer swaps.
 * - Nested-safe DMDESPHub12RefreshGuard for flash/filesystem/OTA operations.
 * - Conservative timing support targeted at 1x1 through 6x1 HUB12 panels.
 *
 * Modified version: 1.1.0, August 2026.
 * DMDESPHub12 https://github.com/romas243/DMDESPHub12 
 * The original authors are not responsible for defects introduced by this
 * modified DMDESPHub12 branch.
 */

#include "DMDESPHub12.h"
#include "Arduino.h"
#include <string.h>
#include <stdlib.h>

#define DMDESPHUB12_PIN_PHASE_LSB       16
#define DMDESPHUB12_PIN_PHASE_MSB       12
#define DMDESPHUB12_PIN_LATCH           0
#define DMDESPHUB12_PIN_OUTPUT_ENABLE   15
#define DMDESPHUB12_PIN_SPI_MOSI        13
#define DMDESPHUB12_PIN_SPI_SCK         14

#define DMDESPHUB12_NUM_COLUMNS      32
#define DMDESPHUB12_NUM_ROWS         16
#define DMDESPHUB12_MAX_REFRESH_US    5000UL

#ifndef DMDESPHUB12_FEED_WDT_FROM_ISR
#define DMDESPHUB12_FEED_WDT_FROM_ISR 1
#endif

static volatile DMDESPHub12 *g_dmdespHub12 = nullptr;

// RAM table: refresh() may run while flash cache is unavailable.
static uint8_t flipBits[256] = {
    0x00,0x80,0x40,0xC0,0x20,0xA0,0x60,0xE0,0x10,0x90,0x50,0xD0,0x30,0xB0,0x70,0xF0,
    0x08,0x88,0x48,0xC8,0x28,0xA8,0x68,0xE8,0x18,0x98,0x58,0xD8,0x38,0xB8,0x78,0xF8,
    0x04,0x84,0x44,0xC4,0x24,0xA4,0x64,0xE4,0x14,0x94,0x54,0xD4,0x34,0xB4,0x74,0xF4,
    0x0C,0x8C,0x4C,0xCC,0x2C,0xAC,0x6C,0xEC,0x1C,0x9C,0x5C,0xDC,0x3C,0xBC,0x7C,0xFC,
    0x02,0x82,0x42,0xC2,0x22,0xA2,0x62,0xE2,0x12,0x92,0x52,0xD2,0x32,0xB2,0x72,0xF2,
    0x0A,0x8A,0x4A,0xCA,0x2A,0xAA,0x6A,0xEA,0x1A,0x9A,0x5A,0xDA,0x3A,0xBA,0x7A,0xFA,
    0x06,0x86,0x46,0xC6,0x26,0xA6,0x66,0xE6,0x16,0x96,0x56,0xD6,0x36,0xB6,0x76,0xF6,
    0x0E,0x8E,0x4E,0xCE,0x2E,0xAE,0x6E,0xEE,0x1E,0x9E,0x5E,0xDE,0x3E,0xBE,0x7E,0xFE,
    0x01,0x81,0x41,0xC1,0x21,0xA1,0x61,0xE1,0x11,0x91,0x51,0xD1,0x31,0xB1,0x71,0xF1,
    0x09,0x89,0x49,0xC9,0x29,0xA9,0x69,0xE9,0x19,0x99,0x59,0xD9,0x39,0xB9,0x79,0xF9,
    0x05,0x85,0x45,0xC5,0x25,0xA5,0x65,0xE5,0x15,0x95,0x55,0xD5,0x35,0xB5,0x75,0xF5,
    0x0D,0x8D,0x4D,0xCD,0x2D,0xAD,0x6D,0xED,0x1D,0x9D,0x5D,0xDD,0x3D,0xBD,0x7D,0xFD,
    0x03,0x83,0x43,0xC3,0x23,0xA3,0x63,0xE3,0x13,0x93,0x53,0xD3,0x33,0xB3,0x73,0xF3,
    0x0B,0x8B,0x4B,0xCB,0x2B,0xAB,0x6B,0xEB,0x1B,0x9B,0x5B,0xDB,0x3B,0xBB,0x7B,0xFB,
    0x07,0x87,0x47,0xC7,0x27,0xA7,0x67,0xE7,0x17,0x97,0x57,0xD7,0x37,0xB7,0x77,0xF7,
    0x0F,0x8F,0x4F,0xCF,0x2F,0xAF,0x6F,0xEF,0x1F,0x9F,0x5F,0xDF,0x3F,0xBF,0x7F,0xFF
};

// Commit 1..16 FIFO words (4 bytes/word) as one HSPI transaction.
static inline void ICACHE_RAM_ATTR spiCommitWordsISR(uint16_t words)
{
    const uint32_t bits = ((uint32_t)words * 32U) - 1U;
    const uint32_t mask = ~((uint32_t)SPIMMOSI << SPILMOSI);
    SPI1U1 = (SPI1U1 & mask) | (bits << SPILMOSI);

    __sync_synchronize();
    SPI1CMD |= SPICMDUSR;
    while (SPI1CMD & SPICMDUSR) {
        // Wait for the hardware FIFO transaction to complete.
    }
}

void ICACHE_RAM_ATTR dmdespHub12HardRefreshISR()
{
    DMDESPHub12 *d = (DMDESPHub12 *)g_dmdespHub12;
    if (!d) return;

#if DMDESPHUB12_FEED_WDT_FROM_ISR
    system_soft_wdt_feed();
#endif

    const uint32_t start = ESP.getCycleCount();
    timer0_write(start + d->_refreshCycles);

    d->refresh();

#if DMDESPHUB12_FEED_WDT_FROM_ISR
    system_soft_wdt_feed();
#endif

    const uint32_t elapsed = ESP.getCycleCount() - start;
    ++d->_isrCount;
    if (elapsed > d->_refreshCycles) ++d->_isrOverruns;
    if (elapsed > d->_maxIsrCycles) d->_maxIsrCycles = elapsed;
}

static uint32_t recommendedRefreshForPanels(uint8_t wide, uint8_t high)
{
    if (high != 1) return 1500UL;
    if (wide <= 4) return 1000UL;
    if (wide == 5) return 1100UL;
    return 1200UL; // conservative starting point for 6x1 and above
}

DMDESPHub12::DMDESPHub12(int widthPanels, int heightPanels)
  : DMDESPHub12Bitmap(((widthPanels < 1) ? 1 : widthPanels) * DMDESPHUB12_NUM_COLUMNS,
           ((heightPanels < 1) ? 1 : heightPanels) * DMDESPHUB12_NUM_ROWS)
  , cr(150)
  , _doubleBuffer(false)
  , _running(false)
  , phase(0)
  , _panelsWide((uint8_t)((widthPanels < 1) ? 1 : widthPanels))
  , _panelsHigh((uint8_t)((heightPanels < 1) ? 1 : heightPanels))
  , fb0(0)
  , fb1(0)
  , displayfb(0)
  , _swapRequested(false)
  , _swapCompleted(0)
  , _swapTimeouts(0)
  , _refreshUs(recommendedRefreshForPanels(_panelsWide, _panelsHigh))
  , _refreshCycles(microsecondsToClockCycles(_refreshUs))
  , _isrCount(0)
  , _isrOverruns(0)
  , _maxIsrCycles(0)
{
    fb0 = displayfb = fb;

    pinMode(DMDESPHUB12_PIN_SPI_SCK, OUTPUT);
    pinMode(DMDESPHUB12_PIN_SPI_MOSI, OUTPUT);
    digitalWrite(DMDESPHUB12_PIN_SPI_SCK, LOW);
    digitalWrite(DMDESPHUB12_PIN_SPI_MOSI, LOW);

    pinMode(DMDESPHUB12_PIN_PHASE_LSB, OUTPUT);
    pinMode(DMDESPHUB12_PIN_PHASE_MSB, OUTPUT);
    pinMode(DMDESPHUB12_PIN_LATCH, OUTPUT);
    pinMode(DMDESPHUB12_PIN_OUTPUT_ENABLE, OUTPUT);

    digitalWrite(DMDESPHUB12_PIN_PHASE_LSB, LOW);
    digitalWrite(DMDESPHUB12_PIN_PHASE_MSB, LOW);
    digitalWrite(DMDESPHUB12_PIN_LATCH, LOW);
    digitalWrite(DMDESPHUB12_PIN_OUTPUT_ENABLE, HIGH); // blank until start()
    digitalWrite(DMDESPHUB12_PIN_SPI_MOSI, HIGH);
}

DMDESPHub12::~DMDESPHub12()
{
    stopRefresh();

    if (fb0) free(fb0);
    if (fb1) free(fb1);
    fb = 0;
    fb0 = 0;
    fb1 = 0;
    displayfb = 0;
}

void DMDESPHub12::setDoubleBuffer(bool doubleBuffer)
{
    if (doubleBuffer == _doubleBuffer) return;

    if (doubleBuffer) {
        // The second buffer is only useful when the primary DMDESPHub12Bitmap buffer
        // was allocated successfully.
        if (!fb0) return;

        const unsigned int size = _stride * _height;
        uint8_t *newBuffer = (uint8_t *)malloc(size);
        if (!newBuffer) return;

        memset(newBuffer, 0xFF, size);

        noInterrupts();
        fb1 = newBuffer;
        fb = fb1;
        displayfb = fb0;
        _swapRequested = false;
        _doubleBuffer = true;
        interrupts();
    } else {
        noInterrupts();
        _swapRequested = false;
        fb = fb0;
        displayfb = fb0;
        _doubleBuffer = false;
        interrupts();

        if (fb1) {
            free(fb1);
            fb1 = 0;
        }
    }
}

void DMDESPHub12::immediateSwapLocked()
{
    uint8_t *oldDisplay = displayfb;
    displayfb = fb;
    fb = oldDisplay;
    _swapRequested = false;
    ++_swapCompleted;
}

void ICACHE_RAM_ATTR DMDESPHub12::commitPendingSwapFromISR()
{
    if (_doubleBuffer && _swapRequested) {
        uint8_t *oldDisplay = displayfb;
        displayfb = fb;
        fb = oldDisplay;
        _swapRequested = false;
        ++_swapCompleted;
    }
}

uint32_t DMDESPHub12::automaticSwapTimeoutUs() const
{
    // A frame is four phases. Allow at least two frame periods plus margin.
    uint32_t timeoutUs = _refreshUs * 10UL;
    if (timeoutUs < 10000UL) timeoutUs = 10000UL;
    if (timeoutUs > 60000UL) timeoutUs = 60000UL;
    return timeoutUs;
}

bool DMDESPHub12::swapBuffersSync(uint32_t timeoutUs)
{
    if (!_doubleBuffer || !fb0 || !fb1) return false;

    // Before start(), during RefreshGuard, or when another object owns Timer0,
    // no ISR can commit this object's request. Commit immediately and safely.
    if (!_running || g_dmdespHub12 != this) {
        noInterrupts();
        immediateSwapLocked();
        interrupts();
        return true;
    }

    uint32_t target;
    noInterrupts();
    target = _swapCompleted + 1U;
    _swapRequested = true;
    interrupts();

    if (timeoutUs == 0) timeoutUs = automaticSwapTimeoutUs();
    const uint32_t started = micros();

    while (_swapCompleted != target) {
        delay(0); // service ESP8266 WiFi/background tasks while waiting

        if ((uint32_t)(micros() - started) >= timeoutUs) {
            // The main context cannot run concurrently with the ISR. Once
            // interrupts are disabled here, a direct fallback swap is safe.
            noInterrupts();
            if (_swapCompleted != target) {
                immediateSwapLocked();
                ++_swapTimeouts;
            }
            interrupts();
            break;
        }
    }

    return true;
}

void DMDESPHub12::swapBuffers()
{
    (void)swapBuffersSync();
}

void DMDESPHub12::swapBuffersAndCopy()
{
    if (swapBuffersSync()) {
        memcpy((void *)fb, (const void *)displayfb, _stride * _height);
    }
}

void DMDESPHub12::loop()
{
    // Compatibility no-op. Refresh is Timer0-driven after start().
}

void ICACHE_RAM_ATTR DMDESPHub12::refresh()
{
    // Exact frame boundary: commit before phase 0 reads displayfb.
    if (phase == 0) commitPendingSwapFromISR();

    const int stride4 = _stride * 4;
    volatile uint8_t *data0;
    volatile uint8_t *data1;
    volatile uint8_t *data2;
    volatile uint8_t *data3;
    bool flipRow = ((_height & 0x10) == 0);

    for (byte y = 0; y < _height; y += 16) {
        if (!flipRow) {
            data0 = displayfb + _stride * (y + phase);
            data1 = data0 + stride4;
            data2 = data1 + stride4;
            data3 = data2 + stride4;

            int remaining = _stride;
            while (remaining > 0) {
                const uint16_t words = (remaining > 16) ? 16U : (uint16_t)remaining;
                volatile uint32_t *fifo = &SPI1W0;

                // One panel column group becomes one FIFO word. This removes
                // the intermediate packed[64] buffer used by v1.0.0 and is
                // especially useful for 5x1 and 6x1 chains (two FIFO bursts).
                for (uint16_t x = 0; x < words; ++x) {
                    const uint32_t v =
                        ((uint32_t)(*data3++))       |
                        ((uint32_t)(*data2++) << 8)  |
                        ((uint32_t)(*data1++) << 16) |
                        ((uint32_t)(*data0++) << 24);
                    fifo[x] = v;
                }

                spiCommitWordsISR(words);
                remaining -= words;
            }
            flipRow = true;
        } else {
            data0 = displayfb + _stride * (y + 16 - phase) - 1;
            data1 = data0 - stride4;
            data2 = data1 - stride4;
            data3 = data2 - stride4;

            int remaining = _stride;
            while (remaining > 0) {
                const uint16_t words = (remaining > 16) ? 16U : (uint16_t)remaining;
                volatile uint32_t *fifo = &SPI1W0;

                for (uint16_t x = 0; x < words; ++x) {
                    const uint32_t v =
                        ((uint32_t)flipBits[*data3--])       |
                        ((uint32_t)flipBits[*data2--] << 8)  |
                        ((uint32_t)flipBits[*data1--] << 16) |
                        ((uint32_t)flipBits[*data0--] << 24);
                    fifo[x] = v;
                }

                spiCommitWordsISR(words);
                remaining -= words;
            }
            flipRow = false;
        }
    }

    pinMode(DMDESPHUB12_PIN_OUTPUT_ENABLE, INPUT);
    digitalWrite(DMDESPHUB12_PIN_LATCH, HIGH);
    digitalWrite(DMDESPHUB12_PIN_LATCH, LOW);
    digitalWrite(DMDESPHUB12_PIN_PHASE_LSB, (phase & 0x01) != 0);
    digitalWrite(DMDESPHUB12_PIN_PHASE_MSB, (phase & 0x02) != 0);
    pinMode(DMDESPHUB12_PIN_OUTPUT_ENABLE, OUTPUT);
    analogWrite(DMDESPHUB12_PIN_OUTPUT_ENABLE, cr);

    phase = (phase + 1) & 0x03;
}

void DMDESPHub12::configureHardware()
{
    // Preserve the historical 0..1023 brightness API across ESP8266 core 2.x
    // and 3.x, whose default PWM range differs.
    analogWriteRange(1023);
    analogWriteFreq(16384);

    pinMode(SCK, SPECIAL);
    pinMode(MOSI, SPECIAL);
    SPI1C = 0;
    SPI1U = SPIUMOSI | SPIUDUPLEX | SPIUSSE;
    SPI1U1 = (7 << SPILMOSI) | (7 << SPILMISO);
    SPI1C1 = 0;
    SPI1C &= ~(SPICWBO | SPICRBO);
    SPI1U &= ~(SPIUSME);
    SPI1P &= ~(1 << 29);
    SPI.setFrequency(10000000);

    digitalWrite(DMDESPHUB12_PIN_PHASE_LSB, LOW);
    digitalWrite(DMDESPHUB12_PIN_PHASE_MSB, LOW);
    digitalWrite(DMDESPHUB12_PIN_LATCH, LOW);
    pinMode(DMDESPHUB12_PIN_OUTPUT_ENABLE, OUTPUT);
    digitalWrite(DMDESPHUB12_PIN_OUTPUT_ENABLE, HIGH); // blank until first phase
}

void DMDESPHub12::attachRefreshTimer()
{
    noInterrupts();
    timer0_detachInterrupt();
    timer0_isr_init();
    timer0_attachInterrupt(dmdespHub12HardRefreshISR);
    g_dmdespHub12 = this;
    timer0_write(ESP.getCycleCount() + _refreshCycles);
    _running = true;
    interrupts();
}

void DMDESPHub12::start()
{
    if (_running || !fb0 || !displayfb || !isValid()) return;

    // Timer0 and SPI1 are single-owner resources in this implementation.
    // Refuse to silently steal them from another active DMDESPHub12 instance.
    noInterrupts();
    const bool timerAvailable = (g_dmdespHub12 == nullptr || g_dmdespHub12 == this);
    interrupts();
    if (!timerAvailable) return;

    configureHardware();
    phase = 0;
    attachRefreshTimer();
}

void DMDESPHub12::stopRefresh()
{
    bool ownedTimer = false;

    noInterrupts();
    ownedTimer = (g_dmdespHub12 == this);

    // Finish a pending frame request before the timer is removed so callers
    // do not retain a drawing buffer that was expected to become visible.
    if (_doubleBuffer && _swapRequested) immediateSwapLocked();

    if (ownedTimer) {
        g_dmdespHub12 = nullptr;
        timer0_detachInterrupt();
    }
    _running = false;
    interrupts();

    // OE is active-low. Blank the panel while refresh is intentionally
    // paused by RefreshGuard or OTA/filesystem code.
    pinMode(DMDESPHUB12_PIN_OUTPUT_ENABLE, OUTPUT);
    digitalWrite(DMDESPHUB12_PIN_OUTPUT_ENABLE, HIGH);
}

void DMDESPHub12::setBrightness(uint16_t b)
{
    if (b > 1023U) b = 1023U;
    if (b == 1U) b = 2U;

    noInterrupts();
    cr = b;
    interrupts();
}

uint32_t DMDESPHub12::minimumRefreshIntervalUs() const
{
    if (_panelsHigh != 1) return 1000UL;
    if (_panelsWide <= 3) return 500UL;
    if (_panelsWide == 4) return 650UL;
    if (_panelsWide == 5) return 800UL;
    return 950UL;
}

uint32_t DMDESPHub12::recommendedRefreshIntervalUs() const
{
    return recommendedRefreshForPanels(_panelsWide, _panelsHigh);
}

void DMDESPHub12::setRefreshIntervalUs(uint32_t us)
{
    const uint32_t minimumUs = minimumRefreshIntervalUs();
    if (us < minimumUs) us = minimumUs;
    if (us > DMDESPHUB12_MAX_REFRESH_US) us = DMDESPHUB12_MAX_REFRESH_US;

    noInterrupts();
    _refreshUs = us;
    _refreshCycles = microsecondsToClockCycles(us);
    interrupts();
}

void DMDESPHub12::resetDiagnostics()
{
    noInterrupts();
    _isrCount = 0;
    _isrOverruns = 0;
    _maxIsrCycles = 0;
    _swapTimeouts = 0;
    interrupts();
}

DMDESPHub12::Color DMDESPHub12::fromRGB(uint8_t r, uint8_t g, uint8_t b)
{
    return (r || g || b) ? White : Black;
}
