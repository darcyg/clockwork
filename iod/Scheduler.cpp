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

#include <iostream>
#include <iomanip>
#include "Logger.h"
#include "Scheduler.h"
#include "MachineInstance.h"
#include "DebugExtra.h"

Scheduler *Scheduler::instance_;
static const uint32_t ONEMILLION = 1000000;

static void setTime(struct timeval &now, unsigned long delta) {
	gettimeofday(&now, 0);
	DBG_SCHEDULER << "setTime: " << now.tv_sec << std::setfill('0')<< std::setw(6) << now.tv_usec << "\n";
	now.tv_sec += delta / ONEMILLION;
	now.tv_usec += delta % ONEMILLION;
	if (now.tv_usec >= ONEMILLION) {
		++now.tv_sec;
		now.tv_usec -= ONEMILLION;
	}
}

bool ScheduledItem::operator<(const ScheduledItem& other) const {
    if (delivery_time.tv_sec < other.delivery_time.tv_sec) return true;
    if (delivery_time.tv_sec == other.delivery_time.tv_sec) return delivery_time.tv_usec < other.delivery_time.tv_usec;
    return false;
}

bool ScheduledItem::operator>=(const ScheduledItem& other) const {
    if (delivery_time.tv_sec > other.delivery_time.tv_sec) return true;
    if (delivery_time.tv_sec == other.delivery_time.tv_sec) return delivery_time.tv_usec >= other.delivery_time.tv_usec;
    return false;
}

void PriorityQueue::push(ScheduledItem *item) {
	std::list<ScheduledItem*>::iterator iter = queue.begin();
	while (iter!= queue.end()) {
		ScheduledItem *queued = *iter;
		if (*item < *queued) {
			queue.insert(iter, item);
			return;
		}
		else
			iter++;
	}
	queue.push_back(item);
}

ScheduledItem::ScheduledItem(long when, Package *p) :package(p), action(0) {
	setTime(delivery_time, when);
}
ScheduledItem::ScheduledItem(long when, Action *a) :package(0), action(a) {
	setTime(delivery_time, when);
	DBG_SCHEDULER << "scheduled: " << delivery_time.tv_sec << std::setfill('0')<< std::setw(6) << delivery_time.tv_usec << "\n";
}

std::ostream &ScheduledItem::operator <<(std::ostream &out) const {
    out << delivery_time.tv_sec << std::setfill('0')<< std::setw(6) << delivery_time.tv_usec;
    if (package) out << *package; else if (action) out << *action;
    return out;
}

std::ostream &operator <<(std::ostream &out, const ScheduledItem &item) {
    return item.operator<<(out);
}

Scheduler::Scheduler() { 
	next_time.tv_sec = 0;
	next_time.tv_usec = 0;
}

bool CompareSheduledItems::operator()(const ScheduledItem *a, const ScheduledItem *b) const {
    bool result = (*a) >= (*b);
    return result;
}

void Scheduler::add(ScheduledItem*item) {
    DBG_SCHEDULER << "Scheduling item: " << *item << "\n";
	//DBG_SCHEDULER << "Before schedule::add() " << *this << "\n";
	items.push(item);
	next_time = next()->delivery_time;
	//DBG_SCHEDULER << "After schedule::add() " << *this << "\n";
}

/*
std::ostream &Scheduler::operator<<(std::ostream &out) const  {
    std::list<ScheduledItem*>::const_iterator iter = items.begin();
	while (iter != items.queue.end()) {
        ScheduledItem *item = *iter++;
        struct timeval now;
        gettimeofday(&now, 0);
        long dt = get_diff_in_microsecs(&item->delivery_time, &now);
		out << "Scheduler: {" << item->delivery_time.tv_sec << "." << std::setfill('0')<< std::setw(6) << item->delivery_time.tv_usec
                << " (" << dt << "usec) }" << "\n  ";
		if (item->package) out << *(item->package);
		else if (item->action) out << *(item->action);
        break;
	}
    out << " (" << items.size() << " items)\n";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Scheduler &m) {
    return m.operator<<(out);
}
 */

bool Scheduler::ready() {
	if (empty()) return false;
	//DBG_SCHEDULER<< "scheduler checking " << *this << "\n";
	if (next_time.tv_sec == 0) {
		next_time = next()->delivery_time;
	}
	struct timeval now;
	gettimeofday(&now, 0);
    ScheduledItem *top = next();
    long dt = get_diff_in_microsecs(&top->delivery_time, &now);
	//if (now.tv_sec > next_time.tv_sec) return true;
	//if (now.tv_sec == next_time.tv_sec) return now.tv_usec >= next_time.tv_usec;
    if (dt<=0) return true;
	return false;
}

void Scheduler::idle() {
	while ( ready()) {
		ScheduledItem *item = next();
		DBG_SCHEDULER << "Scheduled item " << (*item) << " is ready. \n"; //(" << items.size() << " items)\n"<< *this;
		pop();
		next_time.tv_sec = 0;
		if (item->package) {
			//item->package->receiver->enqueue(*(item->package));
			item->package->receiver->handle(item->package->message, item->package->transmitter);
			delete item->package;
            delete item;
		}
		else if (item->action) {
			item->action->getOwner()->push(item->action);
            delete item;
		}
	}
}

