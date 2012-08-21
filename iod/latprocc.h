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

#ifndef latprocc_latprocc_h
#define latprocc_latprocc_h

#include <list>
#include "symboltable.h"
#include "MachineInstance.h"
#include "Expression.h"

/*
typedef union {
	int iVal;
	const char *sVal;
	const char *symbol;
	Predicate *expr;
	PredicateOperator opr;
	Value *val;
} mytype;
*/
//#undef YYSTYPE
//#define YYSTYPE mytype

void yyerror(const char *str);

#ifndef __MAIN__
extern int line_num;
extern SymbolTable globals;
extern const char *yyfilename;
#endif

//#ifndef COMPILER_MAIN
//extern std::list<MachineInstance> machines;
//#endif


#endif
