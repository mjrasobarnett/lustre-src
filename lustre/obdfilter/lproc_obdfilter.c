/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2002, 2003 Cluster File Systems, Inc.
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
 *
 */
#define DEBUG_SUBSYSTEM S_CLASS

#include <linux/version.h>
#include <linux/lprocfs_status.h>
#include <linux/obd.h>

#include "filter_internal.h"

#ifndef LPROCFS
static struct lprocfs_vars lprocfs_obd_vars[]  = { {0} };
static struct lprocfs_vars lprocfs_module_vars[] = { {0} };
#else

static int lprocfs_filter_rd_mntdev(char *page, char **start, off_t off,
                                    int count, int *eof, void *data)
{
        struct obd_device* obd = (struct obd_device *)data;

        LASSERT(obd != NULL);
        LASSERT(obd->u.filter.fo_vfsmnt->mnt_devname);
        *eof = 1;
        return snprintf(page, count, "%s\n",
                        obd->u.filter.fo_vfsmnt->mnt_devname);
}

static int lprocfs_filter_rd_last_id(char *page, char **start, off_t off,
                                     int count, int *eof, void *data)
{
        struct obd_device *obd = data;

        if (obd == NULL)
                return 0;

        return snprintf(page, count, LPU64"\n",
                        filter_last_id(&obd->u.filter, 0));
}

static struct lprocfs_vars lprocfs_obd_vars[] = {
        { "uuid",         lprocfs_rd_uuid,          0, 0 },
        { "blocksize",    lprocfs_rd_blksize,       0, 0 },
        { "kbytestotal",  lprocfs_rd_kbytestotal,   0, 0 },
        { "kbytesfree",   lprocfs_rd_kbytesfree,    0, 0 },
        { "filestotal",   lprocfs_rd_filestotal,    0, 0 },
        { "filesfree",    lprocfs_rd_filesfree,     0, 0 },
        //{ "filegroups",   lprocfs_rd_filegroups,    0, 0 },
        { "fstype",       lprocfs_rd_fstype,        0, 0 },
        { "mntdev",       lprocfs_filter_rd_mntdev, 0, 0 },
        { "last_id",      lprocfs_filter_rd_last_id, 0, 0 },
        { 0 }
};

static struct lprocfs_vars lprocfs_module_vars[] = {
        { "num_refs",     lprocfs_rd_numrefs,       0, 0 },
        { 0 }
};

#endif /* LPROCFS */
LPROCFS_INIT_VARS(filter,lprocfs_module_vars, lprocfs_obd_vars)
