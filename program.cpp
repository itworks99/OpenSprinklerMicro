/* OpenSprinkler AVR/RPI/BBB Library
 * Copyright (C) 2014 by Ray Wang (ray@opensprinkler.com)
 *
 * Program data structures and functions
 * Sep 2014 @ Rayshobby.net
 *
 * This file is part of the OpenSprinkler library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>. 
 */

#include <limits.h>
#include "program.h"

#if !defined(SECS_PER_DAY)
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24UL)
#endif

// Declare static data members
byte ProgramData::nprograms = 0;
ulong ProgramData::scheduled_start_time[(MAX_EXT_BOARDS+1)*8];
ulong ProgramData::scheduled_stop_time[(MAX_EXT_BOARDS+1)*8];
byte ProgramData::scheduled_program_index[(MAX_EXT_BOARDS+1)*8];
LogStruct ProgramData::lastrun;
ulong ProgramData::last_seq_stop_time;

void ProgramData::init() {
	reset_runtime();
  load_count();
}

void ProgramData::reset_runtime() {
  for (byte i=0; i<MAX_NUM_STATIONS; i++) {
    scheduled_start_time[i] = 0;
    scheduled_stop_time[i] = 0;
    scheduled_program_index[i] = 0;
  }
  last_seq_stop_time = 0;
}

// load program count from NVM
void ProgramData::load_count() {
  nprograms = nvm_read_byte((byte *) ADDR_PROGRAMCOUNTER);
}

// save program count to NVM
void ProgramData::save_count() {
  nvm_write_byte((byte *) ADDR_PROGRAMCOUNTER, nprograms);
}

// erase all program data
void ProgramData::eraseall() {
  nprograms = 0;
  save_count();
}

// read a program
void ProgramData::read(byte pid, ProgramStruct *buf) {
  if (pid >= nprograms) return;
  if (0) {
    // todo: handle SD card
  } else {
    unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)pid * PROGRAMSTRUCT_SIZE;
    nvm_read_block((void*)buf, (const void *)addr, PROGRAMSTRUCT_SIZE);  
  }
}

// add a program
byte ProgramData::add(ProgramStruct *buf) {
  if (0) {
    // todo: handle SD card
  } else {
    if (nprograms >= MAX_NUMBER_PROGRAMS)  return 0;
    unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)nprograms * PROGRAMSTRUCT_SIZE;
    nvm_write_block((const void*)buf, (void *)addr, PROGRAMSTRUCT_SIZE);
    nprograms ++;
    save_count();
  }
  return 1;
}

// move a program up (i.e. swap a program with the one above it)
void ProgramData::moveup(byte pid) {
  if(pid >= nprograms || pid == 0) return;

  if(0) {
    // todo: handle SD card
  } else {
    // swap program pid-1 and pid
    unsigned int src = ADDR_PROGRAMDATA + (unsigned int)(pid-1) * PROGRAMSTRUCT_SIZE;
    unsigned int dst = src + PROGRAMSTRUCT_SIZE;
#if defined(ARDUINO) // NVM write for Arduino
    byte tmp;
    for(int i=0;i<PROGRAMSTRUCT_SIZE;i++,src++,dst++) {
      tmp = nvm_read_byte((byte *)src);
      nvm_write_byte((byte *)src, nvm_read_byte((byte *)dst));
      nvm_write_byte((byte *)dst, tmp);
    }
#else // NVM write for RPI/BBB
    ProgramStruct tmp1, tmp2;
    nvm_read_block(&tmp1, (void *)src, PROGRAMSTRUCT_SIZE);
    nvm_read_block(&tmp2, (void *)dst, PROGRAMSTRUCT_SIZE);
    nvm_write_block(&tmp1, (void *)dst, PROGRAMSTRUCT_SIZE);
    nvm_write_block(&tmp2, (void *)src, PROGRAMSTRUCT_SIZE);
#endif // NVM write
  }
}

// modify a program
byte ProgramData::modify(byte pid, ProgramStruct *buf) {
  if (pid >= nprograms)  return 0;
  if (0) {
    // handle SD card
  } else {
    unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)pid * PROGRAMSTRUCT_SIZE;
    nvm_write_block((const void*)buf, (void *)addr, PROGRAMSTRUCT_SIZE);
  }
  return 1;
}

// delete program(s)
byte ProgramData::del(byte pid) {
  if (pid >= nprograms)  return 0;
  if (nprograms == 0) return 0;
  if (0) {
    // handle SD card
  } else {
    ProgramStruct copy;
    unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)(pid+1) * PROGRAMSTRUCT_SIZE;
    // erase by copying backward
    for (; addr < ADDR_PROGRAMDATA + nprograms * PROGRAMSTRUCT_SIZE; addr += PROGRAMSTRUCT_SIZE) {
      nvm_read_block((void*)&copy, (const void *)addr, PROGRAMSTRUCT_SIZE);  
      nvm_write_block((const void*)&copy, (void *)(addr-PROGRAMSTRUCT_SIZE), PROGRAMSTRUCT_SIZE);
    }
    nprograms --;
    save_count();
  }
  return 1;
}

// decode a sunrise/sunset start time to actual start time
int16_t ProgramStruct::starttime_decode(int16_t t) {
  if((t>>15)&1) return -1;
  int16_t offset = t&0x7ff;
  if((t>>STARTTIME_SIGN_BIT)&1) offset = -offset;
  if((t>>STARTTIME_SUNRISE_BIT)&1) { // sunrise time
    t = os.nvdata.sunrise_time + offset;
    if (t<0) t=0; // clamp it to 0 if less than 0
  } else if((t>>STARTTIME_SUNSET_BIT)&1) {
    t = os.nvdata.sunset_time + offset;
    if (t>=1440) t=1439; // clamp it to 1440 if larger than 1440
  }
  return t;
}

// Check if a given time matches program schedule
byte ProgramStruct::check_match(time_t t) {

#if defined(ARDUINO) // get current time from Arduino
  unsigned int hour_t = hour(t);
  unsigned int minute_t = minute(t);
  byte weekday_t = weekday(t);        // weekday ranges from [0,6] within Sunday being 1
  byte day_t = day(t);
  byte month_t = month(t);
#else // get current time from RPI/BBB
  time_t ct = t;
  struct tm *ti = gmtime(&ct);
  unsigned int hour_t = ti->tm_hour;
  unsigned int minute_t = ti->tm_min;
  byte weekday_t = (ti->tm_wday+1)%7;  // tm_wday ranges from [0,6] with Sunday being 0
  byte day_t = ti->tm_mday;
  byte month_t = ti->tm_mon+1;   // tm_mon ranges from [0,11]
#endif // get current time

  unsigned int current_minute = hour_t*60+minute_t;
  
  // check program enable status
  if (!enabled) return 0;
 
  byte wd = (weekday_t+5)%7;
  byte dt = day_t;
  byte i;
  // check day match
  switch(type) {
    case PROGRAM_TYPE_WEEKLY:
      // weekday match
      if (!(days[0] & (1<<wd)))
        return 0;
    break;
    
    case PROGRAM_TYPE_BIWEEKLY:
      // todo
    break;
    
    case PROGRAM_TYPE_MONTHLY:
      if (dt != (days[0]&0b11111))
        return 0;
    break;
    
    case PROGRAM_TYPE_INTERVAL:
      // this is an inverval program
      if (((t/SECS_PER_DAY)%days[1]) != days[0])  return 0;      
    break;
  }

  // check odd/even day restriction
  if (!oddeven) { }
  else if (oddeven == 2) {
    // even day restriction
    if((dt%2)!=0)  return 0;
  } else if (oddeven == 1) {
    // odd day restriction
    // skip 31st and Feb 29
    if(dt==31)  return 0;
    else if (dt==29 && month_t==2)  return 0;
    else if ((dt%2)!=1)  return 0;
  }
  
  // check start time match
  if (!starttime_type) {
    // repeating type
    int16_t start = starttime_decode(starttimes[0]);
    int16_t repeat = starttimes[1];
    int16_t interval = starttimes[2];
    // if current time is prior to start time, return false
    if (current_minute < start)
      return 0;
      
    // if this is a single run program
    if (!repeat) {
      return (current_minute == start) ? 1 : 0;
    }
    
    // if this is a multiple-run program
    // first ensure the interval is non-zero
    if (!interval) {
      return 0;
    }

    // check if we are on any interval match
    int16_t c = (current_minute - start) / interval;
    if ((c * interval == (current_minute - start)) && c <= repeat) {
      return 1;
    }
    
    // check previous day in case the repeating start times went over night
    // this needs to be fixed because we have to check if yesterday
    // is a valid starting day
    /*c = (current_minute - start + 1440) / interval;
    if ((c * interval == (current_minute - start + 1440)) && c <= repeat) {
      return 1;
    }*/
    
  } else {
    // given start time type
    for(i=0;i<MAX_NUM_STARTTIMES;i++) {
      if (current_minute == starttime_decode(starttimes[i]))  return 1;
    }
  }
  return 0;
}

// convert absolute remainder (reference time 1970 01-01) to relative remainder (reference time today)
// absolute remainder is stored in nvm, relative remainder is presented to web
void ProgramData::drem_to_relative(byte days[2]) {
  byte rem_abs=days[0];
  byte inv=days[1];
  // todo: use now_tz()?
  days[0] = (byte)((rem_abs + inv - (os.now_tz()/SECS_PER_DAY) % inv) % inv);
}

// relative remainder -> absolute remainder
void ProgramData::drem_to_absolute(byte days[2]) {
  byte rem_rel=days[0];
  byte inv=days[1];
  // todo: use now_tz()?
  days[0] = (byte)(((os.now_tz()/SECS_PER_DAY) + rem_rel) % inv);
}


