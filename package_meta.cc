/*
 * Copyright (c) 2001, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include "concat.h"

#include "io_stream.h"
#include "compress.h"

#include "filemanip.h"
#include "hash.h"
#include "log.h"
/* io_stream needs a bit of tweaking to get rid of this. TODO */
#include "mount.h"
/* this goes at the same time */
#include "win32.h"


#include "category.h"

#include "package_version.h"
#include "cygpackage.h"
#include "package_meta.h"
#include "package_db.h"

static const char *standard_dirs[] = {
  "/bin",
  "/etc",
  "/lib",
  "/tmp",
  "/usr",
  "/usr/bin",
  "/usr/lib",
  "/usr/src",
  "/usr/local",
  "/usr/local/bin",
  "/usr/local/etc",
  "/usr/local/lib",
  "/usr/tmp",
  "/var/run",
  "/var/tmp",
  0
};

void
hash::add_subdirs (char const *tpath)
{
  char *nonp, *pp;
  char *path = strdup (tpath);
  for (nonp = path; *nonp == '\\' || *nonp == '/'; nonp++);
  for (pp = path + strlen (path) - 1; pp > nonp; pp--)
    if (*pp == '/' || *pp == '\\')
      {
	int i, s = 0;
	char c = *pp;
	*pp = 0;
	for (i = 0; standard_dirs[i]; i++)
	  if (strcmp (standard_dirs[i] + 1, path) == 0)
	    {
	      s = 1;
	      break;
	    }
	if (s == 0)
	  add (path);
	*pp = c;
      }
}

void
packagemeta::add_version (packageversion & thepkg)
{
  versions.registerbyobject (thepkg);
}

/* assumption: package thepkg is already in the metadata list. */
void
packagemeta::set_installed (packageversion & thepkg)
{
  packageversion *temp = versions.getbykey (thepkg.key);
  if (temp == &thepkg)
    installed = &thepkg;
}

/* uninstall a package if it's installed */
void
packagemeta::uninstall ()
{
  if (installed)
    {
      /* this will need to be pushed down to the version, or even the source level
       * to allow differences between formats to be seamlessly managed
       * but for now: here is ok
       */
      hash dirs;
      const char *line = installed->getfirstfile ();
      while (line)
	{
	  dirs.add_subdirs (line);

	  char *d = cygpath ("/", line, NULL);
	  DWORD dw = GetFileAttributes (d);
	  if (dw != 0xffffffff && !(dw & FILE_ATTRIBUTE_DIRECTORY))
	    {
	      log (LOG_BABBLE, "unlink %s", d);
	      DeleteFile (d);
	    }
	  line = installed->getnextfile ();
	}
      installed->uninstall ();

      dirs.reverse_sort ();
      char *subdir = 0;
      while ((subdir = dirs.enumerate (subdir)) != 0)
	{
	  char *d = cygpath ("/", subdir, NULL);
	  if (RemoveDirectory (d))
	    log (LOG_BABBLE, "rmdir %s", d);
	}
    }
  installed = 0;
}


void
packagemeta::add_category (Category & cat)
{
  /* add a new record for the package list */
  CategoryPackage & catpack = Categories.registerbykey (cat);
  catpack.pkg = this;
}

char const *
packagemeta::SDesc ()
{
  return versions[1]->SDesc ();
};

/* Return an appropriate caption given the current action. */
char const *
packagemeta::action_caption ()
{
  if (!desired && installed)
    return "Uninstall";
  else if (!desired)
    return "Skip";
  else if (desired == installed && desired->binpicked)
    {
      packagedb db;
      return db.task == PackageDB_Install ? "Reinstall" : "Retrieve";
    }
  else if (desired == installed && desired->srcpicked)
    /* FIXME: Redo source should come up if the tarball is already present locally */
    return "Source";
  else if (desired == installed)	/* and neither src nor bin */
    return "Keep";
  else
    return desired->Canonical_version ();
}

/* Set the next action given a current action.  */
void
packagemeta::set_action (packageversion * default_version)
{
  /* actions are the following:

     for install modes (from net/local)
     for each version:
     install this version
     install the source for this version
     and a boolean flag - force install to allow reinstallation, or bypassing requirements
     globally:
     install the source for the current version.

     to uninstall a package, the desired version is set to NULL;

     for mirroring modes (download only)
     for each version
     download this version
     download source for this version

     these are represented by the following:
     the desired pointer in the packagemetadata indicated which version we are operating on.
     if we are operating on the installed version, reinstall is a valid option.
     for the selected version, forceinstall means Do an install no matter what, and
     srcpicked means download the source.

     The default action for any installed package is to install the 'curr version'
     if it is not already installed.

     The default action for any non-installed package is to do nothing.

     To achieve a no-op, set desired==installed, and if (installed) set forceinstall=0 and
     srcpicked = 0;

     Iteration through versions should follow the following rules:
     selected radio button (prev/curr/test) (show as reinstall if that is the
     current version) ->source only (only if the package is installed) ->oldest version....s
     kip version of radio button...
     newest version->uninstall->no-op->selected radio button.

     If any state cannot be set (ie because (say) no prev entry exists for a package
     simply progress to the next option.

   */

  /* We were set to uninstall the package */
  if (!desired && installed)
    {
      /* No-op - keep whatever we've got */
      desired = installed;
      if (desired)
	{
	  desired->binpicked = 0;
	  desired->srcpicked = 0;
	}
      return;
    }
  else if (desired == installed
	   && (!installed
	       || !(installed->binpicked || installed->srcpicked)))
    /* Install the default trust version - this is a 'reinstall' for installed
       * packages */
    {
      desired = NULL;
      /* No-op */
      desired = default_version;
      if (desired)
	{
	  desired->binpicked = 1;
	  return;
	}
    }
  /* are we currently on the radio button selection and installed */
  if (desired == default_version && installed &&
      (!desired || desired->binpicked)
      && (desired &&
	  (desired->src.Cached () || desired->src.sites.number ())))
    {
      /* source only this file */
      desired = installed;
      desired->binpicked = 0;
      desired->srcpicked = 1;
      return;
    }
  /* are we currently on source only or on the radio button but not installed */
  else if ((desired == installed && installed
	    && installed->srcpicked) || desired == default_version)
    {
      /* move onto the loop through versions */
      desired = versions[1];
      if (desired == default_version)
	desired = versions.number () > 1 ? versions[2] : NULL;
      if (desired)
	{
	  desired->binpicked = 1;
	  desired->srcpicked = 0;
	}
      return;
    }
  else
    {
      /* preserve the src tick box */
      int source = desired->srcpicked;
      /* bump the version selected, skipping the radio button trust along the way */
      size_t n;
      for (n = 1;
	   n <= versions.number ()
	   && desired != versions[n]; n++);
      /* n points at pkg->desired */
      n++;
      if (n <= versions.number ())
	{
	  if (default_version == versions[n])
	    n++;
	  if (n <= versions.number ())
	    {
	      desired = versions[n];
	      desired->srcpicked = source;
	      return;
	    }
	}
      /* went past the end - uninstall the package */
      desired = NULL;
    }
}
