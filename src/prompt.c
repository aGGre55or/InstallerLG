//----------------------------------------------------------------------------
// prompt.c:
//
// User prompting
//----------------------------------------------------------------------------
// Copyright (C) 2018, Ola Söder. All rights reserved.
// Licensed under the AROS PUBLIC LICENSE (APL) Version 1.1
//----------------------------------------------------------------------------

#include "alloc.h"
#include "gui.h"
#include "error.h"
#include "eval.h"
#include "prompt.h"
#include "resource.h"
#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef AMIGA
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#endif

//----------------------------------------------------------------------------
// (askbool (prompt..) (help..) (default..) (choices..))
//     0=no, 1=yes
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_askbool(entry_p contxt)
{
    if(contxt)
    {
        const char *yes = tr(S_AYES), *nay = tr(S_NONO);
        entry_p prompt   = get_opt(contxt, OPT_PROMPT),
                help     = get_opt(contxt, OPT_HELP),
                back     = get_opt(contxt, OPT_BACK),
                deflt    = get_opt(contxt, OPT_DEFAULT),
                choices  = get_opt(contxt, OPT_CHOICES);

        // Default = 'no'.
        DNUM = 0;

        // Do we have both prompt and help text?
        if(!prompt || !help)
        {
            // Missing one or more.
            ERR(ERR_MISSING_OPTION, prompt ? "help" : "prompt");
            RCUR;
        }

        // Do we have a choice option?
        if(choices)
        {
            // Unless the parser is broken,
            // we will have >= one child.
            entry_p *entry = choices->children;

            // Pick up whatever we can, use the default
            // value if we only have a single choice.
            yes = *entry && *entry != end() ? str(*entry) : yes;
            nay = *(++entry) && *entry != end() ? str(*entry) : nay;
        }

        // Do we have a user specified default?
        if(deflt)
        {
            DNUM = num(deflt);
        }

        // Show requester unless we're executing in
        // 'novice' mode.
        if(get_numvar(contxt, "@user-level") > 0)
        {
            const char *prt = str(prompt),
                       *hlp = str(help);

            // Only show requester if we could
            // resolve all options.
            if(!DID_ERR)
            {
                // Prompt user.
                inp_t grc = gui_bool(prt, hlp, yes, nay, back != false);

                // Is the back option available?
                if(back)
                {
                    // Fake input?
                    if(get_numvar(contxt, "@back"))
                    {
                        grc = G_ABORT;
                    }

                    // On abort execute.
                    if(grc == G_ABORT)
                    {
                        return resolve(back);
                    }
                }

                // FIXME
                if(grc == G_ABORT || grc == G_EXIT)
                {
                    HALT;
                }

                // Translate return code.
                DNUM = (grc == G_TRUE) ? 1 : 0;
            }
        }
    }
    else
    {
        // The parser is broken
        PANIC(contxt);
    }

    // Success, failure or
    // broken parser.
    RCUR;
}

//----------------------------------------------------------------------------
// (askchoice (prompt..) (choices..) (default..))
//     choose 1 option
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//
// The installer in OS 3.9 doesn't seem to return a bitmap, which
// is how it is supposed to work according to the Installer.guide,
// instead it returns a zero index. We choose to ignore the guide
// and mimic the behaviour of the implementation in 3.9.
//----------------------------------------------------------------------------
entry_p m_askchoice(entry_p contxt)
{
    if(contxt)
    {
        entry_p prompt   = get_opt(contxt, OPT_PROMPT),
                help     = get_opt(contxt, OPT_HELP),
                back     = get_opt(contxt, OPT_BACK),
                choices  = get_opt(contxt, OPT_CHOICES),
                deflt    = get_opt(contxt, OPT_DEFAULT);

        DNUM = 0;

        // We need something to choose from,
        // a help text and a prompt text.
        if(!prompt || !help || !choices)
        {
            // Missing one or more.
            ERR(ERR_MISSING_OPTION, prompt ? help ?
                "choices" : "help" : "prompt");
            RCUR;
        }

        // Unless the parser is broken,
        // we will have >= one child.
        entry_p *entry = choices->children;

        // The choice is represented by a bitmask of
        // 32 bits, refer to Install.guide. Thus, we
        // need room for 32 pointers + NULL.
        static const char *chs[33];
        static int add[32];

        // Indices
        int ndx = 0, off = 0;

        // Pick up a string representation of all
        // the options.
        while(*entry && *entry != end() && off < 32)
        {
            // Resolve once.
            char *opt = str(*entry);

            // Save skip deltas. See (1).
            add[ndx] = off - ndx;

            // From the Installer.guide:
            //
            // 1. If you use an empty string as choice descriptor, the choice will
            //    be invisible to the user, i.e. it will not be displayed on screen.
            //    By using variables you can easily set up a programable number of
            //    choices then while retaining the bit numbering.
            if(*opt)
            {
                // 2. Previous versions of Installer did not support proportional fonts
                //    well and some people depended on the non proportional layout of
                //    the display for table like choices.  So Installer will continue to
                //    render choices non proportional unless you start one of the
                //    choices with a special escape sequence `"<ESC>[2p"'. This escape
                //    sequence allows proportional rendering. It is wise to specify this
                //    only in the first choice of the list. Note this well.  (V42)
                if(strlen(opt) > 3 && !memcmp("\x1B[2p", opt, 4))
                {
                    // We rely on Zune / MUI for #2. Hide
                    // this control sequence if it exists.
                    opt += 4;
                }

                // Make sure that the removal of the control
                // sequence hasn't cleared the string.
                if(*opt)
                {
                    // Something to show.
                    chs[ndx++] = opt;
                }
            }

            // Invisible items are valid as default
            // values, so we need to count these as
            // well.
            off++;

            // Next option.
            entry++;
        }

        // Exit if there's nothing to show.
        if(!ndx)
        {
            // Use the default value if such
            // exists.
            RNUM(deflt ? num(deflt) : 0);
        }

        // Terminate array.
        chs[ndx] = NULL;

        // Do we have default option?
        if(deflt)
        {
            // Is there such a choice?
            int def = num(deflt);

            // Check for negative values
            // as well.
            if(def < 0 || def >= off)
            {
                // Nope, out of range.
                ERR(ERR_NO_ITEM, str(deflt));
                RNUM(0);
            }

            // Yes, use the default
            // value given.
            ndx = def;
        }
        else
        {
            // No default = 0
            ndx = 0;
        }

        // Show requester unless we're executing in
        // 'novice' mode.
        if(get_numvar(contxt, "@user-level") > 0)
        {
            const char *prt = str(prompt),
                       *hlp = str(help);

            // Only show requester if we could
            // resolve all options.
            if(!DID_ERR)
            {
                // Skipper.
                int del = 0;

                // Cap / compute skipper.
                if(ndx > 0 && ndx < 31 &&
                   ndx - add[ndx - 1] > 0 &&
                   ndx + add[ndx - 1] < 31)
                {
                    del = add[ndx - 1];
                }

                // Prompt user. Subtract skipper from default.
                inp_t grc = gui_choice(prt, hlp, chs, ndx - del, back != false, &DNUM);

                // Add skipper. Don't trust the GUI.
                DNUM += ((DNUM < 32 && DNUM >= 0) ?
                        add[DNUM] : 0);

                // Is the back option available?
                if(back)
                {
                    // Fake input?
                    if(get_numvar(contxt, "@back"))
                    {
                        grc = G_ABORT;
                    }

                    // On abort execute.
                    if(grc == G_ABORT)
                    {
                        return resolve(back);
                    }
                }

                // FIXME
                if(grc == G_ABORT || grc == G_EXIT)
                {
                    HALT;
                }
            }
        }
        else
        {
            DNUM = ndx;
        }
    }
    else
    {
        // The parser is broken
        PANIC(contxt);
    }

    // Success, failure or
    // broken parser.
    RCUR;
}

//----------------------------------------------------------------------------
// (askdir (prompt..) (help..) (default..) (newpath) (disk))
//      ask for directory name
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//
// We don't support (assigns) and (newpath), or rather, we act
// as if they are always present.
//----------------------------------------------------------------------------
entry_p m_askdir(entry_p contxt)
{
    if(contxt)
    {
        entry_p prompt   = get_opt(contxt, OPT_PROMPT),
                help     = get_opt(contxt, OPT_HELP),
                back     = get_opt(contxt, OPT_BACK),
                deflt    = get_opt(contxt, OPT_DEFAULT),
                newpath  = get_opt(contxt, OPT_NEWPATH),
                disk     = get_opt(contxt, OPT_DISK),
                assigns  = get_opt(contxt, OPT_ASSIGNS);

        // Are all mandatory options (!?) present?
        if(prompt && help && deflt)
        {
            const char *ret;

            // Show requeter unless we're executing in
            // 'novice' mode.
            if(get_numvar(contxt, "@user-level") > 0)
            {
                const char *prt = str(prompt),
                           *hlp = str(help),
                           *def = str(deflt);

                // Could we resolve all options?
                if(DID_ERR)
                {
                    // Return empty string.
                    REST;
                }

                // Prompt user.
                inp_t grc = gui_askdir(prt, hlp, newpath != false, disk != false,
                                       assigns != false, def, back != false, &ret);

                // Is the back option available?
                if(back)
                {
                    // Fake input?
                    if(get_numvar(contxt, "@back"))
                    {
                        grc = G_ABORT;
                    }

                    // On abort execute.
                    if(grc == G_ABORT)
                    {
                        return resolve(back);
                    }
                }

                // FIXME
                if(grc == G_ABORT || grc == G_EXIT)
                {
                    HALT;
                    REST;
                }
            }
            else
            {
                // We're executing in 'novice' mode,
                // use the default value.
                ret = str(deflt);
            }

            // We have a file.
            RSTR(DBG_ALLOC(strdup(ret)));
        }

        // What option are we missing?
        ERR(ERR_MISSING_OPTION, prompt ? help ?
            "default" : "help" : "prompt");

        // Return empty string on failure.
        REST;
    }

    // Broken parser.
    PANIC(contxt);
    RCUR;
}

//----------------------------------------------------------------------------
// (askdisk (prompt..) (help..) (dest..) (newname..) (assigns))
//     ask user to insert disk
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//
// Limitations: (disk) and (assigns) aren't supported. Assigns
// always satisfy the request and (disk) is simply ignored.
//
//----------------------------------------------------------------------------
entry_p m_askdisk(entry_p contxt)
{
    if(contxt)
    {
        entry_p prompt   = get_opt(contxt, OPT_PROMPT),
                help     = get_opt(contxt, OPT_HELP),
                back     = get_opt(contxt, OPT_BACK),
                dest     = get_opt(contxt, OPT_DEST),
                newname  = get_opt(contxt, OPT_NEWNAME);

        DNUM = 0;

        // Are all mandatory options (!?) present?
        if(prompt && help && dest)
        {
            char dsk[PATH_MAX];

            // Append ':' to turn 'dest' into something
            // we can 'Lock'.
            snprintf(dsk, sizeof(dsk), "%s:", str(dest));

            // Volume names must be > 0 characters long.
            if(strlen(dsk) > 1)
            {
                #if defined(AMIGA) && !defined(LG_TEST)
                struct Process *p = (struct Process *)
                    FindTask(NULL);

                // Save the current window ptr.
                APTR w = p->pr_WindowPtr;

                // Disable auto request.
                p->pr_WindowPtr = (APTR) -1L;

                // Is this volume present already?
                BPTR l = (BPTR) Lock(dsk, ACCESS_READ);
                if(!l)
                {
                    const char *msg = str(prompt),
                               *hlp = str(help),
                               *bt1 = tr(S_RTRY),
                               *bt2 = tr(S_SKIP);

                    // Only show requester if we could
                    // resolve all options.
                    if(!DID_ERR)
                    {
                        // Retry until we can get a lock or the
                        // user aborts.
                        while(!l)
                        {
                            // Prompt user.
                            inp_t grc = gui_bool(msg, hlp, bt1, bt2, back != false);

                            if(grc == G_TRUE)
                            {
                                l = (BPTR) Lock(dsk, ACCESS_READ);
                            }
                            else
                            {
                                // Is the back option available?
                                if(back)
                                {
                                    // Fake input?
                                    if(get_numvar(contxt, "@back"))
                                    {
                                        grc = G_ABORT;
                                    }

                                    // On abort execute.
                                    if(grc == G_ABORT)
                                    {
                                        // Restore auto request before
                                        // executing the 'back' code.
                                        p->pr_WindowPtr = w;
                                        return resolve(back);
                                    }
                                }
                                // FIXME
                                if(grc == G_ABORT || grc == G_EXIT)
                                {
                                    HALT;
                                }

                                // User abort or err.
                                break;
                            }
                        }
                    }
                }

                // Did the user abort?
                if(l)
                {
                    // Are we going to create an assign
                    // aliasing 'dest'?
                    if(newname)
                    {
                        const char *nn = str(newname);

                        // Assigns must be > 0 characters long.
                        if(*nn)
                        {
                            // On success, the lock belongs to
                            // the system. Do not UnLock().
                            DNUM = AssignLock(nn, l) ? 1 : 0;

                            // On failure, we need to UnLock()
                            // it ourselves.
                            if(!DNUM)
                            {
                                // Could not create 'newname' assign.
                                ERR(ERR_ASSIGN, str(CARG(1)));
                                UnLock(l);
                            }
                        }
                        else
                        {
                            // An assign must contain at
                            // least one character.
                            ERR(ERR_INVALID_ASSIGN, nn);
                            UnLock(l);
                        }
                    }
                    else
                    {
                        // Sucess.
                        DNUM = 1;
                        UnLock(l);
                    }
                }

                // Restore auto request.
                p->pr_WindowPtr = w;
                #else
                // On non-Amiga systems, or in test mode,
                // we always succeed.
                DNUM = 1;

                // For testing purposes only.
                printf("%d", (newname || back) ? 1 : 0);
                #endif
            }
            else
            {
                // A volume name must contain at
                // least one character.
                ERR(ERR_INVALID_VOLUME, dsk);
            }
        }
        else
        {
            // Missing one or more options.
            ERR(ERR_MISSING_OPTION, prompt ? help ?
                "dest" : "help" : "prompt");
        }
    }
    else
    {
        // The parser is broken
        PANIC(contxt);
    }

    // Success, failure or
    // broken parser.
    RCUR;
}

//----------------------------------------------------------------------------
// (askfile (prompt..) (help..) (default..) (newpath) (disk))
//     ask for file name
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//
// The installer in OS 3.9 doesn't seem to recognise (disk) and
// (newpath). We support (disk) but not (newpath), or rather we
// act as if (newpath) is always present.
//----------------------------------------------------------------------------
entry_p m_askfile(entry_p contxt)
{
    if(contxt)
    {
        entry_p prompt   = get_opt(contxt, OPT_PROMPT),
                help     = get_opt(contxt, OPT_HELP),
                back     = get_opt(contxt, OPT_BACK),
                newpath  = get_opt(contxt, OPT_NEWPATH),
                disk     = get_opt(contxt, OPT_DISK),
                deflt    = get_opt(contxt, OPT_DEFAULT);

        // Are all mandatory options (!?) present?
        if(prompt && help && deflt)
        {
            const char *ret;

            // Show file dialog unless we're executing
            // in 'novice' mode.
            if(get_numvar(contxt, "@user-level") > 0)
            {
                const char *prt = str(prompt),
                           *hlp = str(help),
                           *def = str(deflt);

                // Could we resolve all options?
                if(DID_ERR)
                {
                    // Return empty string.
                    REST;
                }

                // Prompt user.
                inp_t grc = gui_askfile(prt, hlp, newpath != false, disk != false,
                                        def, back != false, &ret);

                // Is the back option available?
                if(back)
                {
                    // Fake input?
                    if(get_numvar(contxt, "@back"))
                    {
                        grc = G_ABORT;
                    }

                    // On abort execute.
                    if(grc == G_ABORT)
                    {
                        return resolve(back);
                    }
                }

                // FIXME
                if(grc == G_ABORT || grc == G_EXIT)
                {
                    HALT;
                    REST;
                }
            }
            else
            {
                // We're executing in 'novice' mode,
                // use the default value.
                ret = str(deflt);
            }

            // We have a file.
            RSTR(DBG_ALLOC(strdup(ret)));
        }

        // Missing one or more options.
        ERR(ERR_MISSING_OPTION, prompt ? help ?
            "default" : "help" : "prompt");

        // Return empty string
        // on failure.
        REST;
    }

    // Broken parser.
    PANIC(contxt);
    RCUR;
}

//----------------------------------------------------------------------------
// (asknumber (prompt..) (help..) (range..) (default..))
//     ask for a number
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//
// NOTE: We do not follow the Installer.guide when it comes to the default
// range. Instead of all positive values, we use 0 - 100 in order to be
// able to use a slider instead of a string gadget. This might be a problem.
// Scrap it?
//----------------------------------------------------------------------------
entry_p m_asknumber(entry_p contxt)
{
    if(contxt)
    {
        entry_p prompt   = get_opt(contxt, OPT_PROMPT),
                help     = get_opt(contxt, OPT_HELP),
                back     = get_opt(contxt, OPT_BACK),
                range    = get_opt(contxt, OPT_RANGE),
                deflt    = get_opt(contxt, OPT_DEFAULT);

        DNUM = 0;

        if(prompt && help && deflt)
        {
            // Default range.
            int min = 0, max = 100;

            if(range)
            {
                if(c_sane(range, 2))
                {
                    min = num(range->children[0]);
                    max = num(range->children[1]);

                    // Use default range when the
                    // user given range is invalid.
                    if(min >= max)
                    {
                        max = 100;
                        min = 0;
                    }
                }
                else
                {
                    // The parser is broken
                    PANIC(contxt);
                    RCUR;
                }
            }

            // Show requester unless we're executing in
            // 'novice' mode.
            if(get_numvar(contxt, "@user-level") > 0)
            {
                int def = num(deflt);
                const char *prt = str(prompt),
                           *hlp = str(help);

                // Only show requester if we could
                // resolve all options.
                if(!DID_ERR)
                {
                    // Prompt user.
                    inp_t grc = gui_number(prt, hlp, min, max, def, back != false, &DNUM);

                    // Is the back option available?
                    if(back)
                    {
                        // Fake input?
                        if(get_numvar(contxt, "@back"))
                        {
                            grc = G_ABORT;
                        }

                        // On abort execute.
                        if(grc == G_ABORT)
                        {
                            return resolve(back);
                        }
                    }

                    // FIXME
                    if(grc == G_ABORT || grc == G_EXIT)
                    {
                        HALT;
                    }
                }
            }
            else
            {
                // Use the default value.
                DNUM = num(deflt);
            }
        }
        else
        {
            // Missing one or more options.
            ERR(ERR_MISSING_OPTION, prompt ? help ?
                "default" : "help" : "prompt");
        }
    }
    else
    {
        // The parser is broken
        PANIC(contxt);
    }

    // Success, failure or
    // broken parser.
    RCUR;
}

//----------------------------------------------------------------------------
// (askoptions (prompt (help..) (choices..) default..))
//     choose n options
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_askoptions(entry_p contxt)
{
    if(contxt)
    {
        entry_p prompt   = get_opt(contxt, OPT_PROMPT),
                help     = get_opt(contxt, OPT_HELP),
                back     = get_opt(contxt, OPT_BACK),
                choices  = get_opt(contxt, OPT_CHOICES),
                deflt    = get_opt(contxt, OPT_DEFAULT);

        DNUM = -1;

        // We need everything but a default value.
        if(prompt && help && choices)
        {
            // Unless the parser is broken,
            // we will have >= one child.
            entry_p *chl = choices->children;

            // Options are represented by bitmask of 32
            // bits, refer to Install.guide. Thus, we
            // need room for 32 pointers + NULL.
            static const char *chs[33];

            // Choice index.
            int ndx = 0;

            // Pick up a string representation of all
            // the options.
            while(*chl && *chl != end() && ndx < 32)
            {
                char *cur = str(*chl);

                // From the Installer.guide:
                //
                // Previous versions of Installer did not support proportional fonts
                // well and some people depended on the non proportional layout of
                // the display for table like choices.  So Installer will continue to
                // render choices non proportional unless you start one of the
                // choices with a special escape sequence `"<ESC>[2p"'. This escape
                // sequence allows proportional rendering. It is wise to specify this
                // only in the first choice of the list. Note this well.  (V42)
                if(strlen(cur) > 3 && !memcmp("\x1B[2p", cur, 4))
                {
                    // We rely on Zune / MUI for #2. Hide
                    // this control sequence if it exists.
                    cur += 4;
                }

                // Save choice.
                chs[ndx++] = cur;

                // Next option.
                chl++;
            }

            // Exit if there's nothing to show.
            if(!ndx)
            {
                // Use the default value if such
                // exists.
                RNUM(deflt ? num(deflt) : -1);
            }

            // Terminate array.
            chs[ndx] = NULL;

            // Do we have default option?
            if(deflt)
            {
                // Is there such a choice?
                int def = num(deflt);

                if(def >= (1L << ndx))
                {
                    // Nope, out of range.
                    ERR(ERR_NO_ITEM, str(deflt));
                    RNUM(0);
                }

                // Yes, use the default
                // value given.
                ndx = def;
            }
            else
            {
                // No default = -1
                ndx = -1;
            }

            // Show requester unless we're executing in
            // 'novice' mode.
            if(get_numvar(contxt, "@user-level") > 0)
            {
                const char *prt = str(prompt),
                           *hlp = str(help);

                // Only show requester if we could
                // resolve all options.
                if(!DID_ERR)
                {
                    // Prompt user.
                    inp_t grc = gui_options(prt, hlp, chs, ndx, back != false, &DNUM);

                    // Is the back option available?
                    if(back)
                    {
                        // Fake input?
                        if(get_numvar(contxt, "@back"))
                        {
                            grc = G_ABORT;
                        }

                        // On abort execute.
                        if(grc == G_ABORT)
                        {
                            return resolve(back);
                        }
                    }

                    // FIXME
                    if(grc == G_ABORT || grc == G_EXIT)
                    {
                        HALT;
                    }
                }
            }
            else
            {
                DNUM = ndx;
            }
        }
        else
        {
            // Missing one or more options.
            ERR(ERR_MISSING_OPTION, prompt ? help ?
                "choices" : "help" : "prompt");
        }
    }
    else
    {
        // The parser is broken
        PANIC(contxt);
    }

    // Success, failure or
    // broken parser.
    RCUR;
}

//----------------------------------------------------------------------------
// (askstring (prompt..) (help..) (default..))
//     ask for a string
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_askstring(entry_p contxt)
{
    if(contxt)
    {
        entry_p prompt   = get_opt(contxt, OPT_PROMPT),
                help     = get_opt(contxt, OPT_HELP),
                back     = get_opt(contxt, OPT_BACK),
                deflt    = get_opt(contxt, OPT_DEFAULT);

        if(prompt && help && deflt)
        {
            const char *res = NULL;

            // Show requester unless we're executing in
            // 'novice' mode.
            if(get_numvar(contxt, "@user-level") > 0)
            {
                const char *prt = str(prompt),
                           *hlp = str(help),
                           *def = str(deflt);

                // Could we resolve all options?
                if(DID_ERR)
                {
                    // Return empty string.
                    REST;
                }

                // Prompt user.
                inp_t grc = gui_string(prt, hlp, def, back != false, &res);

                // Is the back option available?
                if(back)
                {
                    // Fake input?
                    if(get_numvar(contxt, "@back"))
                    {
                        grc = G_ABORT;
                    }

                    // On abort execute.
                    if(grc == G_ABORT)
                    {
                        return resolve(back);
                    }
                }

                // FIXME
                if(grc == G_ABORT || grc == G_EXIT)
                {
                    HALT;
                    REST;
                }
            }
            else
            {
                // Use the default value.
                res = str(deflt);
            }

            RSTR(DBG_ALLOC(strdup(res)));
        }

        // Missing one or more options.
        ERR(ERR_MISSING_OPTION, prompt ? help ?
            "default" : "help" : "default");

        // Return empty string
        // on failure.
        REST;
    }

    // The parser is broken
    PANIC(contxt);
    RCUR;
}
