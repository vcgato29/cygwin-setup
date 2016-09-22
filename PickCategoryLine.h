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

#ifndef SETUP_PICKCATEGORYLINE_H
#define SETUP_PICKCATEGORYLINE_H

#include "package_meta.h"
#include "ListView.h"
#include "PickView.h"

class PickCategoryLine: public ListViewLine
{
public:
  PickCategoryLine (PickView & aView, Category & _cat, bool &_collapsed) :
  theView (aView), cat (_cat), collapsed(_collapsed)
  {
  };
  ~PickCategoryLine ()
  {
  }

  const char *text(int col);
  const char *tooltip(int col);
  bool click(int col);

private:
  void set_action (packagemeta::_actions);

  PickView & theView;
  Category & cat;
  bool & collapsed;

  packagemeta::_actions current_default;
};
#endif /* SETUP_PICKCATEGORYLINE_H */
