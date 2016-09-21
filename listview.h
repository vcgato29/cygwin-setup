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

#ifndef SETUP_LISTVIEW_H
#define SETUP_LISTVIEW_H

#include "win32.h"
#include <vector>
#include <window.h>

// ---------------------------------------------------------------------------
// interface to class ListView
//
// ListView Common Control
// ---------------------------------------------------------------------------

class ListViewLine
{
 public:
  virtual const char *text(int col) = 0;
  virtual const char *tooltip(int col) = 0;
  virtual bool click(int col) = 0;
};

typedef std::vector<ListViewLine *> ListViewContents;

class ListView : Window
{
 public:
  class Header;
  typedef Header *HeaderList;

  void init(Window *parent);

  void empty(void);
  void initColumns(HeaderList hl);
  void noteColumnWidth(int col_num, const std::string& string);
  void resizeColumns(void);

  void set_contents(ListViewContents *contents);

  void setemptytext(const char *text);

  bool OnMessageCmd (int id, HWND hwndctl, UINT code);
  LRESULT OnNotify (NMHDR *pNmHdr);

  class Header
  {
  public:
    const char *text;
    int width;
    int fmt;
    int hdr_width;
  };

  LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

 private:
  HWND hWndParent;
  HWND hWndListView;

  ListViewContents *contents;
  HeaderList headers;
  const char *empty_list_text;
};

#endif /* SETUP_LISTVIEW_H */
