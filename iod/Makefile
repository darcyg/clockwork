# Copyright (C) 2012 Martin Leadbeater, Michael O'Connor
# 
# This file is part of Latproc
#
# Latproc is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# Latproc is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with Latproc; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
# 

APPS = iod iosh persistd device_connector zmq_ecat_monitor beckhoffd modbusd #cw
SIMULATED=#-DEC_SIMULATOR=1

# add any extra include or library directory paths as necessary

ETHERCAT = ../ethercat
TOOLLIB = ../ec_tool/build/*.o
TOOLINC = ../ethercat/tool
EXTRAINCS = -I/opt/local/include -I/usr/local/include -I/opt/latproc/include \
	-I$(ETHERCAT)/include -I$(ETHERCAT) -I$(ETHERCAT)/master \
	-I$(TOOLINC)  -I.
#	-I/usr/src/linux-headers-3.5.0-32-generic/include -I/usr/src/linux-headers-3.5.0-32/arch/ia64/include
EXTRALIBS = -L/opt/local/lib -L/usr/local/lib -L/opt/latproc/lib \
	-L$(ETHERCAT)/lib/.libs 

# adjust the following linker flags to set the boost libraries you 
# would like to use.

BOOST_LIB_EXTN = #-mt
BOOST_THREAD_LIB = -lboost_thread$(BOOST_LIB_EXTN)
BOOST_FILESYSTEM_LIB = -lboost_system$(BOOST_LIB_EXTN) -lboost_filesystem$(BOOST_LIB_EXTN)
BOOST_PROGRAM_OPTIONS_LIB = -lboost_program_options$(BOOST_LIB_EXTN)
BOOST_SYSTEM_LIB = -lboost_system$(BOOST_LIB_EXTN)

CFLAGS = $(SIMULATED) -O -pedantic -Wall -fPIC -rdynamic -Wno-unused-variable -Wno-variadic-macros \
	-Wno-unused-but-set-variable \
	$(EXTRAINCS) -DUSE_ETHERCAT
CC = g++ $(CFLAGS)
LDFLAGS = $(EXTRALIBS) -O

SRCS = CallMethodAction.cpp		LogAction.cpp			WaitAction.cpp \
	DebugExtra.cpp			Logger.cpp			beckhoffd.cpp \
	DisableAction.cpp		MachineCommandAction.cpp	cw.cpp \
	Dispatcher.cpp			MachineInstance.cpp		iod.cpp \
	ECInterface.cpp			Message.cpp			iosh.cpp \
	EnableAction.cpp		MessagingInterface.cpp		cwlang.tab.cpp \
	ExecuteMessageAction.cpp	ModbusInterface.cpp		cwlang.yy.cpp \
	Expression.cpp			PredicateAction.cpp		modbusd.cpp \
	ExpressionAction.cpp		ResumeAction.cpp		options.cpp \
	FireTriggerAction.cpp		Scheduler.cpp			persistd.cpp \
	HandleMessageAction.cpp		SendMessageAction.cpp		schedule.cpp \
	IOComponent.cpp			SetStateAction.cpp		symboltable.cpp \
	IODCommands.cpp			ShutdownAction.cpp		test_client.cpp \
	IfCommandAction.cpp		State.cpp			zmq_ecat_monitor.cpp \
	ClearListAction.cpp		SetOperationAction.cpp	\
	LockAction.cpp			UnlockAction.cpp	clockwork.cpp \
	ClientInterface.cpp		MQTTInterface.cpp dynamic_value.cpp value.cpp IncludeAction.cpp \
	SortListAction.cpp SetListEntriesAction.cpp CopyPropertiesAction.cpp \
	MessageLog.cpp filtering.cpp Plugin.cpp

all:	$(APPS)

.cpp.o:	
	$(CC) -c $<

# warning: the following makedepend will not work at present
#depend:
#	makedepend -Y -fMakefile.cw -- -I /usr/include/c++/4.2.1/ $(CFLAGS) -- $(SRCS)

iod.o:	iod.cpp	MessagingInterface.h

iod:	iod.o IODCommand.h ECInterface.o IOComponent.o Message.o MessagingInterface.o anet.o \
			Dispatcher.o dynamic_value.o value.o symboltable.o DebugExtra.o Logger.o \
			cwlang.tab.o cwlang.yy.o Scheduler.o FireTriggerAction.o IfCommandAction.o Expression.o \
			DisableAction.o EnableAction.o ExpressionAction.o LogAction.o PredicateAction.o \
			SortListAction.o SetListEntriesAction.o CopyPropertiesAction.o \
			ClearListAction.o SetOperationAction.o \
			IncludeAction.o LockAction.o UnlockAction.o ModbusInterface.o ResumeAction.o ShutdownAction.o \
			SetStateAction.o WaitAction.o SendMessageAction.o HandleMessageAction.o \
			CallMethodAction.o ExecuteMessageAction.o MachineCommandAction.o IODCommands.o \
			State.o cJSON.o options.o MachineInstance.o Plugin.o \
			PatternAction.o regular_expressions.o clockwork.o ClientInterface.o MQTTInterface.o \
			PersistentStore.o MessageLog.o filtering.o
	$(CC) -o $@ iod.o $(LDFLAGS) \
			ECInterface.o IOComponent.o Message.o MessagingInterface.o anet.o \
			Dispatcher.o dynamic_value.o value.o symboltable.o \
			DebugExtra.o Logger.o State.o cJSON.o options.o \
			Expression.o FireTriggerAction.o IfCommandAction.o \
			DisableAction.o EnableAction.o ExpressionAction.o LogAction.o PredicateAction.o \
			SortListAction.o SetListEntriesAction.o CopyPropertiesAction.o \
			ClearListAction.o SetOperationAction.o \
			IncludeAction.o LockAction.o UnlockAction.o ModbusInterface.o ResumeAction.o ShutdownAction.o \
			SetStateAction.o WaitAction.o SendMessageAction.o HandleMessageAction.o \
			CallMethodAction.o ExecuteMessageAction.o MachineCommandAction.o IODCommands.o \
			MachineInstance.o Plugin.o cwlang.tab.o cwlang.yy.o Scheduler.o \
			PatternAction.o regular_expressions.o clockwork.o ClientInterface.o MQTTInterface.o \
			PersistentStore.o MessageLog.o filtering.o \
			-lzmq $(BOOST_THREAD_LIB) $(BOOST_FILESYSTEM_LIB) $(BOOST_SYSTEM_LIB) \
			$(TOOLLIB) -lethercat -lmosquitto -ldl

beckhoffd:	beckhoffd.cpp IODCommand.h ECInterface.o IOComponent.o Message.o MessagingInterface.o  \
			Logger.o State.o DebugExtra.o cJSON.o options.o MachineInstance.o Plugin.o \
			cwlang.tab.o cwlang.yy.o Scheduler.o FireTriggerAction.o IfCommandAction.o Expression.o \
			ModbusInterface.o SetStateAction.o ExecuteMessageAction.o \
			MachineCommandAction.o HandleMessageAction.o anet.o \
			value.o filtering.o \
			CallMethodAction.o IODCommands.o regular_expressions.o MessageLog.o
	$(CC) -o $@ beckhoffd.cpp $(LDFLAGS) \
			ECInterface.o DebugExtra.o IOComponent.o Message.o MessagingInterface.o anet.o \
			Logger.o State.o cJSON.o options.o MachineInstance.o Plugin.o Dispatcher.o dynamic_value.o \
			value.o symboltable.o Scheduler.o \
			Expression.o FireTriggerAction.o IfCommandAction.o ModbusInterface.o IODCommands.o \
			SetStateAction.o ExecuteMessageAction.o MachineCommandAction.o HandleMessageAction.o \
			CallMethodAction.o regular_expressions.o MessageLog.o filtering.o \
			MQTTInterface.o $(TOOLLIB) -lzmq $(BOOST_THREAD_LIB) $(BOOST_SYSTEM_LIB) \
			-lethercat -lmosquitto -ldl

cw.o:	cw.cpp	MessagingInterface.h
		$(CC) -c -o $@ cw.cpp

cw:	cw.o IODCommand.h ECInterface.o IOComponent.o Message.o MessagingInterface.o anet.o \
			Dispatcher.o dynamic_value.o value.o symboltable.o DebugExtra.o Logger.o \
			State.o cJSON.o options.o MachineInstance.o Plugin.o \
			cwlang.tab.o cwlang.yy.o Scheduler.o FireTriggerAction.o IfCommandAction.o Expression.o \
			DisableAction.o EnableAction.o ExpressionAction.o LogAction.o PredicateAction.o \
			SortListAction.o SetListEntriesAction.o CopyPropertiesAction.o \
			ClearListAction.o SetOperationAction.o \
			IncludeAction.o LockAction.o UnlockAction.o ModbusInterface.o ResumeAction.o ShutdownAction.o \
			SetStateAction.o WaitAction.o SendMessageAction.o HandleMessageAction.o \
			CallMethodAction.o ExecuteMessageAction.o MachineCommandAction.o IODCommands.o \
			PatternAction.o regular_expressions.o clockwork.o ClientInterface.o MQTTInterface.o \
			PersistentStore.o MessageLog.o filtering.o
	$(CC) -o $@ cw.o $(LDFLAGS) \
			ECInterface.o IOComponent.o Message.o MessagingInterface.o anet.o \
			Dispatcher.o dynamic_value.o value.o symboltable.o DebugExtra.o Logger.o \
			State.o cJSON.o options.o MachineInstance.o Plugin.o \
			cwlang.tab.o cwlang.yy.o Scheduler.o Expression.o FireTriggerAction.o IfCommandAction.o \
			DisableAction.o EnableAction.o ExpressionAction.o LogAction.o PredicateAction.o \
			SortListAction.o SetListEntriesAction.o CopyPropertiesAction.o \
			ClearListAction.o SetOperationAction.o \
			IncludeAction.o LockAction.o UnlockAction.o ModbusInterface.o ResumeAction.o ShutdownAction.o \
			SetStateAction.o WaitAction.o SendMessageAction.o HandleMessageAction.o CallMethodAction.o \
			ExecuteMessageAction.o MachineCommandAction.o IODCommands.o PatternAction.o regular_expressions.o \
			PersistentStore.o MessageLog.o filtering.o \
			$(BOOST_THREAD_LIB) $(BOOST_FILESYSTEM_LIB) -lzmq \
			clockwork.o ClientInterface.o MQTTInterface.o -lmosquitto -ldl

persistd:	persistd.cpp dynamic_value.o value.o symboltable.o Logger.o DebugExtra.o \
			MessagingInterface.o cJSON.o options.o PersistentStore.o Message.o anet.o MessageLog.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ persistd.cpp \
		$(BOOST_SYSTEM_LIB) $(BOOST_PROGRAM_OPTIONS_LIB) $(BOOST_THREAD_LIB) \
		value.o symboltable.o Logger.o DebugExtra.o MessagingInterface.o cJSON.o \
		options.o PersistentStore.o Message.o anet.o MessageLog.o -lzmq

modbusd:	modbusd.cpp value.o symboltable.o Logger.o DebugExtra.o MessagingInterface.o \
					options.o cJSON.o Message.o anet.o MessageLog.o
	$(CC) $(LDFLAGS) -o modbusd modbusd.cpp -lzmq $(BOOST_THREAD_LIB) $(BOOST_THREAD_LIB) \
			$(BOOST_PROGRAM_OPTIONS_LIB) $(BOOST_SYSTEM_LIB) \
			value.o symboltable.o Logger.o DebugExtra.o -lmodbus MessagingInterface.o \
				cJSON.o options.o Message.o anet.o MessageLog.o 

iosh: iosh.cpp value.o cmdline.tab.cpp cmdline.yy.cpp \
		cmdline.h Logger.o DebugExtra.o MessagingInterface.o \
		symboltable.o cJSON.o options.o Message.o anet.o MessageLog.o 
	g++ $(CFLAGS) $(LDFLAGS) -DUSE_READLINE -o iosh iosh.cpp \
			value.o cmdline.tab.cpp cmdline.yy.cpp \
			Logger.o DebugExtra.o MessagingInterface.o \
			symboltable.o cJSON.o options.o Message.o anet.o MessageLog.o \
			-lzmq -lreadline $(BOOST_SYSTEM_LIB) $(BOOST_PROGRAM_OPTIONS_LIB)

cmdline.tab.cpp:	cmdline.ypp cmdline.lpp
	yacc -o $@ -g -v -d cmdline.ypp

cmdline.yy.cpp:	cmdline.lpp
	lex -o $@ cmdline.lpp

zmq_ecat_monitor: zmq_ecat_monitor.o
	g++ $(CFLAGS) $(LDFLAGS) -o zmq_ecat_monitor zmq_ecat_monitor.o -lzmq $(BOOST_SYSTEM_LIB)

device_connector:	device_connector.o regular_expressions.o anet.o \
		value.o MessagingInterface.o Logger.o DebugExtra.o cJSON.o symboltable.o options.o \
		Message.o MessageLog.o
	g++ $(CFLAGS) $(LDFLAGS) -o $@ device_connector.o regular_expressions.o anet.o \
		value.o MessagingInterface.o Logger.o DebugExtra.o cJSON.o symboltable.o options.o \
		Message.o MessageLog.o \
		 $(BOOST_THREAD_LIB) $(BOOST_SYSTEM_LIB) -lzmq 

device_connector.o:	device_connector.cpp regular_expressions.h anet.h

cwlang.tab.cpp:	cwlang.ypp cwlang.lpp
	yacc -o $@ -g -v -d cwlang.ypp

cwlang.yy.cpp:	cwlang.lpp
	lex -o $@ cwlang.lpp

clean:
	rm -f $(APPS) *.o cwlang.tab.cpp cwlang.tab.hpp cwlang.yy.cpp
	rm -rf iod.dSYM

# DO NOT DELETE

CallMethodAction.o:	\
		CallMethodAction.h\
		Action.h\
		Message.h\
		value.h\
		symboltable.h\
		DebugExtra.h\
		Logger.h\
		IOComponent.h\
		State.h\
		ECInterface.h\
		MachineInstance.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h

LogAction.o:	\
		LogAction.h\
		Action.h\
		Message.h\
		symboltable.h\
		Logger.h\
		MachineInstance.h\
		State.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h

WaitAction.o:	\
		WaitAction.h\
		Action.h\
		Message.h\
		symboltable.h\
		State.h\
		DebugExtra.h\
		Logger.h\
		IOComponent.h\
		ECInterface.h\
		Scheduler.h\
		FireTriggerAction.h\
		MachineInstance.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h

DebugExtra.o:	\
		Logger.h\
		DebugExtra.h

Logger.o:	\
		Logger.h

beckhoffd.o:	\
		ECInterface.h\
		IOComponent.h\
		State.h\
		Message.h\
		symboltable.h\
		ControlSystemMachine.h\
		cJSON.h\
		Logger.h\
		IODCommands.h\
		IODCommand.h\
		Statistics.h\
		Statistic.h

clockwork.o:

ClientInterface.o:

DisableAction.o:	\
		DisableAction.h\
		Action.h\
		Message.h\
		MachineInstance.h\
		symboltable.h\
		State.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		Logger.h

MachineCommandAction.o:	\
		MachineCommandAction.h\
		Action.h\
		Message.h\
		symboltable.h\
		DebugExtra.h\
		Logger.h\
		IOComponent.h\
		State.h\
		ECInterface.h\
		MachineInstance.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h

cw.o:	\
		ECInterface.h\
		ControlSystemMachine.h\
		IOComponent.h\
		State.h\
		Message.h\
		cJSON.h\
		cwlang.h\
		symboltable.h\
		MachineInstance.h\
		Action.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		options.h\
		Logger.h\
		IODCommand.h\
		Dispatcher.h\
		Statistic.h\
		DebugExtra.h\
		Scheduler.h\
		PredicateAction.h\
		IODCommands.h\
		Statistics.h

Dispatcher.o:	\
		Dispatcher.h\
		Message.h\
		MachineInstance.h\
		symboltable.h\
		State.h\
		Action.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		DebugExtra.h\
		Logger.h

MachineInstance.o:	\
		MachineInstance.h\
		symboltable.h\
		Message.h\
		State.h\
		Action.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		Dispatcher.h\
		IOComponent.h\
		ECInterface.h\
		Logger.h\
		MessagingInterface.h\
		DebugExtra.h\
		Scheduler.h\
		IfCommandAction.h\
		FireTriggerAction.h\
		HandleMessageAction.h\
		ExecuteMessageAction.h\
		CallMethodAction.h

iod.o:	\
		ECInterface.h\
		ControlSystemMachine.h\
		IOComponent.h\
		State.h\
		Message.h\
		cJSON.h\
		cwlang.h\
		symboltable.h\
		MachineInstance.h\
		Action.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		options.h\
		Logger.h\
		IODCommand.h\
		Dispatcher.h\
		Statistic.h\
		DebugExtra.h\
		Scheduler.h\
		PredicateAction.h\
		Statistics.h\
		IODCommands.h

ECInterface.o:	\
		ECInterface.h\
		IOComponent.h\
		State.h\
		Message.h

Message.o:	\
		Message.h

iosh.o:	\
		Logger.h

EnableAction.o:	\
		EnableAction.h\
		Action.h\
		Message.h\
		MachineInstance.h\
		symboltable.h\
		State.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		Logger.h

MessagingInterface.o:	\
		MessagingInterface.h\
		Message.h

cwlang.tab.o:	\
		symboltable.h\
		Logger.h\
		MachineInstance.h\
		Message.h\
		State.h\
		Action.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		IfCommandAction.h\
		EnableAction.h\
		ResumeAction.h\
		DisableAction.h\
		ExpressionAction.h\
		LogAction.h\
		PredicateAction.h\
		DebugExtra.h\
		LockAction.h\
		UnlockAction.h\
		ShutdownAction.h\
		WaitAction.h\
		SendMessageAction.h\
		CallMethodAction.h\
		cwlang.h

ExecuteMessageAction.o:	\
		ExecuteMessageAction.h\
		Action.h\
		Message.h\
		symboltable.h\
		DebugExtra.h\
		Logger.h\
		IOComponent.h\
		State.h\
		ECInterface.h\
		MachineInstance.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h

ModbusInterface.o:	\
		ModbusInterface.h\
		Logger.h\
		DebugExtra.h\
		MessagingInterface.h\
		Message.h

cwlang.yy.o:	\
		cwlang.h\
		symboltable.h\
		MachineInstance.h\
		Message.h\
		State.h\
		Action.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h

Expression.o:	\
		Expression.h\
		symboltable.h\
		MachineInstance.h\
		Message.h\
		State.h\
		Action.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		Logger.h\
		DebugExtra.h

PredicateAction.o:	\
		PredicateAction.h\
		Expression.h\
		symboltable.h\
		Action.h\
		Message.h\
		Logger.h\
		DebugExtra.h\
		MachineInstance.h\
		State.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h

modbusd.o:	\
		Logger.h\
		DebugExtra.h\
		symboltable.h\
		IODCommand.h\
		MessagingInterface.h\
		Message.h

ExpressionAction.o:	\
		ExpressionAction.h\
		Expression.h\
		symboltable.h\
		Action.h\
		Message.h\
		Logger.h\
		MachineInstance.h\
		State.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h

ResumeAction.o:	\
		ResumeAction.h\
		Action.h\
		Message.h\
		MachineInstance.h\
		symboltable.h\
		State.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		Logger.h

options.o:	\
		options.h

FireTriggerAction.o:	\
		FireTriggerAction.h\
		Action.h\
		Message.h\
		MachineInstance.h\
		symboltable.h\
		State.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		Logger.h\
		DebugExtra.h

Scheduler.o:	\
		Logger.h\
		Scheduler.h\
		Action.h\
		Message.h\
		MachineInstance.h\
		symboltable.h\
		State.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		DebugExtra.h

persistd.o:	\
		Logger.h\
		symboltable.h

HandleMessageAction.o:	\
		HandleMessageAction.h\
		Action.h\
		Message.h\
		symboltable.h\
		DebugExtra.h\
		Logger.h\
		IOComponent.h\
		State.h\
		ECInterface.h\
		MachineInstance.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h

SendMessageAction.o:	\
		SendMessageAction.h\
		Action.h\
		Message.h\
		symboltable.h\
		DebugExtra.h\
		Logger.h\
		IOComponent.h\
		State.h\
		ECInterface.h\
		MachineInstance.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h

schedule.o:	\
		Scheduler.h\
		Action.h\
		Message.h

IOComponent.o:	\
		IOComponent.h\
		State.h\
		ECInterface.h\
		Message.h\
		MachineInstance.h\
		symboltable.h\
		Action.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		MessagingInterface.h

SetStateAction.o:	\
		SetStateAction.h\
		Action.h\
		Message.h\
		symboltable.h\
		State.h\
		Expression.h\
		DebugExtra.h\
		Logger.h\
		IOComponent.h\
		ECInterface.h\
		MachineInstance.h\
		ModbusInterface.h\
		MachineCommandAction.h

symboltable.o:	\
		symboltable.h\
		Logger.h\
		DebugExtra.h

IODCommands.o:	\
		IODCommands.h\
		IODCommand.h\
		MachineInstance.h\
		symboltable.h\
		Message.h\
		State.h\
		Action.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		IOComponent.h\
		ECInterface.h\
		Logger.h\
		DebugExtra.h\
		cJSON.h\
		options.h\
		Statistic.h\
		Statistics.h

ShutdownAction.o:	\
		ShutdownAction.h\
		Action.h\
		Message.h\
		MachineInstance.h\
		symboltable.h\
		State.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		Logger.h

test_client.o:	

IfCommandAction.o:	\
		IfCommandAction.h\
		Expression.h\
		symboltable.h\
		Action.h\
		Message.h\
		MachineInstance.h\
		State.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		Logger.h\
		DebugExtra.h

State.o:	\
		State.h

zmq_ecat_monitor.o:	\
		Logger.h

LockAction.o:	\
		LockAction.h\
		Action.h\
		Message.h\
		MachineInstance.h\
		symboltable.h\
		State.h\
		Expression.h\
		ModbusInterface.h\
		SetStateAction.h\
		MachineCommandAction.h\
		Logger.h

