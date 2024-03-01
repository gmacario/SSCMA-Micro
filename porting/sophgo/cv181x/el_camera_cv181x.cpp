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

#include "el_camera_cv181x.h"

#include "core/el_debug.h"
#include "el_config_porting.h"

#define XCLK_FREQ_HZ 15000000
#define FRAME_WIDTH  1280
#define FRAME_HEIGHT 720

namespace edgelab {
el_err_code_t CameraCV181x::vpss_init() {
    //VPSS_GRP_ATTR_S stVpssGrpAttr;
    VPSS_CHN_ATTR_S stVpssChnAttr;
    // RGB
    stVpssChnAttr.u32Width                     = FRAME_WIDTH;
    stVpssChnAttr.u32Height                    = FRAME_HEIGHT;
    stVpssChnAttr.enVideoFormat                = VIDEO_FORMAT_LINEAR;
    stVpssChnAttr.enPixelFormat                = PIXEL_FORMAT_RGB_888;
    stVpssChnAttr.stFrameRate.s32SrcFrameRate  = -1;
    stVpssChnAttr.stFrameRate.s32DstFrameRate  = -1;
    stVpssChnAttr.u32Depth                     = 0;
    stVpssChnAttr.bMirror                      = CVI_FALSE;
    stVpssChnAttr.bFlip                        = CVI_FALSE;
    stVpssChnAttr.stNormalize.bEnable          = CVI_FALSE;
    stVpssChnAttr.stAspectRatio.enMode         = ASPECT_RATIO_AUTO;
    stVpssChnAttr.stAspectRatio.bEnableBgColor = CVI_TRUE;
    stVpssChnAttr.stAspectRatio.u32BgColor     = 0x00000000;

    CVI_VPSS_SetChnAttr(this->Grp, this->Chn, &stVpssChnAttr);
    CVI_VPSS_EnableChn(this->Grp, this->Chn);

    //JPEG
    PARAM_VENC_CFG_S*     pstVencCfg  = PARAM_getVencCtx();
    VENC_RECV_PIC_PARAM_S stRecvParam = {0};

    pstVencCfg->pstVencChnCfg[this->venc_Chn].stRcParam.u16BitRate = 20480;
    pstVencCfg->pstVencChnCfg[this->venc_Chn].stRcParam.u16RcMode  = VENC_RC_MODE_MJPEGCBR;

    CVI_VPSS_GetChnAttr(this->jpeg_Grp, this->jpeg_Chn, &stVpssChnAttr);
    stVpssChnAttr.enPixelFormat = PIXEL_FORMAT_NV21;
    stVpssChnAttr.u32Width      = FRAME_WIDTH;
    stVpssChnAttr.u32Height     = FRAME_HEIGHT;
    CVI_VPSS_SetChnAttr(this->jpeg_Grp, this->jpeg_Chn, &stVpssChnAttr);

    pstVencCfg->pstVencChnCfg[this->venc_Chn].stChnParam.u16Width   = FRAME_WIDTH;
    pstVencCfg->pstVencChnCfg[this->venc_Chn].stChnParam.u16Height  = FRAME_HEIGHT;
    pstVencCfg->pstVencChnCfg[this->venc_Chn].stChnParam.u16EnType  = PT_JPEG;
    pstVencCfg->pstVencChnCfg[this->venc_Chn].stChnParam.u8DevId    = this->jpeg_Grp;
    pstVencCfg->pstVencChnCfg[this->venc_Chn].stChnParam.u8DevChnid = this->jpeg_Chn;

    MEDIA_VIDEO_VencInit(pstVencCfg);
    stRecvParam.s32RecvPicNum = -1;
    CVI_VENC_StartRecvFrame(this->venc_Chn, &stRecvParam);
    pstVencCfg->pstVencChnCfg[this->venc_Chn].stChnParam.u8InitStatus = 1;

    return EL_OK;
}

el_err_code_t CameraCV181x::init(size_t width, size_t height) {
    //return CVI_SUCCESS;
    PARAM_SYS_CFG_S* pstSysCtx = PARAM_getSysCtx();
    PARAM_VI_CFG_S*  pstViCfg  = PARAM_getViCtx();

    //media vi init
    CHECK_RET(MEDIA_VIDEO_SysVbInit(pstSysCtx), "MEDIA_VIDEO_SysVbInit failed");
    CHECK_RET(MEDIA_VIDEO_ViInit(pstViCfg), "MEDIA_VIDEO_ViInit failed");

    if (EL_OK != vpss_init()) {
        printf("init vpss failed!\n");
        this->_is_present = false;
    }

    this->_is_present = true;

    return this->_is_present ? EL_OK : EL_EIO;
}

el_err_code_t CameraCV181x::deinit() {
    PARAM_VI_CFG_S* pstViCfg = PARAM_getViCtx();
    MEDIA_VIDEO_ViDeinit(pstViCfg);
    MEDIA_VIDEO_SysVbDeinit();

    this->_is_present = false;
    return EL_OK;
}

el_err_code_t CameraCV181x::start_stream() { return EL_OK; }

el_err_code_t CameraCV181x::stop_stream() {
    // CVI_VI_ReleasePipeFrame(dev, this->stVideoFrame);
    if (this->stFrameInfo.stVFrame.enPixelFormat == PIXEL_FORMAT_RGB_888) {
        if (CVI_VPSS_ReleaseChnFrame(this->Grp, this->Chn, &this->stFrameInfo) != CVI_SUCCESS)
            printf("CVI_VPSS_ReleaseChnFrame fail GRP:%d CHN:%d\n", this->Grp, this->Chn);
    } else {
        if (MEDIA_VIDEO_VencReleaseStream(this->venc_Chn, this->pstStream) != CVI_SUCCESS)
            printf("MEDIA_VIDEO_VencReleaseStream failed\n");
    }

    return EL_OK;
}

el_err_code_t CameraCV181x::get_frame(el_img_t* img) {
    CVI_S32 s32MilliSec = 1000;
    if (CVI_VPSS_GetChnFrame(this->Grp, this->Chn, &this->stFrameInfo, s32MilliSec) != CVI_SUCCESS) {
        printf("Get frame fail \n");
        return EL_EIO;
    }
    printf("get frame success! pixel_format:%d width:%d height:%d\n",
           this->stFrameInfo.stVFrame.enPixelFormat,
           this->stFrameInfo.stVFrame.u32Width,
           this->stFrameInfo.stVFrame.u32Height);
    for (int i = 0; i < 3; ++i) {
        printf("plane(%d): paddr(%lx) vaddr(%p) stride(%d)\n",
               i,
               this->stFrameInfo.stVFrame.u64PhyAddr[i],
               this->stFrameInfo.stVFrame.pu8VirAddr[i],
               this->stFrameInfo.stVFrame.u32Stride[i]);
    }

    img->width  = this->stFrameInfo.stVFrame.u32Width;
    img->height = this->stFrameInfo.stVFrame.u32Height, img->data = (CVI_U8*)this->stFrameInfo.stVFrame.u64PhyAddr[0];
    img->size   = this->stFrameInfo.stVFrame.u32Stride[0] * this->stFrameInfo.stVFrame.u32Height;
    img->format = EL_PIXEL_FORMAT_RGB888;

    return EL_OK;
}

el_err_code_t CameraCV181x::get_processed_frame(el_img_t* img) {
    VENC_STREAM_S stStream = {0};
    this->pstStream        = &stStream;
    
    if (MEDIA_VIDEO_VencGetStream(this->venc_Chn, this->pstStream, 2000) != CVI_SUCCESS) {
        return EL_EIO;
    }

    img->data   = (CVI_U8*)&this->pstStream->pstPack[0].pu8Addr + this->pstStream->pstPack[0].u32Offset;
    img->size   = this->pstStream->pstPack[0].u32Len - this->pstStream->pstPack[0].u32Offset;
    img->format = EL_PIXEL_FORMAT_JPEG;

    return EL_OK;
}

}  // namespace edgelab
