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

#ifndef cwlang_MessagingInterface_h
#define cwlang_MessagingInterface_h

#include <string>
#include <zmq.hpp>
#include "Message.h"
#include "value.h"
#include "symboltable.h" 
#include "cJSON.h"

enum Protocol { eCLOCKWORK, eRAW, eZMQ };

class MessagingInterface : public Receiver {
public:
    MessagingInterface(int num_threads, int port, Protocol proto = eZMQ);
    MessagingInterface(std::string host, int port, Protocol proto = eZMQ);
    ~MessagingInterface();
    char *send(const char *msg);
    char *send(const Message &msg);
    bool send_raw(const char *msg);
    void setCurrent(MessagingInterface *mi) { current = mi; }
    static MessagingInterface *getCurrent();
    static MessagingInterface *create(std::string host, int port, Protocol proto = eZMQ);
	static Value valueFromJSONObject(cJSON *obj, cJSON *cjType);
    char *sendCommand(std::string cmd, std::list<Value> *params);
    char *sendCommand(std::string cmd, Value p1 = SymbolTable::Null,
                     Value p2 = SymbolTable::Null,
                     Value p3 = SymbolTable::Null);
    char *sendState(std::string cmd, std::string name, std::string state_name);
    static char *encodeCommand(std::string cmd, std::list<Value> *params);
    static char *encodeCommand(std::string cmd, Value p1 = SymbolTable::Null,
                     Value p2 = SymbolTable::Null,
                     Value p3 = SymbolTable::Null);
    static bool getCommand(const char *msg, std::string &cmd, std::list<Value> **params);
    static bool getState(const char *msg, std::string &cmd, std::list<Value> **params);
    
    //Receiver interface
    virtual bool receives(const Message&, Transmitter *t);
    virtual void handle(const Message&, Transmitter *from, bool needs_receipt = false );

		static void setContext(zmq::context_t *ctx) { context = ctx; }
		static zmq::context_t *getContext() { return context; }
    
private:
    void connect();
    static MessagingInterface *current;
	Protocol protocol;
    static zmq::context_t *context;
    zmq::socket_t *socket;
    static std::map<std::string, MessagingInterface *>interfaces;
    bool is_publisher;
    std::string url;
	int connection;
	std::string hostname;
	int port;
};

#endif
