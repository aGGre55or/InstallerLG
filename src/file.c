//----------------------------------------------------------------------------
// file.h: 
//
// File operations 
//
//----------------------------------------------------------------------------

#include "alloc.h"
#include "error.h"
#include "eval.h"
#include "file.h"
#include "gui.h"
#include "probe.h"
#include "resource.h"
#include "strop.h"
#include "util.h"

#include <dirent.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#ifdef AMIGA
#include <dos/dos.h>
#include <dos/dosasl.h>
#include <dos/dosextens.h>
#include <workbench/workbench.h>
#endif

//----------------------------------------------------------------------------
// (expandpath <path>)
//     Expands a short path to its full path equivalent
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_expandpath(entry_p contxt)
{
    // We need one argument, a short path.
    if(c_sane(contxt, 1))
    {
        // On non Amiga systems, this is a dummy.
        #ifdef AMIGA
        BPTR lock = (BPTR) Lock(str(CARG(1)), ACCESS_READ);
        if(lock)
        {
            // Allocate enough to hold any valid path.
            char *buf = CALLOC(PATH_MAX, 1);

            if(buf)
            {
                // FIXME - error handling
                NameFromLock(lock, buf, PATH_MAX); 
                UnLock(lock); 
                RSTR(buf);
            }

            UnLock(lock); 
        }
        #endif
    }
    else
    {
        // Broken parser
        error(PANIC); 
    }

    // Return empty string.
    REST;
}

//----------------------------------------------------------------------------
// Name:        h_confirm(entry_p contxt, 
//                        const char *hlp, 
//                        const char *msg,
//                        ...)
//
// Description: Ask user for confirmation (proceed / skip / abort). 
//
// Input:       entry_p contxt:     The execution context.
//              const char *hlp:    Help text.
//              const char *msg:    Message format string.
//              ...:                Format string varargs.
//
// Return:      int:                If confirmed '1', else '0'. Both skip and
//                                  abort will return a value of '0'.
//----------------------------------------------------------------------------
static int h_confirm(entry_p contxt, 
                     const char *hlp, 
                     const char *msg,
                     ...) 
{
    if(contxt)
    {
        // By setting @yes, @skip or @abort
        // user behaviour can be simulated.
        int yes = get_numvar(contxt, "@yes"),
            skip = get_numvar(contxt, "@skip"),
            abort = get_numvar(contxt, "@abort"),
            ret; 

        va_list ap; 

        // Format messsage string.
        va_start(ap, msg); 
        vsnprintf(get_buf(), buf_size(), msg, ap); 
        va_end(ap); 

        // If we have any overrides, translate
        // them to the corresponding gui return 
        // value. 
        if(yes || skip || abort)
        {
            ret = abort ? -1 : yes; 
        }
        // No overrides, get confirmation from 
        // the user. 
        else
        {
            ret = gui_run(get_buf(), hlp);
        }

        // On abort, set HALT state. The return
        // value is that same as skip.
        if(ret < 0)
        {
            error(HALT); 
            ret = 0; 
        }

        // True or false.
        return ret; 
    }
    else
    {
        // Broken parser. 
        error(PANIC); 
        return 0; 
    }
}

//----------------------------------------------------------------------------
// Name:        h_exists(const char *n)
//
// Description: Get file / dir info.
//
// Input:       entry_p contxt:     The execution context.
//              const char *n:      Path to file / dir.
//
// Return:      int:                Dir = '2', file = '1' else '0'.
//----------------------------------------------------------------------------
static int h_exists(const char *n)
{
    // NULL is a valid argument but
    // this 'file' doesn't exist.
    if(n)
    {
        #ifdef AMIGA
        int r = 0; 
        struct FileInfoBlock *fib = (struct FileInfoBlock *) 
               AllocDosObject(DOS_FIB, NULL); 

        if(fib)
        {
            BPTR lock = (BPTR) Lock(n, ACCESS_READ);

            if(lock)
            {
                if(Examine(lock, fib))
                {
                    // ST_FILE         -3
                    // ST_LINKFILE     -4
                    if(fib->fib_DirEntryType < 0)
                    {
                        // A file. 
                        r = 1;  
                    }
                    // ST_ROOT          1
                    // ST_USERDIR       2
                    // ST_SOFTLINK      3 
                    // ST_LINKDIR       4
                    else if(fib->fib_DirEntryType > 0)
                    {
                        // A directory. 
                        r = 2; 
                    }
                    // Return values according to the 
                    // CBM installer documentation.
                }

                UnLock(lock); 
            }

            FreeDosObject(DOS_FIB, fib); 
        }

        return r; 
        #else
        // This implementation doesn't work on MorphOS.
        // I have no clue why, it works on AROS. Let's
        // use the implementation above on all Amiga
        // like systems for now.
        struct stat fs; 
        if(!stat(n, &fs))
        {
            // A file ...
            if(S_ISREG(fs.st_mode))
            {
                // Value according to the CBM 
                // installer documentation.
                return 1; 
            }
            // ... or a directory ...
            if(S_ISDIR(fs.st_mode))
            {
                // Ibid.
                return 2; 
            }
        }
        #endif
    }

    // Ibid.
    return 0; 
}

//----------------------------------------------------------------------------
// Name:        h_fileonly(int id,
//                         const char *n)
//
// Description: Get file part from full path.
//
// Input:       int id:             The ID of the execution context.
//              const char *n:      Path to file.
//
// Return:      const char *:       On success, file part of path, otherwise
//                                  empty string.
//----------------------------------------------------------------------------
static const char *h_fileonly(int id, 
                              const char *s)
{
    if(s)
    {
        size_t i = strlen(s); 

        // Do we have a string that doesn't
        // look like a directory / volume?
        if(i-- && 
           s[i] != '/' && 
           s[i] != ':' )
        {
            // Go backwards until we find a
            // delimiter or the beginning of
            // the string.
            while(i &&
                  s[i - 1] != '/' && 
                  s[i - 1] != ':' ) i--;

            // Return the new offset.
            return (s + i); 
        }   
        else
        {
            // Empty string or dir / vol.
            error(id, ERR_NOT_A_FILE, s); 
        }   
    }
    else
    {
        // Bad input.
        error(PANIC);
    }

    // Always return a valid string pointer.
    return ""; 
}

//----------------------------------------------------------------------------
// Name:        h_filetree(int id, 
//                         const char *src, 
//                         const char *dst, 
//                         entry_p files, 
//                         entry_p fonts, 
//                         entry_p choices, 
//                         entry_p pattern)
//
// Description: Generate a complete file / directory tree with source and
//              destination tuples. Used by m_copyfiles.
//
// Input:       int id:             The ID of the execution context.
//              const char *src:    Source directory / file.
//              const char *dst:    Destination directory.
//              entry_p files:      * Files only.
//              entry_p fonts:      * Skip fonts.
//              entry_p choices:    * List of files.
//              entry_p pattern:    * File / dir pattern.
//
//              * Refer to the Istaller.guide.
//
// Return:      int:                On success '1', else '0'.
//----------------------------------------------------------------------------
static pnode_p h_filetree(int id, 
                          const char *src, 
                          const char *dst, 
                          entry_p files, 
                          entry_p fonts, 
                          entry_p choices, 
                          entry_p pattern)
{
    char *n_src = NULL,
         *n_dst = NULL; 

    if(src && dst)
    {
        int type = h_exists(src); 

        // Is source a directory?
        if(type == 2)
        {
            DIR *dir = opendir(src);  

            if(dir) 
            {
                // If the (choices) option is set we need to
                // verify that all choices exist (as files).
                if(choices)
                {
                    // Unless the parser is broken, 
                    // we will have >= one child. 
                    entry_p *e = choices->children;

                    // Iterate over all filenames.
                    while(*e && *e != end())
                    {
                        // Build path. 
                        n_src = h_tackon(id, src, str(*e)); 

                        if(n_src)
                        {
                            // If it doesn't exist, free everything and
                            // bail out. 
                            if(!h_exists(n_src))
                            {
                                error(id, ERR_NO_SUCH_FILE_OR_DIR, n_src); 
                                free(n_src); 
                                closedir(dir); 
                                return NULL; 
                            }
                            else
                            {
                                // Next filename.
                                free(n_src); 
                                e++; 
                            }
                        }
                        else
                        {
                            // Out of memory.
                            break;
                        }
                    }
                }

                // Create head node.
                pnode_p node = CALLOC(1, sizeof(struct pnode_t)), 
                        head = node; 

                if(node)
                {
                    struct dirent *entry = readdir(dir); 

                    // We already know the type of the
                    // first element; it's a directory.
                    node->name = STRDUP(src); 
                    node->copy = STRDUP(dst); 
                    node->type = 2; 

                    // Iterate over all entries in the source
                    // directory.
                    while(entry)
                    {
                        // Create the source destination tuple
                        n_src = h_tackon(id, src, entry->d_name), 
                        n_dst = h_tackon(id, dst, entry->d_name); 

                        if(n_src && n_dst)
                        {
                            if(choices)
                            {
                                // Unless the parser is broken, 
                                // we will have >= one child. 
                                entry_p *e = choices->children;

                                // Iterate over all filenames.
                                while(*e && *e != end())
                                {
                                    // Stop when we have a match.
                                    if(!strcasecmp(str(*e), entry->d_name))
                                    {
                                        break; 
                                    }

                                    // Next filename.
                                    e++; 
                                }

                                // Get proper type if we have a match.
                                if(*e && *e != end())
                                {
                                    type = h_exists(n_src);
                                }
                                // Otherwise clear type, this will make
                                // m_copyfiles skip it.
                                else
                                {
                                    type = 0; 
                                }
                            }
                            else 
                            if(pattern)
                            {
                                #ifdef AMIGA
                                // Use a static buffer, Installer.guide
                                // restricts pattern length to 64. It 
                                // seems like MatchPattern can use a lot
                                // of stack if we use long patterns, so 
                                // let's not get rid of this limitation. 
                                static char pat[BUFSIZ]; 
                                LONG w = ParsePattern(str(pattern), pat, sizeof(pat)); 

                                // Can we parse the pattern?
                                if(w >= 0)
                                {
                                    // Use pattern matching if we have one or more
                                    // wildcards, otherwise use plain strcmp().
                                    if((w && MatchPattern(pat, entry->d_name)) ||
                                       (w && !strcmp(pat, entry->d_name)))
                                    {
                                        // Match, get proper type.
                                        type = h_exists(n_src); 
                                    }
                                    else
                                    {
                                        // Not a match, skip this.
                                        type = 0; 
                                    }
                                }
                                else
                                {
                                    // We probably had a buffer overflow.
                                    // No more pattern matching today.
                                    error(id, ERR_OVERFLOW, str(pattern)); 
                                    pattern = NULL; 
                                }
                                #else
                                type = h_exists(n_src); 
                                #endif
                            }
                            else
                            {
                                // File or directory? 
                                type = h_exists(n_src); 
                            }

                            // Filter out fonts?
                            if(fonts)
                            {
                                static char suf[] = ".font"; 
                                char *pos = strstr(n_src, suf); 

                                if(pos && !strcmp(pos, suf))
                                {
                                    // It's a font. Skip it.
                                    type = 0; 
                                }
                            }

                            // If we have a directory, recur.
                            if(type == 2)
                            {
                                // Unless the (files) option is set. 
                                if(!files)
                                {
                                    #ifndef AMIGA 
                                    if(strcmp(entry->d_name, ".") &&
                                       strcmp(entry->d_name, ".."))
                                    #endif
                                    {
                                        // Get tree of subdirectory.
                                        // Don't promote (choices), 
                                        // dirs will be assumed to 
                                        // be files and things will
                                        // break. 
                                        node->next = h_filetree
                                        (
                                            id, 
                                            n_src, 
                                            n_dst, 
                                            files,
                                            fonts,
                                            NULL,
                                            pattern
                                        ); 

                                        // Fast forward to the end of
                                        // the list.
                                        while(node->next)
                                        {
                                            node = node->next; 
                                        }
                                    }
                                }

                                // We don't need to store the names
                                // of directories, release them.
                                free(n_src); 
                                free(n_dst); 
                            }
                            else
                            {
                                node->next = CALLOC(1, sizeof(struct pnode_t)); 

                                if(node->next)
                                {
                                    node->next->type = type; 
                                    node->next->name = n_src; 
                                    node->next->copy = n_dst; 
                                    node = node->next; 
                                }
                                else
                                {
                                    error(PANIC); 
                                    break; 
                                }
                            }
                        }
                        else
                        {
                            // Out of memory.
                            break; 
                        }

                        // Get next entry.
                        entry = readdir(dir); 
                    }
                }

                // No more entries.
                closedir(dir); 

                // The list is complete.
                return head; 
            }
            else
            {
                error(id, ERR_READ_DIR, src); 
                return NULL; 
            }
        }
        else
        // Is source a file? 
        if(type == 1)
        {
            pnode_p file = CALLOC(1, sizeof(struct pnode_t)),
                    head = CALLOC(1, sizeof(struct pnode_t)); 

            if(head && file)
            {
                n_src = STRDUP(src); 
                n_dst = STRDUP(dst); 

                if(n_src && n_dst)  
                {
                    // The destination of the head element
                    // will be a directory even though the
                    // source is a file. We need somewhere
                    // to put the file.
                    head->type = 2; 
                    head->next = file; 
                    head->name = n_src; 
                    head->copy = n_dst; 

                    // Create destination file path.
                    n_dst = h_tackon(id, dst, h_fileonly(id, src)); 
                    n_src = STRDUP(src);

                    if(n_src && n_dst)  
                    {
                        // The second element in the list
                        // will be the file.
                        file->type = 1; 
                        file->name = n_src; 
                        file->copy = n_dst; 

                        // The list is complete.
                        return head; 
                    }
                    else
                    {
                        // Out of memory.
                        free(head->name); 
                        free(head->copy); 
                    }
                }

                // Out of memory.
                free(head); 
                free(file); 
            }
        }
        else
        // It's neither a directory or a file.
        {
            error(id, ERR_NO_SUCH_FILE_OR_DIR, src); 
            return NULL; 
        }
    }

    // Out of memory.
    error(PANIC); 

    // These might leak when OOM
    // if we don't free them.
    free(n_src); 
    free(n_dst); 

    return NULL; 
}

//----------------------------------------------------------------------------
// Name:        h_protect_get(entry_p contxt, 
//                            const char *file, 
//                            LONG *mask)
//
// Description: Utility function used by m_protect and m_copyfiles to get
//              file / dir protection bits. 
//
// Input:       entry_p contxt:     The execution context.
//              const char *file:   File / dir.
//              LONG *mask:         Pointer to the LONG to hold the result.
//
// Return:      int:                On success '1', else '0'. 
//----------------------------------------------------------------------------
static int h_protect_get(entry_p contxt, 
                         const char *file,
                         LONG *mask)
{
    if(contxt && mask && file)
    {
        // On non Amiga systems, this is a stub.
        #ifdef AMIGA
        struct FileInfoBlock *fib = (struct FileInfoBlock *) 
            AllocDosObject(DOS_FIB, NULL); 

        if(fib)
        {
            // Attempt to lock file / dir.
            BOOL done = FALSE; 
            BPTR lock = (BPTR) Lock(file, ACCESS_READ);

            // Did we obtain a lock?
            if(lock)
            {
                // Fill up FIB and get bits.
                if(Examine(lock, fib))
                {
                    *mask = fib->fib_Protection; 
                    done = TRUE; 
                }

                UnLock(lock); 
            }

            // Free FIB memory.
            FreeDosObject(DOS_FIB, fib); 

            // Did everything above succeed?
            if(!done)
            {
                // No, fail and set impossible mask.
                error(contxt->id, ERR_GET_PERM, file); 
                *mask = -1; 
            }

            // If enabled, write to log file.
            h_log(contxt, tr(S_GMSK), file, *mask); 

            // MASK or -1.
            return done; 
        }
        #else
        // Always succeed on non Amiga systems.
        return 1; 
        #endif
    }

    // Out of memory.
    error(PANIC); 
    return 0; 
}

//----------------------------------------------------------------------------
// Name:        h_protect_set(entry_p contxt, 
//                            const char *file, 
//                            LONG mask)
//
// Description: Utility function used by m_protect and m_copyfiles to set
//              file / dir protection bits. 
//
// Input:       entry_p contxt:     The execution context.
//              const char *file:   File / dir.
//              LONG mask:          Protection bits
//
// Return:      int:                On success '1', else '0'. 
//----------------------------------------------------------------------------
static int h_protect_set(entry_p contxt, 
                         const char *file, 
                         LONG mask)
{
    if(contxt && file)
    {
        // On non Amiga systems this is a stub.
        #ifdef AMIGA
        if(!SetProtection(file, mask))
        {
            error(contxt->id, ERR_SET_PERM, file); 
            return 0; 
        }
        #endif

        // If logging is enabled, write to log.
        h_log(contxt, tr(S_PTCT), file, mask); 
        return 1; 
    }
    else
    {
        // Bad input.
        error(PANIC); 
        return 0; 
    }
}

#define CF_INFOS        (1 << 0)
#define CF_FONTS        (1 << 1)
#define CF_NOGAUGE      (1 << 2)
#define CF_NOFAIL       (1 << 3)
#define CF_OKNODELETE   (1 << 4)
#define CF_FORCE        (1 << 5)
#define CF_ASKUSER      (1 << 6)
#define CF_NOPOSITION   (1 << 7)
#define CF_SILENT       (1 << 8)

//----------------------------------------------------------------------------
// Name:        h_copyfile(entry_p contxt, 
//                         const char *hlp, 
//                         const char *msg,
//                         int mode)
//
// Description: Copy file. Helper used by m_copyfiles and m_copylib.
//
// Input:       entry_p contxt:     The execution context.
//              const char *src:    Source file.
//              const char *dst:    Destination file.
//              int mode:           Copy mode, see CF_*.
//
// Return:      int:                On success '1', else '0'.
//----------------------------------------------------------------------------
static int h_copyfile(entry_p contxt, 
                      const char *src, 
                      const char *dst,
                      int mode)
{
    if(contxt && src && dst)
    { 
        // Prepare GUI unless we're in silent mode
        // as used by copylib. 
        if((mode & CF_SILENT) ||
           gui_copyfiles_setcur(src, mode & CF_NOGAUGE))
        { 
            static char buf[BUFSIZ]; 
            FILE *fs = fopen(src, "r");
            size_t n = fs ? fread(buf, 1, BUFSIZ, fs) : 0; 
            int err = 
                #ifdef AMIGA
                IoErr();
                #else
                fs ? ferror(fs) : 0;
                #endif

            if(fs && !err)
            {
                // Is there an existing destination
                // file that is write protected?
                if(!access(dst, F_OK) && 
                    access(dst, W_OK))
                {
                    // No need to ask if only (force).
                    if((mode & CF_FORCE) &&
                      !(mode & CF_ASKUSER))
                    {
                        // Unprotect file.
                        chmod(dst, S_IRWXU);
                    }
                    else
                    // Ask for confirmation if (askuser).
                    if(mode & CF_ASKUSER)
                    {
                        // Unless we're running in novice mode
                        // and use (force) at the same time.
                        if((mode & CF_FORCE) ||
                            get_numvar(contxt, "@user-level"))
                        {
                            if(h_confirm(contxt, "", tr(S_OWRT), dst))
                            {
                                // Unprotect file.
                                chmod(dst, S_IRWXU);
                            }
                            else
                            {
                                // Skip file or abort.
                                fclose(fs); 
                                return did_halt() ? 0 : 1; 
                            }
                        }
                    }
                }

                // Create / overwrite file. 
                FILE *fd = fopen(dst, "w"); 

                if(fd)
                {
                    // Read and write until there is nothing more
                    // to read.
                    while(n)
                    {
                        if(fwrite(buf, 1, n, fd) != n)
                        {
                            error(contxt->id, ERR_WRITE_FILE, dst); 
                            break; 
                        }

                        n = fread(buf, 1, BUFSIZ, fs);
                    }

                    // Close input and output files.
                    fclose(fs); 
                    fclose(fd); 
                    
                    // The number of bytes read and not written 
                    // should be zero.
                    if(!n)
                    {
                        // Write to the log file (if logging is
                        // enabled). 
                        h_log(contxt, tr(S_CPYD), src, dst); 

                        // Are we going to copy the icon as well?
                        if(mode & CF_INFOS)
                        {
                            // The source icon. 
                            static char is[PATH_MAX]; 
                            snprintf(is, sizeof(is), "%s.info", src); 

                            // Only if it exists, it's not an error
                            // if it's missing.
                            if(h_exists(is) == 1)
                            {
                                static char id[PATH_MAX]; 

                                // The destination icon. 
                                snprintf(id, sizeof(id), "%s.info", dst); 

                                // Recur without info set.
                                if(h_copyfile(contxt, is, id, mode & ~CF_INFOS))
                                {
                                    // Reset icon position?
                                    if(mode & CF_NOPOSITION)
                                    {
                                        #ifdef AMIGA
                                        struct DiskObject *obj = (struct DiskObject *) 
                                            GetDiskObject(dst);

                                        if(obj)
                                        {
                                            // Reset icon position.
                                            obj->do_CurrentX = NO_ICON_POSITION;
                                            obj->do_CurrentY = NO_ICON_POSITION;

                                            // Save the changes to the .info file. 
                                            if(!PutDiskObject(dst, obj))
                                            {
                                                // We failed for some unknown reason.
                                                error(contxt->id, ERR_WRITE_FILE, id); 
                                            }

                                            FreeDiskObject(obj);
                                        }
                                        #endif
                                    }
                                }
                            }
                        }

                        // Preserve file permissions. On err,
                        // code will be set by h_protect_x().
                        LONG prm; 

                        if(h_protect_get(contxt, src, &prm))
                        {
                            h_protect_set(contxt, dst, prm);
                        }

                        // Reset error codes if necessary.
                        if(did_error())
                        {
                            if(mode & CF_NOFAIL)
                            {
                                // Forget all errors.
                                error(RESET);
                            }
                            else
                            {
                                // Fail for real.
                                return 0;
                            }
                        }

                        // We succeeded.
                        return 1; 
                    }
                }
                else
                {
                    // The source handle is open.
                    fclose(fs); 

                    if((mode & CF_NOFAIL) ||
                       (mode & CF_OKNODELETE))
                    {
                        // Ignore failure. 
                        h_log(contxt, tr(S_NCPY), src, dst); 
                        return 1;
                    }
                    else
                    {
                        // Fail for real.
                        error(contxt->id, ERR_WRITE_FILE, dst); 
                    }
                }
            }
            else
            {
                if(mode & CF_NOFAIL)
                {
                    // Ignore failure. 
                    h_log(contxt, tr(S_NCPY), src, dst); 
                    return 1;
                }
                else
                {
                    // Fail for real.
                    error(contxt->id, ERR_READ_FILE, src); 
                }
            }
        }
        else
        {
            error(HALT); 
            h_log(contxt, tr(S_ACPY), src, dst); 
        }
    }

    // Failure or user abort.
    return 0; 
}

//----------------------------------------------------------------------------
// Name:        h_makedir(entry_p contxt, 
//                        const char *dst,
//                        int mode)
//
// Description: Create directory / tree of directories.
//
// Input:       entry_p contxt:     The execution context.
//              const char *dst:    The directory.
//              int mode:           FIXME.
//
// Return:      int:                On success '1', else '0'.
//----------------------------------------------------------------------------
static int h_makedir(entry_p contxt, const char *dst, int mode)
{
    if(contxt && dst)
    {
        char *dir; 

        // Return immediately if directory exists.
        if(h_exists(dst) == 2)
        {
            // Nothing to do.
            h_log(contxt, tr(S_EDIR), dst); 
            return 1; 
        }

        // Create working copy.
        dir = STRDUP(dst); 

        if(dir)
        {
            int d = 1, 
                l = (int) strlen(dir);

            // Get directory depth.
            for(int i = 0; i < l; i++)
            {
                if(dir[i] == '/')
                {
                    d++;  
                }
            }

            // Maximum number of retries == depth.
            while(d--)
            {
                // Shrink scope until mkdir works.
                for(int i = l; i >= 0; i--)
                {
                    // Is the current char a delimiter?
                    if(dir[i] == '/' ||
                       dir[i] == '\0')
                    {
                        // Save delimiter.
                        char c = dir[i]; 

                        // Truncate string.
                        dir[i] = '\0'; 

                        // Attempt to create path.
                        if(mkdir(dir, 0777) == 0)
                        {
                            // Is this the full path?
                            if(i == l)
                            {
                                free(dir);
                                h_log(contxt, tr(S_CRTD), dst); 

                                // Yes, we're done.
                                return 1; 
                            }

                            // Not the full path, reinstate 
                            // delimiter and start all over
                            // with the full path.
                            dir[i] = c; 
                            break; 
                        }
                        else
                        {
                            // Reinstate delimiter and shrink
                            // scope.
                            dir[i] = c; 
                        }
                    }
                }
            }

            // Free working copy.
            free(dir);

            if(mode)
            {
                // FIXME
                return 1; 
            }

            // For some unknown reason, we can't 
            // create the directory.
            error(contxt->id, ERR_WRITE_DIR, dst); 
        }
        else
        {
            // Out of memory.
            error(PANIC); 
        }
    }

    // Unknown error.
    return 0; 
}

//----------------------------------------------------------------------------
// (copyfiles (prompt..) (help..) (source..) (dest..) (newname..) (choices..)
//     (all) (pattern..) (files) (infos) (confirm..) (safe) (optional
//     <option> <option> ...) (delopts <option> <option> ...) (nogauge))
//
//     copy files (and subdir's by default).  files option say NO
//     subdirectories
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_copyfiles(entry_p contxt)
{
    // We need atleast one argument
    if(c_sane(contxt, 1))
    {
        entry_p prompt     = get_opt(contxt, OPT_PROMPT),
                help       = get_opt(contxt, OPT_HELP),
                source     = get_opt(contxt, OPT_SOURCE),
                dest       = get_opt(contxt, OPT_DEST),
                newname    = get_opt(contxt, OPT_NEWNAME),
                choices    = get_opt(contxt, OPT_CHOICES),
                all        = get_opt(contxt, OPT_ALL),
                pattern    = get_opt(contxt, OPT_PATTERN),
                infos      = get_opt(contxt, OPT_INFOS),
                confirm    = get_opt(contxt, OPT_CONFIRM),
                safe       = get_opt(contxt, OPT_SAFE),
                nogauge    = get_opt(contxt, OPT_NOGAUGE),
                fonts      = get_opt(contxt, OPT_FONTS),
                optional   = get_opt(contxt, OPT_OPTIONAL),
                delopts    = get_opt(contxt, OPT_DELOPTS),
                files      = all ? NULL : get_opt(contxt, OPT_FILES),
                fail       = get_opt(delopts, OPT_FAIL) ?
                             NULL : get_opt(optional, OPT_FAIL),
                nofail     = get_opt(delopts, OPT_NOFAIL) ?
                             NULL : get_opt(optional, OPT_NOFAIL),
                oknodelete = get_opt(delopts, OPT_OKNODELETE) ?
                             NULL : get_opt(optional, OPT_OKNODELETE),
                force      = get_opt(delopts, OPT_FORCE) ? 
                             NULL : get_opt(optional, OPT_FORCE),
                askuser    = get_opt(delopts, OPT_ASKUSER) ?
                             NULL : get_opt(optional, OPT_ASKUSER);

        DNUM = 0; 

        // The (pattern) (choices) and (all) options
        // are mutually exclusive. 
        if((pattern && (choices || all)) ||
           (choices && (pattern || all)) ||
           (all && (choices || pattern)))
        {
            error(contxt->id, ERR_OPTION_MUTEX, 
                  "pattern/choices/all"); 
            RCUR; 
        }

        // The (fail) (nofail) and (oknodelete) options
        // are mutually exclusive. 
        if((fail && (nofail || oknodelete)) ||
           (nofail && (fail || oknodelete)) ||
           (oknodelete && (nofail || fail)))
        {
            error(contxt->id, ERR_OPTION_MUTEX, 
                  "fail/nofail/oknodelete"); 
            RCUR; 
        }

        // We need a source and a destination dir.
        if(source && dest) 
        {
            // Tree of source / destination tuples.
            pnode_p tree; 

            const char *src = str(source), 
                       *dst = str(dest); 

            // If the source is a directory, (all), 
            // (choices) or (pattern) must be used.
            if(h_exists(src) == 2 &&
               !all && !choices && !pattern)
            {
                error(contxt->id, ERR_MISSING_OPTION, 
                      "all/choices/pattern"); 
                RCUR; 
            }

            // A non safe operation in pretend mode 
            // always succeeds. 
            if(get_numvar(contxt, "@pretend") && !safe)
            {
                RNUM(1); 
            }

            // Does the destination already exist?
            if(h_exists(dst) == 2)
            {
                // Don't trust h_exists, this string
                // might be empty.
                size_t dln = strlen(dst); 

                // If it's not a volume, prompt for
                // confirmation before overwriting.
                if(dln && dst[dln - 1] != ':')
                {
                    if(!h_confirm(contxt, "", tr(S_ODIR), dst))
                    {
                        h_log(contxt, tr(S_ACPY), src, dst); 
                        RCUR; 
                    }
                    else
                    {
                        // Give permission so that overwriting
                        // can succeed. The proper (as in what 
                        // the source has) permissions are set
                        // later.
                        chmod(dst, S_IRWXU);
                    }
                }
            }

            // Traverse the source directory and create
            // the corresponding destination strings.
            tree = h_filetree
            (
                contxt->id, src, dst, 
                files,
                fonts,
                choices, 
                pattern
            ); 

            // Unless we ran out of memory when traversing
            // the source directory, we now have a list of
            // source -> destination tuples.
            if(tree)
            {
                int go = 0; 
                pnode_p cur = tree; 

                if(newname)
                {
                    // Replace file name if single file and 
                    // the 'newname' option is set
                    if(cur->next && cur->type == 2 &&
                       cur->next->type == 1 && !cur->next->next)
                    {
                        free(cur->next->copy); 
                        cur->next->copy = h_tackon(contxt->id, dst, str(newname)); 
                    }
                }

                // Do we need confirmation?
                if(confirm)
                {
                    // The default threshold is expert.
                    int level = get_numvar(contxt, "@user-level"),
                        th = 2;

                    // If the (confirm ...) option contains 
                    // something that can be translated into
                    // a new threshold value...
                    if(confirm->children && 
                       confirm->children[0] && 
                       confirm->children[0] != end())
                    {
                        // ...then do so.
                        th = num(confirm->children[0]);
                    }
                                    
                    // If we are below the threshold value, or
                    // user input has been short-circuited by 
                    // @yes, skip confirmation.
                    if(level < th ||
                       get_numvar(contxt, "@yes")) 
                    {
                        confirm = NULL; 
                    }

                    // Make sure that we have the prompt and
                    // help texts that we need if 'confirm'
                    // is set.
                    if(!prompt || !help)
                    {
                        cur = NULL; 
                        error(contxt->id, ERR_MISSING_OPTION, 
                              prompt ? "help" : "prompt"); 
                    }
                }

                // Initialize GUI, set up file lists, events,
                // and so on.
                if(cur)
                {
                    go = gui_copyfiles_start
                    (
                        prompt ? str(prompt) : NULL, 
                        confirm ? str(help) : NULL, 
                        cur, 
                        confirm != NULL
                    ); 
                }

                // Start copy unless user canceled / aborted.
                if(go == 1)
                {
                    // Set file copy mode.
                    int md = (infos ? CF_INFOS : 0) |
                             (fonts ? CF_FONTS : 0) |
                             (nogauge ? CF_NOGAUGE : 0) |
                             (nofail ? CF_NOFAIL : 0) |
                             (oknodelete ? CF_OKNODELETE : 0) |
                             (force ? CF_FORCE : 0) |
                             (askuser ? CF_ASKUSER : 0);

                    DNUM = 1; 

                    // For all files / dirs in list, copy / create.
                    for(; cur && DNUM; 
                        cur = cur->next)
                    {
                        LONG prm; 

                        // Copy file / create dir / skip if zero:ed
                        switch(cur->type)
                        {
                            case 0: 
                                continue; 

                            case 1: 
                                DNUM = h_copyfile
                                (
                                    contxt, 
                                    cur->name, 
                                    cur->copy, 
                                    md
                                ); 
                                break; 

                            case 2: 
                                // Make directory and make sure the protection
                                // bits the new one matches the old.
                                if(h_makedir(contxt, cur->copy, md) &&
                                   h_protect_get(contxt, cur->name, &prm) &&
                                   h_protect_set(contxt, cur->copy, prm))
                                {
                                    DNUM = 1; 
                                }
                                else
                                {
                                    DNUM = 0; 
                                }
                                break; 

                            default: 
                                error(PANIC); 
                                DNUM = 0; 
                        }
                    }

                    // GUI teardown.
                    gui_copyfiles_end(); 
                }
                else
                {
                    //  '0' == skip.
                    // '-1' == halt.
                    if(go == -1)
                    {
                        error(HALT); 
                    }
                }

                cur = tree; 

                // Free list of files / dirs.
                while(cur)
                {
                    tree = cur; 
                    free(cur->name); 
                    free(cur->copy); 
                    cur = cur->next; 
                    free(tree); 
                }
            }
        }
        else
        {
            error(contxt->id, ERR_MISSING_OPTION, 
                  !source ? "source" : "dest"); 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC);
    }

    // We don't know if we're successsful, 
    // at this point, return what we have. 
    RCUR; 
}

//----------------------------------------------------------------------------
// (copylib (prompt..) (help..) (source..) (dest..) (newname..) (infos)
//     (confirm) (safe) (optional <option> <option> ...) (delopts <option>
//     <option> ...) (nogauge))
//
//     install a library if newer version
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_copylib(entry_p contxt)
{
    // We need atleast one argument
    if(c_sane(contxt, 1))
    {   
        entry_p prompt     = get_opt(contxt, OPT_PROMPT),
                help       = get_opt(contxt, OPT_HELP),
                source     = get_opt(contxt, OPT_SOURCE),
                dest       = get_opt(contxt, OPT_DEST),
                newname    = get_opt(contxt, OPT_NEWNAME),
                infos      = get_opt(contxt, OPT_INFOS),
                confirm    = get_opt(contxt, OPT_CONFIRM),
                safe       = get_opt(contxt, OPT_SAFE),
                nogauge    = get_opt(contxt, OPT_NOGAUGE),
                noposition = get_opt(contxt, OPT_NOPOSITION),
                optional   = get_opt(contxt, OPT_OPTIONAL),
                delopts    = get_opt(contxt, OPT_DELOPTS),
                fail       = get_opt(delopts, OPT_FAIL) ?
                             NULL : get_opt(optional, OPT_FAIL),
                nofail     = get_opt(delopts, OPT_NOFAIL) ?
                             NULL : get_opt(optional, OPT_NOFAIL),
                oknodelete = get_opt(delopts, OPT_OKNODELETE) ?
                             NULL : get_opt(optional, OPT_OKNODELETE),
                force      = get_opt(delopts, OPT_FORCE) ? 
                             NULL : get_opt(optional, OPT_FORCE),
                askuser    = get_opt(delopts, OPT_ASKUSER) ?
                             NULL : get_opt(optional, OPT_ASKUSER);

        DNUM = 0; 

        // The (fail) (nofail) and (oknodelete) options
        // are mutually exclusive. 
        if((fail && (nofail || oknodelete)) ||
           (nofail && (fail || oknodelete)) ||
           (oknodelete && (nofail || fail)))
        {
            error(contxt->id, ERR_OPTION_MUTEX, 
                  "fail/nofail/oknodelete"); 
            RCUR; 
        }

        // We always need a prompt and help since trying
        // to overwrite new files with older ones will 
        // force a confirm if we're in expert mode. The
        // CBM installer doesn't behave this way, but the 
        // Installer guide says so, and it makes sense, so
        // let's do it this way until we know for sure that
        // this will break existing scripts.
        if(source && dest && 
           prompt && help) 
        {
            const char *s = str(source), 
                       *d = str(dest); 

            // Does the source file exist?
            if(h_exists(s) == 1)
            {
                // User level.
                int level = get_numvar(contxt, "@user-level"),
                // Source file version.
                vs = h_getversion(contxt, s),
                // Destination type.
                dt = h_exists(d);
                // Destination file. 
                char *f; 

                // A non safe operation in pretend mode
                // always succeeds. 
                if(get_numvar(contxt, "@pretend") && !safe)
                {
                    RNUM(1); 
                }

                if(vs < 0)
                {
                    // Could not find version string.
                    error(contxt->id, ERR_NO_VERSION, s); 
                    RCUR;
                }

                if(dt == 1)
                {
                    // Destination is a file, not a dir. 
                    error(contxt->id, ERR_NOT_A_DIR, d); 
                    RCUR;
                }

                // Do we need confirmation?
                if(confirm)
                {
                    // The default threshold is expert.
                    int th = 2; 

                    // If the (confirm ...) option contains 
                    // something that can be translated into
                    // a new threshold value...
                    if(confirm->children && 
                       confirm->children[0] && 
                       confirm->children[0] != end())
                    {
                        // ...then do so.
                        th = num(confirm->children[0]);
                    }
                                    
                    // If we are below the threshold value, or
                    // user input has been short-circuited by 
                    // @yes, skip confirmation.
                    if(level < th ||
                       get_numvar(contxt, "@yes")) 
                    {
                        confirm = NULL; 
                    }
                }

                if(!dt)
                {
                    // Directory doesn't exist, create
                    // it. One level deep only. 
                    if(!mkdir(d, 0777))
                    {
                        // Log the success. 
                        h_log(contxt, tr(S_CRTD), d); 
                    }
                    else
                    {
                        // Permission problems or the dir is
                        // more than 1 level deeper than the
                        // existing path. 
                        error(contxt->id, ERR_WRITE_DIR, d); 
                        RCUR;
                    }
                }

                // Are we renaming the file?
                if(newname)
                {
                    // Yes, append the new name to the path.
                    f = h_tackon(contxt->id, d, str(newname));
                }
                else
                {
                    // No, append the file part of the (possibly)
                    // full source path to the destination path. 
                    f = h_tackon(contxt->id, d, h_fileonly(contxt->id, s));
                }

                // If we're not out of memory and destination dir
                // and / or destination file is non empty, f will
                // be <> 0. 
                if(f) 
                {
                    // Get f type info and set copy mode.
                    int ft = h_exists(f),
                        md = CF_SILENT |
                             (infos ? CF_INFOS : 0) |
                             (noposition ? CF_NOPOSITION : 0) |
                             (nogauge ? CF_NOGAUGE : 0) |
                             (nofail ? CF_NOFAIL : 0) |
                             (oknodelete ? CF_OKNODELETE : 0) |
                             (force ? CF_FORCE : 0) |
                             (askuser ? CF_ASKUSER : 0);

                    // Does it exist?
                    if(!ft)
                    {
                        // No such file, copy source to the 
                        // destination directory. If needed
                        // get confirmation.
                        if(!confirm || 
                            h_confirm
                            (
                                contxt, 
                                "", 
                                "%s\n\n%s: %d.%d\n%s\n\n%s: %s",
                                str(prompt), 
                                tr(S_VINS),
                                vs >> 16,
                                vs & 0xffff,
                                tr(S_NINS),
                                tr(S_DDRW),
                                d 
                            ))
                        {
                            DNUM = h_copyfile(contxt, s, f, md);
                        }
                    }
                    else
                    // It's a file.
                    if(ft == 1)
                    {
                        // Get version of existing file.
                        int vf = h_getversion(contxt, f);
                        
                        // Did we find a version not equal to 
                        // that of the current file?
                        if(vf >= 0)
                        {
                            if(vs != vf)
                            {
                                // If we ask for confirmation and get it, copy
                                // the file no matter what version it (and the
                                // existing destination file) has.
                                if(confirm)
                                {
                                    if(h_confirm(
                                        contxt,
                                        "", 
                                        "%s\n\n%s: %d.%d\n%s: %d.%d\n\n%s: %s",
                                        str(prompt), 
                                        tr(S_VINS),
                                        vs >> 16,
                                        vs & 0xffff,
                                        tr(S_VCUR),
                                        vf >> 16,
                                        vf & 0xffff,
                                        tr(S_DDRW),
                                        d))
                                    {
                                        DNUM = h_copyfile(contxt, s, f, md);
                                    }
                                }
                                else
                                {
                                    // If the file to be copied has a higher version
                                    // number than the existing one, overwrite. 
                                    if(vs > vf)
                                    {
                                        DNUM = h_copyfile(contxt, s, f, md);
                                    }
                                    else
                                    {
                                        // If the file to be copied has a lower version
                                        // number than the existing one, and we're in
                                        // expert mode, ask the user to confirm. If we
                                        // get a confirmation, overwrite.
                                        if(level == 2)
                                        {
                                            if(h_confirm(
                                                contxt,
                                                "", 
                                                "%s\n\n%s: %d.%d\n%s: %d.%d\n\n%s: %s",
                                                str(prompt), 
                                                tr(S_VINS),
                                                vs >> 16,
                                                vs & 0xffff,
                                                tr(S_VCUR),
                                                vf >> 16,
                                                vf & 0xffff,
                                                tr(S_DDRW),
                                                d))
                                            {
                                                DNUM = h_copyfile(contxt, s, f, md);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            // Could not find version string.
                            error(contxt->id, ERR_NO_VERSION, f); 
                        }
                    }
                    // It's a dir. 
                    else
                    {
                        // Dest file exists, but is a directory. 
                        error(contxt->id, ERR_NOT_A_FILE, f); 
                    }

                    // Free memory allocated by h_tackon.
                    free(f); 
                }
            }
            else
            {
                // No file, doesn't exist or is dir. 
                error(contxt->id, ERR_NOT_A_FILE, s); 
            }
        }
        else
        {
            error
            (
                contxt->id, ERR_MISSING_OPTION, 
                !source ? "source" : 
                !dest ? "dest" :
                !prompt ? "prompt" : "help"
            ); 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC);
    }

    // Success or failure.
    RCUR; 
}

//----------------------------------------------------------------------------
// Name:        h_delete_file(entry_p contxt, 
//                            const char *file)
//
// Description: Delete file. Helper used by m_delete.
//
// Input:       entry_p contxt:     The execution context.
//              const char *file:   File to delete.
//
// Return:      int:                On success '1', else '0'.
//----------------------------------------------------------------------------
static int h_delete_file(entry_p contxt, const char *file)
{
    if(file)
    {
        entry_p infos    = get_opt(CARG(2), OPT_INFOS), 
                optional = get_opt(CARG(2), OPT_OPTIONAL), 
                delopts  = get_opt(CARG(2), OPT_DELOPTS), 
                force    = get_opt(delopts, OPT_FORCE) ? NULL :
                           get_opt(optional, OPT_FORCE),
                askuser  = get_opt(delopts, OPT_ASKUSER) ? NULL :
                           get_opt(optional, OPT_ASKUSER);

        // If (force) is used, give permissions 
        // so that delete can succeed. 
        if(force)
        {
            // No need to bother with the return
            // value since errors will be caught
            // below.
            chmod(file, S_IRWXU);
        }
        else
        {
            if(access(file, W_OK))
            {
                // Do we need to ask for confirmation?
                if(askuser)
                {
                    // Only ask for confirmation if we're not
                    // running in novice mode. 
                    if(get_numvar(contxt, "@user-level") &&
                       h_confirm(contxt, "", tr(S_DWRT), file))
                    {
                        // Give permissions so that delete
                        // can succeed. No need to bother with
                        // the return value since errors will
                        // be caught below.
                        chmod(file, S_IRWXU);
                    }
                    else
                    {
                        // Halt will be set by h_confirm. Skip
                        // will result in nothing.
                        return 0;
                    }
                }
                else
                {
                    // Fail silently just like the original.
                    return 0;
                }
            }
        }

        // If yes, this must succeed, otherwise we
        // will abort with an error. 
        if(!remove(file))
        {
            // The file has been deleted. 
            h_log(contxt, tr(S_DLTD), file); 

            // Shall we delete the info file as well? 
            if(infos)
            {
                // Info = file + .info. 
                char *info = get_buf(); 
                snprintf(info, buf_size(), "%s.info", file); 

                if(h_exists(info) == 1)
                {
                    // Set permissions so that delete can
                    // succeed. 
                    chmod(info, S_IRWXU);

                    // Delete the info file.
                    if(!remove(info))
                    {
                        // The info file has been deleted. 
                        h_log(contxt, tr(S_DLTD), info); 
                    }
                    else
                    {
                        error(contxt->id, ERR_DELETE_FILE, info); 
                        return 0;
                    }
                }
            }

            // All done.
            return 1;
        }
        else
        {
            error(contxt->id, ERR_DELETE_FILE, file); 
            return 0;
        }
    }
    else
    {
        // Unknown error.
        error(PANIC); 
        return 0;
    }
}

//----------------------------------------------------------------------------
// Name:        h_delete_dir(entry_p contxt, 
//                           const char *dir)
//
// Description: Delete directory. Helper used by m_delete.
//
// Input:       entry_p contxt:     The execution context.
//              const char *dir:    Directory to delete.
//
// Return:      int:                On success '1', else '0'.
//----------------------------------------------------------------------------
static int h_delete_dir(entry_p contxt, const char *dir)
{
    if(dir)
    {
        entry_p infos    = get_opt(CARG(2), OPT_INFOS), 
                optional = get_opt(CARG(2), OPT_OPTIONAL), 
                delopts  = get_opt(CARG(2), OPT_DELOPTS), 
                all      = get_opt(CARG(2), OPT_ALL), 
                force    = get_opt(delopts, OPT_FORCE) ? NULL :
                           get_opt(optional, OPT_FORCE),
                askuser  = get_opt(delopts, OPT_ASKUSER) ? NULL :
                           get_opt(optional, OPT_ASKUSER);

        if(!force && access(dir, W_OK))
        {
            // Do we need to ask for confirmation?
            if(askuser)
            {

                // Only ask for confirmation if we're not
                // running in novice mode. 
                if(!get_numvar(contxt, "@user-level") ||
                   !h_confirm(contxt, "", tr(S_DWRD), dir))
                {
                    // Halt will be set by h_confirm. Skip
                    // will result in nothing.
                    return 0;
                }
            }
            else
            {
                // Exit silently.
                return 0;
            }
        }

        // Give permissions so that delete can succeed. 
        // No need to bother with the return value since
        // errors will be caught below.
        chmod(dir, S_IRWXU);

        if(rmdir(dir))
        {
            if(all)
            {
                DIR *d = opendir(dir);  

                // Permission to read? 
                if(d) 
                {
                    char *w; 
                    struct dirent *e = readdir(d); 

                    // Find all files in the directory.
                    while(e)
                    {
                        // Create full path.
                        w = h_tackon(contxt->id, dir, e->d_name); 

                        // Is it a file? 
                        if(w && h_exists(w) == 1)
                        {
                            // Delete it.
                            h_delete_file(contxt, w); 
                        }

                        // Free full path.
                        free(w); 

                        // Get next entry. 
                        e = readdir(d); 
                    }

                    // Restart from the beginning.
                    rewinddir(d); 
                    e = readdir(d); 

                    // Find all subdirectories in the
                    // directory.
                    while(e)
                    {
                        #ifndef AMIGA
                        // Filter out the magic on non-Amigas.
                        if(strcmp(e->d_name, ".") &&
                           strcmp(e->d_name, ".."))
                        #endif
                        {
                            // Create full path.
                            w = h_tackon(contxt->id, dir, e->d_name); 

                            // Is it a directory? 
                            if(w && h_exists(w) == 2)
                            {
                                // Recur into subdirectory.
                                h_delete_dir(contxt, w); 
                            }

                            // Free full path.
                            free(w); 
                        }

                        // Get next entry. 
                        e = readdir(d); 
                    }

                    // Close the (by now, hopefully) empty dir.
                    closedir(d); 
                }

                if(rmdir(dir))
                {
                    // Fail silently.
                    return 0;
                }
            }
            else
            {
                // Fail silently.
                return 0;
            }
        }

        // Shall we delete the info file as well? 
        if(infos)
        {
            // Info = file + .info. 
            char *info = get_buf(); 
            snprintf(info, buf_size(), "%s.info", dir); 

            if(h_exists(info) == 1)
            {
                // Set permissions so that delete can
                // succeed. 
                chmod(info, S_IRWXU);

                // Delete the info file.
                if(!remove(info))
                {
                    // The info file has been deleted. 
                    h_log(contxt, tr(S_DLTD), info); 
                }
                else
                {
                    // Fail silently.
                    return 0;
                }
            }
        }

        // Done.
        return 1;
    }
    else
    {
        // Bad input.
        error(PANIC); 
    }

    // Fail silently.
    return 0;
}

//----------------------------------------------------------------------------
// Name:        h_delete_pattern(entry_p contxt, 
//                               const char *pat)
//
// Description: Delete file / dir matching pattern. Helper used by m_delete.
//
// Input:       entry_p contxt:     The execution context.
//              const char *pat:    Pattern.
//
// Return:      int:                On success '1', else '0'.
//----------------------------------------------------------------------------
static int h_delete_pattern(entry_p contxt, const char *pat)
{
    if(contxt && pat)
    {
        // Pattern matching is only done on Amiga like OS:es
        #ifdef AMIGA
        struct AnchorPath *ap = 
            CALLOC(1, sizeof(struct AnchorPath) + PATH_MAX); 

        if(ap)
        {
            int err; 
            ap->ap_Strlen = PATH_MAX; 

            // For all matches, invoke the appropriate
            // function for deletion.
            for(err = MatchFirst(pat, ap); !err;
                err = MatchNext(ap))
            {
                // ST_FILE         -3
                // ST_LINKFILE     -4
                if(ap->ap_Info.fib_DirEntryType < 0)
                {
                    if(!h_delete_file(contxt, ap->ap_Buf))
                    {
                        // Break out on trouble / user
                        // abort.
                        break;
                    }
                }
                else
                // ST_ROOT          1
                // ST_USERDIR       2
                // ST_SOFTLINK      3 
                // ST_LINKDIR       4
                if(ap->ap_Info.fib_DirEntryType > 0)
                {
                    if(!h_delete_dir(contxt, ap->ap_Buf))
                    {
                        // Break out on trouble / user
                        // abort.
                        break;
                    }
                }
            }

            // Free all resources.
            MatchEnd(ap);
            free(ap); 

            // Is there nothing left?
            if(err == ERROR_NO_MORE_ENTRIES)
            {
                // Done.
                return 1;
            }
            else
            {
                // Please note that 'breaks' will take us
                // here, so will MatchFirst / MatchNext()
                // problems.
                error(contxt->id, ERR_DELETE_FILE, pat); 
                return 0;
            }
        }
        #else
        // On non Amiga systems this is a stub.
        return 1;
        #endif
    }
    else
    { 
        // Bad input.
        error(PANIC); 
        return 0;
    }
}

//----------------------------------------------------------------------------
// (delete file (help..) (prompt..) (confirm..) 
//     (infos) (optional <option> <option> ...) (all)
//     (delopts <option> <option> ...) (safe))
//
//     delete file
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_delete(entry_p contxt)
{
    // We need atleast one argument
    if(c_sane(contxt, 1))
    {
        int wc = 0; 
        const char *w = str(CARG(1));

        #ifdef AMIGA
        wc = ParsePattern(w, get_buf(), buf_size());
        #endif

        // Assume failure.
        DNUM = 0; 

        // Can we parse the input string? 
        if(wc >= 0)
        {
            entry_p help     = get_opt(CARG(2), OPT_HELP),
                    prompt   = get_opt(CARG(2), OPT_PROMPT), 
                    confirm  = get_opt(CARG(2), OPT_CONFIRM), 
                    safe     = get_opt(CARG(2), OPT_SAFE);

            // Do we need confirmation?
            if(confirm)
            {
                // The default threshold is expert.
                int level = get_numvar(contxt, "@user-level"),
                    th = 2;

                // If the (confirm ...) option contains 
                // something that can be translated into
                // a new threshold value...
                if(confirm->children && 
                   confirm->children[0] && 
                   confirm->children[0] != end())
                {
                    // ...then do so.
                    th = num(confirm->children[0]);
                }
                                
                // If we are below the threshold value,
                // don't care about getting confirmation
                // from the user.
                if(level < th) 
                {
                    confirm = NULL; 
                }

                // Make sure that we have the prompt and
                // help texts that we need if 'confirm'
                // is set.
                if(!prompt || !help)
                {
                    error(contxt->id, ERR_MISSING_OPTION, 
                          prompt ? "help" : "prompt"); 
                    RCUR; 
                }
            }

            // If we need confirmation and the user skips
            // or aborts, return. On abort, the HALT will
            // be set by h_confirm. 
            if(confirm && 
               !h_confirm(contxt, str(help), str(prompt)))
            {
                RCUR; 
            }

            // Is this a safe operation or are we not 
            // running in pretend mode? 
            if(safe || !get_numvar(contxt, "@pretend"))
            {
                // Did the input string contain any 
                // wildcards?
                if(wc)        
                {
                    // Delete everything matching the
                    // wildcard pattern.
                    DNUM = h_delete_pattern(contxt, w); 
                }
                else
                // No wildcards, delete directory, file
                // or something that doesn't exist.
                {
                    switch(h_exists(w))
                    {
                        case 2:
                            DNUM = h_delete_dir(contxt, w); 
                            break;

                        case 1:
                            DNUM = h_delete_file(contxt, w); 
                            break;

                        case 0:
                            h_log(contxt, tr(S_NSFL), w); 
                            DNUM = 0;
                    }
                }
            }
            else
            {
                // A non safe operation in pretend
                // mode always succeeds. 
                DNUM = 1; 
            }
        }
        else
        {
            // We probably had a buffer overflow. 
            error(contxt->id, ERR_OVERFLOW, w); 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC); 
    }

    RCUR; 
}

//----------------------------------------------------------------------------
// (exists <filename> (noreq))
//     0 if no, 1 if file, 2 if dir
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_exists(entry_p contxt)
{
    if(c_sane(contxt, 1))
    {
        // Non-existance. 
        int r = 0; 

        // Supress volume requester?
        if(get_opt(contxt, OPT_NOREQ))
        {
            #ifdef AMIGA
            struct Process *p = (struct Process *) 
                FindTask(NULL);

            // Save the current window ptr. 
            APTR w = p->pr_WindowPtr;

            // Disable auto request. 
            p->pr_WindowPtr = (APTR) -1L; 
            #endif

            // Get type (file / dir / 0)
            r = h_exists(str(CARG(1))); 

            #ifdef AMIGA
            // Restore auto request. 
            p->pr_WindowPtr = w; 
            #endif
        }
        else
        {
            // Get type (file / dir / 0)
            r = h_exists(str(CARG(1))); 
        }

        RNUM(r); 
    }
    else
    {
        // The parser is broken
        error(PANIC);
        RCUR; 
    }
}

//----------------------------------------------------------------------------
// (fileonly <path>)
//     return file part of path (see pathonly)
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_fileonly(entry_p contxt)
{
    if(c_sane(contxt, 1))
    {
        const char *p = str(CARG(1)), 
                   *f = h_fileonly(contxt->id, p);

        RSTR(STRDUP(f));
    }
    else
    {
        // The parser is broken
        error(PANIC); 
        REST; 
    }
}

//----------------------------------------------------------------------------
// (foreach <dir> <pattern> <statements>)
//     do for entries in directory
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_foreach(entry_p contxt)
{
    if(contxt || c_sane(contxt, 3))
    {
        // Pattern matching is not done
        // on non-Amiga systems.
        #ifdef AMIGA
        const char *pt = str(CARG(2)); 
        #endif

        // Open dir and assume failure. 
        const char *dn = str(CARG(1)); 
        DIR *dir = opendir(dn);  
        pnode_p top = NULL;
        int err = 1; 

        // Permission to read? 
        if(dir) 
        {
            static char cwd[PATH_MAX];
            struct dirent *ent = readdir(dir); 

            // Save the current working directory. 
            if(getcwd(cwd, PATH_MAX) == cwd)
            {
                // Enter the directory <drawer name>
                if(!chdir(dn))
                {
                    // Allocate memory for the start node. 
                    pnode_p cur; 
                    top = CALLOC(1, sizeof(struct pnode_t)); 

                    #ifdef AMIGA
                    // This file info block will be used for all files
                    // and subdirs in the directory. 
                    struct FileInfoBlock *fib = (struct FileInfoBlock *) 
                           AllocDosObject(DOS_FIB, NULL); 

                    // Use cur as a gatekeeper. If set to NULL, nothing
                    // will take place except clean ups. 
                    cur = fib ? top : NULL; 
                    #else
                    // No pattern matching = no fib.
                    cur = top; 
                    #endif

                    // The dir might be empty but that's not an 
                    // error. 
                    err = 0; 

                    // Iterate over all entries in dir.
                    while(ent && cur && !err)
                    {
                        // Name of file / dir. 
                        const char *fn = ent->d_name; 

                        #ifndef AMIGA 
                        // Filter out the magic on non-Amigas.
                        if(strcmp(fn, ".") &&
                           strcmp(fn, ".."))
                        #endif
                        {
                            #ifdef AMIGA
                            // The dir is not empty, we should be able
                            // to go all the way here. Assume failure.
                            err = 1; 

                            // Lock and get the information we need
                            // from the current entry
                            BPTR lock = (BPTR) Lock(fn, ACCESS_READ);

                            if(lock)
                            {
                                if(Examine(lock, fib))
                                {
                                    // Please note that if we fail to 
                                    // lock and examine cur->name will
                                    // be NULL.
                                    cur->type = fib->fib_DirEntryType;
                                    cur->name = STRDUP(fn); 

                                    // We're probably good. PANIC:s will 
                                    // be caught further down. 
                                    err = 0; 
                                }

                                UnLock(lock); 
                            }
                            #else
                            cur->type = h_exists(fn);
                            cur->name = STRDUP(fn); 
                            #endif

                            // An empty name indicates a PANIC only if
                            // we didn't have any locking problems. 
                            if(!cur->name)
                            {
                                // Setting cur to NULL will generate a
                                // PANIC further down. 
                                if(!err)
                                {
                                    cur = NULL;
                                }

                                // Always bail out. 
                                break;
                            }
                        }

                        // Get next entry. 
                        ent = readdir(dir); 

                        // We need to check for cur->name here, or else the
                        // the filtering of '.' and '..' would not work, we 
                        // would get entries without names. 
                        if(ent && cur->name)
                        {
                            cur->next = CALLOC(1, sizeof(struct pnode_t));
                            cur = cur->next;
                        }
                    }

                    #ifdef AMIGA
                    // If dir is empty, fib will be NULL. 
                    if(fib)
                    {
                        FreeDosObject(DOS_FIB, fib); 
                    }
                    #endif

                    if(!cur)
                    {
                        // Out of memory.
                        error(PANIC); 
                        err = 1;
                    }

                    // Go back the directory where
                    // we started. 
                    chdir(cwd); 
                }
            }

            // Done. 
            closedir(dir); 
        }
        
        if(err)
        {
            // We could end up here for a number
            // of reasons. More details perhaps?
            error(contxt->id, ERR_READ_DIR, dn); 
        }

        // Iterate over the entire list of files / dirs and
        // free entries one by one. If the pattern matches 
        // the current entry, set variables and execute what
        // we have in argument 3. 
        while(top)
        {
            int skip = err; 
            pnode_p old = top; 

            // 'Export' name and type info if we have a name. 
            if(top->name)
            {
                #ifdef AMIGA
                if(!err)
                {
                    static char buf[BUFSIZ]; 

                    // Parse pattern. 
                    switch(ParsePattern(pt, buf, sizeof(buf)))
                    {
                        // If we have any wildcards, try to match. 
                        case 1: 
                            skip = MatchPattern(buf, top->name) ? 0 : 1;
                            break;

                        // If no wildcards, do a simple string comparsion.
                        case 0:
                            skip = strcmp(top->name, pt); 
                            break; 

                        // We probably had a buffer overflow. 
                        default:
                            error(contxt->id, ERR_OVERFLOW, pt); 
                            err = 1; 
                    }
                }
                #endif

                // We always export, for memory management reasons. 
                set_numvar(contxt, "@each-type", (int) top->type); 
                set_strvar(contxt, "@each-name", top->name); 

                if(!skip)
                {
                    // Execute the code contained in
                    // the third argument. 
                    invoke(CARG(3)); 
                }
            }

            // Free current entry and proceed with the next. 
            top = top->next; 
            free(old); 
        }
        
        RNUM
        (
            err ? 0 : 1
        ); 
    }
    else
    {
        // The parser is broken
        error(PANIC); 
        RNUM(0); 
    }
}

//----------------------------------------------------------------------------
// (makeassign <assign> [<path>] (safe)) ; note: <assign> doesn't need `:''
//     create an assignment
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_makeassign(entry_p contxt)
{
    if(c_sane(contxt, 1))
    {
        entry_p safe = get_opt(contxt, OPT_SAFE);

        // Is this a safe operation or are we not 
        // running in pretend mode? 
        if(safe || !get_numvar(contxt, "@pretend"))
        {
            // The name of the assign. 
            char *asn = str(CARG(1)); 

            // Assume failure..
            DNUM = 0; 

            // Are we going to create an assign?
            if(CARG(2) && CARG(2) != end() &&
               CARG(2)->type != OPTION)
            { 
                // The destination. 
                char *dst = str(CARG(2)); 

                #ifdef AMIGA
                BPTR lock = (BPTR) Lock(dst, ACCESS_READ);
                if(lock)
                {
                    // Create the assign. After this,
                    // the lock will be owned by the 
                    // system, do not unlock or use.
                    DNUM = AssignLock(asn, lock) ? 1 : 0; 
                }
                #else
                DNUM = 1;
                #endif
                
                // Log the outcome. 
                h_log
                (
                    contxt, 
                    DNUM ? tr(S_ACRT) : tr(S_ACRE),
                    asn, 
                    dst
                ); 
            }
            else
            {
                #ifdef AMIGA
                // Remove assign.
                DNUM = AssignLock(str(CARG(1)), NULL) ? 1 : 0; 
                #else
                DNUM = 2;
                #endif

                // Log the outcome. 
                h_log
                (
                    contxt, 
                    DNUM ? tr(S_ADEL) : tr(S_ADLE),
                    asn
                ); 
            }

            if(!DNUM)
            {
                // Could not create / rm assign / get lock. 
                error(contxt->id, ERR_ASSIGN, str(CARG(1))); 
            }
        }
        else
        {
            // Pretend. 
            DNUM = 1; 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC); 
    }

    // Return what we have. Could
    // be a failure or a success.
    RCUR; 
}

//----------------------------------------------------------------------------
// (makedir <name> (prompt..) (help..) (infos) (confirm..) (safe))
//     make a directory
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_makedir(entry_p contxt)
{
    // We need atleast one argument, the name
    // of the directory to be created.
    if(c_sane(contxt, 1))
    {
        entry_p prompt   = get_opt(CARG(2), OPT_PROMPT),
                help     = get_opt(CARG(2), OPT_HELP),
                infos    = get_opt(CARG(2), OPT_INFOS),
                confirm  = get_opt(CARG(2), OPT_CONFIRM),
                safe     = get_opt(CARG(2), OPT_SAFE); 

        DNUM = 0; 

        // Do we need confirmation?
        if(confirm)
        {
            // The default threshold is expert.
            int level = get_numvar(contxt, "@user-level"),
                th = 2;

            // If the (confirm ...) option contains 
            // something that can be translated into
            // a new threshold value...
            if(confirm->children && 
               confirm->children[0] && 
               confirm->children[0] != end())
            {
                // ...then do so.
                th = num(confirm->children[0]);
            }
                            
            // If we are below the threshold value,
            // don't care about getting confirmation
            // from the user.
            if(level < th) 
            {
                confirm = NULL; 
            }

            // Make sure that we have the prompt and
            // help texts that we need if 'confirm'
            // is set.
            if(!prompt || !help)
            {
                error(contxt->id, ERR_MISSING_OPTION, 
                      prompt ? "help" : "prompt"); 
                RCUR; 
            }
        }

        // If we need confirmation and the user skips
        // or aborts, return. On abort, the HALT will
        // be set by h_confirm. 
        if(confirm && 
           !h_confirm(contxt, str(help), str(prompt)))
        {
            RCUR; 
        }

        // Is this a safe operation or are we not 
        // running in pretend mode? 
        if(safe || !get_numvar(contxt, "@pretend"))
        {
            // The name of the directory. 
            const char *dn = str(CARG(1)); 

            // Create the directory. 
            DNUM = h_makedir(contxt, dn, 0 /* FIXME */); 

            // Are we supposed to create an icon
            // as well? 
            if(infos && DNUM)
            {
                #ifdef AMIGA
                // Get the default drawer icon from the OS. 
                struct DiskObject *obj = (struct DiskObject *) 
                    GetDefDiskObject(WBDRAWER);

                // Assume failure. 
                DNUM = 0; 

                // If we have a default icon, let our newly
                // created directory have it.
                if(obj)
                {
                    // Create new .info.
                    if(PutDiskObject(dn, obj))
                    {
                        // Done. 
                        DNUM = 1; 
                    }

                    // Free def. icon. 
                    FreeDiskObject(obj);
                }

                // If any of the above failed, ioerr 
                // will be set. Export that value as
                // a variable.
                if(!DNUM)
                {
                    LONG ioe = IoErr(); 
                    set_numvar(contxt, "@ioerr", ioe); 
                }
                #endif
            }
        }
        else
        {
            // A non safe operation in pretend
            // mode always succeeds. 
            DNUM = 1; 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC); 
    }
    
    // Success / failure. 
    RCUR;
}

//----------------------------------------------------------------------------
// (protect <file> [<string of flags to change>] [<dec. mask>] <parameters>)
//     get/set file protection flags
//
// 8 7 6 5 4 3 2 1  <- Bit number
// h s p a r w e d  <- corresponding protection flag
// ^ ^ ^ ^ ^ ^ ^ ^
// | | | | | | | |
// | | | | | | | +- | 
// | | | | | | +--- | 0 = flag set
// | | | | | +----- | 1 = flag clear
// | | | | +------- | 
// | | | |
// | | | |
// | | | +--------- | 
// | | +----------- | 0 = flag clear
// | +------------- | 1 = flag set
// +--------------- | 
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_protect(entry_p contxt)
{
    // A single argument is all we need. 
    if(c_sane(contxt, 1))
    {
        LONG r = 0; 
        const char *file = str(CARG(1)); 

        if(CARG(2) && CARG(2) != end())
        {
            // Get with option.
            if(CARG(2)->type == CONTXT)
            {
                entry_p override = get_opt(CARG(2), OPT_OVERRIDE);

                if(override)
                {
                    // From user (script). 
                    r = num(override); 
                }
                else
                {
                    // From file. 
                    h_protect_get(contxt, file, &r);
                }
            }
            // Set value. 
            else
            {
                // Assume string operation. 
                const char *flg = str(CARG(2)); 
                size_t len = strlen(flg); 

                entry_p safe = get_opt(CARG(3), OPT_SAFE),
                        override = get_opt(CARG(3), OPT_OVERRIDE);

                // If any numbers are present in the string, 
                // treat it as an absolute mask instead.
                for(size_t i = 0; i < len; i++)
                {
                    if(flg[i] < 58 &&
                       flg[i] > 47)
                    {
                        // We no longer have a string.
                        len = 0; 
                        break; 
                    }
                }
                
                // Do we have a string? 
                if(len)
                {
                    // Mode (+/-). 
                    int m = 0; 

                    // Use a real file or override? 
                    if(!override)
                    {
                        // Get flags from file. 
                        if(!h_protect_get(contxt, file, &r))
                        {
                            // Helper will set proper error
                            RNUM(-1); 
                        }
                    }
                    else
                    {
                        // Get flags from user (script). 
                        r = num(override); 
                    }
                    
                    // Invert 1-4. 
                    r ^= 0x0f;

                    // For all flags. 
                    for(size_t i = 0; i < len; i++)
                    {
                        // Mask.
                        int b = 0; 

                        // Which protection bit?
                        switch(flg[i])
                        {
                            case '+': m = 1; break; 
                            case '-': m = 2; break; 
                            case 'h': b = 1 << 7; break; 
                            case 's': b = 1 << 6; break; 
                            case 'p': b = 1 << 5; break; 
                            case 'a': b = 1 << 4; break; 
                            case 'r': b = 1 << 3; break; 
                            case 'w': b = 1 << 2; break; 
                            case 'e': b = 1 << 1; break; 
                            case 'd': b = 1 << 0; break; 
                        }

                        // Adding or subtracting?
                        switch(m)
                        {
                            case 0: r = b; m = 1; break; 
                            case 1: r |= b; break; 
                            case 2: r &= ~b; break; 
                        }
                    }

                    // Invert 1-4. 
                    r ^= 0x0f;
                }
                else
                {
                    // Use an absolute mask. 
                    r = num(CARG(2)); 
                }

                if(!override)
                {
                    // Is this a safe operation or are we not 
                    // running in pretend mode? 
                    if(safe || !get_numvar(contxt, "@pretend"))
                    {
                        // Helper will set error on failure.
                        r = h_protect_set(contxt, file, r);
                    }
                    else
                    {
                        // A non safe operation in pretend
                        // mode always succeeds. 
                        r = 1; 
                    }
                }
            }
        }
        else
        {
            // Get without options.
            h_protect_get(contxt, file, &r);
        }
            
        // Success or failure. 
        RNUM(r); 
    }
    else
    {
        // The parser is broken
        error(PANIC); 
        RCUR; 
    }
}

//----------------------------------------------------------------------------
// (startup (prompt..) (command..))
//     add a command to the boot scripts (startup-sequence, user-startup)
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_startup(entry_p contxt)
{
    // We need atleast two arguments
    if(c_sane(contxt, 2))
    {
        const char *app = str(CARG(1)); 

        entry_p command  = get_opt(CARG(2), OPT_COMMAND),
                help     = get_opt(CARG(2), OPT_HELP),
                prompt   = get_opt(CARG(2), OPT_PROMPT);

        // Expect failure. 
        DNUM = 0; 

        // We need a command, a prompt and a help text. 
        if(!command || !prompt || !help) 
        {
            error(contxt->id, ERR_MISSING_OPTION, 
                  !command ? "command" : 
                  !prompt ? "prompt" : 
                  "help"); 
            RCUR; 
        }

        // And somewhere to put the command.  
        if(!strlen(app))
        {
            error(contxt->id, ERR_INVALID_APP, app); 
            RCUR; 
        }

        // If we need confirmation and the user skips
        // or aborts, return. On abort, the HALT will
        // be set by h_confirm. Confirmation is needed
        // when user level is not novice or (confirm)
        // is used.
        if(get_opt(CARG(2), OPT_CONFIRM) || 
           get_numvar(contxt, "@user-level") > 0)
        {
            if(!h_confirm(contxt, str(help), str(prompt)))
            {
                RCUR; 
            }
        }

        // We're done if executing in pretend mode.
        if(get_numvar(contxt, "@pretend"))
        {
            RNUM(1); 
        }

        if(c_sane(command, 1))
        {
            const char *cmd = str(command->children[0]), 
                       *fln = get_strvar(contxt, "@user-startup");

            const size_t len = strlen(";BEGIN ") + strlen(app), 
                         ins = strlen(cmd) + 2;

            char *pre = CALLOC(len + 1, 1), 
                 *pst = CALLOC(len + 1, 1), 
                 *buf = NULL;

            if(pre && pst)
            {
                // We don't need to write yet. 
                FILE *fp = fopen(fln, "r"); 

                // If the file doesn't exist, 
                // try to create it.
                if(!fp && !h_exists(fln))
                {
                    fp = fopen(fln, "w"); 

                    // If successful, close and
                    // reopen as read only.
                    if(fp)
                    {
                        fclose(fp); 
                        fp = fopen(fln, "r"); 
                    }
                }

                if(fp)
                {
                    // Seek to the end so that we
                    // can use ftell below to get
                    // the size of the file. 
                    if(!fseek(fp, 0L, SEEK_END))
                    {
                        // Worst case: empty file + 3 NL +
                        // terminating 0 + BEGIN and END 
                        // markers + command.
                        size_t osz = (size_t) ftell(fp), 
                               nsz = osz + 2 * len + ins;

                        // Allocate enough memory so that
                        // we can keep the old file + any
                        // changes that we need to do in 
                        // memory at the same time. 
                        buf = CALLOC(nsz, 1);

                        if(buf)
                        {
                            // Read the whole file in one go. 
                            if(!fseek(fp, 0L, SEEK_SET) && 
                               fread(buf, 1, osz, fp) == osz) 
                            {
                                // Prepare the header and footer. 
                                snprintf(pre, len + 1, ";BEGIN %s", app);
                                snprintf(pst, len + 1, ";END %s", app);

                                // Do we already have an entry in
                                // the current file? 
                                char *beg = strstr(buf, pre), 
                                     *end = strstr(buf, pst); 

                                // Replace the current entry by inserting
                                // the new one at the same location. 
                                if(beg && end)
                                {
                                    // Move the tail of the buffer to the right place. 
                                    memmove(beg + len + ins, end, (buf + osz) - end + 1); 

                                    // Insert the command string. 
                                    memcpy(beg + len + 1, cmd, ins - 2); 

                                    // Add surrounding line feeds so that the 
                                    // command won't end up being a comment. 
                                    beg[len + ins - 1] = beg[len] = '\n';
                                }
                                // No existing entry. Append the new entry. 
                                else
                                {
                                    // Header. 
                                    memcpy(buf + osz, pre, len); 

                                    // Command. 
                                    memcpy(buf + osz + len + 1, cmd, ins - 2); 

                                    // Footer. 
                                    memcpy(buf + osz + len + ins, pst, len); 

                                    // Add surrounding line feeds so that the 
                                    // command won't end up being a comment. 
                                    buf[osz + len + ins - 1] = buf[osz + len] = '\n';

                                    // Add a newline at the end. 
                                    buf[osz + len + ins + strlen(pst)] = '\n'; 
                                }
                            }
                            else
                            {
                                // Read problem, error will be set
                                // further down if buf == NULL.
                                free(buf); 
                                buf = NULL; 
                            }
                        }
                        else
                        {
                            // Out of memory
                            error(PANIC); 
                        }
                    }

                    // Reading done. 
                    fclose(fp); 
                }
            }
            else
            {
                // Out of memory
                error(PANIC); 
            }

            // We have no use for the header
            // and footer anymore. 
            free(pre); 
            free(pst); 

            // If we have a buffer everything wen't fine
            // above. Go ahead and write buffer to file. 
            if(buf)
            {
                // Use a temporary file to make sure that we 
                // don't mess up the current file if disk space
                // becomes a problem, the system crashes, the 
                // power is lost and so on and so forth. 
                size_t tln = strlen(fln) + sizeof(".XXXXXX\0");
                char *tmp = CALLOC(tln, 1); 

                if(tmp)
                {
                    // Use mktemp, I don't know if we can use 
                    // anything better without risking that the
                    // temp file ends up outside of S:, T: for 
                    // example. 
                    snprintf(tmp, tln, "%s.XXXXXX", fln);

                    // Temporary workaround to make this build
                    // on MorphOS. Where's mktemp?
                    #ifndef __MORPHOS__
                    tmp = mktemp(tmp); 
                    #endif

                    // When using C2011 we should replace this 
                    // with "wx" in case 'tmp' already exists.
                    FILE *fp = fopen(tmp, "w"); 

                    if(fp)  
                    {
                        // Write everything to the temporary 
                        // file at once. 
                        if(fputs(buf, fp) != EOF)
                        {
                            // Close file and release buffer. 
                            fclose(fp); 
                            free(buf); 
                            buf = NULL; 

                            // Open the target file just to 
                            // make sure that we have write 
                            // permissions. 
                            fp = fopen(fln, "a"); 

                            if(fp)
                            {
                                // Close it immediately, we're not
                                // going to write directly to it. 
                                fclose(fp); 
                                
                                // Do a less un-atomic write to
                                // the real file by renaming the
                                // temporary file.
                                if(!rename(tmp, fln))
                                {
                                    // We're done. 
                                    free(tmp); 
                                    tmp = NULL; 
                                    DNUM = 1; 
                                }
                            }
                            else
                            {
                                // We aren't allowed to write data
                                // to the target file so we need to 
                                // clean up. We don't want old temp 
                                // files laying around.
                                if(remove(tmp))
                                {
                                    // This is highly unlikely, but why not? 
                                    error(contxt->id, ERR_WRITE_FILE, tmp); 
                                }
                            }
                        }
                        else
                        {
                            // Failure. Could not write to disk.
                            // The old file is still intact. 
                            fclose(fp); 
                        }
                    }

                    // If we haven't released tmp by now, we failed
                    // when attempting one of the write operations 
                    // above. 
                    if(tmp)
                    {
                        free(tmp); 
                        error(contxt->id, ERR_WRITE_FILE, fln); 
                    }
                }
                else
                {
                    // Out of memory
                    error(PANIC); 
                }

                // We no longer need the buffer holding
                // the new file contents. 
                free(buf); 
            }
            else
            {
                // No buffer == read problem, or OOM but then
                // we already have a PANIC. 
                error(contxt->id, ERR_READ_FILE, fln); 
            }
        }
        else
        {
            // Broken parser. 
            error(PANIC); 
        }
    }
    else
    {
        // Broken parser. 
        error(PANIC); 
    }

    // Return whatever we
    // have at this point. 
    RCUR; 
}

//----------------------------------------------------------------------------
// (textfile (prompt..) (help..) (dest..) (append) (include..) (confirm..) (safe))
//      create text file from other text files and strings
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//
//----------------------------------------------------------------------------
entry_p m_textfile(entry_p contxt)
{
    // We need atleast one argument
    if(c_sane(contxt, 1))
    {
        entry_p prompt   = get_opt(contxt, OPT_PROMPT),
                help     = get_opt(contxt, OPT_HELP),
                dest     = get_opt(contxt, OPT_DEST),
                append   = get_opt(contxt, OPT_APPEND),
                include  = get_opt(contxt, OPT_INCLUDE),
                confirm  = get_opt(contxt, OPT_CONFIRM),
                safe     = get_opt(contxt, OPT_SAFE); 

        DNUM = 0; 

        if(dest)
        {
            // Do we need confirmation?
            if(confirm)
            {
                // The default threshold is expert.
                int level = get_numvar(contxt, "@user-level"),
                    th = 2;

                // If the (confirm ...) option contains 
                // something that can be translated into
                // a new threshold value...
                if(confirm->children && 
                   confirm->children[0] && 
                   confirm->children[0] != end())
                {
                    // ...then do so.
                    th = num(confirm->children[0]);
                }
                                
                // If we are below the threshold value,
                // don't care about getting confirmation
                // from the user.
                if(level < th) 
                {
                    confirm = NULL; 
                }

                // Make sure that we have the prompt and
                // help texts that we need if 'confirm'
                // is set.
                if(!prompt || !help)
                {
                    error(contxt->id, ERR_MISSING_OPTION, 
                          prompt ? "help" : "prompt"); 
                    RCUR; 
                }
            }


            // If we need confirmation and the user skips
            // or aborts, return. On abort, the HALT will
            // be set by h_confirm. 
            if(confirm && 
               !h_confirm(contxt, str(help), str(prompt)))
            {
                RCUR; 
            }

            // Is this a safe operation or are we not 
            // running in pretend mode? 
            if(safe || !get_numvar(contxt, "@pretend"))
            {
                // Overwrite existing file. 
                const char *fn = str(dest); 
                FILE *fp = fopen(fn, "w"); 

                if(fp)
                {
                    // Assume success. 
                    DNUM = 1; 

                    // Include before append. 
                    if(include)
                    {
                        // File to copy. 
                        const char *fi = str(include); 
                        FILE *fs = fopen(fi, "r"); 

                        if(fs)
                        {
                            static char buf[BUFSIZ]; 
                            size_t n = fread(buf, 1, BUFSIZ, fs);

                            // Log operation.
                            h_log(contxt, tr(S_INCL), fi, fn); 

                            // Copy the whole file. 
                            while(n)
                            {
                                if(fwrite(buf, 1, n, fp) != n)
                                {
                                    error(contxt->id, ERR_WRITE_FILE, fn); 
                                    DNUM = 0; 
                                    break; 
                                }

                                // More data? 
                                n = fread(buf, 1, BUFSIZ, fs);
                            }

                            fclose(fs); 
                        }
                    }

                    // Don't append if the include failed. 
                    if(append && DNUM)
                    {
                        // String to append. 
                        const char *ap = str(append); 

                        // Log operation.
                        h_log(contxt, tr(S_APND), ap, fn); 

                        if(fputs(ap, fp) == EOF)
                        {
                            error(contxt->id, ERR_WRITE_FILE, fn); 
                            DNUM = 0; 
                        }
                    }

                    fclose(fp); 

                    // We must append and / or include,
                    // or else this doesn't make sense.
                    if(!append && !include)
                    {
                        error(contxt->id, ERR_NOTHING_TO_DO, contxt->name); 
                        DNUM = 0; 
                    }
                }
                else
                {
                    error(contxt->id, ERR_WRITE_FILE, fn); 
                }
            }
            else
            {
                // A non safe operation in pretend
                // mode always succeeds. 
                DNUM = 1; 
            }
        }
        else
        {
            error(contxt->id, ERR_MISSING_OPTION, "dest"); 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC); 
    }
            
    // Success / failure. 
    RCUR; 
}

//----------------------------------------------------------------------------
// (tooltype (prompt..) (help..) (dest..) (settooltype..) (setstack..)
//     (setdefaulttool..) (noposition) (confirm..) (safe))
//
//     modify an icon
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_tooltype(entry_p contxt)
{
    // No arguments needed. 
    if(c_sane(contxt, 0))
    {
        entry_p help            = get_opt(contxt, OPT_HELP),
                prompt          = get_opt(contxt, OPT_PROMPT),
                dest            = get_opt(contxt, OPT_DEST),
                settooltype     = get_opt(contxt, OPT_SETTOOLTYPE),
                setdefaulttool  = get_opt(contxt, OPT_SETDEFAULTTOOL),
                setstack        = get_opt(contxt, OPT_SETSTACK),
                noposition      = get_opt(contxt, OPT_NOPOSITION),
                setposition     = get_opt(contxt, OPT_SETPOSITION),
                confirm         = get_opt(contxt, OPT_CONFIRM),
                safe            = get_opt(contxt, OPT_SAFE);

        DNUM = 0; 

        // We need something to work with. 
        if(dest) 
        {
            // Something is 'dest'.info 
            const char *file = str(dest);

            // Do we need confirmation?
            if(confirm)
            {
                // The default threshold is expert.
                int level = get_numvar(contxt, "@user-level"),
                    th = 2;

                // If the (confirm ...) option contains 
                // something that can be translated into
                // a new threshold value...
                if(confirm->children && 
                   confirm->children[0] && 
                   confirm->children[0] != end())
                {
                    // ...then do so.
                    th = num(confirm->children[0]);
                }
                                
                // If we are below the threshold value,
                // don't care about getting confirmation
                // from the user.
                if(level < th) 
                {
                    confirm = NULL; 
                }

                // Make sure that we have the prompt and
                // help texts that we need if 'confirm'
                // is set.
                if(!prompt || !help)
                {
                    error(contxt->id, ERR_MISSING_OPTION, 
                          prompt ? "help" : "prompt"); 
                    RCUR; 
                }
            }

            // The (noposition) and (setposition)
            // options are mutually exclusive. 
            if(noposition && setposition)
            {
                error(contxt->id, ERR_OPTION_MUTEX, 
                      "noposition/setposition"); 
                RCUR; 
            }

            // A non safe operation in pretend mode 
            // always succeeds. 
            if(get_numvar(contxt, "@pretend") && !safe)
            {
                RNUM(1); 
            }

            // Get confirmation if necessary.
            if(!confirm ||
               h_confirm(contxt, str(help), str(prompt)))
            {
                #ifdef AMIGA
                // Get icon information. 
                struct DiskObject *obj = (struct DiskObject *) 
                    GetDiskObject(file);

                if(obj)
                {
                    // We need to save the current value
                    // of the tool type and default tool
                    // members in order to not trash mem
                    // when free:ing the diskobject.
                    char *odt = obj->do_DefaultTool;
                    char **tts = (char **) obj->do_ToolTypes; 

                    // If we're going to set tooltypes the option
                    // must have one or two children.
                    if(settooltype &&
                       c_sane(settooltype, 1))
                    { 
                        // The number of tooltypes.
                        size_t n = 0; 

                        // Get tooltype and current value (if it exists).
                        const char *t = str(settooltype->children[0]),
                                   *o = FindToolType(obj->do_ToolTypes, t);

                        // Get size of tooltype array.
                        while(*(tts + n++));

                        // Set value or create tooltype?
                        if(settooltype->children[1] && 
                           settooltype->children[1] != end())
                        {
                            // Resolve tooltype value. 
                            const char *v = str(settooltype->children[1]);

                            // If it already exists, we will replace the old
                            // value with the new one.
                            if(o)
                            {
                                // Allocate memory for a new temporary array.
                                obj->do_ToolTypes = CALLOC(n, sizeof(char *)); 

                                if(obj->do_ToolTypes)
                                {
                                    char **nts = (char **) obj->do_ToolTypes;

                                    // Copy the current set of tooltypes.
                                    memcpy(nts, tts, n  * sizeof(char **)); 

                                    // Iterate over the current set.
                                    while(*nts)
                                    {
                                        // Is the found value is within the
                                        // bounds of the current string?
                                        if(o >= *nts &&
                                           o <= (*nts + strlen(*nts)))
                                        {
                                            // Create either a new key -> value pair, or a
                                            // naked key. 
                                            if(strlen(v))
                                            {
                                                // Tooltype with value.
                                                snprintf(get_buf(), buf_size(), "%s=%s", t, v); 
                                            }
                                            else
                                            {
                                                // Naked tooltype.
                                                snprintf(get_buf(), buf_size(), "%s", t); 
                                            }

                                            // Overwrite the old tooltype.
                                            *nts = get_buf(); 
                                            break; 
                                        }

                                        // Next tooltype.
                                        nts++;
                                    }
                                }
                            }
                            // It doesn't exist, append new tooltype.
                            else
                            {
                                // Allocate memory for a new temporary array.
                                obj->do_ToolTypes = CALLOC(n + 1, sizeof(char *)); 

                                if(obj->do_ToolTypes)
                                {
                                    char **nts = (char **) obj->do_ToolTypes;

                                    // Copy the current set of tooltypes.
                                    memcpy(nts, tts, n * sizeof(char **)); 

                                    if(strlen(v))
                                    {
                                        // Tooltype with value.
                                        snprintf(get_buf(), buf_size(), "%s=%s", t, v); 
                                    }
                                    else
                                    {
                                        // Naked tooltype.
                                        snprintf(get_buf(), buf_size(), "%s", t); 
                                    }

                                    // Append tooltype.
                                    *(nts + n - 1) = get_buf(); 
                                }
                            }
                        }
                        // Delete tooltype.
                        else
                        {
                            // Is there anything to delete?
                            if(o && n > 1)
                            {
                                // Allocate memory for a new temporary array.
                                obj->do_ToolTypes = CALLOC(n, sizeof(char *)); 

                                if(obj->do_ToolTypes)
                                {
                                    char **nts = (char **) obj->do_ToolTypes,
                                         **ots = tts;

                                    // Delete tooltype by copying everything
                                    // except the tooltype to the new array.
                                    while(*ots)
                                    {
                                        if((o < *ots) ||
                                           (o > (*ots + strlen(*ots))))
                                        {
                                            *nts = *ots; 
                                            nts++;
                                        }

                                        ots++;
                                    }
                                }
                            }
                        }
                    }

                    // Change the default tool of project?
                    if(setdefaulttool)
                    {
                        // Set temporary string.
                        obj->do_DefaultTool = (char *)
                            str(setdefaulttool);
                    }
                       
                    // Set tool stacksize?
                    if(setstack)
                    { 
                        // Is a minimum value a good idea?
                        obj->do_StackSize = num(setstack); 
                    }

                    // Reset icon position?
                    if(noposition)
                    { 
                        obj->do_CurrentX = NO_ICON_POSITION;
                        obj->do_CurrentY = NO_ICON_POSITION;
                    }

                    // Set icon position?
                    if(setposition &&
                       c_sane(setposition, 2))
                    { 
                        obj->do_CurrentX = num(setposition->children[0]); 
                        obj->do_CurrentY = num(setposition->children[1]); 
                    }
                    
                    // Save all changes to the .info file. 
                    if(PutDiskObject(file, obj))
                    {
                        // Done.
                        DNUM = 1; 
                    }
                    else
                    {
                        // We failed for some unknown reason.
                        error(contxt->id, ERR_WRITE_FILE, file); 
                    }

                    // Restore DiskObject, otherwise memory
                    // will be lost / trashed. Refer to the 
                    // icon.library documentation. 

                    // If we have a new tooltype array, free
                    // it and reinstate the old one.
                    if(obj->do_ToolTypes != tts)
                    {
                        free(obj->do_ToolTypes);
                        obj->do_ToolTypes = tts;
                    }

                    // No need to free the current string,
                    // just overwrite what we have.
                    obj->do_DefaultTool = odt; 

                    // Free the DiskObject after restoring it to
                    // the state it was in before our changes. 
                    FreeDiskObject(obj);
                }
                else
                {
                    // More information? IoErr() is nice.  
                    error(contxt->id, ERR_READ_FILE, file); 
                }
                #else
                // On non-Amiga systems we always succeed.
                DNUM = 1; 

                // Remove warnings
                settooltype = setstack = setdefaulttool;
                file = NULL; 
                #endif
            }
            else
            {
                // The user did not confirm.
                DNUM = 0; 
            }
        }
        else
        {
            // We need an icon.
            error(contxt->id, ERR_MISSING_OPTION, "dest"); 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC); 
    }

    // We don't know if we're successsful, 
    // at this point, return what we have. 
    RCUR; 
}

//----------------------------------------------------------------------------
// (transcript <string1> <string2>)
//     puts concatenated strings in log file
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_transcript(entry_p contxt)
{
    // We need atleast one argument, one or
    // more strings to put into the log file.
    if(c_sane(contxt, 1))
    {
        size_t len = 0; 

        // Sum up the length of all children.
        for(entry_p *e = contxt->children; 
            *e && *e != end(); e++)
        {
            // Make it possible to mix override
            // options and strings.
            if((*e)->type != OPTION)
            {
                len += strlen(str(*e)); 
            }
        }

        // Allocate enough memory to hold the
        // concatenation of all children.
        char *buf = CALLOC(len + 2, 1);

        if(buf)
        {
            // Concatenate all children.
            for(entry_p *e = contxt->children; 
                *e && *e != end(); e++)
            {
                // Make it possible to mix override
                // options and strings.
                if((*e)->type != OPTION)
                {
                    strcat(buf, str(*e)); 
                }
            }

            // If logging is enabled, write the result
            // to the log file.
            DNUM = h_log(contxt, "%s\n", buf) ? 1 : 0; 
            free(buf); 
            RCUR; 
        }
    }

    // The parser isn't necessarily broken 
    // if we end up here. We could also be
    // out of memory.
    error(PANIC); 
    RCUR; 
}

//----------------------------------------------------------------------------
// (rename <old> <new> (help..) (prompt..) (confirm..) (safe))
//     rename files
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_rename(entry_p contxt)
{
    // We need atleast two arguments
    if(c_sane(contxt, 2))
    {
        entry_p help    = get_opt(CARG(3), OPT_HELP),
                prompt  = get_opt(CARG(3), OPT_PROMPT), 
                confirm = get_opt(CARG(3), OPT_CONFIRM), 
                disk    = get_opt(CARG(3), OPT_DISK), 
                safe    = get_opt(CARG(3), OPT_SAFE);

        const char *fr  = str(CARG(1)), 
                   *to  = str(CARG(2));

        // Do we need confirmation?
        if(confirm)
        {
            // The default threshold is expert.
            int level = get_numvar(contxt, "@user-level"); 
            int th = 2;

            // If the (confirm ...) option contains 
            // something that can be translated into
            // a new threshold value...
            if(confirm->children && 
               confirm->children[0] && 
               confirm->children[0] != end())
            {
                // ...then do so.
                th = num(confirm->children[0]);
            }
                            
            // If we are below the threshold value,
            // don't care about getting confirmation
            // from the user.
            if(level < th) 
            {
                confirm = NULL; 
            }

            // Make sure that we have the prompt and
            // help texts that we need if 'confirm'
            // is set.
            if(!prompt || !help)
            {
                error(contxt->id, ERR_MISSING_OPTION, 
                      prompt ? "help" : "prompt"); 
                RNUM(0); 
            }
        }

        // If we need confirmation and the user skips
        // or aborts, return. On abort, the HALT will
        // be set by h_confirm. 
        if(confirm && 
           !h_confirm(contxt, str(help), str(prompt)))
        {
            RNUM(0); 
        }

        // Is this a safe operation or are we not 
        // running in pretend mode? 
        if(safe || !get_numvar(contxt, "@pretend"))
        {
            // Are we going to rename a file/dir?
            if(!disk)
            {
                // Fail if the target exists. 
                if(!h_exists(to) && !rename(fr, to))
                {
                    // Success. 
                    h_log(contxt, tr(S_FRND), fr, to); 
                    RNUM(-1); 
                }
                else
                {
                    error(contxt->id, ERR_RENAME_FILE, fr); 
                    RNUM(0); 
                }
            }
            // No, we're going to relabel a volume. 
            else
            {
                #ifdef AMIGA
                // Rename volume.
                if(!Relabel(fr, to))
                {
                    // Failure.
                    RNUM(0);
                }
                #endif

                // Success. 
                h_log(contxt, tr(S_FRND), fr, to); 
                RNUM(-1); 
            }
        }
        else
        {
            // Non safe operations always 
            // succeed in pretend mode.
            RNUM(-1); 
        }
    }  
    else
    {
        // The parser is broken
        error(PANIC); 
        RCUR; 
    }
}

//----------------------------------------------------------------------------
// Name:        h_log(entry_p contxt, 
//                    const char *fmt, 
//                    ...)
//
// Description: Write formatted message to log file. 
//
// Input:       entry_p contxt:     The execution context.
//              const char *fmt:    Message format string.
//              ...:                Format string varargs.
//
// Return:      int:                Number of characters written to log file
//                                  if logging is enabled. If logging is 
//                                  disabled, '1' will always be returned.
//----------------------------------------------------------------------------
int h_log(entry_p contxt, const char *fmt, ...)
{
    // Use whatever format and arguments we get
    va_list ap; 

    if(contxt && fmt)
    {
        // Only if logging is enabled... 
        if(get_numvar(contxt, "@log"))
        {
            // Use the log file set in init(..) or by the user.
            const char *lf = get_strvar(contxt, "@log-file");

            // Don't care about existing files, just append.
            FILE *fp = fopen(lf, "a"); 
            int n = -1; 

            if(fp)
            {
                // Line number and function name as prefix.
                n = fprintf(fp, "[%d:%s] ", contxt->id, contxt->name);  

                // Append formatted string.
                if(n > 0)
                {
                    va_start(ap, fmt); 
                    n = vfprintf(fp, fmt, ap); 
                    va_end(ap); 
                }

                fclose(fp); 
            }

            // Could we open the file AND write all data to it? 
            if(n < 0)
            {
                error(contxt->id, ERR_WRITE_FILE, lf); 
                n = 0; 
            }

            return n; 
        }

        // We suceeded doing
        // nothing.
        return 1; 
    }

    // We failed.
    return 0; 
}

