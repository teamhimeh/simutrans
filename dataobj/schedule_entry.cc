#include "schedule_entry.h"

void schedule_entry_t::push_journey_time(uint32 t)
{
    journey_time[jt_at_index] = t;
    jt_at_index = (jt_at_index+1)%NUM_ARRIVAL_TIME_STORED;
}

void schedule_entry_t::push_waiting_time(uint32 t) {
    waiting_time[wt_at_index] = t;
    wt_at_index = (wt_at_index+1)%NUM_WAITING_TIME_STORED;
}

uint32 schedule_entry_t::get_median_journey_time() const {
    uint8 valid_record_count = 0;
	uint32 valid_records[NUM_ARRIVAL_TIME_STORED];
	for(  uint8 i=0;  i<NUM_ARRIVAL_TIME_STORED;  i++  ) {
		if(  journey_time[i] > 0  ) {
			valid_records[i] = journey_time[i];
			valid_record_count++;
		}
	}
	if(  valid_record_count==0  ) {
		// no valid records
		return 0;
	}
	// get the median value by a simple bubble sort
	for(  uint8 i=0;  i<valid_record_count-1;  i++  ) {
		for(  uint8 j=i+1;  j<valid_record_count;  j++  ) {
			if(  valid_records[i] > valid_records[j]  ) {
				uint32 temp = valid_records[i];
				valid_records[i] = valid_records[j];
				valid_records[j] = temp;
			}
		}
	}
	return valid_records[valid_record_count/2];
}

uint32 schedule_entry_t::get_average_waiting_time() const {
    uint64 sum = 0;
    uint8 count = 0;
    for(  uint8 i=0;  i<NUM_WAITING_TIME_STORED;  i++  ) {
        if(  waiting_time[i] > 0  ) {
            sum += waiting_time[i];
            count++;
        }
    }
    return count > 0 ? (uint32)(sum / count) : 0;
}
