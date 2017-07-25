/*++
Copyright (c) 2008 Microsoft Corporation

Module Name:

    progress_callback.h

Abstract:

    Virtual callback for reporting progress.

Author:

    Michal Moskal (micmo) 2009-02-17.

Revision History:

--*/
#ifndef _PROGRESS_CALLBACK_H_
#define _PROGRESS_CALLBACK_H_

class progress_callback {
public:
    virtual ~progress_callback() {}
    
    // Called approx. every m_progress_sampling_freq miliseconds
    virtual void slow_progress_sample() { }
    
    // Called on every check for reqsource limit exceeded (mach more frequent).
    virtual void fast_progress_sample() { }
};

#endif
