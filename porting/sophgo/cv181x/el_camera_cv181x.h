/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Hongtai Liu (Seeed Technology Inc.)
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

#ifndef _EL_CAMERA_ESP_H_
#define _EL_CAMERA_ESP_H_

#include "cvi_venc.h"
#include "cvi_vpss.h"
#include "media_video.h"

#include "core/el_types.h"
#include "porting/el_camera.h"

namespace edgelab {

#define CHECK_RET(actual, fmt, arg...)                                   \
	do {																		 \
		if ((actual) != 0) {													 \
			MEDIABUG_PRINTF("[%d]:%s() \n" fmt, __LINE__, __func__, ## arg); \
			return EL_EIO;																\
		}																		 \
	} while (0)

class CameraCV181x final : public Camera {
   public:
    CameraCV181x()  = default;
    ~CameraCV181x() = default;

    el_err_code_t init(size_t width, size_t height) override;
    el_err_code_t deinit() override;

    el_err_code_t start_stream() override;
    el_err_code_t stop_stream() override;

    el_err_code_t get_frame(el_img_t* img) override;
    el_err_code_t get_processed_frame(el_img_t* img) override;

   protected:
    el_err_code_t vpss_init();
    int Grp = 0;
    int Chn = 0;
    int venc_Chn = 0;
    int jpeg_Grp = 1;
    int jpeg_Chn = 1;
    VENC_STREAM_S *pstStream;
    VIDEO_FRAME_INFO_S stFrameInfo;
};

}  // namespace edgelab

#endif
