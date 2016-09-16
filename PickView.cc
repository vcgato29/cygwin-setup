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
#include "PickPackageLine.h"
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

enum
  {
    pkgname_col = 0,
    current_col = 1,
    new_col = 2,
    bintick_col = 3,
    srctick_col = 4,
    cat_col = 5,
    size_col = 6,
    pkg_col = 7,  // actually desc
  };

#if 0
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
#endif

#if 0
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
#endif

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
PickView::setViewMode (views mode)
{
  view_mode = mode;
  //set_headers ();
  packagedb db;

  contents = new ListViewContents();

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

  listview->set_contents(contents);
  listview->resizeColumns();
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
isObsolete (std::set <std::string, casecompare_lt_op> &categories)
{
  std::set <std::string, casecompare_lt_op>::const_iterator i;
  
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

  contents->push_back(new PickPackageLine(*this, pkg));
}

void
PickView::insert_category (Category *cat, bool collapsed)
{
  // Urk, special case
  if (casecompare(cat->first, "All") == 0 ||
      (!showObsolete && isObsolete (cat->first)))
    return;

  int packageCount = 0;
  for (std::vector <packagemeta *>::iterator i = cat->second.begin ();
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
    {
      // listview->insert (cat->first.c_str());
    }
}

/* this means to make the 'category' column wide enough to fit the first 'n'
   categories for each package.  */
#define NUM_CATEGORY_COL_WIDTH 2

void
PickView::init_headers (void)
{
  // widths of the 'bin' and 'src' checkbox columns just need to accommodate the
  // column name
  listview->noteColumnWidth (bintick_col, "");
  listview->noteColumnWidth (srctick_col, "");

  // (In category view) accommodate the width of each category name
  packagedb db;
  for (packagedb::categoriesType::iterator n = packagedb::categories.begin();
       n != packagedb::categories.end(); ++n)
    {
      if (!showObsolete && isObsolete (n->first))
        continue;
      listview->noteColumnWidth (cat_col, n->first);
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
        listview->noteColumnWidth (current_col, pkg.installed.Canonical_version ());
      for (std::set<packageversion>::iterator i = pkg.versions.begin ();
           i != pkg.versions.end (); ++i)
        {
          if (*i != pkg.installed)
            listview->noteColumnWidth (new_col, i->Canonical_version ());
          std::string z = format_1000s(packageversion(*i).source ()->size);
          listview->noteColumnWidth (size_col, z);
          z = format_1000s(packageversion(i->sourcePackage ()).source ()->size);
          listview->noteColumnWidth (size_col, z);
        }
      std::string s = pkg.name;
      listview->noteColumnWidth (pkgname_col, s);

      s = pkg.SDesc ();
      listview->noteColumnWidth (pkg_col, s);

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
          listview->noteColumnWidth (cat_col, compound_cat);
        }
    }

  // ensure that the new_col is wide enough for all the labels
  const char *captions[] = { "Uninstall", "Skip", "Reinstall", "Retrieve",
                             "Source", "Keep", NULL };
  for (int i = 0; captions[i]; i++)
    listview->noteColumnWidth (cat_col, captions[i]);
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
  // init headers for the current mode
  // set_headers ();
  init_headers ();

  // save the current mode
  views cur_view_mode = view_mode;

  // switch to the other type and do those headers
  view_mode = (view_mode == PickView::views::Category) ?
                    PickView::views::PackageFull : PickView::views::Category;
  set_headers ();
  init_headers ();

  view_mode = cur_view_mode;
  setViewMode (view_mode);
}
