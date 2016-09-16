/*
 * Copyright (c) 2016 Jon Turney
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#include "listview.h"
#include "dialog.h" // for hinstance
#include "LogSingleton.h"

#include <commctrl.h>
#include <resource.h>

// ---------------------------------------------------------------------------
// implements class ListView
//
// ListView Common Control
// ---------------------------------------------------------------------------

static ListView::Header pkg_headers[] = {
  {"Current", 0, LVCFMT_LEFT},
  {"New", 0, LVCFMT_LEFT},
  {"Bin?", 0, LVCFMT_LEFT},
  {"Src?", 0, LVCFMT_LEFT},
  {"Categories", 0, LVCFMT_LEFT},
  {"Size", 0, LVCFMT_RIGHT},
  {"Package", 0, LVCFMT_LEFT},
  {0, 0, 0}
};

void
ListView::init(HWND parent)
{
  hWndParent = parent;
  Log (LOG_PLAIN) << parent << endLog;

  // locate the listview control
  hWndListView = ::GetDlgItem(parent, IDC_CHOOSE_LIST);
  Log (LOG_PLAIN) << hWndListView << endLog;

  // populate
  initColumns(pkg_headers);

  insert("foo");
  insert("bah");
}

void
ListView::initColumns(HeaderList hl)
{
  LVCOLUMN lvc;
  lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

  int i;
  for (i = 0; hl[i].text != 0; i++)
    {
      lvc.iSubItem = i;
      lvc.pszText = (char *)(hl[i].text);
      lvc.cx = 100;
      lvc.fmt = hl[i].fmt;

      if (ListView_InsertColumn(hWndListView, i, &lvc) == -1)
        printf("ListView_InsertColumn failed");
    }
}

void
ListView::insert(const char *text)
{
  LVITEM lvi;
  lvi.mask = LVIF_TEXT;
  lvi.iItem = ListView_GetItemCount(hWndListView) + 1;
  lvi.iSubItem = 0;
  lvi.pszText = (char *)text;
  if (ListView_InsertItem(hWndListView, &lvi) == -1)
        printf("ListView_InsertItem failed");

  //  ListView_SetItemText(hWndListView, index, subite, text);
}

bool
ListView::OnMessageCmd (int id, HWND hwndctl, UINT code)
{
  // We don't care.
  return false;
}

void
ListView::empty(void)
{
  if (ListView_DeleteAllItems(hWndListView) == -1)
    printf("ListView_DeleteAllItems failed");
}
