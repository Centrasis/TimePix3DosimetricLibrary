#ifdef __SIMULATION__
#pragma once
//Header that only declares all used libkatherine types to obtain compatibility
#include <KatherineTpx3.h>
#include "MultiplattformTypes.h"
#include <unistd.h>
#include <functional>

//Empty Def
typedef struct katherine_device {
	void* control_socket;
	void* data_socket;
} katherine_device_t;

PACK(
typedef struct katherine_coord {
	uint8_t x;
	uint8_t y;
}) katherine_coord_t;

PACK(
typedef struct katherine_px_f_toa_tot {
	katherine_coord coord;
	uint8_t ftoa;
	uint64_t toa;
	uint16_t tot;

	katherine_px_f_toa_tot()
	{
		coord.y = 0;
		coord.x = 0;
		toa = 0;
		tot = 0;
		ftoa = 0;
	}

	inline operator bool()
	{
		return IsValid();
	};

	katherine_px_f_toa_tot(uint8_t x, uint8_t y, uint16_t tot = 0, uint64_t toa = 0, uint8_t ftoa = 0)
	{
		this->coord.x = x;
		this->coord.y = y;
		this->ftoa = ftoa;
		this->toa = toa;
		this->tot = tot;
	}

	bool IsValid()
	{
		if (this == NULL)
			return false;

		return !(tot == 0 && ftoa == 0 && toa == 0);
	}
}) katherine_px_f_toa_tot_t;


typedef struct katherine_px_toa_tot {
	katherine_coord coord;
	uint64_t toa;
	uint8_t hit_count;
	uint16_t tot;
} katherine_px_toa_tot_t;

typedef struct katherine_px_f_toa_only {
	katherine_coord coord;
	uint8_t ftoa;
	uint64_t toa;
} katherine_px_f_toa_only_t;

typedef struct katherine_px_toa_only {
	katherine_coord coord;
	uint64_t toa;
	uint8_t hit_count;
} katherine_px_toa_only_t;

typedef struct katherine_px_f_event_itot {
	katherine_coord coord;
	uint8_t hit_count;
	uint16_t event_count;
	uint16_t integral_tot;
} katherine_px_f_event_itot_t;

typedef struct katherine_px_event_itot {
	katherine_coord coord;
	uint16_t event_count;
	uint16_t integral_tot;
} katherine_px_event_itot_t;

typedef struct katherine_frame_info {
	uint64_t received_pixels;
	uint64_t sent_pixels;
	uint64_t lost_pixels;

	union {
		struct {
			uint32_t msb, lsb;
		} b;
		uint64_t d;
	} start_time;

	union {
		struct {
			uint32_t msb, lsb;
		} b;
		uint64_t d;
	} end_time;
} katherine_frame_info_t;

typedef struct katherine_acquisition_handlers {
	std::function<void(const void * pixels, size_t count)> pixels_received;
	void(*frame_started)(int);
	void(*frame_ended)(int, bool, const katherine_frame_info *);
} katherine_acquisition_handlers_t;

typedef enum katherine_readout_type {
	READOUT_SEQUENTIAL = 0,
	READOUT_DATA_DRIVEN = 1
} katherine_readout_type_t;

typedef enum katherine_acquisition_state {
	ACQUISITION_NOT_STARTED = 0,
	ACQUISITION_RUNNING = 1,
	ACQUISITION_SUCCEEDED = 2,
	ACQUISITION_TIMED_OUT = 3,
	ACQUISITION_ABORTED = 4
} katherine_acquisition_state_t;

typedef struct katherine_acquisition {
	katherine_device *device;

	char state;
	char readout_mode;
	char acq_mode;
	bool fast_vco_enabled;

	void *md_buffer;
	size_t md_buffer_size;

	void *pixel_buffer;
	size_t pixel_buffer_size;
	size_t pixel_buffer_valid;
	size_t pixel_buffer_max_valid;

	int requested_frames;
	int completed_frames;
	size_t dropped_measurement_data;

	katherine_acquisition_handlers handlers;
	katherine_frame_info current_frame_info;

	uint64_t last_toa_offset;

} katherine_acquisition_t;
#endif