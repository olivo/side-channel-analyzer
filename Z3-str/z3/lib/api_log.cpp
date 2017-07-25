/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    api_log.cpp

Abstract:
    API for creating logs

Author:

    Leonardo de Moura (leonardo) 2012-02-29.

Revision History:

--*/
#include<iostream>
#include<fstream>
#include"z3.h"
#include"api_log_macros.h"
#include"util.h"

std::ostream * g_z3_log = 0;
bool g_z3_log_enabled   = false;

extern "C" {
    Z3_bool Z3_API Z3_open_log(Z3_string filename) {
        if (g_z3_log != 0)
            Z3_close_log();
        g_z3_log = alloc(std::ofstream, filename);
        g_z3_log_enabled = true;
        if (g_z3_log->bad() || g_z3_log->fail()) {
            dealloc(g_z3_log);
            g_z3_log = 0;
            return Z3_FALSE;
        }
        return Z3_TRUE;
    }

    void Z3_API Z3_append_log(Z3_string str) {
        if (g_z3_log == 0)
            return;
        _Z3_append_log(static_cast<char const *>(str));
    }

    void Z3_API Z3_close_log() {
        if (g_z3_log != 0) {
            dealloc(g_z3_log);
            g_z3_log_enabled = false;
            g_z3_log = 0;
        }
    }
}
