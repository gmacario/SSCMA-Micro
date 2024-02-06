/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Seeed Technology Co.,Ltd
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

#include "el_spi_at.h"

extern "C" {
#include <console_io.h>
#include <hx_drv_gpio.h>
#include <hx_drv_scu.h>
#include <hx_drv_spi.h>
}

#include <cctype>
#include <cstdint>

#include "el_config_porting.h"
namespace edgelab {

namespace porting {

typedef struct EL_ATTR_ALIGNED(4) {
    uint32_t direct : 8;  // 0x01: read, 0x02: write
    uint32_t seq    : 8;
    uint32_t len    : 16;
} trans_ctrl_t;

enum event_t {
    AT_TRANS_TX_START_WAIT = 0x01,
    AT_TRANS_TX_START      = 0x02,
    AT_TRANS_RX_START_WAIT = 0x04,
    AT_TRANS_RX_START      = 0x08,
    AT_TRANS_DONE_WAIT     = 0x10,
    AT_TRANS_DONE          = 0x20
};

static lwRingBuffer*      _rb_rx = nullptr;
static lwRingBuffer*      _rb_tx = nullptr;
static char               _buf_rx[4095]{};
static char               _buf_tx[4095]{};
volatile static bool      _tx_busy  = false;
volatile static DEV_SPI*  _spi      = nullptr;
volatile static uint8_t   _send_seq = 0;
static SemaphoreHandle_t  _mutex_tx = nullptr;
static SemaphoreHandle_t  _tx_done  = nullptr;
static EventGroupHandle_t _event    = nullptr;
static TaskHandle_t       _task     = nullptr;

static const uint8_t cmd_query[3]       = {0x02, 0x04, 0x00};
static const uint8_t cmd_read_start[3]  = {0x04, 0x04, 0x00};
static const uint8_t cmd_read_done[3]   = {0x08, 0x04, 0x00};
static const uint8_t cmd_write_start[3] = {0x03, 0x00, 0x00};
static const uint8_t cmd_write_done[3]  = {0x07, 0x00, 0x00};

#define ESP_AT_SPI_MAX_PAYLOAD_SIZE 4092
#define ESP_AT_SPI_MAX_WAIT_TIME_MS 100

static void _handshake_cb(uint8_t group, uint8_t aIndex) {
    BaseType_t  taskwaken = pdFALSE;
    uint8_t     value     = 0;
    EventBits_t event     = xEventGroupGetBitsFromISR(_event);
    hx_drv_gpio_get_in_value(AON_GPIO0, &value);
    if (value == 1) {
        if (event & AT_TRANS_TX_START_WAIT) {
            xEventGroupClearBitsFromISR(_event, AT_TRANS_TX_START_WAIT);
            xEventGroupSetBitsFromISR(_event, AT_TRANS_TX_START, &taskwaken);
        } else if (event & AT_TRANS_RX_START_WAIT) {
            xEventGroupClearBitsFromISR(_event, AT_TRANS_RX_START_WAIT);
            xEventGroupSetBitsFromISR(_event, AT_TRANS_RX_START, &taskwaken);
        }
    } else {
        if (event & AT_TRANS_DONE_WAIT) {
            xEventGroupClearBitsFromISR(_event, AT_TRANS_DONE_WAIT);
            xEventGroupSetBitsFromISR(_event, AT_TRANS_DONE, &taskwaken);
        }
    }
    portYIELD_FROM_ISR(taskwaken);
}

static void spi_dma_cb(void* status) {
    BaseType_t taskwaken = pdFALSE;
    xSemaphoreGiveFromISR(_tx_done, &taskwaken);
    portYIELD_FROM_ISR(taskwaken);
}

static void _spi_trans_ctrl_task(void* arg) {
    uint8_t value = 0;
    while (true) {
        trans_ctrl_t _trans_ctrl = {0};
        SCB_CleanDCache_by_Addr((volatile void*)&(_trans_ctrl), 4);
        _spi->spi_write_then_read_dma((void*)cmd_query, sizeof(cmd_query), (void*)&(_trans_ctrl), 4, (void*)spi_dma_cb);
        xSemaphoreTake(_tx_done, ESP_AT_SPI_MAX_WAIT_TIME_MS);
        if (_trans_ctrl.direct == 0x01) {
            el_printf("[%d]read: %d\n", __LINE__, _trans_ctrl.len);
            memset((void*)_buf_rx, 0, sizeof(_buf_rx));
            SCB_CleanDCache_by_Addr((volatile void*)_buf_rx, sizeof(_buf_rx));
            _spi->spi_write_then_read_dma(
              (void*)cmd_read_start, sizeof(cmd_read_start), (void*)_buf_rx, _trans_ctrl.len, (void*)spi_dma_cb);
            xSemaphoreTake(_tx_done, ESP_AT_SPI_MAX_WAIT_TIME_MS);
            _buf_rx[_trans_ctrl.len + 1] = '\0';
            el_printf("[%d]recv: %s\n", __LINE__, _buf_rx);
            _rb_rx->put(_buf_rx, _trans_ctrl.len);
            xEventGroupSetBits(_event, AT_TRANS_DONE_WAIT);
            _spi->spi_write(cmd_read_done, sizeof(cmd_read_done));
            if (AT_TRANS_DONE &
                xEventGroupWaitBits(_event, AT_TRANS_DONE, pdFALSE, pdTRUE, ESP_AT_SPI_MAX_WAIT_TIME_MS)) {
                xEventGroupClearBits(_event, AT_TRANS_DONE);
            }
        } else if (_trans_ctrl.direct == 0x02) {
            // flush buffer
            el_printf("[%d]write: %d\n", __LINE__, sizeof(cmd_write_start));
            if (_rb_tx->size() == 0) {
                el_printf("flush buffer\n");
                memset((void*)_buf_tx, 0, sizeof(_buf_tx));
                memcpy(_buf_tx, cmd_write_start, sizeof(cmd_write_start));
                _spi->spi_write_dma(_buf_tx, sizeof(_buf_tx), (void*)spi_dma_cb);
                xSemaphoreTake(_tx_done, ESP_AT_SPI_MAX_WAIT_TIME_MS);
                xEventGroupSetBits(_event, AT_TRANS_DONE_WAIT);
                _spi->spi_write(cmd_write_done, sizeof(cmd_write_done));
                if (AT_TRANS_DONE &
                    xEventGroupWaitBits(_event, AT_TRANS_DONE, pdFALSE, pdTRUE, ESP_AT_SPI_MAX_WAIT_TIME_MS)) {
                    xEventGroupClearBits(_event, AT_TRANS_DONE);
                } else {
                    el_printf("flush buffer timeout\n");
                }
            }
        }
        xSemaphoreGive(_mutex_tx);
    }
}

}  // namespace porting

using namespace porting;

SPIAT::SPIAT() : _console_spi(nullptr) {}

SPIAT::~SPIAT() { deinit(); }

el_err_code_t SPIAT::init() {
    if (this->_is_present) [[unlikely]]
        return EL_EPERM;

    SCU_PAD_PULL_LIST_T pull_cfg;
    pull_cfg.pa0.pull_en  = SCU_PAD_PULL_EN;
    pull_cfg.pa0.pull_sel = SCU_PAD_PULL_DOWN;
    pull_cfg.pb2.pull_en  = SCU_PAD_PULL_EN;
    pull_cfg.pb2.pull_sel = SCU_PAD_PULL_DOWN;
    pull_cfg.pb3.pull_en  = SCU_PAD_PULL_EN;
    pull_cfg.pb3.pull_sel = SCU_PAD_PULL_DOWN;

    hx_drv_scu_set_PB2_pinmux(SCU_PB2_PINMUX_SPI_M_DO_1, 0);
    hx_drv_scu_set_PB3_pinmux(SCU_PB3_PINMUX_SPI_M_DI_1, 0);
    hx_drv_scu_set_PB4_pinmux(SCU_PB4_PINMUX_SPI_M_SCLK_1, 1);
    hx_drv_scu_set_PB11_pinmux(SCU_PB11_PINMUX_SPI_M_CS, 1);

    _console_spi = hx_drv_spi_mst_get_dev(USE_DW_SPI_MST_S);
    _console_spi->spi_open(DEV_MASTER_MODE, 12000000);

    if (!_rb_rx) [[likely]]
        _rb_rx = new lwRingBuffer{8192};

    if (!_rb_tx) [[likely]]
        _rb_tx = new lwRingBuffer{32768};

    _event = xEventGroupCreate();

    _mutex_tx = xSemaphoreCreateMutex();

    if (!_tx_done) [[likely]] {
        _tx_done = xSemaphoreCreateCounting(1, 0);
    }

    EL_LOGD("spi at init: %d\n", this->_is_present);

    EL_ASSERT(_rb_rx);
    EL_ASSERT(_rb_tx);
    EL_ASSERT(_mutex_tx);

    _spi = _console_spi;

    this->_is_present = _console_spi != nullptr;
    _tx_busy          = false;

    xTaskCreate(_spi_trans_ctrl_task, "_spi_trans_ctrl_task", 1024, nullptr, 3, nullptr);

    hx_drv_scu_set_PA0_pinmux(SCU_PA0_PINMUX_AON_GPIO0_0, 0);
    hx_drv_scu_set_all_pull_cfg(&pull_cfg);
    hx_drv_gpio_set_int_type(AON_GPIO0, GPIO_IRQ_TRIG_TYPE_EDGE_BOTH);
    hx_drv_gpio_cb_register(AON_GPIO0, _handshake_cb);
    hx_drv_gpio_set_input(AON_GPIO0);
    hx_drv_gpio_set_int_enable(AON_GPIO0, 1);

    return this->_is_present ? EL_OK : EL_EIO;
}

el_err_code_t SPIAT::deinit() {
    if (!this->_is_present) [[unlikely]]
        return EL_EPERM;

    this->_is_present = !hx_drv_spi_mst_deinit(USE_DW_SPI_MST_S) ? false : true;

    delete _rb_rx;
    delete _rb_tx;
    vSemaphoreDelete(_mutex_tx);

    _rb_rx    = nullptr;
    _rb_tx    = nullptr;
    _mutex_tx = nullptr;

    return !this->_is_present ? EL_OK : EL_EIO;
}

char SPIAT::echo(bool only_visible) {
    if (!this->_is_present) [[unlikely]]
        return '\0';

    char c{get_char()};
    if (only_visible && !isprint(c)) return c;

    _console_spi->spi_write(&c, sizeof(c));

    return c;
}

char SPIAT::get_char() {
    if (!this->_is_present) [[unlikely]]
        return '\0';

    return porting::_rb_rx->get();
}

std::size_t SPIAT::get_line(char* buffer, size_t size, const char delim) {
    if (!this->_is_present) [[unlikely]]
        return 0;

    return porting::_rb_rx->extract(delim, buffer, size);
}

std::size_t SPIAT::read_bytes(char* buffer, size_t size) {
    if (!this->_is_present) [[unlikely]]
        return 0;

    size_t time_start = el_get_time_ms();
    size_t read{0};
    size_t pos_of_bytes{0};

    while (size) {
        pos_of_bytes = _rb_rx->size();
        if (pos_of_bytes) {
            read += _rb_rx->get(buffer + read, size);
            size -= read;
        }
        if (el_get_time_ms() - time_start > 3000) {
            break;
        }
    }

    return read;
}

std::size_t SPIAT::send_bytes(const char* buffer, size_t size) {
    if (!this->_is_present) [[unlikely]]
        return 0;

    size_t   time_start = el_get_time_ms();
    uint16_t bytes_to_send{0};
    size_t   sent  = 0;
    uint8_t  value = 0;

    xSemaphoreTake(_mutex_tx, portMAX_DELAY);
    uint8_t send_req[7] = {0x01, 0x00, 0x00, 0xfe, 0, 0, 0};

    if (_rb_tx->free() < size) {
        goto done;
    }

    hx_drv_gpio_get_in_value(AON_GPIO0, &value);
    if (value) {
        // if value is high, flush tx buffer
        goto done;
    }

    _rb_tx->put(buffer, size);

    while (size) {
        bytes_to_send = _rb_tx->size() < ESP_AT_SPI_MAX_PAYLOAD_SIZE ? _rb_tx->size() : ESP_AT_SPI_MAX_PAYLOAD_SIZE;
        el_printf("\nsend: %d %d\n", bytes_to_send, _rb_tx->size());
        send_req[4] = _send_seq++;
        send_req[5] = bytes_to_send & 0xff;
        send_req[6] = (bytes_to_send >> 8) & 0xff;
        xEventGroupSetBits(_event, AT_TRANS_TX_START_WAIT);
        _spi->spi_write(send_req, sizeof(send_req));
        if (AT_TRANS_TX_START &
            xEventGroupWaitBits(_event, AT_TRANS_TX_START, pdFALSE, pdTRUE, ESP_AT_SPI_MAX_WAIT_TIME_MS)) {
            xEventGroupClearBits(_event, AT_TRANS_TX_START);
        } else {
            hx_drv_gpio_get_in_value(AON_GPIO0, &value);
            if (!value) {  // already send
                continue;
            }
        }

        trans_ctrl_t _trans_ctrl = {0};
        SCB_CleanDCache_by_Addr((volatile void*)&(_trans_ctrl), 4);
        _spi->spi_write_then_read_dma((void*)cmd_query, sizeof(cmd_query), (void*)&(_trans_ctrl), 4, (void*)spi_dma_cb);
        xSemaphoreTake(_tx_done, ESP_AT_SPI_MAX_WAIT_TIME_MS);

        if (_trans_ctrl.direct == 0x01) {
            // read
            el_printf("[%d]read: %d\n", __LINE__, _trans_ctrl.len);
            memset(_buf_rx, 0, sizeof(_buf_rx));
            SCB_CleanDCache_by_Addr((volatile void*)_buf_rx, sizeof(_buf_rx));
            _spi->spi_write_then_read_dma(
              (void*)cmd_read_start, sizeof(cmd_read_start), (void*)_buf_rx, _trans_ctrl.len, (void*)spi_dma_cb);
            xSemaphoreTake(_tx_done, ESP_AT_SPI_MAX_WAIT_TIME_MS);
            _buf_rx[_trans_ctrl.len + 1] = '\0';
            el_printf("[%d]recv: %s\n", __LINE__, _buf_rx);
            _rb_rx->put(_buf_rx, _trans_ctrl.len);
            xEventGroupSetBits(_event, AT_TRANS_DONE_WAIT);
            _spi->spi_write(cmd_read_done, sizeof(cmd_read_done));
            if (AT_TRANS_DONE &
                xEventGroupWaitBits(_event, AT_TRANS_DONE, pdFALSE, pdTRUE, ESP_AT_SPI_MAX_WAIT_TIME_MS)) {
                xEventGroupClearBits(_event, AT_TRANS_DONE);
            }
            _trans_ctrl.direct = 0x02;
            el_sleep(5);
        }
        if (_trans_ctrl.direct == 0x02) {
            // write
            el_printf("\nwrite: %d\n", __LINE__);
            hx_drv_gpio_get_in_value(AON_GPIO0, &value);
            if (!value) {
                el_printf("\nwait tx failure\n");
                continue;
            }
            el_printf("\nwait tx success\n");
            size -= bytes_to_send;
            sent += bytes_to_send;
            memcpy(_buf_tx, cmd_write_start, sizeof(cmd_write_start));
            _rb_tx->get(_buf_tx + sizeof(cmd_write_start), bytes_to_send);
            _spi->spi_write_dma(_buf_tx, bytes_to_send + sizeof(cmd_write_start), (void*)spi_dma_cb);
            xSemaphoreTake(_tx_done, ESP_AT_SPI_MAX_WAIT_TIME_MS);
            xEventGroupSetBits(_event, AT_TRANS_DONE_WAIT);
            _spi->spi_write(cmd_write_done, sizeof(cmd_write_done));
            if (AT_TRANS_DONE &
                xEventGroupWaitBits(_event, AT_TRANS_DONE, pdFALSE, pdTRUE, ESP_AT_SPI_MAX_WAIT_TIME_MS)) {
                xEventGroupClearBits(_event, AT_TRANS_DONE);
            }
        }
    }

done:
    xSemaphoreGive(_mutex_tx);
    return sent;
}

std::size_t SPIAT::available() {
    if (!this->_is_present) [[unlikely]]
        return 0;
    return _rb_rx->size();
}

std::size_t SPIAT::free() {
    if (!this->_is_present) [[unlikely]]
        return 0;
    return _rb_tx->free();
}

}  // namespace edgelab