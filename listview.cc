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
  {"Package",     LVCFMT_LEFT,  0},
  {"Current",     LVCFMT_LEFT,  0},
  {"New",         LVCFMT_LEFT,  0},
  {"Bin?",        LVCFMT_LEFT,  0},
  {"Src?",        LVCFMT_LEFT,  0},
  {"Categories",  LVCFMT_LEFT,  0},
  {"Size",        LVCFMT_RIGHT, 0},
  {"Description", LVCFMT_LEFT,  0},
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


  // give the header control a border
  HWND hWndHeader = ListView_GetHeader(hWndListView);
  SetWindowLongPtr(hWndHeader, GWL_STYLE,
                   GetWindowLongPtr(hWndHeader, GWL_STYLE) | WS_BORDER);

  // ensure an initial item exists for width calculations...
  LVITEM lvi;
  lvi.mask = LVIF_TEXT;
  lvi.iItem = 0;
  lvi.iSubItem = 0;
  lvi.pszText = (char *)"Working...";
  (void)ListView_InsertItem(hWndListView, &lvi);

  // populate
  initColumns(pkg_headers);
}

void
ListView::initColumns(HeaderList headers_)
{
  // store HeaderList for later use
  headers = headers_;

  // create the columns
  LVCOLUMN lvc;
  lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

  int i;
  for (i = 0; headers[i].text != 0; i++)
    {
      lvc.iSubItem = i;
      lvc.pszText = (char *)(headers[i].text);
      lvc.cx = 100;
      lvc.fmt = headers[i].fmt;

      if (ListView_InsertColumn(hWndListView, i, &lvc) == -1)
        printf("ListView_InsertColumn failed");
    }

  // now do some width calculations
  for (i = 0; headers[i].text != 0; i++)
    {
      headers[i].width = 0;

      if (ListView_SetColumnWidth(hWndListView, i, LVSCW_AUTOSIZE_USEHEADER) == -1)
        printf("ListView_SetColumnWidth failed");
      headers[i].hdr_width = ListView_GetColumnWidth(hWndListView, i);
    }
}

void
ListView::noteColumnWidth(int col_num, const std::string& string)
{
  ListView_SetItemText(hWndListView, 0, col_num, (char *)string.c_str());

  if (ListView_SetColumnWidth(hWndListView, col_num, LVSCW_AUTOSIZE_USEHEADER) == -1)
    printf("ListView_SetColumnWidth failed");

  int width = ListView_GetColumnWidth(hWndListView, col_num);

  if (width > headers[col_num].width)
    headers[col_num].width = width;
}

void
ListView::resizeColumns(void)
{
  // ensure the last column stretches all the way to the right-hand side of the
  // listview control
  int i;
  int total = 0;
  for (i = 0; headers[i].text != 0; i++)
    total = total + headers[i].width;

  RECT r;
  GetClientRect(hWndListView, &r);
  int width = r.right - r.left;

  if (total < width)
    headers[i-1].width += width - total;

  // size each column
  LVCOLUMN lvc;
  lvc.mask = LVCF_WIDTH | LVCF_MINWIDTH;
  for (i = 0; headers[i].text != 0; i++)
    {
      lvc.iSubItem = i;
      lvc.cx = headers[i].width;
      lvc.cxMin = headers[i].hdr_width;

      if (ListView_SetColumn(hWndListView, i, &lvc) == -1)
        printf("ListView_SetColumn failed");
    }
}

void
ListView::set_contents(ListViewContents *_contents)
{
  empty();
  contents = _contents;

  // disable redrawing of ListView
  // (otherwise it will redraw every time a row is added, which makes this very slow)
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
      if (contents)
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
      NMITEMACTIVATE *pNmItemAct = (NMITEMACTIVATE *) pNmHdr;
      Log (LOG_BABBLE) << "NM_CLICK: pnmitem->iItem " << pNmItemAct->iItem << " pNmItemAct->iSubItem " << pNmItemAct->iSubItem << endLog;

      if (pNmItemAct->iItem >= 0)
        {
          // Inform the item of the click
          bool update = (*contents)[pNmItemAct->iItem]->click(pNmItemAct->iSubItem);

          // Update the item, if needed
          if (update)
            {
              if (!ListView_RedrawItems(hWndListView, pNmItemAct->iItem, pNmItemAct->iItem))
                Log (LOG_BABBLE) << "ListView_RedrawItems failed " << endLog;
            }
        }
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
