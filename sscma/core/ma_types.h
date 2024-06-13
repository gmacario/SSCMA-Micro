#ifndef _MA_C_TYPES_H_
#define _MA_C_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "core/ma_compiler.h"
#include "core/ma_config_internal.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    MA_FAILED   = -1,  // UNKOWN ERROR
    MA_OK       = 0,   // No error
    MA_AGAIN    = 1,   // Not ready yet
    MA_ELOG     = 2,   // logic error
    MA_ETIMOUT  = 3,   // Timeout
    MA_EIO      = 4,   // IO error
    MA_EINVAL   = 5,   // Invalid argument
    MA_ENOMEM   = 6,   // Not enough memory
    MA_EBUSY    = 7,   // Busy
    MA_ENOTSUP  = 8,   // Not supported yet
    MA_EPERM    = 9,   // Operation not permitted
    MA_ENOENT   = 10,  // No such entity
    MA_EEXIST   = 11,  // the entity already exists
    MA_OVERFLOW = 12,  // overflow
} ma_err_t;

typedef struct {
    void* pool;
    size_t size;
    bool own;
} ma_memory_pool_t;

typedef struct {
    float scale;
    int32_t zero_point;
} ma_quant_param_t;

typedef struct {
    uint32_t size;
    int32_t dims[MA_ENGINE_SHAPE_MAX_DIM];
} ma_shape_t;

typedef enum {
    MA_TENSOR_TYPE_NONE = 0,
    MA_TENSOR_TYPE_U8   = 1,
    MA_TENSOR_TYPE_S8   = 2,
    MA_TENSOR_TYPE_U16  = 3,
    MA_TENSOR_TYPE_S16  = 4,
    MA_TENSOR_TYPE_U32  = 5,
    MA_TENSOR_TYPE_S32  = 6,
    MA_TENSOR_TYPE_U64  = 7,
    MA_TENSOR_TYPE_S64  = 8,
    MA_TENSOR_TYPE_F16  = 9,
    MA_TENSOR_TYPE_F32  = 10,
    MA_TENSOR_TYPE_F64  = 11,
    MA_TENSOR_TYPE_STR  = 12,
    MA_TENSOR_TYPE_BOOL = 13,
    MA_TENSOR_TYPE_BF16 = 14,
} ma_tensor_type_t;

typedef struct {
    ma_shape_t shape;
    ma_quant_param_t quant_param;
    ma_tensor_type_t type;
    size_t size;
#if MA_USE_ENGINE_TENSOR_INDEX
    size_t index;
#endif
#if MA_USE_ENGINE_TENSOR_NAME
    const char* name;
#endif
    union {
        void* data;
        uint8_t* u8;
        int8_t* s8;
        uint16_t* u16;
        int16_t* s16;
        uint32_t* u32;
        int32_t* s32;
        uint64_t* u64;
        int64_t* s64;
        float* f32;
        double* f64;
    } data;
    bool is_variable;  // For constant tensor
} ma_tensor_t;

typedef enum {
    MA_PIXEL_FORMAT_RGB888 = 0,
    MA_PIXEL_FORMAT_RGB565,
    MA_PIXEL_FORMAT_YUV422,
    MA_PIXEL_FORMAT_GRAYSCALE,
    MA_PIXEL_FORMAT_JPEG,
    MA_PIXEL_FORMAT_UNKNOWN,
} ma_pixel_format_t;

typedef enum {
    MA_PIXEL_ROTATE_0 = 0,
    MA_PIXEL_ROTATE_90,
    MA_PIXEL_ROTATE_180,
    MA_PIXEL_ROTATE_270,
    MA_PIXEL_ROTATE_UNKNOWN,
} ma_pixel_rotate_t;

typedef struct {
    uint8_t* data;
    size_t size;
    uint16_t width;
    uint16_t height;
    ma_pixel_format_t format;
    ma_pixel_rotate_t rotate;
} ma_img_t;


typedef enum {
    MA_MODEL_DETECTOR       = 0,
    MA_MODEL_CLASSIFIER     = 1,
    MA_MODEL_SEGMENTER      = 2,
    MA_MODEL_POSE_ESTIMATOR = 3,
    MA_MODEL_POINTCLOUD     = 4,
} ma_model_type_t;

typedef struct {
    int64_t preprocess;
    int64_t inference;
    int64_t postprocess;
} ma_perf_t;


typedef struct {
    float x;
    float y;
    float score;
    int target;
} ma_point_t;

typedef struct {
    float score;
    int target;
} ma_class_t;

typedef struct {
    float x;
    float y;
    float w;
    float h;
    float score;
    int target;
} ma_bbox_t;

typedef enum {
    MA_MODEL_CFG_OPT_THRESHOLD = 0,
    MA_MODEL_CFG_OPT_NMS       = 1,
    MA_MODEL_CFG_OPT_TOPK      = 2,
} ma_model_cfg_opt_t;


typedef enum {
    MA_TRANSPORT_UNKOWN  = 0,
    MA_TRANSPORT_CONSOLE = 1,
    MA_TRANSPORT_SERIAL  = 2,
    MA_TRANSPORT_SPI     = 3,
    MA_TRANSPORT_I2C     = 4,
    MA_TRANSPORT_MQTT    = 5,
    MA_TRANSPORT_TCP     = 6,
    MA_TRANSPORT_UDP     = 7
} ma_transport_type_t;

typedef enum {
    MA_TRANSPORT_CFG_OPT_NONE    = 0,
    MA_TRANSPORT_CFG_OPT_TIMEOUT = 1,
} ma_transport_cfg_opt_t;

typedef enum {
    MA_REPLY_RESPONSE = 0,
    MA_REPLY_EVENT    = 1,
    MA_REPLY_LOG      = 2,
} ma_reply_t;


#ifdef __cplusplus
}
#endif

#endif  // _MA_C_TYPES_H_