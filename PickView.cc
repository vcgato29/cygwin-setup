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

#include "PickView.h"
#include <algorithm>
#include <limits.h>
#include <shlwapi.h>

#include "package_db.h"
#include "package_version.h"
#include "dialog.h"
#include "resource.h"
/* For 'source' */
#include "state.h"
#include "LogSingleton.h"

using namespace std;

static PickView::Header pkg_headers[] = {
  {"Current", 0, 0, true},
  {"New", 0, 0, true},
  {"Bin?", 0, 0, false},
  {"Src?", 0, 0, false},
  {"Categories", 0, 0, true},
  {"Size", 0, 0, true},
  {"Package", 0, 0, true},
  {0, 0, 0, false}
};

static PickView::Header cat_headers[] = {
  {"Category", 0, 0, true},
  {"Current", 0, 0, true},
  {"New", 0, 0, true},
  {"Bin?", 0, 0, false},
  {"Src?", 0, 0, false},
  {"Size", 0, 0, true},
  {"Package", 0, 0, true},
  {0, 0, 0, false}
};

int
PickView::set_header_column_order (views vm)
{
  if (vm == views::PackageFull || vm == views::PackagePending
      || vm == views::PackageKeeps || vm == views::PackageSkips
      || vm == views::PackageUserPicked)
    {
      headers = pkg_headers;
      current_col = 0;
      new_col = 1;
      bintick_col = new_col + 1;
      srctick_col = bintick_col + 1;
      cat_col = srctick_col + 1;
      size_col = cat_col + 1;
      pkg_col = size_col + 1;
      last_col = pkg_col;
    }
  else if (vm == views::Category)
    {
      headers = cat_headers;
      cat_col = 0;
      current_col = 1;
      new_col = current_col + 1;
      bintick_col = new_col + 1;
      srctick_col = bintick_col + 1;
      size_col = srctick_col + 1;
      pkg_col = size_col + 1;
      last_col = pkg_col;
    }
  else
    return -1;
  return last_col;
}

void
PickView::set_headers ()
{
#if 0
  if (set_header_column_order (view_mode) == -1)
    return;
  while (int n = SendMessage (listheader, HDM_GETITEMCOUNT, 0, 0))
    {
      SendMessage (listheader, HDM_DELETEITEM, n - 1, 0);
    }
  int i;
  for (i = 0; i <= last_col; i++)
    DoInsertItem (listheader, i, headers[i].width, (char *) headers[i].text);
#endif
}

void
PickView::note_width (PickView::Header *hdrs, HDC dc,
                      const std::string& string, int addend, int column)
{
  SIZE s = { 0, 0 };

  if (string.size())
    GetTextExtentPoint32 (dc, string.c_str(), string.size(), &s);
  if (hdrs[column].width < s.cx + addend)
    hdrs[column].width = s.cx + addend;
}

void
PickView::setViewMode (views mode)
{
  view_mode = mode;
  set_headers ();
  packagedb db;

  listview->empty ();
  if (view_mode == PickView::views::Category)
    {
      // contents.ShowLabel (true);
      /* start collapsed. TODO: make this a chooser flag */
      for (packagedb::categoriesType::iterator n =
            packagedb::categories.begin(); n != packagedb::categories.end();
            ++n)
        insert_category (&*n, (*n).first.c_str()[0] == '.'
				? CATEGORY_EXPANDED : CATEGORY_COLLAPSED);
    }
  else
    {
      // contents.ShowLabel (false);
      // iterate through every package
      for (packagedb::packagecollection::iterator i = db.packages.begin ();
            i != db.packages.end (); ++i)
        {
          packagemeta & pkg = *(i->second);

          if ( // "Full" : everything
              (view_mode == PickView::views::PackageFull)

              // "Pending" : packages that are being added/removed/upgraded
              || (view_mode == PickView::views::PackagePending &&
                  ((!pkg.desired && pkg.installed) ||         // uninstall
                    (pkg.desired &&
                      (pkg.desired.picked () ||               // install bin
                       pkg.desired.sourcePackage ().picked ())))) // src
              
              // "Up to date" : installed packages that will not be changed
              || (view_mode == PickView::views::PackageKeeps &&
                  (pkg.installed && pkg.desired && !pkg.desired.picked ()
                    && !pkg.desired.sourcePackage ().picked ()))

              // "Not installed"
              || (view_mode == PickView::views::PackageSkips &&
                  (!pkg.desired && !pkg.installed))

              // "UserPick" : installed packages that were picked by user
              || (view_mode == PickView::views::PackageUserPicked &&
                  (pkg.installed && pkg.user_picked)))
            {
              // Filter by package name
              if (packageFilterString.empty ()
		  || StrStrI (pkg.name.c_str (), packageFilterString.c_str ()))
                insert_pkg (pkg);
            }
        }
    }
}

PickView::views
PickView::getViewMode ()
{
  return view_mode;
}

const char *
PickView::mode_caption (views mode)
{
  switch (mode)
    {
    case views::PackageFull:
      return "Full";
    case views::PackagePending:
      return "Pending";
    case views::PackageKeeps:
      return "Up To Date";
    case views::PackageSkips:
      return "Not Installed";
    case views::PackageUserPicked:
      return "Picked";
    case views::Category:
      return "Category";
    default:
      return "";
    }
}

/* meant to be called on packagemeta::categories */
bool
isObsolete (set <std::string, casecompare_lt_op> &categories)
{
  set <std::string, casecompare_lt_op>::const_iterator i;
  
  for (i = categories.begin (); i != categories.end (); ++i)
    if (isObsolete (*i))
      return true;
  return false;
}

bool
isObsolete (const std::string& catname)
{
  if (casecompare(catname, "ZZZRemovedPackages") == 0 
        || casecompare(catname, "_", 1) == 0)
    return true;
  return false;
}

/* Sets the mode for showing/hiding obsolete junk packages.  */
void
PickView::setObsolete (bool doit)
{
  showObsolete = doit;
  refresh ();
}

void
PickView::insert_pkg (packagemeta & pkg)
{
  if (!showObsolete && isObsolete (pkg.categories))
    return;

  int row = listview->insert (pkg.name.c_str());

  const char *bintick = "?";
  if (/* uninstall or skip */ !pkg.desired ||
      /* current version */ pkg.desired == pkg.installed ||
      /* no source */ !pkg.desired.accessible())
    bintick = "n/a";
  else if (pkg.desired.picked())
    bintick = "yes";
  else
    bintick = "no";

  const char *srctick = "?";
  if ( /* uninstall */ !pkg.desired ||
       /* when no source mirror available */
       !pkg.desired.sourcePackage().accessible())
    srctick = "n/a";
  else if (pkg.desired.sourcePackage().picked())
    srctick = "yes";
  else
    srctick = "no";

  int sz = 0;
  packageversion picked;

  /* Find the size of the package.  If user has chosen to upgrade/downgrade
     the package, use that version.  Otherwise use the currently installed
     version, or if not installed then use the version that would be chosen
     based on the current trust level (curr/prev/test).  */
  if (pkg.desired)
    picked = pkg.desired;
  else if (pkg.installed)
    picked = pkg.installed;
  else
    picked = pkg.trustp (false, deftrust);

  /* Include the size of the binary package, and if selected, the source
     package as well.  */
  sz += picked.source()->size;
  if (picked.sourcePackage().picked())
    sz += picked.sourcePackage().source()->size;

  /* If size still 0, size must be unknown.  */
  std::string size = (sz == 0) ? "?" : format_1000s((sz+1023)/1024) + "k";

  listview->insert_column (row, 1, pkg.installed.Canonical_version ().c_str());
  listview->insert_column (row, 2, pkg.action_caption ().c_str());
  listview->insert_column (row, 3, bintick);
  listview->insert_column (row, 4, srctick);
  listview->insert_column (row, 5, pkg.getReadableCategoryList().c_str());
  listview->insert_column (row, 6, size.c_str());
  listview->insert_column (row, 7, pkg.SDesc().c_str());
}

void
PickView::insert_category (Category *cat, bool collapsed)
{
  // Urk, special case
  if (casecompare(cat->first, "All") == 0 ||
      (!showObsolete && isObsolete (cat->first)))
    return;

  int packageCount = 0;
  for (vector <packagemeta *>::iterator i = cat->second.begin ();
       i != cat->second.end () ; ++i)
    {
      if (packageFilterString.empty () \
          || (*i
	      && StrStrI ((*i)->name.c_str (), packageFilterString.c_str ())))
	{
	  packageCount++;
	}
    }

  if (packageFilterString.empty () || packageCount)
    listview->insert (cat->first.c_str());
}

/* this means to make the 'category' column wide enough to fit the first 'n'
   categories for each package.  */
#define NUM_CATEGORY_COL_WIDTH 2

void
PickView::init_headers (HDC dc)
{
#if 0  
  int i;

  for (i = 0; headers[i].text; i++)
    {
      headers[i].width = 0;
      headers[i].x = 0;
    }

  // A margin of 3*GetSystemMetrics(SM_CXEDGE) is used at each side of the
  // header text.  (Probably should use that rather than hard-coding HMARGIN
  // everywhere)
  int addend = 2*3*GetSystemMetrics(SM_CXEDGE);

  // accommodate widths of the 'bin' and 'src' checkbox columns
  note_width (headers, dc, headers[bintick_col].text, addend, bintick_col);
  note_width (headers, dc, headers[srctick_col].text, addend, srctick_col);

  // accomodate the width of each category name
  packagedb db;
  for (packagedb::categoriesType::iterator n = packagedb::categories.begin();
       n != packagedb::categories.end(); ++n)
    {
      if (!showObsolete && isObsolete (n->first))
        continue;
      note_width (headers, dc, n->first, HMARGIN, cat_col);
    }

  /* For each package, accomodate the width of the installed version in the
     current_col, the widths of all other versions in the new_col, and the
     width of the sdesc for the pkg_col.  Also, if this is not a Category
     view, adjust the 'category' column so that the first NUM_CATEGORY_COL_WIDTH
     categories from each package fits.  */
  for (packagedb::packagecollection::iterator n = db.packages.begin ();
       n != db.packages.end (); ++n)
    {
      packagemeta & pkg = *(n->second);
      if (!showObsolete && isObsolete (pkg.categories))
        continue;
      if (pkg.installed)
        note_width (headers, dc, pkg.installed.Canonical_version (),
                    HMARGIN, current_col);
      for (set<packageversion>::iterator i = pkg.versions.begin ();
	   i != pkg.versions.end (); ++i)
	{
          if (*i != pkg.installed)
            note_width (headers, dc, i->Canonical_version (),
                        HMARGIN + SPIN_WIDTH, new_col);
	  std::string z = format_1000s(packageversion(*i).source ()->size);
	  note_width (headers, dc, z, HMARGIN, size_col);
	  z = format_1000s(packageversion(i->sourcePackage ()).source ()->size);
	  note_width (headers, dc, z, HMARGIN, size_col);
	}
      std::string s = pkg.name;
      if (pkg.SDesc ().size())
	s += std::string (": ") + std::string(pkg.SDesc ());
      note_width (headers, dc, s, HMARGIN, pkg_col);
      
      if (view_mode != PickView::views::Category && pkg.categories.size () > 2)
        {
          std::string compound_cat("");          
          std::set<std::string, casecompare_lt_op>::const_iterator cat;
          size_t cnt;
          
          for (cnt = 0, cat = pkg.categories.begin (); 
               cnt < NUM_CATEGORY_COL_WIDTH && cat != pkg.categories.end ();
               ++cat)
            {
              if (casecompare(*cat, "All") == 0)
                continue;
              if (compound_cat.size ())
                compound_cat += ", ";
              compound_cat += *cat;
              cnt++;
            }
          note_width (headers, dc, compound_cat, HMARGIN, cat_col);
        }
    }
  
  // ensure that the new_col is wide enough for all the labels
  const char *captions[] = { "Uninstall", "Skip", "Reinstall", "Retrieve", 
                             "Source", "Keep", NULL };
  for (int i = 0; captions[i]; i++)
    note_width (headers, dc, captions[i], HMARGIN + SPIN_WIDTH, new_col);

  // finally, compute the actual x values based on widths
  headers[0].x = 0;
  for (i = 1; i <= last_col; i++)
    headers[i].x = headers[i - 1].x + headers[i - 1].width;
  // and allow for resizing to ensure the last column reaches
  // all the way to the end of the chooser box.
  headers[last_col].width += total_delta_x;
#endif
}


PickView::PickView() :
  deftrust (TRUST_UNKNOWN),
  showObsolete (false),
  packageFilterString ()
{
}

void
PickView::init(views _mode, ListView *_listview)
{
  view_mode = _mode;
  listview = _listview;
  refresh ();
}

PickView::~PickView()
{
}

void
PickView::defaultTrust (trusts trust)
{
  this->deftrust = trust;

  packagedb db;
  db.defaultTrust(trust);

  // force the picker to redraw
  RECT r = GetClientRect ();
  InvalidateRect (this->GetHWND(), &r, TRUE);
}

/* This recalculates all column widths and resets the view */
void
PickView::refresh()
{
  HDC dc = GetDC (GetHWND ());
  
  // we must set the font of the DC here, otherwise the width calculations
  // will be off because the system will use the wrong font metrics
  sysfont = GetStockObject (DEFAULT_GUI_FONT);
  SelectObject (dc, sysfont);

  // init headers for the current mode
  set_headers ();
  init_headers (dc);
  
  // save the current mode
  views cur_view_mode = view_mode;
  
  // switch to the other type and do those headers
  view_mode = (view_mode == PickView::views::Category) ? 
                    PickView::views::PackageFull : PickView::views::Category;
  set_headers ();
  init_headers (dc);
  ReleaseDC (GetHWND (), dc);

  view_mode = cur_view_mode;
  setViewMode (view_mode);
}
