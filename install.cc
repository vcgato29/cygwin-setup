/*
 * Copyright (c) 2000, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

/* The purpose of this file is to intall all the packages selected in
   the install list (in ini.h).  Note that we use a separate thread to
   maintain the progress dialog, so we avoid the complexity of
   handling two tasks in one thread.  We also create or update all the
   files in /etc/setup/* and create the mount points. */

static char *cvsid = "\n%%% $Id$\n";

#include "win32.h"
#include "commctrl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "zlib/zlib.h"

#include "resource.h"
#include "ini.h"
#include "dialog.h"
#include "concat.h"
#include "geturl.h"
#include "mkdir.h"
#include "state.h"
#include "tar.h"
#include "diskfull.h"
#include "msg.h"
#include "mount.h"
#include "log.h"
#include "hash.h"

#include "port.h"

static HWND ins_dialog = 0;
static HWND ins_action = 0;
static HWND ins_pkgname = 0;
static HWND ins_filename = 0;
static HWND ins_pprogress = 0;
static HWND ins_iprogress = 0;
static HWND ins_diskfull = 0;
static HANDLE init_event;

static int total_bytes = 0;
static int total_bytes_sofar = 0;
static int package_bytes = 0;

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {
    case IDCANCEL:
      exit_setup (1);
    }
}

static BOOL CALLBACK
dialog_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  int i, j;
  HWND listbox;
  switch (message)
    {
    case WM_INITDIALOG:
      ins_dialog = h;
      ins_action = GetDlgItem (h, IDC_INS_ACTION);
      ins_pkgname = GetDlgItem (h, IDC_INS_PKG);
      ins_filename = GetDlgItem (h, IDC_INS_FILE);
      ins_pprogress = GetDlgItem (h, IDC_INS_PPROGRESS);
      ins_iprogress = GetDlgItem (h, IDC_INS_IPROGRESS);
      ins_diskfull = GetDlgItem (h, IDC_INS_DISKFULL);
      SetEvent (init_event);
      return FALSE;
    case WM_COMMAND:
      return HANDLE_WM_COMMAND (h, wParam, lParam, dialog_cmd);
    }
  return FALSE;
}

static WINAPI DWORD
dialog (void *)
{
  int rv = 0;
  MSG m;
  HANDLE ins_dialog = CreateDialog (hinstance, MAKEINTRESOURCE (IDD_INSTATUS),
				   0, dialog_proc);
  if (ins_dialog == 0)
    fatal ("create dialog");
  ShowWindow (ins_dialog, SW_SHOWNORMAL);
  UpdateWindow (ins_dialog);
  while (GetMessage (&m, 0, 0, 0) > 0) {
    TranslateMessage (&m);
    DispatchMessage (&m);
  }
}

static DWORD start_tics;

static void
init_dialog ()
{
  if (ins_dialog == 0)
    {
      DWORD tid;
      HANDLE thread;
      init_event = CreateEvent (0, 0, 0, 0);
      thread = CreateThread (0, 0, dialog, 0, 0, &tid);
      WaitForSingleObject (init_event, 10000);
      CloseHandle (init_event);
      SendMessage (ins_pprogress, PBM_SETRANGE, 0, MAKELPARAM (0, 100));
      SendMessage (ins_iprogress, PBM_SETRANGE, 0, MAKELPARAM (0, 100));
      SendMessage (ins_diskfull, PBM_SETRANGE, 0, MAKELPARAM (0, 100));
    }

  SetWindowText (ins_pkgname, "");
  SetWindowText (ins_filename, "");
  SendMessage (ins_pprogress, PBM_SETPOS, (WPARAM) 0, 0);
  SendMessage (ins_iprogress, PBM_SETPOS, (WPARAM) 0, 0);
  SendMessage (ins_diskfull, PBM_SETPOS, (WPARAM) 0, 0);
  ShowWindow (ins_dialog, SW_SHOWNORMAL);
  SetForegroundWindow (ins_dialog);
}

static void
progress (int bytes)
{
  int perc;

  if (package_bytes > 100)
    {
      perc = bytes / (package_bytes / 100);
      SendMessage (ins_pprogress, PBM_SETPOS, (WPARAM) perc, 0);
    }

  if (total_bytes > 100)
    {
      perc = (total_bytes_sofar + bytes) / (total_bytes / 100);
      SendMessage (ins_iprogress, PBM_SETPOS, (WPARAM) perc, 0);
    }
}

static void
badrename (char *o, char *n)
{
  char buf[1000];
  char *err = strerror (errno);
  if (!err)
    err = "(unknown error)";
  note (IDS_ERR_RENAME, o, n, err);
}

static char *standard_dirs[] = {
  "/bin",
  "/etc",
  "/lib",
  "/tmp",
  "/usr",
  "/usr/bin",
  "/usr/lib",
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
hash::add_subdirs (char *path)
{
  char *nonp, *pp;
  for (nonp = path; *nonp == '\\' || *nonp == '/'; nonp++);
  for (pp = path + strlen(path) - 1; pp>nonp; pp--)
    if (*pp == '/' || *pp == '\\')
      {
	int i, s=0;
	char c = *pp;
	*pp = 0;
	for (i=0; standard_dirs[i]; i++)
	  if (strcmp (standard_dirs[i]+1, path) == 0)
	    {
	      s = 1;
	      break;
	    }
	if (s == 0)
	  add (path);
	*pp = c;
      }
}

char *
map_filename (char *fn)
{
  char *dest_file;
  while (*fn == '/' || *fn == '\\')
    fn++;
  if (strncmp (fn, "usr/bin/", 8) == 0)
    dest_file = concat (root_dir, "/bin/", fn+8, 0);
  else if (strncmp (fn, "usr/lib/", 8) == 0)
    dest_file = concat (root_dir, "/lib/", fn+8, 0);
  else
    dest_file = concat (root_dir, "/", fn, 0);
  return dest_file;
}

#define pi (package[i].info[package[i].trust])

#define LOOP_PACKAGES \
  for (i=0; i<npackages; i++) \
    if ((package[i].action == ACTION_NEW \
	 || package[i].action == ACTION_UPGRADE) \
	&& pi.install)

void
do_install (HINSTANCE h)
{
  int i, num_installs = 0, num_uninstalls = 0;
  next_dialog = IDD_DESKTOP;

  mkdir_p (1, root_dir);

  for (i=0; standard_dirs[i]; i++)
    {
      char *p = concat (root_dir, standard_dirs[i], 0);
      mkdir_p (1, p);
      free (p);
    }

  /* Create /var/run/utmp */
  char *utmp = concat (root_dir, "/var/run/utmp", 0);
  FILE *ufp = fopen (utmp, "wb");
  if (ufp)
    fclose (ufp);
  free (utmp);

  dismiss_url_status_dialog ();

  init_dialog ();

  total_bytes = 0;
  total_bytes_sofar = 0;

  int df = diskfull (root_dir);
  SendMessage (ins_diskfull, PBM_SETPOS, (WPARAM) df, 0);

  LOOP_PACKAGES
    {
      total_bytes += pi.install_size;
    }

  for (i=0; i<npackages; i++)
    {
      if (package[i].action == ACTION_UNINSTALL
	  || (package[i].action == ACTION_UPGRADE && pi.install))
	{
	  hash dirs;
	  char line[_MAX_PATH];

	  gzFile lst = gzopen (concat (root_dir, "/etc/setup/",
				       package[i].name, ".lst.gz", 0),
			       "rb");
	  if (lst)
	    {
	      SetWindowText (ins_pkgname, package[i].name);
	      SetWindowText (ins_action, "Uninstalling...");
	      log (0, "Uninstalling %s", package[i].name);

	      while (gzgets (lst, line, sizeof (line)))
		{
		  if (line[strlen(line)-1] == '\n')
		    line[strlen(line)-1] = 0;

		  dirs.add_subdirs (line);

		  char *d = map_filename (line);
		  DWORD dw = GetFileAttributes (d);
		  if (dw != 0xffffffff && !(dw & FILE_ATTRIBUTE_DIRECTORY))
		    {
		      log (LOG_BABBLE, "unlink %s", d);
		      DeleteFile (d);
		    }
		}
	      gzclose (lst);

	      dirs.reverse_sort ();
	      char *subdir = 0;
	      while ((subdir = dirs.enumerate (subdir)) != 0)
		{
		  char *d = map_filename (subdir);
		  if (RemoveDirectory (d))
		    log (LOG_BABBLE, "rmdir %s", d);
		}
	      num_uninstalls ++;
	    }
	}

      if ((package[i].action == ACTION_NEW
	   || package[i].action == ACTION_UPGRADE)
	  && pi.install)
	{
	  char *local = pi.install, *cp, *fn, *base;

	  base = local;
	  for (cp=pi.install; *cp; cp++)
	    if (*cp == '/' || *cp == '\\' || *cp == ':')
	      base = cp+1;
	  SetWindowText (ins_pkgname, base);

	  gzFile lst = gzopen (concat (root_dir, "/etc/setup/",
				       package[i].name, ".lst.gz", 0),
			       "wb9");

	  package_bytes = pi.install_size;

	  switch (package[i].action)
	    {
	    case ACTION_NEW:
	      SetWindowText (ins_action, "Installing...");
	      break;
	    case ACTION_UPGRADE:
	      SetWindowText (ins_action, "Upgrading...");
	      break;
	    }

	  log (0, "Installing %s", local);
	  tar_open (local);
	  while (fn = tar_next_file ())
	    {
	      char *dest_file;

	      if (lst)
		gzprintf (lst, "%s\n", fn);

	      dest_file = map_filename (fn);

	      SetWindowText (ins_filename, dest_file);
	      log (LOG_BABBLE, "Installing file %s", dest_file);
	      tar_read_file (dest_file);

	      progress (tar_ftell ());
	      num_installs ++;
	    }
	  tar_close ();

	  total_bytes_sofar += pi.install_size;
	  progress (0);

	  df = diskfull (root_dir);
	  SendMessage (ins_diskfull, PBM_SETPOS, (WPARAM) df, 0);

	  if (lst)
	    gzclose (lst);
	}
    } // end of big package loop

  ShowWindow (ins_dialog, SW_HIDE);

  char *odbn = concat (root_dir, "/etc/setup/installed.db", 0);
  char *ndbn = concat (root_dir, "/etc/setup/installed.db.new", 0);
  char *sdbn = concat (root_dir, "/etc/setup/installed.db.old", 0);

  mkdir_p (0, ndbn);

  FILE *odb = fopen (odbn, "rt");
  FILE *ndb = fopen (ndbn, "wb");

  if (!ndb)
    {
      char *err = strerror (errno);
      if (!err)
	err = "(unknown error)";
      fatal (IDS_ERR_OPEN_WRITE, ndb, err);
    }

  if (odb)
    {
      char line[1000], pkg[1000];
      int printit;
      while (fgets (line, 1000, odb))
	{
	  printit = 1;
	  sscanf (line, "%s", pkg);
	  for (i=0; i<npackages; i++)
	    {
	      if (strcmp (pkg, package[i].name) == 0)
		switch (package[i].action)
		  {
		  case ACTION_NEW:
		  case ACTION_UPGRADE:
		  case ACTION_UNINSTALL:
		    printit = 0;
		    break;
		  }
	    }
	  if (printit)
	    fputs (line, ndb);
	}
      
    }

  LOOP_PACKAGES
    {
      fprintf (ndb, "%s %s %d\n", package[i].name,
	       pi.install, pi.install_size);
    }

  if (odb)
    fclose (odb);
  fclose (ndb);

  remove (sdbn);
  if (odb && rename (odbn, sdbn))
    badrename (odbn, sdbn);

  remove (odbn);
  if (rename (ndbn, odbn))
    badrename (ndbn, odbn);

  if (num_installs == 0 && num_uninstalls == 0)
    {
      exit_msg = IDS_NOTHING_INSTALLED;
      return;
    }
  if (num_installs == 0)
    {
      exit_msg = IDS_UNINSTALL_COMPLETE;
      return;
    }

  remove_mount ("/");
  remove_mount ("/usr");
  remove_mount ("/usr/bin");
  remove_mount ("/usr/lib");
  remove_mount ("/var");
  remove_mount ("/lib");
  remove_mount ("/bin");
  remove_mount ("/etc");

  int istext = (root_text == IDC_ROOT_TEXT) ? 1 : 0;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;

  create_mount ("/", root_dir, istext, issystem);
  create_mount ("/usr/bin", concat (root_dir, "/bin", 0), istext, issystem);
  create_mount ("/usr/lib", concat (root_dir, "/lib", 0), istext, issystem);

  exit_msg = IDS_INSTALL_COMPLETE;
}
