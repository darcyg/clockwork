/*
  Copyright (C) 2012 Martin Leadbeater, Michael O'Connor

  This file is part of Latproc

  Latproc is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  
  Latproc is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Latproc; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "IOComponent.h"
#include <string>
#include <iostream>
#include <iomanip>
#include "MachineInstance.h"
#include "MessagingInterface.h"

std::list<IOComponent *> IOComponent::processing_queue;
std::map<std::string, IOAddress> IOComponent::io_names;
static uint64_t current_time;
size_t IOComponent::process_data_size = 0;
uint8_t *IOComponent::process_data = 0;
uint8_t *IOComponent::process_mask = 0;
int IOComponent::outputs_waiting = 0;
static uint8_t *last_process_data = 0;

unsigned int IOComponent::max_offset = 0;
unsigned int IOComponent::min_offset = 1000000L;
static std::vector<IOComponent*> *indexed_components = 0;

IOComponent::DeviceList IOComponent::devices;

IOComponent::IOComponent(IOAddress addr) 
		: last_event(e_none), address(addr), io_index(-1) { 
	processing_queue.push_back(this); 
	// the io_index is the bit offset of the first bit in this objects address space
	io_index = addr.io_offset*8 + addr.io_bitpos;
	
}

IOComponent::IOComponent() : last_event(e_none), io_index(-1) { 
	processing_queue.push_back(this); 
	// use the same io-updated index as the processing queue position
}

// most io devices set their initial state to reflect the 
// current hardware state
// output devices reset to an 'off' state
void IOComponent::setInitialState() {
}

IOComponent* IOComponent::lookup_device(const std::string name) {
    IOComponent::DeviceList::iterator device_iter = IOComponent::devices.find(name);
    if (device_iter != IOComponent::devices.end())
        return (*device_iter).second;
    return 0;
}

// these components need to synchronise with clockwork
std::set<IOComponent*> updatedComponents;

static void display(uint8_t *p, unsigned int count = 0) {
	int max = IOComponent::getMaxIOOffset();
	int min = IOComponent::getMinIOOffset();
	if (count == 0)
		for (int i=min; i<=max; ++i) 
			std::cout << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)p[i];
	else
		for (unsigned int i=0; i<count; ++i) 
			std::cout << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)p[i];
	std::cout << std::dec;
}

void IOComponent::processAll(size_t data_size, uint8_t *mask, uint8_t *data) {
	// receive process data updates and mask to yield updated components
    struct timeval now;
    gettimeofday(&now, NULL);
    current_time = now.tv_sec * 1000000 + now.tv_usec;


#if 0
	std::cout << "IOComponent::processAll()\n";
	std::cout << "size: " << data_size << "\n";
	std::cout << "data: "; display( data); std::cout << "\n";
	std::cout << "mask: "; display( mask); std::cout << "\n";
#endif

	assert(data_size == process_data_size);
	// step through the incoming mask and update bits in process data
	uint8_t *p = data;
	uint8_t *m = mask;
	uint8_t *q = process_data;
	for (unsigned int i=0; i<process_data_size; ++i) {
		if (!last_process_data) {
			if (*m) notifyComponentsAt(i);
		}
		if (*p != *q && *m) { // copy masked bits if any
			uint8_t bitmask = 0x01;
			int j = 0;
			// check each bit against the mask and if the mask if
			// set, check if the bit has changed. If the bit has
			// changed, notify components that use this bit and 
			// update the bit
			while (bitmask) {
				if ( *m & bitmask) {
					//std::cout << "looking up " << i << ":" << j << "\n";
					IOComponent *ioc = (*indexed_components)[ i*8+j ];
					if (!ioc) std::cout << "no component at " << i << ":" << j << " found\n"; else std::cout << "found " << ioc->io_name << "\n";
					if (ioc && ioc->last_event != e_none) { 
						// pending locally sourced change on this io
						std::cout << " adding " << ioc->io_name << " due to event " << ioc->last_event << "\n";
						updatedComponents.insert(ioc);
					}
					else if ( (*p & bitmask) != (*q & bitmask) ) {
						// remotely source change on this io
						if (ioc) {
							std::cout << " adding " << ioc->io_name << " due to bit change\n";
							updatedComponents.insert(ioc);
						}

						if (*p & bitmask) *q |= bitmask; 
						else *q &= (uint8_t)(0xff - bitmask);
					}
					//else { 
					//	std::cout << "no change " << (unsigned int)*p << " vs " << 
					//		(unsigned int)*q << "\n";}
				}
				bitmask = bitmask << 1;
				++j;
			}
		}
		++p; ++q; ++m;
	}
	
	// save the domain data for the next check
	if (last_process_data) delete last_process_data;
	last_process_data = new uint8_t[max_offset+1];
	memcpy(last_process_data + min_offset, process_data, max_offset - min_offset + 1);

    
#ifdef USE_EXPERIMENTAL_IDLE_LOOP
	std::set<IOComponent*>::iterator iter = updatedComponents.begin();
	while (iter != updatedComponents.end()) {
		IOComponent *ioc = *iter++;
		std::cout << "processing " << ioc->io_name << "\n";
		ioc->read_time = current_time;
		//if (ioc->last_event == e_none) 
			updatedComponents.erase(ioc); 
		//else std::cout << "still waiting for " << ioc->io_name << " event: " << ioc->last_event << "\n";
		ioc->idle();
	}
#else
	std::list<IOComponent *>::iterator iter = processing_queue.begin();
	while (iter != processing_queue.end()) {
		IOComponent *ioc = *iter++;
		ioc->read_time = current_time;
		ioc->idle();
	}
#endif
}

IOAddress IOComponent::add_io_entry(const char *name, unsigned int module_pos, 
		unsigned int io_offset, unsigned int bit_offset, 
		unsigned int entry_pos, unsigned int bit_len){
	IOAddress addr(module_pos, io_offset, bit_offset, entry_pos, bit_len);
	addr.module_position = module_pos;
	addr.io_offset = io_offset;
	addr.io_bitpos = bit_offset;
    addr.entry_position = entry_pos;
	addr.description = name;
	if (io_names.find(std::string(name)) != io_names.end())  {
		std::cerr << "IOComponent::add_io_entry: warning - an IO component named " 
			<< name << " already existed\n";
	}
	io_names[name] = addr;
	return addr;
}

void IOComponent::add_publisher(const char *name, const char *topic, const char *message) {
    MQTTTopic mqt(topic, message);
    mqt.publisher = true;
//    pubs[name] = mqt;
}

void IOComponent::add_subscriber(const char *name, const char *topic) {
    MQTTTopic mqt(topic, "");
    mqt.publisher = false;
//    subs[name] = mqt;
}


std::ostream &IOComponent::operator<<(std::ostream &out) const{
	out << '[' << address.description<<" "
		<<address.module_position << " "
		<< address.io_offset << ':' 
		<< address.io_bitpos << "." 
		<< address.bitlen << "]=" 
		<< address.value;
	return out;
}

std::ostream &operator<<(std::ostream&out, const IOComponent &ioc) {
	return ioc.operator<<(out);
}

#ifdef EC_SIMULATOR
unsigned char mem[1000];
#define EC_READ_BIT(offset, bitpos) ( (*offset) & (1 << (bitpos)) )
#define EC_WRITE_BIT(offset, bitpos, val) *(offset) |=  ( 1<< (bitpos))
#define EC_READ_S8(offset) 0
#define EC_READ_S16(offset) 0
#define EC_READ_S32(offset) 0
#define EC_READ_U8(offset) 0
#define EC_READ_U16(offset) 0
#define EC_READ_U32(offset) 0
#define EC_WRITE_U8(offset, val) 0
#define EC_WRITE_U16(offset, val) 0
#define EC_WRITE_U32(offset, val) 0
#endif

/*
unsigned int get_bits(uint8_t *offset, unsigned int bitpos, unsigned int bitlen)
{
	uint32_t val=0;
	int n = bitlen;
	if (n>32) n = 32;
	if (bitlen == 1) {
		val = EC_READ_BIT(offset, bitpos);
	}
	else if (bitlen == 8) 
		val = EC_READ_U8(offset);
	else if (bitlen == 16) 
		val = EC_READ_U16(offset);
	else if (bitlen == 32) 
		val = EC_READ_U32(offset);
    else {
		uint8_t mask=1<<(bitpos%8);
		while (n-- > 0) {
			val = val<<1;
			if (*offset & mask) ++val;
			mask = mask >> 1;
			if (mask == 0) {
				mask=1<<7;
				++offset;
			}
		}
	}
	return val;
}
*/
void set_bits(uint8_t *offset, unsigned int bitpos, unsigned int bitlen, uint32_t value)
{
    
}

int32_t IOComponent::filter(int32_t val) {
    return val;
}

class InputFilterSettings {
public:
    bool property_changed;
    uint32_t noise_tolerance; // filter out changes with +/- this range
    LongBuffer positions;
    uint32_t last_sent; // this is the value to send unless the read value moves away from the mean
    uint16_t buffer_len;
    
    InputFilterSettings() :property_changed(true), noise_tolerance(12), positions(8), last_sent(0), buffer_len(4) { }
};

AnalogueInput::AnalogueInput(IOAddress addr) : IOComponent(addr) { 
	config = new InputFilterSettings();
}
int32_t AnalogueInput::filter(int32_t raw) {
    if (config->property_changed) {
        config->property_changed = false;
    }
    config->positions.append(raw);
    uint32_t mean = (config->positions.average(8) + 0.5f);
    if ( (uint32_t)abs(mean - config->last_sent) > config->noise_tolerance) {
        config->last_sent = mean;
    }
    return config->last_sent;
}

void AnalogueInput::update() {
    config->property_changed = true;
}

Counter::Counter(IOAddress addr) : IOComponent(addr) { }

CounterRate::CounterRate(IOAddress addr) : IOComponent(addr), times(16), positions(16) {
    struct timeval now;
    gettimeofday(&now, 0);
    start_t = now.tv_sec * 1000000 + now.tv_usec;
}

int32_t CounterRate::filter(int32_t val) {
    return IOComponent::filter(val);
/* disabled since this is now implemented at the MachineInstance level
    position = val;
    struct timeval now;
    gettimeofday(&now, 0);
    uint64_t now_t = now.tv_sec * 1000000 + now.tv_usec;
    uint64_t delta_t = now_t - start_t;
    times.append(delta_t);
    positions.append(val);
    if (positions.length() < 4) return 0;
    //float speed = positions.difference(positions.length()-1, 0) / times.difference(times.length()-1,0) * 1000000;
    float speed = positions.slopeFromLeastSquaresFit(times) * 250000;
    return speed;
 */
}

/* The Speed Controller class */

/*
    the user provides settings that the controller uses in its
    calculations. The main input from the user is a speed set point and
    this is set via the setValue() method used by other IOComponents.
    Clockwork has a spefic test for this class and after updating properties
    on this class an update() is called.
 */

class PID_Settings {
public:
    bool property_changed;
    uint32_t max_forward;
    uint32_t max_reverse;
    float Kp;
    uint32_t set_point;
    
	float estimated_speed;
	float Pe; 		// used for process error: SetPoint - EstimatedSpeed;
	float current; 	// current power level this is our PV (control variable)
	float last_pos;
	uint64_t position;
	uint64_t measure_time;
	uint64_t last_time;
	float last_power;
    uint32_t slow_speed;
    uint32_t full_speed;
    uint32_t stopping_time;
    uint32_t stopping_distance;
    uint32_t tolerance;
    uint32_t min_update_time;
    uint32_t deceleration_allowance;
    float deceleration_rate;

    PID_Settings() : property_changed(true),
        max_forward(16383),
        max_reverse(-16383),
        Kp(20.0),
        set_point(0),
        estimated_speed(0.0f),
        Pe(0.0f),
        current(0),
        last_pos(0.0f),
        position(0),
        measure_time(0),
        last_time(0),
        last_power(0),
        slow_speed(200),
        full_speed(1000),
        stopping_time(300),
        stopping_distance(2000),
        tolerance(10),
        min_update_time(20),
        deceleration_allowance(20),
        deceleration_rate(0.8f)
    { }
};

int32_t PIDController::filter(int32_t raw) {
    return raw;
}

PIDController::PIDController(IOAddress addr) : Output(addr), config(0) {
    config = new PID_Settings();
}

PIDController::~PIDController() {
    delete config;
}

void PIDController::update() {
    config->property_changed = true;
}

void PIDController::idle() {
    //calculate..
    
    if (config->property_changed) {
        config->property_changed = false;
    }
    
    if (last_event == e_change) {
        //config->
    }
    
    IOComponent::idle();
}

/* ---------- */


const char *IOComponent::getStateString() {
	if (last_event == e_on) return "turning_on";
	else if (last_event == e_off) return "turning_off";
	else if (last_event == e_change) return "changing";
	else if (address.bitlen > 1) return "stable";
	else if (address.value == 0) return "off"; else return "on";
}

std::vector< std::list<IOComponent*> *>io_map;

int IOComponent::getMinIOOffset() {
	return min_offset;
}

int IOComponent::getMaxIOOffset() {
	return max_offset;
}

int IOComponent::notifyComponentsAt(unsigned int offset) {
	assert(offset <= max_offset && offset >= min_offset);
	int count = 0;
	std::list<IOComponent *> *cl = io_map[offset];
	if (cl) {
		//std::cout << "component list at offset " << offset << " size: " << cl->size() << "\n";
		std::list<IOComponent*>::iterator items = cl->begin();
		//std::cout << "items: \n";
		//while (items != cl->end()) {
		//	IOComponent *c = *items++;
			//std::cout << c->io_name << "\n";
		//}
		//items = cl->begin();
		while (items != cl->end()) {
			IOComponent *c = *items++;
			if (c) {
			//std::cout << "notifying " << c->io_name << "\n";
			updatedComponents.insert(c);
			++count;
			}
			//else { std::cout << "Warning: null item detected in io list at offset " << offset << "\n"; }
		}
	}
	return count;
}

bool IOComponent::hasUpdates() {
	return !updatedComponents.empty();
}

uint8_t *generateProcessMask() {
	unsigned int max = IOComponent::getMaxIOOffset();
	unsigned int min = IOComponent::getMinIOOffset();
	// process data size
	uint8_t *res = new uint8_t[max+1];
	memset(res, 0, max+1);

	IOComponent::Iterator iter = IOComponent::begin();
	while (iter != IOComponent::end()) {
		IOComponent *ioc = *iter++;
		unsigned int offset = ioc->address.io_offset;
		unsigned int bitpos = ioc->address.io_bitpos;
		offset += bitpos/8;
		bitpos = bitpos % 8;
		uint8_t mask = 0x01 << bitpos;
		// set  a bit in the mask for each bit of this value
		for (unsigned int i=0; i<ioc->address.bitlen; ++i) {
			res[offset] |= mask;
			mask = mask << 1;
			if (!mask) {mask = 0x01; ++offset; }
		}
	}
	return res;
}

uint8_t *generateMask() {
	//  returns null if there are no updates, otherwise returns
	// a mask for the update data

	//std::cout << "generating mask\n";
	if (updatedComponents.empty()) return 0;

	unsigned int max = IOComponent::getMaxIOOffset();
	unsigned int min = IOComponent::getMinIOOffset();
	uint8_t *res = new uint8_t[max+1];
	memset(res, 0, max+1);
	//std::cout << "mask is " << (max+1) << " bytes\n";

	std::set<IOComponent*>::iterator iter = updatedComponents.begin();
	while (iter != updatedComponents.end()) {
		IOComponent *ioc = *iter++;
		unsigned int offset = ioc->address.io_offset;
		unsigned int bitpos = ioc->address.io_bitpos;
		offset += bitpos/8;
		bitpos = bitpos % 8;
		uint8_t mask = 0x01 << bitpos;
		// set  a bit in the mask for each bit of this value
		for (unsigned int i=0; i<ioc->address.bitlen; ++i) {
			res[offset] |= mask;
			mask = mask << 1;
			if (!mask) {mask = 0x01; ++offset; }
		}
	}
	//std::cout << "generated mask: "; display(res); std::cout << "\n";
	return res;
}

IOUpdate *IOComponent::getUpdates() {
	outputs_waiting = 0; // reset work indicator flag
	uint8_t *mask = generateMask();
	if (!mask) {
		//std::cout << " no changes detected\n";
		return 0;
	}
	IOUpdate *res = new IOUpdate;
	res->size = max_offset - min_offset + 1;
	res->data = getProcessData();
	res->mask = mask;
	//std::cout << "preparing to send " << res->size << ":"; display(res->data); 
	//std::cout << ":"; display(res->mask);
	//std::cout << "\n";
	return res;
}


void IOComponent::setupIOMap() {
	max_offset = 0;
	min_offset = 1000000L;

	if (indexed_components) delete indexed_components;
	//std::cout << "\n\n\n";
	// find the highest and lowest offset location within the process data
	std::list<IOComponent *>::iterator iter = processing_queue.begin();
	while (iter != processing_queue.end()) {
		IOComponent *ioc = *iter++;
		//std::cout << ioc->getName() << " " << ioc->address << "\n";
		unsigned int offset = ioc->address.io_offset;
		unsigned int bitpos = ioc->address.io_bitpos;
		offset += bitpos/8;
		bitpos = bitpos / 8;
		if (ioc->address.bitlen>=8) offset += ioc->address.bitlen/8 - 1;
		if (offset > max_offset) max_offset = offset;
		if (offset < min_offset) min_offset = offset;
	}
	std::cout << "min io offset: " << min_offset << "\n";
	std::cout << "max io offset: " << max_offset << "\n";
	std::cout << ( (max_offset+1)*8*sizeof(IOComponent*)) << " bytes reserved for index io\n";

	indexed_components = new std::vector<IOComponent*>( (max_offset+1) *8);

	process_data_size = max_offset+1;
	process_data = new uint8_t[process_data_size];

	io_map.resize(max_offset+1);
	for (unsigned int i=0; i<=max_offset; ++i) io_map[i] = 0;
	iter = processing_queue.begin();
	while (iter != processing_queue.end()) {
		IOComponent *ioc = *iter++;
		if (ioc->io_name == "") continue;
		unsigned int offset = ioc->address.io_offset;
		unsigned int bitpos = ioc->address.io_bitpos;
		offset += bitpos/8;
		int bytes = 1;
		(*indexed_components)[offset*8 + bitpos] = ioc;
		for (int i=0; i<bytes; ++i) {
			std::list<IOComponent *> *cl = io_map[offset+i];
			if (!cl) cl = new std::list<IOComponent *>();
			cl->push_back(ioc);
			io_map[offset+i] = cl;
			std::cout << "offset: " << (offset+i) << " io name: " << ioc->io_name << "\n";
		}
	}
	std::cout << "\n\n\n";
	process_mask = generateProcessMask();
}


void IOComponent::idle() {
	MachineInstance *self = dynamic_cast<MachineInstance*>(this);
	assert(process_data);
	//std::cout << io_name << "::idle() " << last_event << "\n";
	uint8_t *offset = process_data + address.io_offset;
	int bitpos = address.io_bitpos;
	offset += (bitpos / 8);
	bitpos = bitpos % 8;
/*
	while (bitpos>=8) { 
		++offset;
		bitpos-=8;
	}
*/

	if (address.bitlen == 1) {
		int32_t value = EC_READ_BIT(offset, bitpos);

		// only outputs will have an e_on or e_off event queued, 
		// if they do, set the bit accordingly, ignoring the previous value
		if (!value && last_event == e_on) {
			EC_WRITE_BIT(offset, bitpos, 1);			
		}
		else if (value &&last_event == e_off) {
			EC_WRITE_BIT(offset, bitpos, 0);
		}
		else {
			last_event = e_none;
			const char *evt;
			if (address.value != value) 
			{
				std::list<MachineInstance*>::iterator iter;
#ifndef DISABLE_LEAVE_FUNCTIONS
				if (self && self->enabled()) {
					if (address.value) evt ="on_leave";
					else evt = "off_leave";
					iter = depends.begin();
					while (iter != depends.end()) {
						MachineInstance *m = *iter++;
						Message msg(evt);
						if (m->receives(msg, this)) m->execute(msg, this);
					}
				}
#endif
  			if (self && self->enabled()) {              
					if (value) evt = "on_enter";
					else evt = "off_enter";
					iter = depends.begin();
					while (iter != depends.end()) {
						MachineInstance *m = *iter++;
						Message msg(evt);
						m->execute(msg, this);
					}
				}
			}
			address.value = value;
		}
	}
	else {
		if (last_event == e_change) {
			if (address.bitlen == 8) {
				EC_WRITE_U8(offset, (uint8_t)(pending_value % 256));
			}
			else if (address.bitlen == 16) {
				uint16_t x = pending_value % 65536;
				EC_WRITE_U16(offset, x);
			}
			else if (address.bitlen == 32) {
				EC_WRITE_U32(offset, pending_value);
			}
			last_event = e_none;
			address.value = pending_value;
		}
		else {
			std::cout << io_name << " object of size " << address.bitlen << " val: ";
			display(offset, 1);
			std:: cout << " bit pos: " << bitpos << " ";
			int32_t val = 0;
			if (address.bitlen < 8)  {
				uint8_t bitmask = 0x8 >> bitpos;
				val = 0;
				for (unsigned int count = 0; count < address.bitlen; ++count) {
					val = val + ( *offset & bitmask );
					bitmask = bitmask >> 1;
					// check for objects traversing byte boundaries
					if (count< address.bitlen && !bitmask) { bitmask = 0x80; ++offset; }
				}
				
				while (bitmask) { val = val >> 1; bitmask = bitmask >> 1; }
				std::cout << " value: " << val << "\n";
			}
			else if (address.bitlen == 8) 
				val = EC_READ_S8(offset);
			else if (address.bitlen == 16) 
				val = EC_READ_S16(offset);
			else if (address.bitlen == 32) 
				val = EC_READ_S32(offset);
			else {
				val = 0;
			}

			//if (val) {for (int xx = 0; xx<4; ++xx) { std::cout << std::setw(2) << std::setfill('0') 
			//	<< std::hex << (int)*((uint8_t*)(offset+xx));
			//  << ":" << std::dec << val <<" "; }
			if (address.value != val || strcmp(type(), "CounterRate") == 0) {
				//address.value = get_bits(offset, bitpos, address.bitlen);
				address.value = filter(val);
				if (self && !self->enabled()) return;
				last_event = e_none;
				const char *evt = "property_change";
				std::list<MachineInstance*>::iterator iter = depends.begin();
				while (iter != depends.end()) {
					MachineInstance *m = *iter++;
					Message msg(evt);
					m->execute(msg, this);
				}
			}
		}
	}
}

void IOComponent::turnOn() { 
	std::cout << "Turning on " << address.io_offset << ':' << address.io_bitpos << "\n";
}
void IOComponent::turnOff() { 
	std::cout << "Turning off " << address.io_offset << ':' << address.io_bitpos << "\n";
}

void Output::turnOn() { 
	//std::cout << "Turning on " << address.io_offset << ':' << address.io_bitpos << "\n";
	assert(process_data);
	uint8_t *offset = process_data + address.io_offset;
	int bitpos = address.io_bitpos;
	while (bitpos>=8) { 
		++offset;
		bitpos-=8;
	}
	gettimeofday(&last, 0);
	last_event = e_on;
	updatedComponents.insert(this);
	++outputs_waiting;
	idle();
}

void Output::turnOff() { 
	//std::cout << "Turning off " << address.io_offset << ':' << address.io_bitpos << "\n";
	assert(process_data);
	uint8_t *offset = process_data + address.io_offset;
	int bitpos = address.io_bitpos;
	while (bitpos>=8) { 
		++offset;
		bitpos-=8;
	}
	gettimeofday(&last, 0);
	last_event = e_off;
	updatedComponents.insert(this);
	++outputs_waiting;
	idle();
}

void IOComponent::setValue(uint32_t new_value) {
	pending_value = new_value;
	gettimeofday(&last, 0);
	last_event = e_change;
	updatedComponents.insert(this);
	++outputs_waiting;
	idle();
}

/*
int IOComponent::getStatus() {
	uint8_t *offset = ECInterface::domain1_pd + address.io_offset;
	int bitpos = address.io_bitpos;
	while (bitpos>=8) { 
		++offset;
		bitpos-=8;
	}
    if (address.bitlen == 1) {
		int value = EC_READ_BIT(offset, bitpos);
		return value;
    }
    else {
		address.value = get_bits(offset, address.io_bitpos, address.bitlen);
        last_event = e_none;
        const char *evt = "value_changed";
        std::list<MachineInstance*>::iterator iter = depends.begin();
        while (iter != depends.end()) {
            MachineInstance *m = *iter++;
            Message msg(evt);
            m->execute(msg, this);
        }
        return address.value;
    }
}
*/

bool IOComponent::isOn() {
	return last_event == e_none && address.value != 0;
}

bool IOComponent::isOff() {
	return last_event == e_none && address.value == 0;
}
