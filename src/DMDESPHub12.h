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

#ifndef DMDESPHUB12_LIBRARY_H
#define DMDESPHUB12_LIBRARY_H

#define DMDESPHUB12_VERSION_MAJOR 1
#define DMDESPHUB12_VERSION_MINOR 1
#define DMDESPHUB12_VERSION_PATCH 0
#define DMDESPHUB12_VERSION_STRING "1.1.0"

#include <Arduino.h>
#include "DMDESPHub12Bitmap.h"
#include <SPI.h>

#if defined(ESP8266)
#include <user_interface.h>
#include <osapi.h>
#else
#error "DMDESPHub12 currently supports ESP8266 only."
#endif

class DMDESPHub12 : public DMDESPHub12Bitmap
{
public:
    explicit DMDESPHub12(int widthPanels = 1, int heightPanels = 1);
    ~DMDESPHub12();

    bool doubleBuffer() const { return _doubleBuffer; }
    void setDoubleBuffer(bool doubleBuffer);

    // Source-compatible API. In v1.1.0 these calls wait until the next
    // full-scan boundary before returning. The actual pointer exchange is
    // performed by the Timer0 ISR immediately before phase 0 is scanned.
    void swapBuffers();
    void swapBuffersAndCopy();

    // Explicit synchronized form. timeoutUs == 0 selects an automatic
    // timeout derived from the current refresh interval. Returns false only
    // when double buffering is unavailable. A timeout fallback still commits
    // the frame and is counted by swapTimeouts().
    bool swapBuffersSync(uint32_t timeoutUs = 0);
    bool swapPending() const { return _swapRequested; }
    uint32_t swapCount() const { return _swapCompleted; }
    uint32_t swapTimeouts() const { return _swapTimeouts; }

    // Compatibility stub: scanning is Timer0-driven after start().
    void loop();

    // One HUB12 scan phase. Do not call manually when start() is active.
    void ICACHE_RAM_ATTR refresh();

    void start();
    void stopRefresh();

    void setBrightness(uint16_t crh);
    static Color fromRGB(uint8_t r, uint8_t g, uint8_t b);

    void setRefreshIntervalUs(uint32_t us);
    uint32_t refreshIntervalUs() const { return _refreshUs; }
    uint32_t minimumRefreshIntervalUs() const;
    uint32_t recommendedRefreshIntervalUs() const;

    uint8_t panelsWide() const { return _panelsWide; }
    uint8_t panelsHigh() const { return _panelsHigh; }
    bool optimizedForConfiguration() const {
        return (_panelsHigh == 1 && _panelsWide >= 1 && _panelsWide <= 6);
    }

    // Diagnostics. Values are updated by the ISR unless noted otherwise.
    uint32_t refreshISRCount() const { return _isrCount; }
    uint32_t refreshOverruns() const { return _isrOverruns; }
    uint32_t maxRefreshCycles() const { return _maxIsrCycles; }
    bool refreshRunning() const { return _running; }
    uint32_t refreshUtilizationPermille() const {
        if (!_refreshCycles) return 0;
        const uint64_t p = (uint64_t)_maxIsrCycles * 1000ULL;
        return (uint32_t)(p / _refreshCycles);
    }
    void resetDiagnostics();

private:
    DMDESPHub12(const DMDESPHub12 &other) : DMDESPHub12Bitmap(other) {}
    DMDESPHub12 &operator=(const DMDESPHub12 &) { return *this; }

    void configureHardware();
    void attachRefreshTimer();
    void immediateSwapLocked();
    void ICACHE_RAM_ATTR commitPendingSwapFromISR();
    uint32_t automaticSwapTimeoutUs() const;

    uint16_t cr;
    bool _doubleBuffer;
    volatile bool _running;
    volatile uint8_t phase;
    uint8_t _panelsWide;
    uint8_t _panelsHigh;
    uint8_t *fb0;
    uint8_t *fb1;
    uint8_t *displayfb;

    volatile bool _swapRequested;
    volatile uint32_t _swapCompleted;
    volatile uint32_t _swapTimeouts;

    uint32_t _refreshUs;
    uint32_t _refreshCycles;

    volatile uint32_t _isrCount;
    volatile uint32_t _isrOverruns;
    volatile uint32_t _maxIsrCycles;

    friend void ICACHE_RAM_ATTR dmdespHub12HardRefreshISR();
};

/*
 * RAII guard for operations that must not overlap the HardRefresh ISR.
 *
 * Required around flash/filesystem/EEPROM writes and the complete OTA upload
 * write window. It is not required for normal drawing, ordinary HTTP GET
 * responses, or read-only configuration access.
 *
 * The guard is nested-safe: only the outer guard restarts a display that was
 * running when it was created.
 */
class DMDESPHub12RefreshGuard
{
public:
    explicit DMDESPHub12RefreshGuard(DMDESPHub12 &display)
        : _display(display), _wasRunning(display.refreshRunning())
    {
        if (_wasRunning) _display.stopRefresh();
    }

    ~DMDESPHub12RefreshGuard()
    {
        if (_wasRunning) _display.start();
    }

private:
    DMDESPHub12RefreshGuard(const DMDESPHub12RefreshGuard &);
    DMDESPHub12RefreshGuard &operator=(const DMDESPHub12RefreshGuard &);

    DMDESPHub12 &_display;
    bool _wasRunning;
};

#endif
