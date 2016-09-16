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

// ---------------------------------------------------------------------------
// interface to class ListView
//
// ListView Common Control
// ---------------------------------------------------------------------------

class ListView
{
 public:
  class Header;
  typedef Header *HeaderList;

  void init(HWND parent);
  void initColumns(HeaderList hl);
  void addRow(int index, const char *text);

  class Header
  {
  public:
    const char *text;
    int width;
    int fmt;
  };

 private:
  HWND hWndParent;
  HWND hWndListView;
};

#endif /* SETUP_LISTVIEW_H */
