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

#include "ExecuteMessageAction.h"
#include "DebugExtra.h"
#include "Logger.h"
#include "IOComponent.h"
#include "MachineInstance.h"

// ExecuteMessageAction - immediately execute a triggers and handle a message on a machine

Action *ExecuteMessageActionTemplate::factory(MachineInstance *mi) { 
  return new ExecuteMessageAction(mi, *this); 
}

std::ostream &ExecuteMessageActionTemplate::operator<<(std::ostream &out) const {
    return out << "ExecuteMessageActionTemplate " << message.get() << " " << target.get() << "\n";
}

Action::Status ExecuteMessageAction::run() {
	owner->start(this);
	if (!target_machine) target_machine = owner->lookup(target.get());

	if (target_machine) target_machine->execute(Message(message.get()), owner);
	status = Action::Complete;
	owner->stop(this);
	return status;
}

Action::Status ExecuteMessageAction::checkComplete() {
	return Action::Complete;
}

std::ostream &ExecuteMessageAction::operator<<(std::ostream &out) const {
    return out << "ExecuteMessageAction " << message.get() << " " << target.get() << "\n";
}


