/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    smt2parser.h

Abstract:

    SMT 2.0 parser

Author:

    Leonardo de Moura (leonardo) 2011-03-01

Revision History:

--*/
#ifndef _SMT2_PARSER_H_
#define _SMT2_PARSER_H_

#include"cmd_context.h"

bool parse_smt2_commands(cmd_context & ctx, std::istream & is, bool interactive = false);

#endif
