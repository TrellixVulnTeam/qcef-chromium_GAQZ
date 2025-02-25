// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com
// Original code is licensed as follows:
/*
 * Copyright 2010 ZXing authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fxbarcode/oned/BC_OnedUPCAWriter.h"

#include <vector>

#include "core/fxge/cfx_defaultrenderdevice.h"
#include "fxbarcode/BC_Writer.h"
#include "fxbarcode/oned/BC_OneDimWriter.h"
#include "fxbarcode/oned/BC_OnedEAN13Writer.h"
#include "third_party/base/ptr_util.h"

CBC_OnedUPCAWriter::CBC_OnedUPCAWriter() {
  m_bLeftPadding = true;
  m_bRightPadding = true;
}

void CBC_OnedUPCAWriter::Init() {
  m_subWriter = pdfium::MakeUnique<CBC_OnedEAN13Writer>();
}

CBC_OnedUPCAWriter::~CBC_OnedUPCAWriter() {}

bool CBC_OnedUPCAWriter::CheckContentValidity(const CFX_WideStringC& contents) {
  for (FX_STRSIZE i = 0; i < contents.GetLength(); ++i) {
    if (contents.GetAt(i) < '0' || contents.GetAt(i) > '9')
      return false;
  }
  return true;
}

CFX_WideString CBC_OnedUPCAWriter::FilterContents(
    const CFX_WideStringC& contents) {
  CFX_WideString filtercontents;
  wchar_t ch;
  for (int32_t i = 0; i < contents.GetLength(); i++) {
    ch = contents.GetAt(i);
    if (ch > 175) {
      i++;
      continue;
    }
    if (ch >= '0' && ch <= '9') {
      filtercontents += ch;
    }
  }
  return filtercontents;
}

int32_t CBC_OnedUPCAWriter::CalcChecksum(const CFX_ByteString& contents) {
  int32_t odd = 0;
  int32_t even = 0;
  int32_t j = 1;
  for (int32_t i = contents.GetLength() - 1; i >= 0; i--) {
    if (j % 2) {
      odd += FXSYS_atoi(contents.Mid(i, 1).c_str());
    } else {
      even += FXSYS_atoi(contents.Mid(i, 1).c_str());
    }
    j++;
  }
  int32_t checksum = (odd * 3 + even) % 10;
  checksum = (10 - checksum) % 10;
  return (checksum);
}

uint8_t* CBC_OnedUPCAWriter::EncodeWithHint(const CFX_ByteString& contents,
                                            BCFORMAT format,
                                            int32_t& outWidth,
                                            int32_t& outHeight,
                                            int32_t hints) {
  if (format != BCFORMAT_UPC_A)
    return nullptr;

  CFX_ByteString toEAN13String = '0' + contents;
  m_iDataLenth = 13;
  return m_subWriter->EncodeWithHint(toEAN13String, BCFORMAT_EAN_13, outWidth,
                                     outHeight, hints);
}

uint8_t* CBC_OnedUPCAWriter::EncodeImpl(const CFX_ByteString& contents,
                                        int32_t& outLength) {
  return nullptr;
}

bool CBC_OnedUPCAWriter::ShowChars(const CFX_WideStringC& contents,
                                   CFX_RenderDevice* device,
                                   const CFX_Matrix* matrix,
                                   int32_t barWidth,
                                   int32_t multiple) {
  if (!device)
    return false;

  int32_t leftPadding = 7 * multiple;
  int32_t leftPosition = 10 * multiple + leftPadding;
  CFX_ByteString str = FX_UTF8Encode(contents);
  int32_t iLen = str.GetLength();
  std::vector<FXTEXT_CHARPOS> charpos(iLen);
  CFX_ByteString tempStr = str.Mid(1, 5);
  float strWidth = (float)35 * multiple;
  float blank = 0.0;

  iLen = tempStr.GetLength();
  int32_t iFontSize = (int32_t)fabs(m_fFontSize);
  int32_t iTextHeight = iFontSize + 1;

  CFX_Matrix matr(m_outputHScale, 0.0, 0.0, 1.0, 0.0, 0.0);
  CFX_FloatRect rect((float)leftPosition, (float)(m_Height - iTextHeight),
                     (float)(leftPosition + strWidth - 0.5), (float)m_Height);
  matr.Concat(*matrix);
  matr.TransformRect(rect);
  FX_RECT re = rect.GetOuterRect();
  device->FillRect(&re, m_backgroundColor);
  CFX_Matrix matr1(m_outputHScale, 0.0, 0.0, 1.0, 0.0, 0.0);
  CFX_FloatRect rect1((float)(leftPosition + 40 * multiple),
                      (float)(m_Height - iTextHeight),
                      (float)((leftPosition + 40 * multiple) + strWidth - 0.5),
                      (float)m_Height);
  matr1.Concat(*matrix);
  matr1.TransformRect(rect1);
  re = rect1.GetOuterRect();
  device->FillRect(&re, m_backgroundColor);
  float strWidth1 = (float)multiple * 7;
  CFX_Matrix matr2(m_outputHScale, 0.0, 0.0, 1.0, 0.0, 0.0);
  CFX_FloatRect rect2(0.0, (float)(m_Height - iTextHeight),
                      (float)strWidth1 - 1, (float)m_Height);
  matr2.Concat(*matrix);
  matr2.TransformRect(rect2);
  re = rect2.GetOuterRect();
  device->FillRect(&re, m_backgroundColor);
  CFX_Matrix matr3(m_outputHScale, 0.0, 0.0, 1.0, 0.0, 0.0);
  CFX_FloatRect rect3((float)(leftPosition + 85 * multiple),
                      (float)(m_Height - iTextHeight),
                      (float)((leftPosition + 85 * multiple) + strWidth1 - 0.5),
                      (float)m_Height);
  matr3.Concat(*matrix);
  matr3.TransformRect(rect3);
  re = rect3.GetOuterRect();
  device->FillRect(&re, m_backgroundColor);
  strWidth = strWidth * m_outputHScale;

  CalcTextInfo(tempStr, &charpos[1], m_pFont, strWidth, iFontSize, blank);
  {
    CFX_Matrix affine_matrix1(1.0, 0.0, 0.0, -1.0,
                              (float)leftPosition * m_outputHScale,
                              (float)(m_Height - iTextHeight + iFontSize));
    if (matrix)
      affine_matrix1.Concat(*matrix);
    device->DrawNormalText(iLen, &charpos[1], m_pFont,
                           static_cast<float>(iFontSize), &affine_matrix1,
                           m_fontColor, FXTEXT_CLEARTYPE);
  }
  tempStr = str.Mid(6, 5);
  iLen = tempStr.GetLength();
  CalcTextInfo(tempStr, &charpos[6], m_pFont, strWidth, iFontSize, blank);
  {
    CFX_Matrix affine_matrix1(
        1.0, 0.0, 0.0, -1.0,
        (float)(leftPosition + 40 * multiple) * m_outputHScale,
        (float)(m_Height - iTextHeight + iFontSize));
    if (matrix)
      affine_matrix1.Concat(*matrix);
    device->DrawNormalText(iLen, &charpos[6], m_pFont,
                           static_cast<float>(iFontSize), &affine_matrix1,
                           m_fontColor, FXTEXT_CLEARTYPE);
  }
  tempStr = str.Mid(0, 1);
  iLen = tempStr.GetLength();
  strWidth = (float)multiple * 7;
  strWidth = strWidth * m_outputHScale;

  CalcTextInfo(tempStr, charpos.data(), m_pFont, strWidth, iFontSize, blank);
  {
    CFX_Matrix affine_matrix1(1.0, 0.0, 0.0, -1.0, 0,
                              (float)(m_Height - iTextHeight + iFontSize));
    if (matrix)
      affine_matrix1.Concat(*matrix);
    device->DrawNormalText(iLen, charpos.data(), m_pFont,
                           static_cast<float>(iFontSize), &affine_matrix1,
                           m_fontColor, FXTEXT_CLEARTYPE);
  }
  tempStr = str.Mid(11, 1);
  iLen = tempStr.GetLength();
  CalcTextInfo(tempStr, &charpos[11], m_pFont, strWidth, iFontSize, blank);
  {
    CFX_Matrix affine_matrix1(
        1.0, 0.0, 0.0, -1.0,
        (float)(leftPosition + 85 * multiple) * m_outputHScale,
        (float)(m_Height - iTextHeight + iFontSize));
    if (matrix)
      affine_matrix1.Concat(*matrix);
    device->DrawNormalText(iLen, &charpos[11], m_pFont,
                           static_cast<float>(iFontSize), &affine_matrix1,
                           m_fontColor, FXTEXT_CLEARTYPE);
  }
  return true;
}
