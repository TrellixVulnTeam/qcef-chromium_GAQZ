// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/cxfa_ffpageview.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"
#include "xfa/fde/cfde_rendercontext.h"
#include "xfa/fxfa/app/xfa_ffcheckbutton.h"
#include "xfa/fxfa/app/xfa_ffchoicelist.h"
#include "xfa/fxfa/app/xfa_fffield.h"
#include "xfa/fxfa/app/xfa_ffimageedit.h"
#include "xfa/fxfa/app/xfa_ffpushbutton.h"
#include "xfa/fxfa/app/xfa_fftextedit.h"
#include "xfa/fxfa/app/xfa_fwladapter.h"
#include "xfa/fxfa/cxfa_ffdoc.h"
#include "xfa/fxfa/cxfa_ffdocview.h"
#include "xfa/fxfa/cxfa_ffwidget.h"
#include "xfa/fxfa/parser/cxfa_node.h"

namespace {

CFX_Matrix GetPageMatrix(const CFX_RectF& docPageRect,
                         const CFX_Rect& devicePageRect,
                         int32_t iRotate,
                         uint32_t dwCoordinatesType) {
  ASSERT(iRotate >= 0 && iRotate <= 3);

  bool bFlipX = (dwCoordinatesType & 0x01) != 0;
  bool bFlipY = (dwCoordinatesType & 0x02) != 0;
  CFX_Matrix m((bFlipX ? -1.0f : 1.0f), 0, 0, (bFlipY ? -1.0f : 1.0f), 0, 0);
  if (iRotate == 0 || iRotate == 2) {
    m.a *= (float)devicePageRect.width / docPageRect.width;
    m.d *= (float)devicePageRect.height / docPageRect.height;
  } else {
    m.a *= (float)devicePageRect.height / docPageRect.width;
    m.d *= (float)devicePageRect.width / docPageRect.height;
  }
  m.Rotate(iRotate * 1.57079632675f);
  switch (iRotate) {
    case 0:
      m.e = bFlipX ? (float)devicePageRect.right() : (float)devicePageRect.left;
      m.f = bFlipY ? (float)devicePageRect.bottom() : (float)devicePageRect.top;
      break;
    case 1:
      m.e = bFlipY ? (float)devicePageRect.left : (float)devicePageRect.right();
      m.f = bFlipX ? (float)devicePageRect.bottom() : (float)devicePageRect.top;
      break;
    case 2:
      m.e = bFlipX ? (float)devicePageRect.left : (float)devicePageRect.right();
      m.f = bFlipY ? (float)devicePageRect.top : (float)devicePageRect.bottom();
      break;
    case 3:
      m.e = bFlipY ? (float)devicePageRect.right() : (float)devicePageRect.left;
      m.f = bFlipX ? (float)devicePageRect.top : (float)devicePageRect.bottom();
      break;
    default:
      break;
  }
  return m;
}

bool PageWidgetFilter(CXFA_FFWidget* pWidget,
                      uint32_t dwFilter,
                      bool bTraversal,
                      bool bIgnorerelevant) {
  CXFA_WidgetAcc* pWidgetAcc = pWidget->GetDataAcc();

  if (!!(dwFilter & XFA_WidgetStatus_Focused) &&
      pWidgetAcc->GetElementType() != XFA_Element::Field) {
    return false;
  }

  uint32_t dwStatus = pWidget->GetStatus();
  if (bTraversal && (dwStatus & XFA_WidgetStatus_Disabled))
    return false;
  if (bIgnorerelevant)
    return !!(dwStatus & XFA_WidgetStatus_Visible);

  dwFilter &= (XFA_WidgetStatus_Visible | XFA_WidgetStatus_Viewable |
               XFA_WidgetStatus_Printable);
  return (dwFilter & dwStatus) == dwFilter;
}

bool IsLayoutElement(XFA_Element eElement, bool bLayoutContainer) {
  switch (eElement) {
    case XFA_Element::Draw:
    case XFA_Element::Field:
    case XFA_Element::InstanceManager:
      return !bLayoutContainer;
    case XFA_Element::Area:
    case XFA_Element::Subform:
    case XFA_Element::ExclGroup:
    case XFA_Element::SubformSet:
    case XFA_Element::PageArea:
    case XFA_Element::Form:
      return true;
    default:
      return false;
  }
}

}  // namespace

CXFA_FFPageView::CXFA_FFPageView(CXFA_FFDocView* pDocView, CXFA_Node* pPageArea)
    : CXFA_ContainerLayoutItem(pPageArea), m_pDocView(pDocView) {}

CXFA_FFPageView::~CXFA_FFPageView() {}

CXFA_FFDocView* CXFA_FFPageView::GetDocView() const {
  return m_pDocView.Get();
}

CFX_RectF CXFA_FFPageView::GetPageViewRect() const {
  return CFX_RectF(0, 0, GetPageSize());
}

CFX_Matrix CXFA_FFPageView::GetDisplayMatrix(const CFX_Rect& rtDisp,
                                             int32_t iRotate) const {
  return GetPageMatrix(CFX_RectF(0, 0, GetPageSize()), rtDisp, iRotate, 0);
}

std::unique_ptr<IXFA_WidgetIterator> CXFA_FFPageView::CreateWidgetIterator(
    uint32_t dwTraverseWay,
    uint32_t dwWidgetFilter) {
  switch (dwTraverseWay) {
    case XFA_TRAVERSEWAY_Tranvalse:
      return pdfium::MakeUnique<CXFA_FFTabOrderPageWidgetIterator>(
          this, dwWidgetFilter);
    case XFA_TRAVERSEWAY_Form:
      return pdfium::MakeUnique<CXFA_FFPageWidgetIterator>(this,
                                                           dwWidgetFilter);
  }
  return nullptr;
}

CXFA_FFPageWidgetIterator::CXFA_FFPageWidgetIterator(CXFA_FFPageView* pPageView,
                                                     uint32_t dwFilter)
    : m_pPageView(pPageView), m_dwFilter(dwFilter), m_sIterator(pPageView) {
  m_bIgnorerelevant =
      m_pPageView->GetDocView()->GetDoc()->GetXFADoc()->GetCurVersionMode() <
      XFA_VERSION_205;
}

CXFA_FFPageWidgetIterator::~CXFA_FFPageWidgetIterator() {}

void CXFA_FFPageWidgetIterator::Reset() {
  m_sIterator.Reset();
}

CXFA_FFWidget* CXFA_FFPageWidgetIterator::MoveToFirst() {
  m_sIterator.Reset();
  for (CXFA_LayoutItem* pLayoutItem = m_sIterator.GetCurrent(); pLayoutItem;
       pLayoutItem = m_sIterator.MoveToNext()) {
    if (CXFA_FFWidget* hWidget = GetWidget(pLayoutItem)) {
      return hWidget;
    }
  }
  return nullptr;
}

CXFA_FFWidget* CXFA_FFPageWidgetIterator::MoveToLast() {
  m_sIterator.SetCurrent(nullptr);
  return MoveToPrevious();
}

CXFA_FFWidget* CXFA_FFPageWidgetIterator::MoveToNext() {
  for (CXFA_LayoutItem* pLayoutItem = m_sIterator.MoveToNext(); pLayoutItem;
       pLayoutItem = m_sIterator.MoveToNext()) {
    if (CXFA_FFWidget* hWidget = GetWidget(pLayoutItem)) {
      return hWidget;
    }
  }
  return nullptr;
}

CXFA_FFWidget* CXFA_FFPageWidgetIterator::MoveToPrevious() {
  for (CXFA_LayoutItem* pLayoutItem = m_sIterator.MoveToPrev(); pLayoutItem;
       pLayoutItem = m_sIterator.MoveToPrev()) {
    if (CXFA_FFWidget* hWidget = GetWidget(pLayoutItem)) {
      return hWidget;
    }
  }
  return nullptr;
}

CXFA_FFWidget* CXFA_FFPageWidgetIterator::GetCurrentWidget() {
  CXFA_LayoutItem* pLayoutItem = m_sIterator.GetCurrent();
  return pLayoutItem ? XFA_GetWidgetFromLayoutItem(pLayoutItem) : nullptr;
}

bool CXFA_FFPageWidgetIterator::SetCurrentWidget(CXFA_FFWidget* hWidget) {
  return hWidget && m_sIterator.SetCurrent(hWidget);
}

CXFA_FFWidget* CXFA_FFPageWidgetIterator::GetWidget(
    CXFA_LayoutItem* pLayoutItem) {
  CXFA_FFWidget* pWidget = XFA_GetWidgetFromLayoutItem(pLayoutItem);
  if (!pWidget)
    return nullptr;

  if (!PageWidgetFilter(pWidget, m_dwFilter, false, m_bIgnorerelevant))
    return nullptr;

  if (!pWidget->IsLoaded() &&
      !!(pWidget->GetStatus() & XFA_WidgetStatus_Visible)) {
    pWidget->LoadWidget();
  }
  return pWidget;
}

void CXFA_TabParam::AppendTabParam(CXFA_TabParam* pParam) {
  m_Children.push_back(pParam->GetWidget());
  m_Children.insert(m_Children.end(), pParam->GetChildren().begin(),
                    pParam->GetChildren().end());
}

void CXFA_TabParam::ClearChildren() {
  m_Children.clear();
}

CXFA_FFTabOrderPageWidgetIterator::CXFA_FFTabOrderPageWidgetIterator(
    CXFA_FFPageView* pPageView,
    uint32_t dwFilter)
    : m_pPageView(pPageView), m_dwFilter(dwFilter), m_iCurWidget(-1) {
  m_bIgnorerelevant =
      m_pPageView->GetDocView()->GetDoc()->GetXFADoc()->GetCurVersionMode() <
      XFA_VERSION_205;
  Reset();
}

CXFA_FFTabOrderPageWidgetIterator::~CXFA_FFTabOrderPageWidgetIterator() {}

void CXFA_FFTabOrderPageWidgetIterator::Reset() {
  CreateTabOrderWidgetArray();
  m_iCurWidget = -1;
}

CXFA_FFWidget* CXFA_FFTabOrderPageWidgetIterator::MoveToFirst() {
  for (int32_t i = 0;
       i < pdfium::CollectionSize<int32_t>(m_TabOrderWidgetArray); i++) {
    if (PageWidgetFilter(m_TabOrderWidgetArray[i], m_dwFilter, true,
                         m_bIgnorerelevant)) {
      m_iCurWidget = i;
      return m_TabOrderWidgetArray[m_iCurWidget];
    }
  }
  return nullptr;
}

CXFA_FFWidget* CXFA_FFTabOrderPageWidgetIterator::MoveToLast() {
  for (int32_t i = pdfium::CollectionSize<int32_t>(m_TabOrderWidgetArray) - 1;
       i >= 0; i--) {
    if (PageWidgetFilter(m_TabOrderWidgetArray[i], m_dwFilter, true,
                         m_bIgnorerelevant)) {
      m_iCurWidget = i;
      return m_TabOrderWidgetArray[m_iCurWidget];
    }
  }
  return nullptr;
}

CXFA_FFWidget* CXFA_FFTabOrderPageWidgetIterator::MoveToNext() {
  for (int32_t i = m_iCurWidget + 1;
       i < pdfium::CollectionSize<int32_t>(m_TabOrderWidgetArray); i++) {
    if (PageWidgetFilter(m_TabOrderWidgetArray[i], m_dwFilter, true,
                         m_bIgnorerelevant)) {
      m_iCurWidget = i;
      return m_TabOrderWidgetArray[m_iCurWidget];
    }
  }
  m_iCurWidget = -1;
  return nullptr;
}

CXFA_FFWidget* CXFA_FFTabOrderPageWidgetIterator::MoveToPrevious() {
  for (int32_t i = m_iCurWidget - 1; i >= 0; i--) {
    if (PageWidgetFilter(m_TabOrderWidgetArray[i], m_dwFilter, true,
                         m_bIgnorerelevant)) {
      m_iCurWidget = i;
      return m_TabOrderWidgetArray[m_iCurWidget];
    }
  }
  m_iCurWidget = -1;
  return nullptr;
}

CXFA_FFWidget* CXFA_FFTabOrderPageWidgetIterator::GetCurrentWidget() {
  return m_iCurWidget >= 0 ? m_TabOrderWidgetArray[m_iCurWidget] : nullptr;
}

bool CXFA_FFTabOrderPageWidgetIterator::SetCurrentWidget(
    CXFA_FFWidget* hWidget) {
  auto it = std::find(m_TabOrderWidgetArray.begin(),
                      m_TabOrderWidgetArray.end(), hWidget);
  if (it == m_TabOrderWidgetArray.end())
    return false;

  m_iCurWidget = it - m_TabOrderWidgetArray.begin();
  return true;
}

CXFA_FFWidget* CXFA_FFTabOrderPageWidgetIterator::GetTraverseWidget(
    CXFA_FFWidget* pWidget) {
  CXFA_WidgetAcc* pAcc = pWidget->GetDataAcc();
  CXFA_Node* pTraversal = pAcc->GetNode()->GetChild(0, XFA_Element::Traversal);
  if (pTraversal) {
    CXFA_Node* pTraverse = pTraversal->GetChild(0, XFA_Element::Traverse);
    if (pTraverse) {
      CFX_WideString wsTraverseWidgetName;
      if (pTraverse->GetAttribute(XFA_ATTRIBUTE_Ref, wsTraverseWidgetName)) {
        return FindWidgetByName(wsTraverseWidgetName, pWidget);
      }
    }
  }
  return nullptr;
}
CXFA_FFWidget* CXFA_FFTabOrderPageWidgetIterator::FindWidgetByName(
    const CFX_WideString& wsWidgetName,
    CXFA_FFWidget* pRefWidget) {
  return pRefWidget->GetDocView()->GetWidgetByName(wsWidgetName, pRefWidget);
}

void CXFA_FFTabOrderPageWidgetIterator::CreateTabOrderWidgetArray() {
  m_TabOrderWidgetArray.clear();

  std::vector<CXFA_FFWidget*> SpaceOrderWidgetArray;
  CreateSpaceOrderWidgetArray(&SpaceOrderWidgetArray);
  if (SpaceOrderWidgetArray.empty())
    return;

  int32_t nWidgetCount = pdfium::CollectionSize<int32_t>(SpaceOrderWidgetArray);
  CXFA_FFWidget* hWidget = SpaceOrderWidgetArray[0];
  while (pdfium::CollectionSize<int32_t>(m_TabOrderWidgetArray) <
         nWidgetCount) {
    if (!pdfium::ContainsValue(m_TabOrderWidgetArray, hWidget)) {
      m_TabOrderWidgetArray.push_back(hWidget);
      CXFA_WidgetAcc* pWidgetAcc = hWidget->GetDataAcc();
      if (pWidgetAcc->GetUIType() == XFA_Element::ExclGroup) {
        auto it = std::find(SpaceOrderWidgetArray.begin(),
                            SpaceOrderWidgetArray.end(), hWidget);
        int32_t iWidgetIndex = it != SpaceOrderWidgetArray.end()
                                   ? it - SpaceOrderWidgetArray.begin() + 1
                                   : 0;
        while (true) {
          CXFA_FFWidget* pRadio =
              SpaceOrderWidgetArray[iWidgetIndex % nWidgetCount];
          if (pRadio->GetDataAcc()->GetExclGroup() != pWidgetAcc) {
            break;
          }
          if (!pdfium::ContainsValue(m_TabOrderWidgetArray, hWidget)) {
            m_TabOrderWidgetArray.push_back(pRadio);
          }
          iWidgetIndex++;
        }
      }
      if (CXFA_FFWidget* hTraverseWidget = GetTraverseWidget(hWidget)) {
        hWidget = hTraverseWidget;
        continue;
      }
    }
    auto it = std::find(SpaceOrderWidgetArray.begin(),
                        SpaceOrderWidgetArray.end(), hWidget);
    int32_t iWidgetIndex = it != SpaceOrderWidgetArray.end()
                               ? it - SpaceOrderWidgetArray.begin() + 1
                               : 0;
    hWidget = SpaceOrderWidgetArray[iWidgetIndex % nWidgetCount];
  }
}

void CXFA_FFTabOrderPageWidgetIterator::OrderContainer(
    CXFA_LayoutItemIterator* sIterator,
    CXFA_LayoutItem* pContainerItem,
    CXFA_TabParam* pContainer,
    bool& bCurrentItem,
    bool& bContentArea,
    bool bMarsterPage) {
  std::vector<std::unique_ptr<CXFA_TabParam>> tabParams;
  CXFA_LayoutItem* pSearchItem = sIterator->MoveToNext();
  while (pSearchItem) {
    if (!pSearchItem->IsContentLayoutItem()) {
      bContentArea = true;
      pSearchItem = sIterator->MoveToNext();
      continue;
    }
    if (bMarsterPage && bContentArea) {
      break;
    }
    if (bMarsterPage || bContentArea) {
      CXFA_FFWidget* hWidget = GetWidget(pSearchItem);
      if (!hWidget) {
        pSearchItem = sIterator->MoveToNext();
        continue;
      }
      if (pContainerItem && (pSearchItem->GetParent() != pContainerItem)) {
        bCurrentItem = true;
        break;
      }
      tabParams.push_back(pdfium::MakeUnique<CXFA_TabParam>(hWidget));
      if (IsLayoutElement(pSearchItem->GetFormNode()->GetElementType(), true)) {
        OrderContainer(sIterator, pSearchItem, tabParams.back().get(),
                       bCurrentItem, bContentArea, bMarsterPage);
      }
    }
    if (bCurrentItem) {
      pSearchItem = sIterator->GetCurrent();
      bCurrentItem = false;
    } else {
      pSearchItem = sIterator->MoveToNext();
    }
  }
  std::sort(tabParams.begin(), tabParams.end(),
            [](const std::unique_ptr<CXFA_TabParam>& arg1,
               const std::unique_ptr<CXFA_TabParam>& arg2) {
              const CFX_RectF& rt1 = arg1->GetWidget()->GetWidgetRect();
              const CFX_RectF& rt2 = arg2->GetWidget()->GetWidgetRect();
              if (rt1.top - rt2.top >= XFA_FLOAT_PERCISION)
                return rt1.top < rt2.top;
              return rt1.left < rt2.left;
            });
  for (const auto& pParam : tabParams)
    pContainer->AppendTabParam(pParam.get());
}

void CXFA_FFTabOrderPageWidgetIterator::CreateSpaceOrderWidgetArray(
    std::vector<CXFA_FFWidget*>* WidgetArray) {
  CXFA_LayoutItemIterator sIterator(m_pPageView);
  auto pParam = pdfium::MakeUnique<CXFA_TabParam>(nullptr);
  bool bCurrentItem = false;
  bool bContentArea = false;
  OrderContainer(&sIterator, nullptr, pParam.get(), bCurrentItem, bContentArea);
  WidgetArray->insert(WidgetArray->end(), pParam->GetChildren().begin(),
                      pParam->GetChildren().end());

  sIterator.Reset();
  bCurrentItem = false;
  bContentArea = false;
  pParam->ClearChildren();
  OrderContainer(&sIterator, nullptr, pParam.get(), bCurrentItem, bContentArea,
                 true);
  WidgetArray->insert(WidgetArray->end(), pParam->GetChildren().begin(),
                      pParam->GetChildren().end());
}

CXFA_FFWidget* CXFA_FFTabOrderPageWidgetIterator::GetWidget(
    CXFA_LayoutItem* pLayoutItem) {
  if (CXFA_FFWidget* pWidget = XFA_GetWidgetFromLayoutItem(pLayoutItem)) {
    if (!pWidget->IsLoaded() &&
        (pWidget->GetStatus() & XFA_WidgetStatus_Visible)) {
      pWidget->LoadWidget();
    }
    return pWidget;
  }
  return nullptr;
}

CXFA_TabParam::CXFA_TabParam(CXFA_FFWidget* pWidget) : m_pWidget(pWidget) {}

CXFA_TabParam::~CXFA_TabParam() {}
