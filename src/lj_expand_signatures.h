#ifndef _LJ_EXPAND_SIGNATURES_H
#define _LJ_EXPAND_SIGNATURES_H

#define SIGDEF(name, _) LJE_SIG_##name,
enum LJE_Signature
{
#include "lje_signatures.h"
};

#undef SIGDEF
#define SIGDEF(_, sig) sig,
static const char* const lje_signature_str[] = {
#include "lje_signatures.h"
};
#undef SIGDEF

#define lje_sig(name) lje_signature_str[LJE_SIG_##name]

#endif