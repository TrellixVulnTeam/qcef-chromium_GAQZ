// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_CCODEC_GIFMODULE_H_
#define CORE_FXCODEC_CODEC_CCODEC_GIFMODULE_H_

#include <memory>

#include "core/fxcodec/lgif/fx_gif.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CFX_DIBAttribute;

class CCodec_GifModule {
 public:
  class Delegate {
   public:
    virtual void GifRecordCurrentPosition(uint32_t& cur_pos) = 0;
    virtual bool GifInputRecordPositionBuf(uint32_t rcd_pos,
                                           const FX_RECT& img_rc,
                                           int32_t pal_num,
                                           void* pal_ptr,
                                           int32_t delay_time,
                                           bool user_input,
                                           int32_t trans_index,
                                           int32_t disposal_method,
                                           bool interlace) = 0;
    virtual void GifReadScanline(int32_t row_num, uint8_t* row_buf) = 0;
  };

  CCodec_GifModule();
  ~CCodec_GifModule();

  std::unique_ptr<CGifContext> Start();
  uint32_t GetAvailInput(CGifContext* context,
                         uint8_t** avail_buf_ptr = nullptr);

  void Input(CGifContext* context, const uint8_t* src_buf, uint32_t src_size);

  GifDecodeStatus ReadHeader(CGifContext* context,
                             int* width,
                             int* height,
                             int* pal_num,
                             void** pal_pp,
                             int* bg_index,
                             CFX_DIBAttribute* pAttribute);

  GifDecodeStatus LoadFrameInfo(CGifContext* context, int* frame_num);
  GifDecodeStatus LoadFrame(CGifContext* context,
                            int frame_num,
                            CFX_DIBAttribute* pAttribute);

  Delegate* GetDelegate() const { return m_pDelegate; }
  void SetDelegate(Delegate* pDelegate) { m_pDelegate = pDelegate; }

 protected:
  Delegate* m_pDelegate;
  char m_szLastError[256];
};

#endif  // CORE_FXCODEC_CODEC_CCODEC_GIFMODULE_H_
