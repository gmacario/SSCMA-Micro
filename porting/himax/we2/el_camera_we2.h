/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Seeed Technology Inc.
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

#ifndef _EL_CAMERA_WE2_H_
#define _EL_CAMERA_WE2_H_

#include "core/el_types.h"
#include "porting/el_camera.h"

namespace edgelab {

class CameraWE2 final : public Camera {
   public:
    CameraWE2();
    ~CameraWE2() = default;

    el_err_code_t init(SensorOptIdType opt_id) override;
    el_err_code_t deinit() override;

    el_err_code_t start_stream() override;
    el_err_code_t stop_stream() override;

    el_err_code_t get_frame(el_img_t* img) override;
    el_err_code_t get_processed_frame(el_img_t* img) override;

private:
    el_pixel_rotate_t _rotation_override = EL_PIXEL_ROTATE_0;
};

}  // namespace edgelab

#endif
