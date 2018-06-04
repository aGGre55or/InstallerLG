//----------------------------------------------------------------------------
// eval.h: 
//
// Functions for evaluation of entry_t objects. 
//----------------------------------------------------------------------------

#ifndef EVAL_H_
#define EVAL_H_

#include "types.h"

int num(entry_p entry);
int tru(entry_p entry);
void run(entry_p entry);
char *str(entry_p entry);
entry_p invoke(entry_p entry);
entry_p resolve(entry_p entry);
entry_p find_symbol(entry_p entry);

#endif
