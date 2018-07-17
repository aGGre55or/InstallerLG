//----------------------------------------------------------------------------
// util.c: 
//
// Misc utility functions primarily used by the 'native' methods.
//----------------------------------------------------------------------------
// Copyright (C) 2018, Ola Söder. All rights reserved.
// Licensed under the AROS PUBLIC LICENSE (APL) Version 1.1
//----------------------------------------------------------------------------

#include "alloc.h"
#include "eval.h"
#include "util.h"

#include <limits.h> 
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef AMIGA
#include <clib/debug_protos.h>
#define DBG(...) KPrintF((CONST_STRPTR)__VA_ARGS__)
#else
#define DBG(...) fprintf(stderr, __VA_ARGS__)
#endif

//----------------------------------------------------------------------------
// Name:        ror
// Description: Rotate array of entries to the right.
// Input:       entry_p *e: Array of entries.
// Return:      -
//----------------------------------------------------------------------------
void ror(entry_p *e)
{
    // We have an array and it contains something.
    if(e && *e)
    {
        int i = 0; 

        // Let 'i' be the index of the last entry. 
        while(e[i] && e[i] != end())
        {
            i++; 
        }
        i--; 

        // Nothing to do if we have < 2 entries.
        if(i > 0)
        {
            // Save the last entry.
            entry_p f = e[i]; 

            // Shift the rest to the right. 
            while(i)
            {
                e[i] = e[i - 1]; 
                i--; 
            }

            // Put the saved entry first.
            e[0] = f; 
        }
    }
}

//----------------------------------------------------------------------------
// Name:        local
// Description: Find the next context going upwards in the tree.
// Input:       entry_p e:  The starting point.
// Return:      entry_p:    The closest context found, or 
//                          NULL if no context was found. 
//----------------------------------------------------------------------------
entry_p local(entry_p e)
{
    // Go upwards until we find what we're
    // looking for, or hit the (broken) top. 
    for(; e && 
        e->type != CONTXT && 
        e->type != CUSTOM 
        ; e = e->parent);

    // Something or NULL: 
    return e; 
}

//----------------------------------------------------------------------------
// Name:        global
// Description: Find the root context. 
// Input:       entry_p e:  The starting point.
// Return:      entry_p:    The root context, or NULL
//                          if no context was found. 
//----------------------------------------------------------------------------
entry_p global(entry_p e)
{
    // Go upwards until we can't go 
    // any further.
    for(e = local(e); 
        e && local(e->parent); 
        e = local(e->parent)); 

    // Something or NULL: 
    return e; 
}

//----------------------------------------------------------------------------
// Name:        get_opt_aux
// Description: Find option of a given type in a context.
// Input:       entry_p c:  The context to search in. 
//              opt_t t:    The type of option to search for.
// Return:      entry_p:    An OPTION entry if found, NULL otherwise.
//----------------------------------------------------------------------------
static entry_p get_opt_aux(entry_p c, opt_t t)
{
    // We need a context. 
    if(c && c != end() &&
       c->children)
    {
        // And we need children.
        entry_p *v = c->children;

        // Real option or (optional)?
        if(c->type != OPTION)
        {
            // Iterate over all options.
            while(*v && *v != end())
            {
                entry_p e = *v; 

                // ID == the type of option.
                if(e->type == OPTION)
                {
                    // Dynamic options need to
                    // be resolved before eval.
                    if(e->id == OPT_DYNOPT)
                    {
                        // Replace dummy with
                        // its resolved value.
                        e = resolve(e);
                    }

                    // Have we found the right
                    // type of option?
                    if(e->id == t)
                    {
                        // We found it.
                        return e; 
                    }
                }

                // Nope, next. 
                v++; 
            }
        }
        // An (optional) string. 
        else
        {
            // Iterate over all strings.
            while(*v && *v != end())
            {
                if((t == OPT_FAIL && !strcmp(str(*v), "fail")) ||
                   (t == OPT_FORCE && !strcmp(str(*v), "force")) ||
                   (t == OPT_NOFAIL && !strcmp(str(*v), "nofail")) ||
                   (t == OPT_ASKUSER && !strcmp(str(*v), "askuser")) ||
                   (t == OPT_OKNODELETE && !strcmp(str(*v), "oknodelete")))
                {
                    return *v;
                }
                    
                // Nope, next. 
                v++; 
            }
        }
    }

    // Nothing found.
    return NULL; 
}

//----------------------------------------------------------------------------
// Name:        get_opt_va
// Description: Find option of a given type in one or more contexts.
// Input:       opt_t t:    The type of option to search for.
//              ...         Any number of entry_p contexts to search in.
//              NULL        Terminating NULL
// Return:      entry_p:    An OPTION entry if found, NULL otherwise.
//----------------------------------------------------------------------------
entry_p get_opt_va(opt_t t, ...)
{
    // Init VA.
    va_list ap;
    entry_p cur, top;

    va_start(ap, t);
    top = cur = va_arg(ap, entry_p);

    // For all arguments following the
    // type, treat them as contxts and
    // search for the option therein.
    while(cur)
    {
        // Search for option.
        cur = get_opt_aux(cur, t);

        if(cur)
        {
            // We found it.
            break;
        }
        else
        {
            // Next argument.
            cur = va_arg(ap, entry_p);
        }
    }

    // End VA.
    va_end(ap);

    // If in non strict mode, allow the absense
    // of (prompt) and (help).
    if(!cur && top &&
       !get_numvar(top, "@strict"))
    {
        if(t == OPT_PROMPT || t == OPT_HELP)
        {
            // This will be resolved as an
            // emtpy string.
            return new_failure();
        }
    }

    // Option or NULL.
    return cur;
}

//----------------------------------------------------------------------------
// Name:        c_sane
// Description: Context sanity check used by the NATIVE functions to verify
//              that we have atleast the number of children needed and that 
//              these are valid. If this fails, it typically, but not always,
//              means that we have a parser problem.
// Input:       entry_p c:  The context. 
//              size_t n:   The number of children necessary.  
// Return:      int:        1 if context is valid, 0 otherwise. 
//----------------------------------------------------------------------------
int c_sane(entry_p c, size_t n)
{
    // Assume success; 
    int status = 1; 

    // All contexts used by NATIVE functions must allocate an
    // array for children, it could be empty though. 
    if(c && c->children)
    {
        // Expect atleast n children. 
        for(size_t i = 0; 
            i < n; i++)
        {
            // Assume failure; 
            status = 0; 

            // Make sure that something exists.
            if(c->children[i] == NULL)
            {
                DBG("c->children[%d] == NULL\n", (int) i);
                break;
            }

            // Make sure that it's not a sentinel.
            if(c->children[i] == end())
            {
                DBG("c->children[%d] == end()\n", (int) i);
                break;
            }

            // Make sure that it belongs to us. 
            if(c->children[i]->parent != c)
            {
                DBG("c->children[%d]->parent != %p\n", (int) i, (void *) c);
                break;
            }

            // Make sure that all NATIVE functions have
            // a default return value. 
            if(c->type == NATIVE && !c->resolved)
            {
                DBG("c->type == NATIVE && !c->resolved\n"); 
                break;
            }
            
            // We're OK; 
            status = 1; 
        }
    }
    else
    {
        // Badly broken.
        status = 0; 
        DBG("!c || !c->children\n");
    }

    // Did we fail? 
    if(!status)
    {
        // Additional debugging info.
        pretty_print(c); 
    }

    return status; 
}

//----------------------------------------------------------------------------
// Name:        s_sane
// Description: Context sanity check used by the NATIVE functions to verify
//              that we have atleast the number of symbols needed and that 
//              these are valid. If this fails it typically, but not always,
//              means that we have a parser problem.
// Input:       entry_p c:  The context. 
//              size_t n:   The number of symbols necessary.  
// Return:      int:        1 if context is valid, 0 otherwise. 
//----------------------------------------------------------------------------
int s_sane(entry_p c, size_t n)
{
    // Assume success; 
    int status = 1; 

    if(c && c->symbols)
    {
        // Expect atleast n symbols. 
        for(size_t i = 0; 
            i < n; i++)
        {
            // Assume failure; 
            status = 0; 

            // Make sure that somethings exists.
            if(c->symbols[i] == NULL)
            {
                DBG("c->symbols[%d] == NULL\n", (int) i);
                break;
            }

            // Make sure that it's not a sentinel.
            if(c->symbols[i] == end())
            {
                DBG("c->symbols[%d] == end()\n", (int) i);
                break;
            }

            // Make sure that it belongs to us. 
            if(c->symbols[i]->parent != c)
            {
                DBG("c->symbols[%d]->parent != %p\n", (int) i, (void *) c);
                break;
            }

            // We're OK; 
            status = 1; 
        }
    }
    else
    {
        // Badly broken.
        status = 0; 
        DBG("!c || !c->symbols\n");
    }

    return status; 
}

//----------------------------------------------------------------------------
// Name:        set_numvar
// Description: Give an existing numerical variable a new value. Please note
//              that the variable must exist and that the current resolved
//              value must be a NUMBER, if not, this function will silently
//              fail. 
// Input:       entry_p c:  The context. 
//              char *v:    The name of the variable.   
//              int n:      The new value of the variable. 
// Return:      -
//----------------------------------------------------------------------------
void set_numvar(entry_p c, char *v, int n)
{
    // We need a name and a context. 
    if(c && v)
    {
        // Dummy reference used to find
        // the variable.
        entry_p s; 
        static entry_t ref = { .type = SYMREF }; 

        // Name and reparent dummy. 
        ref.name = v; 
        ref.parent = c; 

        // Find whatever 'v' is. 
        s = find_symbol(&ref);

        // This should be a symbol.
        if(s && s->type == SYMBOL)
        {
            // And it should be a resolved
            // numerical one. 
            if(s->resolved &&
               s->resolved->type == NUMBER)
            {
                // Success. 
                s->resolved->id = n;
                return; 
            }
        }
    }

    // Failure. 
}

//----------------------------------------------------------------------------
// Name:        get_numvar
// Description: Get the value of an existing numerical variable. Please note
//              that the variable must exist and that the current resolved
//              value must be a NUMBER.
// Input:       entry_p c:  The context. 
//              char *v:    The name of the variable.   
// Return:      int:        The value of the variable or zero if the variable
//                          can't be found.  
//----------------------------------------------------------------------------
int get_numvar(entry_p c, char *v)
{
    // We need a name and a context. 
    if(c && v)
    {
        // Dummy reference used to find
        // the variable.
        entry_p s; 
        static entry_t ref = { .type = SYMREF }; 

        // Name and reparent dummy. 
        ref.name = v; 
        ref.parent = c; 

        // Find whatever 'v' is. 
        s = find_symbol(&ref);

        // This should be a symbol.
        if(s && s->type == SYMBOL)
        {
            // And it should be a resolved
            // numerical one. 
            if(s->resolved &&
               s->resolved->type == NUMBER)
            {
                // Success.
                return s->resolved->id; 
            }
        }
    }

    // Failure. 
    return 0; 
}

//----------------------------------------------------------------------------
// Name:        get_strvar
// Description: Get the value of an existing string variable. Please note
//              that the variable must exist and that the current resolved
//              value must be a STRING.
// Input:       entry_p c:  The context. 
//              char *v:    The name of the variable.   
// Return:      char *:     The value of the variable or an empty string if 
//                          the variable can't be found.  
//----------------------------------------------------------------------------
char *get_strvar(entry_p c, char *v)
{
    // We need a name and a context. 
    if(c && v)
    {
        // Dummy reference used to find
        // the variable.
        entry_p s; 
        static entry_t ref = { .type = SYMREF }; 

        // Name and reparent dummy. 
        ref.name = v; 
        ref.parent = c; 

        // Find whatever 'v' is. 
        s = find_symbol(&ref);

        // This should be a symbol.
        if(s && s->type == SYMBOL)
        {
            // And it should be a resolved
            // string. 
            if(s->resolved &&
               s->resolved->name &&
               s->resolved->type == STRING)
            {
                // Success.
                return s->resolved->name; 
            }
        }
    }

    // Failure. 
    return ""; 
}

//----------------------------------------------------------------------------
// Name:        get_optstr_va
// Description: Concatenate all the strings in all the options of a given
//              type in a variable number of contexts.
// Input:       opt_t t:    The option type.
//              ...         Any number of entry_p contexts to search in.
// Return:      char *:     A concatenation of all the strings found.
//----------------------------------------------------------------------------
char *get_optstr_va(opt_t t, ...)
{
    entry_p c; 
    va_list ap;
    size_t n = 0; 
    char *r = NULL;

    // Init VA.
    va_start(ap, t);
    c = va_arg(ap, entry_p);

    // Iterate over all contexts.
    while(c && c_sane(c, 0))
    {
        entry_p *e = c->children;

        // Count the number of children
        // of the given option type.
        while(*e && *e != end())
        {
            if((*e)->type == OPTION &&
               (*e)->id == t)
            {
                n++;
            }

            // Next child.
            e++;
        }

        // Next argument.
        c = va_arg(ap, entry_p);
    }

    // End VA.
    va_end(ap);

    // Did we find any children with
    // the right type?
    if(n)
    {
        // Subtotals.
        char *cs[n];
        size_t s = 1,
               i = 0;

        // Init VA, again.
        va_start(ap, t);
        c = va_arg(ap, entry_p);

        // Iterate over all contexts once
        // again, find the right options,
        // and resolve them all.
        while(c)
        {
            entry_p *e = c->children;

            // For all children of the current
            // context, resolve the ones with
            // the correct type.
            while(*e && *e != end())
            {
                if((*e)->type == OPTION &&
                   (*e)->id == t)
                {
                    // Merge strings if needed.
                    cs[i] = get_chlstr(*e);

                    // If we run out of memory,
                    // free everything we have
                    // allocated this far
                    if(!cs[i])
                    {
                        while(i--)
                        {
                            free(cs[i]);
                        }

                        // End VA.
                        va_end(ap);
                        return r;
                    }

                    // Keep track of the total
                    // string length.
                    s += strlen(cs[i]);
                    i++;
                }

                // Next child.
                e++;
            }

            // Next argument.
            c = va_arg(ap, entry_p);
        }

        // End VA.
        va_end(ap);

        // Allocate memory to hold the
        // final concatenation.
        r = calloc(s, 1);

        if(r)
        {
            // Concatenate and free
            // substrings in one go.
            for(i = 0; i < n; i++)
            {
                strcat(r, cs[i]);
                free(cs[i]);
            }
        }
    }
    
    // Success or OOM.
    return r; 
}

//----------------------------------------------------------------------------
// Name:        get_chlst
// Description: Concatenate the string representations of all children of a 
//              context. 
// Input:       entry_p c:  The context.
// Return:      char *:     The concatenation of the string representations 
//                          of all children of 'c'.
//----------------------------------------------------------------------------
char *get_chlstr(entry_p c)
{
    // Concatenation.
    char *r = NULL;

    // We don't really need anything to
    // concatenate but we expect a sane
    // contxt.
    if(c_sane(c, 0))
    {
        size_t n = 0; 
        entry_p *e = c->children;

        // Count the number of children.
        while(*e && *e != end())
        {
            n++;
            e++;
        }

        if(n)
        {
            // Allocate memory to hold a string pointer
            // for each child.
            char **v = calloc(n + 1, sizeof(char *));

            if(v)
            {
                size_t l = 0; 

                // Save all string pointers so that we
                // don't evaluate children twice and 
                // thereby set of side effects more
                // than once.
                while(n > 0)
                {
                    v[--n] = str(*(--e));
                    l += strlen(v[n]);
                }

                // If the total length is non zero, we
                // will concatenate all children.
                if(l)
                {
                    // Allocate memory to hold the full
                    // concatenated string.
                    r = calloc(l + 1, 1);

                    if(r)
                    {
                        // The actual concatenation. The
                        // 'v' array is null terminated.
                        while(v[n])
                        {
                            strcat(r, v[n]); 
                            n++;
                        }
                    }
                }
                else
                {
                    // No data to concatenate.
                    r = strdup("");
                }

                // Free the references before
                // returning.
                free(v);
            }
        }
        else
        {
            // No children to concatenate.
            r = strdup("");
        }
    }

    // We could be in any state
    // here, success or panic. 
    return r;
}

//----------------------------------------------------------------------------
// Name:        set_strvar
// Description: Give an existing string variable a new value. Please note
//              that the variable must exist and that the current resolved
//              value must be a STRING, if not, this function will silently
//              fail. 
// Input:       entry_p c:  The context. 
//              char *v:    The name of the variable.   
//              char *n:    The new value of the variable. 
// Return:      -
//----------------------------------------------------------------------------
void set_strvar(entry_p c, char *v, char *n)
{
    // We need a name and a context. 
    if(c && v)
    {
        // Dummy reference used to find
        // the variable.
        entry_p s; 
        static entry_t ref = { .type = SYMREF }; 

        // Name and reparent dummy. 
        ref.name = v; 
        ref.parent = c; 

        // Find whatever 'v' is. 
        s = find_symbol(&ref);

        // This should be a symbol.
        if(s && s->type == SYMBOL)
        {
            // And it should be a resolved
            // STRING. 
            if(s->resolved &&
               s->resolved->type == STRING)
            {
                // Taking ownership of 'n'. 
                free(s->resolved->name); 
                s->resolved->name = n; 
            }
        }
    }
}

//----------------------------------------------------------------------------
// Name:        pp_aux
// Description: Refer to pretty_print below. 
// Input:       entry_p entry:  The tree to print.  
//              int indent:     Indentation level.
// Return:      -
//----------------------------------------------------------------------------
static void pp_aux(entry_p entry, int indent)
{
    // Indentation galore. 
    char t[16] = "\t\t\t\t"
                 "\t\t\t\t"
                 "\t\t\t\t"
                 "\t\t\t\0"; 
    
    // Going backwards to go forward. 
    char *ts = t + sizeof(t) - 1 - indent; 
    ts = ts < t ? t : ts; 

    // Data type descriptions. 
    char *tps[] = 
    {
        "NUMBER",
        "STRING", 
        "SYMBOL",
        "SYMREF",
        "NATIVE", 
        "OPTION", 
        "CUSTOM", 
        "CUSREF", 
        "CONTXT",
        "STATUS",
        "DANGLE"
    };

    // NULL is a valid value.
    if(entry)
    {
        // All entries have a type, a parent and an ID.
        DBG("%s\n", tps[entry->type]);
        DBG("%sThis:%p\n", ts, (void *) entry);
        DBG("%sParent:%p\n", ts, (void *) entry->parent);
        DBG("%sId:\t%d\n", ts, entry->id);

        // Most, but not all, have a name.
        if(entry->name) 
        {
            DBG("%sName:\t%s\n", ts, entry->name);
        }

         // Natives and cusrefs have callbacks.
        if(entry->call)
        {
            DBG("%sCall:\t%p\n", ts, (void *) entry->call);
        }

        // Functions / symbols can be 'resolved'.
        if(entry->resolved) 
        {
            DBG("%sRes:\t", ts);

            // Pretty print the 'resolved' entry, 
            // last / default return value and 
            // values refered to by symbols. 
            pp_aux(entry->resolved, indent + 1);
        }

        // Pretty print all children.
        if(entry->children) 
        {
            entry_p *e = entry->children;

            while(*e && *e != end())
            {
                DBG("%sChl:\t", ts); 
                pp_aux(*e, indent + 1); 
                e++; 
            }
        }

        // Pretty print all symbols.
        if(entry->symbols) 
        {
            entry_p *e = entry->symbols;

            while(*e && *e != end())
            {
                DBG("%sSym:\t", ts); 
                pp_aux(*e, indent + 1); 
                e++; 
            }
        }
    }
    else
    {
        DBG("NULL\n\n");
    }
}

//----------------------------------------------------------------------------
// Name:        pretty_print
// Description: Pretty print the complete tree in 'entry'.
// Input:       entry_p entry:  The tree to print.  
// Return:      -
//----------------------------------------------------------------------------
void pretty_print(entry_p entry)
{
    // Start with no indentation.
    pp_aux(entry, 0); 
}

#define LG_BUFSIZ ((BUFSIZ > PATH_MAX ? BUFSIZ : PATH_MAX) + 1)

//----------------------------------------------------------------------------
// Name:        get_buf
// Description: Get pointer to temporary buffer.
// Input:       - 
// Return:      char *: Buffer pointer.
//----------------------------------------------------------------------------
char *get_buf(void)
{
    static char buf[LG_BUFSIZ];
    return buf; 
}

//----------------------------------------------------------------------------
// Name:        buf_size
// Description: Get size of temporary buffer.
// Input:       - 
// Return:      size_t: Buffer size.
//----------------------------------------------------------------------------
size_t buf_size(void)
{
    return LG_BUFSIZ - 1; 
}

