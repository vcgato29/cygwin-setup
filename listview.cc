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
  {"Package",     LVSCW_AUTOSIZE,           LVCFMT_LEFT},
  {"Current",     LVSCW_AUTOSIZE,           LVCFMT_LEFT},
  {"New",         LVSCW_AUTOSIZE,           LVCFMT_LEFT},
  {"Bin?",        LVSCW_AUTOSIZE_USEHEADER, LVCFMT_LEFT},
  {"Src?",        LVSCW_AUTOSIZE_USEHEADER, LVCFMT_LEFT},
  {"Categories",  LVSCW_AUTOSIZE,           LVCFMT_LEFT},
  {"Size",        LVSCW_AUTOSIZE,           LVCFMT_RIGHT},
  {"Description", LVSCW_AUTOSIZE,           LVCFMT_LEFT},
  {0, 0, 0}
};

void
ListView::init(HWND parent)
{
  hWndParent = parent;

  // locate the listview control
  hWndListView = ::GetDlgItem(parent, IDC_CHOOSE_LIST);

  SendMessage(hWndListView, CCM_SETVERSION, 6, 0);

  (void)ListView_SetExtendedListViewStyle(hWndListView,
                                          LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  // populate
  initColumns(pkg_headers);
}

void
ListView::initColumns(HeaderList hl)
{
  // store HeaderList for later use
  headers = hl;

  LVCOLUMN lvc;
  lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_MINWIDTH | LVCF_TEXT | LVCF_SUBITEM;

  int i;
  for (i = 0; hl[i].text != 0; i++)
    {
      lvc.iSubItem = i;
      lvc.pszText = (char *)(hl[i].text);
      lvc.cx = 100;
      lvc.cxMin = LVSCW_AUTOSIZE_USEHEADER;
      lvc.fmt = hl[i].fmt;

      if (ListView_InsertColumn(hWndListView, i, &lvc) == -1)
        printf("ListView_InsertColumn failed");
    }
}

void
ListView::resizeColumns(void)
{
  int i;
  for (i = 0; headers[i].text != 0; i++)
    {
      if (ListView_SetColumnWidth(hWndListView, i, headers[i].width) == -1)
        printf("ListView_SetColumnWidth failed");
    }
}

void
ListView::set_contents(ListViewContents *_contents)
{
  empty();
  contents = _contents;

  // disable redrawing of ListView
  // (otherwise it will redraw every time a row is added, which is makes this very slow)
  SendMessage(hWndListView, WM_SETREDRAW, FALSE, 0);

  size_t i;
  for (i = 0; i < contents->size();  i++)
    {
      LVITEM lvi;
      lvi.mask = LVIF_TEXT;
      lvi.iItem = i;
      lvi.iSubItem = 0;
      lvi.pszText = LPSTR_TEXTCALLBACK;

      int i = ListView_InsertItem(hWndListView, &lvi);

      if (i == -1)
        printf("ListView_InsertItem failed");
    }

  // enable redrawing of ListView and redraw
  SendMessage(hWndListView, WM_SETREDRAW, TRUE, 0);
  RedrawWindow(hWndListView, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

bool
ListView::OnMessageCmd (int id, HWND hwndctl, UINT code)
{
  // We don't care.
  return false;
}

bool
ListView::OnNotify (NMHDR *pNmHdr)
{
  if (pNmHdr->code == LVN_GETDISPINFO)
    {
      NMLVDISPINFO *pNmLvDispInfo = (NMLVDISPINFO *)pNmHdr;
      // Log (LOG_BABBLE) << "LVN_GETDISPINFO " << pNmLvDispInfo->item.iItem << endLog;
      pNmLvDispInfo->item.pszText = (char *) (*contents)[pNmLvDispInfo->item.iItem]->text(pNmLvDispInfo->item.iSubItem);
      return true;
    }
  else if (pNmHdr->code == LVN_GETEMPTYMARKUP)
    {
      NMLVEMPTYMARKUP *pnmMarkup = (NMLVEMPTYMARKUP*) pNmHdr;

      MultiByteToWideChar(CP_UTF8, 0,
                          empty_list_text, -1,
                          pnmMarkup->szMarkup, L_MAX_URL_LENGTH);

      return true;
    }
  else if (pNmHdr->code == NM_CLICK)
    {
      NMITEMACTIVATE *pnmitem = (NMITEMACTIVATE *) pNmHdr;
      Log (LOG_BABBLE) << "NM_CLICK: pnmitem->iItem " << pnmitem->iItem << endLog;

      int i;
      i = ListView_GetNextItem(hWndListView, -1, LVNI_FOCUSED);
      Log (LOG_BABBLE) << "NM_CLICK: focused item " << i << endLog;

      //ListView_SubItemHitTest(hWndListView, ...
    }

  // We don't care.
  return false;
}

void
ListView::empty(void)
{
  if (ListView_DeleteAllItems(hWndListView) == -1)
    printf("ListView_DeleteAllItems failed");
}

void
ListView::setemptytext(const char *text)
{
  empty_list_text = text;
}
