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

#include <stdlib.h>
#include <string.h>
#include "options.h"

static int opt_verbose = 0;
static int opt_test_only = 0;
const char *persistent_store_name  = 0;
const char *modbus_map_name = 0;
const char *debug_config_name = 0;
const char *dependency_graph_name = 0;
static int publisher_port_num = 5556;
static int persistent_port_num = 5557;
static int modbus_port_num = 5558;
static int command_port_num = 5555;
static bool keep_stats = false;
static char *dev_name = 0;

const char *device_name() { return dev_name; }
void set_device_name(const char *new_name) { if (dev_name) free(dev_name); dev_name = strdup(new_name); }


void set_verbose(int trueOrFalse)
{
	opt_verbose = trueOrFalse;
}

int verbose()
{
	return opt_verbose;
}

void set_test_only(int trueOrFalse) {
	opt_test_only = trueOrFalse;
}

int test_only() {
	return opt_test_only;
}

void set_persistent_store(const char *name) { 
	persistent_store_name = name; 
}

const char *persistent_store() { 
	return persistent_store_name; 
}

void set_modbus_map(const char *name) {
	modbus_map_name = name;
}

const char *modbus_map() {
	return modbus_map_name;
}

void set_debug_config(const char *name) {
	debug_config_name = name; 
}

const char *debug_config() {
	return debug_config_name;
}

void set_dependency_graph(const char *name) {
    dependency_graph_name = name;
}

const char *dependency_graph() {
    return dependency_graph_name;
}

void set_publisher_port(int port) {
    publisher_port_num = port;
}

int publisher_port() {
    return publisher_port_num;
}


void set_persistent_store_port(int port) {
    persistent_port_num = port;
}

int persistent_store_port() {
    return persistent_port_num;
}

void set_modbus_port(int port) {
    modbus_port_num = port;
}

int modbus_port() {
    return modbus_port_num;
}

void set_command_port(int port) {
    command_port_num = port;
}

int command_port() {
    return command_port_num;
}

void enable_statistics(bool which) {
    keep_stats = which;
}

int keep_statistics() {
    return keep_stats;
}


