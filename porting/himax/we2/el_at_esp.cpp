/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Seeed Technology Co.,Ltd
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

#include "el_at_esp.h"

#include <cctype>
#include <cstdint>

#include "core/el_debug.h"
#include "core/el_types.h"
#include "el_config_porting.h"

namespace edgelab {

static SemaphoreHandle_t _tx_sem;
static lwRingBuffer*     _rb_rx = nullptr;
static lwRingBuffer*     _rb_tx = nullptr;
static char              _buf_rx[ESP_AT_MAX_RX_PAYLOAD]{};
static char              _buf_tx[ESP_AT_MAX_TX_PAYLOAD]{};
DEV_UART*                _uart;

namespace porting {

static void _uart_dma_recv(void*) {
    _rb_rx->put(_buf_rx[0]);
    _uart->uart_read_udma(_buf_rx, 1, (void*)_uart_dma_recv);
}

static void _uart_dma_send(void*) {
    BaseType_t taskwaken = pdFALSE;
    size_t remaind = _rb_tx->size() < 4095 ? _rb_tx->size() : 4095;
    if (remaind != 0) {
        _rb_tx->get(_buf_tx, remaind);
        SCB_CleanDCache_by_Addr((volatile void*)_buf_tx, remaind);
        _uart->uart_write_udma(_buf_tx, remaind, (void*)_uart_dma_send);
    } else {
        xSemaphoreGiveFromISR(_tx_sem, &taskwaken);
        portYIELD_FROM_ISR(taskwaken);
    }
}

}  // namespace porting

using namespace porting;

ESP_AT::ESP_AT() {}

ESP_AT::~ESP_AT() {}
el_err_code_t ESP_AT::init() {
    hx_drv_scu_set_PB6_pinmux(SCU_PB6_PINMUX_UART1_RX, 0);
    hx_drv_scu_set_PB7_pinmux(SCU_PB7_PINMUX_UART1_TX, 0);
    hx_drv_uart_init(USE_DW_UART_1, HX_UART1_BASE);
    _uart = hx_drv_uart_get_dev(USE_DW_UART_1);
    _uart->uart_open(UART_BAUDRATE_921600);
    memset((void*)_buf_tx, 0, sizeof(_buf_tx));
    _uart->uart_read_udma(_buf_rx, 1, (void*)_uart_dma_recv);

    if (!_rb_rx) [[likely]]
        _rb_rx = new lwRingBuffer{8192};

    if (!_rb_tx) [[likely]]
        _rb_tx = new lwRingBuffer{32768};

    _tx_sem = xSemaphoreCreateBinary();

    return EL_OK;
}

el_err_code_t ESP_AT::deinit() { return EL_OK; }

size_t ESP_AT::write(const char* buffer, size_t size) {
    size_t time_start = el_get_time_ms();
    size_t bytes_to_send{0};
    size_t sent = 0;

    xSemaphoreTake(_tx_sem, portMAX_DELAY);

    _rb_tx->put(buffer, size);

    bytes_to_send = _rb_tx->size() < 4095 ? _rb_tx->size() : 4095;
    _rb_tx->get(_buf_tx, bytes_to_send);
    SCB_CleanDCache_by_Addr((volatile void*)_buf_tx, bytes_to_send);
    _uart->uart_write_udma(_buf_tx, bytes_to_send, (void*)_uart_dma_send);

    return sent;
}

size_t ESP_AT::read(char* buffer, size_t size) { return _rb_rx->get(buffer, size); }

size_t ESP_AT::aviliable() { return _rb_rx->size(); }

}  // namespace edgelab
