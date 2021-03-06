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

#include "Expression.h"
#include "MachineInstance.h"
#include "Logger.h"
#include "DebugExtra.h"
#include <iomanip>
#include "regular_expressions.h"
#include "Scheduler.h"
#include "FireTriggerAction.h"
#include "dynamic_value.h"
#include "MessageLog.h"

std::ostream &operator <<(std::ostream &out, const Predicate &p) { return p.operator<<(out); }
    std::ostream &operator<<(std::ostream &out, const PredicateOperator op) {
        const char *opstr;
        switch (op) {
            case opNone: opstr = "None"; break;
            case opGE: opstr = ">="; break;
            case opGT: opstr = ">"; break;
            case opLE: opstr = "<="; break;
            case opLT: opstr = "<"; break;
            case opEQ: opstr = "=="; break;
            case opNE: opstr = "!="; break;
            case opAND: opstr = "AND"; break;
            case opOR: opstr = "OR"; break;
			case opNOT: opstr = "NOT"; break;
			case opUnaryMinus: opstr = "-"; break;
			case opPlus: opstr = "+"; break;
			case opMinus: opstr = "-"; break;
			case opTimes: opstr = "*"; break;
			case opDivide: opstr = "/"; break;
			case opMod: opstr = "%"; break;
			case opAssign: opstr = ":="; break;
            case opMatch: opstr = "~"; break;
            case opBitAnd: opstr = "&"; break;
            case opBitOr: opstr = "|"; break;
            case opNegate: opstr = "~"; break;
            case opBitXOr: opstr = "^"; break;
            case opAny: opstr = "ANY"; break;
            case opAll: opstr = "ALL"; break;
            case opCount: opstr = "COUNT"; break;
            case opIncludes: opstr = "INCLUDES"; break;
        }
        return out << opstr;
    }

static bool stringEndsWith(const std::string &str, const std::string &subs) {
	size_t n1 = str.length();
	size_t n2 = subs.length();
	if (n1 >= n2 && str.substr(n1-n2) == subs) 
		return true;
	return false;
}

Condition::~Condition() {
    delete predicate;
}

Predicate::~Predicate() {
    delete left_p;
    delete right_p;
    if (dyn_value) delete dyn_value;
}

bool Predicate::usesTimer(Value &timer_val) const {
	if (left_p) {
		if (!left_p->left_p) {
			if (left_p->entry.kind == Value::t_symbol 
                && (left_p->entry.token_id == ClockworkToken::TIMER
                    || stringEndsWith(left_p->entry.sValue,".TIMER"))) {
				//DBG_MSG << "Copying timer value " << right_p->entry << "\n";
                timer_val = right_p->entry;
				return true;
			}
			else 
				return false;
		}
		return left_p->usesTimer(timer_val) || right_p->usesTimer(timer_val);
	}
	if (entry.kind == Value::t_symbol && entry.token_id == ClockworkToken::TIMER)
		return true; 
	else 
		return false;
}

void Predicate::findTimerClauses(std::list<Predicate*>&clauses) {
	if (left_p) {
        if (!left_p->left_p) {
            if (left_p->entry.kind == Value::t_symbol
                && (left_p->entry.token_id == ClockworkToken::TIMER
                    || stringEndsWith(left_p->entry.sValue,".TIMER"))) {
                clauses.push_back(this);
			}
        }
        else
            left_p->findTimerClauses(clauses);
    }
    if (right_p) {
        if (!right_p->left_p) {
            if (right_p->entry.kind == Value::t_symbol
                && (right_p->entry.token_id == ClockworkToken::TIMER || stringEndsWith(right_p->entry.sValue,".TIMER"))) {
                    clauses.push_back(this);
            }
        }
        else {
            right_p->findTimerClauses(clauses);
        }
    }
}

Value &Predicate::getTimerValue() {
    if (left_p->entry.kind == Value::t_symbol
        && (left_p->entry.token_id == ClockworkToken::TIMER || stringEndsWith(left_p->entry.sValue,".TIMER"))) {
        return right_p->entry;
    }
    if (right_p->entry.kind == Value::t_symbol
        && (right_p->entry.token_id == ClockworkToken::TIMER || stringEndsWith(right_p->entry.sValue,".TIMER"))) {
        return left_p->entry;
    }
    return SymbolTable::Null;
}

void Predicate::scheduleTimerEvents(MachineInstance *target) // setup timer events that trigger the supplied machine
{
    long scheduled_time = -10000;
    long current_time = 0;
    // below, we check the clauses of this predicate and if we find a timer test
    // we set the above variables. At the end of the method, we actually set the timer

	if (left_p && left_p) left_p->scheduleTimerEvents(target);
    
    // clauses like (machine.TIMER >= 10)
    if (left_p && left_p->entry.kind == Value::t_symbol
            && left_p->entry.token_id == ClockworkToken::TIMER && right_p) {

        if (right_p->entry.kind == Value::t_symbol && target->getValue(right_p->entry.sValue).asInteger(current_time))
            ;
        else if (right_p->entry.asInteger(scheduled_time))
            current_time = target->getTimerVal()->iValue;
        else
            DBG_MSG << "Error: clause " << *this << " does not yield an integer comparison\n";
    }
    else if (left_p && left_p->entry.kind == Value::t_symbol && stringEndsWith(left_p->entry.sValue,".TIMER")) {
        if (right_p->entry.kind == Value::t_symbol && target->getValue(right_p->entry.sValue).asInteger(current_time))
            ;
        else if (right_p->entry.asInteger(scheduled_time)) {
            // lookup the machine
            MachineInstance *timed_machine = 0;
            size_t pos = left_p->entry.sValue.find('.');
            std::string machine_name(left_p->entry.sValue);
            machine_name.erase(pos);
            timed_machine = target->lookup(machine_name);
            if (timed_machine) current_time = timed_machine->getTimerVal()->iValue;
        }
        else
            DBG_MSG << "Error: clause " << *this << " does not yield an integer comparison\n";
    }
    
    // clauses like (10 <= machine.TIMER)
    else if (right_p && right_p->entry.kind == Value::t_symbol
                && right_p->entry.token_id == ClockworkToken::TIMER && left_p) {
        if (left_p->entry.kind == Value::t_symbol && target->getValue(left_p->entry.sValue).asInteger(current_time))
            ;
        else if (left_p->entry.asInteger(scheduled_time))
            current_time = target->getTimerVal()->iValue;
        else
            DBG_MSG << "Error: clause " << *this << " does not yield an integer comparison\n";
    }
    else if (right_p && left_p && right_p->entry.kind == Value::t_symbol
                    && stringEndsWith(right_p->entry.sValue,".TIMER")) {
        if (left_p->entry.kind == Value::t_symbol && target->getValue(left_p->entry.sValue).asInteger(current_time))
            ;
        else if (left_p->entry.asInteger(scheduled_time)) {
            // lookup and cache the machine
            MachineInstance *timed_machine = 0;
            size_t pos = right_p->entry.sValue.find('.');
            std::string machine_name(right_p->entry.sValue);
            machine_name.erase(pos);
            timed_machine = target->lookup(machine_name);
            if (timed_machine) current_time = timed_machine->getTimerVal()->iValue;
        }
        else
            DBG_MSG << "Error: clause " << *this << " does not yield an integer comparison\n";
    }
    else {
        
    }
    //TBD there is an issue with testing current_time <= scheduled_time because there may have been some
    // processing delays and current time may already be a little > scheduled time. This is especially
    // true on slow clock cycles. For now we reschedule the trigger for up to 10ms past the necessary time.
    if (current_time <= scheduled_time + 10) {
        Trigger *trigger = new Trigger("Timer");
        Scheduler::instance()->add(new ScheduledItem( (scheduled_time - current_time) * 1000, new FireTriggerAction(target, trigger)));
        trigger->release();
    }
    else if (scheduled_time > 0) {
        DBG_SCHEDULER << "no event scheduled for " << ( (target)?target->getName() : "unknown" ) << ".  over time\n";
    }
    if (right_p) right_p->scheduleTimerEvents(target);
}

void Predicate::clearTimerEvents(MachineInstance *target) // clear all timer events scheduled for the supplid machine
{
	if (left_p) {
        left_p->clearTimerEvents(target);
        if (entry.kind == Value::t_symbol
                && (left_p->entry.token_id == ClockworkToken::TIMER
                        || stringEndsWith(left_p->entry.sValue,".TIMER"))) {
            DBG_SCHEDULER << "clear timer event for entry " << entry << "\n";
        }
    }
    if (right_p) right_p->clearTimerEvents(target);
}


std::ostream &Predicate::operator <<(std::ostream &out) const {
    if (left_p) {
        out << "(";
		if (op != opNOT) left_p->operator<<(out); // ignore the lhs for NOT operators
        out << " " << op << " ";
        if (right_p) right_p->operator<<(out);
        out << ")";
    }
    else {
        if (cached_entry) {
            out << entry;
            if (entry.kind == Value::t_symbol) out << " (" << *cached_entry << ")";
        }
        else if (last_calculation) {
            out << entry;
            if (entry.kind == Value::t_symbol || entry.kind == Value::t_dynamic)
                out <<  " (" << *last_calculation << ") ";
        }
        else out << " " << entry;
	}
    return out;
}


// Conditions
Predicate::Predicate(const Predicate &other) : left_p(0), op(opNone), right_p(0) {
	if (other.left_p) left_p = new Predicate( *(other.left_p) );
	op = other.op;
	if (other.right_p) right_p = new Predicate( *(other.right_p) );
	entry = other.entry;
    if (other.entry.dyn_value) {
        entry.dyn_value = DynamicValue::ref(other.entry.dyn_value->clone());
        //dyn_value = DynamicValue::ref(other.dyn_value); // note shared copy, should be a shared pointer
    }
    if (other.dyn_value)
        dyn_value = new Value(DynamicValue::ref(other.dyn_value->dyn_value));
    else
        dyn_value = 0;
	entry.cached_machine = 0; // do not preserve any cached values in this clone
	priority = other.priority;
	mi = 0;
    cached_entry = 0;
    lookup_error = false;
    last_calculation = 0;
    needs_reevaluation = true;
}

Predicate &Predicate::operator=(const Predicate &other) {
	if (other.left_p) left_p = new Predicate( *(other.left_p) );
	op = other.op;
	if (other.right_p) right_p = new Predicate( *(other.right_p) );
	entry = other.entry;
    if (other.entry.dyn_value) {
        entry.dyn_value = DynamicValue::ref(other.entry.dyn_value->clone());
        //dyn_value = DynamicValue::ref(other.dyn_value); // note shared copy, should be a shared pointer
    }
    if (other.dyn_value)
        dyn_value = new Value(DynamicValue::ref(other.dyn_value->dyn_value));
    else
        dyn_value = 0;
	entry.cached_machine = 0; // do not preserve any cached machine pointers in this clone
	priority = other.priority;
	mi = 0;
    cached_entry = 0; // do not preserve cached the value pointer
    lookup_error = false;
    last_calculation = 0;
    needs_reevaluation = true;
	return *this;
}

void Predicate::flushCache() {
    cached_entry = 0;
    last_calculation = 0;
    needs_reevaluation = true;
    if (dyn_value) {
        delete dyn_value;
        dyn_value = 0;
    }
    /*if (op == opNone) {
        if (entry.kind == Value::t_dynamic) {
            //entry.dyn_value->flushCache();
            entry = entry.sValue; // force resolution of dynamic values again
        }
        //entry.cached_machine = 0;
        //entry.cached_value = &SymbolTable::Null;
    }
    if (left_p) left_p->flushCache();
    if (right_p) right_p->flushCache();*/
     
}

Condition::Condition(Predicate*p) : predicate(0) {
    if (p) predicate = new Predicate(*p);
}

Condition::Condition(const Condition &other) : predicate(0) {
	if (other.predicate)
        predicate = new Predicate( *(other.predicate) );
}

Condition &Condition::operator=(const Condition &other) {
	if (other.predicate)
		predicate = new Predicate( *(other.predicate) );
	return *this;
}

std::ostream &operator<<(std::ostream&out, const Stack &s) {
    return s.operator<<(out);
}

std::ostream &Stack::operator<<(std::ostream&out) const {
#if 1
    // prefix
    BOOST_FOREACH(ExprNode n, stack) {
		if (n.kind == ExprNode::t_op) {
			out << n.op << " "; 
		}
		else {
            if (n.node) out << *n.node;
			out << " (" << *n.val<< ") ";
		}
    }
    return out;
#else
    // disabled due to error
    std::list<ExprNode>::const_iterator iter = stack.begin();
    std::list<ExprNode>::const_iterator end = stack.end();
    return traverse(out, iter, end);
#endif
}

std::ostream &Stack::traverse(std::ostream &out, std::list<ExprNode>::const_iterator &iter, std::list<ExprNode>::const_iterator &end) const
{
    // note current error with display of rhs before lhs
    if (iter == end) return out;
    const ExprNode &n = *iter++;
    if (n.kind == ExprNode::t_op) {
        out << "(";
        if (n.op != opNOT) // ignore lhs for the not operator
            traverse(out, iter, end);
        out <<" " << n.op << " " ;
        traverse(out, iter, end);
        out << ")";
    }
    else {
        if (n.node) out << *n.node;
        out << " ("<< *n.val << ") ";
    }
    return out;
}

class ClearFlagOnReturn {
public:
    ClearFlagOnReturn(bool *val) : to_clear(val) { }
    ~ClearFlagOnReturn() {
        *to_clear = false;
    }
private:
    bool *to_clear;
};

Value *resolveCacheMiss(Predicate *p, MachineInstance *m, bool left, bool reevaluate) {
	Value *v = &p->entry;
	p->clearError();
	if (v->kind == Value::t_symbol) {
		// lookup machine and get state. hopefully we have already cached a pointer to the machine
		MachineInstance *found = 0;
		if (p->mi)
			found = p->mi;
		else {
#if 0
            const Value *prop = 0;
#endif
			// before looking up machines, check for specific keywords
		 	if (left && v->sValue == "DEFAULT") { // default state has a low priority but always returns true
				p->cached_entry = &SymbolTable::True;
				return p->cached_entry;
            }
			else if (v->sValue == "SELF") {
				//v = m->getCurrent().getName();
				p->cached_entry = m->getCurrentStateVal();
				return p->cached_entry;
			}
            else if (v->sValue == "TRUE") {
				p->cached_entry = &SymbolTable::True;
				return p->cached_entry;
            }
            else if (v->sValue == "FALSE") {
				p->cached_entry = &SymbolTable::False;
				return p->cached_entry;
            }
            else if (m->hasState(v->sValue)) {
				p->cached_entry = v;
				return p->cached_entry;
            }
#if 1
            else {
                return m->resolve(v->sValue);
            }
#else
            else if ((prop = &m->properties.lookup(v->sValue.c_str()))) {
                return prop;
            }
			else {
				prop = &m->getValue(v->sValue); // property lookup
				if (*prop != SymbolTable::Null) {
	                // do not cache timer values. TBD subclass Value for dynamic values..
	                if ( !stringEndsWith(v->sValue, ".TIMER"))
	                    p->cached_entry = prop;
                    else
                        p->last_calculation = prop;
					return prop;
				}
			}
			found = m->lookup(p->entry);
			p->mi = found; // cache the machine we just looked up with the predicate
			if (p->mi) {
				// found a machine, make sure we get to hear if that machine changes state or if its properties change
				if (p->mi != m) {
					p->mi->addDependancy(m); // ensure this condition will be updated when p->mi changes
					m->listenTo(p->mi);
				}
			}
			else {
				std::stringstream ss;
				ss << "## - Warning: " << m->getName() << " couldn't find a machine called " << p->entry;
				p->setErrorString(ss.str());
				//DBG_MSG << p->errorString() << "\n";
			}
#endif
		}
		// if we found a reference to a machine but that machine is a variable or constant, we
		// are actually interested in its 'VALUE' property
		if (found) {
            Value *res = found->getCurrentValue();
            if (*res == SymbolTable::Null)
                p->cached_entry = 0;
            else
                p->cached_entry = res;
            return p->cached_entry;
            /*
			if (found->_type == "VARIABLE" || found->_type == "CONSTANT") {
				p->cached_entry = &found->getValue("VALUE");
				return p->cached_entry;
			}
			else {
				p->cached_entry = found->getCurrentStateVal();
				return p->cached_entry;
			}
             */
		}
	}
    else if (v->kind == Value::t_dynamic) {
        return v;
    }
	return v;
}

Value *resolve(Predicate *p, MachineInstance *m, bool left, bool reevaluate) {
    if (reevaluate) {
        p->flushCache();
    }
    ClearFlagOnReturn cfor(&p->needs_reevaluation);
	// return the cached pointer to the value if we have one
	if (p->cached_entry)
        return p->cached_entry;
    return resolveCacheMiss(p, m, left, reevaluate);
}

ExprNode eval_stack();
void prep(Predicate *p, MachineInstance *m, bool left);

ExprNode eval_stack(MachineInstance *m, std::list<ExprNode>::const_iterator &stack_iter){
    ExprNode o(*stack_iter++);
    if (o.kind != ExprNode::t_op) {
        if (o.val && o.val->kind == Value::t_dynamic) {
            o.val->dynamicValue()->operator()(m);
            return o.val->dynamicValue()->lastResult();
        }
        return o;
    }
    Value lhs, rhs;
    ExprNode b(eval_stack(m, stack_iter));
    assert(b.kind != ExprNode::t_op);
    if (b.val && b.val->kind == Value::t_dynamic) {
        rhs = b.val->dynamicValue()->operator()(m);
        if (o.op == opAND && rhs.kind == Value::t_bool && rhs.bValue == false) return rhs;
        if (o.op == opOR && rhs.kind == Value::t_bool && rhs.bValue == true) return rhs;
    }
    else if (b.val) rhs = *b.val;
    ExprNode a(eval_stack(m, stack_iter));
    assert(a.kind != ExprNode::t_op);
    if (a.val && a.val->kind == Value::t_dynamic) {
        lhs = a.val->dynamicValue()->operator()(m);
    }
    else if (a.val) lhs = *a.val;
    switch (o.op) {
        case opGE: return lhs >= rhs;
        case opGT: return lhs > rhs;
        case opLE: return lhs <= rhs;
        case opLT: return lhs < rhs;
        case opEQ: return lhs == rhs;
        case opNE: return lhs != rhs;
        case opAND: return lhs && rhs;
        case opOR: return lhs || rhs;
		case opNOT: return !(rhs);
		case opUnaryMinus: return - rhs;
		case opPlus:  return lhs + rhs;
		case opMinus: return lhs - rhs;
		case opTimes: return lhs * rhs;
		case opDivide:return lhs / rhs;
		case opMod:   return lhs % rhs;
        case opBitAnd: return lhs & rhs;
        case opBitOr: return lhs | rhs;
        case opNegate: return ~ rhs;
        case opBitXOr: return lhs ^ rhs;
		case opAssign:return rhs;
        case opMatch: return matches(a.val->asString().c_str(), b.val->asString().c_str());
        case opAny:
        case opCount:
        case opAll:
        case opIncludes:
        {
            //DynamicValue *dv = b.val->dynamicValue();
            //if (dv) return dv->operator()(m);
            //return SymbolTable::False;
            return a.val;
        }
            break;
		case opNone:
            return Value(0);
    }
    return o;
}

/*
 TBD : add a preliminary check for a state test, of one of the
 two forms:   machine IS state  or   state IS machine
 
 If this is the situation, we identify the machine and state and resolve
 the state within the scope of that machine.
 
 */

bool prep(Stack &stack, Predicate *p, MachineInstance *m, bool left, bool reevaluate) {
    if (p->left_p && p->left_p->op == opNone && p->right_p && p->right_p->op == opNone && (p->op == opEQ || p->op == opNE)) {
        if (p->left_p->entry.kind == Value::t_symbol && p->right_p->entry.kind == Value::t_symbol) {
            MachineInstance *lhm = m->lookup(p->left_p->entry);
            MachineInstance *rhm = m->lookup(p->right_p->entry);
            if (lhm && !rhm) {
                if (lhm->hasState(p->right_p->entry.sValue)) {
                    p->right_p->entry.kind = Value::t_string;
                    p->left_p->dyn_value = new Value(new MachineValue(lhm, p->left_p->entry.sValue));
                }
            }
            else if (rhm && !lhm) {
                if (rhm->hasState(p->left_p->entry.sValue)) {
                    p->left_p->entry.kind = Value::t_string;
                    p->right_p->dyn_value = new Value(new MachineValue(rhm, p->right_p->entry.sValue));
                }
            }
        }
    }
    
    if (p->left_p) {
        if (!prep(stack, p->left_p, m, true, reevaluate)) return false;
		//if (p->left_p->mi)
        //    std::cout << *(p->left_p) << " refers to a machine\n";
        if (!prep(stack, p->right_p, m, false, reevaluate)) return false;
		//if (p->left_p->mi)
        //    std::cout << *(p->right_p) << " refers to a state\n";
        stack.push(p->op);
    }
    else {
        Value *result = resolve(p, m, left, reevaluate);
        if (*result == SymbolTable::Null) {
            return false; //result = &p->entry;
        }
        p->last_calculation = result;
        //std::cout << " result: " << *result << "\n";
        if (p->dyn_value) {
            stack.push(ExprNode(result, p->dyn_value));
        }
        else if (p->entry.kind == Value::t_dynamic) {
            stack.push(ExprNode(result, &p->entry));
        }
        else
            stack.push(ExprNode(result, &p->entry));
    }
    return true;
}

ExprNode::~ExprNode() {
}

void Stack::clear() {
    stack.clear();
}

Value Predicate::evaluate(MachineInstance *m) {
    if (stack.stack.size() == 0)
        if (!prep(stack, this, m, true, needs_reevaluation)) {
            std::stringstream ss;
            ss << m->getName() << " Predicate failed to resolve: " << *this << "\n";
            MessageLog::instance()->add(ss.str().c_str());
            return false;
        }
    //Stack work(stack);
    std::list<ExprNode>::const_iterator work = stack.stack.begin();
    Value res = *(eval_stack(m, work).val);
    struct timeval now;
    gettimeofday(&now, 0);
    long t = now.tv_sec*1000000 + now.tv_usec;
    last_evaluation_time = t;
    return res;
}

bool Condition::operator()(MachineInstance *m) {
	if (predicate) {
        struct timeval now;
        if (predicate->last_evaluation_time < m->lastStateEvaluationTime() )
            predicate->stack.stack.clear();
	    if (predicate->stack.stack.size() == 0 )
            if (!prep(predicate->stack, predicate, m, true, predicate->needs_reevaluation)) {
                std::stringstream ss;
                ss << m->getName() << " condition failed: predicate failed to resolve: " << *this->predicate << "\n";
                MessageLog::instance()->add(ss.str().c_str());
                return false;
            }
        //std::cout << m->getName() << " Expression Stack: " << predicate->stack << "\n";
        //Stack work(predicate->stack);
        std::list<ExprNode>::const_iterator work = predicate->stack.stack.begin();
	    ExprNode res(eval_stack(m, work));
        last_result = *res.val;
        std::stringstream ss;
        ss << last_result << " " << *predicate;
        gettimeofday(&now, 0);
        long t = now.tv_sec*1000000 + now.tv_usec;
        predicate->last_evaluation_time = t;
        last_evaluation = ss.str();
	    if (last_result.kind == Value::t_bool) return last_result.bValue;
	}
    return false;
}

