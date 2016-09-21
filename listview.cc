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

  // configure the listview control
  SendMessage(hWndListView, CCM_SETVERSION, 6, 0);

  (void)ListView_SetExtendedListViewStyle(hWndListView,
                                          LVS_EX_COLUMNSNAPPOINTS | // use cxMin
                                          LVS_EX_FULLROWSELECT |
                                          LVS_EX_GRIDLINES |
                                          LVS_EX_HEADERDRAGDROP);   // headers can be re-ordered

  // LVS_EX_INFOTIP);          // enable tooltips
  // LVS_EX_LABELTIP);         // ensure tooltip is not partially hidden

  // tooltips
  //  ActivateTooltips();
  HWND TooltipHandle = CreateWindowEx (WS_EX_TOPMOST,
                                       (LPCTSTR) TOOLTIPS_CLASS,
                                       NULL,
                                       WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT,
                                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                       hWndListView,
                                       (HMENU) 0,
                                       GetModuleHandle(NULL),
                                       NULL);

  TOOLINFO ti;
  memset ((void *)&ti, 0, sizeof(ti));
  ti.cbSize = sizeof(ti);
  ti.uFlags = TTF_IDISHWND;
  ti.hwnd = hWndListView;
  ti.uId = (UINT_PTR)hWndListView;
  // setting hwnd and ui to same window isn't explicitly documented, but seems to work
  ti.lpszText = (LPTSTR)"some tooltip"; //LPSTR_TEXTCALLBACK; // use TTN_GETDISPINFO
  //::GetClientRect (hWndListView, &ti.rect); // XXX: needs to be updated when window changes size...
  SendMessage (TooltipHandle, TTM_ADDTOOL, 0, (LPARAM)&ti);

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
  HDC dc = GetDC (hWndListView);
  SIZE s = { 0, 0 };

  // A margin of 3*GetSystemMetrics(SM_CXEDGE) is used at each side of the
  // header text.
  int addend = 2*3*GetSystemMetrics(SM_CXEDGE);

  // we must set the font of the DC here, otherwise the width calculations
  // will be off because the system will use the wrong font metrics
  HANDLE sysfont = GetStockObject (DEFAULT_GUI_FONT);
  SelectObject (dc, sysfont);

  if (string.size())
    GetTextExtentPoint32 (dc, string.c_str(), string.size(), &s);

  int width = addend + s.cx;

  if (width > headers[col_num].width)
    headers[col_num].width = width;

  ReleaseDC(hWndListView, dc);
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
      //      Log (LOG_BABBLE) << "resizeColumns: " << i << " cx " << lvc.cx << " cxMin " << lvc.cxMin <<endLog;

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
#if DEBUG
  Log (LOG_BABBLE) << "ListView::OnMesageCmd " << id << " " << hwndctl << " " << code << endLog;
#endif

  // We don't care.
  return false;
}

LRESULT
ListView::OnNotify (NMHDR *pNmHdr)
{
#if DEBUG
  Log (LOG_BABBLE) << "ListView::OnNotify id:" << pNmHdr->idFrom << " hwnd:" << pNmHdr->hwndFrom << " code:" << pNmHdr->code << endLog;
#endif

  switch (pNmHdr->code)
  {
  case LVN_GETDISPINFO:
    {
      NMLVDISPINFO *pNmLvDispInfo = (NMLVDISPINFO *)pNmHdr;
      // Log (LOG_BABBLE) << "LVN_GETDISPINFO " << pNmLvDispInfo->item.iItem << endLog;
      if (contents)
        pNmLvDispInfo->item.pszText = (char *) (*contents)[pNmLvDispInfo->item.iItem]->text(pNmLvDispInfo->item.iSubItem);
      return true;
    }
    break;

  // case LVN_GETINFOTIP:
  //   {
  //     NMLVGETINFOTIP *pNmLvGetInfoTip = (NMLVGETINFOTIP *)pNmHdr;
  //     Log (LOG_BABBLE) << "LVN_GETINFOTIP " << pNmLvGetInfoTip->iItem << " " << pNmLvGetInfoTip->iSubItem << endLog;

  //     if (contents)
  //       pNmLvGetInfoTip->pszText = (char *) (*contents)[pNmLvGetInfoTip->iItem]->tooltip(pNmLvGetInfoTip->iSubItem);

  //     return true;
  //   }
  //   break;

  case TTN_GETDISPINFO:
    {
      // LVN_GETINFOTIP doesn't work for subitems, so we have to do our own
      // tooltip handling

      // convert mouse position to item/subitem
      LVHITTESTINFO lvHitTestInfo;
      lvHitTestInfo.flags = LVHT_ONITEM;
      GetCursorPos(&lvHitTestInfo.pt);
      ::ScreenToClient(hWndListView, &lvHitTestInfo.pt);
      if (ListView_HitTest(hWndListView,&lvHitTestInfo) == -1)
        printf("ListView_HitTest failed");

      Log (LOG_BABBLE) << "TTN_GETDISPINFO " << lvHitTestInfo.iItem << " " << lvHitTestInfo.iSubItem << endLog;

      // get the tooltip text for that item/subitem
      const char *pszText = "";
      if (contents)
        pszText = (*contents)[lvHitTestInfo.iItem]->tooltip(lvHitTestInfo.iSubItem);

      // Set the tooltip text
      NMTTDISPINFO *pNmTTDispInfo = (NMTTDISPINFO *) pNmHdr;
      pNmTTDispInfo->lpszText = (char *)pszText;
      pNmTTDispInfo->hinst = NULL;
      pNmTTDispInfo->uFlags = 0;

      return true;
    }
    break;

  case LVN_GETEMPTYMARKUP:
    {
      NMLVEMPTYMARKUP *pNmMarkup = (NMLVEMPTYMARKUP*) pNmHdr;

      MultiByteToWideChar(CP_UTF8, 0,
                          empty_list_text, -1,
                          pNmMarkup->szMarkup, L_MAX_URL_LENGTH);

      pNmMarkup->dwFlags = EMF_CENTERED;
      wcscpy(pNmMarkup->szMarkup, L"Link one and two.");

      return true;
    }
    break;

  case NM_CLICK:
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
      return true;
    }
    break;

  case NM_CUSTOMDRAW:
    {
      NMLVCUSTOMDRAW *pNmLvCustomDraw = (NMLVCUSTOMDRAW *)pNmHdr;

      switch(pNmLvCustomDraw->nmcd.dwDrawStage)
        {
        case CDDS_PREPAINT:
          return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT:
          {
            COLORREF crText, crBkgnd;
            int iRow = pNmLvCustomDraw->nmcd.dwItemSpec;
            Log (LOG_BABBLE) << "NM_CUSTOMDRAW: stage " << pNmLvCustomDraw->nmcd.dwDrawStage << " row:" << iRow << endLog;

            if ((iRow % 2) == 0)
              {
                crText = RGB(255,0,0);
                crBkgnd = RGB(128,128,255);
              }
            else
              {
                crText = RGB(0,255,0);
                crBkgnd = RGB(255,0,0);
            }

            pNmLvCustomDraw->clrText = crText;
            pNmLvCustomDraw->clrTextBk = crBkgnd;

            return CDRF_NEWFONT;
          }
          //          return CDRF_NOTIFYSUBITEMDRAW;
        case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
          {
            // int iCol = pNmLvCustomDraw->iSubItem;
            int iRow = pNmLvCustomDraw->nmcd.dwItemSpec;
            pNmLvCustomDraw->clrTextBk = RGB(10+iRow*10, 10+iRow*10, 10+iRow*10);
            return CDRF_NEWFONT;
          }
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
