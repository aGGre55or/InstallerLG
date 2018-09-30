//----------------------------------------------------------------------------
// resource.c:
//
// Encapsulation of resources that we might want to localize later on.
//----------------------------------------------------------------------------
// Copyright (C) 2018, Ola Söder. All rights reserved.
// Licensed under the AROS PUBLIC LICENSE (APL) Version 1.1
//----------------------------------------------------------------------------

#include "error.h"
#include "resource.h"
#ifdef AMIGA
#include <proto/exec.h>
#include <proto/locale.h>
#define CATCOMP_BLOCK
#define CATCOMP_CODE
#include "strings.h"

static struct LocaleInfo li;
#endif

//----------------------------------------------------------------------------
// Name:        tr
// Description: Get string from string ID.
// Input:       A res_t string ID.
// Return:      The string corresponding to the string ID.
//----------------------------------------------------------------------------
const char *tr(res_t r)
{
    // Fail nicely if we're out of range.
    res_t i = r > S_GONE ? S_GONE : r;

#ifndef AMIGA
    // res_t -> string mappings.
    static const char *res[] =
    {
        // Out of range.
        "NONE",

        "Copying file(s)",
        "Overwrite existing directory '%s'?",
        "Abort copy '%s' to '%s'.\n",
        "Could not copy '%s' to '%s'.\n",
        "Delete write protected file '%s'?",
        "Delete write protected directory '%s'?",
        "Overwrite write protected file '%s'?",
        "Delete file '%s'?",
        "Deleted '%s'.\n",
        "No such file '%s'.\n",
        "Renamed '%s' to '%s'.\n",
        "Copied '%s' to '%s'.\n",
        "Directory '%s' exists already.\n",
        "Created '%s'.\n",
        "Get mask '%s' %d\n",
        "Protect '%s' %d\n",
        "Installer",
        "Set Installation Mode",
        "Destination drawer",
        "Copying file",
        "Files to install",
        "Installation mode and logging",
        "Progress",
        "Proceed",
        "Retry",
        "Abort",
        "Yes",
        "No",
        "Ok",
        "Next",
        "Skip",
        "Unknown error",
        "Setup failed",
        "Failed creating MUI custom class",
        "Failed creating MUI application",
        "Initialization failed",
        "Error",
        "Success",
        "Line %d: %s '%s'",
        "Novice User - All Actions Automatic",
        "Intermediate User - Limited Manual Control",
        "Expert User - Must Confirm all actions",
        "Install for real",
        "Dry run only",
        "No logging",
        "Log to file",
        "installer: required argument missing\n",
        "%s\n\n%s can be found in %s",
        "Installation complete",
        "Executed '%s'.\n",
        "Error: Could not open file '%s'\n",
        "Line %d: %s '%s'\n",
        "Working on installation...\n\n",
        "Select directory",
        "Select file",
        "Assign '%s' to '%s'\n",
        "Remove assign '%s'\n",
        "Could not assign '%s' to '%s'\n",
        "Could not remove assign '%s'\n",
        "Append '%s' to '%s'\n",
        "Include '%s' in '%s'\n",
        "Version to install",
        "Version currently installed",
        "Unknown version",
        "Destination drawer",
        "There is no currently installed version",

        // Out of range.
        "GONE"
    };

    // res[i] is a valid string.
    return res[i];
#else
    if(!li.li_LocaleBase)
    {
        li.li_LocaleBase = OpenLibrary("locale.library", 37);
        li.li_Catalog = OpenCatalog(NULL, "Installer.catalog", TAG_DONE);
    }
    return GetString(&li, i - 1);
#endif
}
