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

#ifndef __MACHINECOMMAND_ACTION
#define __MACHINECOMMAND_ACTION 1

#include <iostream>
#include "Action.h"
#include "symboltable.h"

/*
  A MachineCommand executes a list of actions, 
 usage example:
 
    MachineCommand f;
    f.addAction(new WaitAction,new ActionParameterList(1));
    f.addAction(new SetStateAction,new ActionParameterList(output, "on"));
	Action::Status status = *(f);
    if (status == Action::Error) {
        // handle error
    }
    else {
    }
 */

class MachineInstance;
	
struct MachineCommandTemplate : public ActionTemplate {
    MachineCommandTemplate(CStringHolder cmd_name, CStringHolder state)
    : command_name(cmd_name), state_name(state), timeout(0) { }
    virtual Action *factory(MachineInstance *mi);

    std::ostream &operator<<(std::ostream &out) const {
       return out << command_name.get() << " " << state_name.get();
    }
    void setActionTemplates(std::list<ActionTemplate*> &new_actions);
    void setActionTemplate(ActionTemplate *action);
    
    std::vector<ActionTemplate*> action_templates;
    CStringHolder command_name;
    CStringHolder state_name;
    long timeout;
};

class MachineCommand : public Action {
public:
    MachineCommand(MachineInstance *mi, MachineCommandTemplate *mct);
    ~MachineCommand();
    void addAction(Action *a, ActionParameterList *params);
    Status runActions();
    Status run();
    Status checkComplete();
    void reset();
    void setActions(std::list<Action*> &new_actions);
    const std::string name() const { return command_name.get(); }
    const std::string stateName() const { return state_name.get(); }
    virtual std::ostream &operator<<(std::ostream &out)const;
private:
    std::vector<Action*> actions;
    unsigned int current_step;
    CStringHolder command_name;
    CStringHolder state_name;
    long timeout;
    Trigger *timeout_trigger;
};

#endif