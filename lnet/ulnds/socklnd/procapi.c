/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (c) 2002 Cray Inc.
 *  Copyright (c) 2003 Cluster File Systems, Inc.
 *
 *   This file is part of Lustre, http://www.lustre.org.
 *
 *   Lustre is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   Lustre is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Lustre; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* api.c:
 *  This file provides the 'api' side for the process-based nals.
 *  it is responsible for creating the 'library' side thread,
 *  and passing wrapped portals transactions to it.
 *
 *  Along with initialization, shutdown, and transport to the library
 *  side, this file contains some stubs to satisfy the nal definition.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifndef __CYGWIN__
#include <syscall.h>
#endif
#include <sys/socket.h>
#include <procbridge.h>
#include <pqtimer.h>
#include <dispatch.h>
#include <errno.h>


/* XXX CFS workaround, to give a chance to let nal thread wake up
 * from waiting in select
 */
static int procbridge_notifier_handler(void *arg)
{
    static char buf[8];
    procbridge p = (procbridge) arg;

    syscall(SYS_read, p->notifier[1], buf, sizeof(buf));
    return 1;
}

void procbridge_wakeup_nal(procbridge p)
{
    static char buf[8];
    syscall(SYS_write, p->notifier[0], buf, sizeof(buf));
}

/* Function: shutdown
 * Arguments: nal: a pointer to my top side nal structure
 *            ni: my network interface index
 *
 * cleanup nal state, reclaim the lower side thread and
 *   its state using PTL_FINI codepoint
 */
static void procbridge_shutdown(nal_t *n)
{
    lib_nal_t *nal = n->nal_data;
    bridge b=(bridge)nal->libnal_data;
    procbridge p=(procbridge)b->local;

    p->nal_flags |= NAL_FLAG_STOPPING;
    procbridge_wakeup_nal(p);

    do {
        pthread_mutex_lock(&p->mutex);
        if (p->nal_flags & NAL_FLAG_STOPPED) {
                pthread_mutex_unlock(&p->mutex);
                break;
        }
        pthread_cond_wait(&p->cond, &p->mutex);
        pthread_mutex_unlock(&p->mutex);
    } while (1);

    free(p);
}


/* forward decl */
extern int procbridge_startup (nal_t *, ptl_pid_t,
                               ptl_ni_limits_t *, ptl_ni_limits_t *);

/* api_nal
 *  the interface vector to allow the generic code to access
 *  this nal. this is seperate from the library side lib_nal.
 *  TODO: should be dyanmically allocated
 */
nal_t procapi_nal = {
    nal_data: NULL,
    nal_ni_init: procbridge_startup,
    nal_ni_fini: procbridge_shutdown,
};

ptl_nid_t tcpnal_mynid;

/* Function: procbridge_startup
 *
 * Arguments:  pid: requested process id (port offset)
 *                  PTL_ID_ANY not supported.
 *             desired: limits passed from the application
 *                      and effectively ignored
 *             actual:  limits actually allocated and returned
 *
 * Returns: portals rc
 *
 * initializes the tcp nal. we define unix_failure as an
 * error wrapper to cut down clutter.
 */
int procbridge_startup (nal_t *nal, ptl_pid_t requested_pid,
                        ptl_ni_limits_t *requested_limits,
                        ptl_ni_limits_t *actual_limits)
{
    nal_init_args_t args;

    procbridge p;
    bridge b;
    /* XXX nal_type is purely private to tcpnal here */
    int nal_type = PTL_IFACE_TCP;/* PTL_IFACE_DEFAULT FIXME hack */

    LASSERT(nal == &procapi_nal);

    init_unix_timer();

    b=(bridge)malloc(sizeof(struct bridge));
    p=(procbridge)malloc(sizeof(struct procbridge));
    b->local=p;

    args.nia_requested_pid = requested_pid;
    args.nia_requested_limits = requested_limits;
    args.nia_actual_limits = actual_limits;
    args.nia_nal_type = nal_type;
    args.nia_bridge = b;
    args.nia_apinal = nal;

    /* init procbridge */
    pthread_mutex_init(&p->mutex,0);
    pthread_cond_init(&p->cond, 0);
    p->nal_flags = 0;

    /* initialize notifier */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, p->notifier)) {
        perror("socketpair failed");
        return PTL_FAIL;
    }

    if (!register_io_handler(p->notifier[1], READ_HANDLER,
                procbridge_notifier_handler, p)) {
        perror("fail to register notifier handler");
        return PTL_FAIL;
    }

    /* create nal thread */
    if (pthread_create(&p->t, NULL, nal_thread, &args)) {
        perror("nal_init: pthread_create");
        return PTL_FAIL;
    }

    do {
        pthread_mutex_lock(&p->mutex);
        if (p->nal_flags & (NAL_FLAG_RUNNING | NAL_FLAG_STOPPED)) {
                pthread_mutex_unlock(&p->mutex);
                break;
        }
        pthread_cond_wait(&p->cond, &p->mutex);
        pthread_mutex_unlock(&p->mutex);
    } while (1);

    if (p->nal_flags & NAL_FLAG_STOPPED)
        return PTL_FAIL;

    b->lib_nal->libnal_ni.ni_pid.nid = tcpnal_mynid;

    return PTL_OK;
}
