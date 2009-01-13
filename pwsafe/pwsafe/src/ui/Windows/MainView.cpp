/*
* Copyright (c) 2003-2009 Rony Shapiro <ronys@users.sourceforge.net>.
* All rights reserved. Use of the code is allowed under the
* Artistic License 2.0 terms, as specified in the LICENSE file
* distributed with this code, or available from
* http://www.opensource.org/licenses/artistic-license-2.0.php
*/
/// file MainView.cpp
//
// View-related methods of DboxMain
//-----------------------------------------------------------------------------

#include "PasswordSafe.h"

#include "ThisMfcApp.h"

#if defined(POCKET_PC)
#include "pocketpc/resource.h"
#else
#include "resource.h"
#include "resource2.h"  // Menu, Toolbar & Accelerator resources
#include "resource3.h"  // String resources
#endif

#include "DboxMain.h"
#include "TryAgainDlg.h"
#include "ColumnChooserDlg.h"
#include "GeneralMsgBox.h"
#include "PWFontDialog.h"
#include "PWFont.h"
#include "InfoDisplay.h"
#include "ViewReport.h"
#include "corelib/pwsprefs.h"
#include "corelib/UUIDGen.h"
#include "corelib/corelib.h"
#include "corelib/PWHistory.h"
#include "corelib/Debug.h"
#include "os/dir.h"
#include "corelib/StringXStream.h"

#include "commctrl.h"
#include <shlwapi.h>
#include <vector>
#include <algorithm>
#include <sys/stat.h>

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void DboxMain::StopFind(LPARAM instance)
{
  // Callback from PWScore if the password list has been changed invalidating the 
  // indices vector
  DboxMain *self = (DboxMain*)instance;

  self->m_core.SuspendOnListNotification();
  self->InvalidateSearch();
  self->OnHideFindToolBar();
}

//-----------------------------------------------------------------------------

/*
* Compare function used by m_ctlItemList.SortItems()
* "The comparison function must return a negative value if the first item should precede 
* the second, a positive value if the first item should follow the second, or zero if
* the two items are equivalent."
*
* If sorting is by group (title/user), title (username), username (title) 
* fields in brackets are the secondary fields if the primary fields are identical.
*/
int CALLBACK DboxMain::CompareFunc(LPARAM lParam1, LPARAM lParam2,
                                   LPARAM closure)
{
  // closure is "this" of the calling DboxMain, from which we use:
  // m_iTypeSortColumn to determine which column is getting sorted
  // which is by column data TYPE and NOT by column index!
  // m_bSortAscending to determine the direction of the sort (duh)

  DboxMain *self = (DboxMain*)closure;
  const int nTypeSortColumn = self->m_iTypeSortColumn;
  CItemData* pLHS = (CItemData *)lParam1;
  CItemData* pRHS = (CItemData *)lParam2;
  StringX group1, group2;
  time_t t1, t2;
  int xint1, xint2;

  int iResult;
  switch(nTypeSortColumn) {
    case CItemData::UUID:  // Image
      iResult = (pLHS->GetEntryType() < pRHS->GetEntryType()) ? -1 : 1;
      break;
    case CItemData::GROUP:
      group1 = pLHS->GetGroup();
      group2 = pRHS->GetGroup();
      if (group1.empty())  // root?
        group1 = _T("\xff");
      if (group2.empty())  // root?
        group2 = _T("\xff");
      iResult = CompareNoCase(group1, group2);
      if (iResult == 0) {
        iResult = CompareNoCase(pLHS->GetTitle(), pRHS->GetTitle());
        if (iResult == 0) {
          iResult = CompareNoCase(pLHS->GetUser(), pRHS->GetUser());
        }
      }
      break;
    case CItemData::TITLE:
      iResult = CompareNoCase(pLHS->GetTitle(), pRHS->GetTitle());
      if (iResult == 0) {
        iResult = CompareNoCase(pLHS->GetUser(), pRHS->GetUser());
      }
      break;
    case CItemData::USER:
      iResult = CompareNoCase(pLHS->GetUser(), pRHS->GetUser());
      if (iResult == 0) {
        iResult = CompareNoCase(pLHS->GetTitle(), pRHS->GetTitle());
      }
      break;
    case CItemData::NOTES:
      iResult = CompareNoCase(pLHS->GetNotes(), pRHS->GetNotes());
      break;
    case CItemData::PASSWORD:
      iResult = CompareNoCase(pLHS->GetPassword(), pRHS->GetPassword());
      break;
    case CItemData::URL:
      iResult = CompareNoCase(pLHS->GetURL(), pRHS->GetURL());
      break;
    case CItemData::AUTOTYPE:
      iResult = CompareNoCase(pLHS->GetAutoType(), pRHS->GetAutoType());
      break;
    case CItemData::CTIME:
      pLHS->GetCTime(t1);
      pRHS->GetCTime(t2);
      iResult = ((long) t1 < (long) t2) ? -1 : 1;
      break;
    case CItemData::PMTIME:
      pLHS->GetPMTime(t1);
      pRHS->GetPMTime(t2);
      iResult = ((long) t1 < (long) t2) ? -1 : 1;
      break;
    case CItemData::ATIME:
      pLHS->GetATime(t1);
      pRHS->GetATime(t2);
      iResult = ((long) t1 < (long) t2) ? -1 : 1;
      break;
    case CItemData::XTIME:
      pLHS->GetXTime(t1);
      pRHS->GetXTime(t2);
      iResult = ((long) t1 < (long) t2) ? -1 : 1;
      break;
    case CItemData::XTIME_INT:
      pLHS->GetXTimeInt(xint1);
      pRHS->GetXTimeInt(xint2);
      iResult = (xint1 < xint2) ? -1 : 1;
      break;
    case CItemData::RMTIME:
      pLHS->GetRMTime(t1);
      pRHS->GetRMTime(t2);
      iResult = ((long) t1 < (long) t2) ? -1 : 1;
      break;
    case CItemData::POLICY:
      iResult = CompareNoCase(pLHS->GetPWPolicy(), pRHS->GetPWPolicy());
      break;
    default:
      iResult = 0; // should never happen - just keep compiler happy
      ASSERT(FALSE);
  }
  if (!self->m_bSortAscending) {
    iResult *= -1;
  }
  return iResult;
}

void DboxMain::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(DboxMain)
  DDX_Control(pDX, IDC_ITEMLIST, m_ctlItemList);
  DDX_Control(pDX, IDC_ITEMTREE, m_ctlItemTree);
  DDX_Control(pDX, IDC_STATIC_DRAGGROUP, m_DDGroup);
  DDX_Control(pDX, IDC_STATIC_DRAGTITLE, m_DDTitle);
  DDX_Control(pDX, IDC_STATIC_DRAGUSER, m_DDUser);
  DDX_Control(pDX, IDC_STATIC_DRAGPASSWORD, m_DDPassword);
  DDX_Control(pDX, IDC_STATIC_DRAGNOTES, m_DDNotes);
  DDX_Control(pDX, IDC_STATIC_DRAGURL, m_DDURL);
  //}}AFX_DATA_MAP
}

void DboxMain::UpdateToolBar(bool state)
{
  if (m_toolbarsSetup == TRUE) {
    BOOL State = (state) ? FALSE : TRUE;
    CToolBarCtrl& mainTBCtrl = m_MainToolBar.GetToolBarCtrl();
    mainTBCtrl.EnableButton(ID_MENUITEM_ADD, State);
    mainTBCtrl.EnableButton(ID_MENUITEM_DELETE, State);
    mainTBCtrl.EnableButton(ID_MENUITEM_SAVE, State);
  }
}

void DboxMain::UpdateToolBarForSelectedItem(CItemData *ci)
{
  // Following test required since this can be called on exit, with a ci
  // from ItemData that's already been deleted. Ugh.
  CItemData *entry(ci);
  if (m_core.GetNumEntries() != 0) {
    BOOL State = (entry == NULL) ? FALSE : TRUE;
    int IDs[] = {ID_MENUITEM_COPYPASSWORD, ID_MENUITEM_COPYUSERNAME,
                 ID_MENUITEM_COPYNOTESFLD, ID_MENUITEM_AUTOTYPE, ID_MENUITEM_EDIT,
                 ID_MENUITEM_PASSWORDSUBSET};

    CToolBarCtrl& mainTBCtrl = m_MainToolBar.GetToolBarCtrl();
    for (int i = 0; i < sizeof(IDs)/sizeof(IDs[0]); i++) {
      mainTBCtrl.EnableButton(IDs[i], State);
    }

    uuid_array_t entry_uuid, base_uuid;
    if (entry != NULL && entry->IsShortcut()) {
      // This is an shortcut
      entry->GetUUID(entry_uuid);
      m_core.GetShortcutBaseUUID(entry_uuid, base_uuid);

      ItemListIter iter = m_core.Find(base_uuid);
      if (iter != End()) {
        entry = &iter->second;
      }
    }

    if (entry == NULL || entry->IsURLEmpty()) {
      mainTBCtrl.EnableButton(ID_MENUITEM_BROWSEURL, FALSE);
      UpdateBrowseURLSendEmailButton(false);
    } else {
      mainTBCtrl.EnableButton(ID_MENUITEM_BROWSEURL, TRUE);
      const bool bIsEmail = entry->IsURLEmail();
      UpdateBrowseURLSendEmailButton(bIsEmail);
    }

    bool bDragBarState = PWSprefs::GetInstance()->GetPref(PWSprefs::ShowDragbar);
    if (bDragBarState) {
      // Note: Title & Password are mandatory
      if (entry == NULL) {
        m_DDGroup.SetStaticState(true);
        m_DDTitle.SetStaticState(false);
        m_DDPassword.SetStaticState(false);
        m_DDUser.SetStaticState(false);
        m_DDNotes.SetStaticState(false);
        m_DDURL.SetStaticState(false);
      } else {
        m_DDGroup.SetStaticState(!entry->IsGroupEmpty());
        m_DDTitle.SetStaticState(true);
        m_DDPassword.SetStaticState(true);
        m_DDUser.SetStaticState(!entry->IsUserEmpty());
        m_DDNotes.SetStaticState(!entry->IsNotesEmpty());
        m_DDURL.SetStaticState(!entry->IsURLEmpty());
      }
    }
  }
}

void DboxMain::setupBars()
{
#if !defined(POCKET_PC)
  // This code is copied from the DLGCBR32 example that comes with MFC

  // Add the status bar
  if (m_statusBar.Create(this)) {
    // Set up DoubleClickAction text
    const int dca = int(PWSprefs::GetInstance()->
      GetPref(PWSprefs::DoubleClickAction));
    switch (dca) {
      case PWSprefs::DoubleClickAutoType:
        statustext[CPWStatusBar::SB_DBLCLICK] = IDS_STATAUTOTYPE;
        break;
      case PWSprefs::DoubleClickBrowse:
        statustext[CPWStatusBar::SB_DBLCLICK] = IDS_STATBROWSE;
        break;
      case PWSprefs::DoubleClickCopyNotes:
        statustext[CPWStatusBar::SB_DBLCLICK] = IDS_STATCOPYNOTES;
        break;
      case PWSprefs::DoubleClickCopyPassword:
        statustext[CPWStatusBar::SB_DBLCLICK] = IDS_STATCOPYPASSWORD;
        break;
      case PWSprefs::DoubleClickCopyUsername:
        statustext[CPWStatusBar::SB_DBLCLICK] = IDS_STATCOPYUSERNAME;
        break;
      case PWSprefs::DoubleClickViewEdit:
        statustext[CPWStatusBar::SB_DBLCLICK] = IDS_STATVIEWEDIT;
        break;
	  case PWSprefs::DoubleClickCopyPasswordMinimize:
		statustext[CPWStatusBar::SB_DBLCLICK] = IDS_STATCOPYPASSWORDMIN;
		break;
      default:
        statustext[CPWStatusBar::SB_DBLCLICK] = IDS_STATCOMPANY;
    }

    statustext[CPWStatusBar::SB_CLIPBOARDACTION] = IDS_BLANK;
    // Set up Configuration source indicator (debug only)
#ifdef DEBUG
    statustext[CPWStatusBar::SB_CONFIG] = PWSprefs::GetInstance()->GetConfigIndicator();
#endif /* DEBUG */
    // Set up the rest - all but one empty as pane now re-sized according to contents
    statustext[CPWStatusBar::SB_MODIFIED] = IDS_BLANK;
    statustext[CPWStatusBar::SB_NUM_ENT] = IDS_BLANK;
    statustext[CPWStatusBar::SB_FILTER] = IDS_BLANK;
    statustext[CPWStatusBar::SB_READONLY] = IDS_READ_ONLY;

    // And show
    m_statusBar.SetIndicators(statustext, CPWStatusBar::SB_TOTAL);

    UINT uiID, uiStyle;
    int cxWidth;
    m_statusBar.GetPaneInfo(CPWStatusBar::SB_FILTER, uiID,
                            uiStyle, cxWidth);
    int iBMWidth = m_statusBar.GetBitmapWidth();
    m_statusBar.SetPaneInfo(CPWStatusBar::SB_FILTER, uiID,
                            uiStyle | SBT_OWNERDRAW, iBMWidth);

    // Make a sunken or recessed border around the first pane
    m_statusBar.SetPaneInfo(CPWStatusBar::SB_DBLCLICK, 
                            m_statusBar.GetItemID(CPWStatusBar::SB_DBLCLICK), 
                            SBPS_STRETCH, NULL);
  }

  CDC* pDC = this->GetDC();
  int NumBits = (pDC ? pDC->GetDeviceCaps(12 /*BITSPIXEL*/) : 32);
  m_MainToolBar.Init(NumBits);
  m_FindToolBar.Init(NumBits, this, WM_TOOLBAR_FIND);
  ReleaseDC(pDC);

  // Add the Main ToolBar.
  if (!m_MainToolBar.CreateEx(this, TBSTYLE_FLAT | TBSTYLE_TRANSPARENT,
                              WS_CHILD | WS_VISIBLE | CCS_ADJUSTABLE |
                              CBRS_TOP | CBRS_SIZE_DYNAMIC,
                              CRect(0, 0, 0, 0), AFX_IDW_RESIZE_BAR + 1)) {
    TRACE("Failed to create Main toolbar\n");
    return;      // fail to create
  }
  DWORD dwStyle = m_MainToolBar.GetBarStyle();
  dwStyle = dwStyle | CBRS_BORDER_BOTTOM | CBRS_BORDER_TOP |
                      CBRS_BORDER_LEFT   | CBRS_BORDER_RIGHT |
                      CBRS_TOOLTIPS | CBRS_FLYBY;
  m_MainToolBar.SetBarStyle(dwStyle);
  m_MainToolBar.SetWindowText(_T("Standard"));

  // Add the Find ToolBar.
  if (!m_FindToolBar.CreateEx(this, TBSTYLE_FLAT | TBSTYLE_TRANSPARENT,
                              WS_CHILD | WS_VISIBLE |
                              CBRS_BOTTOM | CBRS_SIZE_DYNAMIC,
                              CRect(0, 0, 0, 0), AFX_IDW_RESIZE_BAR + 2)) {
    TRACE("Failed to create Find toolbar\n");
    return;      // fail to create
  }
  dwStyle = m_FindToolBar.GetBarStyle();
  dwStyle = dwStyle | CBRS_BORDER_BOTTOM | CBRS_BORDER_TOP |
                      CBRS_BORDER_LEFT   | CBRS_BORDER_RIGHT |
                      CBRS_TOOLTIPS | CBRS_FLYBY;
  m_FindToolBar.SetBarStyle(dwStyle);
  m_FindToolBar.SetWindowText(_T("Find"));

  // Set up DragBar bitmaps before calling SetToolBar
  m_DDGroup.Init(IDB_DRAGGROUP, IDB_DRAGGROUPX);
  m_DDTitle.Init(IDB_DRAGTITLE, IDB_DRAGTITLEX);
  m_DDUser.Init(IDB_DRAGUSER, IDB_DRAGUSERX);
  m_DDPassword.Init(IDB_DRAGPASSWORD, IDB_DRAGPASSWORDX);
  m_DDNotes.Init(IDB_DRAGNOTES, IDB_DRAGNOTESX);
  m_DDURL.Init(IDB_DRAGURL, IDB_DRAGURLX);

  // Set toolbar according to graphic capabilities, overridable by user choice.
  if (NumBits < 16 || !PWSprefs::GetInstance()->GetPref(PWSprefs::UseNewToolbar))  {
    SetToolbar(ID_MENUITEM_OLD_TOOLBAR, true);
  } else {
    SetToolbar(ID_MENUITEM_NEW_TOOLBAR, true);
  }

  m_FindToolBar.ShowFindToolBar(false);

  // Set flag - we're done
  m_toolbarsSetup = TRUE;
  m_MainToolBar.ShowWindow(PWSprefs::GetInstance()->GetPref(PWSprefs::ShowToolbar) ?
                           SW_SHOW : SW_HIDE);
  UpdateToolBar(m_core.IsReadOnly());
  m_menuManager.SetImageList(&m_MainToolBar);
  m_menuManager.SetMapping(&m_MainToolBar);

  // Register for update notification
  m_core.RegisterOnListModified(StopFind, (LPARAM)this);

  m_DDGroup.EnableWindow(TRUE);
  m_DDGroup.ShowWindow(SW_SHOW);
  m_DDTitle.EnableWindow(TRUE);
  m_DDTitle.ShowWindow(SW_SHOW);
  m_DDUser.EnableWindow(TRUE);
  m_DDUser.ShowWindow(SW_SHOW);
  m_DDPassword.EnableWindow(TRUE);
  m_DDPassword.ShowWindow(SW_SHOW);
  m_DDNotes.EnableWindow(TRUE);
  m_DDNotes.ShowWindow(SW_SHOW);
  m_DDURL.EnableWindow(TRUE);
  m_DDURL.ShowWindow(SW_SHOW);
#endif
}

void DboxMain::UpdateListItem(const int lindex, const int type, const CString &newText)
{
  int iSubItem = m_nColumnIndexByType[type];

  // Ignore if this column is not being displayed
  if (iSubItem < 0)
    return;

  BOOL brc = m_ctlItemList.SetItemText(lindex, iSubItem, newText);
  ASSERT(brc == TRUE);
  if (m_iTypeSortColumn == type) { // resort if necessary
    m_bSortAscending = PWSprefs::GetInstance()->GetPref(PWSprefs::SortAscending);
    m_ctlItemList.SortItems(CompareFunc, (LPARAM)this);
    FixListIndexes();
  }
}

// Find in m_pwlist entry with same title and user name as the i'th entry in m_ctlItemList
ItemListIter DboxMain::Find(int i)
{
  CItemData *ci = (CItemData *)m_ctlItemList.GetItemData(i);
  ASSERT(ci != NULL);
  const StringX curGroup = ci->GetGroup();
  const StringX curTitle = ci->GetTitle();
  const StringX curUser = ci->GetUser();
  return Find(curGroup, curTitle, curUser);
}

/*
* Finds all entries in m_pwlist that contain str in any text
* field, returns their sorted indices in m_listctrl via indices, which is
* assumed to be allocated by caller to DboxMain::GetNumEntries() ints.
* FindAll returns the number of entries that matched.
*/

size_t DboxMain::FindAll(const CString &str, BOOL CaseSensitive,
                         vector<int> &indices)
{
  CItemData::FieldBits bsFields;
  bsFields.set();  // Default search is all text fields!

  return FindAll(str, CaseSensitive, indices, bsFields, BST_UNCHECKED, 
                 _T(""), 0, 0);
}

size_t DboxMain::FindAll(const CString &str, BOOL CaseSensitive,
                         vector<int> &indices,
                         const CItemData::FieldBits &bsFields, const int subgroup_set, 
                         const CString &subgroup_name, const int subgroup_object,
                         const int subgroup_function)
{
  ASSERT(!str.IsEmpty());
  ASSERT(indices.empty());

  StringX curGroup, curTitle, curUser, curNotes, curPassword, curURL, curAT, curXInt;
  StringX listTitle, saveTitle;
  bool bFoundit;
  CString searchstr(str); // Since str is const, and we might need to MakeLower
  size_t retval = 0;

  if (!CaseSensitive)
    searchstr.MakeLower();

  int ititle(-1);  // Must be there as it is mandatory!
  for (int ic = 0; ic < m_nColumns; ic++) {
    if (m_nColumnTypeByIndex[ic] == CItemData::TITLE) {
      ititle = ic;
      break;
    }
  }

  ItemListConstIter listPos, listEnd;

  OrderedItemList orderedItemList;
  OrderedItemList::const_iterator olistPos, olistEnd;
  if (m_IsListView) {
    listPos = m_core.GetEntryIter();
    listEnd = m_core.GetEntryEndIter();
  } else {
    MakeOrderedItemList(orderedItemList);
    olistPos = orderedItemList.begin();
    olistEnd = orderedItemList.end();
  }

  while (m_IsListView ? (listPos != listEnd) : (olistPos != olistEnd)) {
    const CItemData &curitem = m_IsListView ? listPos->second : *olistPos;
    if (subgroup_set == BST_CHECKED &&
        !curitem.Matches(stringT(subgroup_name),
                         subgroup_object, subgroup_function))
      goto nextentry;

    bFoundit = false;
    saveTitle = curTitle = curitem.GetTitle(); // savetitle keeps orig case
    curGroup = curitem.GetGroup();
    curUser =  curitem.GetUser();
    curPassword = curitem.GetPassword();
    curNotes = curitem.GetNotes();
    curURL = curitem.GetURL();
    curAT = curitem.GetAutoType();
    curXInt = curitem.GetXTimeInt();

    if (!CaseSensitive) {
      ToLower(curGroup);
      ToLower(curTitle);
      ToLower(curUser);
      ToLower(curPassword);
      ToLower(curNotes);
      ToLower(curURL);
      ToLower(curAT);
    }

    // do loop to easily break out as soon as a match is found
    // saves more checking if entry already selected
    do {
      if (bsFields.test(CItemData::GROUP) && ::_tcsstr(curGroup.c_str(), searchstr)) {
        bFoundit = true;
        break;
      }
      if (bsFields.test(CItemData::TITLE) && ::_tcsstr(curTitle.c_str(), searchstr)) {
        bFoundit = true;
        break;
      }
      if (bsFields.test(CItemData::USER) && ::_tcsstr(curUser.c_str(), searchstr)) {
        bFoundit = true;
        break;
      }
      if (bsFields.test(CItemData::PASSWORD) && ::_tcsstr(curPassword.c_str(), searchstr)) {
        bFoundit = true;
        break;
      }
      if (bsFields.test(CItemData::NOTES) && ::_tcsstr(curNotes.c_str(), searchstr)) {
        bFoundit = true;
        break;
      }
      if (bsFields.test(CItemData::URL) && ::_tcsstr(curURL.c_str(), searchstr)) {
        bFoundit = true;
        break;
      }
      if (bsFields.test(CItemData::AUTOTYPE) && ::_tcsstr(curAT.c_str(), searchstr)) {
        bFoundit = true;
        break;
      }
      if (bsFields.test(CItemData::PWHIST)) {
        size_t pwh_max, err_num;
        PWHistList pwhistlist;
        CreatePWHistoryList(curitem.GetPWHistory(), pwh_max, err_num,
                            pwhistlist, TMC_XML);
        PWHistList::iterator iter;
        for (iter = pwhistlist.begin(); iter != pwhistlist.end();
             iter++) {
          PWHistEntry pwshe = *iter;
          if (!CaseSensitive)
            ToLower(pwshe.password);
          if (::_tcsstr(pwshe.password.c_str(), searchstr)) {
            bFoundit = true;
            break;  // break out of for loop
          }
        }
        pwhistlist.clear();
        if (bFoundit)
          break;  // break out of do loop
      }
      if (bsFields.test(CItemData::XTIME_INT) && ::_tcsstr(curXInt.c_str(), searchstr)) {
        bFoundit = true;
        break;
      }
    } while(FALSE);  // only do it once!

    if (bFoundit) {
      // Find index in displayed list
      DisplayInfo *di = (DisplayInfo *)curitem.GetDisplayInfo();
      ASSERT(di != NULL);
      int li = di->list_index;
      ASSERT(m_ctlItemList.GetItemText(li, ititle) == saveTitle.c_str());
      // add to indices, bump retval
      indices.push_back(li);
    } // match found in m_pwlist

nextentry:
    if (m_IsListView)
      listPos++;
    else
      olistPos++;
  } // while

  retval = indices.size();
  // Sort indices if in List View
  if (m_IsListView && retval > 1)
    sort(indices.begin(), indices.end());

  if (!m_IsListView)
    orderedItemList.clear();

  return retval;
}

//Checks and sees if everything works and something is selected
BOOL DboxMain::SelItemOk()
{
  CItemData *ci = getSelectedItem();
  return (ci == NULL) ? FALSE : TRUE;
}

BOOL DboxMain::SelectEntry(int i, BOOL MakeVisible)
{
  BOOL retval;
  if (m_ctlItemList.GetItemCount() == 0)
    return false;

  if (m_ctlItemList.IsWindowVisible()) {
    retval = m_ctlItemList.SetItemState(i,
                                        LVIS_FOCUSED | LVIS_SELECTED,
                                        LVIS_FOCUSED | LVIS_SELECTED);
    if (MakeVisible) {
      m_ctlItemList.EnsureVisible(i, FALSE);
    }
    m_ctlItemList.Invalidate();
  } else { //Tree view active
    CItemData *ci = (CItemData *)m_ctlItemList.GetItemData(i);
    ASSERT(ci != NULL);
    DisplayInfo *di = (DisplayInfo *)ci->GetDisplayInfo();
    ASSERT(di != NULL);
    ASSERT(di->list_index == i);

    // Was there anything selected before?
    HTREEITEM hti = m_ctlItemTree.GetSelectedItem();
    // NULL means nothing was selected.
    if (hti != NULL) {
      // Time to remove the old "fake selection" (a.k.a. drop-hilite)
      // Make sure to undo "MakeVisible" on the previous selection.
      m_ctlItemTree.SetItemState(hti, 0, TVIS_DROPHILITED);
    }

    retval = m_ctlItemTree.SelectItem(di->tree_item);
    if (MakeVisible) {
      // Following needed to show selection when Find dbox has focus. Ugh.
      m_ctlItemTree.SetItemState(di->tree_item,
                                 TVIS_DROPHILITED | TVIS_SELECTED,
                                 TVIS_DROPHILITED | TVIS_SELECTED);
    }
    m_ctlItemTree.Invalidate();
  }
  return retval;
}

void DboxMain::SelectFirstEntry()
{
  if (m_core.GetNumEntries() > 0) {
    // Ensure an entry is selected after open
    CItemData *pci(NULL);
    if (m_ctlItemList.IsWindowVisible()) {
      m_ctlItemList.SetItemState(0,
                                 LVIS_FOCUSED | LVIS_SELECTED,
                                 LVIS_FOCUSED | LVIS_SELECTED);
      m_ctlItemList.EnsureVisible(0, FALSE);
      pci = (CItemData *)m_ctlItemList.GetItemData(0);
    } else {
      HTREEITEM hitem = m_ctlItemTree.GetFirstVisibleItem();
      if (hitem != NULL) {
        m_ctlItemTree.SelectItem(hitem);
        pci = (CItemData *)m_ctlItemTree.GetItemData(hitem);
      }
    }
    if (pci != NULL)
      UpdateToolBarForSelectedItem(pci);
  }
}

BOOL DboxMain::SelectFindEntry(int i, BOOL MakeVisible)
{
  BOOL retval;
  if (m_ctlItemList.GetItemCount() == 0)
    return FALSE;

  if (m_ctlItemList.IsWindowVisible()) {
    retval = m_ctlItemList.SetItemState(i,
                                        LVIS_FOCUSED | LVIS_SELECTED,
                                        LVIS_FOCUSED | LVIS_SELECTED);
    m_LastFoundListItem = i;
    if (MakeVisible) {
      m_ctlItemList.EnsureVisible(i, FALSE);
    }
    m_ctlItemList.Invalidate();
  } else { //Tree view active
    CItemData *ci = (CItemData *)m_ctlItemList.GetItemData(i);
    ASSERT(ci != NULL);
    DisplayInfo *di = (DisplayInfo *)ci->GetDisplayInfo();
    ASSERT(di != NULL);
    ASSERT(di->list_index == i);

    UnFindItem();

    retval = m_ctlItemTree.SelectItem(di->tree_item);
    if (MakeVisible) {
      m_ctlItemTree.SetItemState(di->tree_item, TVIS_BOLD, TVIS_BOLD);
      m_LastFoundTreeItem = di->tree_item;
      m_bBoldItem = true;
    }
    m_ctlItemTree.Invalidate();
  }
  return retval;
}

// Updates m_ctlItemList and m_ctlItemTree from m_pwlist
// updates of windows suspended until all data is in.
void DboxMain::RefreshViews(const int iView)
{
  if (!m_windowok)
    return;

#if defined(POCKET_PC)
  HCURSOR waitCursor = app.LoadStandardCursor(IDC_WAIT);
#endif

  m_bNumPassedFiltering = 0;

  // Get current selected items and save ptr to the entries (unchanged over 
  // refresh of Tree/List
  POSITION pSelected = m_ctlItemList.GetFirstSelectedItemPosition();
  HTREEITEM hSelected = m_ctlItemTree.GetSelectedItem();
  CItemData *ciList(NULL), *ciTree(NULL);
  if (pSelected != NULL)
    ciList = (CItemData *)m_ctlItemList.GetItemData((int)pSelected - 1);
  if (hSelected != NULL)
    ciTree = (CItemData *)m_ctlItemTree.GetItemData(hSelected);

  // Save expand/collapse status of groups
  vector <bool> displaystatus = GetGroupDisplayStatus();

  // can't use LockWindowUpdate 'cause only one window at a time can be locked
  if (iView & iListOnly) {
    m_ctlItemList.SetRedraw(FALSE);
    m_ctlItemList.DeleteAllItems();
  }
  if (iView & iTreeOnly) {
    m_ctlItemTree.SetRedraw(FALSE);
    m_ctlItemTree.DeleteAllItems();
  }
  m_bBoldItem = false;

  if (m_core.GetNumEntries() != 0) {
    ItemListIter listPos;
#if defined(POCKET_PC)
    SetCursor(waitCursor);
#endif
    for (listPos = m_core.GetEntryIter(); listPos != m_core.GetEntryEndIter();
         listPos++) {
      CItemData &ci = m_core.GetEntry(listPos);
      DisplayInfo *di = (DisplayInfo *)ci.GetDisplayInfo();
      if (di != NULL)
        di->list_index = -1; // easier, but less efficient, to delete di
      insertItem(ci, -1, false, iView);
    }

    m_ctlItemTree.SortTree(TVI_ROOT);
    SortListView();

#if defined(POCKET_PC)
    SetCursor( NULL );
#endif
  } // we have entries

  if (m_bImageInLV) {
    m_ctlItemList.SetColumnWidth(0, LVSCW_AUTOSIZE);
  }

  // re-enable and force redraw!
  if (iView & iListOnly) {
    m_ctlItemList.SetRedraw( TRUE ); 
    m_ctlItemList.Invalidate();
  }
  if (iView & iTreeOnly) {
    m_ctlItemTree.SetRedraw( TRUE );
    m_ctlItemTree.Invalidate();
  }

  // FixListIndexes(); // No need as just added them all in again!

  // Restore expand/collapse status of groups
  SetGroupDisplayStatus(displaystatus);

  // Select previously selected items and ensure they are visible.
  // Note: di->list_index and di->tree_item will have been changed by
  // "insertItem" and "FixListIndexes" above.
  if (ciList != NULL) {
    DisplayInfo *di = (DisplayInfo *)ciList->GetDisplayInfo();
    m_ctlItemList.SetItemState(di->list_index,
                               LVIS_FOCUSED | LVIS_SELECTED,
                               LVIS_FOCUSED | LVIS_SELECTED);
    m_ctlItemList.EnsureVisible(di->list_index, FALSE);
  }
  if (ciTree != NULL) {
    DisplayInfo *di = (DisplayInfo *)ciTree->GetDisplayInfo();
    m_ctlItemTree.SelectItem(di->tree_item);
    m_ctlItemTree.EnsureVisible(di->tree_item);
  }
  if (m_bFilterActive)
    UpdateStatusBar();
}

void DboxMain::OnSize(UINT nType, int cx, int cy) 
{
  // Note that onsize runs before InitDialog (Gee, I love MFC)
  //  Also, OnSize is called AFTER the function has been peformed.
  //  To verify IF the function should be done at all, it must be checked in OnSysCommand.
  CDialog::OnSize(nType, cx, cy);

  if (m_windowok) {
    // Position the control bars
    CRect rect, dragrect;
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0, reposQuery, &rect);
    bool bDragBarState = PWSprefs::GetInstance()->GetPref(PWSprefs::ShowDragbar);
    if (bDragBarState) {
      const int i = GetSystemMetrics(SM_CYBORDER);
      m_DDGroup.GetWindowRect(&dragrect);
      ScreenToClient(&dragrect);
      const int j = rect.top + i;
      m_DDGroup.SetWindowPos(NULL, dragrect.left, j, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
      m_DDTitle.GetWindowRect(&dragrect);
      ScreenToClient(&dragrect);
      m_DDTitle.SetWindowPos(NULL, dragrect.left, j, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
      m_DDUser.GetWindowRect(&dragrect);
      ScreenToClient(&dragrect);
      m_DDUser.SetWindowPos(NULL, dragrect.left, j, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
      m_DDPassword.GetWindowRect(&dragrect);
      ScreenToClient(&dragrect);
      m_DDPassword.SetWindowPos(NULL, dragrect.left, j, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
      m_DDNotes.GetWindowRect(&dragrect);
      ScreenToClient(&dragrect);
      m_DDNotes.SetWindowPos(NULL, dragrect.left, j, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
      m_DDURL.GetWindowRect(&dragrect);
      ScreenToClient(&dragrect);
      m_DDURL.SetWindowPos(NULL, dragrect.left, j, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
      rect.top += dragrect.Height() + 2 * i;
    }
    m_ctlItemList.MoveWindow(&rect, TRUE);
    m_ctlItemTree.MoveWindow(&rect, TRUE);
  } else
    return;

  // {kjp} Only SIZE_RESTORED is supported on Pocket PC.
#if !defined(POCKET_PC)
  if (nType == SIZE_MINIMIZED) {
    // Called when minimize button select on main dialog control box
    // or by right clicking in the Taskbar (not using System Tray)
    PWSprefs *prefs = PWSprefs::GetInstance();

    // Suspend notification of changes
    m_core.SuspendOnListNotification();

    m_selectedAtMinimize = getSelectedItem();
    m_ctlItemList.DeleteAllItems();
    m_ctlItemTree.DeleteAllItems();
    m_bBoldItem = false;

    if (prefs->GetPref(PWSprefs::ClearClipboardOnMinimize))
      OnClearClipboard();

    if (prefs->GetPref(PWSprefs::DatabaseClear)) {
      if (m_core.IsChanged() ||  m_bTSUpdated)
        if (Save() != PWScore::SUCCESS) {
          // If we don't warn the user, data may be lost!
          CString cs_text(MAKEINTRESOURCE(IDS_COULDNOTSAVE)), 
            cs_title(MAKEINTRESOURCE(IDS_SAVEERROR));
          MessageBox(cs_text, cs_title, MB_ICONSTOP);
          ShowWindow(SW_SHOW);
          return;
        }
        ClearData(false);
    }
    if (PWSprefs::GetInstance()->GetPref(PWSprefs::UseSystemTray)) {      
      app.SetMenuDefaultItem(ID_MENUITEM_UNMINIMIZE);
      ShowWindow(SW_HIDE);
    }
  } else if (nType == SIZE_MAXIMIZED) {
    RefreshViews();
  } else if (nType == SIZE_RESTORED) {
    if (!m_bSizing) { // here if actually restored
#endif
      app.SetMenuDefaultItem(ID_MENUITEM_MINIMIZE);
      UnMinimize(false);
      m_ctlItemTree.SetRestoreMode(true);
      m_bIsRestoring = true;
      RefreshViews();
      if (m_selectedAtMinimize != NULL)
        SelectEntry(((DisplayInfo *)m_selectedAtMinimize->GetDisplayInfo())->list_index, false);
      m_ctlItemTree.SetRestoreMode(false);
      m_bIsRestoring = false;

      // Resume notification of changes
      m_core.ResumeOnListNotification();
      if (m_FindToolBar.IsVisible()) {
        SetFindToolBar(true);
      }
#if !defined(POCKET_PC)
    } else { // m_bSizing == true: here if size changed
      CRect rect;
      GetWindowRect(&rect);
      PWSprefs::GetInstance()->SetPrefRect(rect.top, rect.bottom,
                                           rect.left, rect.right);

      // Make sure Find toolbar is above Status bar
      if (m_FindToolBar.IsVisible()) {
        SetToolBarPositions();
      }
    }
  } // nType == SIZE_RESTORED
#endif
  m_bSizing = false;
}

// Called when right-click is invoked in the client area of the window.
void DboxMain::OnContextMenu(CWnd* /* pWnd */, CPoint screen) 
{
#if defined(POCKET_PC)
  const DWORD dwTrackPopupFlags = TPM_LEFTALIGN;
#else
  const DWORD dwTrackPopupFlags = TPM_LEFTALIGN | TPM_RIGHTBUTTON;
#endif

  CPoint client;
  int item = -1;
  CItemData *pci = NULL;
  CMenu menu;

  MENUINFO minfo;
  memset(&minfo, 0x00, sizeof(minfo));
  minfo.cbSize = sizeof(MENUINFO);
  minfo.fMask = MIM_MENUDATA;
  
  // Note if point = (-1, -1) then invoked via keyboard.
  // Need coordinates of current selected itme instead on mouse position when message sent
  bool bKeyboard = (screen.x == -1 && screen.y == -1);

  CPoint mp; // Screen co-ords (from "message point" or via Shift+F10 selected item
  CRect rect, appl_rect;

  // Get client window position
  if (bKeyboard) {
    CRect r;
    if (m_ctlItemList.IsWindowVisible()) {
      POSITION pos = m_ctlItemList.GetFirstSelectedItemPosition();
      if (pos == NULL)
        return;  // Nothing selected!

      int nIndex = m_ctlItemList.GetNextSelectedItem(pos);
      m_ctlItemList.GetItemRect(nIndex, &r, LVIR_LABEL);
      m_ctlItemList.ClientToScreen(&r);
    } else
    if (m_ctlItemTree.IsWindowVisible()) {
      HTREEITEM hItem = m_ctlItemTree.GetSelectedItem();
      if (hItem == NULL)
        return;  // Nothing selected!

      m_ctlItemTree.GetItemRect(hItem, &r, TRUE);
      m_ctlItemTree.ClientToScreen(&r);
    }
    mp.x = (r.left + r.right) / 2;
    mp.y = (r.top + r.bottom) / 2;
    screen = mp;  // In screen co-ords
  } else {
    mp = ::GetMessagePos();
  }
  GetWindowRect(&appl_rect);
  m_MainToolBar.GetWindowRect(&rect);

  // RClick over Main Toolbar - allow Customize Main toolbar
  if (mp.x > appl_rect.left && mp.x < appl_rect.right &&
      mp.y > rect.top && mp.y < rect.bottom) {
    if (menu.LoadMenu(IDR_POPCUSTOMIZETOOLBAR)) {
      minfo.dwMenuData = IDR_POPCUSTOMIZETOOLBAR;
      menu.SetMenuInfo(&minfo);
      CMenu* pPopup = menu.GetSubMenu(0);
      ASSERT(pPopup != NULL);
      pPopup->TrackPopupMenu(dwTrackPopupFlags, screen.x, screen.y, this); // use this window for commands
    }
    return;
  }

  client = screen;
  // RClick over ListView
  if (m_ctlItemList.IsWindowVisible()) {
    // currently in flattened list view.
    m_ctlItemList.GetWindowRect(&rect);
    if (mp.x < rect.left || mp.x > appl_rect.right ||
        mp.y < rect.top || mp.y > rect.bottom) {
      // But not in the window
      return;
    }

    m_ctlItemList.ScreenToClient(&client);
    item = m_ctlItemList.HitTest(client);
    if (item < 0)
      return; // right click on empty list

    pci = (CItemData *)m_ctlItemList.GetItemData(item);
    if (SelectEntry(item) == 0) {
      return;
    }
    m_ctlItemList.SetFocus();
  }

  // RClick over TreeView
  if (m_ctlItemTree.IsWindowVisible()) {
    // currently in tree view
    m_ctlItemTree.GetWindowRect(&rect);
    if (mp.x < rect.left || mp.x > appl_rect.right ||
        mp.y < rect.top || mp.y > rect.bottom) {
      // But not in the window
      return;
    }
    ASSERT(m_ctlItemTree.IsWindowVisible());
    m_ctlItemTree.ScreenToClient(&client);
    HTREEITEM ti = m_ctlItemTree.HitTest(client);
    if (ti != NULL) {
      pci = (CItemData *)m_ctlItemTree.GetItemData(ti);
      if (pci != NULL) {
        // right-click was on an item (LEAF of some kind: normal, alias, shortcut)
        DisplayInfo *di = (DisplayInfo *)pci->GetDisplayInfo();
        ASSERT(di != NULL);
        ASSERT(di->tree_item == ti);
        item = di->list_index;
        m_ctlItemTree.SelectItem(ti); // So that OnEdit gets the right one
      } else {
        // right-click was on a group (NODE)
        m_ctlItemTree.SelectItem(ti); 
        if (menu.LoadMenu(IDR_POPEDITGROUP)) {
          minfo.dwMenuData = IDR_POPEDITGROUP;
          menu.SetMenuInfo(&minfo);

          CMenu* pPopup = menu.GetSubMenu(0);
          ASSERT(pPopup != NULL);
          m_TreeViewGroup = m_ctlItemTree.GetGroup(ti);
          // use this DboxMain for commands
          pPopup->TrackPopupMenu(dwTrackPopupFlags, screen.x, screen.y, this);
        }
      }
    } else {
      // not over anything
      if (menu.LoadMenu(IDR_POPTREE)) {  // "Add Group"
        minfo.dwMenuData = IDR_POPTREE;
        menu.SetMenuInfo(&minfo);
        CMenu* pPopup = menu.GetSubMenu(0);
        ASSERT(pPopup != NULL);
        // use this DboxMain for commands
        pPopup->TrackPopupMenu(dwTrackPopupFlags, screen.x, screen.y, this);
      }
    }
    m_ctlItemTree.SetFocus();
  } // tree view handling

  // RClick over an entry
  if (item >= 0) {
    menu.LoadMenu(IDR_POPEDITMENU);
    minfo.dwMenuData = IDR_POPEDITMENU;
    menu.SetMenuInfo(&minfo);

    CMenu* pPopup = menu.GetSubMenu(0);
    ASSERT(pPopup != NULL);
    ASSERT(pci != NULL);

    CItemData::EntryType etype = pci->GetEntryType();
    switch (etype) {
      case CItemData::ET_NORMAL:
      case CItemData::ET_SHORTCUTBASE:
        pPopup->ModifyMenu(ID_MENUITEM_GOTOBASEENTRY, MF_BYCOMMAND,
                           ID_MENUITEM_CREATESHORTCUT, CS_CREATESHORTCUT);
        break;
      case CItemData::ET_ALIASBASE:
        pPopup->RemoveMenu(ID_MENUITEM_CREATESHORTCUT, MF_BYCOMMAND);
        pPopup->RemoveMenu(ID_MENUITEM_GOTOBASEENTRY, MF_BYCOMMAND);
        break;
      case CItemData::ET_ALIAS:
      case CItemData::ET_SHORTCUT:
        pPopup->ModifyMenu(ID_MENUITEM_CREATESHORTCUT, MF_BYCOMMAND,
                           ID_MENUITEM_GOTOBASEENTRY, CS_GOTOBASEENTRY);
        break;
      default:
        ASSERT(0);
    }

    uuid_array_t entry_uuid, base_uuid;
    if (pci->IsShortcut()) {
      // This is an shortcut
      pci->GetUUID(entry_uuid);
      m_core.GetShortcutBaseUUID(entry_uuid, base_uuid);

      ItemListIter iter = m_core.Find(base_uuid);
      if (iter != End()) {
        pci = &iter->second;
      }
    }

    if (pci->IsURLEmpty()) {
      pPopup->ModifyMenu(ID_MENUITEM_SENDEMAIL, MF_BYCOMMAND,
                         ID_MENUITEM_BROWSEURL, CS_BROWSEURL);
      pPopup->ModifyMenu(ID_MENUITEM_COPYEMAIL, MF_BYCOMMAND,
                         ID_MENUITEM_COPYURL, CS_COPYURL);
      pPopup->EnableMenuItem(ID_MENUITEM_BROWSEURL, MF_GRAYED);
      pPopup->EnableMenuItem(ID_MENUITEM_COPYURL, MF_GRAYED);
      UpdateBrowseURLSendEmailButton(false);
    } else {
      pPopup->EnableMenuItem(ID_MENUITEM_BROWSEURL, MF_ENABLED);
      pPopup->EnableMenuItem(ID_MENUITEM_COPYURL, MF_ENABLED);
      const bool bIsEmail = pci->IsURLEmail();
      if (bIsEmail) {
        pPopup->ModifyMenu(ID_MENUITEM_BROWSEURL, MF_BYCOMMAND,
                           ID_MENUITEM_SENDEMAIL, CS_SENDEMAIL);
        pPopup->ModifyMenu(ID_MENUITEM_COPYURL, MF_BYCOMMAND,
                           ID_MENUITEM_COPYEMAIL, CS_COPYEMAIL);
      } else {
        pPopup->ModifyMenu(ID_MENUITEM_SENDEMAIL, MF_BYCOMMAND,
                           ID_MENUITEM_BROWSEURL, CS_BROWSEURL);
        pPopup->ModifyMenu(ID_MENUITEM_COPYEMAIL, MF_BYCOMMAND,
                           ID_MENUITEM_COPYURL, CS_COPYURL);
      }
      UpdateBrowseURLSendEmailButton(bIsEmail);
    }

    // use this DboxMain for commands
    pPopup->TrackPopupMenu(dwTrackPopupFlags, screen.x, screen.y, this);
  } // if (item >= 0)
}

void DboxMain::OnListItemSelected(NMHDR *pNotifyStruct, LRESULT *pLResult)
{
  *pLResult = 0L;
  NMITEMACTIVATE *plv = (NMITEMACTIVATE *)pNotifyStruct;

  int item = plv->iItem;
  if (item != -1) { // -1 if nothing selected, e.g., empty list
    CItemData *cilist = (CItemData *)m_ctlItemList.GetItemData(item);
    UpdateToolBarForSelectedItem(cilist);
  }

  m_LastFoundTreeItem = NULL;
  m_LastFoundListItem = -1;
}

void DboxMain::OnTreeItemSelected(NMHDR * /*pNotifyStruct */, LRESULT *pLResult)
{
  // Seems that under Vista with Windows Common Controls V6, it is ignoring
  // the single click on the button (+/-) of a node and only processing the 
  // double click, which generates a copy of whatever the user selected
  // for a double click (except that it invalid for a node!) and then does
  // the expand/collapse as appropriate.
  // This codes attemts to fix this.  There may be better solutions but I 
  // don't know them and have very limited testing facilities on Vista.

  UnFindItem();
  m_LastFoundTreeItem = NULL;
  m_LastFoundListItem = -1;

  *pLResult = 0L;
  TVHITTESTINFO htinfo = {0};

  CPoint local;
  local = ::GetMessagePos();
  m_ctlItemTree.ScreenToClient(&local);
  htinfo.pt = local;
  m_ctlItemTree.HitTest(&htinfo);

  if (htinfo.hItem != NULL && 
      (htinfo.flags & (TVHT_ONITEMINDENT | TVHT_ONITEMBUTTON)) &&
      !m_ctlItemTree.IsLeaf(htinfo.hItem)) {
    m_ctlItemTree.Expand(htinfo.hItem, TVE_TOGGLE);
    *pLResult = 1L;  // We did it!
  }
}

void DboxMain::OnKeydownItemlist(NMHDR* pNMHDR, LRESULT* pResult)
{
  LV_KEYDOWN *pLVKeyDown = (LV_KEYDOWN*)pNMHDR;

  // TRUE = we have processed the key stroke - don't call anyone else
  *pResult = TRUE;

  switch (pLVKeyDown->wVKey) {
    case VK_DELETE:
      OnDelete();
      return;
    case VK_INSERT:
      OnAdd();
      return;
    case VK_ADD:
      if ((GetKeyState(VK_CONTROL) & 0x8000) == 0x8000) {
        SetHeaderInfo();
        return;
      }
      break;
    default:
      break;    
  }

  // FALSE = call next in line to process event
  *pResult = FALSE;
}

#if !defined(POCKET_PC)
void DboxMain::OnChangeItemFocus(NMHDR* /* pNMHDR */, LRESULT* /* pResult */) 
{
  // Called on NM_{SET,KILL}FOCUS for IDC_ITEM{LIST,TREE}
  // Seems excessive to do this all the time
  // Should be done only on Open and on Change (Add, Delete, Modify)
  if (m_toolbarsSetup == TRUE)
    UpdateStatusBar();
}
#endif

////////////////////////////////////////////////////////////////////////////////
// NOTE!
// itemData must be the actual item in the item list.  if the item is remove
// from the list, it must be removed from the display as well and vice versa.
// a pointer is associated with the item in the display that is used for
// sorting.
// {kjp} We could use itemData.GetNotes(CString&) to reduce the number of
// {kjp} temporary objects created and copied.
//
int DboxMain::insertItem(CItemData &itemData, int iIndex, 
                         const bool bSort, const int iView)
{
  if (itemData.GetDisplayInfo() != NULL &&
      ((DisplayInfo *)itemData.GetDisplayInfo())->list_index != -1) {
    // true iff item already displayed
    return iIndex;
  }

  int iResult = iIndex;
  if (iResult < 0) {
    iResult = m_ctlItemList.GetItemCount();
  }

  DisplayInfo *di = (DisplayInfo *)itemData.GetDisplayInfo();
  if (di == NULL) {
    di = new DisplayInfo;
    itemData.SetDisplayInfo((void *)di);
  }
  di->list_index = -1;
  di->tree_item = NULL;

  if (m_bFilterActive) {
    if (!PassesFiltering(itemData, m_currentfilter))
      return -1;
    m_bNumPassedFiltering++;
  }

  int nImage = GetEntryImage(itemData);
  StringX group = itemData.GetGroup();
  StringX title = itemData.GetTitle();
  StringX username = itemData.GetUser();
  // get only the first line for display
  StringX strNotes = itemData.GetNotes();
  StringX::size_type iEOL = strNotes.find(TCHAR('\r'));
  if (iEOL != StringX::npos) {
    StringX strTemp = strNotes.substr(0, iEOL);
    strNotes = strTemp;
  }
  StringX cs_fielddata;

  int xint;
  itemData.GetXTimeInt(xint);

  if (iView & iListOnly) {
    // Insert the first column data
    switch (m_nColumnTypeByIndex[0]) {
      case CItemData::UUID:
        cs_fielddata = _T("");
        break;
      case CItemData::GROUP:
        cs_fielddata = group;
        break;
      case CItemData::TITLE:
        cs_fielddata = title;
        break;
      case CItemData::USER:
        cs_fielddata = username;
        break;
      case CItemData::NOTES:
        cs_fielddata = strNotes;
        break;
      case CItemData::PASSWORD:
        cs_fielddata = itemData.GetPassword();
        break;
      case CItemData::URL:
        cs_fielddata = itemData.GetURL();
        break;
      case CItemData::CTIME:
        cs_fielddata = itemData.GetCTimeL();
        break;
      case CItemData::PMTIME:
        cs_fielddata = itemData.GetPMTimeL();
        break;
      case CItemData::ATIME:
        cs_fielddata = itemData.GetATimeL();
        break;
      case CItemData::XTIME:
        cs_fielddata = itemData.GetXTimeL();
        if (xint != 0)
          cs_fielddata += _T(" *");
        break;
      case CItemData::XTIME_INT:
        cs_fielddata = itemData.GetXTimeInt();
        break;
      case CItemData::RMTIME:
        cs_fielddata = itemData.GetRMTimeL();
        break;
      case CItemData::POLICY:
      {
        PWPolicy pwp;
        itemData.GetPWPolicy(pwp);
        if (pwp.flags != 0) {
          CString cs_pwp(_T("")), cs_text;
          if (pwp.flags & PWSprefs::PWPolicyUseLowercase) {
            cs_pwp += _T("L");
            if (pwp.lowerminlength > 1) {
              cs_text.Format(_T("(%d)"), pwp.lowerminlength);
              cs_pwp += cs_text;
            }
          }
          if (pwp.flags & PWSprefs::PWPolicyUseUppercase) {
            cs_pwp += _T("U");
            if (pwp.upperminlength > 1) {
              cs_text.Format(_T("(%d)"), pwp.upperminlength);
              cs_pwp += cs_text;
            }
          }
          if (pwp.flags & PWSprefs::PWPolicyUseDigits) {
            cs_pwp += _T("D");
            if (pwp.digitminlength > 1) {
              cs_text.Format(_T("(%d)"), pwp.digitminlength);
              cs_pwp += cs_text;
            }
          }
          if (pwp.flags & PWSprefs::PWPolicyUseSymbols) {
            cs_pwp += _T("S");
            if (pwp.symbolminlength > 1) {
              cs_text.Format(_T("(%d)"), pwp.symbolminlength);
              cs_pwp += cs_text;
            }
          }
          if (pwp.flags & PWSprefs::PWPolicyUseHexDigits)
            cs_pwp += _T("H");
          if (pwp.flags & PWSprefs::PWPolicyUseEasyVision)
            cs_pwp += _T("E");
          if (pwp.flags & PWSprefs::PWPolicyMakePronounceable)
            cs_pwp += _T("P");

          oStringXStream osx;
          osx << cs_pwp << _T(":") << pwp.length;
          cs_fielddata = osx.str();
        } else
          cs_fielddata = _T("");
        break;
      }
      default:
        ASSERT(0);
    }
    iResult = m_ctlItemList.InsertItem(iResult, cs_fielddata.c_str());

    if (iResult < 0) {
      // TODO: issue error here...
      return iResult;
    }

    di->list_index = iResult;
    if (m_bImageInLV)
      SetEntryImage(iResult, nImage);
  }

  if (iView & iTreeOnly) {
    HTREEITEM ti;
    StringX treeDispString = m_ctlItemTree.MakeTreeDisplayString(itemData);
    // get path, create if necessary, add title as last node
    ti = m_ctlItemTree.AddGroup(itemData.GetGroup().c_str());
    if (!PWSprefs::GetInstance()->GetPref(PWSprefs::ExplorerTypeTree)) {
      ti = m_ctlItemTree.InsertItem(treeDispString.c_str(), ti, TVI_SORT);
      m_ctlItemTree.SetItemData(ti, (DWORD_PTR)&itemData);
    } else {
      ti = m_ctlItemTree.InsertItem(treeDispString.c_str(), ti, TVI_LAST);
      m_ctlItemTree.SetItemData(ti, (DWORD_PTR)&itemData);
      if (bSort)
        m_ctlItemTree.SortTree(m_ctlItemTree.GetParentItem(ti));
    }

    SetEntryImage(ti, nImage);

    ASSERT(ti != NULL);
    di->tree_item = ti;
  }

  if (iView & iListOnly) {
    // Set the data in the rest of the columns
    for (int i = 1; i < m_nColumns; i++) {
      switch (m_nColumnTypeByIndex[i]) {
        case CItemData::GROUP:
          cs_fielddata = group;
          break;
        case CItemData::TITLE:
          cs_fielddata = title;
          break;
        case CItemData::USER:
          cs_fielddata = username;
          break;
        case CItemData::NOTES:
          cs_fielddata = strNotes;
          break;
        case CItemData::PASSWORD:
          cs_fielddata = itemData.GetPassword();
          break;
        case CItemData::URL:
          cs_fielddata = itemData.GetURL();
          break;
        case CItemData::CTIME:
          cs_fielddata = itemData.GetCTimeL();
          break;
        case CItemData::PMTIME:
          cs_fielddata = itemData.GetPMTimeL();
          break;
        case CItemData::ATIME:
          cs_fielddata = itemData.GetATimeL();
          break;
        case CItemData::XTIME:
          cs_fielddata = itemData.GetXTimeL();
          if (xint != 0)
            cs_fielddata += _T(" *");
          break;
        case CItemData::XTIME_INT:
          cs_fielddata = itemData.GetXTimeInt();
          break;
        case CItemData::RMTIME:
          cs_fielddata = itemData.GetRMTimeL();
          break;
        case CItemData::POLICY:
        {
          PWPolicy pwp;
          itemData.GetPWPolicy(pwp);
          if (pwp.flags != 0) {
            CString cs_pwp(_T("")), cs_text;
            if (pwp.flags & PWSprefs::PWPolicyUseLowercase) {
              cs_pwp += _T("L");
              if (pwp.lowerminlength > 1) {
                cs_text.Format(_T("(%d)"), pwp.lowerminlength);
                cs_pwp += cs_text;
              }
            }
            if (pwp.flags & PWSprefs::PWPolicyUseUppercase) {
              cs_pwp += _T("U");
              if (pwp.upperminlength > 1) {
                cs_text.Format(_T("(%d)"), pwp.upperminlength);
                cs_pwp += cs_text;
              }
            }
            if (pwp.flags & PWSprefs::PWPolicyUseDigits) {
              cs_pwp += _T("D");
              if (pwp.digitminlength > 1) {
                cs_text.Format(_T("(%d)"), pwp.digitminlength);
                cs_pwp += cs_text;
              }
            }
            if (pwp.flags & PWSprefs::PWPolicyUseSymbols) {
              cs_pwp += _T("S");
              if (pwp.symbolminlength > 1) {
                cs_text.Format(_T("(%d)"), pwp.symbolminlength);
                cs_pwp += cs_text;
              }
            }
            if (pwp.flags & PWSprefs::PWPolicyUseHexDigits)
              cs_pwp += _T("H");
            if (pwp.flags & PWSprefs::PWPolicyUseEasyVision)
              cs_pwp += _T("E");
            if (pwp.flags & PWSprefs::PWPolicyMakePronounceable)
              cs_pwp += _T("P");
            oStringXStream osx;
            osx << cs_pwp << _T(":") << pwp.length;
            cs_fielddata = osx.str();
          } else
            cs_fielddata = _T("");
          break;
        }
        default:
          ASSERT(0);
      }
      m_ctlItemList.SetItemText(iResult, i, cs_fielddata.c_str());
    }

    m_ctlItemList.SetItemData(iResult, (DWORD_PTR)&itemData);
  }
  return iResult;
}

CItemData *DboxMain::getSelectedItem()
{
  CItemData *retval = NULL;
  if (m_ctlItemList.IsWindowVisible()) {
    // flattened list mode.
    POSITION p = m_ctlItemList.GetFirstSelectedItemPosition();
    if (p) {
      int i = m_ctlItemList.GetNextSelectedItem(p);
      retval = (CItemData *)m_ctlItemList.GetItemData(i);
      ASSERT(retval != NULL);
      DisplayInfo *di = (DisplayInfo *)retval->GetDisplayInfo();
      ASSERT(di != NULL && di->list_index == i);
    }
  } else {
    // heirarchy tree mode; go from HTREEITEM to index
    HTREEITEM ti = m_ctlItemTree.GetSelectedItem();
    if (ti != NULL) {
      retval = (CItemData *)m_ctlItemTree.GetItemData(ti);
      if (retval != NULL) {  // leaf node
        DisplayInfo *di = (DisplayInfo *)retval->GetDisplayInfo();
        ASSERT(di != NULL && di->tree_item == ti);
      }
    }    
  }
  return retval;
}

// functor for ClearData
struct deleteDisplayInfo {
  void operator()(pair<CUUIDGen, CItemData> p)
  {delete p.second.GetDisplayInfo();} // no need to set to NULL
};

void DboxMain::ClearData(bool clearMRE)
{
  // Iterate over item list, delete DisplayInfo
  deleteDisplayInfo ddi;
  for_each(m_core.GetEntryIter(), m_core.GetEntryEndIter(),
    ddi);

  m_core.ClearData();

  UpdateSystemTray(m_bOpen ? LOCKED : CLOSED);

  // If data is cleared, m_selectedAtMinimize is useless,
  // since it will be deleted and rebuilt from the file.
  // This means that selection won't be restored in this case.
  // Tough.
  m_selectedAtMinimize = NULL;

  if (clearMRE)
    m_RUEList.ClearEntries();

  //Because GetText returns a copy, we cannot do anything about the names
  if (m_windowok) {
    // For long lists, this is painful, so we disable updates
    m_ctlItemList.LockWindowUpdate();
    m_ctlItemList.DeleteAllItems();
    m_ctlItemList.UnlockWindowUpdate();
    m_ctlItemTree.LockWindowUpdate();
    m_ctlItemTree.DeleteAllItems();
    m_ctlItemTree.UnlockWindowUpdate();
    m_bBoldItem = false;
  }
  m_needsreading = true;
}

void DboxMain::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult) 
{
  NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

  // Get column index to CItemData value
  int iIndex = pNMListView->iSubItem;
  int iTypeSortColumn = m_nColumnTypeByIndex[iIndex];

  HDITEM hdi;
  hdi.mask = HDI_FORMAT;

  if (m_iTypeSortColumn == iTypeSortColumn) {
    m_bSortAscending = !m_bSortAscending;
    PWSprefs *prefs = PWSprefs::GetInstance();
    prefs->SetPref(PWSprefs::SortAscending, m_bSortAscending);
    if (!m_core.GetCurFile().empty() &&
        m_core.GetReadFileVersion() == PWSfile::VCURRENT) {
      if (!m_core.IsReadOnly()) {
        const StringX prefString(prefs->Store());
        SetChanged(m_core.HaveHeaderPreferencesChanged(prefString) ? DBPrefs : ClearDBPrefs);
      }
      ChangeOkUpdate();
    }
  } else {
    // Turn off all previous sort arrrows
    // Note: not sure where, as user may have played with the columns!
    for (int i = 0; i < m_LVHdrCtrl.GetItemCount(); i++) {
      m_LVHdrCtrl.GetItem(i, &hdi);
      if ((hdi.fmt & (HDF_SORTUP | HDF_SORTDOWN)) != 0) {
        hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        m_LVHdrCtrl.SetItem(i, &hdi);
      }
    }

    m_iTypeSortColumn = iTypeSortColumn;
    m_bSortAscending = true;
  }
  SortListView();
  OnHideFindToolBar();

  *pResult = TRUE;
}

void DboxMain::SortListView()
{
  HDITEM hdi;
  hdi.mask = HDI_FORMAT;

  m_bSortAscending = PWSprefs::GetInstance()->GetPref(PWSprefs::SortAscending);
  m_ctlItemList.SortItems(CompareFunc, (LPARAM)this);
  FixListIndexes();

  const int iIndex = m_nColumnIndexByType[m_iTypeSortColumn];
  m_LVHdrCtrl.GetItem(iIndex, &hdi);
  // Turn off all arrows
  hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
  // Turn on the correct arrow
  hdi.fmt |= ((m_bSortAscending == TRUE) ? HDF_SORTUP : HDF_SORTDOWN);
  m_LVHdrCtrl.SetItem(iIndex, &hdi);

  if (!m_bIsRestoring && m_FindToolBar.IsVisible())
    OnHideFindToolBar();
}

void DboxMain::OnHeaderRClick(NMHDR* /* pNMHDR */, LRESULT *pResult)
{
#if defined(POCKET_PC)
  const DWORD dwTrackPopupFlags = TPM_LEFTALIGN;
#else
  const DWORD dwTrackPopupFlags = TPM_LEFTALIGN | TPM_RIGHTBUTTON;
#endif
  CMenu menu;
  CPoint ptMousePos;
  GetCursorPos(&ptMousePos);

  if (menu.LoadMenu(IDR_POPCOLUMNS)) {
    MENUINFO minfo;
    memset(&minfo, 0x00, sizeof(minfo));
    minfo.cbSize = sizeof(MENUINFO);
    minfo.fMask = MIM_MENUDATA;
    minfo.dwMenuData = IDR_POPCOLUMNS;
    menu.SetMenuInfo(&minfo);
    CMenu* pPopup = menu.GetSubMenu(0);
    ASSERT(pPopup != NULL);
    if (m_pCC != NULL)
      pPopup->CheckMenuItem(ID_MENUITEM_COLUMNPICKER,
                            m_pCC->IsWindowVisible() ? MF_CHECKED : MF_UNCHECKED);
    else
      pPopup->CheckMenuItem(ID_MENUITEM_COLUMNPICKER, MF_UNCHECKED);

    pPopup->TrackPopupMenu(dwTrackPopupFlags, ptMousePos.x, ptMousePos.y, this);
  }
  *pResult = TRUE;
}

void DboxMain::OnHeaderBeginDrag(NMHDR* pNMHDR, LRESULT *pResult)
{
  // Called for HDN_BEGINDRAG which changes the column order when CC not visible
  // Stop drag of first column (image)

  NMHEADER *phdn = (NMHEADER *) pNMHDR;

  if (m_bImageInLV && phdn->iItem == 0)
    *pResult = TRUE;
  else
    *pResult = FALSE;
}

void DboxMain::OnHeaderEndDrag(NMHDR* pNMHDR, LRESULT *pResult)
{
  // Called for HDN_ENDDRAG which changes the column order when CC not visible
  // Unfortunately the changes are only really done when this call returns,
  // hence the PostMessage to get the information later

  // Get control after operation is really complete

  // Stop drag of first column (image)
  if (!m_bImageInLV)
    return;

  NMHEADER *phdn = (NMHEADER *) pNMHDR;
  if (phdn->iItem == 0 || 
    (((phdn->pitem->mask & HDI_ORDER) == HDI_ORDER) && 
    phdn->pitem->iOrder == 0)) {
      *pResult = TRUE;
      return;
  }

  // Otherwise allow
  PostMessage(WM_HDR_DRAG_COMPLETE);

  *pResult = FALSE;
}

void DboxMain::OnHeaderNotify(NMHDR* pNMHDR, LRESULT *pResult)
{
  NMHEADER *phdn = (NMHEADER *) pNMHDR;
  *pResult = FALSE;

  if (m_nColumnWidthByIndex == NULL || phdn->pitem == NULL)
    return;

  UINT mask = phdn->pitem->mask;
  if ((mask & HDI_WIDTH) != HDI_WIDTH)
    return;

  // column width changed
  switch (phdn->hdr.code) {
    case HDN_ENDTRACK:
    case HDN_ITEMCHANGED:
      m_nColumnWidthByIndex[phdn->iItem] = phdn->pitem->cxy;
      break;
    default:
      break;
  }
}

void DboxMain::OnToggleView() 
{
  if (m_IsListView)
    OnTreeView();
  else
    OnListView();
}

void DboxMain::OnListView() 
{
  SetListView();
  if (m_FindToolBar.IsVisible())
    OnHideFindToolBar();
}

void DboxMain::OnTreeView() 
{
  SetTreeView();
  if (m_FindToolBar.IsVisible())
    OnHideFindToolBar();
}

void DboxMain::SetListView()
{
  UnFindItem();
  m_ctlItemTree.ShowWindow(SW_HIDE);
  m_ctlItemList.ShowWindow(SW_SHOW);
  PWSprefs::GetInstance()->SetPref(PWSprefs::LastView, _T("list"));
  m_ctlItemList.SetFocus();
  m_IsListView = true;
  // Some items may change on change of view
  UpdateMenuAndToolBar(m_bOpen);
}

void DboxMain::SetTreeView()
{
  UnFindItem();
  m_ctlItemList.ShowWindow(SW_HIDE);
  m_ctlItemTree.ShowWindow(SW_SHOW);
  PWSprefs::GetInstance()->SetPref(PWSprefs::LastView, _T("tree"));
  m_ctlItemTree.SetFocus();
  m_IsListView = false;
  // Some items may change on change of view
  UpdateMenuAndToolBar(m_bOpen);
}

void DboxMain::OnShowHideToolbar() 
{
  bool bState = PWSprefs::GetInstance()->GetPref(PWSprefs::ShowToolbar);
  
  PWSprefs::GetInstance()->SetPref(PWSprefs::ShowToolbar, !bState);
  m_MainToolBar.ShowWindow(bState ? SW_HIDE : SW_SHOW);
  SetToolBarPositions();
  UpdateToolBar(m_core.IsReadOnly());
}

void DboxMain::OnShowHideDragbar() 
{
  bool bDragBarState = PWSprefs::GetInstance()->GetPref(PWSprefs::ShowDragbar);

  PWSprefs::GetInstance()->SetPref(PWSprefs::ShowDragbar, !bDragBarState);
  SetToolBarPositions();
}

void DboxMain::OnOldToolbar() 
{
  PWSprefs::GetInstance()->SetPref(PWSprefs::UseNewToolbar, false);
  SetToolbar(ID_MENUITEM_OLD_TOOLBAR);
  UpdateToolBar(m_core.IsReadOnly());
}

void DboxMain::OnNewToolbar() 
{
  PWSprefs::GetInstance()->SetPref(PWSprefs::UseNewToolbar, true);
  SetToolbar(ID_MENUITEM_NEW_TOOLBAR);
  UpdateToolBar(m_core.IsReadOnly());
}

void DboxMain::SetToolbar(const int menuItem, bool bInit)
{
  // Toolbar
  m_toolbarMode = menuItem;

  if (bInit) {
    m_MainToolBar.LoadDefaultToolBar(m_toolbarMode);
    m_FindToolBar.LoadDefaultToolBar(m_toolbarMode);
    CString csButtonNames = PWSprefs::GetInstance()->
      GetPref(PWSprefs::MainToolBarButtons).c_str();
    m_MainToolBar.CustomizeButtons(csButtonNames);
  } else {
    m_MainToolBar.ChangeImages(m_toolbarMode);
    m_FindToolBar.ChangeImages(m_toolbarMode);
  }
  m_menuManager.SetImageList(&m_MainToolBar);

  m_MainToolBar.Invalidate();
  m_FindToolBar.Invalidate();

  SetToolBarPositions();
}

void DboxMain::OnExpandAll()
{
  m_ctlItemTree.OnExpandAll();
}

void DboxMain::OnCollapseAll()
{
  m_ctlItemTree.OnCollapseAll();
}

void DboxMain::OnTimer(UINT_PTR nIDEvent )
{
  if ((nIDEvent == TIMER_CHECKLOCK && IsWorkstationLocked()) ||
      (nIDEvent == TIMER_USERLOCK && DecrementAndTestIdleLockCounter())) {
    /*
    * Since we clear the data, any unchanged changes will be lost,
    * so we force a save if database is modified, and fail
    * to lock if the save fails (unless db is r-o).
    */
    // Need to save display status for when we return from minimize
    SaveDisplayStatus();
    if (m_core.IsReadOnly() || m_core.GetNumEntries() == 0 ||
        !(m_core.IsChanged() || m_bTSUpdated ||
        m_core.WasDisplayStatusChanged()) ||
        Save() == PWScore::SUCCESS) {
      TRACE("locking database\n");
      if (IsWindowVisible()){
        ShowWindow(SW_MINIMIZE);
      }
      ClearData(false);
      if (nIDEvent == TIMER_CHECKLOCK)
        KillTimer(TIMER_CHECKLOCK);
    }
  }
}

// This function determines if the workstation is locked.
BOOL DboxMain::IsWorkstationLocked() const
{
  BOOL Result = false;
  HDESK hDesktop = OpenInputDesktop(0, FALSE, DESKTOP_SWITCHDESKTOP);
  if( hDesktop != 0 ) {
    // SwitchDesktop fails if hDesktop invisible, screensaver or winlogin.
    Result = ! SwitchDesktop(hDesktop);
    CloseDesktop(hDesktop);
  }
  return Result;
}

void DboxMain::OnChangeTreeFont() 
{
  PWSprefs *prefs = PWSprefs::GetInstance();
  CFont *pOldFontTree;
  pOldFontTree = m_ctlItemTree.GetFont();

  // make sure we know what is inside the font.
  LOGFONT lf;
  pOldFontTree->GetLogFont(&lf);

  // present Tree/List view font and possibly change it
  // Allow user to apply changes to font
  StringX cs_TreeListSampleText = prefs->GetPref(PWSprefs::TreeListSampleText);

  CPWFontDialog fontdlg(&lf, CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT, NULL, NULL);

  fontdlg.m_sampletext = cs_TreeListSampleText.c_str();

  if (fontdlg.DoModal() == IDOK) {
    CString treefont_str;
    treefont_str.Format(_T("%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%s"),
                        lf.lfHeight, lf.lfWidth, lf.lfEscapement, lf.lfOrientation,
                        lf.lfWeight, lf.lfItalic, lf.lfUnderline, lf.lfStrikeOut,
                        lf.lfCharSet, lf.lfOutPrecision, lf.lfClipPrecision,
                        lf.lfQuality, lf.lfPitchAndFamily, lf.lfFaceName);

    m_pFontTree->DeleteObject();
    m_pFontTree->CreateFontIndirect(&lf);

    // transfer the fonts to the tree and list windows
    m_ctlItemTree.SetFont(m_pFontTree);
    m_ctlItemList.SetFont(m_pFontTree);
    m_LVHdrCtrl.SetFont(m_pFontTree);

    // Recalculate header widths
    CalcHeaderWidths();
    // Reset column widths
    AutoResizeColumns();

    // save user's choice of Tree/List font
    prefs->SetPref(PWSprefs::TreeFont, LPCTSTR(treefont_str));
    // save user's sample text
    prefs->SetPref(PWSprefs::TreeListSampleText,
                   LPCTSTR(fontdlg.m_sampletext));
  }
}

void DboxMain::OnChangePswdFont() 
{
  PWSprefs *prefs = PWSprefs::GetInstance();
  LOGFONT lf;
  // Get Password font in case the user wants to change this.
  GetPasswordFont(&lf);

  // present Password font and possibly change it
  // Allow user to apply changes to font
  StringX cs_PswdSampleText = prefs->GetPref(PWSprefs::PswdSampleText);

  CPWFontDialog fontdlg(&lf, CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT);

  fontdlg.m_sampletext = cs_PswdSampleText.c_str();

  if (fontdlg.DoModal() == IDOK) {
    CString pswdfont_str;
    pswdfont_str.Format(_T("%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%s"),
                        lf.lfHeight, lf.lfWidth, lf.lfEscapement, lf.lfOrientation,
                        lf.lfWeight, lf.lfItalic, lf.lfUnderline, lf.lfStrikeOut,
                        lf.lfCharSet, lf.lfOutPrecision, lf.lfClipPrecision,
                        lf.lfQuality, lf.lfPitchAndFamily, lf.lfFaceName);

    // transfer the new font to the passwords
    SetPasswordFont(&lf);
    // save user's choice of password font
    prefs->SetPref(PWSprefs::PasswordFont, LPCTSTR(pswdfont_str));
    // save user's sample text
    prefs->SetPref(PWSprefs::PswdSampleText, LPCTSTR(fontdlg.m_sampletext));
  }
}

static CString GetToken(CString& str, LPCTSTR c)
{
  // helper function for DboxMain::ExtractFont()
  int pos = str.Find(c);
  CString token = str.Left(pos);
  str = str.Mid(pos + 1);

  return token;
}

void DboxMain::ExtractFont(CString& str, LOGFONT *plogfont)
{
  plogfont->lfHeight = _ttol((LPCTSTR)GetToken(str, _T(",")));
  plogfont->lfWidth = _ttol((LPCTSTR)GetToken(str, _T(",")));
  plogfont->lfEscapement = _ttol((LPCTSTR)GetToken(str, _T(",")));
  plogfont->lfOrientation = _ttol((LPCTSTR)GetToken(str, _T(",")));
  plogfont->lfWeight = _ttol((LPCTSTR)GetToken(str, _T(",")));

#pragma warning(push)
#pragma warning(disable:4244) //conversion from 'int' to 'BYTE', possible loss of data
  plogfont->lfItalic = _ttoi((LPCTSTR)GetToken(str, _T(",")));
  plogfont->lfUnderline = _ttoi((LPCTSTR)GetToken(str, _T(",")));
  plogfont->lfStrikeOut = _ttoi((LPCTSTR)GetToken(str, _T(",")));
  plogfont->lfCharSet = _ttoi((LPCTSTR)GetToken(str, _T(",")));
  plogfont->lfOutPrecision = _ttoi((LPCTSTR)GetToken(str, _T(",")));
  plogfont->lfClipPrecision = _ttoi((LPCTSTR)GetToken(str, _T(",")));
  plogfont->lfQuality = _ttoi((LPCTSTR)GetToken(str, _T(",")));
  plogfont->lfPitchAndFamily = _ttoi((LPCTSTR)GetToken(str, _T(",")));
#pragma warning(pop)

#if (_MSC_VER >= 1400)
  _tcscpy_s(plogfont->lfFaceName, LF_FACESIZE, str);
#else
  _tcscpy(plogfont->lfFaceName, str);
#endif  
}

void DboxMain::UpdateSystemTray(const STATE s)
{
  switch (s) {
    case LOCKED:
      app.SetSystemTrayState(ThisMfcApp::LOCKED);
      if (!m_core.GetCurFile().empty()) {
        CString ttt(_T("["));
        ttt += m_core.GetCurFile().c_str();
        ttt += _T("]");
        app.SetTooltipText(ttt);
      }
      break;
    case UNLOCKED:
      app.SetSystemTrayState(ThisMfcApp::UNLOCKED);
      if (!m_core.GetCurFile().empty())
        app.SetTooltipText(m_core.GetCurFile().c_str());
      break;
    case CLOSED:
      app.SetSystemTrayState(ThisMfcApp::CLOSED);
        break;
    default:
    ASSERT(0);
  }
}

BOOL DboxMain::LaunchBrowser(const CString &csURL)
{
  CString theURL(csURL);

  // csURL should be a well-formed URL as defined in RFC3986 with
  // the exceptions listed below.
  // 
  // The default behaviour of this function is to pass the URL to the Windows shell.
  // This will invoke a the default browser for http: and https: schema, the default
  // mailer for mailto: schema, etc.
  //
  // The exceptions to the csURL syntax are as follows:
  // 0. csURL is "sanitized" - newlines, tabs & carriage returns are removed.
  // 1. If csURL doesn't contain "://" OR "mailto:" or [ssh], then we prepend
  //    "http://" to it. This is to allow users to enter "amazon.com" and get the
  //    behaviour the expect.
  // 2. If csURL contains [alt], then the rest of the text is passed to the "alternate"
  //    browser defined in the preferences, along with the command-line parameters.
  //    For example, if AltBrowser is "CoolBrowser.exe" and AltBrowserCmdLineParms
  //    is "-x", then the csURL "[alt] supersite.com" will cause the following to
  //    be invoked:
  //          CoolBrowser.exe -x http://supersite.com
  // 3. If csURL contains {alt} or [ssh], then the behaviour is the same as in (2),
  //    except that "http://" is NOT prepended to the rest of the text. This allows
  //    one to specify an ssh client (such as Putty) as the alternate browser,
  //    and user@machine as the URL.

  theURL.Remove(_T('\r'));
  theURL.Remove(_T('\n'));
  theURL.Remove(_T('\t'));

  bool isMailto = (theURL.Find(_T("mailto:")) != -1);
  UINT errID = isMailto ? IDS_CANTEMAIL : IDS_CANTBROWSE;

  int altReplacements = theURL.Replace(_T("[alt]"), _T(""));
  int alt2Replacements = (theURL.Replace(_T("[ssh]"), _T("")) +
                          theURL.Replace(_T("{alt}"), _T("")));

  if (alt2Replacements <= 0 && !isMailto && theURL.Find(_T("://")) == -1)
    theURL = _T("http://") + theURL;

  CString csAltBrowser(PWSprefs::GetInstance()->
                       GetPref(PWSprefs::AltBrowser).c_str());
  bool useAltBrowser = ((altReplacements > 0 || alt2Replacements > 0) &&
                        !csAltBrowser.IsEmpty());

  SHELLEXECUTEINFO si;
  si.cbSize = sizeof(SHELLEXECUTEINFO);
  si.fMask = 0;
  si.hwnd = NULL;
  si.lpVerb = si.lpFile = si.lpParameters = si.lpDirectory = NULL;
  si.nShow = SW_SHOWNORMAL;

  if (!useAltBrowser) {
    si.lpFile = theURL;
  } else { // alternate browser specified, invoke w/optional args
    CString csCmdLineParms(PWSprefs::GetInstance()->
                           GetPref(PWSprefs::AltBrowserCmdLineParms).c_str());

    if (!csCmdLineParms.IsEmpty())
      theURL = csCmdLineParms + _T(" ") + theURL;
    si.lpFile = csAltBrowser;
    si.lpParameters = theURL;
  }

  BOOL shellExecStatus = ::ShellExecuteEx(&si);
  if(shellExecStatus != TRUE) {
    AfxMessageBox(errID, MB_ICONSTOP);
    return FALSE;
  }
  return TRUE;
}

void DboxMain::SetColumns()
{
  // User hasn't yet saved the columns he/she wants and so gets our order!
  // Or - user has reset the columns (popup menu from right click on Header)
  CString cs_header;
  HDITEM hdi;
  hdi.mask = HDI_LPARAM;

  PWSprefs *prefs = PWSprefs::GetInstance();
  int ipwd = prefs->GetPref(PWSprefs::ShowPasswordInTree) ? 1 : 0;

  CRect rect;
  m_ctlItemList.GetClientRect(&rect);
  int i1stWidth = prefs->GetPref(PWSprefs::Column1Width,
                                 (rect.Width() / 3 + rect.Width() % 3));
  int i2ndWidth = prefs->GetPref(PWSprefs::Column2Width,
                                 rect.Width() / 3);
  int i3rdWidth = prefs->GetPref(PWSprefs::Column3Width,
                                 rect.Width() / 3);

  cs_header = GetHeaderText(CItemData::TITLE);
  m_ctlItemList.InsertColumn(0, cs_header);
  hdi.lParam = CItemData::TITLE;
  m_LVHdrCtrl.SetItem(0, &hdi);
  m_ctlItemList.SetColumnWidth(0, i1stWidth);

  cs_header = GetHeaderText(CItemData::USER);
  m_ctlItemList.InsertColumn(1, cs_header);
  hdi.lParam = CItemData::USER;
  m_LVHdrCtrl.SetItem(1, &hdi);
  m_ctlItemList.SetColumnWidth(1, i2ndWidth);

  cs_header = GetHeaderText(CItemData::NOTES);
  m_ctlItemList.InsertColumn(2, cs_header);
  hdi.lParam = CItemData::NOTES;
  m_LVHdrCtrl.SetItem(2, &hdi);
  m_ctlItemList.SetColumnWidth(2, i3rdWidth);

  if (PWSprefs::GetInstance()->GetPref(PWSprefs::ShowPasswordInTree)) {
    cs_header = GetHeaderText(CItemData::PASSWORD);
    m_ctlItemList.InsertColumn(3, cs_header);
    hdi.lParam = CItemData::PASSWORD;
    m_LVHdrCtrl.SetItem(3, &hdi);
    m_ctlItemList.SetColumnWidth(3,
                                 PWSprefs::GetInstance()->GetPref(PWSprefs::Column4Width,
                                 rect.Width() / 4));
  }

  int ioff = 3;
  cs_header = GetHeaderText(CItemData::URL);
  m_ctlItemList.InsertColumn(ipwd + ioff, cs_header);
  hdi.lParam = CItemData::URL;
  m_LVHdrCtrl.SetItem(ipwd + ioff, &hdi);
  ioff++;

  cs_header = GetHeaderText(CItemData::CTIME);
  m_ctlItemList.InsertColumn(ipwd + ioff, cs_header);
  hdi.lParam = CItemData::CTIME;
  m_LVHdrCtrl.SetItem(ipwd + ioff, &hdi);
  ioff++;

  cs_header = GetHeaderText(CItemData::PMTIME);
  m_ctlItemList.InsertColumn(ipwd + ioff, cs_header);
  hdi.lParam = CItemData::PMTIME;
  m_LVHdrCtrl.SetItem(ipwd + ioff, &hdi);
  ioff++;

  cs_header = GetHeaderText(CItemData::ATIME);
  m_ctlItemList.InsertColumn(ipwd + ioff, cs_header);
  hdi.lParam = CItemData::ATIME;
  m_LVHdrCtrl.SetItem(ipwd + ioff, &hdi);
  ioff++;

  cs_header = GetHeaderText(CItemData::XTIME);
  m_ctlItemList.InsertColumn(ipwd + ioff, cs_header);
  hdi.lParam = CItemData::XTIME;
  m_LVHdrCtrl.SetItem(ipwd + ioff, &hdi);
  ioff++;

  cs_header = GetHeaderText(CItemData::RMTIME);
  m_ctlItemList.InsertColumn(ipwd + ioff, cs_header);
  hdi.lParam = CItemData::RMTIME;
  m_LVHdrCtrl.SetItem(ipwd + ioff, &hdi);
  ioff++;

  cs_header = GetHeaderText(CItemData::POLICY);
  m_ctlItemList.InsertColumn(ipwd + ioff, cs_header);
  hdi.lParam = CItemData::POLICY;
  m_LVHdrCtrl.SetItem(ipwd + ioff, &hdi);
  ioff++;

  m_ctlItemList.SetRedraw(FALSE);

  for (int i = ipwd + 3; i < (ipwd + ioff); i++) {
    m_ctlItemList.SetColumnWidth(i, m_iDateTimeFieldWidth);
  }

  SetHeaderInfo();

  return;
}

void DboxMain::SetColumns(const CString cs_ListColumns)
{
  //  User has saved the columns he/she wants and now we are putting them back

  CString cs_header;
  HDITEM hdi;
  hdi.mask = HDI_LPARAM;

  vector<int> vi_columns;
  vector<int>::const_iterator vi_IterColumns;
  const TCHAR pSep[] = _T(",");
  TCHAR *pTemp;

  // Duplicate as strtok modifies the string
  pTemp = _tcsdup((LPCTSTR)cs_ListColumns);

#if _MSC_VER >= 1400
  // Capture columns shown:
  TCHAR *next_token;
  TCHAR *token = _tcstok_s(pTemp, pSep, &next_token);
  while(token) {
    vi_columns.push_back(_ttoi(token));
    token = _tcstok_s(NULL, pSep, &next_token);
  }
#else
  // Capture columns shown:
  TCHAR *token = _tcstok(pTemp, pSep);
  while(token) {
    vi_columns.push_back(_ttoi(token));
    token = _tcstok(NULL, pSep);
  }
#endif
  free(pTemp);

  // If present, the images are always first
  int iType= *vi_columns.begin();
  if (iType == CItemData::UUID) {
    m_bImageInLV = true;
    m_ctlItemList.SetImageList(m_pImageList, LVSIL_NORMAL);
    m_ctlItemList.SetImageList(m_pImageList, LVSIL_SMALL);
  }

  int icol(0);
  for (vi_IterColumns = vi_columns.begin();
       vi_IterColumns != vi_columns.end();
       vi_IterColumns++) {
    iType = *vi_IterColumns;
    cs_header = GetHeaderText(iType);
    // Images (if present) must be the first column!
    if (iType == CItemData::UUID && icol != 0)
      continue;

    if (!cs_header.IsEmpty()) {
      m_ctlItemList.InsertColumn(icol, cs_header);
      hdi.lParam = iType;
      m_LVHdrCtrl.SetItem(icol, &hdi);
      icol++;
    }
  }

  SetHeaderInfo();

  return;
}

void DboxMain::SetColumnWidths(const CString cs_ListColumnsWidths)
{
  //  User has saved the columns he/she wants and now we are putting them back
  std::vector<int> vi_widths;
  std::vector<int>::const_iterator vi_IterWidths;
  const TCHAR pSep[] = _T(",");
  TCHAR *pWidths;

  // Duplicate as strtok modifies the string
  pWidths = _tcsdup((LPCTSTR)cs_ListColumnsWidths);

#if _MSC_VER >= 1400
  // Capture column widths shown:
  TCHAR *next_token;
  TCHAR *token = _tcstok_s(pWidths, pSep, &next_token);
  while(token) {
    vi_widths.push_back(_ttoi(token));
    token = _tcstok_s(NULL, pSep, &next_token);
  }
#else
  // Capture columnwidths shown:
  TCHAR *token = _tcstok(pWidths, pSep);
  while(token) {
    vi_widths.push_back(_ttoi(token));
    token = _tcstok(NULL, pSep);
  }
#endif
  free(pWidths);

  int icol = 0, index;

  for (vi_IterWidths = vi_widths.begin();
       vi_IterWidths != vi_widths.end();
       vi_IterWidths++) {
    if (icol == (m_nColumns - 1))
      break;
    int iWidth = *vi_IterWidths;
    m_ctlItemList.SetColumnWidth(icol, iWidth);
    index = m_LVHdrCtrl.OrderToIndex(icol);
    m_nColumnWidthByIndex[index] = iWidth;
    icol++;
  }

  // First column special if the Image
  if (m_bImageInLV) {
    m_ctlItemList.SetColumnWidth(0, LVSCW_AUTOSIZE);
  }
  // Last column special
  index = m_LVHdrCtrl.OrderToIndex(m_nColumns - 1);
  m_ctlItemList.SetColumnWidth(index, LVSCW_AUTOSIZE_USEHEADER);
}

void DboxMain::AddColumn(const int iType, const int iIndex)
{
  // Add new column of type iType after current column index iIndex
  CString cs_header;
  HDITEM hdi;
  int iNewIndex(iIndex);

  //  If iIndex = -1, means drop on the end
  if (iIndex < 0)
    iNewIndex = m_nColumns;

  hdi.mask = HDI_LPARAM | HDI_WIDTH;
  cs_header = GetHeaderText(iType);
  ASSERT(!cs_header.IsEmpty());
  iNewIndex = m_ctlItemList.InsertColumn(iNewIndex, cs_header);
  ASSERT(iNewIndex != -1);
  hdi.lParam = iType;
  hdi.cxy = GetHeaderWidth(iType);
  m_LVHdrCtrl.SetItem(iNewIndex, &hdi);

  // Reset values
  SetHeaderInfo();

  // Now show the user
  RefreshViews(iListOnly);
}

void DboxMain::DeleteColumn(const int iType)
{
  // Delete column
  m_ctlItemList.DeleteColumn(m_nColumnIndexByType[iType]);

  // Reset values
  SetHeaderInfo();
}

void DboxMain::SetHeaderInfo()
{
  HDITEM hdi_get;
  // CHeaderCtrl get values
  hdi_get.mask = HDI_LPARAM | HDI_ORDER;

  m_nColumns = m_LVHdrCtrl.GetItemCount();
  ASSERT(m_nColumns > 1);  // Title & User are mandatory!

  // re-initialise array
  for (int i = 0; i < CItemData::LAST; i++)
    m_nColumnIndexByType[i] = 
    m_nColumnIndexByOrder[i] =
    m_nColumnTypeByIndex[i] =
    m_nColumnWidthByIndex[i] = -1;

  m_LVHdrCtrl.GetOrderArray(m_nColumnIndexByOrder, m_nColumns);

  for (int iOrder = 0; iOrder < m_nColumns; iOrder++) {
    const int iIndex = m_nColumnIndexByOrder[iOrder];
    m_ctlItemList.SetColumnWidth(iIndex, LVSCW_AUTOSIZE);
    m_LVHdrCtrl.GetItem(iIndex, &hdi_get);
    ASSERT(iOrder == hdi_get.iOrder);
    m_nColumnIndexByType[hdi_get.lParam] = iIndex;
    m_nColumnTypeByIndex[iIndex] = (int)hdi_get.lParam;
  }

  // Check sort column still there; if not TITLE always is!
  if (m_nColumnIndexByType[m_iTypeSortColumn] == -1)
    m_iTypeSortColumn = CItemData::TITLE;

  SortListView();

  AutoResizeColumns();
}

void DboxMain::OnResetColumns()
{
  // Delete all existing columns
  for (int i = 0; i < m_nColumns; i++) {
    m_ctlItemList.DeleteColumn(0);
  }

  if (m_bImageInLV) {
    m_bImageInLV = false;
    m_ctlItemList.SetImageList(NULL, LVSIL_NORMAL);
    m_ctlItemList.SetImageList(NULL, LVSIL_SMALL);
  }

  // re-initialise array
  for (int itype = 0; itype < CItemData::LAST; itype++)
    m_nColumnIndexByType[itype] = -1;

  // Set default columns
  SetColumns();

  // Reset the column widths
  AutoResizeColumns();

  // Refresh the ListView
  RefreshViews(iListOnly);

  // Reset Column Chooser dialog but only if already created
  if (m_pCC != NULL)
    SetupColumnChooser(false);
}

void DboxMain::AutoResizeColumns()
{
  int iIndex, iType;
  // CHeaderCtrl get values
  for (int iOrder = 0; iOrder < m_nColumns; iOrder++) {
    iIndex = m_nColumnIndexByOrder[iOrder];
    iType = m_nColumnTypeByIndex[iIndex];

    m_ctlItemList.SetColumnWidth(iIndex, LVSCW_AUTOSIZE);
    m_nColumnWidthByIndex[iIndex] = m_ctlItemList.GetColumnWidth(iIndex);

    if (m_nColumnWidthByIndex[iIndex] < m_nColumnHeaderWidthByType[iType]) {
      m_ctlItemList.SetColumnWidth(iIndex, m_nColumnHeaderWidthByType[iType]);
      m_nColumnWidthByIndex[iIndex] = m_nColumnHeaderWidthByType[iType];
    }
  }

  m_ctlItemList.UpdateWindow();

  // First column is special if an image
  if (m_bImageInLV) {
    m_ctlItemList.SetColumnWidth(0, LVSCW_AUTOSIZE);
  }
  // Last column is special
  iIndex = m_nColumnIndexByOrder[m_nColumns - 1];
  m_ctlItemList.SetColumnWidth(iIndex, LVSCW_AUTOSIZE);
  m_ctlItemList.SetColumnWidth(iIndex, LVSCW_AUTOSIZE_USEHEADER);
  m_nColumnWidthByIndex[iIndex] = m_ctlItemList.GetColumnWidth(iIndex);
}

void DboxMain::OnColumnPicker()
{
  SetupColumnChooser(true);
}

void DboxMain::SetupColumnChooser(const bool bShowHide)
{
  if (m_pCC == NULL) {
    m_pCC = new CColumnChooserDlg;
    BOOL ret = m_pCC->Create(IDD_COLUMNCHOOSER, this);
    if (!ret) {   //Create failed.
      m_pCC = NULL;
      return;
    }
    m_pCC->SetLVHdrCtrlPtr(&m_LVHdrCtrl);

    // Set extended style
    DWORD dw_style = m_pCC->m_ccListCtrl.GetExtendedStyle() | LVS_EX_ONECLICKACTIVATE;
    m_pCC->m_ccListCtrl.SetExtendedStyle(dw_style);

    // Make sure it doesn't appear obscure the header
    CRect HDRrect, CCrect;
    m_LVHdrCtrl.GetWindowRect(&HDRrect);
    m_pCC->GetWindowRect(&CCrect);
    // Note (0,0) is the top left of screen
    if (CCrect.top < HDRrect.bottom) {
      int x = CCrect.left;
      int y = HDRrect.bottom + 20;
      m_pCC->SetWindowPos(0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    // Insert column with "dummy" header
    m_pCC->m_ccListCtrl.InsertColumn(0, _T(""));
    m_pCC->m_ccListCtrl.SetColumnWidth(0, m_iheadermaxwidth);

    // Make it just wide enough to take the text
    CRect rect1, rect2;
    m_pCC->GetWindowRect(&rect1);
    m_pCC->m_ccListCtrl.GetWindowRect(&rect2);
    m_pCC->SetWindowPos(NULL, 0, 0, m_iheadermaxwidth + 18,
                        rect1.Height(), SWP_NOMOVE | SWP_NOZORDER);
    m_pCC->m_ccListCtrl.SetWindowPos(NULL, 0, 0, m_iheadermaxwidth + 6,
                                     rect2.Height(), SWP_NOMOVE | SWP_NOZORDER);
  }

  int i;
  CString cs_header;

  // Clear all current entries
  m_pCC->m_ccListCtrl.DeleteAllItems();

  // and repopulate
  int iItem;
  for (i = CItemData::LAST - 1; i >= 0; i--) {
    // Can't play with Title or User columns
    if (i == CItemData::TITLE || i == CItemData::USER)
      continue;

    if (m_nColumnIndexByType[i] == -1) {
      cs_header = GetHeaderText(i);
      if (!cs_header.IsEmpty()) {
        iItem = m_pCC->m_ccListCtrl.InsertItem(0, cs_header);
        m_pCC->m_ccListCtrl.SetItemData(iItem, (DWORD)i);
      }
    }
  }

  // If called by user right clicking on header, hide it or show it
  if (bShowHide)
    m_pCC->ShowWindow(m_pCC->IsWindowVisible() ? SW_HIDE : SW_SHOW);
}

CString DboxMain::GetHeaderText(const int iType)
{
  CString cs_header;
  switch (iType) {
    case CItemData::UUID:
      cs_header.LoadString(IDS_ICON);
      break;
    case CItemData::GROUP:
      cs_header.LoadString(IDS_GROUP);
      break;
    case CItemData::TITLE:
      cs_header.LoadString(IDS_TITLE);
      break;
    case CItemData::USER:
      cs_header.LoadString(IDS_USERNAME);
      break;
    case CItemData::PASSWORD:
      cs_header.LoadString(IDS_PASSWORD);
      break;
    case CItemData::URL:
      cs_header.LoadString(IDS_URL);
      break;
    case CItemData::NOTES:
      cs_header.LoadString(IDS_NOTES);
      break;
    case CItemData::CTIME:        
      cs_header.LoadString(IDS_CREATED);
      break;
    case CItemData::PMTIME:
      cs_header.LoadString(IDS_PASSWORDMODIFIED);
      break;
    case CItemData::ATIME:
      cs_header.LoadString(IDS_LASTACCESSED);
      break;
    case CItemData::XTIME:
      cs_header.LoadString(IDS_PASSWORDEXPIRYDATE);
      break;
    case CItemData::XTIME_INT:
      cs_header.LoadString(IDS_PASSWORDEXPIRYDATEINT);
      break;
    case CItemData::RMTIME:
      cs_header.LoadString(IDS_LASTMODIFIED);
      break;
    case CItemData::POLICY:        
      cs_header.LoadString(IDS_PWPOLICY);
      break;
    default:
      cs_header.Empty();
  }
  return cs_header;
}

int DboxMain::GetHeaderWidth(const int iType)
{
  int nWidth(0);

  switch (iType) {
    case CItemData::UUID:
    case CItemData::GROUP:
    case CItemData::TITLE:
    case CItemData::USER:
    case CItemData::PASSWORD:
    case CItemData::NOTES:
    case CItemData::URL:
    case CItemData::POLICY:
    case CItemData::XTIME_INT:
      nWidth = m_nColumnHeaderWidthByType[iType];
      break;
    case CItemData::CTIME:        
    case CItemData::PMTIME:
    case CItemData::ATIME:
    case CItemData::XTIME:
    case CItemData::RMTIME:
      nWidth = m_iDateTimeFieldWidth;
      break;
    default:
      break;
  }

  return nWidth;
}

void DboxMain::CalcHeaderWidths()
{
  // Get default column width for datetime fields
  TCHAR time_str[80], datetime_str[80];
  // Use "fictitious" longest English date
  SYSTEMTIME systime;
  systime.wYear = (WORD)2000;
  systime.wMonth = (WORD)9;
  systime.wDay = (WORD)30;
  systime.wDayOfWeek = (WORD)3;
  systime.wHour = (WORD)23;
  systime.wMinute = (WORD)44;
  systime.wSecond = (WORD)55;
  systime.wMilliseconds = (WORD)0;
  TCHAR szBuf[80];
  VERIFY(::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, szBuf, 80));
  GetDateFormat(LOCALE_USER_DEFAULT, 0, &systime, szBuf, datetime_str, 80);
  szBuf[0] = _T(' ');  // Put a blank between date and time
  VERIFY(::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STIMEFORMAT, &szBuf[1], 79));
  GetTimeFormat(LOCALE_USER_DEFAULT, 0, &systime, szBuf, time_str, 80);
#if _MSC_VER >= 1400
  _tcscat_s(datetime_str, 80, time_str);
#else
  _tcscat(datetime_str, 80, time_str);
#endif

  m_iDateTimeFieldWidth = m_ctlItemList.GetStringWidth(datetime_str) + 6;

  m_iheadermaxwidth = -1;
  CString cs_header;

  for (int iType = 0; iType < CItemData::LAST; iType++) {
    switch (iType) {
      case CItemData::UUID:
        cs_header.LoadString(IDS_ICON);
        break;
      case CItemData::GROUP:
        cs_header.LoadString(IDS_GROUP);
        break;
      case CItemData::TITLE:
        cs_header.LoadString(IDS_TITLE);
        break;
      case CItemData::USER:
        cs_header.LoadString(IDS_USERNAME);
        break;
      case CItemData::PASSWORD:
        cs_header.LoadString(IDS_PASSWORD);
        break;
      case CItemData::URL:
        cs_header.LoadString(IDS_URL);
        break;
      case CItemData::NOTES:
        cs_header.LoadString(IDS_NOTES);
        break;
      case CItemData::CTIME:        
        cs_header.LoadString(IDS_CREATED);
        break;
      case CItemData::PMTIME:
        cs_header.LoadString(IDS_PASSWORDMODIFIED);
        break;
      case CItemData::ATIME:
        cs_header.LoadString(IDS_LASTACCESSED);
        break;
      case CItemData::XTIME:
        cs_header.LoadString(IDS_PASSWORDEXPIRYDATE);
        break;
      case CItemData::XTIME_INT:
        cs_header.LoadString(IDS_PASSWORDEXPIRYDATEINT);
        break;
      case CItemData::RMTIME:
        cs_header.LoadString(IDS_LASTMODIFIED);
        break;
      case CItemData::POLICY:        
        cs_header.LoadString(IDS_PWPOLICY);
        break;
      default:
        cs_header.Empty();
          }

    if (!cs_header.IsEmpty())
      m_nColumnHeaderWidthByType[iType] = m_ctlItemList.GetStringWidth(cs_header) + 20;
    else
      m_nColumnHeaderWidthByType[iType] = -4;

    m_iheadermaxwidth = max(m_iheadermaxwidth, m_nColumnHeaderWidthByType[iType]);
  }
}

void DboxMain::UnFindItem()
{
  // Entries found are made bold - remove it here.
  if (m_bBoldItem) {
    m_ctlItemTree.SetItemState(m_LastFoundTreeItem, 0, TVIS_BOLD);
    m_bBoldItem = false;
  }
}

bool DboxMain::GetDriveAndDirectory(const StringX &cs_infile, CString &cs_drive,
                                    CString &cs_directory)
{
  stringT applicationpath = pws_os::getexecdir();
  stringT inpath = cs_infile.c_str();
  stringT appdrive, appdir;
  stringT drive, dir, file, ext;

  pws_os::splitpath(applicationpath, appdrive, appdir, file, ext);
  if (!pws_os::splitpath(inpath, drive, dir, file, ext)) {
    PWSDebug::IssueError(_T("View Report: Error finding path to database"));
    return false;
  }

  if (drive.empty())
    drive = appdrive;
  if (dir.empty())
    dir = appdir;
  cs_drive = drive.c_str();
  cs_directory = dir.c_str();
  return true;
}

void DboxMain::OnViewReports()
{
  CString cs_filename, cs_path, csAction;
  CString cs_directory, cs_drive;

  if (!GetDriveAndDirectory(m_core.GetCurFile(), cs_drive, cs_directory))
    return;

  CGeneralMsgBox gmb;
  CString cs_msg(MAKEINTRESOURCE(IDS_SELECTREPORT));
  gmb.SetMsg(cs_msg);

  struct _stat statbuf;
  bool bReportExists(false);

  csAction.LoadString(IDS_RPTCOMPARE);
  cs_filename.Format(IDSC_REPORTFILENAME, cs_drive, cs_directory, csAction);
  if (::_tstat(cs_filename, &statbuf) == 0) {
    gmb.AddButton(1, csAction);
    bReportExists = true;
  }
  csAction.LoadString(IDS_RPTFIND);
  cs_filename.Format(IDSC_REPORTFILENAME, cs_drive, cs_directory, csAction);
  if (::_tstat(cs_filename, &statbuf) == 0) {
    gmb.AddButton(2, csAction);
    bReportExists = true;
  }
  csAction.LoadString(IDS_RPTIMPORTTEXT);
  cs_filename.Format(IDSC_REPORTFILENAME, cs_drive, cs_directory, csAction);
  if (::_tstat(cs_filename, &statbuf) == 0) {
    gmb.AddButton(3, csAction);
    bReportExists = true;
  }
  csAction.LoadString(IDS_RPTIMPORTXML);
  cs_filename.Format(IDSC_REPORTFILENAME, cs_drive, cs_directory, csAction);
  if (::_tstat(cs_filename, &statbuf) == 0) {
    gmb.AddButton(4, csAction);
    bReportExists = true;
  }
  csAction.LoadString(IDS_RPTMERGE);
  cs_filename.Format(IDSC_REPORTFILENAME, cs_drive, cs_directory, csAction);
  if (::_tstat(cs_filename, &statbuf) == 0) {
    gmb.AddButton(5, csAction);
    bReportExists = true;
  }
  csAction.LoadString(IDS_RPTVALIDATE);
  cs_filename.Format(IDSC_REPORTFILENAME, cs_drive, cs_directory, csAction);
  if (::_tstat(cs_filename, &statbuf) == 0) {
    gmb.AddButton(6, csAction);
    bReportExists = true;
  }

  if (!bReportExists) {
    AfxMessageBox(IDS_NOREPORTSEXIST);
    return;
  }

  gmb.AddButton(6, IDS_CANCEL, TRUE, TRUE);
  gmb.SetStandardIcon(MB_ICONQUESTION);

  INT_PTR rc = gmb.DoModal();
  UINT uistring(0);
  switch (rc) {
    case 1:
      uistring = IDS_RPTCOMPARE;
      break;
    case 2:
      uistring = IDS_RPTFIND;
      break;
    case 3:
      uistring = IDS_RPTIMPORTTEXT;
      break;
    case 4:
      uistring = IDS_RPTIMPORTXML;
      break;
    case 5:
      uistring = IDS_RPTMERGE;
      break;
    case 6:
      uistring = IDS_RPTVALIDATE;
      break;
    default:
      return;
  }
  csAction.LoadString(uistring);
  cs_filename.Format(IDSC_REPORTFILENAME, cs_drive, cs_directory, csAction);

  ViewReport(cs_filename);
  return;
}

void DboxMain::OnViewReports(UINT nID)
{
  ASSERT((nID >= ID_MENUITEM_REPORT_COMPARE) &&
    (nID <= ID_MENUITEM_REPORT_VALIDATE));

  CString cs_filename, cs_path, csAction;
  CString cs_drive, cs_directory;

  if (!GetDriveAndDirectory(m_core.GetCurFile(), cs_drive, cs_directory))
    return;

  UINT uistring(0);
  switch (nID) {
    case ID_MENUITEM_REPORT_COMPARE:
      uistring = IDS_RPTCOMPARE;
      break;
    case ID_MENUITEM_REPORT_FIND:
      uistring = IDS_RPTFIND;
      break;
    case ID_MENUITEM_REPORT_IMPORTTEXT:
      uistring = IDS_RPTIMPORTTEXT;
      break;
    case ID_MENUITEM_REPORT_IMPORTXML:
      uistring = IDS_RPTIMPORTXML;
      break;
    case ID_MENUITEM_REPORT_MERGE:
      uistring = IDS_RPTMERGE;
      break;
    case ID_MENUITEM_REPORT_VALIDATE:
      uistring = IDS_RPTVALIDATE;
      break;
    default:
      ASSERT(0);
  }
  csAction.LoadString(uistring);
  cs_filename.Format(IDSC_REPORTFILENAME, cs_drive, cs_directory, csAction);

  ViewReport(cs_filename);

  return;
}

void DboxMain::ViewReport(CReport &rpt)
{
  CViewReport vr_dlg(this, &rpt);

  vr_dlg.DoModal();
}

void DboxMain::ViewReport(const CString &cs_ReportFileName)
{
  CString cs_path, csAction;
  CString cs_drive, cs_directory;

  if (!GetDriveAndDirectory(LPCTSTR(cs_ReportFileName), cs_drive, cs_directory))
    return;

  cs_path.Format(_T("%s%s"), cs_drive, cs_directory);

  TCHAR szExecName[MAX_PATH + 1];

  // Find out the users default editor for "txt" files
  DWORD dwSize(MAX_PATH);
  HRESULT stat = ::AssocQueryString(0, ASSOCSTR_EXECUTABLE, _T(".txt"), _T("Open"),
                                    szExecName, &dwSize);
  if (int(stat) != S_OK) {  
#ifdef _DEBUG
    AfxMessageBox(_T("oops"));
#endif
    return;
  }

  // Create an Edit process
  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ZeroMemory( &si, sizeof(si) );
  si.cb = sizeof(si);
  ZeroMemory( &pi, sizeof(pi) );

  DWORD dwCreationFlags(0);
#ifdef _UNICODE
  dwCreationFlags = CREATE_UNICODE_ENVIRONMENT;
#endif

  CString cs_CommandLine;

  // Make the command line = "<program>" "file" 
  cs_CommandLine.Format(_T("\"%s\" \"%s\""), szExecName, cs_ReportFileName);
  int ilen = cs_CommandLine.GetLength();
  LPTSTR pszCommandLine = cs_CommandLine.GetBuffer(ilen);

  if (!CreateProcess(NULL, pszCommandLine, NULL, NULL, FALSE, dwCreationFlags, 
                     NULL, cs_path, &si, &pi)) {
    TRACE( "CreateProcess failed (%d).\n", GetLastError() );
  }

  // Close process and thread handles. 
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  cs_CommandLine.ReleaseBuffer();

  return;
}

int DboxMain::OnUpdateViewReports(const int nID)
{
  StringX cs_Database(m_core.GetCurFile());

  if (cs_Database.empty()) {
    return FALSE;
  }

  CString cs_filename, csAction;
  CString cs_drive, cs_directory;

  if (!GetDriveAndDirectory(cs_Database, cs_drive, cs_directory))
    return FALSE;

  UINT uistring(0);
  switch (nID) {
    case ID_MENUITEM_REPORT_COMPARE:
      uistring = IDS_RPTCOMPARE;
      break;
    case ID_MENUITEM_REPORT_FIND:
      uistring = IDS_RPTFIND;
      break;
    case ID_MENUITEM_REPORT_IMPORTTEXT:
      uistring = IDS_RPTIMPORTTEXT;
      break;
    case ID_MENUITEM_REPORT_IMPORTXML:
      uistring = IDS_RPTIMPORTXML;
      break;
    case ID_MENUITEM_REPORT_MERGE:
      uistring = IDS_RPTMERGE;
      break;
    case ID_MENUITEM_REPORT_VALIDATE:
      uistring = IDS_RPTVALIDATE;
      break;
    default:
      TRACE(_T("ID=%d\n"), nID);
      ASSERT(0);
  }

  csAction.LoadString(uistring);
  cs_filename.Format(IDSC_REPORTFILENAME, cs_drive, cs_directory, csAction);

  struct _stat statbuf;

  // Only allow selection if file exists!
  int status = ::_tstat(cs_filename, &statbuf);
  return (status != 0) ? FALSE : TRUE;
}

void DboxMain::OnRefreshWindow()
{
  // Useful for users if the are using a filter and have edited an entry
  // so it no longer passes
  RefreshViews();
}

void DboxMain::OnCustomizeToolbar()
{
  CToolBarCtrl& mainTBCtrl = m_MainToolBar.GetToolBarCtrl();
  mainTBCtrl.Customize();

  StringX cs_temp = LPCTSTR(m_MainToolBar.GetButtonString());
  PWSprefs::GetInstance()->SetPref(PWSprefs::MainToolBarButtons, cs_temp);
}

void DboxMain::OnHideFindToolBar()
{
  SetFindToolBar(false);

  // Select the last found item on closing the FindToolbar (either by pressing
  // close or via Esc key if not used to minimize application).
  if (m_ctlItemList.IsWindowVisible() && m_LastFoundListItem != -1) {
    m_ctlItemList.SetFocus();
    m_ctlItemList.SetItemState(m_LastFoundListItem,
                               LVIS_FOCUSED | LVIS_SELECTED,
                               LVIS_FOCUSED | LVIS_SELECTED);
  } else
  if (m_ctlItemTree.IsWindowVisible() && m_LastFoundTreeItem != NULL) {
    m_ctlItemTree.SetFocus();
    m_ctlItemTree.Select(m_LastFoundTreeItem, TVGN_CARET);
  }
}

void DboxMain::SetFindToolBar(bool bShow)
{
  if (m_FindToolBar.GetSafeHwnd() == NULL)
    return;

  if (bShow)
    m_core.ResumeOnListNotification();

  m_FindToolBar.ShowFindToolBar(bShow);

  SetToolBarPositions();
}

void DboxMain::SetToolBarPositions()
{
  if (m_FindToolBar.GetSafeHwnd() == NULL)
    return;

  CRect rect, dragrect;
  RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
  RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0, reposQuery, &rect);
  bool bDragBarState = PWSprefs::GetInstance()->GetPref(PWSprefs::ShowDragbar);
  if (bDragBarState) {
    // Get the image states just incase another entry selected
    // since last shown
    CItemData *entry = GetLastSelected();
    if (entry == NULL) {
      m_DDGroup.SetStaticState(true);
      m_DDTitle.SetStaticState(false);
      m_DDPassword.SetStaticState(false);
      m_DDUser.SetStaticState(false);
      m_DDNotes.SetStaticState(false);
      m_DDURL.SetStaticState(false);
    } else {
      m_DDGroup.SetStaticState(!entry->IsGroupEmpty());
      m_DDTitle.SetStaticState(true);
      m_DDPassword.SetStaticState(true);
      m_DDUser.SetStaticState(!entry->IsUserEmpty());
      m_DDNotes.SetStaticState(!entry->IsNotesEmpty());
      m_DDURL.SetStaticState(!entry->IsURLEmpty());
    }

    const int i = GetSystemMetrics(SM_CYBORDER);
    m_DDGroup.ShowWindow(SW_SHOW);
    m_DDGroup.EnableWindow(TRUE);
    m_DDGroup.GetWindowRect(&dragrect);
    ScreenToClient(&dragrect);
    m_DDGroup.SetWindowPos(NULL, dragrect.left, rect.top + i, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    m_DDTitle.ShowWindow(SW_SHOW);
    m_DDTitle.EnableWindow(TRUE);
    m_DDTitle.GetWindowRect(&dragrect);
    ScreenToClient(&dragrect);
    m_DDTitle.SetWindowPos(NULL, dragrect.left, rect.top + i, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    m_DDUser.ShowWindow(SW_SHOW);
    m_DDUser.EnableWindow(TRUE);
    m_DDUser.GetWindowRect(&dragrect);
    ScreenToClient(&dragrect);
    m_DDUser.SetWindowPos(NULL, dragrect.left, rect.top + i, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    m_DDPassword.ShowWindow(SW_SHOW);
    m_DDPassword.EnableWindow(TRUE);
    m_DDPassword.GetWindowRect(&dragrect);
    ScreenToClient(&dragrect);
    m_DDPassword.SetWindowPos(NULL, dragrect.left, rect.top + i, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    m_DDNotes.ShowWindow(SW_SHOW);
    m_DDNotes.EnableWindow(TRUE);
    m_DDNotes.GetWindowRect(&dragrect);
    ScreenToClient(&dragrect);
    m_DDNotes.SetWindowPos(NULL, dragrect.left, rect.top + i, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    m_DDURL.ShowWindow(SW_SHOW);
    m_DDURL.EnableWindow(TRUE);
    m_DDURL.GetWindowRect(&dragrect);
    ScreenToClient(&dragrect);
    m_DDURL.SetWindowPos(NULL, dragrect.left, rect.top + i, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    rect.top += dragrect.Height() + 2 * i;
  } else {
    m_DDGroup.ShowWindow(SW_HIDE);
    m_DDGroup.EnableWindow(FALSE);
    m_DDTitle.ShowWindow(SW_HIDE);
    m_DDTitle.EnableWindow(FALSE);
    m_DDUser.ShowWindow(SW_HIDE);
    m_DDUser.EnableWindow(FALSE);
    m_DDPassword.ShowWindow(SW_HIDE);
    m_DDPassword.EnableWindow(FALSE);
    m_DDNotes.ShowWindow(SW_HIDE);
    m_DDNotes.EnableWindow(FALSE);
    m_DDURL.ShowWindow(SW_HIDE);
    m_DDURL.EnableWindow(FALSE);
  }
  m_ctlItemList.MoveWindow(&rect, TRUE);
  m_ctlItemTree.MoveWindow(&rect, TRUE);

  if (m_FindToolBar.IsVisible()) {
    // Is visible.  Try to get FindToolBar "above" the StatusBar!
    ASSERT(m_FindToolBar.GetParent() == m_statusBar.GetParent());

    CRect ftb_rect, stb_rect;
    m_FindToolBar.GetWindowRect(&ftb_rect);
    m_statusBar.GetWindowRect(&stb_rect);

    if (ftb_rect.top > stb_rect.top) {
      // FindToolBar is "below" the StatusBar
      ScreenToClient(&ftb_rect);
      ScreenToClient(&stb_rect);
      // Move FindToolBar up by the height of the Statusbar
      m_FindToolBar.MoveWindow(ftb_rect.left, ftb_rect.top - stb_rect.Height(),
        ftb_rect.Width(), ftb_rect.Height());
      // Move Statusbar down by the height of the FindToolBar
      m_statusBar.MoveWindow(stb_rect.left, stb_rect.top + ftb_rect.Height(),
        stb_rect.Width(), stb_rect.Height());
      m_FindToolBar.Invalidate();
      m_statusBar.Invalidate();
    }
  }
}

void DboxMain::OnToolBarClearFind()
{
  m_FindToolBar.ClearFind();
}

void DboxMain::OnToolBarFindCase()
{
  m_FindToolBar.ToggleToolBarFindCase();
}

void DboxMain::OnToolBarFindAdvanced()
{
  m_FindToolBar.ShowFindAdvanced();
}

void DboxMain::OnToolBarFindReport()
{
  std::vector<int> *pindices;
  CString csFindString;

  pindices = m_FindToolBar.GetSearchResults();
  m_FindToolBar.GetSearchText(csFindString);
  if (pindices == NULL || csFindString.IsEmpty())
    return;

  CString buffer, cs_temp;
  CReport rpt;
  cs_temp.LoadString(IDS_RPTFIND);
  rpt.StartReport(cs_temp, m_core.GetCurFile().c_str());

  CItemData::FieldBits bsFFields;
  bool bFAdvanced;
  CString Fsubgroup_name;
  int Fsubgroup_set, Fsubgroup_object, Fsubgroup_function;

  m_FindToolBar.GetSearchInfo(bFAdvanced, bsFFields, Fsubgroup_name, 
                              Fsubgroup_set, Fsubgroup_object, Fsubgroup_function);

  // tell the user we're done & provide short Compare report
  if (!bFAdvanced) {
    cs_temp.LoadString(IDS_NONE);
    buffer.Format(IDS_ADVANCEDOPTIONS, cs_temp);
    rpt.WriteLine((LPCTSTR)buffer);
    rpt.WriteLine();
  } else {
    if (Fsubgroup_set == BST_UNCHECKED) {
      cs_temp.LoadString(IDS_NONE);
    } else {
      CString cs_Object, cs_case, cs_text;
      UINT uistring(0);

      switch(Fsubgroup_object) {
        case CItemData::GROUP:
          uistring = IDS_GROUP;
          break;
        case CItemData::TITLE:
          uistring = IDS_TITLE;
          break;
        case CItemData::USER:
          uistring = IDS_USERNAME;
          break;
        case CItemData::GROUPTITLE:
          uistring = IDS_GROUPTITLE;
          break;
        case CItemData::URL:
          uistring = IDS_URL;
          break;
        case CItemData::NOTES:
          uistring = IDS_NOTES;
          break;
        default:
          ASSERT(0);
      }
      cs_Object.LoadString(uistring);

      cs_case.LoadString(Fsubgroup_function > 0 ? 
                         IDS_ADVCASE_INSENSITIVE : IDS_ADVCASE_SENSITIVE);

      switch (Fsubgroup_function) {
        case -PWSMatch::MR_EQUALS:
        case  PWSMatch::MR_EQUALS:
          uistring = IDSC_EQUALS;
          break;
        case -PWSMatch::MR_NOTEQUAL:
        case  PWSMatch::MR_NOTEQUAL:
          uistring = IDSC_DOESNOTEQUAL;
          break;
        case -PWSMatch::MR_BEGINS:
        case  PWSMatch::MR_BEGINS:
          uistring = IDSC_BEGINSWITH;
          break;
        case -PWSMatch::MR_NOTBEGIN:
        case  PWSMatch::MR_NOTBEGIN:
          uistring = IDSC_DOESNOTBEGINSWITH;
          break;
        case -PWSMatch::MR_ENDS:
        case  PWSMatch::MR_ENDS:
          uistring = IDSC_ENDSWITH;
          break;
        case -PWSMatch::MR_NOTEND:
        case  PWSMatch::MR_NOTEND:
          uistring = IDSC_DOESNOTENDWITH;
          break;
        case -PWSMatch::MR_CONTAINS:
        case  PWSMatch::MR_CONTAINS:
          uistring = IDSC_CONTAINS;
          break;
        case -PWSMatch::MR_NOTCONTAIN:
        case  PWSMatch::MR_NOTCONTAIN:
          uistring = IDSC_DOESNOTCONTAIN;
          break;
        default:
          ASSERT(0);
      }
      cs_text.LoadString(uistring);
      cs_temp.Format(IDS_ADVANCEDSUBSET, cs_Object, cs_text, Fsubgroup_name,
                     cs_case);
    }
    buffer.Format(IDS_ADVANCEDOPTIONS, cs_temp);
    rpt.WriteLine((LPCTSTR)buffer);
    rpt.WriteLine();

    cs_temp.LoadString(IDS_RPTFIND);
    buffer.Format(IDS_ADVANCEDFIELDS, cs_temp);
    rpt.WriteLine((LPCTSTR)buffer);

    buffer = _T("\t");
    if (bsFFields.test(CItemData::GROUP))
      buffer += _T("\t") + CString(MAKEINTRESOURCE(IDS_FINDGROUP));
    if (bsFFields.test(CItemData::TITLE))
      buffer += _T("\t") + CString(MAKEINTRESOURCE(IDS_FINDTITLE));
    if (bsFFields.test(CItemData::USER))
      buffer += _T("\t") + CString(MAKEINTRESOURCE(IDS_FINDUSER));
    if (bsFFields.test(CItemData::PASSWORD))
      buffer += _T("\t") + CString(MAKEINTRESOURCE(IDS_COMPPASSWORD));
    if (bsFFields.test(CItemData::NOTES))
      buffer += _T("\t") + CString(MAKEINTRESOURCE(IDS_COMPNOTES));
    if (bsFFields.test(CItemData::URL))
      buffer += _T("\t") + CString(MAKEINTRESOURCE(IDS_COMPURL));
    if (bsFFields.test(CItemData::AUTOTYPE))
      buffer += _T("\t") + CString(MAKEINTRESOURCE(IDS_COMPAUTOTYPE));
    if (bsFFields.test(CItemData::PWHIST))
      buffer += _T("\t") + CString(MAKEINTRESOURCE(IDS_COMPPWHISTORY));
    rpt.WriteLine((LPCTSTR)buffer);
    rpt.WriteLine();
  }

  if (pindices->size() == 0) {
    buffer.Format(IDS_SEARCHRESULTS1, csFindString);
    rpt.WriteLine((LPCTSTR)buffer);
  } else {
    buffer.Format(IDS_SEARCHRESULTS2, csFindString);
    rpt.WriteLine((LPCTSTR)buffer);
    int i, index;
    for (i = 0; i < (int)pindices->size(); i++) {
      index = pindices->at(i);
      CItemData *ci = (CItemData *)m_ctlItemList.GetItemData(index);
      buffer.Format(IDS_COMPARESTATS, ci->GetGroup(), ci->GetTitle(), ci->GetUser());
      rpt.WriteLine((LPCTSTR)buffer, false);
    }
  }
  rpt.WriteLine();
  rpt.EndReport();

  CGeneralMsgBox gmb;
  gmb.SetTitle(IDS_RPTFIND);
  gmb.SetMsg(IDS_REPORTCREATED);
  gmb.SetStandardIcon(MB_ICONINFORMATION);
  gmb.AddButton(1, _T("OK"), TRUE, TRUE);
  gmb.AddButton(2, IDS_VIEWREPORT);
  INT_PTR msg_rc = gmb.DoModal();
  if (msg_rc == 2)
    ViewReport(rpt);

  m_FindToolBar.SetStatus(cs_temp);
}

void DboxMain::UpdateBrowseURLSendEmailButton(const bool bIsEmail)
{
  CToolBarCtrl &mainTBCtrl =  m_MainToolBar.GetToolBarCtrl();
  if (mainTBCtrl.IsButtonHidden(ID_MENUITEM_BROWSEURL) == TRUE)
    return;

  TBBUTTONINFO tbinfo;
  memset(&tbinfo, 0x00, sizeof(tbinfo));
  tbinfo.cbSize = sizeof(tbinfo);
  mainTBCtrl.HideButton(ID_MENUITEM_BROWSEURL, TRUE);
  if (bIsEmail) {
    tbinfo.iImage = m_MainToolBar.GetSendEmailImageIndex();
  } else {
    tbinfo.iImage = m_MainToolBar.GetBrowseURLImageIndex();
  }
  tbinfo.dwMask = TBIF_IMAGE;
  mainTBCtrl.SetButtonInfo(ID_MENUITEM_BROWSEURL, &tbinfo);
  mainTBCtrl.HideButton(ID_MENUITEM_BROWSEURL, FALSE);
}

int DboxMain::GetEntryImage(const CItemData &ci)
{
  int entrytype = ci.GetEntryType();
  if (entrytype == CItemData::ET_ALIAS) {
    return CPWTreeCtrl::ALIAS;
  }
  if (entrytype == CItemData::ET_SHORTCUT) {
    return CPWTreeCtrl::SHORTCUT;
  }

  time_t tXTime, now, warnexptime((time_t)0);
  time(&now);
  if (PWSprefs::GetInstance()->GetPref(PWSprefs::PreExpiryWarn)) {
    int idays = PWSprefs::GetInstance()->GetPref(PWSprefs::PreExpiryWarnDays);
    struct tm st;
#if _MSC_VER >= 1400
    errno_t err;
    err = localtime_s(&st, &now);  // secure version
    ASSERT(err == 0);
#else
    st = *localtime(&now);
    ASSERT(st != NULL); // null means invalid time
#endif
    st.tm_mday += idays;
    warnexptime = mktime(&st);

    if (warnexptime == (time_t)-1)
      warnexptime = (time_t)0;
  }

  int nImage;
  switch (entrytype) {
    case CItemData::ET_NORMAL:
      nImage = CPWTreeCtrl::NORMAL;
      break;
    case CItemData::ET_ALIASBASE:
      nImage = CPWTreeCtrl::ALIASBASE;
      break;
    case CItemData::ET_SHORTCUTBASE:
      nImage = CPWTreeCtrl::SHORTCUTBASE;
      break;
    default:
      nImage = CPWTreeCtrl::NORMAL;
  }

  ci.GetXTime(tXTime);
  if ((long)tXTime > 0L && (long)tXTime <= 3650L) {
    time_t tCPMTime;
    ci.GetPMTime(tCPMTime);
    if ((long)tCPMTime == 0L)
      ci.GetCTime(tCPMTime);
    tXTime = (time_t)((long)tCPMTime + (long)tXTime * 86400);
  }

  if (tXTime != 0) {
    if (tXTime <= now) {
      nImage += 2;  // Expired
    } else if (tXTime < warnexptime) {
      nImage += 1;  // Warn nearly expired
    }
  }
  return nImage;
}

void DboxMain::SetEntryImage(const int &index, const int nImage, const bool bOneEntry)
{
  if (!m_bImageInLV)
    return;

  m_ctlItemList.SetItem(index, 0, LVIF_IMAGE, NULL, nImage, 0, 0, 0);

  if (bOneEntry) {
    CRect rect;
    m_ctlItemList.GetItemRect(index, &rect, LVIR_ICON);
    m_ctlItemList.InvalidateRect(&rect);
  }
}

void DboxMain::SetEntryImage(HTREEITEM &ti, const int nImage, const bool bOneEntry)
{
  m_ctlItemTree.SetItemImage(ti, nImage, nImage);

  if (bOneEntry) {
    CRect rect;
    m_ctlItemTree.GetItemRect(ti, &rect, FALSE);
    m_ctlItemTree.InvalidateRect(&rect);
  }
}

void DboxMain::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpdis)
{
  if (lpdis == NULL || lpdis->CtlType != ODT_MENU) {
    // not for an ownerdrawn menu
    CDialog::OnDrawItem(nIDCtl, lpdis);
    return;
  }

  if (lpdis->rcItem.left != 2) {
    lpdis->rcItem.left -= (lpdis->rcItem.left - 2);
    lpdis->rcItem.right -= 60;
    if (lpdis->itemState & ODS_SELECTED) {
      lpdis->rcItem.left++;
      lpdis->rcItem.right++;
    }
  }

  CRUEItemData *pmd;
  pmd = (CRUEItemData *)lpdis->itemData;
  if (!pmd || !pmd->IsRUEID() || pmd->nImage < 0)
    return;

  HICON hIcon = GetEntryIcon(pmd->nImage);
  if (hIcon) {
    ICONINFO iconinfo;
    ::GetIconInfo(hIcon, &iconinfo);

    BITMAP bitmap;
    ::GetObject(iconinfo.hbmColor, sizeof(bitmap), &bitmap);

    ::DeleteObject(iconinfo.hbmColor);
    ::DeleteObject(iconinfo.hbmMask);

    ::DrawIconEx(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top, hIcon,
                 bitmap.bmWidth, bitmap.bmHeight, 
                 0, NULL, DI_IMAGE /*NORMAL*/);

    ::DeleteObject(hIcon);
  }
}

void DboxMain::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpmis)
{
  if (lpmis == NULL || lpmis->CtlType != ODT_MENU) {
    // not for an ownerdrawn menu
    CDialog::OnMeasureItem(nIDCtl, lpmis);
    return;
  }

  lpmis->itemWidth = 16;
  lpmis->itemHeight = 16;

  CRUEItemData *pmd;
  pmd = (CRUEItemData *)lpmis->itemData;
  if (!pmd || !pmd->IsRUEID() || pmd->nImage < 0)
    return;

  HICON hIcon = GetEntryIcon(pmd->nImage);
  if (hIcon) {
    ICONINFO iconinfo;
    ::GetIconInfo(hIcon, &iconinfo);

    BITMAP bitmap;
    ::GetObject(iconinfo.hbmColor, sizeof(bitmap), &bitmap);

    ::DeleteObject(iconinfo.hbmColor);
    ::DeleteObject(iconinfo.hbmMask);

    lpmis->itemWidth = bitmap.bmWidth;
    lpmis->itemHeight = bitmap.bmHeight;
  }
}

HICON DboxMain::GetEntryIcon(const int nImage) const
{
  int nID;
  switch (nImage) {
    case CPWTreeCtrl::NORMAL:
      nID = IDI_NORMAL;
      break;
    case CPWTreeCtrl::WARNEXPIRED_NORMAL:
      nID = IDI_NORMAL_WARNEXPIRED;
      break;
    case CPWTreeCtrl::EXPIRED_NORMAL:
      nID = IDI_NORMAL_EXPIRED;
      break;
    case CPWTreeCtrl::ALIASBASE:
      nID = IDI_ABASE;
      break;
    case CPWTreeCtrl::WARNEXPIRED_ALIASBASE:
      nID = IDI_ABASE_WARNEXPIRED;
      break;
    case CPWTreeCtrl::EXPIRED_ALIASBASE:
      nID = IDI_ABASE_EXPIRED;
      break;
    case CPWTreeCtrl::ALIAS:
      nID = IDI_ALIAS;
      break;
    case CPWTreeCtrl::SHORTCUTBASE:
      nID = IDI_SBASE;
      break;
    case CPWTreeCtrl::WARNEXPIRED_SHORTCUTBASE:
      nID = IDI_SBASE_WARNEXPIRED;
      break;
    case CPWTreeCtrl::EXPIRED_SHORTCUTBASE:
      nID = IDI_SBASE_EXPIRED;
      break;
    case CPWTreeCtrl::SHORTCUT:
      nID = IDI_SHORTCUT;
      break;
    default:
      nID = IDI_NORMAL;
  }
  HICON hIcon = (HICON)::LoadImage(::AfxGetResourceHandle(), MAKEINTRESOURCE(nID), 
    IMAGE_ICON, 0, 0, 
    LR_LOADMAP3DCOLORS | LR_SHARED);
  return hIcon;
}

bool DboxMain::SetNotesWindow(const CPoint point, const bool bVisible)
{
  CItemData *ci(NULL);
  CPoint target(point);
  StringX cs_notes(_T(""));
  UINT nFlags;
  HTREEITEM hItem(NULL);
  int nItem(-1);

  if (m_pNotesDisplay == NULL)
    return false;

  if (!bVisible) {
    m_pNotesDisplay->SetWindowText(cs_notes.c_str());
    m_pNotesDisplay->ShowWindow(SW_HIDE);
    return false;
  }

  if (m_ctlItemTree.IsWindowVisible()) {
    m_ctlItemTree.ClientToScreen(&target);
    hItem = m_ctlItemTree.HitTest(point, &nFlags);
    if (hItem != NULL &&
        (nFlags & (TVHT_ONITEM | TVHT_ONITEMBUTTON | TVHT_ONITEMINDENT))) {
      ci = (CItemData *)m_ctlItemTree.GetItemData(hItem);
    }
  } else {
    m_ctlItemList.ClientToScreen(&target);
    nItem = m_ctlItemList.HitTest(point, &nFlags);
    if (nItem >= 0) {
      ci = (CItemData *)m_ctlItemList.GetItemData(nItem);
    }
  }
  target.y += ::GetSystemMetrics(SM_CYCURSOR); // height of cursor

  if (ci != NULL) {
    if (ci->IsShortcut()) {
      // This is an shortcut
      uuid_array_t entry_uuid, base_uuid;
      ci->GetUUID(entry_uuid);
      GetShortcutBaseUUID(entry_uuid, base_uuid);

      ItemListIter iter = Find(base_uuid);
      if (iter != End()) {
        ci = &iter->second;
      }
    }
    cs_notes = ci->GetNotes();
  }

  if (!cs_notes.empty()) {
    Replace(cs_notes, StringX(_T("\r\n")), StringX(_T("\n")));
    Remove(cs_notes, _T('\r'));

    if (cs_notes.length() > 180)
      cs_notes = cs_notes.substr(0, 180) + _T("[...]");
  }

  // move window
  CString cs_oldnotes;
  m_pNotesDisplay->GetWindowText(cs_oldnotes);
  if (LPCTSTR(cs_oldnotes) != cs_notes)
    m_pNotesDisplay->SetWindowText(cs_notes.c_str());

  m_pNotesDisplay->SetWindowPos(NULL, target.x, target.y, 0, 0,
                                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  m_pNotesDisplay->ShowWindow(!cs_notes.empty() ? SW_SHOWNA : SW_HIDE);

  return !cs_notes.empty();
}

CItemData *DboxMain::GetLastSelected()
{
  CItemData *retval(NULL);
  if (m_core.GetNumEntries() == 0)
    return retval;

  if (m_ctlItemTree.IsWindowVisible()) {
    HTREEITEM hSelected = m_ctlItemTree.GetSelectedItem();
    if (hSelected != NULL)
      retval = (CItemData *)m_ctlItemTree.GetItemData(hSelected);
  } else {
    POSITION pSelected = m_ctlItemList.GetFirstSelectedItemPosition();
    if (pSelected != NULL)
      retval = (CItemData *)m_ctlItemList.GetItemData((int)pSelected - 1);
  }
  return retval;
}

StringX DboxMain::GetGroupName()
{
  HTREEITEM hi = m_ctlItemTree.GetSelectedItem();
  StringX s(_T(""));
  if (hi != NULL)
    s = m_ctlItemTree.GetItemText(hi);

  if ((GetKeyState(VK_CONTROL) & 0x8000) == 0) {
    while ((hi = m_ctlItemTree.GetParentItem(hi)) != NULL) {
      s = StringX(m_ctlItemTree.GetItemText(hi)) + StringX(_T(".")) + s;
    }
  }
  return s;
}