/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Hongtai Liu (Seeed Technology Inc.)
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

#ifndef _EL_ESP_AT_H_
#define _EL_ESP_AT_H_

extern "C" {
#include <WE2_core.h>
#include <hx_drv_gpio.h>
#include <hx_drv_scu.h>
#include <hx_drv_uart.h>
}

#include "core/utils/el_ringbuffer.hpp"

#define ESP_AT_MAX_RX_PAYLOAD 32
#define ESP_AT_MAX_TX_PAYLOAD 4095

namespace edgelab {

class ESP_AT {
   public:
    ESP_AT();
    ~ESP_AT();

    el_err_code_t init();
    el_err_code_t deinit();

    size_t write(const char* buffer, size_t size);
    size_t read(char* buffer, size_t size);
    size_t aviliable();
};

}  // namespace edgelab

#endif
