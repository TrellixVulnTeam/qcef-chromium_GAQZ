// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com
// Original code is licensed as follows:
/*
 * Copyright 2011 ZXing authors
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

#include "fxbarcode/oned/BC_OneDimWriter.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "core/fxge/cfx_defaultrenderdevice.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/cfx_unicodeencodingex.h"
#include "fxbarcode/BC_Writer.h"
#include "third_party/base/ptr_util.h"

CBC_OneDimWriter::CBC_OneDimWriter() {
  m_locTextLoc = BC_TEXT_LOC_BELOWEMBED;
  m_bPrintChecksum = true;
  m_iDataLenth = 0;
  m_bCalcChecksum = false;
  m_pFont = nullptr;
  m_fFontSize = 10;
  m_iFontStyle = 0;
  m_fontColor = 0xff000000;
  m_iContentLen = 0;
  m_bLeftPadding = false;
  m_bRightPadding = false;
}

CBC_OneDimWriter::~CBC_OneDimWriter() {}

void CBC_OneDimWriter::SetPrintChecksum(bool checksum) {
  m_bPrintChecksum = checksum;
}

void CBC_OneDimWriter::SetDataLength(int32_t length) {
  m_iDataLenth = length;
}

void CBC_OneDimWriter::SetCalcChecksum(bool state) {
  m_bCalcChecksum = state;
}

bool CBC_OneDimWriter::SetFont(CFX_Font* cFont) {
  if (!cFont)
    return false;

  m_pFont = cFont;
  return true;
}

void CBC_OneDimWriter::SetFontSize(float size) {
  m_fFontSize = size;
}

void CBC_OneDimWriter::SetFontStyle(int32_t style) {
  m_iFontStyle = style;
}

void CBC_OneDimWriter::SetFontColor(FX_ARGB color) {
  m_fontColor = color;
}

wchar_t CBC_OneDimWriter::Upper(wchar_t ch) {
  if (ch >= 'a' && ch <= 'z') {
    ch = ch - ('a' - 'A');
  }
  return ch;
}

uint8_t* CBC_OneDimWriter::EncodeWithHint(const CFX_ByteString& contents,
                                          BCFORMAT format,
                                          int32_t& outWidth,
                                          int32_t& outHeight,
                                          int32_t hints) {
  outHeight = 1;
  return EncodeImpl(contents, outWidth);
}

uint8_t* CBC_OneDimWriter::Encode(const CFX_ByteString& contents,
                                  BCFORMAT format,
                                  int32_t& outWidth,
                                  int32_t& outHeight) {
  return EncodeWithHint(contents, format, outWidth, outHeight, 0);
}

int32_t CBC_OneDimWriter::AppendPattern(uint8_t* target,
                                        int32_t pos,
                                        const int8_t* pattern,
                                        int32_t patternLength,
                                        int32_t startColor,
                                        int32_t& e) {
  if (startColor != 0 && startColor != 1) {
    e = BCExceptionValueMustBeEither0or1;
    return 0;
  }
  uint8_t color = (uint8_t)startColor;
  int32_t numAdded = 0;
  for (int32_t i = 0; i < patternLength; i++) {
    for (int32_t j = 0; j < pattern[i]; j++) {
      target[pos++] = color;
      numAdded += 1;
    }
    color ^= 1;
  }
  return numAdded;
}

void CBC_OneDimWriter::CalcTextInfo(const CFX_ByteString& text,
                                    FXTEXT_CHARPOS* charPos,
                                    CFX_Font* cFont,
                                    float geWidth,
                                    int32_t fontSize,
                                    float& charsLen) {
  std::unique_ptr<CFX_UnicodeEncodingEx> encoding =
      FX_CreateFontEncodingEx(cFont, FXFM_ENCODING_NONE);

  int32_t length = text.GetLength();
  uint32_t* pCharCode = FX_Alloc(uint32_t, text.GetLength());
  float charWidth = 0;
  for (int32_t j = 0; j < text.GetLength(); j++) {
    pCharCode[j] = encoding->CharCodeFromUnicode(text[j]);
    int32_t glyp_code = encoding->GlyphFromCharCode(pCharCode[j]);
    int32_t glyp_value = cFont->GetGlyphWidth(glyp_code);
    float temp = (float)((glyp_value)*fontSize / 1000.0);
    charWidth += temp;
  }
  charsLen = charWidth;
  float leftPositon = (float)(geWidth - charsLen) / 2.0f;
  if (leftPositon < 0 && geWidth == 0) {
    leftPositon = 0;
  }
  float penX = 0.0;
  float penY = (float)abs(cFont->GetDescent()) * (float)fontSize / 1000.0f;
  float left = leftPositon;
  float top = 0.0;
  charPos[0].m_Origin = CFX_PointF(penX + left, penY + top);
  charPos[0].m_GlyphIndex = encoding->GlyphFromCharCode(pCharCode[0]);
  charPos[0].m_FontCharWidth = cFont->GetGlyphWidth(charPos[0].m_GlyphIndex);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  charPos[0].m_ExtGID = charPos[0].m_GlyphIndex;
#endif
  penX += (float)(charPos[0].m_FontCharWidth) * (float)fontSize / 1000.0f;
  for (int32_t i = 1; i < length; i++) {
    charPos[i].m_Origin = CFX_PointF(penX + left, penY + top);
    charPos[i].m_GlyphIndex = encoding->GlyphFromCharCode(pCharCode[i]);
    charPos[i].m_FontCharWidth = cFont->GetGlyphWidth(charPos[i].m_GlyphIndex);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
    charPos[i].m_ExtGID = charPos[i].m_GlyphIndex;
#endif
    penX += (float)(charPos[i].m_FontCharWidth) * (float)fontSize / 1000.0f;
  }
  FX_Free(pCharCode);
}

void CBC_OneDimWriter::ShowDeviceChars(CFX_RenderDevice* device,
                                       const CFX_Matrix* matrix,
                                       const CFX_ByteString str,
                                       float geWidth,
                                       FXTEXT_CHARPOS* pCharPos,
                                       float locX,
                                       float locY,
                                       int32_t barWidth) {
  int32_t iFontSize = (int32_t)fabs(m_fFontSize);
  int32_t iTextHeight = iFontSize + 1;
  CFX_FloatRect rect((float)locX, (float)locY, (float)(locX + geWidth),
                     (float)(locY + iTextHeight));
  if (geWidth != m_Width) {
    rect.right -= 1;
  }
  matrix->TransformRect(rect);
  FX_RECT re = rect.GetOuterRect();
  device->FillRect(&re, m_backgroundColor);
  CFX_Matrix affine_matrix(1.0, 0.0, 0.0, -1.0, (float)locX,
                           (float)(locY + iFontSize));
  if (matrix) {
    affine_matrix.Concat(*matrix);
  }
  device->DrawNormalText(str.GetLength(), pCharPos, m_pFont,
                         static_cast<float>(iFontSize), &affine_matrix,
                         m_fontColor, FXTEXT_CLEARTYPE);
}

bool CBC_OneDimWriter::ShowChars(const CFX_WideStringC& contents,
                                 CFX_RenderDevice* device,
                                 const CFX_Matrix* matrix,
                                 int32_t barWidth,
                                 int32_t multiple) {
  if (!device || !m_pFont)
    return false;

  CFX_ByteString str = FX_UTF8Encode(contents);
  int32_t iLen = str.GetLength();
  std::vector<FXTEXT_CHARPOS> charpos(iLen);
  float charsLen = 0;
  float geWidth = 0;
  if (m_locTextLoc == BC_TEXT_LOC_ABOVEEMBED ||
      m_locTextLoc == BC_TEXT_LOC_BELOWEMBED) {
    geWidth = 0;
  } else if (m_locTextLoc == BC_TEXT_LOC_ABOVE ||
             m_locTextLoc == BC_TEXT_LOC_BELOW) {
    geWidth = (float)barWidth;
  }
  int32_t iFontSize = (int32_t)fabs(m_fFontSize);
  int32_t iTextHeight = iFontSize + 1;
  CalcTextInfo(str, charpos.data(), m_pFont, geWidth, iFontSize, charsLen);
  if (charsLen < 1)
    return true;

  int32_t locX = 0;
  int32_t locY = 0;
  switch (m_locTextLoc) {
    case BC_TEXT_LOC_ABOVEEMBED:
      locX = (int32_t)(barWidth - charsLen) / 2;
      locY = 0;
      geWidth = charsLen;
      break;
    case BC_TEXT_LOC_ABOVE:
      locX = 0;
      locY = 0;
      geWidth = (float)barWidth;
      break;
    case BC_TEXT_LOC_BELOWEMBED:
      locX = (int32_t)(barWidth - charsLen) / 2;
      locY = m_Height - iTextHeight;
      geWidth = charsLen;
      break;
    case BC_TEXT_LOC_BELOW:
    default:
      locX = 0;
      locY = m_Height - iTextHeight;
      geWidth = (float)barWidth;
      break;
  }
  ShowDeviceChars(device, matrix, str, geWidth, charpos.data(), (float)locX,
                  (float)locY, barWidth);
  return true;
}

bool CBC_OneDimWriter::RenderDeviceResult(CFX_RenderDevice* device,
                                          const CFX_Matrix* matrix,
                                          const CFX_WideStringC& contents) {
  if (m_output.empty())
    return false;

  CFX_GraphStateData stateData;
  CFX_PathData path;
  path.AppendRect(0, 0, static_cast<float>(m_Width),
                  static_cast<float>(m_Height));
  device->DrawPath(&path, matrix, &stateData, m_backgroundColor,
                   m_backgroundColor, FXFILL_ALTERNATE);
  CFX_Matrix scaledMatrix(m_outputHScale, 0.0, 0.0,
                          static_cast<float>(m_Height), 0.0, 0.0);
  scaledMatrix.Concat(*matrix);
  for (auto& rect : m_output) {
    CFX_GraphStateData data;
    device->DrawPath(&rect, &scaledMatrix, &data, m_barColor, 0,
                     FXFILL_WINDING);
  }

  return m_locTextLoc == BC_TEXT_LOC_NONE || contents.Find(' ') == -1 ||
         ShowChars(contents, device, matrix, m_barWidth, m_multiple);
}

bool CBC_OneDimWriter::RenderResult(const CFX_WideStringC& contents,
                                    uint8_t* code,
                                    int32_t codeLength,
                                    bool isDevice) {
  if (codeLength < 1)
    return false;

  m_ModuleHeight = std::max(m_ModuleHeight, 20);
  const int32_t codeOldLength = codeLength;
  const int32_t leftPadding = m_bLeftPadding ? 7 : 0;
  const int32_t rightPadding = m_bRightPadding ? 7 : 0;
  codeLength += leftPadding;
  codeLength += rightPadding;
  m_outputHScale =
      m_Width > 0 ? static_cast<float>(m_Width) / static_cast<float>(codeLength)
                  : 1.0;
  if (!isDevice) {
    m_outputHScale =
        std::max(m_outputHScale, static_cast<float>(m_ModuleWidth));
  }
  float dataLengthScale = 1.0;
  if (m_iDataLenth > 0 && contents.GetLength() != 0)
    dataLengthScale = float(contents.GetLength()) / float(m_iDataLenth);
  if (m_iDataLenth > 0 && contents.GetLength() == 0)
    dataLengthScale = float(1) / float(m_iDataLenth);
  m_multiple = 1;
  if (!isDevice) {
    m_multiple = (int32_t)ceil(m_outputHScale * dataLengthScale);
  }
  int32_t outputHeight = 1;
  if (!isDevice)
    outputHeight = m_Height ? m_Height : std::max(20, m_ModuleHeight);
  int32_t outputWidth = codeLength;
  if (!isDevice)
    outputWidth = (int32_t)(codeLength * m_multiple / dataLengthScale);
  m_barWidth = m_Width;
  if (!isDevice)
    m_barWidth = codeLength * m_multiple;

  m_output.clear();
  for (int32_t inputX = 0, outputX = leftPadding * m_multiple;
       inputX < codeOldLength; ++inputX, outputX += m_multiple) {
    if (code[inputX] != 1)
      continue;

    if (outputX >= outputWidth)
      return true;

    if (outputX + m_multiple > outputWidth && outputWidth - outputX > 0) {
      RenderVerticalBars(outputX, outputWidth - outputX, outputHeight);
      return true;
    }

    RenderVerticalBars(outputX, m_multiple, outputHeight);
  }
  return true;
}

void CBC_OneDimWriter::RenderVerticalBars(int32_t outputX,
                                          int32_t width,
                                          int32_t height) {
  for (int i = 0; i < width; ++i) {
    float x = outputX + i;
    CFX_PathData rect;
    rect.AppendRect(x, 0.0f, x + 1, static_cast<float>(height));
    m_output.push_back(rect);
  }
}

CFX_WideString CBC_OneDimWriter::RenderTextContents(
    const CFX_WideStringC& contents) {
  return CFX_WideString();
}
