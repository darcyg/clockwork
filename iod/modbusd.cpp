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
#include <iterator>
#include <stdio.h>
#include <boost/program_options.hpp>
#include <zmq.hpp>
#include <sstream>
#include <string.h>
#include "Logger.h"
#include "DebugExtra.h"
#include <inttypes.h>
#include <fstream>
#include "symboltable.h"
#include "value.h"
#include <list>
#include <utility>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include "IODCommand.h"
#include <modbus/modbus.h>
#include <bitset>
#include "MessagingInterface.h"
#include "cJSON.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <signal.h>


namespace po = boost::program_options;

static boost::mutex q_mutex;
static boost::condition_variable_any cond;

const int MemSize = 65536;
bool debug = false;

std::stringstream dummy;
time_t last = 0;
time_t now;
#define OUT (time(&now) == last) ? dummy : std::cout

//class IODInterfaceThread;
//struct IODCommandInterface;

//IODInterfaceThread *g_iod_interface;
//IODCommandInterface *g_iodcmd;
MessagingInterface *g_iodcmd;
static modbus_mapping_t *modbus_mapping = 0;
static modbus_t *modbus_context = 0;

static std::set<std::string> active_addresses;
static std::map<std::string, bool> initialised_address;

std::string getIODSyncCommand(int group, int addr, int new_value);
char *sendIOD(int group, int addr, int new_value);
char *sendIODMessage(const std::string &s);


void activate_address(std::string& addr_str) {
    if (active_addresses.find(addr_str) == active_addresses.end()) {
	active_addresses.insert(addr_str);
	initialised_address[addr_str] = false;
    }
}

void insert(int group, int addr, const char *value, int len) {
	char buf[20];
	snprintf(buf, 19, "%d.%d", group, addr);
	std::string addr_str(buf);
	if (len % 2 != 0) len++;// pad
	uint16_t *dest = 0;
	if (group == 3)
		dest = &modbus_mapping->tab_input_registers[addr];
	else if (group == 4)
		dest = &modbus_mapping->tab_registers[addr];
	uint16_t as_int=0;
	const uint8_t *p = (uint8_t *)value;
	for (int i=0; i<len/2; ++i) {
		for (int j=0; j<2; ++j) {
			as_int += (as_int<<8)+*p++;
		}
		*dest++ = as_int;
	}
}
void insert(int group, int addr, int value, int len) {
	char buf[20];
	snprintf(buf, 19, "%d.%d", group, addr);
	std::string addr_str(buf);
	if (group == 1) {
		modbus_mapping->tab_input_bits[addr] = value;
std::cout << "Updated Modbus memory for input bit " << addr << " to " << value << "\n";
		activate_address(addr_str);
	}
	else if (group == 0) {
		modbus_mapping->tab_bits[addr] = value;
std::cout << "Updated Modbus memory for bit " << addr << " to " << value << "\n";
		activate_address(addr_str);
	}
	else if (group == 3) {
		if (len == 1) {
			modbus_mapping->tab_input_registers[addr] = value & 0xffff;
			activate_address(addr_str);
		} else if (len == 2) {
			int *l = (int32_t*) &modbus_mapping->tab_input_registers[addr];
			*l = value & 0xffffffff;
			activate_address(addr_str);
		}
	} else if (group == 4) {
		if (len == 1) {
			modbus_mapping->tab_registers[addr] = value & 0xffff;
std::cout << "Updated Modbus memory for reg " << addr << " to " << (value  & 0xffff) << "\n";
			activate_address(addr_str);
		} else if (len == 2) {
			int *l = (int32_t*) &modbus_mapping->tab_registers[addr];
			*l = value & 0xffffffff;
			activate_address(addr_str);
		}
	}
}

// ------------ Command interface -----------

struct IODCommandUnknown : public IODCommand {
    bool run(std::vector<Value> &params) {
        std::stringstream ss;
        ss << "Unknown command: ";
        std::ostream_iterator<std::string> oi(ss, " ");
        ss << std::flush;
        error_str = strdup(ss.str().c_str());
        return false;
    }
};

#if 0
struct IODInterfaceThread {
    void operator()() {
		std::cout << "------------------ IOD Interface Thread Started -----------------\n";
        context = new zmq::context_t(1);
        if (!socket)
			socket = new zmq::socket_t(*context, ZMQ_REQ);
        socket->connect ("tcp://localhost:5555");
		is_ready = true;
    }
    IODInterfaceThread() : done(false), is_ready(false), context(0), socket(0){
	}
	~IODInterfaceThread() { 
		if (socket) delete socket;
		socket = 0;
		if (context) delete context;
		context = 0;
	}

    void stop() { done = true; }
	bool ready() { return is_ready; }
    bool done;
	bool is_ready;
    zmq::context_t *context;
    zmq::socket_t *socket;
};
#endif

#if 0
struct IODCommandInterface {
	IODCommandInterface() : REQUEST_TIMEOUT(2000), REQUEST_RETRIES(3), context(0), socket(0){
	    context = new zmq::context_t(1);
		connect();
	}
	~IODCommandInterface() { 
		if (socket) delete socket;
		socket = 0;
		if (context) delete context;
		context = 0;
	}

    const int REQUEST_TIMEOUT;
    const int REQUEST_RETRIES;

    char *sendMessage(const char *message) {
        try {
            const char *msg = (message) ? message : "";

            int retries = REQUEST_RETRIES;
            while (retries) {
                size_t len = strlen(msg);
                zmq::message_t request (len);
                memcpy ((void *) request.data (), msg, len);
                socket->send (request);
                bool expect_reply = true;
                
                while (expect_reply) {
                    zmq::pollitem_t items[] = { { *socket, 0, ZMQ_POLLIN, 0 } };
                    zmq::poll( &items[0], 1, REQUEST_TIMEOUT*1000);
                    if (items[0].revents & ZMQ_POLLIN) {
                        zmq::message_t reply;
                        socket->recv(&reply);
	                    len = reply.size();
	                    char *data = (char *)malloc(len+1);
	                    memcpy(data, reply.data(), len);
	                    data[len] = 0;
	                    if (debug || strcmp(data, "OK") ) std::cout << data << "\n";
						return data;
                    }
                    else if (--retries == 0) {
                        // abandon
                        expect_reply = false;
                        std::cerr << "abandoning message " << msg << "\n" << std::flush;
                        delete socket;
                        connect();
                    }
                    else {
                        // retry
                        std::cerr << "retrying message " << msg << "\n" << std::flush;
                        delete socket;
                        connect();
                        socket->send (request);
                    }
                }
            }
        }
        catch(std::exception e) {
            std::cerr <<e.what() << "\n";
        }
		return 0;
    }

	void connect() {
		socket = new zmq::socket_t(*context, ZMQ_REQ);
	    socket->connect ("tcp://localhost:5555");
        int linger = 0; // do not wait at socket close time
        socket->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
	}

    zmq::context_t *context;
    zmq::socket_t *socket;
};
#endif


int getInt(uint8_t *p) {
	int x = *p++;
	x = (x<<8) + *p;
	return x;
}

void display(uint8_t *p, int len) {
	while (len--) printf("%02x",*p++);
	printf("\n");
}

struct ModbusServerThread {
	
    void operator()() {
		std::cout << "------------------ Modbus Server Thread Started -----------------\n" << std::flush;

		if ( LogState::instance()->includes(DebugExtra::instance()->DEBUG_MODBUS) )
			modbus_set_debug(modbus_context, TRUE);
			
		int function_code_offset = modbus_get_header_length(modbus_context);

		int socket = modbus_tcp_listen(modbus_context, 3);
		if (debug) std::cout << "Modbus listen socket: " << socket << "\n" << std::flush;
		FD_ZERO(&connections);
		FD_SET(socket, &connections);
		max_fd = socket;
		fd_set activity;
		while (!done) {
			activity = connections;
			int nfds;
			if ( (nfds = select(max_fd +1, &activity, 0, 0, 0)) == -1) {
				perror("select");
				continue; // TBD
			}
			if (nfds == 0) continue;
			for (int conn = 0; conn <= max_fd; ++conn) {
				if (!FD_ISSET(conn, &activity)) continue;
			
				if (conn == socket) { // new connection
                    struct sockaddr_in panel_in;
                    socklen_t addr_size = sizeof(panel_in);

                    memset(&panel_in, 0, sizeof(struct sockaddr_in));
					int panel_fd;
                    if ( (panel_fd = accept(socket, (struct sockaddr *)&panel_in, &addr_size)) == -1) {
                        perror("accept");
                    } else {
						int option = 1;
					    int res = setsockopt(panel_fd, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(int)); 
						if (res == -1) { perror("setsockopt"); }
					    res = setsockopt(panel_fd, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof(int)); 
						if (res == -1) { perror("setsockopt"); }
                        FD_SET(panel_fd, &connections);
                        if (panel_fd > max_fd) max_fd = panel_fd;;
						std::cout << "new connection: " << panel_fd << "\n";
					}

				}
				else {
		
			        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
			        uint8_t query_backup[MODBUS_TCP_MAX_ADU_LENGTH];
			        int n; 
	
					modbus_set_socket(modbus_context, conn); // tell modbus to use this current connection
			        n = modbus_receive(modbus_context, query);
			        if (n != -1) {
						std::list<std::string>iod_sync_commands;
						memcpy(query_backup, query, n);
						int addr = getInt( &query[function_code_offset+1]);
						//int len = getInt( &query[function_code_offset+3]);
						int fc = query[function_code_offset];
						// ensure changes to coils are not sent to iod if they are not required
						bool ignore_coil_change = false; 
						if (fc == 5) {  // coil write function
							ignore_coil_change = (query_backup[function_code_offset + 3] && modbus_mapping->tab_bits[addr]);
							if (ignore_coil_change) std::cout << "ignoring coil change " << addr    
								<< ((query_backup[function_code_offset + 3]) ? "on" : "off") << "\n";
						}
						else if (fc == 15) {
							int num_coils = (query_backup[function_code_offset+3] <<16)
								+ query_backup[function_code_offset + 4];
							int num_bytes = query_backup[function_code_offset+5];
							int curr_coil = 0;
							unsigned char *data = query_backup + function_code_offset + 6;
							for (int b = 0; b<num_bytes; ++b) {
								for (int bit = 0; bit < 8; ++bit) {
									if (curr_coil >= num_coils) break;
									char buf[20];
									snprintf(buf, 19, "%d.%d", 0, addr);
									std::string addr_str(buf);

									if (active_addresses.find(addr_str) != active_addresses.end()) {
										if (debug) std::cout << "updating cw with new discrete: " << addr 
											<< " (" << (int)(modbus_mapping->tab_bits[addr]) <<")"
											<< " (" << (int)(modbus_mapping->tab_input_bits[addr]) <<")"
											<< "\n";
										unsigned char val = (*data) & (1<<bit);
									

										if ( !initialised_address[addr_str] ||  ( val != 0 && modbus_mapping->tab_bits[addr] == 0 ) 
												|| (!val && modbus_mapping->tab_bits[addr] ) ) {
											if (debug) std::cout << "setting iod address " << addr+1 << " to " << ( (val) ? 1 : 0) << "\n";
											iod_sync_commands.push_back( getIODSyncCommand(0, addr+1, (val) ? 1 : 0) );
											initialised_address[addr_str] = true;
										}
									}
									++addr;
									++curr_coil;
								}
								++data;
							}
						}
						else if (fc == 16) {
							int num_words = (query_backup[function_code_offset+3] <<16)
								+ query_backup[function_code_offset + 4];
							//int num_bytes = query_backup[function_code_offset+5];
							unsigned char *data = query_backup + function_code_offset + 6; // interpreted as binary
							for (int reg = 0; reg<num_words; ++reg) {
								char buf[20];
								snprintf(buf, 19, "%d.%d", 4, addr);
								std::string addr_str(buf);

								if (active_addresses.find(addr_str) != active_addresses.end()) {
									uint16_t val = 0;
									//val = ((*data >> 4) * 10 + (*data & 0xf)); ++data; val = val*100 + ((*data >> 4) * 10 + (*data & 0xf)); // BCD
									val = getInt(data);
									data += 2;
									if (!initialised_address[addr_str] || val != modbus_mapping->tab_registers[addr]) {
										if (debug) std::cout << " Updating register " << addr 
											<< " to " << val << " from connection " << conn << "\n";									
										iod_sync_commands.push_back( getIODSyncCommand(4, addr+1, val) );
										initialised_address[addr_str] = true;
									}
								}
								++addr;
							}
						}

						// process the request, updating our ram as appropriate
			            n = modbus_reply(modbus_context, query, n, modbus_mapping);

						// post process - make sure iod is informed of the change
						if (fc == 2) {
							if (debug) std::cout << "connection " << conn << " read coil " << addr << "\n";
						} 
						else if (fc == 1) {
							char buf[20];
							snprintf(buf, 19, "%d.%d", 1, addr);
							std::string addr_str(buf);
							if (active_addresses.find(addr_str) != active_addresses.end() ) 
								if (debug) std::cout << "connection " << conn << " read discrete " << addr  
									<< " (" << (int)(modbus_mapping->tab_input_bits[addr]) <<")"
								<< "\n";
						}
						else if (fc == 3) {
							if (debug) std::cout << "connection " << conn << " got rw_register " << addr << "\n";
						}
						else if (fc == 4) {
							if (debug) std::cout << "connection " << conn << " got register " << addr << "\n";
						}
						else if (fc == 15 || fc == 16) {
							if (fc == 15) {
								char buf[20];
								snprintf(buf, 19, "%d.%d", 0, addr);
								std::string addr_str(buf);
								if (active_addresses.find(addr_str) != active_addresses.end() ) 
									if (debug)std::cout << "connection " << conn << " write multi discrete " 
										<< addr << "\n"; //<< " n:" << len << "\n";
							}
							else if (fc == 16) {
								if (debug) 
									std::cout << "write multiple register " << addr  << "\n"; 
							}
							std::list<std::string>::iterator iter = iod_sync_commands.begin();
							while (iter != iod_sync_commands.end()) {
								std::string cmd = *iter++;
								char *res = sendIODMessage(cmd);
								if (res) free(res);
							}
							iod_sync_commands.clear();
						}
						else if (fc == 5) {
							if (!ignore_coil_change) {
								char *res = sendIOD(0, addr+1, (query_backup[function_code_offset + 3]) ? 1 : 0);
								if (res) free(res);
								if (debug) std::cout << " Updating coil " << addr << " from connection " << conn 
										<< ((query_backup[function_code_offset + 3]) ? "on" : "off") << "\n";
							}
						}
						else if (fc == 6) {
							int val = getInt( &query[function_code_offset+5]);
							char *res = sendIOD(4, addr+1, modbus_mapping->tab_registers[addr]);
							if (res) free(res);
							if (debug) std::cout << " Updating register " << addr << " to " << val << " from connection " << conn << "\n";
						}
						else 
							if (debug) std::cout << " function code: " << (int)query_backup[function_code_offset] << "\n";
						if (n == -1) {
							if (debug) std::cout << "Error: " << modbus_strerror(errno) << "\n";

						}
			        } else {
			            /* Connection closed by the client or error */
						std::cout << "Error: " << modbus_strerror(errno) << "\n";
						std::cout << "Modbus connection " << conn << " lost\n";
						close(conn);
						if (conn == max_fd) --max_fd;
						FD_CLR(conn, &connections);
			        }
				}
		    }
		}
		modbus_free(modbus_context);
		modbus_context = 0;
		
    }
    ModbusServerThread() : done(false) {
		FD_ZERO(&connections);
    }
	~ModbusServerThread() {
		if (modbus_mapping) modbus_mapping_free(modbus_mapping);
		if (modbus_context) modbus_free(modbus_context);
	}

    void stop() { done = true; }
    bool done;
	fd_set connections;
	int max_fd;
};

std::string getIODSyncCommand(int group, int addr, int new_value) {
	std::stringstream ss;
	ss << "MODBUS " << group << " " << addr << " " << new_value;
	std::string s(ss.str());
	return s;
}

char *sendIOD(int group, int addr, int new_value) {
	std::string s(getIODSyncCommand(group, addr, new_value));
	if (g_iodcmd) 
		return g_iodcmd->send(s.c_str());
	else 	
		return strdup("IOD interface not ready\n");
}

char *sendIODMessage(const std::string &s) {
	if (g_iodcmd) 
		return g_iodcmd->send(s.c_str());
	else 	
		return strdup("IOD interface not ready\n");
}

void loadData(const char *initial_settings) {
    cJSON *obj = cJSON_Parse(initial_settings);
    if (!obj){ 
		std::istringstream init(initial_settings);

		char buf[200];
	    while (init.getline(buf, 200, '\n')) {
	        std::istringstream iss(buf);
	        int group, addr, len;
			int value;
			std::string name;
	        iss >> group >> addr >> name >> len >> value;
			if (debug) std::cout << name << ": " << group << " " << addr << " " << len << " " << value <<  "\n";
			insert(group, addr-1, value, len);
		}
	}
	else
    {
		int num_params = cJSON_GetArraySize(obj);
		if (num_params) {
			for (int i=0; i<num_params; ++i) {
				cJSON *item = cJSON_GetArrayItem(obj, i);
				if (item->type == cJSON_Array) {
					Value group = MessagingInterface::valueFromJSONObject(cJSON_GetArrayItem(item, 0), 0);
					Value addr = MessagingInterface::valueFromJSONObject(cJSON_GetArrayItem(item, 1), 0);
					Value name = MessagingInterface::valueFromJSONObject(cJSON_GetArrayItem(item, 2), 0);
					Value len = MessagingInterface::valueFromJSONObject(cJSON_GetArrayItem(item, 3), 0);
					Value value = MessagingInterface::valueFromJSONObject(cJSON_GetArrayItem(item, 4), 0);
					if (value.kind == Value::t_string) 
						insert(group.iValue, addr.iValue, value.asString().c_str(), len.iValue);
					else
						insert(group.iValue, addr.iValue, value.iValue, len.iValue);
				}
				else
				{
					char *node = cJSON_Print(item);
					std::cerr << "item " << i << " is not of the expected format: " << node << "\n";
					free(node);
				}
			}
		}
	}
}

bool done = false;

static void finish(int sig)
{
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, 0);
    sigaction(SIGINT, &sa, 0);
    done = true;
}

bool setup_signals()
{
    struct sigaction sa;
    sa.sa_handler = finish;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, 0) || sigaction(SIGINT, &sa, 0)) {
        return false;
    }
    return true;
}

int main(int argc, const char * argv[]) {

	po::options_description desc("Allowed options");
	desc.add_options()
	("help", "produce help message")
	("debug","enable debug")
	("host", "remote host (localhost)")
	("port", "clockwork port (5558)")
	;
	po::variables_map vm;   
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);    
	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 1;
	}
	int port = 5558;
	std::string host("localhost");
	// backward compatibility
	if (argc > 2 && strcmp(argv[1],"-p") == 0) {
		port = strtol(argv[2], 0, 0);
		std::cerr << "NOTICE: the -p option is deprecated, please use --port\n";
	}
	if (vm.count("port")) port = vm["port"].as<int>();
	if (vm.count("host")) host = vm["host"].as<std::string>();
	

	if (vm.count("debug")) {
		LogState::instance()->insert(DebugExtra::instance()->DEBUG_MODBUS);
		debug = true;
	}

	modbus_mapping = modbus_mapping_new(10000, 10000, 10000, 10000);
	if (!modbus_mapping) {
		std::cerr << "Error setting up modbus mapping regions\n";
		modbus_free(modbus_context);
		modbus_context = 0;
		return 1;
	}
	modbus_context = modbus_new_tcp("localhost", 1502);
	if (!modbus_context) {
		std::cerr << "Error creating a libmodbus TCP interface\n";
		return 1;
	}

	std::cout << "-------- Starting Command Interface ---------\n" << std::flush;	
	g_iodcmd = MessagingInterface::create(host, port);
	
	// initialise memory
	{
		std::cout << "-------- Collecting IO Status ---------\n" << std::flush;
		char *initial_settings;
		do {	
			active_addresses.clear();
			initialised_address.clear();
			initial_settings = g_iodcmd->sendCommand("MODBUS", "REFRESH");
			if (initial_settings && strncmp(initial_settings, "ignored", strlen("ignored")) != 0) {
				loadData(initial_settings);
				free(initial_settings);
			}
			else
				sleep(2);
		} while (!initial_settings);
	}

	//std::cout << "-------- Starting to listen to IOD ---------\n" << std::flush;	
	//IODInterfaceThread iod_interface;
	//g_iod_interface = &iod_interface;
	//boost::thread monitor(boost::ref(iod_interface));

	std::cout << "-------- Starting Modbus Interface ---------\n" << std::flush;	

	ModbusServerThread modbus_interface;
	boost::thread monitor_modbus(boost::ref(modbus_interface));

	setup_signals();
	
    try {
        
		std::cout << "Listening on port " << port << "\n";
        // client
        int res;
		std::stringstream ss;
		ss << "tcp://localhost:" << port;
        zmq::context_t context (1);
        zmq::socket_t subscriber (context, ZMQ_SUB);
        res = zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
        assert (res == 0);
        subscriber.connect(ss.str().c_str());
        //subscriber.connect("ipc://ecat.ipc");
        while (!done) {
            zmq::message_t update;
			if (!subscriber.recv(&update)) continue;
			long len = update.size();
           	char *data = (char *)malloc(len+1);
           	memcpy(data, update.data(), len);
           	data[len] = 0;

            std::list<Value> parts;
            int count = 0;
            std::string ds;
            std::vector<Value> params(0);
            {
                std::list<Value> *param_list = 0;
                if (MessagingInterface::getCommand(data, ds, &param_list)) {
                    params.push_back(ds);
                    if (param_list) {
                        std::list<Value>::const_iterator iter = param_list->begin();
                        while (iter != param_list->end()) {
                            const Value &v  = *iter++;
                            params.push_back(v);
                        }
                    }
                    count = params.size();
                }
                else {
                    std::istringstream iss(data);
                    while (iss >> ds) {
                        parts.push_back(ds.c_str());
                        ++count;
                    }
                    std::copy(parts.begin(), parts.end(), std::back_inserter(params));
                }
            }
            std::string cmd(params[0].asString());

            //std::istringstream iss(data);
			//std::string cmd;
			//iss >> cmd;
			if (cmd == "UPDATE") {
            	int group, addr, len, value;
				std::string name;
				
				assert(params.size() >= 6);
				assert(params[1].kind == Value::t_integer);
				assert(params[2].kind == Value::t_integer);
				assert(params[3].kind == Value::t_string);
				assert(params[4].kind == Value::t_integer);
				
				group = params[1].iValue;
				addr = params[2].iValue;
				name = params[3].sValue;
				len = params[4].iValue;
            	//iss >> group >> addr >> name >> len >> value;
	
				if (debug) std::cout << "IOD: " << group << " " << addr << " " << name << " " << len << " " << params[5] <<  "\n";
				if (params[5].kind == Value::t_string) {
					insert(group, addr-1, params[5].asString().c_str(), len);
				}
				else
					insert(group, addr-1, params[5].iValue, len);
			}
			else if (cmd == "STARTUP") {
#if 1
				active_addresses.clear();
				initialised_address.clear();
				sleep(2);
				break;		
#else
				char *initial_settings = g_iodcmd->sendMessage("MODBUS REFRESH");
				loadData(initial_settings);
				free(initial_settings);
#endif
			}
			else if (cmd == "DEBUG") {
				assert(params.size() >= 2);
				assert(params[1].kind == Value::t_symbol);
				debug =  (params[1].sValue == "ON") ? true : false;
			}
        }
    }
    catch(std::exception& e) {
        if (zmq_errno())
            std::cerr << zmq_strerror(zmq_errno()) << "\n";
        else
            std::cerr << e.what() << "\n";
        return 1;
    }
    catch(...) {
        std::cerr << "Exception of unknown type!\n";
    }

//    iod_interface.stop();
//   monitor.join();
	modbus_interface.stop(); // may hang if clients are connected
	monitor_modbus.join();
    return 0;
}

