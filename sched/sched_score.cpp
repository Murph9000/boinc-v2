// This file is part of BOINC.
// http://boinc.berkeley.edu
// Copyright (C) 2008 University of California
//
// BOINC is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// BOINC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with BOINC.  If not, see <http://www.gnu.org/licenses/>.

// job dispatch using a score-based approach:
// - scan the job array, assigning a score to each job and building a list
//   (the score reflect a variety of factors).
// - sort the list
// - send jobs in order of decreasing score until request is satisfied
// - do the above separately for each resource type

#include <algorithm>

#include "boinc_db.h"
#include "error_numbers.h"
#include "util.h"

#include "sched_check.h"
#include "sched_config.h"
#include "sched_hr.h"
#include "sched_main.h"
#include "sched_msgs.h"
#include "sched_send.h"
#include "sched_shmem.h"
#include "sched_types.h"
#include "sched_version.h"

#include "sched_score.h"

// given the host's estimated speed, determine its size class
//
static int get_size_class(APP& app, double es) {
    for (int i=0; i<app.n_size_classes-1; i++) {
        if (es < app.size_class_quantiles[i]) return i;
    }
    return app.n_size_classes - 1;
}

// Assign a score to this job,
// representing the value of sending the job to this host.
// Also do some initial screening,
// and return false if can't send the job to host
//
bool JOB::get_score(WU_RESULT& wu_result) {
    score = 0;

    if (!app->beta && wu_result.need_reliable) {
        if (!bavp->reliable) {
            return false;
        }
    }

    if (app->beta) {
        if (g_wreq->allow_beta_work) {
            score += 1;
        } else {
            if (config.debug_send) {
                log_messages.printf(MSG_NORMAL,
                    "[send] can't send job %d for beta app to non-beta user\n",
                    wu_result.workunit.id
                );
            }
            return false;
        }
    }

    if (app_not_selected(wu_result.workunit)) {
        if (g_wreq->allow_non_preferred_apps) {
            score -= 1;
        } else {
            return false;
        }
    }

    if (wu_result.infeasible_count) {
        score += 1;
    }

    if (app->locality_scheduling == LOCALITY_SCHED_LITE
        && g_request->file_infos.size()
    ) {
        int n = nfiles_on_host(wu_result.workunit);
        if (config.debug_locality_lite) {
            log_messages.printf(MSG_NORMAL,
                "[loc_lite] job %s has %d files on this host\n",
                wu_result.workunit.name, n
            );
        }
        if (n > 0) {
            score += 10;
        }
    }

    if (app->n_size_classes > 1) {
        double effective_speed = bavp->host_usage.projected_flops * available_frac(*bavp);
        int target_size = get_size_class(*app, effective_speed);
        if (config.debug_send) {
            log_messages.printf(MSG_NORMAL,
                "[send] size: host %d job %d speed %f\n",
                target_size, wu_result.workunit.size_class, effective_speed
            );
        }
        if (target_size == wu_result.workunit.size_class) {
            score += 5;
        } else if (target_size < wu_result.workunit.size_class) {
            score -= 2;
        } else {
            score -= 1;
        }
    }
    if (config.debug_send) {
        log_messages.printf(MSG_NORMAL,
            "[send]: score %f for result %d\n", score, wu_result.resultid
        );
    }

    return true;
}

bool job_compare(JOB j1, JOB j2) {
    return (j1.score > j2.score);
}

static double req_sec_save[NPROC_TYPES];
static double req_inst_save[NPROC_TYPES];

static void clear_others(int rt) {
    for (int i=0; i<NPROC_TYPES; i++) {
        if (i == rt) continue;
        req_sec_save[i] = g_wreq->req_secs[i];
        g_wreq->req_secs[i] = 0;
        req_inst_save[i] = g_wreq->req_instances[i];
        g_wreq->req_instances[i] = 0;
    }
}

static void restore_others(int rt) {
    for (int i=0; i<NPROC_TYPES; i++) {
        if (i == rt) continue;
        g_wreq->req_secs[i] += req_sec_save[i];
        g_wreq->req_instances[i] += req_inst_save[i];
    }
}

// send work for a particular processor type
//
void send_work_score_type(int rt) {
    vector<JOB> jobs;

    if (config.debug_send) {
        log_messages.printf(MSG_NORMAL,
            "[send] scanning for %s jobs\n", proc_type_name(rt)
        );
    }

    clear_others(rt);

    int nscan = config.mm_max_slots;
    if (!nscan) nscan = ssp->max_wu_results;
    int rnd_off = rand() % ssp->max_wu_results;
    for (int j=0; j<nscan; j++) {
        int i = (j+rnd_off) % ssp->max_wu_results;
        WU_RESULT& wu_result = ssp->wu_results[i];
        if (wu_result.state != WR_STATE_PRESENT) {
            continue;
        }
        WORKUNIT wu = wu_result.workunit;
        JOB job;
        job.app = ssp->lookup_app(wu.appid);
        if (job.app->non_cpu_intensive) continue;
        job.bavp = get_app_version(wu, true, false);
        if (!job.bavp) continue;

        job.index = i;
        job.result_id = wu_result.resultid;
        if (!job.get_score(wu_result)) {
            continue;
        }
        jobs.push_back(job);
    }

    std::sort(jobs.begin(), jobs.end(), job_compare);

    bool sema_locked = false;
    for (unsigned int i=0; i<jobs.size(); i++) {
        if (!work_needed(false)) {
            break;
        }
        if (!g_wreq->need_proc_type(rt)) {
            break;
        }
        JOB& job = jobs[i];
        if (!sema_locked) {
            lock_sema();
            sema_locked = true;
        }

        // make sure the job is still in the cache
        // array is locked at this point.
        //
        WU_RESULT& wu_result = ssp->wu_results[job.index];
        if (wu_result.state != WR_STATE_PRESENT) {
            continue;
        }
        if (wu_result.resultid != job.result_id) {
            continue;
        }
        WORKUNIT wu = wu_result.workunit;
        int retval = wu_is_infeasible_fast(
            wu,
            wu_result.res_server_state, wu_result.res_priority,
            wu_result.res_report_deadline,
            *job.app,
            *job.bavp
        );

        if (retval) {
            continue;
        }
        wu_result.state = g_pid;

        // It passed fast checks.
        // Release sema and do slow checks
        //
        unlock_sema();
        sema_locked = false;

        switch (slow_check(wu_result, job.app, job.bavp)) {
        case 1:
            wu_result.state = WR_STATE_PRESENT;
            break;
        case 2:
            wu_result.state = WR_STATE_EMPTY;
            break;
        default:
            // slow_check() refreshes fields of wu_result.workunit;
            // update our copy too
            //
            wu.hr_class = wu_result.workunit.hr_class;
            wu.app_version_id = wu_result.workunit.app_version_id;

            // mark slot as empty AFTER we've copied out of it
            // (since otherwise feeder might overwrite it)
            //
            wu_result.state = WR_STATE_EMPTY;

            // reread result from DB, make sure it's still unsent
            // TODO: from here to end of add_result_to_reply()
            // (which updates the DB record) should be a transaction
            //
            SCHED_DB_RESULT result;
            result.id = wu_result.resultid;
            if (result_still_sendable(result, wu)) {
                add_result_to_reply(result, wu, job.bavp, false);

                // add_result_to_reply() fails only in pathological cases -
                // e.g. we couldn't update the DB record or modify XML fields.
                // If this happens, don't replace the record in the array
                // (we can't anyway, since we marked the entry as "empty").
                // The feeder will eventually pick it up again,
                // and hopefully the problem won't happen twice.
            }
            break;
        }
    }
    if (sema_locked) {
        unlock_sema();
    }

    restore_others(rt);
    g_wreq->best_app_versions.clear();
}

void send_work_score() {
    for (int i=0; i<NPROC_TYPES; i++) {
        if (g_wreq->need_proc_type(i)) {
            send_work_score_type(i);
        }
    }
}
