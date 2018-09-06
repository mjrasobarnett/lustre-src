/*
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.  A copy is
 * included in the COPYING file that accompanied this code.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * GPL HEADER END
 */
/*
 * Copyright (c) 2018, Intel Corporation.
 */

#define DEBUG_SUBSYSTEM S_MDS

#include <linux/kernel.h>
#include <linux/kmod.h>
#include <obd_support.h>
#include "mdt_internal.h"

int mdt_hsm_upcall(struct mdt_thread_info *mti,
                   enum hsm_copytool_action action,
                   const struct hsm_request *hr,
                   const struct hsm_user_item *hui,
                   const void *data)
{
	struct mdt_device *mdt = mti->mti_mdt;
        struct coordinator *cdt = &mdt->mdt_coordinator;
	u64 archive_id = hr->hr_archive_id != 0 ?
		hr->hr_archive_id : cdt->cdt_default_archive_id;
	u64 flags = hr->hr_flags;
	unsigned int count = hr->hr_itemcount;
	char fsname[MTI_NAME_MAXLEN + 1];
	char archive_id_arg[3 * sizeof(archive_id) + 1];
	char flags_arg[3 * sizeof(flags) + 1];
	char **argv = NULL;
	unsigned int i;
	int rc;
	ENTRY;
	CERROR("DEBUG: 1\n");

	/* If action is not in the upcall mask or the upcall path has
	 * not been set then return +1 to reject the request and it
	 * the CDT will handle the request normally. */
	if (!(cdt->cdt_upcall_mask & (1UL << action)) ||
	    cdt->cdt_upcall_path[0] == '\0')
		RETURN(+1);

	/* XXX MDT will not dedup upcalled actions.
	 * XXX Cancel is not supported for upcalled actions.
	 * XXX Get actions is not supported for upcalled actions.
	 * XXX Progress is not supported for upcalled actions.
	 *
	 * XXX The RPC handler will block until the upcall
	 * completes. So for safety/liveness the upcall should really
	 * not access Lustre. Instead the upcall should put the
	 * request in an off-Lustre persistent queue or database and
	 * then exit.
	 *
	 * FIXME Future safe extent support.
	 *
	 * Caller has already validated hr, hui, data.
	 *
	 * Upcall invocation:
	 *   UPCALL ACTION FSNAME ARCHIVE_ID FLAGS DATA FID...
	 */

	if (archive_id == 0)
		archive_id = cdt->cdt_default_archive_id;

	CERROR("DEBUG: 2\n");
	obd_uuid2fsname(fsname, mdt_obd_name(mdt), sizeof(fsname));
	CERROR("DEBUG: 3\n");
	snprintf(archive_id_arg, sizeof(archive_id_arg), "%llu", archive_id);
	CERROR("DEBUG: 4\n");
	snprintf(flags_arg, sizeof(flags_arg), "%llu", flags);
	CERROR("DEBUG: 5\n");

	OBD_ALLOC(argv, (6 + count) * sizeof(argv[0]));
	if (argv == NULL)
		GOTO(out, rc = -ENOMEM);

	CERROR("DEBUG: 6\n");
	argv[0] = cdt->cdt_upcall_path;
	argv[1] = (char *)hsm_copytool_action2name(action);
	argv[2] = fsname;
	argv[3] = archive_id_arg;
	argv[4] = flags_arg;
	argv[5] = (char *)data;
	CERROR("DEBUG: 7\n");

	for (i = 0; i < count; i++) {
	  CERROR("DEBUG: 8 (%d)\n", i);
		OBD_ALLOC(argv[6 + i], FID_LEN + 1);
	  CERROR("DEBUG: 8.1 (%d)\n", i);
		if (argv[6 + i] == NULL)
	    CERROR("DEBUG: 8.2 (%d)\n", i);
			GOTO(out_argv, rc = -ENOMEM);

	  CERROR("DEBUG: 9 (%d)\n", i);
	  CERROR("DEBUG: 9.0 (%d) - count: %d \n", i, count);
	  CERROR("DEBUG: 9.1 (%d) - argv[6+i]: %s \n", i, argv[6+i]);
	  CERROR("DEBUG: 9.3 (%d) - FID_LEN: %d \n", i, FID_LEN);
	  CERROR("DEBUG: 9.4 (%d) - DFID: %s \n", i, DFID);
	  CERROR("DEBUG: 9.5 (%d) - hui[i]: %s \n", i, hui[i]);
		snprintf(argv[6 + i], FID_LEN + 1, DFID,
			 PFID(&hui[i].hui_fid));
	  CERROR("DEBUG: 10 (%d)\n", i);
	}

	rc = call_usermodehelper(cdt->cdt_upcall_path, argv, NULL /* env */,
				 UMH_WAIT_PROC);
	CERROR("DEBUG: 11\n");
	if (rc != 0) {
		CERROR("%s: HSM upcall '%s' failed: rc = %d\n",
		       mdt_obd_name(mdt), cdt->cdt_upcall_path, rc);
	}

out_argv:
	CERROR("DEBUG: 12.1\n");
	for (i = 0; i < count; i++) {
		if (argv[6 + i] != NULL)
			OBD_FREE(argv[6 + i], FID_LEN + 1);
	}

	CERROR("DEBUG: 12.2\n");
	OBD_FREE(argv, (6 + count) * sizeof(argv[0]));
out:
	CERROR("DEBUG: 13\n");
	RETURN(rc);
}
