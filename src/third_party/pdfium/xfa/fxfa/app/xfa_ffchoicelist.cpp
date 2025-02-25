// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/app/xfa_ffchoicelist.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"
#include "xfa/fwl/cfwl_app.h"
#include "xfa/fwl/cfwl_combobox.h"
#include "xfa/fwl/cfwl_edit.h"
#include "xfa/fwl/cfwl_eventselectchanged.h"
#include "xfa/fwl/cfwl_listbox.h"
#include "xfa/fwl/cfwl_notedriver.h"
#include "xfa/fwl/cfwl_widgetproperties.h"
#include "xfa/fxfa/app/xfa_fffield.h"
#include "xfa/fxfa/app/xfa_fwladapter.h"
#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/cxfa_ffdoc.h"
#include "xfa/fxfa/cxfa_ffdocview.h"
#include "xfa/fxfa/cxfa_ffpageview.h"
#include "xfa/fxfa/cxfa_ffwidget.h"

namespace {

CFWL_ListBox* ToListBox(CFWL_Widget* widget) {
  return static_cast<CFWL_ListBox*>(widget);
}

CFWL_ComboBox* ToComboBox(CFWL_Widget* widget) {
  return static_cast<CFWL_ComboBox*>(widget);
}

}  // namespace

CXFA_FFListBox::CXFA_FFListBox(CXFA_WidgetAcc* pDataAcc)
    : CXFA_FFField(pDataAcc), m_pOldDelegate(nullptr) {}

CXFA_FFListBox::~CXFA_FFListBox() {
  if (!m_pNormalWidget)
    return;

  CFWL_NoteDriver* pNoteDriver =
      m_pNormalWidget->GetOwnerApp()->GetNoteDriver();
  pNoteDriver->UnregisterEventTarget(m_pNormalWidget.get());
}

bool CXFA_FFListBox::LoadWidget() {
  auto pNew = pdfium::MakeUnique<CFWL_ListBox>(
      GetFWLApp(), pdfium::MakeUnique<CFWL_WidgetProperties>(), nullptr);
  CFWL_ListBox* pListBox = pNew.get();
  pListBox->ModifyStyles(FWL_WGTSTYLE_VScroll | FWL_WGTSTYLE_NoBackground,
                         0xFFFFFFFF);
  m_pNormalWidget = std::move(pNew);
  m_pNormalWidget->SetLayoutItem(this);

  CFWL_NoteDriver* pNoteDriver =
      m_pNormalWidget->GetOwnerApp()->GetNoteDriver();
  pNoteDriver->RegisterEventTarget(m_pNormalWidget.get(),
                                   m_pNormalWidget.get());
  m_pOldDelegate = m_pNormalWidget->GetDelegate();
  m_pNormalWidget->SetDelegate(this);
  m_pNormalWidget->LockUpdate();

  for (const auto& label : m_pDataAcc->GetChoiceListItems(false))
    pListBox->AddString(label.AsStringC());

  uint32_t dwExtendedStyle = FWL_STYLEEXT_LTB_ShowScrollBarFocus;
  if (m_pDataAcc->GetChoiceListOpen() == XFA_ATTRIBUTEENUM_MultiSelect)
    dwExtendedStyle |= FWL_STYLEEXT_LTB_MultiSelection;

  dwExtendedStyle |= GetAlignment();
  m_pNormalWidget->ModifyStylesEx(dwExtendedStyle, 0xFFFFFFFF);
  for (int32_t selected : m_pDataAcc->GetSelectedItems())
    pListBox->SetSelItem(pListBox->GetItem(nullptr, selected), true);

  m_pNormalWidget->UnlockUpdate();
  return CXFA_FFField::LoadWidget();
}

bool CXFA_FFListBox::OnKillFocus(CXFA_FFWidget* pNewFocus) {
  if (!ProcessCommittedData())
    UpdateFWLData();

  CXFA_FFField::OnKillFocus(pNewFocus);
  return true;
}

bool CXFA_FFListBox::CommitData() {
  auto* pListBox = ToListBox(m_pNormalWidget.get());
  std::vector<int32_t> iSelArray;
  int32_t iSels = pListBox->CountSelItems();
  for (int32_t i = 0; i < iSels; ++i)
    iSelArray.push_back(pListBox->GetSelIndex(i));

  m_pDataAcc->SetSelectedItems(iSelArray, true, false, true);
  return true;
}

bool CXFA_FFListBox::IsDataChanged() {
  std::vector<int32_t> iSelArray = m_pDataAcc->GetSelectedItems();
  int32_t iOldSels = pdfium::CollectionSize<int32_t>(iSelArray);
  auto* pListBox = ToListBox(m_pNormalWidget.get());
  int32_t iSels = pListBox->CountSelItems();
  if (iOldSels != iSels)
    return true;

  for (int32_t i = 0; i < iSels; ++i) {
    CFWL_ListItem* hlistItem = pListBox->GetItem(nullptr, iSelArray[i]);
    if (!(hlistItem->GetStates() & FWL_ITEMSTATE_LTB_Selected))
      return true;
  }
  return false;
}

uint32_t CXFA_FFListBox::GetAlignment() {
  CXFA_Para para = m_pDataAcc->GetPara();
  if (!para)
    return 0;

  uint32_t dwExtendedStyle = 0;
  switch (para.GetHorizontalAlign()) {
    case XFA_ATTRIBUTEENUM_Center:
      dwExtendedStyle |= FWL_STYLEEXT_LTB_CenterAlign;
      break;
    case XFA_ATTRIBUTEENUM_Justify:
      break;
    case XFA_ATTRIBUTEENUM_JustifyAll:
      break;
    case XFA_ATTRIBUTEENUM_Radix:
      break;
    case XFA_ATTRIBUTEENUM_Right:
      dwExtendedStyle |= FWL_STYLEEXT_LTB_RightAlign;
      break;
    default:
      dwExtendedStyle |= FWL_STYLEEXT_LTB_LeftAlign;
      break;
  }
  return dwExtendedStyle;
}

bool CXFA_FFListBox::UpdateFWLData() {
  if (!m_pNormalWidget)
    return false;

  auto* pListBox = ToListBox(m_pNormalWidget.get());
  std::vector<int32_t> iSelArray = m_pDataAcc->GetSelectedItems();
  std::vector<CFWL_ListItem*> selItemArray(iSelArray.size());
  std::transform(iSelArray.begin(), iSelArray.end(), selItemArray.begin(),
                 [pListBox](int32_t val) { return pListBox->GetSelItem(val); });

  pListBox->SetSelItem(pListBox->GetSelItem(-1), false);
  for (CFWL_ListItem* pItem : selItemArray)
    pListBox->SetSelItem(pItem, true);

  m_pNormalWidget->Update();
  return true;
}

void CXFA_FFListBox::OnSelectChanged(CFWL_Widget* pWidget) {
  CXFA_EventParam eParam;
  eParam.m_eType = XFA_EVENT_Change;
  eParam.m_pTarget = m_pDataAcc.Get();
  m_pDataAcc->GetValue(eParam.m_wsPrevText, XFA_VALUEPICTURE_Raw);

  auto* pListBox = ToListBox(m_pNormalWidget.get());
  int32_t iSels = pListBox->CountSelItems();
  if (iSels > 0) {
    CFWL_ListItem* item = pListBox->GetSelItem(0);
    eParam.m_wsNewText = item ? item->GetText() : L"";
  }
  m_pDataAcc->ProcessEvent(XFA_ATTRIBUTEENUM_Change, &eParam);
}

void CXFA_FFListBox::SetItemState(int32_t nIndex, bool bSelected) {
  auto* pListBox = ToListBox(m_pNormalWidget.get());
  pListBox->SetSelItem(pListBox->GetSelItem(nIndex), bSelected);
  m_pNormalWidget->Update();
  AddInvalidateRect();
}

void CXFA_FFListBox::InsertItem(const CFX_WideStringC& wsLabel,
                                int32_t nIndex) {
  CFX_WideString wsTemp(wsLabel);
  ToListBox(m_pNormalWidget.get())->AddString(wsTemp.AsStringC());
  m_pNormalWidget->Update();
  AddInvalidateRect();
}

void CXFA_FFListBox::DeleteItem(int32_t nIndex) {
  auto* pListBox = ToListBox(m_pNormalWidget.get());
  if (nIndex < 0)
    pListBox->DeleteAll();
  else
    pListBox->DeleteString(pListBox->GetItem(nullptr, nIndex));

  pListBox->Update();
  AddInvalidateRect();
}

void CXFA_FFListBox::OnProcessMessage(CFWL_Message* pMessage) {
  m_pOldDelegate->OnProcessMessage(pMessage);
}

void CXFA_FFListBox::OnProcessEvent(CFWL_Event* pEvent) {
  CXFA_FFField::OnProcessEvent(pEvent);
  switch (pEvent->GetType()) {
    case CFWL_Event::Type::SelectChanged:
      OnSelectChanged(m_pNormalWidget.get());
      break;
    default:
      break;
  }
  m_pOldDelegate->OnProcessEvent(pEvent);
}

void CXFA_FFListBox::OnDrawWidget(CFX_Graphics* pGraphics,
                                  const CFX_Matrix* pMatrix) {
  m_pOldDelegate->OnDrawWidget(pGraphics, pMatrix);
}

CXFA_FFComboBox::CXFA_FFComboBox(CXFA_WidgetAcc* pDataAcc)
    : CXFA_FFField(pDataAcc), m_pOldDelegate(nullptr) {}

CXFA_FFComboBox::~CXFA_FFComboBox() {}

CFX_RectF CXFA_FFComboBox::GetBBox(uint32_t dwStatus, bool bDrawFocus) {
  return bDrawFocus ? CFX_RectF() : CXFA_FFWidget::GetBBox(dwStatus);
}

bool CXFA_FFComboBox::PtInActiveRect(const CFX_PointF& point) {
  auto* pComboBox = ToComboBox(m_pNormalWidget.get());
  return pComboBox && pComboBox->GetBBox().Contains(point);
}

bool CXFA_FFComboBox::LoadWidget() {
  auto pNew = pdfium::MakeUnique<CFWL_ComboBox>(GetFWLApp());
  CFWL_ComboBox* pComboBox = pNew.get();
  m_pNormalWidget = std::move(pNew);
  m_pNormalWidget->SetLayoutItem(this);

  CFWL_NoteDriver* pNoteDriver =
      m_pNormalWidget->GetOwnerApp()->GetNoteDriver();
  pNoteDriver->RegisterEventTarget(m_pNormalWidget.get(),
                                   m_pNormalWidget.get());
  m_pOldDelegate = m_pNormalWidget->GetDelegate();
  m_pNormalWidget->SetDelegate(this);
  m_pNormalWidget->LockUpdate();

  for (const auto& label : m_pDataAcc->GetChoiceListItems(false))
    pComboBox->AddString(label.AsStringC());

  std::vector<int32_t> iSelArray = m_pDataAcc->GetSelectedItems();
  if (!iSelArray.empty()) {
    pComboBox->SetCurSel(iSelArray.front());
  } else {
    CFX_WideString wsText;
    m_pDataAcc->GetValue(wsText, XFA_VALUEPICTURE_Raw);
    pComboBox->SetEditText(wsText);
  }

  UpdateWidgetProperty();
  m_pNormalWidget->UnlockUpdate();
  return CXFA_FFField::LoadWidget();
}

void CXFA_FFComboBox::UpdateWidgetProperty() {
  auto* pComboBox = ToComboBox(m_pNormalWidget.get());
  if (!pComboBox)
    return;

  uint32_t dwExtendedStyle = 0;
  uint32_t dwEditStyles =
      FWL_STYLEEXT_EDT_ReadOnly | FWL_STYLEEXT_EDT_LastLineHeight;
  dwExtendedStyle |= UpdateUIProperty();
  if (m_pDataAcc->IsChoiceListAllowTextEntry()) {
    dwEditStyles &= ~FWL_STYLEEXT_EDT_ReadOnly;
    dwExtendedStyle |= FWL_STYLEEXT_CMB_DropDown;
  }
  if (m_pDataAcc->GetAccess() != XFA_ATTRIBUTEENUM_Open ||
      !m_pDataAcc->GetDoc()->GetXFADoc()->IsInteractive()) {
    dwEditStyles |= FWL_STYLEEXT_EDT_ReadOnly;
    dwExtendedStyle |= FWL_STYLEEXT_CMB_ReadOnly;
  }
  dwExtendedStyle |= GetAlignment();
  m_pNormalWidget->ModifyStylesEx(dwExtendedStyle, 0xFFFFFFFF);

  if (m_pDataAcc->GetHorizontalScrollPolicy() != XFA_ATTRIBUTEENUM_Off)
    dwEditStyles |= FWL_STYLEEXT_EDT_AutoHScroll;

  pComboBox->EditModifyStylesEx(dwEditStyles, 0xFFFFFFFF);
}

bool CXFA_FFComboBox::OnRButtonUp(uint32_t dwFlags, const CFX_PointF& point) {
  if (!CXFA_FFField::OnRButtonUp(dwFlags, point))
    return false;

  GetDoc()->GetDocEnvironment()->PopupMenu(this, point);
  return true;
}

bool CXFA_FFComboBox::OnKillFocus(CXFA_FFWidget* pNewWidget) {
  if (!ProcessCommittedData())
    UpdateFWLData();

  CXFA_FFField::OnKillFocus(pNewWidget);
  return true;
}

void CXFA_FFComboBox::OpenDropDownList() {
  ToComboBox(m_pNormalWidget.get())->OpenDropDownList(true);
}

bool CXFA_FFComboBox::CommitData() {
  return m_pDataAcc->SetValue(m_wsNewValue, XFA_VALUEPICTURE_Raw);
}

bool CXFA_FFComboBox::IsDataChanged() {
  auto* pFWLcombobox = ToComboBox(m_pNormalWidget.get());
  CFX_WideString wsText = pFWLcombobox->GetEditText();
  int32_t iCursel = pFWLcombobox->GetCurSel();
  if (iCursel >= 0) {
    CFX_WideString wsSel = pFWLcombobox->GetTextByIndex(iCursel);
    if (wsSel == wsText)
      m_pDataAcc->GetChoiceListItem(wsText, iCursel, true);
  }

  CFX_WideString wsOldValue;
  m_pDataAcc->GetValue(wsOldValue, XFA_VALUEPICTURE_Raw);
  if (wsOldValue == wsText)
    return false;

  m_wsNewValue = wsText;
  return true;
}

void CXFA_FFComboBox::FWLEventSelChange(CXFA_EventParam* pParam) {
  pParam->m_eType = XFA_EVENT_Change;
  pParam->m_pTarget = m_pDataAcc.Get();
  pParam->m_wsNewText = ToComboBox(m_pNormalWidget.get())->GetEditText();
  m_pDataAcc->ProcessEvent(XFA_ATTRIBUTEENUM_Change, pParam);
}

uint32_t CXFA_FFComboBox::GetAlignment() {
  CXFA_Para para = m_pDataAcc->GetPara();
  if (!para)
    return 0;

  uint32_t dwExtendedStyle = 0;
  switch (para.GetHorizontalAlign()) {
    case XFA_ATTRIBUTEENUM_Center:
      dwExtendedStyle |=
          FWL_STYLEEXT_CMB_EditHCenter | FWL_STYLEEXT_CMB_ListItemCenterAlign;
      break;
    case XFA_ATTRIBUTEENUM_Justify:
      dwExtendedStyle |= FWL_STYLEEXT_CMB_EditJustified;
      break;
    case XFA_ATTRIBUTEENUM_JustifyAll:
      break;
    case XFA_ATTRIBUTEENUM_Radix:
      break;
    case XFA_ATTRIBUTEENUM_Right:
      break;
    default:
      dwExtendedStyle |=
          FWL_STYLEEXT_CMB_EditHNear | FWL_STYLEEXT_CMB_ListItemLeftAlign;
      break;
  }

  switch (para.GetVerticalAlign()) {
    case XFA_ATTRIBUTEENUM_Middle:
      dwExtendedStyle |= FWL_STYLEEXT_CMB_EditVCenter;
      break;
    case XFA_ATTRIBUTEENUM_Bottom:
      dwExtendedStyle |= FWL_STYLEEXT_CMB_EditVFar;
      break;
    default:
      dwExtendedStyle |= FWL_STYLEEXT_CMB_EditVNear;
      break;
  }
  return dwExtendedStyle;
}

bool CXFA_FFComboBox::UpdateFWLData() {
  auto* pComboBox = ToComboBox(m_pNormalWidget.get());
  if (!pComboBox)
    return false;

  std::vector<int32_t> iSelArray = m_pDataAcc->GetSelectedItems();
  if (!iSelArray.empty()) {
    pComboBox->SetCurSel(iSelArray.front());
  } else {
    CFX_WideString wsText;
    pComboBox->SetCurSel(-1);
    m_pDataAcc->GetValue(wsText, XFA_VALUEPICTURE_Raw);
    pComboBox->SetEditText(wsText);
  }
  pComboBox->Update();
  return true;
}

bool CXFA_FFComboBox::CanUndo() {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditCanUndo();
}

bool CXFA_FFComboBox::CanRedo() {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditCanRedo();
}

bool CXFA_FFComboBox::Undo() {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditUndo();
}

bool CXFA_FFComboBox::Redo() {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditRedo();
}

bool CXFA_FFComboBox::CanCopy() {
  return ToComboBox(m_pNormalWidget.get())->EditCanCopy();
}

bool CXFA_FFComboBox::CanCut() {
  return m_pDataAcc->GetAccess() == XFA_ATTRIBUTEENUM_Open &&
         m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditCanCut();
}

bool CXFA_FFComboBox::CanPaste() {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         m_pDataAcc->GetAccess() == XFA_ATTRIBUTEENUM_Open;
}

bool CXFA_FFComboBox::CanSelectAll() {
  return ToComboBox(m_pNormalWidget.get())->EditCanSelectAll();
}

bool CXFA_FFComboBox::Copy(CFX_WideString& wsCopy) {
  return ToComboBox(m_pNormalWidget.get())->EditCopy(wsCopy);
}

bool CXFA_FFComboBox::Cut(CFX_WideString& wsCut) {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditCut(wsCut);
}

bool CXFA_FFComboBox::Paste(const CFX_WideString& wsPaste) {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditPaste(wsPaste);
}

void CXFA_FFComboBox::SelectAll() {
  ToComboBox(m_pNormalWidget.get())->EditSelectAll();
}

void CXFA_FFComboBox::Delete() {
  ToComboBox(m_pNormalWidget.get())->EditDelete();
}

void CXFA_FFComboBox::DeSelect() {
  ToComboBox(m_pNormalWidget.get())->EditDeSelect();
}

void CXFA_FFComboBox::SetItemState(int32_t nIndex, bool bSelected) {
  ToComboBox(m_pNormalWidget.get())->SetCurSel(bSelected ? nIndex : -1);
  m_pNormalWidget->Update();
  AddInvalidateRect();
}

void CXFA_FFComboBox::InsertItem(const CFX_WideStringC& wsLabel,
                                 int32_t nIndex) {
  ToComboBox(m_pNormalWidget.get())->AddString(wsLabel);
  m_pNormalWidget->Update();
  AddInvalidateRect();
}

void CXFA_FFComboBox::DeleteItem(int32_t nIndex) {
  if (nIndex < 0)
    ToComboBox(m_pNormalWidget.get())->RemoveAll();
  else
    ToComboBox(m_pNormalWidget.get())->RemoveAt(nIndex);

  m_pNormalWidget->Update();
  AddInvalidateRect();
}

void CXFA_FFComboBox::OnTextChanged(CFWL_Widget* pWidget,
                                    const CFX_WideString& wsChanged) {
  CXFA_EventParam eParam;
  m_pDataAcc->GetValue(eParam.m_wsPrevText, XFA_VALUEPICTURE_Raw);
  eParam.m_wsChange = wsChanged;
  FWLEventSelChange(&eParam);
}

void CXFA_FFComboBox::OnSelectChanged(CFWL_Widget* pWidget, bool bLButtonUp) {
  CXFA_EventParam eParam;
  m_pDataAcc->GetValue(eParam.m_wsPrevText, XFA_VALUEPICTURE_Raw);
  FWLEventSelChange(&eParam);
  if (m_pDataAcc->GetChoiceListCommitOn() == XFA_ATTRIBUTEENUM_Select &&
      bLButtonUp) {
    m_pDocView->SetFocusWidgetAcc(nullptr);
  }
}

void CXFA_FFComboBox::OnPreOpen(CFWL_Widget* pWidget) {
  CXFA_EventParam eParam;
  eParam.m_eType = XFA_EVENT_PreOpen;
  eParam.m_pTarget = m_pDataAcc.Get();
  m_pDataAcc->ProcessEvent(XFA_ATTRIBUTEENUM_PreOpen, &eParam);
}

void CXFA_FFComboBox::OnPostOpen(CFWL_Widget* pWidget) {
  CXFA_EventParam eParam;
  eParam.m_eType = XFA_EVENT_PostOpen;
  eParam.m_pTarget = m_pDataAcc.Get();
  m_pDataAcc->ProcessEvent(XFA_ATTRIBUTEENUM_PostOpen, &eParam);
}

void CXFA_FFComboBox::OnProcessMessage(CFWL_Message* pMessage) {
  m_pOldDelegate->OnProcessMessage(pMessage);
}

void CXFA_FFComboBox::OnProcessEvent(CFWL_Event* pEvent) {
  CXFA_FFField::OnProcessEvent(pEvent);
  switch (pEvent->GetType()) {
    case CFWL_Event::Type::SelectChanged: {
      auto* postEvent = static_cast<CFWL_EventSelectChanged*>(pEvent);
      OnSelectChanged(m_pNormalWidget.get(), postEvent->bLButtonUp);
      break;
    }
    case CFWL_Event::Type::EditChanged: {
      CFX_WideString wsChanged;
      OnTextChanged(m_pNormalWidget.get(), wsChanged);
      break;
    }
    case CFWL_Event::Type::PreDropDown: {
      OnPreOpen(m_pNormalWidget.get());
      break;
    }
    case CFWL_Event::Type::PostDropDown: {
      OnPostOpen(m_pNormalWidget.get());
      break;
    }
    default:
      break;
  }
  m_pOldDelegate->OnProcessEvent(pEvent);
}

void CXFA_FFComboBox::OnDrawWidget(CFX_Graphics* pGraphics,
                                   const CFX_Matrix* pMatrix) {
  m_pOldDelegate->OnDrawWidget(pGraphics, pMatrix);
}
