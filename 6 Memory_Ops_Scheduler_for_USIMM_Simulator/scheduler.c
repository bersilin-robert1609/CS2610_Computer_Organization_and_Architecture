#include <stdio.h>
#include "utlist.h"
#include "utils.h"

#include "memory_controller.h"
#include "params.h"

/* Adaptive-Page Policy using 4-bit saturating counter */

#define HI_TH 12
#define LO_TH 7

extern long long int CYCLE_VAL;

/* A data structure to see if a bank is a candidate for precharge. */
int recent_colacc[MAX_NUM_CHANNELS][MAX_NUM_RANKS][MAX_NUM_BANKS];

/* Keeping track of how many preemptive precharges are performed. */
long long int num_aggr_precharge = 0;

/* Initial value of the counter */
int counter = 10;

/* Some scheduler stats */

/* Keeping track of how many page misses in open-page policy */
int misses_open_policy = 0;

/* Keeping track of how many page hits in open-page policy */
int hits_open_policy = 0;

/* Keeping track of how many read/write requests in open-page policy */
int total_rw_open_policy = 0;

/* Keeping track of how many page misses in closed-page policy */
int misses_closed_policy = 0;

/* Keeping track of how many page hits in closed-page policy */
int hits_closed_policy = 0;

/* Keeping track of how many read/write in closed-page policy */
int total_rw_closed_policy = 0;

/* Keeping track of how many times the current policy of the scheduler changes */
int policy_switch_count = 0;

/* Specifies page policy 0 for open page, 1 for closed page */
int policy_mode = 0;

/* The data structure to store the last rows accessed in each bank */
int recently_opened_row[MAX_NUM_CHANNELS][MAX_NUM_RANKS][MAX_NUM_BANKS];

/* The increment and decrement functions for the saturated count*/
void increment_counter() {
	if(counter < 15) {
		counter++;
	}
}
void decrement_counter() {
	if(counter > 0) {
		counter--;
	}
}

void init_scheduler_vars()
{
	// initialize all scheduler variables here
	int i, j, k;
	for (i=0; i<MAX_NUM_CHANNELS; i++) {
	  for (j=0; j<MAX_NUM_RANKS; j++) {
	    for (k=0; k<MAX_NUM_BANKS; k++) {
	      recent_colacc[i][j][k] = 0;
		  recently_opened_row[i][j][k] = -1;
	    }
	  }
	}

	return;
}

// write queue high water mark; begin draining writes if write queue exceeds this value
#define HI_WM 40

// end write queue drain once write queue has this many writes in it
#define LO_WM 20

// 1 means we are in write-drain mode for that channel
int drain_writes[MAX_NUM_CHANNELS];

void schedule(int channel)
{
	request_t * rd_ptr = NULL;
	request_t * wr_ptr = NULL;
	int i, j;

	/* The code to check which queue to use */
	if (drain_writes[channel] && (write_queue_length[channel] > LO_WM)) {
	  drain_writes[channel] = 1; // Keep draining.
	}
	else {
	  drain_writes[channel] = 0; // No need to drain.
	}

	if(write_queue_length[channel] > HI_WM)
	{
		drain_writes[channel] = 1; // change to drain 
	}
	else {
	  if (!read_queue_length[channel])
	    drain_writes[channel] = 1; // change to drain
	}
	/* Queue decided */

	/* The policy switcher */
	if(!policy_mode && counter > HI_TH) {
		policy_switch_count++;
		policy_mode = 1;
	}
	else if(policy_mode && counter < LO_TH) {
		policy_switch_count++;
		policy_mode = 0;
	}
	/* Policy Decided */

	if(policy_mode)
	{ //Closed Policy used

		if(drain_writes[channel])
		{
			LL_FOREACH(write_queue_head[channel], wr_ptr)
			{
				if(wr_ptr->command_issuable)
				{
					if (wr_ptr->next_command == COL_WRITE_CMD) 
					{
						total_rw_closed_policy++;
						if(recently_opened_row[channel][wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank] == wr_ptr->dram_addr.row)
						{// page hit it in closed policy
							hits_closed_policy++;
							decrement_counter();
						}
						else 
						{// page miss
							misses_closed_policy++;
						}
						
						recently_opened_row[channel][wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank] = wr_ptr->dram_addr.row;

						recent_colacc[channel][wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank] = 1; // this bank is now a candidate for closure
					}
					/* Before issuing the command, see if this bank is now a candidate for closure (if it just did a column-rd/wr).
					If the bank just did an activate or precharge, it is not a candidate for closure. */

					if (wr_ptr->next_command == ACT_CMD || wr_ptr->next_command == PRE_CMD) {
						recent_colacc[channel][wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank] = 0;
					}
					issue_request_command(wr_ptr);
					break;
				}
			}
		}

		// Draining Reads
		// Simple FCFS 
		if(!drain_writes[channel])
		{
			LL_FOREACH(read_queue_head[channel],rd_ptr)
			{
				if(rd_ptr->command_issuable)
				{
					if (rd_ptr->next_command == COL_READ_CMD) 
					{
						total_rw_closed_policy++;
						if(recently_opened_row[channel][rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank] == rd_ptr->dram_addr.row)
						{// page hit in closed policy
							hits_closed_policy++;
							decrement_counter();
						}
						else 
						{// page miss
							misses_closed_policy++;
						}

						recently_opened_row[channel][rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank] = rd_ptr->dram_addr.row;

						recent_colacc[channel][rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank] = 1; // this bank is now a candidate for closure
					}
					/* Before issuing the command, see if this bank is now a candidate for closure (if it just did a column-rd/wr).
					If the bank just did an activate or precharge, it is not a candidate for closure. */

					if (rd_ptr->next_command == ACT_CMD || rd_ptr->next_command == PRE_CMD)
						recent_colacc[channel][rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank] = 0;
					
					issue_request_command(rd_ptr);
					break;
				}
			}
		}

		/* If a command hasn't yet been issued to this channel in this cycle, issue a precharge. */
		if (!command_issued_current_cycle[channel]) {
			for (i=0; i<NUM_RANKS; i++) {
				for (j=0; j<NUM_BANKS; j++) {  /* For all banks on the channel.. */
					if (recent_colacc[channel][i][j]) {  /* See if this bank is a candidate. */
						if (is_precharge_allowed(channel,i,j)) {  /* See if precharge is doable. */
							if (issue_precharge_command(channel,i,j)) {
								num_aggr_precharge++;
								recent_colacc[channel][i][j] = 0;
							}
						}
					}
				}
			}
		}
	}
	else 
	{ // Open Policy used
		// Draining Writes
		if(drain_writes[channel])
		{
			LL_FOREACH(write_queue_head[channel], wr_ptr)
			{
				if(wr_ptr->command_issuable)
				{
					if (wr_ptr->next_command == COL_WRITE_CMD) 
					{
						total_rw_open_policy++;
						if(recently_opened_row[channel][wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank] != wr_ptr->dram_addr.row)
						{// If page miss occurs in open policy
							misses_open_policy++;
							increment_counter();
						}
						else 
						{// page hit occurs
							hits_open_policy++;
						}

						recently_opened_row[channel][wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank] = wr_ptr->dram_addr.row;
					}					
					issue_request_command(wr_ptr);
					break;
				}
			}
			return;
		}

		// Draining Reads
		// Simple FCFS 
		if(!drain_writes[channel])
		{
			LL_FOREACH(read_queue_head[channel],rd_ptr)
			{
				if(rd_ptr->command_issuable)
				{
					if (rd_ptr->next_command == COL_READ_CMD) 
					{
						total_rw_open_policy++;
						if(recently_opened_row[channel][rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank] != rd_ptr->dram_addr.row)
						{// If page miss occurs in open policy
							misses_open_policy++;
							increment_counter();
						}
						else 
						{// page hit occurs
							hits_open_policy++;
						}

						recently_opened_row[channel][rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank] = rd_ptr->dram_addr.row;
					}
					issue_request_command(rd_ptr);
					break;
				}
			}
			return;
		}
	}
}

void scheduler_stats()
{
  printf("Number of aggressive precharges: %lld\n", num_aggr_precharge);
  printf("The number of policy switches is %d\n", policy_switch_count);
  printf("The percentage of hits while in open policy mode is %lf\n", ((double)hits_open_policy*(100.0)/total_rw_open_policy));
  printf("The percentage of misses while in open policy mode is %lf\n", ((double)misses_open_policy*(100.0)/total_rw_open_policy));
  printf("The percentage of hits while in closed policy mode is %lf\n", ((double)hits_closed_policy*(100.0)/total_rw_closed_policy));
  printf("The percentage of misses while in closed policy mode is %lf\n", ((double)misses_closed_policy*(100.0)/total_rw_closed_policy));
}
