/*
 * Copyright (c) 2002 Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <robertc@hotmail.com>
 *
 */

#ifndef SETUP_PICKVIEW_H
#define SETUP_PICKVIEW_H

#include <string>
#include "win32.h"
#include "window.h"
//#include "RECTWrapper.h"

/* #define HMARGIN         10 */
/* #define ROW_MARGIN      5 */
/* #define ICON_MARGIN     4 */
/* #define SPIN_WIDTH      11 */
/* #define CHECK_SIZE      11 */
/* #define TREE_INDENT     12 */

/* #define CATEGORY_EXPANDED  0 */
/* #define CATEGORY_COLLAPSED 1 */

#include "package_meta.h"
#include "listview.h"

class PickView : public Window
{
public:
  enum class views;
  //  class Header;
  //  int num_columns;
  void defaultTrust (trusts trust);
  void setViewMode (views mode);
  views getViewMode ();
  //  Header *headers;
  PickView ();
  void init(views _mode, ListView *_listview);
  ~PickView();
  static const char *mode_caption (views mode);
  void setObsolete (bool doit);
  void insert_pkg (packagemeta &);
  void insert_category (Category *, bool);
  void refresh();
  //  int row_height;
  //  TEXTMETRIC tm;
  //  HDC bitmap_dc, icon_dc;
  //  HBITMAP bm_icon;
  //  HRGN rect_icon;
  //  HBRUSH bg_fg_brush;
  //  HANDLE bm_spin, bm_treeplus, bm_treeminus;
  trusts deftrust;
  //  int scroll_ulc_x, scroll_ulc_y;
  //  int header_height;
  //  void scroll (HWND hwnd, int which, int *var, int code, int howmany);

  void SetPackageFilter (const std::string &filterString)
  {
    packageFilterString = filterString;
  }

  enum class views
  {
    PackageFull = 0,
    PackagePending,
    PackageKeeps,
    PackageSkips,
    PackageUserPicked,
    Category,
  };

  /* class Header */
  /* { */
  /* public: */
  /*   const char *text; */
  /*   int width; */
  /*   int x; */
  /*   bool needs_clip; */
  /* }; */

private:
  views view_mode;
  ListView *listview;
  bool showObsolete;
  std::string packageFilterString;
  ListViewContents *contents;

  // Stuff needed to handle resizing
  //  bool hasWindowRect;
  //  RECTWrapper lastWindowRect;
  //  int total_delta_x;

  int set_header_column_order (views vm);
  void set_headers ();
  void init_headers ();
};

bool isObsolete (std::set <std::string, casecompare_lt_op> &categories);
bool isObsolete (const std::string& catname);

#endif /* SETUP_PICKVIEW_H */
