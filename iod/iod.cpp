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

#include <unistd.h>
#include "ECInterface.h"
#include "ControlSystemMachine.h"
#include "IOComponent.h"
#include <sstream>
#include <stdio.h>
#include <zmq.hpp>
#include <sys/stat.h>
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"

#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <list>
#include <map>
#include <utility>
#include <fstream>
#include "cJSON.h"
#ifndef EC_SIMULATOR
#include <tool/MasterDevice.h>
#endif

#define __MAIN__
//#include "latprocc.h"
#include "MachineInstance.h"
#include "options.h"
#include <stdio.h>
#include "Logger.h"
#include "IODCommand.h"
#include "Dispatcher.h"
#include "Statistic.h"
#include "symboltable.h"
#include "DebugExtra.h"
#include "Scheduler.h"
#include "PredicateAction.h"
#include "ModbusInterface.h"
#include "Statistics.h"
#include "IODCommands.h"
#include <signal.h>
#include "clockwork.h"
#include "ClientInterface.h"
#include "MessageLog.h"
#include "MessagingInterface.h"

#define USE_RTC
#ifdef USE_RTC
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <sys/time.h>
#include <inttypes.h>
#include <fcntl.h>
#endif


bool program_done = false;
bool machine_is_ready = false;

void usage(int argc, char *argv[]);
void displaySymbolTable();

Statistics *statistics = NULL;
std::list<Statistic *> Statistic::stats;

boost::mutex thread_protection_mutex;
static boost::mutex io_mutex;
static boost::mutex model_mutex;
boost::condition_variable_any io_updated;
boost::condition_variable_any model_updated;
boost::mutex ecat_mutex;
boost::condition_variable_any ecat_polltime;

#ifdef EC_SIMULATOR

// in a simulated environment, we provide a way to wire components together
void checkInputs();

typedef std::list<std::string> StringList;
std::map<std::string, StringList> wiring;


void checkInputs() {
    std::map<std::string, StringList>::iterator iter = wiring.begin();
    while (iter != wiring.end()) {
        const std::pair<std::string, StringList> &link = *iter++;
        Output *device = dynamic_cast<Output*>(IOComponent::lookup_device(link.first));
        if (device) {
            StringList::const_iterator linked_devices = link.second.begin();
            while (linked_devices != link.second.end()) {
                Input *end_point = dynamic_cast<Input*>(IOComponent::lookup_device(*linked_devices++));
                SimulatedRawInput *in = 0;
                if (end_point)
                   in  = dynamic_cast<SimulatedRawInput*>(end_point->access_raw_device());
                if (in) {
                    if (device->isOn() && !end_point->isOn()) {
                        in->turnOn();
                    }
                    else if (device->isOff() && !end_point->isOff())
                        in->turnOff();
                }
            }
        }
    }
}

#else
#endif

void load_debug_config() {
	std::ifstream program_config(debug_config());
	if (program_config) {
		std::string debug_flag;
		while (program_config >> debug_flag) {
			if (debug_flag[0] == '#') continue;
			int dbg = LogState::instance()->lookup(debug_flag);
			if (dbg) LogState::instance()->insert(dbg);
			else if (machines.count(debug_flag)) {
				MachineInstance *mi = machines[debug_flag];
				if (mi) mi->setDebug(true);
			}
			else std::cerr << "Warning: unrecognised DEBUG Flag " << debug_flag << "\n";
		}
	}
}

static void finish(int sig)
{
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, 0);
    sigaction(SIGINT, &sa, 0);
	std::cerr << "received signal..quitting\n";
    program_done = true;
	io_updated.notify_one();
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
	sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, 0)) return false;
	return true;
}


struct ProcessingThread {
    void operator()();
    ProcessingThread(ControlSystemMachine &m, long delay);
    void stop() { program_done = true; }
	void checkAndUpdateCycleDelay();

	ControlSystemMachine &machine;
    int sequence;
	long cycle_delay;
};

ProcessingThread::ProcessingThread(ControlSystemMachine &m, long delay)
	: machine(m), sequence(0), cycle_delay(delay) {	
}

void ProcessingThread::checkAndUpdateCycleDelay() {
	Value *cycle_delay_v = ClockworkInterpreter::instance()->cycle_delay;
	long delay = 100;
	if (cycle_delay_v && cycle_delay_v->iValue >= 100) 
		delay = cycle_delay_v->iValue;
	if (delay != cycle_delay) {
		ECInterface::FREQUENCY = 1000000 / delay;
		//ECInterface::instance()->start();
		cycle_delay = delay;
	}
}

void ProcessingThread::operator()()  {
	
	Statistic *cycle_delay_stat = new Statistic("Cycle Delay");
	Statistic::add(cycle_delay_stat);
	long delta, delta2;

	unsigned long sync = 0;

#ifdef USE_RTC
    int rtc = open("/dev/rtc", 0);
    if (rtc == -1) { perror("open rtc"); exit(1); }
    unsigned long period = 1024;

    int rc = ioctl(rtc, RTC_IRQP_SET, period);
    if (rc == -1) { perror("set rtc period"); exit(1); }

    rc = ioctl(rtc, RTC_IRQP_READ, &period);
    if (rc == -1) { perror("ioctl"); exit(1); }
    std::cout << "Real time clock: period set to : " << period << "\n";

    rc = ioctl(rtc, RTC_PIE_ON, 0);
    if (rc == -1) { perror("enable rtc pie"); exit(1); }
#endif


	while (!program_done) {
#ifdef USE_RTC
    long rtc_val;
    int rc = read(rtc, &rtc_val, sizeof(rtc_val));
    if (rc == -1) {
				if (errno == EAGAIN ) continue;
        if (errno == EBADF) {
            rtc = open("/dev/rtc", 0);
            if (rtc == -1) { perror("open rtc"); exit(1); }
        }
        perror("read rtc"); 
		}
    if (rc == 0) std::cout << "zero bytes read from rtc\n";
#endif

	
//		pause();
		enum { eIdle, eStableStates, ePollingMachines} processingState = eIdle;
		struct timeval start_t, end_t;
#if 0
        {
			boost::system_time const timeout=boost::get_system_time() + boost::posix_time::microseconds(1000000/ECInterface::FREQUENCY);
			bool timed_out = false;
			try {
				boost::mutex::scoped_lock lock(io_mutex);
	            if (!io_updated.timed_wait(io_mutex, timeout)) goto end_process_loop;
			} catch (boost::thread_resource_error err) {
				timed_out = true;
			} catch (std::exception e) {
				std::cerr << "EtherCAT link timed out: " << e.what() << "\n";
				timed_out = true;
            	if (zmq_errno())
            	    std::cerr << zmq_strerror(zmq_errno()) << "\n";
            	else
            	    std::cerr << e.what() << "\n";
				abort();
			}
			if (timed_out) goto end_process_loop;
        }
#endif
		{
		boost::mutex::scoped_lock lock(thread_protection_mutex);
		gettimeofday(&start_t, 0);
		if (machine_is_ready) {
			delta = get_diff_in_microsecs(&start_t, &end_t);
			cycle_delay_stat->add(delta);
		}

		if (true || sync != ECInterface::sig_alarms) {
	    ECInterface::instance()->collectState();
			IOComponent::processAll();
			gettimeofday(&end_t, 0);

			delta = get_diff_in_microsecs(&end_t, &start_t);
			statistics->io_scan_time.add(delta);
			delta2 = delta;
		
#ifdef EC_SIMULATOR
			checkInputs(); // simulated wiring between inputs and outputs
#endif
			if (machine.connected()) {
				gettimeofday(&end_t, 0);
				delta = get_diff_in_microsecs(&end_t, &start_t);
				statistics->points_processing.add(delta - delta2); delta2 = delta;

				if (!machine_is_ready) {
					std::cout << "----------- Machine is Ready --------\n";
					machine_is_ready = true;
					BOOST_FOREACH(std::string &error, error_messages) {
						std::cerr << error << "\n";
						MessageLog::instance()->add(error.c_str());
					}
				}
				bool reported_error = false;
				{
					Value *cycle_delay_v = ClockworkInterpreter::instance()->cycle_delay;
					long remain = 0;
					if (!cycle_delay_v->asInteger(remain))  {
						if (!reported_error) MessageLog::instance()->add("cycle delay error\n");
						remain = 10000;
					}
					else remain /= 2;
					if (processingState == eIdle)
						processingState = ePollingMachines;
					if (processingState == ePollingMachines) {
						if (MachineInstance::processAll(50000, MachineInstance::NO_BUILTINS))
							processingState = eIdle;
						gettimeofday(&end_t, 0);
						delta = get_diff_in_microsecs(&end_t, &start_t);
						remain -= delta;
						statistics->machine_processing.add(delta - delta2); delta2 = delta;
					}
					if (processingState == eIdle) {
						Dispatcher::instance()->idle();
						Scheduler::instance()->idle();
						gettimeofday(&end_t, 0);
						delta = get_diff_in_microsecs(&end_t, &start_t);
						statistics->dispatch_processing.add(delta - delta2); delta2 = delta;
						processingState = eStableStates;
					}	
					if (processingState == eStableStates) {
						if (MachineInstance::checkStableStates(50000))
							processingState = eIdle;
						gettimeofday(&end_t, 0);
						delta = get_diff_in_microsecs(&end_t, &start_t);
						statistics->auto_states.add(delta - delta2); delta2 = delta;
					}
				}
			}
   		ECInterface::instance()->sendUpdates();
//			++sync;
//			if (sync != ECInterface::sig_alarms) sync = ECInterface::sig_alarms-1;
			//else {
			//	std::cout << "wc state: " << ECInterface::domain1_state.wc_state << "\n"
			//		<< "Master link up: " << ECInterface::master_state.link_up << "\n";
			//}
			checkAndUpdateCycleDelay();
		}
		std::cout << std::flush;
//		model_mutex.lock();
//		model_updated.notify_one();
//		model_mutex.unlock();
		}
	}
	std::cout << "processing done\n";
}

void generateIOComponentModules() {
	std::list<Output *> output_list;
	{
		boost::mutex::scoped_lock lock(thread_protection_mutex);

		int remaining = machines.size();
		std::cout << remaining << " Machines\n";
		std::cout << "Linking clockwork machines to hardware\n";
		std::map<std::string, MachineInstance*>::const_iterator iter = machines.begin();
		while (iter != machines.end()) {
			MachineInstance *m = (*iter).second; iter++;
			--remaining;
			if ( (m->_type == "POINT" || m->_type == "ANALOGINPUT"  || m->_type == "COUNTERRATE"
					|| m->_type == "COUNTER" || m->_type == "RATEESTIMATOR" 
					|| m->_type == "STATUS_FLAG" || m->_type == "ANALOGOUTPUT" ) && m->parameters.size() > 1) {
				// points should have two parameters, the name of the io module and the bit offset
				//Parameter module = m->parameters[0];
				//Parameter offset = m->parameters[1];
				//Value params = p.val;
				//if (params.kind == Value::t_list && params.listValue.size() > 1) {
					std::string name;
					unsigned int entry_position = 0;
					if (m->_type == "COUNTERRATE") {
						name = m->parameters[1].real_name;
						entry_position = m->parameters[2].val.iValue;
					}
					else {
						name = m->parameters[0].real_name;
						entry_position = m->parameters[1].val.iValue;
					}
	
					std::cerr << "Setting up point " << m->getName() << " " << entry_position << " on module " << name << "\n";
					MachineInstance *module_mi = MachineInstance::find(name.c_str());
					if (!module_mi) {
						std::cerr << "No machine called " << name << "\n";
						continue;
					}
					if (!module_mi->properties.exists("position")) { // module position not given
						std::cerr << "Machine " << name << " does not specify a position\n";
						continue; 
					}
					std::cerr << module_mi->properties.lookup("position").kind << "\n";
					int module_position = module_mi->properties.lookup("position").iValue;
					if (module_position == -1)  { // module position unmapped
						std::cerr << "Machine " << name << " position not mapped\n";
						continue; 
					}
				
#ifndef EC_SIMULATOR
					// some modules have multiple io types (eg the EK1814) and therefore
					// multiple syncmasters, we number 
					// the points from 1..n but the device numbers them 1.n,1.m,..., resetting
					// the index for each sync master.
					ECModule *module = ECInterface::findModule(module_position);
					if (!module) {
						std::cerr << "No module found at position " << module_position << "\n";
						continue;
					}
				
/*
					unsigned int sm_idx = 0;
					unsigned int entry_pos = 0;
					unsigned int pdo_idx = 0;
					unsigned int bit_pos = 0;
					unsigned int entry_idx = 0;

					while (sm_idx < module->sync_count) {
						for (pdo_idx = 0; pdo_idx<module->syncs[sm_idx].n_pdos; ++pdo_idx) {
							if (entry_pos + module->syncs[sm_idx].pdos[pdo_idx].n_entries > entry_position) {
								entry_idx = entry_position - entry_pos;
								goto found_entry;
							}
							entry_pos += module->syncs[sm_idx].pdos[pdo_idx].n_entries;
						}
						++sm_idx;
					}
					if (sm_idx == module->sync_count) {
						std::cerr << "No entry " << entry_position << " on module " << module_position << "\n";
						continue; // could not find this device
					}
					found_entry:
					unsigned int direction = module->syncs[sm_idx].dir;
					bit_pos = module->syncs[sm_idx].pdos[pdo_idx].entries[entry_idx].subindex;
					unsigned int bitlen = module->syncs[sm_idx].pdos[pdo_idx].entries[entry_idx].bit_length;
					if (m->parameters.size() == 2)
						bit_pos = entry_position;
*/

					if (entry_position >= module->num_entries) {
						std::cerr << "No entry " << entry_position << " on module " << module_position << "\n";
						continue; // could not find this device
					}
					EntryDetails *ed = &module->entry_details[entry_position];
					unsigned int direction = module->syncs[ed->sm_index].dir;
					unsigned int offset_idx = entry_position;
					unsigned int bitlen = module->pdo_entries[entry_position].bit_length;

					if (direction == EC_DIR_OUTPUT) {
                        std::cerr << "Adding new output device " << m->getName()
							<< " position: " << entry_position
							<< " name: " << module->entry_details[offset_idx].name
                            << " bit_pos: " << module->bit_positions[offset_idx]
                            << " offset: " << module->offsets[offset_idx]
                            << " bitlen: " << bitlen <<  "\n";
						IOAddress addr (	
                        	IOComponent::add_io_entry(ed->name.c_str(), 
								module_position, 
								module->offsets[offset_idx],
                                module->bit_positions[offset_idx], offset_idx, bitlen));

						if (bitlen == 1) {
							Output *o = new Output(addr);
							output_list.push_back(o);
							IOComponent::devices[m->getName().c_str()] = o;
							o->setName(m->getName().c_str());
							m->io_interface = o;
							o->addDependent(m);
						}
						else {
                            AnalogueOutput *o = new AnalogueOutput(addr);
							output_list.push_back(o);
							IOComponent::devices[m->getName().c_str()] = o;
							o->setName(m->getName().c_str());
							m->io_interface = o;
							o->addDependent(m);
						}
					}
					else {
						//sstr << m->getName() << "_IN_" << entry_position << std::flush;
						//const char *name_str = sstr.str().c_str();
                        std::cerr << "Adding new input device " << m->getName()
							<< " position: " << entry_position
							<< " name: " << module->entry_details[offset_idx].name
                            << " sm_idx: " << ed->sm_index
                            << " bit_pos: " << module->bit_positions[offset_idx]
                            << " offset: " << module->offsets[offset_idx]
                            <<  " bitlen: " << bitlen << "\n";
                        IOAddress addr( IOComponent::add_io_entry(ed->name.c_str(), 
							module_position, 
							module->offsets[offset_idx],
                            module->bit_positions[offset_idx], offset_idx, bitlen));

						if (bitlen == 1) {
							Input *in = new Input(addr);
							IOComponent::devices[m->getName().c_str()] = in;
							in->setName(m->getName().c_str());
							m->io_interface = in;
							in->addDependent(m);
						}
						else {
							if (m->_type == "COUNTERRATE") {
								CounterRate *in = new CounterRate(addr);
								char *nm = strdup(m->getName().c_str());
								IOComponent::devices[nm] = in;
								free(nm);
								in->setName(m->getName().c_str());
								m->io_interface = in;
								in->addDependent(m);
							}
							else if (m->_type == "COUNTER") {
								Counter *in = new Counter(addr);
								char *nm = strdup(m->getName().c_str());
								IOComponent::devices[nm] = in;
								free(nm);
								in->setName(m->getName().c_str());
								m->io_interface = in;
								in->addDependent(m);
							}
							else {
								AnalogueInput *in = new AnalogueInput(addr);
								char *nm = strdup(m->getName().c_str());
								IOComponent::devices[nm] = in;
								free(nm);
								in->setName(m->getName().c_str());
								m->io_interface = in;
								in->addDependent(m);
							}
						}
					}
#endif
				}
				else {
					if (m->_type != "POINT" && m->_type != "STATUS_FLAG" && m->_type != "COUNTERRATE"
							&& m->_type != "COUNTER"
							&& m->_type != "ANALOGINPUT" && m->_type != "ANALOGOUTPUT" )
						DBG_MSG << "Skipping " << m->_type << " " << m->getName() << " (not a POINT)\n";
					else  
						DBG_MSG << "Skipping " << m->_type << " " << m->getName() << " (no parameters)\n";
				}
			}
			assert(remaining==0);
		}
}

int main(int argc, char const *argv[])
{
	zmq::context_t context;
	MessagingInterface::setContext(&context);
	Logger::instance();
	MessageLog::setMaxMemory(10000);
	ControlSystemMachine machine;

    Logger::instance()->setLevel(Logger::Debug);
	LogState::instance()->insert(DebugExtra::instance()->DEBUG_PARSER);

	load_debug_config();

#if 0
	//LogState::instance()->insert(DebugExtra::instance()->DEBUG_PREDICATES);
	//LogState::instance()->insert(DebugExtra::instance()->DEBUG_INITIALISATION);
	//LogState::instance()->insert(DebugExtra::instance()->DEBUG_MESSAGING);
	//LogState::instance()->insert(DebugExtra::instance()->DEBUG_ACTIONS);
	//std::cout << DebugExtra::instance()->DEBUG_PREDICATES << "\n";
	//assert (!LogState::instance()->includes(DebugExtra::instance()->DEBUG_PREDICATES));
	//LogState::instance()->insert(DebugExtra::instance()->DEBUG_SCHEDULER);
	//LogState::instance()->insert(DebugExtra::instance()->DEBUG_PROPERTIES);
	//LogState::instance()->insert(DebugExtra::instance()->DEBUG_MESSAGING);
	//LogState::instance()->insert(DebugExtra::instance()->DEBUG_STATECHANGES);
	//LogState::instance()->insert(DebugExtra::instance()->DEBUG_AUTOSTATES);
	//LogState::instance()->insert(DebugExtra::instance()->DEBUG_MODBUS);
#endif
	
	IODCommandListJSON::no_display.insert("tab");
	IODCommandListJSON::no_display.insert("type");
	IODCommandListJSON::no_display.insert("name");
	IODCommandListJSON::no_display.insert("image");
	IODCommandListJSON::no_display.insert("class");
	IODCommandListJSON::no_display.insert("state");
	IODCommandListJSON::no_display.insert("export");
	IODCommandListJSON::no_display.insert("startup_enabled");
	IODCommandListJSON::no_display.insert("NAME");
	IODCommandListJSON::no_display.insert("STATE");
	IODCommandListJSON::no_display.insert("PERSISTENT");
    IODCommandListJSON::no_display.insert("POLLING_DELAY");
    IODCommandListJSON::no_display.insert("TRACEABLE");

	statistics = new Statistics;
	int load_result = loadConfig(argc, argv);
	if (load_result)
		return load_result;
    if (dependency_graph()) {
		std::cout << "writing dependency graph to " << dependency_graph() << "\n";
        std::ofstream graph(dependency_graph());
        if (graph) {
            graph << "digraph G {\n";
            std::list<MachineInstance *>::iterator m_iter;
            m_iter = MachineInstance::begin();
            while (m_iter != MachineInstance::end()) {
                MachineInstance *mi = *m_iter++;
				if (!mi->depends.empty()) {
                	BOOST_FOREACH(MachineInstance *dep, mi->depends) {
                		graph << mi->getName() << " -> " << dep->getName() << ";\n";
					}
				}
            }
            graph << "}\n";
        }
        else {
            std::cerr << "not able to open " << dependency_graph() << " for write\n";
        }
    }

	if (test_only() ) {
		const char *backup_file_name = "modbus_mappings.bak";
		rename(modbus_map(), backup_file_name);
		// export the modbus mappings and exit
        std::list<MachineInstance*>::iterator m_iter = MachineInstance::begin();
        std::ofstream out(modbus_map());
        if (!out) {
            std::cerr << "not able to open " << modbus_map() << " for write\n";
            return false;
        }    
        while (m_iter != MachineInstance::end()) {
            (*m_iter)->exportModbusMapping(out);
            m_iter++;
        }    
        out.close();

		return load_result;
	}

  Value *cycle_delay_v = ClockworkInterpreter::instance()->cycle_delay;
	long delay = 2000;
	if (cycle_delay_v) cycle_delay_v->iValue = delay;
	ECInterface::FREQUENCY=1000000 / delay;

#ifndef EC_SIMULATOR
	collectSlaveConfig(true);
	ECInterface::instance()->activate();
#endif
	generateIOComponentModules();
	MachineInstance::displayAll();
	ECInterface::instance()->start();

#ifdef EC_SIMULATOR
		wiring["EL2008_OUT_3"].push_back("EL1008_IN_1");
#endif

	std::cout << "-------- Initialising ---------\n";	
	
	initialise_machines();
	setup_signals();

	std::cout << "-------- Starting Command Interface ---------\n";	
	IODCommandThread stateMonitor;
	boost::thread monitor(boost::ref(stateMonitor));
	
	Statistic *cycle_delay_stat = new Statistic("Cycle Delay");
	Statistic::add(cycle_delay_stat);
	long delta, delta2;

	// Inform the modbus interface we have started
	load_debug_config();
	ModbusAddress::message("STARTUP");

    ProcessingThread processMonitor(machine, delay);
	//boost::thread process(boost::ref(processMonitor)); 
	// do not start a thread, simply run this process directly
   	processMonitor();
    stateMonitor.stop();
    monitor.join();
	return 0;
}
