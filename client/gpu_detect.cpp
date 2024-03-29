// This file is part of BOINC.
// http://boinc.berkeley.edu
// Copyright (C) 2009 University of California
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


// client-specific GPU code.  Mostly GPU detection

#ifndef _DEBUG
#define USE_CHILD_PROCESS_TO_DETECT_GPUS 1
#endif

#include "cpp.h"

#ifdef _WIN32
#include "boinc_win.h"
#ifdef _MSC_VER
#define snprintf _snprintf
#define chdir _chdir
#endif
#else
#include "config.h"
#include <setjmp.h>
#include <signal.h>
#endif

#include "coproc.h"
#include "file_names.h"
#include "util.h"
#include "str_replace.h"
#include "client_msgs.h"
#include "client_state.h"

using std::string;
using std::vector;

#ifndef _WIN32
jmp_buf resume;

void segv_handler(int) {
    longjmp(resume, 1);
}
#endif

vector<COPROC_ATI> ati_gpus;
vector<COPROC_NVIDIA> nvidia_gpus;
vector<COPROC_INTEL> intel_gpus;
vector<OPENCL_DEVICE_PROP> ati_opencls;
vector<OPENCL_DEVICE_PROP> nvidia_opencls;
vector<OPENCL_DEVICE_PROP> intel_gpu_opencls;
vector<OPENCL_CPU_PROP> cpu_opencls;

static char* client_path;
    // argv[0] from the command used to run client.
    // May be absolute or relative.
static char client_dir[MAXPATHLEN];
    // current directory at start of client

void COPROCS::get(
    bool use_all, vector<string>&descs, vector<string>&warnings,
    IGNORE_GPU_INSTANCE& ignore_gpu_instance
) {
#if USE_CHILD_PROCESS_TO_DETECT_GPUS
    int retval = 0;
    char buf[256];

    retval = launch_child_process_to_detect_gpus();
    if (retval) {
        snprintf(buf, sizeof(buf),
            "launch_child_process_to_detect_gpus() returned error %d",
            retval
        );
        warnings.push_back(buf);
    }
    retval = read_coproc_info_file(warnings);
    if (retval) {
        snprintf(buf, sizeof(buf),
            "read_coproc_info_file() returned error %d",
            retval
        );
        warnings.push_back(buf);
    }
#else
    detect_gpus(warnings);
#endif
    correlate_gpus(use_all, descs, ignore_gpu_instance);
}


void COPROCS::detect_gpus(std::vector<std::string> &warnings) {
#ifdef _WIN32
    try {
        nvidia.get(warnings);
    }
    catch (...) {
        warnings.push_back("Caught SIGSEGV in NVIDIA GPU detection");
    }
    try {
        ati.get(warnings);
    } 
    catch (...) {
        warnings.push_back("Caught SIGSEGV in ATI GPU detection");
    }
    try {
        intel_gpu.get(warnings);
    } 
    catch (...) {
        warnings.push_back("Caught SIGSEGV in INTEL GPU detection");
    }
    try {
        // OpenCL detection must come last
        get_opencl(warnings);
    }
    catch (...) {
        warnings.push_back("Caught SIGSEGV in OpenCL detection");
    }
#else
    void (*old_sig)(int) = signal(SIGSEGV, segv_handler);
    if (setjmp(resume)) {
        warnings.push_back("Caught SIGSEGV in NVIDIA GPU detection");
    } else {
        nvidia.get(warnings);
    }
    

#ifndef __APPLE__       // ATI does not yet support CAL on Macs
    if (setjmp(resume)) {
        warnings.push_back("Caught SIGSEGV in ATI GPU detection");
    } else {
        ati.get(warnings);
    }
#endif
    if (setjmp(resume)) {
        warnings.push_back("Caught SIGSEGV in INTEL GPU detection");
    } else {
        intel_gpu.get(warnings);
    }
    if (setjmp(resume)) {
        warnings.push_back("Caught SIGSEGV in OpenCL detection");
    } else {
        // OpenCL detection must come last
        get_opencl(warnings);
    }
    signal(SIGSEGV, old_sig);
#endif
}


void COPROCS::correlate_gpus(
    bool use_all,
    std::vector<std::string> &descs,
    IGNORE_GPU_INSTANCE &ignore_gpu_instance
) {
    unsigned int i;
    char buf[256], buf2[256];

    nvidia.correlate(use_all, ignore_gpu_instance[PROC_TYPE_NVIDIA_GPU]);
    ati.correlate(use_all, ignore_gpu_instance[PROC_TYPE_AMD_GPU]);
    intel_gpu.correlate(use_all, ignore_gpu_instance[PROC_TYPE_INTEL_GPU]);
    correlate_opencl(use_all, ignore_gpu_instance);

    // NOTE: OpenCL can report a max of only 4GB.  
    for (i=0; i<cpu_opencls.size(); i++) {
        gstate.host_info.opencl_cpu_prop[gstate.host_info.num_opencl_cpu_platforms++] = cpu_opencls[i];
    }

    for (i=0; i<nvidia_gpus.size(); i++) {
        // This is really CUDA description
        nvidia_gpus[i].description(buf, sizeof(buf));
        switch(nvidia_gpus[i].is_used) {
        case COPROC_IGNORED:
            snprintf(buf2, sizeof(buf2),
                "CUDA: NVIDIA GPU %d (ignored by config): %s",
                nvidia_gpus[i].device_num, buf
            );
            break;
        case COPROC_USED:
            snprintf(buf2, sizeof(buf2),
                "CUDA: NVIDIA GPU %d: %s",
                nvidia_gpus[i].device_num, buf
            );
            break;
        case COPROC_UNUSED:
        default:
            snprintf(buf2, sizeof(buf2),
                "CUDA: NVIDIA GPU %d (not used): %s",
                nvidia_gpus[i].device_num, buf
            );
            break;
        }
        descs.push_back(string(buf2));
    }

    for (i=0; i<ati_gpus.size(); i++) {
        // This is really CAL description
        ati_gpus[i].description(buf, sizeof(buf));
        switch(ati_gpus[i].is_used) {
        case COPROC_IGNORED:
            snprintf(buf2, sizeof(buf2),
                "CAL: ATI GPU %d (ignored by config): %s",
                ati_gpus[i].device_num, buf
            );
            break;
        case COPROC_USED:
            snprintf(buf2, sizeof(buf2),
                "CAL: ATI GPU %d: %s",
                ati_gpus[i].device_num, buf
            );
            break;
        case COPROC_UNUSED:
        default:
            snprintf(buf2, sizeof(buf2),
                "CAL: ATI GPU %d: (not used) %s",
                ati_gpus[i].device_num, buf
            );
            break;
        }
        descs.push_back(string(buf2));
    }

    // Create descriptions for OpenCL NVIDIA GPUs
    //
    for (i=0; i<nvidia_opencls.size(); i++) {
        nvidia_opencls[i].description(buf, sizeof(buf), proc_type_name(PROC_TYPE_NVIDIA_GPU));
        descs.push_back(string(buf));
    }

    // Create descriptions for OpenCL ATI GPUs
    //
    for (i=0; i<ati_opencls.size(); i++) {
        ati_opencls[i].description(buf, sizeof(buf), proc_type_name(PROC_TYPE_AMD_GPU));
        descs.push_back(string(buf));
    }

    // Create descriptions for OpenCL Intel GPUs
    //
    for (i=0; i<intel_gpu_opencls.size(); i++) {
        intel_gpu_opencls[i].description(buf, sizeof(buf), proc_type_name(PROC_TYPE_INTEL_GPU));
        descs.push_back(string(buf));
    }

    // Create descriptions for OpenCL CPUs
    //
    for (i=0; i<cpu_opencls.size(); i++) {
        cpu_opencls[i].description(buf, sizeof(buf));
        descs.push_back(string(buf));
    }

    ati_gpus.clear();
    nvidia_gpus.clear();
    intel_gpus.clear();
    ati_opencls.clear();
    nvidia_opencls.clear();
    intel_gpu_opencls.clear();
    cpu_opencls.clear();

}

// Some dual-GPU laptops (e.g., Macbook Pro) don't 
// power down the more powerful GPU until all
// applications which used them exit.  To save
// battery life, the client launches a second
// instance of the client as a child process to 
// detect and get information about the GPUs.
// The child process writes the info to a temp
// file which our main client then reads.
//
void COPROCS::set_path_to_client(char *path) {
    client_path = path;
    // The path may be relative to the current directory
     boinc_getcwd(client_dir);
}

int COPROCS::write_coproc_info_file(vector<string> &warnings) {
    MIOFILE mf;
    unsigned int i, temp;
    FILE* f;
    
    f = boinc_fopen(COPROC_INFO_FILENAME, "wb");
    if (!f) return ERR_FOPEN;
    mf.init_file(f);
    
    mf.printf("    <coprocs>\n");

    for (i=0; i<ati_gpus.size(); ++i) {
       ati_gpus[i].write_xml(mf, false);
    }
    for (i=0; i<nvidia_gpus.size(); ++i) {
        temp = nvidia_gpus[i].count;
        nvidia_gpus[i].count = 1;
        nvidia_gpus[i].pci_infos[0] = nvidia_gpus[i].pci_info;
        nvidia_gpus[i].write_xml(mf, false);
        nvidia_gpus[i].count = temp;
    }
    for (i=0; i<intel_gpus.size(); ++i) {
        intel_gpus[i].write_xml(mf, false);
    }
    for (i=0; i<ati_opencls.size(); ++i) {
        ati_opencls[i].write_xml(mf, "ati_opencl", true);
    }
    for (i=0; i<nvidia_opencls.size(); ++i) {
        nvidia_opencls[i].write_xml(mf, "nvidia_opencl", true);
    }
    for (i=0; i<intel_gpu_opencls.size(); ++i) {
        intel_gpu_opencls[i].write_xml(mf, "intel_gpu_opencl", true);
    }
    for (i=0; i<cpu_opencls.size(); i++) {
        cpu_opencls[i].write_xml(mf);
    }
    for (i=0; i<warnings.size(); ++i) {
        mf.printf("<warning>%s</warning>\n", warnings[i].c_str());
    }

    mf.printf("    </coprocs>\n");
    fclose(f);
    return 0;
}

int COPROCS::read_coproc_info_file(vector<string> &warnings) {
    MIOFILE mf;
    int retval;
    FILE* f;
    string s;

    COPROC_ATI my_ati_gpu;
    COPROC_NVIDIA my_nvidia_gpu;
    COPROC_INTEL my_intel_gpu;
    OPENCL_DEVICE_PROP ati_opencl;
    OPENCL_DEVICE_PROP nvidia_opencl;
    OPENCL_DEVICE_PROP intel_gpu_opencl;
    OPENCL_CPU_PROP cpu_opencl;

    ati_gpus.clear();
    nvidia_gpus.clear();
    intel_gpus.clear();
    ati_opencls.clear();
    nvidia_opencls.clear();
    intel_gpu_opencls.clear();
    cpu_opencls.clear();

    f = boinc_fopen(COPROC_INFO_FILENAME, "r");
    if (!f) return ERR_FOPEN;
    XML_PARSER xp(&mf);
    mf.init_file(f);
    if (!xp.parse_start("coprocs")) {
        fclose(f);
        return ERR_XML_PARSE;
    }
    
    while (!xp.get_tag()) {
        if (xp.match_tag("/coprocs")) {
            fclose(f);
            return 0;
        }

        if (xp.match_tag("coproc_ati")) {
            retval = my_ati_gpu.parse(xp);
            if (retval) {
                my_ati_gpu.clear();
            } else {
                my_ati_gpu.device_num = (int)ati_gpus.size();
                ati_gpus.push_back(my_ati_gpu);
            }
            continue;
        }
        if (xp.match_tag("coproc_cuda")) {
            retval = my_nvidia_gpu.parse(xp);
            if (retval) {
                my_nvidia_gpu.clear();
            } else {
                my_nvidia_gpu.device_num = (int)nvidia_gpus.size();
                my_nvidia_gpu.pci_info = my_nvidia_gpu.pci_infos[0];
                memset(&my_nvidia_gpu.pci_infos[0], 0, sizeof(struct PCI_INFO));
                nvidia_gpus.push_back(my_nvidia_gpu);
            }
            continue;
        }
        if (xp.match_tag("coproc_intel_gpu")) {
            retval = my_intel_gpu.parse(xp);
            if (retval) {
                my_intel_gpu.clear();
            } else {
                my_intel_gpu.device_num = (int)intel_gpus.size();
                intel_gpus.push_back(my_intel_gpu);
            }
            continue;
        }
        
        if (xp.match_tag("ati_opencl")) {
            memset(&ati_opencl, 0, sizeof(ati_opencl));
            retval = ati_opencl.parse(xp, "/ati_opencl");
            if (retval) {
                memset(&ati_opencl, 0, sizeof(ati_opencl));
            } else {
                ati_opencl.is_used = COPROC_IGNORED;
                ati_opencls.push_back(ati_opencl);
            }
            continue;
        }

        if (xp.match_tag("nvidia_opencl")) {
            memset(&nvidia_opencl, 0, sizeof(nvidia_opencl));
            retval = nvidia_opencl.parse(xp, "/nvidia_opencl");
            if (retval) {
                memset(&nvidia_opencl, 0, sizeof(nvidia_opencl));
            } else {
                nvidia_opencl.is_used = COPROC_IGNORED;
                nvidia_opencls.push_back(nvidia_opencl);
            }
            continue;
        }

        if (xp.match_tag("intel_gpu_opencl")) {
            memset(&intel_gpu_opencl, 0, sizeof(intel_gpu_opencl));
            retval = intel_gpu_opencl.parse(xp, "/intel_gpu_opencl");
            if (retval) {
                memset(&intel_gpu_opencl, 0, sizeof(intel_gpu_opencl));
            } else {
                intel_gpu_opencl.is_used = COPROC_IGNORED;
                intel_gpu_opencls.push_back(intel_gpu_opencl);
            }
            continue;
        }

        if (xp.match_tag("opencl_cpu_prop")) {
            memset(&cpu_opencl, 0, sizeof(cpu_opencl));
            retval = cpu_opencl.parse(xp);
            if (retval) {
                memset(&cpu_opencl, 0, sizeof(cpu_opencl));
            } else {
                cpu_opencl.opencl_prop.is_used = COPROC_IGNORED;
                cpu_opencls.push_back(cpu_opencl);
            }
            continue;
        }
        
        if (xp.parse_string("warning", s)) {
            warnings.push_back(s);
            continue;
        }

        // TODO: parse OpenCL info for CPU when implemented:
        //  gstate.host_info.have_cpu_opencl
        //  gstate.host_info.cpu_opencl_prop
    }
    
    fclose(f);
    return ERR_XML_PARSE;
}

int COPROCS::launch_child_process_to_detect_gpus() {
#ifdef _WIN32
    HANDLE prog;
#else
    int prog;
#endif
    char quoted_data_dir[MAXPATHLEN+2];
    char data_dir[MAXPATHLEN];
    int retval = 0;
    
    retval = boinc_delete_file(COPROC_INFO_FILENAME);
    if (retval) {
        msg_printf(0, MSG_INFO,
            "Failed to delete old %s. error code %d",
            COPROC_INFO_FILENAME, retval
        );
    } else {
        for (;;) {
            if (!boinc_file_exists(COPROC_INFO_FILENAME)) break;
            boinc_sleep(0.01);
        }
    }
    
    boinc_getcwd(data_dir);

#ifdef _WIN32
    strlcpy(quoted_data_dir, "\"", sizeof(quoted_data_dir));
    strlcat(quoted_data_dir, data_dir, sizeof(quoted_data_dir));
    strlcat(quoted_data_dir, "\"", sizeof(quoted_data_dir));
#else
    strlcpy(quoted_data_dir, data_dir, sizeof(quoted_data_dir));
#endif

    if (log_flags.coproc_debug) {
        msg_printf(0, MSG_INFO,
            "[coproc] launching child process at %s",
            client_path
        );
        msg_printf(0, MSG_INFO,
            "[coproc] relative to directory %s",
            client_dir
        );
        msg_printf(0, MSG_INFO,
            "[coproc] with data directory %s",
            quoted_data_dir
        );
    }
            
    int argc = 4;
    char* const argv[5] = {
#ifdef _WIN32
         const_cast<char *>("boinc.exe"), 
#else
         const_cast<char *>("boinc"), 
#endif
         const_cast<char *>("--detect_gpus"), 
         const_cast<char *>("--dir"), 
         const_cast<char *>(quoted_data_dir),
         NULL
    }; 

    chdir(client_dir);
    
    retval = run_program(
        client_dir,
        client_path,
        argc,
        argv, 
        0,
        prog
    );

    chdir(data_dir);
    
    if (retval) {
        if (log_flags.coproc_debug) {
            msg_printf(0, MSG_INFO,
                "[coproc] run_program of child process returned error %d",
                retval
            );
        }
        return retval;
    }

    retval = get_exit_status(prog);
    if (retval) {
        msg_printf(0, MSG_INFO,
            "GPU detection failed. error code %d",
            retval
        );
    }

    return 0;
}
