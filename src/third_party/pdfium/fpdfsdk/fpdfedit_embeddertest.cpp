// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/fx_system.h"
#include "fpdfsdk/fsdk_define.h"
#include "public/cpp/fpdf_deleters.h"
#include "public/fpdf_edit.h"
#include "public/fpdfview.h"
#include "testing/embedder_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"

class FPDFEditEmbeddertest : public EmbedderTest, public TestSaver {
 protected:
  FPDF_DOCUMENT CreateNewDocument() {
    document_ = FPDF_CreateNewDocument();
    cpdf_doc_ = CPDFDocumentFromFPDFDocument(document_);
    return document_;
  }

  void CheckFontDescriptor(CPDF_Dictionary* font_dict,
                           int font_type,
                           bool bold,
                           bool italic,
                           uint32_t size,
                           const uint8_t* data) {
    CPDF_Dictionary* font_desc = font_dict->GetDictFor("FontDescriptor");
    ASSERT_TRUE(font_desc);
    EXPECT_EQ("FontDescriptor", font_desc->GetStringFor("Type"));
    EXPECT_EQ(font_dict->GetStringFor("BaseFont"),
              font_desc->GetStringFor("FontName"));

    // Check that the font descriptor has the required keys according to spec
    // 1.7 Table 5.19
    ASSERT_TRUE(font_desc->KeyExist("Flags"));
    int font_flags = font_desc->GetIntegerFor("Flags");
    if (bold)
      EXPECT_TRUE(font_flags & FXFONT_BOLD);
    else
      EXPECT_FALSE(font_flags & FXFONT_BOLD);
    if (italic)
      EXPECT_TRUE(font_flags & FXFONT_ITALIC);
    else
      EXPECT_FALSE(font_flags & FXFONT_ITALIC);
    EXPECT_TRUE(font_flags & FXFONT_NONSYMBOLIC);
    ASSERT_TRUE(font_desc->KeyExist("FontBBox"));
    EXPECT_EQ(4U, font_desc->GetArrayFor("FontBBox")->GetCount());
    EXPECT_TRUE(font_desc->KeyExist("ItalicAngle"));
    EXPECT_TRUE(font_desc->KeyExist("Ascent"));
    EXPECT_TRUE(font_desc->KeyExist("Descent"));
    EXPECT_TRUE(font_desc->KeyExist("CapHeight"));
    EXPECT_TRUE(font_desc->KeyExist("StemV"));
    CFX_ByteString present("FontFile");
    CFX_ByteString absent("FontFile2");
    if (font_type == FPDF_FONT_TRUETYPE)
      std::swap(present, absent);
    EXPECT_TRUE(font_desc->KeyExist(present));
    EXPECT_FALSE(font_desc->KeyExist(absent));

    // Check that the font stream is the one that was provided
    CPDF_Stream* font_stream = font_desc->GetStreamFor(present);
    ASSERT_EQ(size, font_stream->GetRawSize());
    uint8_t* stream_data = font_stream->GetRawData();
    for (size_t j = 0; j < size; j++)
      EXPECT_EQ(data[j], stream_data[j]) << " at byte " << j;
  }

  void CheckCompositeFontWidths(CPDF_Array* widths_array,
                                CPDF_Font* typed_font) {
    // Check that W array is in a format that conforms to PDF spec 1.7 section
    // "Glyph Metrics in CIDFonts" (these checks are not
    // implementation-specific).
    EXPECT_GT(widths_array->GetCount(), 1U);
    int num_cids_checked = 0;
    int cur_cid = 0;
    for (size_t idx = 0; idx < widths_array->GetCount(); idx++) {
      int cid = widths_array->GetNumberAt(idx);
      EXPECT_GE(cid, cur_cid);
      ASSERT_FALSE(++idx == widths_array->GetCount());
      CPDF_Object* next = widths_array->GetObjectAt(idx);
      if (next->IsArray()) {
        // We are in the c [w1 w2 ...] case
        CPDF_Array* arr = next->AsArray();
        int cnt = static_cast<int>(arr->GetCount());
        size_t inner_idx = 0;
        for (cur_cid = cid; cur_cid < cid + cnt; cur_cid++) {
          int width = arr->GetNumberAt(inner_idx++);
          EXPECT_EQ(width, typed_font->GetCharWidthF(cur_cid)) << " at cid "
                                                               << cur_cid;
        }
        num_cids_checked += cnt;
        continue;
      }
      // Otherwise, are in the c_first c_last w case.
      ASSERT_TRUE(next->IsNumber());
      int last_cid = next->AsNumber()->GetInteger();
      ASSERT_FALSE(++idx == widths_array->GetCount());
      int width = widths_array->GetNumberAt(idx);
      for (cur_cid = cid; cur_cid <= last_cid; cur_cid++) {
        EXPECT_EQ(width, typed_font->GetCharWidthF(cur_cid)) << " at cid "
                                                             << cur_cid;
      }
      num_cids_checked += last_cid - cid + 1;
    }
    // Make sure we have a good amount of cids described
    EXPECT_GT(num_cids_checked, 900);
  }
  CPDF_Document* cpdf_doc() { return cpdf_doc_; }

 private:
  CPDF_Document* cpdf_doc_;
};

namespace {

const char kExpectedPDF[] =
    "%PDF-1.7\r\n"
    "%\xA1\xB3\xC5\xD7\r\n"
    "1 0 obj\r\n"
    "<</Pages 2 0 R /Type/Catalog>>\r\n"
    "endobj\r\n"
    "2 0 obj\r\n"
    "<</Count 1/Kids\\[ 4 0 R \\]/Type/Pages>>\r\n"
    "endobj\r\n"
    "3 0 obj\r\n"
    "<</CreationDate\\(D:.*\\)/Creator\\(PDFium\\)>>\r\n"
    "endobj\r\n"
    "4 0 obj\r\n"
    "<</Contents 5 0 R /MediaBox\\[ 0 0 640 480\\]"
    "/Parent 2 0 R /Resources<<>>/Rotate 0/Type/Page"
    ">>\r\n"
    "endobj\r\n"
    "5 0 obj\r\n"
    "<</Filter/FlateDecode/Length 8>>stream\r\n"
    // Character '_' is matching '\0' (see comment below).
    "x\x9C\x3____\x1\r\n"
    "endstream\r\n"
    "endobj\r\n"
    "xref\r\n"
    "0 6\r\n"
    "0000000000 65535 f\r\n"
    "0000000017 00000 n\r\n"
    "0000000066 00000 n\r\n"
    "0000000122 00000 n\r\n"
    "0000000192 00000 n\r\n"
    "0000000301 00000 n\r\n"
    "trailer\r\n"
    "<<\r\n"
    "/Root 1 0 R\r\n"
    "/Info 3 0 R\r\n"
    "/Size 6/ID\\[<.*><.*>\\]>>\r\n"
    "startxref\r\n"
    "379\r\n"
    "%%EOF\r\n";

}  // namespace

TEST_F(FPDFEditEmbeddertest, EmptyCreation) {
  EXPECT_TRUE(CreateEmptyDocument());
  FPDF_PAGE page = FPDFPage_New(document(), 0, 640.0, 480.0);
  EXPECT_NE(nullptr, page);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  // The MatchesRegexp doesn't support embedded NUL ('\0') characters. They are
  // replaced by '_' for the purpose of the test.
  std::string result = GetString();
  std::replace(result.begin(), result.end(), '\0', '_');
  EXPECT_THAT(result, testing::MatchesRegex(
                          std::string(kExpectedPDF, sizeof(kExpectedPDF))));
  FPDF_ClosePage(page);
}

// Regression test for https://crbug.com/667012
TEST_F(FPDFEditEmbeddertest, RasterizePDF) {
  const char kAllBlackMd5sum[] = "5708fc5c4a8bd0abde99c8e8f0390615";

  // Get the bitmap for the original document/
  FPDF_BITMAP orig_bitmap;
  {
    EXPECT_TRUE(OpenDocument("black.pdf"));
    FPDF_PAGE orig_page = LoadPage(0);
    EXPECT_NE(nullptr, orig_page);
    orig_bitmap = RenderPage(orig_page);
    CompareBitmap(orig_bitmap, 612, 792, kAllBlackMd5sum);
    UnloadPage(orig_page);
  }

  // Create a new document from |orig_bitmap| and save it.
  {
    FPDF_DOCUMENT temp_doc = FPDF_CreateNewDocument();
    FPDF_PAGE temp_page = FPDFPage_New(temp_doc, 0, 612, 792);

    // Add the bitmap to an image object and add the image object to the output
    // page.
    FPDF_PAGEOBJECT temp_img = FPDFPageObj_NewImageObj(temp_doc);
    EXPECT_TRUE(FPDFImageObj_SetBitmap(&temp_page, 1, temp_img, orig_bitmap));
    EXPECT_TRUE(FPDFImageObj_SetMatrix(temp_img, 612, 0, 0, 792, 0, 0));
    FPDFPage_InsertObject(temp_page, temp_img);
    EXPECT_TRUE(FPDFPage_GenerateContent(temp_page));
    EXPECT_TRUE(FPDF_SaveAsCopy(temp_doc, this, 0));
    FPDF_ClosePage(temp_page);
    FPDF_CloseDocument(temp_doc);
  }
  FPDFBitmap_Destroy(orig_bitmap);

  // Get the generated content. Make sure it is at least as big as the original
  // PDF.
  std::string new_file = GetString();
  EXPECT_GT(new_file.size(), 923U);

  // Read |new_file| in, and verify its rendered bitmap.
  {
    FPDF_FILEACCESS file_access;
    memset(&file_access, 0, sizeof(file_access));
    file_access.m_FileLen = new_file.size();
    file_access.m_GetBlock = GetBlockFromString;
    file_access.m_Param = &new_file;

    FPDF_DOCUMENT new_doc = FPDF_LoadCustomDocument(&file_access, nullptr);
    EXPECT_EQ(1, FPDF_GetPageCount(document_));
    FPDF_PAGE new_page = FPDF_LoadPage(new_doc, 0);
    EXPECT_NE(nullptr, new_page);
    FPDF_BITMAP new_bitmap = RenderPage(new_page);
    CompareBitmap(new_bitmap, 612, 792, kAllBlackMd5sum);
    FPDF_ClosePage(new_page);
    FPDF_CloseDocument(new_doc);
    FPDFBitmap_Destroy(new_bitmap);
  }
}

TEST_F(FPDFEditEmbeddertest, AddPaths) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);

  // We will first add a red rectangle
  FPDF_PAGEOBJECT red_rect = FPDFPageObj_CreateNewRect(10, 10, 20, 20);
  ASSERT_NE(nullptr, red_rect);
  // Expect false when trying to set colors out of range
  EXPECT_FALSE(FPDFPath_SetStrokeColor(red_rect, 100, 100, 100, 300));
  EXPECT_FALSE(FPDFPath_SetFillColor(red_rect, 200, 256, 200, 0));

  // Fill rectangle with red and insert to the page
  EXPECT_TRUE(FPDFPath_SetFillColor(red_rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(red_rect, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(page, red_rect);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  FPDF_BITMAP page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "66d02eaa6181e2c069ce2ea99beda497");
  FPDFBitmap_Destroy(page_bitmap);

  // Now add to that a green rectangle with some medium alpha
  FPDF_PAGEOBJECT green_rect = FPDFPageObj_CreateNewRect(100, 100, 40, 40);
  EXPECT_TRUE(FPDFPath_SetFillColor(green_rect, 0, 255, 0, 128));

  // Make sure the type of the rectangle is a path.
  EXPECT_EQ(FPDF_PAGEOBJ_PATH, FPDFPageObj_GetType(green_rect));

  // Make sure we get back the same color we set previously.
  unsigned int R;
  unsigned int G;
  unsigned int B;
  unsigned int A;
  EXPECT_TRUE(FPDFPath_GetFillColor(green_rect, &R, &G, &B, &A));
  EXPECT_EQ(0U, R);
  EXPECT_EQ(255U, G);
  EXPECT_EQ(0U, B);
  EXPECT_EQ(128U, A);

  EXPECT_TRUE(FPDFPath_SetDrawMode(green_rect, FPDF_FILLMODE_WINDING, 0));
  FPDFPage_InsertObject(page, green_rect);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "7b0b87604594e773add528fae567a558");
  FPDFBitmap_Destroy(page_bitmap);

  // Add a black triangle.
  FPDF_PAGEOBJECT black_path = FPDFPageObj_CreateNewPath(400, 100);
  EXPECT_TRUE(FPDFPath_SetFillColor(black_path, 0, 0, 0, 200));
  EXPECT_TRUE(FPDFPath_SetDrawMode(black_path, FPDF_FILLMODE_ALTERNATE, 0));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 400, 200));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 300, 100));
  EXPECT_TRUE(FPDFPath_Close(black_path));
  FPDFPage_InsertObject(page, black_path);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "eadc8020a14dfcf091da2688733d8806");
  FPDFBitmap_Destroy(page_bitmap);

  // Now add a more complex blue path.
  FPDF_PAGEOBJECT blue_path = FPDFPageObj_CreateNewPath(200, 200);
  EXPECT_TRUE(FPDFPath_SetFillColor(blue_path, 0, 0, 255, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(blue_path, FPDF_FILLMODE_WINDING, 0));
  EXPECT_TRUE(FPDFPath_LineTo(blue_path, 230, 230));
  EXPECT_TRUE(FPDFPath_BezierTo(blue_path, 250, 250, 280, 280, 300, 300));
  EXPECT_TRUE(FPDFPath_LineTo(blue_path, 325, 325));
  EXPECT_TRUE(FPDFPath_LineTo(blue_path, 350, 325));
  EXPECT_TRUE(FPDFPath_BezierTo(blue_path, 375, 330, 390, 360, 400, 400));
  EXPECT_TRUE(FPDFPath_Close(blue_path));
  FPDFPage_InsertObject(page, blue_path);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "9823e1a21bd9b72b6a442ba4f12af946");
  FPDFBitmap_Destroy(page_bitmap);

  // Now save the result, closing the page and document
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  FPDF_ClosePage(page);
  std::string new_file = GetString();

  // Render the saved result
  FPDF_FILEACCESS file_access;
  memset(&file_access, 0, sizeof(file_access));
  file_access.m_FileLen = new_file.size();
  file_access.m_GetBlock = GetBlockFromString;
  file_access.m_Param = &new_file;
  FPDF_DOCUMENT new_doc = FPDF_LoadCustomDocument(&file_access, nullptr);
  ASSERT_NE(nullptr, new_doc);
  EXPECT_EQ(1, FPDF_GetPageCount(new_doc));
  FPDF_PAGE new_page = FPDF_LoadPage(new_doc, 0);
  ASSERT_NE(nullptr, new_page);
  FPDF_BITMAP new_bitmap = RenderPage(new_page);
  CompareBitmap(new_bitmap, 612, 792, "9823e1a21bd9b72b6a442ba4f12af946");
  FPDFBitmap_Destroy(new_bitmap);
  FPDF_ClosePage(new_page);
  FPDF_CloseDocument(new_doc);
}

TEST_F(FPDFEditEmbeddertest, PathOnTopOfText) {
  // Load document with some text
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_NE(nullptr, page);

  // Add an opaque rectangle on top of some of the text.
  FPDF_PAGEOBJECT red_rect = FPDFPageObj_CreateNewRect(20, 100, 50, 50);
  EXPECT_TRUE(FPDFPath_SetFillColor(red_rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(red_rect, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(page, red_rect);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  // Add a transparent triangle on top of other part of the text.
  FPDF_PAGEOBJECT black_path = FPDFPageObj_CreateNewPath(20, 50);
  EXPECT_TRUE(FPDFPath_SetFillColor(black_path, 0, 0, 0, 100));
  EXPECT_TRUE(FPDFPath_SetDrawMode(black_path, FPDF_FILLMODE_ALTERNATE, 0));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 30, 80));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 40, 10));
  EXPECT_TRUE(FPDFPath_Close(black_path));
  FPDFPage_InsertObject(page, black_path);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  // Render and check the result. Text is slightly different on Mac.
  FPDF_BITMAP bitmap = RenderPage(page);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  const char md5[] = "f9e6fa74230f234286bfcada9f7606d8";
#else
  const char md5[] = "2fdfc5dda29374cfba4349362e38ebdb";
#endif
  CompareBitmap(bitmap, 200, 200, md5);
  FPDFBitmap_Destroy(bitmap);
  UnloadPage(page);
}

TEST_F(FPDFEditEmbeddertest, AddStrokedPaths) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);

  // Add a large stroked rectangle (fill color should not affect it).
  FPDF_PAGEOBJECT rect = FPDFPageObj_CreateNewRect(20, 20, 200, 400);
  EXPECT_TRUE(FPDFPath_SetFillColor(rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetStrokeColor(rect, 0, 255, 0, 255));
  EXPECT_TRUE(FPDFPath_SetStrokeWidth(rect, 15.0f));
  EXPECT_TRUE(FPDFPath_SetDrawMode(rect, 0, 1));
  FPDFPage_InsertObject(page, rect);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  FPDF_BITMAP page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "64bd31f862a89e0a9e505a5af6efd506");
  FPDFBitmap_Destroy(page_bitmap);

  // Add crossed-checkmark
  FPDF_PAGEOBJECT check = FPDFPageObj_CreateNewPath(300, 500);
  EXPECT_TRUE(FPDFPath_LineTo(check, 400, 400));
  EXPECT_TRUE(FPDFPath_LineTo(check, 600, 600));
  EXPECT_TRUE(FPDFPath_MoveTo(check, 400, 600));
  EXPECT_TRUE(FPDFPath_LineTo(check, 600, 400));
  EXPECT_TRUE(FPDFPath_SetStrokeColor(check, 128, 128, 128, 180));
  EXPECT_TRUE(FPDFPath_SetStrokeWidth(check, 8.35f));
  EXPECT_TRUE(FPDFPath_SetDrawMode(check, 0, 1));
  FPDFPage_InsertObject(page, check);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "4b6f3b9d25c4e194821217d5016c3724");
  FPDFBitmap_Destroy(page_bitmap);

  // Add stroked and filled oval-ish path.
  FPDF_PAGEOBJECT path = FPDFPageObj_CreateNewPath(250, 100);
  EXPECT_TRUE(FPDFPath_BezierTo(path, 180, 166, 180, 233, 250, 300));
  EXPECT_TRUE(FPDFPath_LineTo(path, 255, 305));
  EXPECT_TRUE(FPDFPath_BezierTo(path, 325, 233, 325, 166, 255, 105));
  EXPECT_TRUE(FPDFPath_Close(path));
  EXPECT_TRUE(FPDFPath_SetFillColor(path, 200, 128, 128, 100));
  EXPECT_TRUE(FPDFPath_SetStrokeColor(path, 128, 200, 128, 150));
  EXPECT_TRUE(FPDFPath_SetStrokeWidth(path, 10.5f));
  EXPECT_TRUE(FPDFPath_SetDrawMode(path, FPDF_FILLMODE_ALTERNATE, 1));
  FPDFPage_InsertObject(page, path);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "ff3e6a22326754944cc6e56609acd73b");
  FPDFBitmap_Destroy(page_bitmap);
  FPDF_ClosePage(page);
}

TEST_F(FPDFEditEmbeddertest, AddStandardFontText) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);

  // Add some text to the page
  FPDF_PAGEOBJECT text_object1 =
      FPDFPageObj_NewTextObj(document(), "Arial", 12.0f);
  EXPECT_TRUE(text_object1);
  std::unique_ptr<unsigned short, pdfium::FreeDeleter> text1 =
      GetFPDFWideString(L"I'm at the bottom of the page");
  EXPECT_TRUE(FPDFText_SetText(text_object1, text1.get()));
  FPDFPageObj_Transform(text_object1, 1, 0, 0, 1, 20, 20);
  FPDFPage_InsertObject(page, text_object1);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  FPDF_BITMAP page_bitmap = RenderPage(page);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  const char md5[] = "a4dddc1a3930fa694bbff9789dab4161";
#else
  const char md5[] = "6e8a9b0682f60fd3ff1bf087b093d30d";
#endif
  CompareBitmap(page_bitmap, 612, 792, md5);
  FPDFBitmap_Destroy(page_bitmap);

  // Try another font
  FPDF_PAGEOBJECT text_object2 =
      FPDFPageObj_NewTextObj(document(), "TimesNewRomanBold", 15.0f);
  EXPECT_TRUE(text_object2);
  std::unique_ptr<unsigned short, pdfium::FreeDeleter> text2 =
      GetFPDFWideString(L"Hi, I'm Bold. Times New Roman Bold.");
  EXPECT_TRUE(FPDFText_SetText(text_object2, text2.get()));
  FPDFPageObj_Transform(text_object2, 1, 0, 0, 1, 100, 600);
  FPDFPage_InsertObject(page, text_object2);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  const char md5_2[] = "a5c4ace4c6f27644094813fe1441a21c";
#elif _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  const char md5_2[] = "56863642d4d8b418cfd810fdb5a5d92f";
#else
  const char md5_2[] = "0c83875429688bda45a55a692d5aa781";
#endif
  CompareBitmap(page_bitmap, 612, 792, md5_2);
  FPDFBitmap_Destroy(page_bitmap);

  // And some randomly transformed text
  FPDF_PAGEOBJECT text_object3 =
      FPDFPageObj_NewTextObj(document(), "Courier-Bold", 20.0f);
  EXPECT_TRUE(text_object3);
  std::unique_ptr<unsigned short, pdfium::FreeDeleter> text3 =
      GetFPDFWideString(L"Can you read me? <:)>");
  EXPECT_TRUE(FPDFText_SetText(text_object3, text3.get()));
  FPDFPageObj_Transform(text_object3, 1, 1.5, 2, 0.5, 200, 200);
  FPDFPage_InsertObject(page, text_object3);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  const char md5_3[] = "40b3ef04f915ff4c4208948001763544";
#elif _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  const char md5_3[] = "d69d419def35a098d9c10f8317d6709b";
#else
  const char md5_3[] = "8b300d3c6dfc12fa9af97e12ed5bd80a";
#endif
  CompareBitmap(page_bitmap, 612, 792, md5_3);
  FPDFBitmap_Destroy(page_bitmap);

  // TODO(npm): Why are there issues with text rotated by 90 degrees?
  // TODO(npm): FPDF_SaveAsCopy not giving the desired result after this.
  FPDF_ClosePage(page);
}

TEST_F(FPDFEditEmbeddertest, DoubleGenerating) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);

  // Add a red rectangle with some non-default alpha
  FPDF_PAGEOBJECT rect = FPDFPageObj_CreateNewRect(10, 10, 100, 100);
  EXPECT_TRUE(FPDFPath_SetFillColor(rect, 255, 0, 0, 128));
  EXPECT_TRUE(FPDFPath_SetDrawMode(rect, FPDF_FILLMODE_WINDING, 0));
  FPDFPage_InsertObject(page, rect);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  // Check the ExtGState
  CPDF_Page* the_page = CPDFPageFromFPDFPage(page);
  CPDF_Dictionary* graphics_dict =
      the_page->m_pResources->GetDictFor("ExtGState");
  ASSERT_TRUE(graphics_dict);
  EXPECT_EQ(1, static_cast<int>(graphics_dict->GetCount()));

  // Check the bitmap
  FPDF_BITMAP page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "5384da3406d62360ffb5cac4476fff1c");
  FPDFBitmap_Destroy(page_bitmap);

  // Never mind, my new favorite color is blue, increase alpha
  EXPECT_TRUE(FPDFPath_SetFillColor(rect, 0, 0, 255, 180));
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_EQ(2, static_cast<int>(graphics_dict->GetCount()));

  // Check that bitmap displays changed content
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "2e51656f5073b0bee611d9cd086aa09c");
  FPDFBitmap_Destroy(page_bitmap);

  // And now generate, without changes
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_EQ(2, static_cast<int>(graphics_dict->GetCount()));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "2e51656f5073b0bee611d9cd086aa09c");
  FPDFBitmap_Destroy(page_bitmap);

  // Add some text to the page
  FPDF_PAGEOBJECT text_object =
      FPDFPageObj_NewTextObj(document(), "Arial", 12.0f);
  std::unique_ptr<unsigned short, pdfium::FreeDeleter> text =
      GetFPDFWideString(L"Something something #text# something");
  EXPECT_TRUE(FPDFText_SetText(text_object, text.get()));
  FPDFPageObj_Transform(text_object, 1, 0, 0, 1, 300, 300);
  FPDFPage_InsertObject(page, text_object);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  CPDF_Dictionary* font_dict = the_page->m_pResources->GetDictFor("Font");
  ASSERT_TRUE(font_dict);
  EXPECT_EQ(1, static_cast<int>(font_dict->GetCount()));

  // Generate yet again, check dicts are reasonably sized
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_EQ(2, static_cast<int>(graphics_dict->GetCount()));
  EXPECT_EQ(1, static_cast<int>(font_dict->GetCount()));
  FPDF_ClosePage(page);
}

TEST_F(FPDFEditEmbeddertest, LoadSimpleType1Font) {
  CreateNewDocument();
  // TODO(npm): use other fonts after disallowing loading any font as any type
  const CPDF_Font* stock_font =
      CPDF_Font::GetStockFont(cpdf_doc(), "Times-Bold");
  const uint8_t* data = stock_font->GetFont()->GetFontData();
  const uint32_t size = stock_font->GetFont()->GetSize();
  std::unique_ptr<void, FPDFFontDeleter> font(
      FPDFText_LoadFont(document(), data, size, FPDF_FONT_TYPE1, false));
  ASSERT_TRUE(font.get());
  CPDF_Font* typed_font = static_cast<CPDF_Font*>(font.get());
  EXPECT_TRUE(typed_font->IsType1Font());

  CPDF_Dictionary* font_dict = typed_font->GetFontDict();
  EXPECT_EQ("Font", font_dict->GetStringFor("Type"));
  EXPECT_EQ("Type1", font_dict->GetStringFor("Subtype"));
  EXPECT_EQ("Times New Roman Bold", font_dict->GetStringFor("BaseFont"));
  ASSERT_TRUE(font_dict->KeyExist("FirstChar"));
  ASSERT_TRUE(font_dict->KeyExist("LastChar"));
  EXPECT_EQ(32, font_dict->GetIntegerFor("FirstChar"));
  EXPECT_EQ(255, font_dict->GetIntegerFor("LastChar"));

  CPDF_Array* widths_array = font_dict->GetArrayFor("Widths");
  ASSERT_TRUE(widths_array);
  ASSERT_EQ(224U, widths_array->GetCount());
  EXPECT_EQ(250, widths_array->GetNumberAt(0));
  EXPECT_EQ(569, widths_array->GetNumberAt(11));
  EXPECT_EQ(500, widths_array->GetNumberAt(223));
  CheckFontDescriptor(font_dict, FPDF_FONT_TYPE1, true, false, size, data);
}

TEST_F(FPDFEditEmbeddertest, LoadSimpleTrueTypeFont) {
  CreateNewDocument();
  const CPDF_Font* stock_font = CPDF_Font::GetStockFont(cpdf_doc(), "Courier");
  const uint8_t* data = stock_font->GetFont()->GetFontData();
  const uint32_t size = stock_font->GetFont()->GetSize();
  std::unique_ptr<void, FPDFFontDeleter> font(
      FPDFText_LoadFont(document(), data, size, FPDF_FONT_TRUETYPE, false));
  ASSERT_TRUE(font.get());
  CPDF_Font* typed_font = static_cast<CPDF_Font*>(font.get());
  EXPECT_TRUE(typed_font->IsTrueTypeFont());

  CPDF_Dictionary* font_dict = typed_font->GetFontDict();
  EXPECT_EQ("Font", font_dict->GetStringFor("Type"));
  EXPECT_EQ("TrueType", font_dict->GetStringFor("Subtype"));
  EXPECT_EQ("Courier New", font_dict->GetStringFor("BaseFont"));
  ASSERT_TRUE(font_dict->KeyExist("FirstChar"));
  ASSERT_TRUE(font_dict->KeyExist("LastChar"));
  EXPECT_EQ(32, font_dict->GetIntegerFor("FirstChar"));
  EXPECT_EQ(255, font_dict->GetIntegerFor("LastChar"));

  CPDF_Array* widths_array = font_dict->GetArrayFor("Widths");
  ASSERT_TRUE(widths_array);
  ASSERT_EQ(224U, widths_array->GetCount());
  EXPECT_EQ(600, widths_array->GetNumberAt(33));
  EXPECT_EQ(600, widths_array->GetNumberAt(74));
  EXPECT_EQ(600, widths_array->GetNumberAt(223));
  CheckFontDescriptor(font_dict, FPDF_FONT_TRUETYPE, false, false, size, data);
}

TEST_F(FPDFEditEmbeddertest, LoadCIDType0Font) {
  CreateNewDocument();
  const CPDF_Font* stock_font =
      CPDF_Font::GetStockFont(cpdf_doc(), "Times-Roman");
  const uint8_t* data = stock_font->GetFont()->GetFontData();
  const uint32_t size = stock_font->GetFont()->GetSize();
  std::unique_ptr<void, FPDFFontDeleter> font(
      FPDFText_LoadFont(document(), data, size, FPDF_FONT_TYPE1, 1));
  ASSERT_TRUE(font.get());
  CPDF_Font* typed_font = static_cast<CPDF_Font*>(font.get());
  EXPECT_TRUE(typed_font->IsCIDFont());

  // Check font dictionary entries
  CPDF_Dictionary* font_dict = typed_font->GetFontDict();
  EXPECT_EQ("Font", font_dict->GetStringFor("Type"));
  EXPECT_EQ("Type0", font_dict->GetStringFor("Subtype"));
  EXPECT_EQ("Times New Roman-Identity-H", font_dict->GetStringFor("BaseFont"));
  EXPECT_EQ("Identity-H", font_dict->GetStringFor("Encoding"));
  CPDF_Array* descendant_array = font_dict->GetArrayFor("DescendantFonts");
  ASSERT_TRUE(descendant_array);
  EXPECT_EQ(1U, descendant_array->GetCount());

  // Check the CIDFontDict
  CPDF_Dictionary* cidfont_dict = descendant_array->GetDictAt(0);
  EXPECT_EQ("Font", cidfont_dict->GetStringFor("Type"));
  EXPECT_EQ("CIDFontType0", cidfont_dict->GetStringFor("Subtype"));
  EXPECT_EQ("Times New Roman", cidfont_dict->GetStringFor("BaseFont"));
  CPDF_Dictionary* cidinfo_dict = cidfont_dict->GetDictFor("CIDSystemInfo");
  ASSERT_TRUE(cidinfo_dict);
  EXPECT_EQ("Adobe", cidinfo_dict->GetStringFor("Registry"));
  EXPECT_EQ("Identity", cidinfo_dict->GetStringFor("Ordering"));
  EXPECT_EQ(0, cidinfo_dict->GetNumberFor("Supplement"));
  CheckFontDescriptor(cidfont_dict, FPDF_FONT_TYPE1, false, false, size, data);

  // Check widths
  CPDF_Array* widths_array = cidfont_dict->GetArrayFor("W");
  ASSERT_TRUE(widths_array);
  EXPECT_GT(widths_array->GetCount(), 1U);
  CheckCompositeFontWidths(widths_array, typed_font);
}

TEST_F(FPDFEditEmbeddertest, LoadCIDType2Font) {
  CreateNewDocument();
  const CPDF_Font* stock_font =
      CPDF_Font::GetStockFont(cpdf_doc(), "Helvetica-Oblique");
  const uint8_t* data = stock_font->GetFont()->GetFontData();
  const uint32_t size = stock_font->GetFont()->GetSize();

  std::unique_ptr<void, FPDFFontDeleter> font(
      FPDFText_LoadFont(document(), data, size, FPDF_FONT_TRUETYPE, 1));
  ASSERT_TRUE(font.get());
  CPDF_Font* typed_font = static_cast<CPDF_Font*>(font.get());
  EXPECT_TRUE(typed_font->IsCIDFont());

  // Check font dictionary entries
  CPDF_Dictionary* font_dict = typed_font->GetFontDict();
  EXPECT_EQ("Font", font_dict->GetStringFor("Type"));
  EXPECT_EQ("Type0", font_dict->GetStringFor("Subtype"));
  EXPECT_EQ("Arial Italic", font_dict->GetStringFor("BaseFont"));
  EXPECT_EQ("Identity-H", font_dict->GetStringFor("Encoding"));
  CPDF_Array* descendant_array = font_dict->GetArrayFor("DescendantFonts");
  ASSERT_TRUE(descendant_array);
  EXPECT_EQ(1U, descendant_array->GetCount());

  // Check the CIDFontDict
  CPDF_Dictionary* cidfont_dict = descendant_array->GetDictAt(0);
  EXPECT_EQ("Font", cidfont_dict->GetStringFor("Type"));
  EXPECT_EQ("CIDFontType2", cidfont_dict->GetStringFor("Subtype"));
  EXPECT_EQ("Arial Italic", cidfont_dict->GetStringFor("BaseFont"));
  CPDF_Dictionary* cidinfo_dict = cidfont_dict->GetDictFor("CIDSystemInfo");
  ASSERT_TRUE(cidinfo_dict);
  EXPECT_EQ("Adobe", cidinfo_dict->GetStringFor("Registry"));
  EXPECT_EQ("Identity", cidinfo_dict->GetStringFor("Ordering"));
  EXPECT_EQ(0, cidinfo_dict->GetNumberFor("Supplement"));
  CheckFontDescriptor(cidfont_dict, FPDF_FONT_TRUETYPE, false, true, size,
                      data);

  // Check widths
  CPDF_Array* widths_array = cidfont_dict->GetArrayFor("W");
  ASSERT_TRUE(widths_array);
  CheckCompositeFontWidths(widths_array, typed_font);
}

TEST_F(FPDFEditEmbeddertest, NormalizeNegativeRotation) {
  // Load document with a -90 degree rotation
  EXPECT_TRUE(OpenDocument("bug_713197.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_NE(nullptr, page);

  EXPECT_EQ(3, FPDFPage_GetRotation(page));
  UnloadPage(page);
}

TEST_F(FPDFEditEmbeddertest, AddTrueTypeFontText) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);
  {
    const CPDF_Font* stock_font = CPDF_Font::GetStockFont(cpdf_doc(), "Arial");
    const uint8_t* data = stock_font->GetFont()->GetFontData();
    const uint32_t size = stock_font->GetFont()->GetSize();
    std::unique_ptr<void, FPDFFontDeleter> font(
        FPDFText_LoadFont(document(), data, size, FPDF_FONT_TRUETYPE, 0));
    ASSERT_TRUE(font.get());

    // Add some text to the page
    FPDF_PAGEOBJECT text_object =
        FPDFPageObj_CreateTextObj(document(), font.get(), 12.0f);
    EXPECT_TRUE(text_object);
    std::unique_ptr<unsigned short, pdfium::FreeDeleter> text =
        GetFPDFWideString(L"I am testing my loaded font, WEE.");
    EXPECT_TRUE(FPDFText_SetText(text_object, text.get()));
    FPDFPageObj_Transform(text_object, 1, 0, 0, 1, 400, 400);
    FPDFPage_InsertObject(page, text_object);
    EXPECT_TRUE(FPDFPage_GenerateContent(page));
    FPDF_BITMAP page_bitmap = RenderPage(page);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
    const char md5[] = "17d2b6cd574cf66170b09c8927529a94";
#else
    const char md5[] = "28e5b10743660dcdfd1618db47b39d32";
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
    CompareBitmap(page_bitmap, 612, 792, md5);
    FPDFBitmap_Destroy(page_bitmap);

    // Add some more text, same font
    FPDF_PAGEOBJECT text_object2 =
        FPDFPageObj_CreateTextObj(document(), font.get(), 15.0f);
    std::unique_ptr<unsigned short, pdfium::FreeDeleter> text2 =
        GetFPDFWideString(L"Bigger font size");
    EXPECT_TRUE(FPDFText_SetText(text_object2, text2.get()));
    FPDFPageObj_Transform(text_object2, 1, 0, 0, 1, 200, 200);
    FPDFPage_InsertObject(page, text_object2);
    EXPECT_TRUE(FPDFPage_GenerateContent(page));
  }
  FPDF_BITMAP page_bitmap2 = RenderPage(page);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  const char md5_2[] = "8eded4193ff1f0f77b8b600a825e97ea";
#else
  const char md5_2[] = "a068eef4110d607f77c87ea8340fa2a5";
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  CompareBitmap(page_bitmap2, 612, 792, md5_2);
  FPDFBitmap_Destroy(page_bitmap2);

  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  FPDF_ClosePage(page);
  std::string new_file = GetString();

  // Render the saved result
  FPDF_FILEACCESS file_access;
  memset(&file_access, 0, sizeof(file_access));
  file_access.m_FileLen = new_file.size();
  file_access.m_GetBlock = GetBlockFromString;
  file_access.m_Param = &new_file;
  FPDF_DOCUMENT new_doc = FPDF_LoadCustomDocument(&file_access, nullptr);
  ASSERT_NE(nullptr, new_doc);
  EXPECT_EQ(1, FPDF_GetPageCount(new_doc));
  FPDF_PAGE new_page = FPDF_LoadPage(new_doc, 0);
  ASSERT_NE(nullptr, new_page);
  FPDF_BITMAP new_bitmap = RenderPage(new_page);
  CompareBitmap(new_bitmap, 612, 792, md5_2);
  FPDFBitmap_Destroy(new_bitmap);
  FPDF_ClosePage(new_page);
  FPDF_CloseDocument(new_doc);
}

// TODO(npm): Add tests using Japanese fonts in other OS.
#if _FXM_PLATFORM_ == _FXM_PLATFORM_LINUX_
TEST_F(FPDFEditEmbeddertest, AddCIDFontText) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);
  CFX_Font CIDfont;
  {
    // First, get the data from the font
    CIDfont.LoadSubst("IPAGothic", 1, 0, 400, 0, 932, 0);
    EXPECT_EQ("IPAGothic", CIDfont.GetFaceName());
    const uint8_t* data = CIDfont.GetFontData();
    const uint32_t size = CIDfont.GetSize();

    // Load the data into a FPDF_Font.
    std::unique_ptr<void, FPDFFontDeleter> font(
        FPDFText_LoadFont(document(), data, size, FPDF_FONT_TRUETYPE, 1));
    ASSERT_TRUE(font.get());

    // Add some text to the page
    FPDF_PAGEOBJECT text_object =
        FPDFPageObj_CreateTextObj(document(), font.get(), 12.0f);
    ASSERT_TRUE(text_object);
    std::wstring wstr = L"ABCDEFGhijklmnop.";
    std::unique_ptr<unsigned short, pdfium::FreeDeleter> text =
        GetFPDFWideString(wstr);
    EXPECT_TRUE(FPDFText_SetText(text_object, text.get()));
    FPDFPageObj_Transform(text_object, 1, 0, 0, 1, 200, 200);
    FPDFPage_InsertObject(page, text_object);

    // And add some Japanese characters
    FPDF_PAGEOBJECT text_object2 =
        FPDFPageObj_CreateTextObj(document(), font.get(), 18.0f);
    ASSERT_TRUE(text_object2);
    std::wstring wstr2 =
        L"\u3053\u3093\u306B\u3061\u306f\u4e16\u754C\u3002\u3053\u3053\u306B1"
        L"\u756A";
    std::unique_ptr<unsigned short, pdfium::FreeDeleter> text2 =
        GetFPDFWideString(wstr2);
    EXPECT_TRUE(FPDFText_SetText(text_object2, text2.get()));
    FPDFPageObj_Transform(text_object2, 1, 0, 0, 1, 100, 500);
    FPDFPage_InsertObject(page, text_object2);
  }

  // Generate contents and check that the text renders properly.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  FPDF_BITMAP page_bitmap = RenderPage(page);
  const char md5[] = "2bc6c1aaa2252e73246a75775ccf38c2";
  CompareBitmap(page_bitmap, 612, 792, md5);
  FPDFBitmap_Destroy(page_bitmap);

  // Save the document, close the page.
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  FPDF_ClosePage(page);
  std::string new_file = GetString();

  // Render the saved result
  FPDF_FILEACCESS file_access;
  memset(&file_access, 0, sizeof(file_access));
  file_access.m_FileLen = new_file.size();
  file_access.m_GetBlock = GetBlockFromString;
  file_access.m_Param = &new_file;
  FPDF_DOCUMENT new_doc = FPDF_LoadCustomDocument(&file_access, nullptr);
  ASSERT_NE(nullptr, new_doc);
  EXPECT_EQ(1, FPDF_GetPageCount(new_doc));
  FPDF_PAGE new_page = FPDF_LoadPage(new_doc, 0);
  ASSERT_NE(nullptr, new_page);
  FPDF_BITMAP new_bitmap = RenderPage(new_page);
  CompareBitmap(new_bitmap, 612, 792, md5);
  FPDFBitmap_Destroy(new_bitmap);
  FPDF_ClosePage(new_page);
  FPDF_CloseDocument(new_doc);
}
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_LINUX_
