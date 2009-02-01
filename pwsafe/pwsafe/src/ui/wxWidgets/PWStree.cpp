/*
 * Copyright (c) 2003-2009 Rony Shapiro <ronys@users.sourceforge.net>.
 * All rights reserved. Use of the code is allowed under the
 * Artistic License 2.0 terms, as specified in the LICENSE file
 * distributed with this code, or available from
 * http://www.opensource.org/licenses/artistic-license-2.0.php
 */

/** \file PWSTreeCtrl.cpp
* 
*/
// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

////@begin includes
////@end includes

#include "PWStree.h"

////@begin XPM images
////@end XPM images


/*!
 * PWSTreeCtrl type definition
 */

IMPLEMENT_DYNAMIC_CLASS( PWSTreeCtrl, wxTreeCtrl )


/*!
 * PWSTreeCtrl event table definition
 */

BEGIN_EVENT_TABLE( PWSTreeCtrl, wxTreeCtrl )

////@begin PWSTreeCtrl event table entries
////@end PWSTreeCtrl event table entries

END_EVENT_TABLE()


/*!
 * PWSTreeCtrl constructors
 */

PWSTreeCtrl::PWSTreeCtrl()
{
  Init();
}

PWSTreeCtrl::PWSTreeCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
{
  Init();
  Create(parent, id, pos, size, style);
}


/*!
 * PWSTreeCtrl creator
 */

bool PWSTreeCtrl::Create(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
{
////@begin PWSTreeCtrl creation
  wxTreeCtrl::Create(parent, id, pos, size, style);
  CreateControls();
////@end PWSTreeCtrl creation
  return true;
}


/*!
 * PWSTreeCtrl destructor
 */

PWSTreeCtrl::~PWSTreeCtrl()
{
////@begin PWSTreeCtrl destruction
////@end PWSTreeCtrl destruction
}


/*!
 * Member initialisation
 */

void PWSTreeCtrl::Init()
{
////@begin PWSTreeCtrl member initialisation
////@end PWSTreeCtrl member initialisation
}


/*!
 * Control creation for PWSTreeCtrl
 */

void PWSTreeCtrl::CreateControls()
{    
////@begin PWSTreeCtrl content construction
////@end PWSTreeCtrl content construction
}

// XXX taken from Windows PWSTreeCtrl.cpp
// XXX move to corelib
static StringX GetPathElem(StringX &path)
{
  // Get first path element and chop it off, i.e., if
  // path = "a.b.c.d"
  // will return "a" and path will be "b.c.d"
  // (assuming GROUP_SEP is '.')
  const char GROUP_SEP = '.';

  StringX retval;
  StringX::size_type N = path.find(GROUP_SEP);
  if (N == StringX::npos) {
    retval = path;
    path = "";
  } else {
    const StringX::size_type Len = path.length();
    retval = path.substr(0, N);
    path = path.substr(Len - N - 1);
  }
  return retval;
}

bool PWSTreeCtrl::ExistsInTree(wxTreeItemId node,
                               const StringX &s, wxTreeItemId &si)
{
  // returns true iff s is a direct descendant of node
  wxTreeItemIdValue cookie;
  wxTreeItemId ti = GetFirstChild(node, cookie);

  while (ti) {
    const wxString itemText = GetItemText(ti);
    if (itemText == s.c_str()) {
      si = ti;
      return true;
    }
    ti = GetNextChild(ti, cookie);
  }
  return false;
}


wxTreeItemId PWSTreeCtrl::AddGroup(const StringX &group)
{
  wxTreeItemId ti = GetRootItem();
  // Add a group at the end of path
  wxTreeItemId si;
  if (!group.empty()) {
    StringX path = group;
    StringX s;
    do {
      s = GetPathElem(path);
      if (!ExistsInTree(ti, s, si)) {
        ti = AppendItem(ti, s.c_str());
        // SetItemImage(ti, CPWTreeCtrl::NODE, CPWTreeCtrl::NODE);
      } else
        ti = si;
    } while (!path.empty());
  }
  return ti;
}

void PWSTreeCtrl::AddItem(const CItemData &item)
{
  wxString title = item.GetTitle().c_str();
  wxString user = item.GetUser().c_str();
  wxString disp = title;
  wxTreeItemId gnode = AddGroup(item.GetGroup());
  if (!user.empty())
    disp += " [" + user + "]";
  AppendItem(gnode, disp);
}
