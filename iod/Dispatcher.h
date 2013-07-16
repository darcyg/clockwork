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

#ifndef latprocc_Dispatcher_h
#define latprocc_Dispatcher_h

#include <ostream>
#include <string>
#include <list>
#include <map>
#include <utility>
#include <boost/thread.hpp>

class Message;
class Receiver;
class Package;

class Dispatcher {
public:
    std::ostream &operator<<(std::ostream &out) const;
    void deliver(Package *p);
    void addReceiver(Receiver*r);
    void removeReceiver(Receiver*r);
    static Dispatcher *instance();
    void idle();
    
private:
    Dispatcher();
    Dispatcher(const Dispatcher &orig);
    Dispatcher &operator=(const Dispatcher &other);
    typedef std::list<Receiver*> ReceiverList;
    ReceiverList all_receivers;
    static Dispatcher *instance_;
    std::list<Package*>to_deliver;
};

std::ostream &operator<<(std::ostream &out, const Dispatcher &m);




#endif