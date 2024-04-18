#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <vector>
#include <iostream>
#include "core.h"
#include "workloads.h"

extern uint64_t  DCACHE_ASSOC;
extern uint64_t  DCACHE_SIZE;
extern uint64_t CACHE_LINESIZE;

extern uint64_t cycle;
extern bool WORKLOAD;

uint64_t access_count = 0;
uint64_t workload_displace = 0;
uint64_t rewrite_display = 0;
uint64_t attack_cacheline_index = 0;

bool attack_sweep = false;
extern void die_message(const char* msg);


using namespace std;


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

Core* core_new(Memsys* memsys, char* trace_fname, uint32_t core_id){
	Core* c = (Core*)calloc(1, sizeof(Core));
	c->core_id = core_id;
	c->memsys  = memsys;

	strcpy(c->trace_fname, trace_fname);
	core_init_trace(c);
	core_read_trace(c);

	return c;
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void core_init_trace(Core* c){
	char command_string[512];
	sprintf(command_string,"gunzip -c %s", c->trace_fname);
	if ((c->trace = popen(command_string, "r")) == NULL) {
		printf("Command string is %s\n", command_string);
		die_message("Unable to open the file with gzip option\n");
	}

}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void core_cycle(Core* c){

	// cout << "A\n";
	attack_sweep = false;
	static uint64_t iterator = 0;
	if (c->done) {
		return;
	}

	#if CacheTiempo
		decr_ctr_inv(c->memsys);
	#endif


	//if core is snoozing on DRAM hits, return ..
	if (cycle <= c->snooze_end_cycle) {
		return;
	}

	//Covert Channel

	c->inst_count++;


	uint32_t ifetch_delay=0, ld_delay=0, bubble_cycles=0;
	
	Addr temp_lineaddr;

	if (access_count < ACCESS_PATTERN1)
	{
		// c->trace_inst_addr = (Addr) ((access_count / DCACHE_ASSOC) + TAG1_ACCESS_PATTERN1);
		c->trace_inst_addr = (Addr) access_count * CACHE_LINESIZE;
		ifetch_delay = memsys_access(c->memsys, c->trace_inst_addr, ACCESS_TYPE_IFETCH, c->core_id);
		access_count++;
	}
	else if ((access_count >= ACCESS_PATTERN1) && (cycle - (DCACHE_SIZE/CACHE_LINESIZE)) < 10000)
	{
		if(WORKLOAD == 0)
		{
			// c->trace_inst_addr = workload1[access_count - ACCESS_PATTERN1] * CACHE_LINESIZE;
			c->trace_inst_addr = workload1[iterator] * CACHE_LINESIZE;
		}
		else if (WORKLOAD == 1)
		{
			// c->trace_inst_addr = workload2[access_count - ACCESS_PATTERN1] * CACHE_LINESIZE;
			c->trace_inst_addr = workload2[iterator] * CACHE_LINESIZE;
		}
		iterator++;
		ifetch_delay = memsys_access(c->memsys, c->trace_inst_addr, ACCESS_TYPE_IFETCH, c->core_id);
		if (ifetch_delay > 1) {
			bubble_cycles += (ifetch_delay-1);
			workload_displace++;
		}

	}
	else if ((cycle - (DCACHE_SIZE/CACHE_LINESIZE) >= 10000) && attack_cacheline_index < (DCACHE_SIZE/CACHE_LINESIZE))
	{
		if(attack_cacheline_index == 0)
		{
			cout << "Cycle count = " << cycle - DCACHE_SIZE/CACHE_LINESIZE << endl;
		}
		attack_sweep = true;
		// c->trace_inst_addr = (Addr) ((access_count - ACCESS_PATTERN1 - ACCESS_PATTERN2) / DCACHE_ASSOC);
		c->trace_inst_addr = (Addr) (attack_cacheline_index * CACHE_LINESIZE);
		attack_cacheline_index++;
		ifetch_delay = memsys_access(c->memsys, c->trace_inst_addr, ACCESS_TYPE_IFETCH, c->core_id);
		if (ifetch_delay > 1) {
			rewrite_display++;
			bubble_cycles += (ifetch_delay-1);
		}
		
	}
	//if(access_count == ACCESS_PATTERN1 + ACCESS_PATTERN2)
	if (attack_cacheline_index == (DCACHE_SIZE/CACHE_LINESIZE))
	{
		cout << "Iterator = " << iterator << endl;
		c->done = true;
		c->done_inst_count = c->inst_count;
		c->done_cycle_count = cycle;
		cout << endl << "Workload Displace" << " " << workload_displace << endl;
		cout << "Rewrite displace" << " " << rewrite_display << endl;
	}

	// ifetch_delay = memsys_access(c->memsys, c->trace_inst_addr, ACCESS_TYPE_IFETCH, c->core_id);
	// if (ifetch_delay > 1) {
	// 	bubble_cycles += (ifetch_delay-1);
	// }

	// if(access_count < ACCESS_PATTERN1)
	// {
	// 	bubble_cycles = 0;
	// }

	// if (c->trace_inst_type == INST_TYPE_LOAD) {
	// 	ld_delay = memsys_access(c->memsys, c->trace_ldst_addr, ACCESS_TYPE_LOAD, c->core_id);
	// }
	// if (ld_delay > 1) {
	// 	bubble_cycles += (ld_delay-1);
	// }

	// if (c->trace_inst_type == INST_TYPE_STORE){
	// 	memsys_access(c->memsys, c->trace_ldst_addr, ACCESS_TYPE_STORE, c->core_id);
	// }
	//No bubbles for store misses

	if (bubble_cycles) {
		c->snooze_end_cycle = (cycle+bubble_cycles);
	}

	//core_read_trace(c);
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void core_read_trace (Core* c){
	uint32_t tmp;
	tmp = fread(&c->trace_inst_addr, 4, 1, c->trace);
	tmp = fread(&c->trace_inst_type, 1, 1, c->trace);
	tmp = fread(&c->trace_ldst_addr, 4, 1, c->trace);

	if (feof(c->trace)) {
		c->done = true;
		c->done_inst_count  = c->inst_count;
		c->done_cycle_count = cycle;
	}

}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

void core_print_stats(Core* c){
	char header[256];
	double ipc = (double)(c->done_inst_count) / (double)(c->done_cycle_count);
	sprintf(header, "CORE_%01d", c->core_id);

	printf("\n");
	printf("\n%s_INST         \t\t : %10llu", header,  c->done_inst_count);
	printf("\n%s_CYCLES       \t\t : %10llu", header,  c->done_cycle_count);
	printf("\n%s_IPC          \t\t : %10.3f", header,  ipc);

	pclose(c->trace);
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
