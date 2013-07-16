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

#ifndef __IOComponent
#define __IOComponent

#include <list>
#include <map>
#include <string>
#include <ostream>
#include "State.h"
#include "ECInterface.h"
#include "Message.h"
#include "MQTTInterface.h"

struct IOAddress {
    unsigned int io_offset;
    int io_bitpos;
	int value;
	IOAddress(unsigned int offs, int bitp) : io_offset(offs), io_bitpos(bitp), value(0) { }
	IOAddress() : io_offset(0), io_bitpos(0), value(0) {}
};

struct MQTTTopic {
    std::string topic;
    std::string message;
    bool publisher;
    MQTTTopic(const char *t, const char *m) : topic(t), message(m) { }
    MQTTTopic(const std::string&t, const std::string&m) : topic(t), message(m) { }
};

class MachineInstance;
class IOComponent : public Transmitter {
public:
	typedef std::list<IOComponent *>::iterator Iterator;
	static Iterator begin() { return processing_queue.begin(); }
	static Iterator end() { return processing_queue.end(); }
	static void add_io_entry(const char *name, unsigned int io_offset, unsigned int bit_offset);
    static void add_publisher(const char *name, const char *topic, const char *message);
    static void add_subscriber(const char *name, const char *topic);
	static void processAll();
protected:
	static std::map<std::string, IOAddress> io_names;
private:
	IOComponent(const IOComponent &);
	static std::list<IOComponent *>processing_queue;

protected:
	enum event { e_on, e_off, e_none};
	event last_event;
	struct timeval last;
public:

    IOComponent(unsigned int offset, int bitpos) : last_event(e_none), address(offset,bitpos) { processing_queue.push_back(this); }
    IOComponent() : last_event(e_none) { processing_queue.push_back(this); }
	virtual ~IOComponent() { processing_queue.remove(this); }
	const char *getStateString();
	virtual void idle();
	void turnOn();
	void turnOff();
	bool isOn();
	bool isOff();
	virtual const char *type() { return "IOComponent"; }
	std::ostream &operator<<(std::ostream &out) const;
	IOAddress address;
	
	void addDependent(MachineInstance *m) {
		depends.push_back(m);
	}
	std::list<MachineInstance*>depends;


	void setName(std::string new_name) { io_name = new_name; }
	std::string io_name;

protected:
	int getStatus(); 
};
std::ostream &operator<<(std::ostream&out, const IOComponent &);

class Output : public IOComponent {
public:
	Output(unsigned int offset, int bitpos) : IOComponent(offset,bitpos) { }
	virtual const char *type() { return "Output"; }
};
class Input : public IOComponent {
public:
	Input(unsigned int offset, int bitpos) : IOComponent(offset,bitpos) { }
	virtual const char *type() { return "Input"; }
};


class MQTTPublisher : public IOComponent {
public:
	MQTTPublisher(unsigned int offset, int bitpos) : IOComponent(offset,bitpos) { }
	virtual const char *type() { return "Output"; }
};
class MQTTSubscriber : public IOComponent {
public:
	MQTTSubscriber(unsigned int offset, int bitpos) : IOComponent(offset,bitpos) { }
	virtual const char *type() { return "Input"; }
};

#endif