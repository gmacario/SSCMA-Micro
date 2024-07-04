#ifndef _MA_PORTING_H_
#define _MA_PORTING_H_

#include <porting/ma_misc.h>
#include <porting/ma_osal.h>

#include <porting/ma_camera.h>
#include <porting/ma_device.h>
#include <porting/ma_sensor.h>
#include <porting/ma_storage.h>
#include <porting/ma_transport.h>

#if __has_include(<ma_porting_extra.h>)
#include <ma_porting_extra.h>
#endif

#endif  // _MA_PORTING_H_