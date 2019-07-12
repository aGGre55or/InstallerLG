//------------------------------------------------------------------------------
// probe.c:
//
// Environment information retrieval
//------------------------------------------------------------------------------
// Copyright (C) 2018-2019, Ola Söder. All rights reserved.
// Licensed under the AROS PUBLIC LICENSE (APL) Version 1.1
//------------------------------------------------------------------------------

#include "alloc.h"
#include "error.h"
#include "eval.h"
#include "probe.h"
#include "util.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef AMIGA
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/resident.h>
# ifdef __MORPHOS__
#  include <exec/system.h>
# endif
# ifdef __AROS__
#  include <proto/processor.h>
#  include <resources/processor.h>
# endif
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <workbench/workbench.h>
#endif

//------------------------------------------------------------------------------
// Name:        h_cpu_name
// Description: Helper for m_database. Get host CPU architecture.
// Input:       ---
// Return:      char *: Host CPU architecture.
//------------------------------------------------------------------------------
static char *h_cpu_name(void)
{
    enum { ERR, PPC, ARM, M68000, M68010, M68020, M68030, M68040, M68060, X86,
           X86_64, ALL };

    static char *cpu[ALL] = { "Unknown", "PowerPC", "ARM", "68000", "68010",
                              "68020", "68030", "68040", "68060", "x86",
                              "x84_64" };
    uint32_t arc = ERR;

    #if defined(__MORPHOS__) && !defined(LG_TEST)
    // On MorphOS, there is only PPC (for now) (and no define).
    NewGetSystemAttrs(&arc, sizeof(arc), SYSTEMINFOTYPE_MACHINE);
    arc = arc == 1 ? PPC : ERR;
    #elif defined(__AROS__) && !defined(LG_TEST)
    // On AROS, everything is possible.
    APTR ProcessorBase = OpenResource("processor.resource");

    if(ProcessorBase)
    {
        ULONG fam;

        struct TagItem tags[] = { { GCIT_Family, (IPTR) &fam },
                                  { TAG_DONE, TAG_DONE } };

        GetCPUInfo(tags);

        switch(fam)
        {
            case CPUFAMILY_60X:
            case CPUFAMILY_7X0:
            case CPUFAMILY_74XX:
            case CPUFAMILY_4XX:
               arc = PPC;
               break;

            case CPUFAMILY_ARM_3:
            case CPUFAMILY_ARM_4:
            case CPUFAMILY_ARM_4T:
            case CPUFAMILY_ARM_5:
            case CPUFAMILY_ARM_5T:
            case CPUFAMILY_ARM_5TE:
            case CPUFAMILY_ARM_5TEJ:
            case CPUFAMILY_ARM_6:
            case CPUFAMILY_ARM_7:
               arc = ARM;
               break;

            case CPUFAMILY_MOTOROLA_68000:
               arc = M68000;
               break;

            case CPUFAMILY_AMD_K5:
            case CPUFAMILY_AMD_K6:
            case CPUFAMILY_AMD_K7:
            case CPUFAMILY_INTEL_486:
            case CPUFAMILY_INTEL_PENTIUM:
            case CPUFAMILY_INTEL_PENTIUM_PRO:
            case CPUFAMILY_INTEL_PENTIUM4:
               arc = X86;
               break;

            case CPUFAMILY_AMD_K8:
            case CPUFAMILY_AMD_K9:
            case CPUFAMILY_AMD_K10:
               arc = X86_64;
               break;

            }
    }
    #elif defined(AMIGA) && !defined(LG_TEST)
    // AmigaOS3 - Beware, !NOT TESTED!.
    struct ExecBase *AbsSysBase = *((struct ExecBase **)4);
    UWORD flags = AbsSysBase->AttnFlags;

    if(flags & AFF_68010)
    {
        arc = M68010;
    }
    else if(flags & AFF_68020)
    {
        arc = M68020;
    }
    else if(flags & AFF_68030)
    {
        arc = M68030;
    }
    else if(flags & AFF_68040)
    {
        arc = M68040;
    }
    else if(flags & AFF_68060)
    {
        arc = M68060;
    }
    else
    {
        arc = M68000;
    }
    #endif

    // CPU or 'Unknown'.
    return cpu[arc];
}

//------------------------------------------------------------------------------
// (database <feature> [<checkvalue>])
//    return information about the Amiga that the Installer
//    is running on.
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//------------------------------------------------------------------------------
entry_p m_database(entry_p contxt)
{
    // We need atleast one argument
    C_SANE(1, NULL);

    int memf = -1;
    char *feat = str(C_ARG(1)), *ret = "Unknown";

    if(strcmp(feat, "cpu") == 0)
    {
        // Get host CPU name.
        ret = h_cpu_name();
    }
    else
    if(strcmp(feat, "os") == 0)
    {
        // Get OS name.
        #if defined(AMIGA) && !defined(LG_TEST)
        if(FindResident("MorphOS"))
        {
            ret = "MorphOS";
        }
        else
        // TODO - Try to open aros.library instead?
        if(FindResident("processor.resource"))
        {
            ret = "AROS";
        }
        else
        {
            ret = "AmigaOS";
        }
        #else
        ret = "Unknown";
        #endif

    }
    else
    if(strcmp(feat, "graphics-mem") == 0)
    {
        memf =
        #if defined(AMIGA) && !defined(LG_TEST)
        AvailMem(MEMF_CHIP);
        #else
        524288;
        #endif
    }
    else
    if(!strcmp(feat, "total-mem"))
    {
        memf =
        #if defined(AMIGA) && !defined(LG_TEST)
        AvailMem(MEMF_ANY);
        #else
        524288;
        #endif
    }

    if(memf != -1)
    {
        ret = get_buf();
        snprintf(get_buf(), buf_size(), "%d", memf);
    }

    // Are we testing for a specific value?
    if(C_ARG(2) && C_ARG(2) != end())
    {
        ret = strcmp(ret, str(C_ARG(2))) ? "0" : "1";
    }

    // Return string value.
    R_STR(DBG_ALLOC(strdup(ret)));
}

//------------------------------------------------------------------------------
// (earlier <file1> <file2>)
//     true if file1 earlier than file2
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//------------------------------------------------------------------------------
entry_p m_earlier(entry_p contxt)
{
    // We need two filenames.
    C_SANE(2, NULL);

    // One struct per file.
    struct stat old, new;

    // Get information about the first file.
    if(stat(str(C_ARG(1)), &old))
    {
        // Could not read from file / dir.
        ERR(ERR_READ, str(C_ARG(1)));
        R_NUM(0);
    }

    // Get information about the second file.
    if(stat(str(C_ARG(2)), &new))
    {
        // Could not read from file / dir.
        ERR(ERR_READ, str(C_ARG(2)));
        R_NUM(0);
    }

    // Is the first older than the second one?
    R_NUM(old.st_mtime < new.st_mtime ? 1 : 0);
}

//------------------------------------------------------------------------------
// (getassign <name> <opts>)
//     return value of logical name (no `:') `<opts>': 'v'-volumes,
//     'a'-assigns, 'd'-devices
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//------------------------------------------------------------------------------
entry_p m_getassign(entry_p contxt)
{
    // We need one or more arguments.
    C_SANE(1, NULL);

    // On non Amiga systems, or in test mode, this is a stub.
    #if defined(AMIGA) && !defined(LG_TEST)
    struct DosList *dl;
    const char *asn = str(C_ARG(1));
    size_t asnl = strlen(asn);

    if(!asnl)
    {
        // Invalid name of assign, and the empty string is how the CBM
        // installer fails.
        R_EST;
    }

    // The second argument is optional.
    entry_p opt = C_ARG(2);

    // The bitmask must contain atleast this LDF_READ.
    ULONG msk = LDF_READ;

    // Parse the option string if it exists.
    if(opt && opt != end())
    {
        const char *o = str(opt);

        if(*o)
        {
            // Translate from Installer speak to DOS speak.
            while(*o)
            {
                msk |= (*o == 'v') ? LDF_VOLUMES : 0;
                msk |= (*o == 'd') ? LDF_DEVICES : 0;
                msk |= (*o == 'a') ? LDF_ASSIGNS : 0;
                o++;
            }
        }
        else
        {
            // The CBM installer returns an empty string if option string is
            // empty.
            R_EST;
        }
    }
    else
    // The default behaviour, when no option string is present, is to look for
    // logical assignments.
    {
        msk |= LDF_ASSIGNS;
    }

    // Prepare to walk the device list.
    dl = (struct DosList *) LockDosList(msk);

    if(dl)
    {
        ULONG bits[] = { LDF_ASSIGNS, LDF_VOLUMES, LDF_DEVICES, 0 };

        // Iterate over all flags.
        for(size_t i = 0; bits[i]; i++)
        {
            // For all flags matching the option string (or its absence),
            // traverse the doslist.
            if(msk & bits[i])
            {
                struct DosList *dc = (struct DosList *)
                                      NextDosEntry(dl, bits[i]);
                while(dc)
                {
                    const char *n = B_TO_CSTR(dc->dol_Name);

                    // Ignore case when looking for match.
                    if(!strcasecmp(asn, n))
                    {
                        // Unlock doslist and allocate enough memory to hold any
                        // path.
                        char *r = DBG_ALLOC(calloc(PATH_MAX, 1));
                        UnLockDosList(msk);

                        if(r)
                        {
                            // The form common to all types. Do we need to do
                            // anything with LDF_VOLUMES?
                            snprintf(r, PATH_MAX, "%s:", n);

                            // Logical assignments. Get the full path from the
                            // lock.
                            if(bits[i] == LDF_ASSIGNS)
                            {
                                BPTR l = (BPTR) Lock(r, ACCESS_READ);

                                if(l)
                                {
                                    NameFromLock(l, r, PATH_MAX);
                                    UnLock(l);
                                }
                            }
                            // Devices. No other options than 'd' are allowed
                            // in the options string for some reason (in the
                            // CBM installer).
                            else if(bits[i] == LDF_DEVICES)
                            {
                                if((bits[i] | LDF_READ) == msk)
                                {
                                    // Cut ':'.
                                    r[asnl] = '\0';
                                }
                                else
                                {
                                    // Clear.
                                    r[0] = '\0';
                                }
                            }

                            // Success.
                            R_STR(r);
                        }
                        else
                        {
                            // Out of memory
                            PANIC(contxt);
                            R_EST;
                        }
                    }
                    else
                    {
                        // No match. Next list item.
                        dc = (struct DosList *) NextDosEntry(dc, bits[i]);
                    }
                }
            }
        }

        // Not found.
        UnLockDosList(msk);
    }
    else
    {
        // Could not lock doslist. I'm not sure this can happen. I believe
        // LockDosList will block until the end of time if a problem occurs.
        ERR(ERR_READ, asn);
    }
    #endif

    // Return empty string on failure.
    R_EST;
}

//------------------------------------------------------------------------------
// (getdevice <path>)
//     returns name of device upon which <path> resides
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//------------------------------------------------------------------------------
entry_p m_getdevice(entry_p contxt)
{
    // We need a path.
    C_SANE(1, NULL);

    #if defined(AMIGA) && !defined(LG_TEST)
    // Attempt to lock path.
    BPTR lock = (BPTR) Lock(str(C_ARG(1)), ACCESS_READ);

    if(lock)
    {
        struct InfoData id;

        // Get vol info from file / dir lock.
        if(Info(lock, &id))
        {
            struct DosList *dl = (struct DosList *) id.id_VolumeNode;
            UnLock(lock);

            if(dl)
            {
                struct MsgPort *mp = dl->dol_Task;
                ULONG msk = LDF_READ | LDF_DEVICES;

                // Search for <path> handler in the list of devices.
                dl = (struct DosList *) LockDosList(msk);

                while(dl && mp != dl->dol_Task)
                {
                    dl = (struct DosList *) NextDosEntry(dl, LDF_DEVICES);
                }

                UnLockDosList(msk);

                // If we found it, we also found the name of the device.
                if(dl)
                {
                    const char *n = B_TO_CSTR(dl->dol_Name);

                    // strdup(NULL) is undefined.
                    if(n)
                    {
                        R_STR(DBG_ALLOC(strdup(n)));
                    }

                    // Unknown error.
                    PANIC(contxt);
                }
            }
        }
        else
        {
            UnLock(lock);
        }
    }

    // Could not get information about <path>.
    ERR(ERR_READ, str(C_ARG(1)));
    #endif

    // Return empty string on failure.
    R_EST;
}

//------------------------------------------------------------------------------
// (getdiskspace <pathname> [<unit>])                                    (V44)
//     return available space
//
// Refer to Installer.guide 1.20 (25.10.1999) 1995-99 by Amiga Inc.
//------------------------------------------------------------------------------
entry_p m_getdiskspace(entry_p contxt)
{
    // We need a path.
    C_SANE(1, NULL);

    #if defined(AMIGA) && !defined(LG_TEST)
    // Attempt to lock path.
    const char *n = str(C_ARG(1));
    BPTR lock = (BPTR) Lock(n, ACCESS_READ);

    // Do we have a lock?
    if(lock)
    {
        struct InfoData id;

        // Retrieve information from lock.
        if(Info(lock, &id))
        {
            long long free = (long long) (id.id_NumBlocks - id.id_NumBlocksUsed)
                                          * id.id_BytesPerBlock;
            // Release lock ASAP.
            UnLock(lock);

            // From the Installer.guide 1.20:
            //
            // The parameter <unit> is optional and defines the unit for the
            // returned disk space: "B" (or omitted) is "Bytes", "K" is
            // "Kilobytes", "M" is "Megabytes" and "G" is "Gigabytes".
            if(C_ARG(2) && C_ARG(2) != end())
            {
                switch(*str(C_ARG(2)))
                {
                    case 'K':
                    case 'k':
                        free >>= 10;
                        break;

                    case 'M':
                    case 'm':
                        free >>= 20;
                        break;

                    case 'G':
                    case 'g':
                        free >>= 30;
                        break;
                }
            }

            // Cap the return value.
            R_NUM(free > INT_MAX ? INT_MAX : (int) free);
        }

        // Info() failed. Release lock.
        UnLock(lock);
    }

    // For some reason, we could not acquire a lock on <path>, or, we could get
    // a lock, but failed when trying to retrieve info from the lock.
    ERR(ERR_READ, n);
    #endif

    // Failure.
    R_NUM(-1);
}

//------------------------------------------------------------------------------
// (getenv <name>)
//     return value of environment variable
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//------------------------------------------------------------------------------
entry_p m_getenv(entry_p contxt)
{
    // We need a variable name.
    C_SANE(1, NULL);

    // Is there such an environment variable?
    char *env = getenv(str(C_ARG(1)));

    if(env)
    {
        // Return what we found.
        R_STR(DBG_ALLOC(strdup(env)));
    }

    // Nothing found, return empty string.
    R_EST;
}

//------------------------------------------------------------------------------
// (getsize <file>)
//     return size
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//------------------------------------------------------------------------------
entry_p m_getsize(entry_p contxt)
{
    // We need a file name.
    C_SANE(1, NULL);

    // Open the file in read only mode.
    FILE *file = fopen(str(C_ARG(1)), "r");

    if(file)
    {
        // Seek to the end of the file.
        fseek(file, 0L, SEEK_END);

        // Let the result be the position.
        int res = (int) ftell(file);

        // We're done.
        fclose(file);

        // Return position.
        R_NUM(res);
    }

    // Could not read from file.
    ERR(ERR_READ_FILE, str(C_ARG(1)));
    R_NUM(0);
}

//------------------------------------------------------------------------------
// (getsum <file>)
//     return checksum of file for comparison purposes
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//------------------------------------------------------------------------------
entry_p m_getsum(entry_p contxt)
{
    // We need a filename.
    C_SANE(1, NULL);

    const char *name = str(C_ARG(1));
    FILE *file = fopen(name, "r");

    if(file)
    {
        // This will yield different results on 32 / 64 bit systems but that's
        // hardly a problem (or?).
        int chr = getc(file), alfa = 1, beta = 0;

        // Adler-32 checksum.
        while(chr != EOF)
        {
            alfa = (alfa + chr) % 65521;
            beta = (alfa + beta) % 65521;
            chr = getc(file);
        }

        // We're done.
        fclose(file);

        // Return checksum.
        R_NUM((beta << 16) | alfa);
    }

    // Could not read from file.
    ERR(ERR_READ_FILE, name);
    R_NUM(0);
}

//------------------------------------------------------------------------------
// Name:        h_getversion_dev
// Description: Helper for m_getversion. Get device version.
// Input:       char *name: Filename.
// Return:      int:        File version.
//------------------------------------------------------------------------------
static int h_getversion_dev(const char *name)
{
    // Assume failure.
    int ver = -1;

    #if defined(AMIGA) && !defined(LG_TEST)
    struct IORequest req;

    // Assume that 'name' is a device that can be opened.
    if(!OpenDevice(name, 0, &req, 0))
    {
        // Be paranoid.
        if(req.io_Device)
        {
            // Get version and revision.
            ver = req.io_Device->dd_Library.lib_Version << 16 |
                  req.io_Device->dd_Library.lib_Revision;
        }

        // Device is no longer needed.
        CloseDevice(&req);
    }
    #else
    // Not used.
    (void) name;
    #endif

    return ver;
}

//------------------------------------------------------------------------------
// Name:        h_getversion_lib
// Description: Helper for m_getversion. Get library version.
// Input:       char *name: Library name.
// Return:      int:        Library version.
//------------------------------------------------------------------------------
static int h_getversion_lib(const char *name)
{
    // Assume failure.
    int ver = -1;

    #if defined(AMIGA) && !defined(LG_TEST)
    struct Library *lib = OpenLibrary(name, 0);

    // Assume that 'name' is a library that can be opened.
    if(lib)
    {
        // Get version and revision.
        ver = (lib->lib_Version << 16) | lib->lib_Revision;

        // Library is no longer needed.
        CloseLibrary(lib);
    }
    #else
    // Not used.
    (void) name;
    #endif

    return ver;
}

//------------------------------------------------------------------------------
// Name:        h_getversion_res
// Description: Helper for m_getversion. Get resident version.
// Input:       char *name: Resident name.
// Return:      int:        Resident version.
//------------------------------------------------------------------------------
static int h_getversion_res(const char *name)
{
    // Assume failure.
    int ver = -1;

    #if defined(AMIGA) && !defined(LG_TEST)
    struct Resident *res = (struct Resident *) FindResident(name);

    if(res)
    {
        // Version string pattern.
        const char *pat = "%*[^0123456789]%d.%d%*[^\0]";
        const char *ids = res->rt_IdString;

        // Major and revision.
        int maj, rev;

        // Try to find the revision, if any, in the id string. The major part
        // of our parsed result should match rt_Version.
        if(sscanf(ids, pat, &maj, &rev) == 2 && maj == res->rt_Version)
        {
            // We found both major and revision.
            ver = (maj << 16) | rev;
        }
        else
        {
            // We can't trust the parsed result. Use what we know, ignore the
            // revision.
            ver = res->rt_Version << 16;
        }
    }
    #else
    // Not used.
    (void) name;
    #endif

    return ver;
}

//------------------------------------------------------------------------------
// Name:        h_getversion_file
// Description: Helper for m_getversion. Get file version.
// Input:       char *name: Filename.
// Return:      int:        File version.
//------------------------------------------------------------------------------
int h_getversion_file(const char *name)
{
    FILE *file = NULL;

    // File path prefixes.
    const char *pre[] = {"", "LIBS:", NULL};

    #if defined(AMIGA) && !defined(LG_TEST)
    struct Process *pro = (struct Process *) FindTask(NULL);

    // Save the current window ptr.
    APTR win = pro->pr_WindowPtr;

    // Disable auto request.
    pro->pr_WindowPtr = (APTR) -1L;
    #endif

    for(size_t ndx = 0; !file && pre[ndx]; ndx++)
    {
        // Use global buffer.
        char *buf = get_buf();

        // Format path string..
        snprintf(buf, buf_size(), "%s%s", pre[ndx], name);

        // Attempt to open file.
        file = fopen(buf, "r");
    }

    #if defined(AMIGA) && !defined(LG_TEST)
    // Restore auto request.
    pro->pr_WindowPtr = win;
    #endif

    // Invalid version.
    int ver = -1;

    // Bail out on error.
    if(!file)
    {
        // Failure.
        return ver;
    }

    // Version key string.
    int key[] = {'$','V','E','R',':', ' ', 0};
    size_t ndx = 0;

    // Find position of the version key.
    for(int chr = 0; chr != EOF && key[ndx];
        ndx = chr == key[ndx] ? ndx + 1 : 0)
    {
        chr = fgetc(file);
    }

    // Did we find the key?
    if(!key[ndx])
    {
        // Use global buffer.
        char *buf = get_buf();

        // Fill up buffer with enough data to hold any realistic version string.
        fread(buf, 1, buf_size(), file);

        // Begin after whitespace.
        char *data = strchr(buf, ' ');

        if(data)
        {
            // Major and revision.
            int maj = 0, rev = 0;

            // Version string pattern.
            const char *pat = "%*[^0123456789]%d.%d%*[^\0]";

            // Try to find version string.
            if(sscanf(data, pat, &maj, &rev) == 2)
            {
                // We found something.
                ver = (maj << 16) | rev;
            }
        }
    }

    // We're done.
    fclose(file);

    // Version or -1;
    return ver;
}

//------------------------------------------------------------------------------
// (getversion <file> (resident))
//     return version/revision of file, library, etc. as 32 bit num
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//------------------------------------------------------------------------------
entry_p m_getversion(entry_p contxt)
{
    // Arguments are optional.
    C_SANE(0, NULL);

    // Any arguments / options given?
    if(contxt->children)
    {
        // If so, they must be valid and >= 1.
        C_SANE(1, contxt);

        // Name of file / lib / etc.
        const char *name = str(C_ARG(1));

        // Invalid version.
        int ver = -1;

        // Get resident module version.
        if(get_opt(contxt, OPT_RESIDENT))
        {
            ver = h_getversion_res(name);
        }

        // Get file version.
        if(ver == -1)
        {
            ver = h_getversion_file(name);
        }

        // Get library version.
        if(ver == -1)
        {
            ver = h_getversion_lib(name);
        }

        // Get device version.
        if(ver == -1)
        {
            ver = h_getversion_dev(name);
        }

        // Did all of the above fail?
        if(ver == -1)
        {
            ver = 0;
        }

        // Return whatever we have, could be a failure (0) or version and
        // revision information.
        R_NUM(ver);
    }

    #if defined(AMIGA) && !defined(LG_TEST)
    // No arguments, return version of Exec.
    extern struct ExecBase *SysBase;
    R_NUM((SysBase->LibNode.lib_Version << 16) | SysBase->SoftVer);
    #else
    R_NUM(0);
    #endif
}

//------------------------------------------------------------------------------
// (iconinfo <parameters>)
//     return information about an icon (V42.12)
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//
// Despite what the Installer.guide says, 'help', 'prompt', 'confirm'
// and 'safe' are dead options in OS 3.9. We choose to ignore the guide
// and mimic the behaviour of the implementation in 3.9. Also, the 3.9
// implementation deletes tooltypes when (gettooltype) is used. This is
// assumed to be a bug and therefore not mimiced.
//------------------------------------------------------------------------------
entry_p m_iconinfo(entry_p contxt)
{
    // One or more arguments.
    C_SANE(1, contxt);

    entry_p dst     =   get_opt(contxt, OPT_DEST);
    entry_p types[] = { get_opt(contxt, OPT_GETTOOLTYPE),
                        get_opt(contxt, OPT_GETDEFAULTTOOL),
                        get_opt(contxt, OPT_GETSTACK),
                        get_opt(contxt, OPT_GETPOSITION), end() };

    // We need an icon.
    if(!dst)
    {
        ERR(ERR_MISSING_OPTION, "dest");
        R_NUM(0);
    }

    // Suffix isn't needed.
    char *file = str(dst);

    #if defined(AMIGA) && !defined(LG_TEST)
    // Get icon information.
    struct DiskObject *obj = (struct DiskObject *) GetDiskObject(file);
    #else
    char *obj = file;
    #endif

    // Exit if we can't read from icon.
    if(!obj)
    {
        ERR(ERR_READ_FILE, file);
        R_NUM(0);
    }

    // Iterate over all options.
    for(size_t i = 0; types[i] != end() && !DID_ERR; i++)
    {
        // Is the current option not set?
        if(!types[i])
        {
            // Next option.
            continue;
        }

        // Iterate over all its children.
        for(size_t j = 0; types[i]->children[j] &&
            types[i]->children[j] != end(); j++)
        {
            // Get variable name.
            char *name = str(types[i]->children[j]);

            // Skip empty variable names.
            if(!(*name))
            {
                continue;
            }

            // Get option type.
            int type = types[i]->id;
            char *svl = NULL;

            #if defined(AMIGA) && !defined(LG_TEST)
            // Is this a numerical value?
            if(type == OPT_GETSTACK || type == OPT_GETPOSITION)
            {
                int v = (type == OPT_GETSTACK ? obj->do_StackSize : j == 0 ?
                         obj->do_CurrentX : obj->do_CurrentY);

                snprintf(get_buf(), buf_size(), "%d", v);
                svl = get_buf();
            }
            else
            if(type == OPT_GETDEFAULTTOOL && obj->do_DefaultTool)
            {
                svl = obj->do_DefaultTool;
            }
            else
            if(type == OPT_GETTOOLTYPE && obj->do_ToolTypes)
            {
                svl = (char *) FindToolType(obj->do_ToolTypes, name);
                name = str(types[i]->children[++j]);
            }

            // Always a valid value.
            svl = svl ? svl : "";
            #else
            // Testing purposes only.
            snprintf(get_buf(), buf_size(), "%d:%zu", type, j);
            svl = get_buf();
            #endif

            // Always a valid (string).
            entry_p val = new_string(DBG_ALLOC(strdup(svl)));

            // Unless we're out of memory.
            if(!val)
            {
                PANIC(contxt);
                break;
            }

            // If a symbol with the same name as that of the option, replace
            // the value of the old one with the new value.
            if(contxt->symbols)
            {
                for(size_t k = 0; contxt->symbols[k] &&
                    contxt->symbols[k] != end(); k++)
                {
                    if(!strcasecmp(contxt->symbols[k]->name, name))
                    {
                        kill(contxt->symbols[k]->resolved);
                        contxt->symbols[k]->resolved = val;
                        push(global(contxt), contxt->symbols[k]);
                        val->parent = contxt->symbols[k];

                        // We no longer own 'val'.
                        val = NULL;
                        break;
                    }
                }
            }

            // Continue with the next symbol if this one exists.
            if(!val)
            {
                continue;
            }

            // This is a new symbol. Create, append to this function and push to
            // the global context.
            entry_p sym = new_symbol(DBG_ALLOC(strdup(name)));

            if(!sym)
            {
                // Out of memory. Do not leak 'val'.
                kill(val);
                continue;
            }

            // Adopt the value found above.
            val->parent = sym;
            sym->resolved = val;

            // PANIC set by append on out of memory.
            if(append(&contxt->symbols, sym))
            {
                push(global(contxt), sym);
                sym->parent = contxt;
            }
        }
    }

    #if defined(AMIGA) && !defined(LG_TEST)
    FreeDiskObject(obj);
    #endif

    // Success.
    R_NUM(1);
}
