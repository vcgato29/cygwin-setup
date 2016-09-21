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

#include "PickPackageLine.h"
#include "PickView.h"
#include "package_db.h"
#include "package_version.h"

const char *
PickPackageLine::text(int col_num)
{
  if (col_num == 0)
    {
      return pkg.name.c_str();
    }
  else if (col_num == 1)
    {
      return pkg.installed.Canonical_version ().c_str();
    }
  else if (col_num == 2)
    {
      return pkg.action_caption ().c_str();
    }
  else if (col_num == 3)
    {
      const char *bintick = "?";
      if (/* uninstall or skip */ !pkg.desired ||
          /* current version */ pkg.desired == pkg.installed ||
          /* no source */ !pkg.desired.accessible())
        bintick = "n/a";
      else if (pkg.desired.picked())
        bintick = "yes";
      else
        bintick = "no";

      return bintick;
    }
  else if (col_num == 4)
    {
      const char *srctick = "?";
      if ( /* uninstall */ !pkg.desired ||
           /* when no source mirror available */
           !pkg.desired.sourcePackage().accessible())
        srctick = "n/a";
      else if (pkg.desired.sourcePackage().picked())
        srctick = "yes";
      else
        srctick = "no";

      return srctick;
    }
  else if (col_num == 5)
    {
      return pkg.getReadableCategoryList().c_str();
    }
  else if (col_num == 6)
    {
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
        picked = pkg.trustp (false, theView.deftrust);

      /* Include the size of the binary package, and if selected, the source
         package as well.  */
      sz += picked.source()->size;
      if (picked.sourcePackage().picked())
        sz += picked.sourcePackage().source()->size;

      /* If size still 0, size must be unknown.  */
      std::string size = (sz == 0) ? "?" : format_1000s((sz+1023)/1024) + "k";

      return size.c_str();
    }
  else if (col_num == 7)
    {
      return pkg.SDesc().c_str();
    }

  return "unknown";
}

const char *
PickPackageLine::tooltip(int col_num)
{
  if (col_num == 7)
    {
      return pkg.LDesc().c_str();
    }

  return "empty tooltip";
}

bool
PickPackageLine::click(int col_num)
{
  if (col_num == 2) // new (action)
    {
      pkg.set_action (theView.deftrust);
    }
  if (col_num == 3) // bintick
    {
      if (pkg.desired.accessible ())
        pkg.desired.pick (!pkg.desired.picked (), &pkg);
    }
  else if (col_num == 4) // srctick
    {
      if (pkg.desired.sourcePackage ().accessible ())
        pkg.desired.sourcePackage ().pick (!pkg.desired.sourcePackage ().picked (), NULL);
    }
  /* Unchecking binary while source is unchecked or vice versa is equivalent
     to uninstalling.  It's essential to set desired correctly, otherwise the
     package gets uninstalled without visual feedback to the user.  The package
     will not even show up in the "Pending" view! */
  if (!pkg.desired.picked () && !pkg.desired.sourcePackage ().picked ())
    pkg.desired = packageversion ();

  return true;

}
