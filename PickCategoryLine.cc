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

#include "PickCategoryLine.h"
#include "package_db.h"
#include "PickView.h"
#include "window.h"

const char *
PickCategoryLine::text (int col_num)
{
  if (col_num == 0) // category
    {
      std::string s = (collapsed ? "+" : "-") + cat.first;
      return s.c_str();
    }
  else if (col_num == 2) // action
    {
      return current_default.caption ();
    }
  return "";
}

bool
PickCategoryLine::click(int col_num)
{
  if (col_num == 0)
    {
      collapsed = !collapsed;
      theView.refresh();
    }
  else if (col_num == 2)
    {
      ++current_default;
      set_action (current_default);
    }
  return true;
}

const char *
PickCategoryLine::tooltip(int col_num)
{
  return "no tooltip";
}

void
PickCategoryLine::set_action (packagemeta::_actions action)
{
  theView.GetParent ()->SetBusy ();
  current_default = action;

  // apply action to all contained packages
  // for (size_t n = 0; n < bucket.size (); n++)
  //      bucket[n]->set_action (current_default);

  theView.GetParent ()->ClearBusy ();
}
