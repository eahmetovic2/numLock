/*----------------------------------------------------------------------
Compiler Generator Coco/R,
Copyright (c) 1990, 2004 Hanspeter Moessenboeck, University of Linz
extended by M. Loeberbauer & A. Woess, Univ. of Linz
ported to C++ by Csaba Balazs, University of Szeged
with improvements by Pat Terry, Rhodes University

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License as published by the 
Free Software Foundation; either version 2, or (at your option) any 
later version.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
for more details.

You should have received a copy of the GNU General Public License along 
with this program; if not, write to the Free Software Foundation, Inc., 
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

As an exception, it is allowed to write an extension of Coco/R that is
used as a plugin in non-free software.

If not otherwise stated, any source code generated by Coco/R (other than 
Coco/R itself) does not fall under the GNU General Public License.
-----------------------------------------------------------------------*/


#include <wchar.h>
#include "Parser.h"
#include "Scanner.h"




void Parser::SynErr(int n) {
	if (errDist >= minErrDist) errors->SynErr(la->line, la->col, n);
	errDist = 0;
}

void Parser::SemErr(const wchar_t* msg) {
	if (errDist >= minErrDist) errors->Error(t->line, t->col, msg);
	errDist = 0;
}

void Parser::Get() {
	for (;;) {
		t = la;
		la = scanner->Scan();
		if (la->kind <= maxT) { ++errDist; break; }

		if (dummyToken != t) {
			dummyToken->kind = t->kind;
			dummyToken->pos = t->pos;
			dummyToken->col = t->col;
			dummyToken->line = t->line;
			dummyToken->next = NULL;
			coco_string_delete(dummyToken->val);
			dummyToken->val = coco_string_create(t->val);
			t = dummyToken;
		}
		la = t;
	}
}

void Parser::Expect(int n) {
	if (la->kind==n) Get(); else { SynErr(n); }
}

void Parser::ExpectWeak(int n, int follow) {
	if (la->kind == n) Get();
	else {
		SynErr(n);
		while (!StartOf(follow)) Get();
	}
}

bool Parser::WeakSeparator(int n, int syFol, int repFol) {
	if (la->kind == n) {Get(); return true;}
	else if (StartOf(repFol)) {return false;}
	else {
		SynErr(n);
		while (!(StartOf(syFol) || StartOf(repFol) || StartOf(0))) {
			Get();
		}
		return StartOf(syFol);
	}
}

void Parser::numLock() {
		ParseList=L"(NUMLOCK "; scopepos=0; 
		while (la->kind == 1) {
			Definition();
		}
		Expect(0);
		ParseList+=L")"; wstringstream data; wstringstream text;
		data << "%include \"asm_io.inc\"" << endl;		
		text << L"section .text" << endl;
		text << L" global _start" << endl << L"_start:" << endl;	
		data << L"section .data" << endl;
		int iluud=0;
		Compile(0,iluud, text, data); std::wcout<<ParseList;
		std::ofstream myfile("test.asm", std::ofstream::binary);
		data << endl << text.str() << L"INT 80h" << endl << L"MOV EAX,1" << endl << L"MOV EBX,0" << endl << L"INT 80h;";	
		wstring ws(&data.str()[0]);
		string stsr(ws.begin(),ws.end());
		myfile.write(&stsr[0],stsr.size());	
		myfile.close(); 
}

void Parser::Definition() {
		Name name; int position;
		while (!(la->kind == 0 || la->kind == 1)) {SynErr(27); Get();}
		position=ParseList.length(); 
		VarList();
		Expect(3);
		while (StartOf(1)) {
			Statement();
		}
}

void Parser::Ident(Name name) {
		Expect(1);
		wcscpy(name, t->val); 
}

void Parser::ConstVal() {
		Name name; 
		
		Expect(2);
		wcscpy(name, t->val); ParseList.append(L"(BROJ ");
		ParseList.append(name); ParseList.append(L")"); 
}

void Parser::VarList() {
		Name name; int position; 
		position=ParseList.length(); 
		Ident(name);
		if (IsDeclaredGlobal(name))  AlreadyErr(name) ; 
		ParseList.insert(position,L"(GVARDEF (NAME "); 
		ParseList.append(name);
		ParseList.append(L")) "); 
		while (la->kind == 4) {
			Get();
			VarList();
		}
}

void Parser::Statement() {
		int position; 
		while (!(StartOf(2))) {SynErr(28); Get();}
		position=ParseList.length(); 
		if (la->kind == 1 || la->kind == 2) {
			StatementExpression();
		} else if (la->kind == 7) {
			IfStatement();
		} else if (la->kind == 9) {
			LoopStatement();
		} else if (la->kind == 5) {
			Print();
		} else SynErr(29);
}

void Parser::StatementExpression() {
		Expression();
		Expect(3);
}

void Parser::IfStatement() {
		int position; bool haselse;
		haselse=false; 
		Expect(7);
		position=ParseList.length(); 
		Expression();
		CompoundStatement();
		if (la->kind == 8) {
			Get();
			CompoundStatement();
			haselse=true; 
		}
		ParseList.insert(position,haselse? L"(IFELSE ": L"(IF ");ParseList.append(L")" );  
}

void Parser::LoopStatement() {
		Name name; int position; 
		position=ParseList.length(); 
		Expect(9);
		if (la->kind == 2) {
			ConstVal();
		} else if (la->kind == 1) {
			Ident(name);
			if (!IsDeclaredGlobal(name))  UndecErr(name) ; 
			ParseList.append(L"(VAR " );ParseList.append(name );ParseList.append(L")" );
		} else SynErr(30);
		CompoundStatement();
		ParseList.insert(position, L"(LOOP "); ParseList.append(L")"); 
}

void Parser::Print() {
		Name name; int position; 
		position=ParseList.length(); 
		Expect(5);
		if (la->kind == 2) {
			ConstVal();
		} else if (la->kind == 1) {
			Ident(name);
			if (!IsDeclaredGlobal(name))  UndecErr(name) ; 
			ParseList.append(L"(VAR " );ParseList.append(name );ParseList.append(L")" );
		} else SynErr(31);
		ParseList.insert(position, L"(PRINT "); ParseList.append(L")"); 
		Expect(3);
}

void Parser::CompoundStatement() {
		Expect(6);
		ParseList.append(L"(BLOCK "); 
		while (StartOf(1)) {
			Statement();
		}
		Expect(6);
		ParseList.append(L") "); 
}

void Parser::Expression() {
		AssignmentExp();
}

void Parser::AssignmentExp() {
		int position; 
		position=ParseList.length(); 
		LogANDExp();
		while (StartOf(3)) {
			if (la->kind == 10) {
				Get();
				AssignmentExp();
				if(!(Assignable(position))) { SemErr(L"Not assignable");} 
				ParseList.insert(position,L"(MOV " ); 
				ParseList.append(L") " );
			} else if (la->kind == 11) {
				Get();
				AssignmentExp();
				if(!(Assignable(position))) { SemErr(L"Not assignable");} 
				ParseList.insert(position,L"(MULTMOV " ); 
				ParseList.append(L") " );
			} else if (la->kind == 12) {
				Get();
				AssignmentExp();
				if(!(Assignable(position))) { SemErr(L"Not assignable");} 
				ParseList.insert(position,L"(DIVMOV " ); 
				ParseList.append(L") " );
			} else if (la->kind == 13) {
				Get();
				AssignmentExp();
				if(!(Assignable(position))) { SemErr(L"Not assignable");} 
				ParseList.insert(position,L"(ADDMOV " ); 
				ParseList.append(L") " );
			} else {
				Get();
				AssignmentExp();
				if(!(Assignable(position))) { SemErr(L"Not assignable");} 
				ParseList.insert(position,L"(SUBMOV " ); 
				ParseList.append(L") " );
			}
		}
}

void Parser::LogANDExp() {
		int position; 
		position=ParseList.length(); 
		EqualExp();
		while (la->kind == 15) {
			Get();
			EqualExp();
			ParseList.insert(position,L"(AND " ); 
			ParseList.append(L") " );
		}
}

void Parser::EqualExp() {
		int position; 
		position=ParseList.length(); 
		RelationExp();
		while (la->kind == 16 || la->kind == 17) {
			if (la->kind == 16) {
				Get();
				RelationExp();
				ParseList.insert(position,L"(EQU " ); 
				ParseList.append(L") " );
			} else {
				Get();
				RelationExp();
				ParseList.insert(position,L"(NEQU " ); 
				ParseList.append(L") " );
			}
		}
}

void Parser::RelationExp() {
		int position; 
		position=ParseList.length(); 
		AddExp();
		while (StartOf(4)) {
			if (la->kind == 18) {
				Get();
				AddExp();
				ParseList.insert(position,L"(LESSTHAN " ); 
				ParseList.append(L") " );
			} else if (la->kind == 19) {
				Get();
				AddExp();
				ParseList.insert(position,L"(GREATERTHAN " ); 
				ParseList.append(L") " );
			} else if (la->kind == 20) {
				Get();
				AddExp();
				ParseList.insert(position,L"(LESSEQUTHAN " ); 
				ParseList.append(L") " );
			} else {
				Get();
				AddExp();
				ParseList.insert(position,L"(GREATEREQUTHAN " ); 
				ParseList.append(L") " );
			}
		}
}

void Parser::AddExp() {
		int position; 
		position=ParseList.length(); 
		MultExp();
		while (la->kind == 22 || la->kind == 23) {
			if (la->kind == 22) {
				Get();
				MultExp();
				ParseList.insert(position,L"(ADD " ); 
				ParseList.append(L") " );
			} else {
				Get();
				MultExp();
				ParseList.insert(position,L"(SUB " ); 
				ParseList.append(L") " );
			}
		}
}

void Parser::MultExp() {
		int position; 
		position=ParseList.length(); 
		PostFixExp();
		while (la->kind == 24 || la->kind == 25) {
			if (la->kind == 24) {
				Get();
				PostFixExp();
				ParseList.insert(position,L"(MUL " ); 
				ParseList.append(L") " );
			} else {
				Get();
				PostFixExp();
				ParseList.insert(position,L"(DIV " ); 
				ParseList.append(L") " );
			}
		}
}

void Parser::PostFixExp() {
		int position; 
		position=ParseList.length(); 
		Primary();
}

void Parser::Primary() {
		Name name; int position; 
		if (la->kind == 1) {
			Ident(name);
			if (!IsDeclaredGlobal(name))  UndecErr(name) ; 
			ParseList.append(L"(VAR " );ParseList.append(name );ParseList.append(L")" );
		} else if (la->kind == 2) {
			ConstVal();
		} else SynErr(32);
}




// If the user declared a method Init and a mehtod Destroy they should
// be called in the contructur and the destructor respctively.
//
// The following templates are used to recognize if the user declared
// the methods Init and Destroy.

template<typename T>
struct ParserInitExistsRecognizer {
	template<typename U, void (U::*)() = &U::Init>
	struct ExistsIfInitIsDefinedMarker{};

	struct InitIsMissingType {
		char dummy1;
	};
	
	struct InitExistsType {
		char dummy1; char dummy2;
	};

	// exists always
	template<typename U>
	static InitIsMissingType is_here(...);

	// exist only if ExistsIfInitIsDefinedMarker is defined
	template<typename U>
	static InitExistsType is_here(ExistsIfInitIsDefinedMarker<U>*);

	enum { InitExists = (sizeof(is_here<T>(NULL)) == sizeof(InitExistsType)) };
};

template<typename T>
struct ParserDestroyExistsRecognizer {
	template<typename U, void (U::*)() = &U::Destroy>
	struct ExistsIfDestroyIsDefinedMarker{};

	struct DestroyIsMissingType {
		char dummy1;
	};
	
	struct DestroyExistsType {
		char dummy1; char dummy2;
	};

	// exists always
	template<typename U>
	static DestroyIsMissingType is_here(...);

	// exist only if ExistsIfDestroyIsDefinedMarker is defined
	template<typename U>
	static DestroyExistsType is_here(ExistsIfDestroyIsDefinedMarker<U>*);

	enum { DestroyExists = (sizeof(is_here<T>(NULL)) == sizeof(DestroyExistsType)) };
};

// The folloing templates are used to call the Init and Destroy methods if they exist.

// Generic case of the ParserInitCaller, gets used if the Init method is missing
template<typename T, bool = ParserInitExistsRecognizer<T>::InitExists>
struct ParserInitCaller {
	static void CallInit(T *t) {
		// nothing to do
	}
};

// True case of the ParserInitCaller, gets used if the Init method exists
template<typename T>
struct ParserInitCaller<T, true> {
	static void CallInit(T *t) {
		t->Init();
	}
};

// Generic case of the ParserDestroyCaller, gets used if the Destroy method is missing
template<typename T, bool = ParserDestroyExistsRecognizer<T>::DestroyExists>
struct ParserDestroyCaller {
	static void CallDestroy(T *t) {
		// nothing to do
	}
};

// True case of the ParserDestroyCaller, gets used if the Destroy method exists
template<typename T>
struct ParserDestroyCaller<T, true> {
	static void CallDestroy(T *t) {
		t->Destroy();
	}
};

void Parser::Parse() {
	t = NULL;
	la = dummyToken = new Token();
	la->val = coco_string_create(L"Dummy Token");
	Get();
	numLock();
	Expect(0);
}

Parser::Parser(Scanner *scanner) {
	maxT = 26;

	ParserInitCaller<Parser>::CallInit(this);
	dummyToken = NULL;
	t = la = NULL;
	minErrDist = 2;
	errDist = minErrDist;
	this->scanner = scanner;
	errors = new Errors();
}

bool Parser::StartOf(int s) {
	const bool T = true;
	const bool x = false;

	static bool set[5][28] = {
		{T,T,T,x, x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{x,T,T,x, x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{T,T,T,x, x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{x,x,x,x, x,x,x,x, x,x,T,T, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,T,T, T,T,x,x, x,x,x,x}
	};



	return set[s][la->kind];
}

Parser::~Parser() {
	ParserDestroyCaller<Parser>::CallDestroy(this);
	delete errors;
	delete dummyToken;
}

Errors::Errors() {
	count = 0;
}

void Errors::SynErr(int line, int col, int n) {
	wchar_t* s;
	switch (n) {
			case 0: s = coco_string_create(L"EOF expected"); break;
			case 1: s = coco_string_create(L"ident expected"); break;
			case 2: s = coco_string_create(L"broj expected"); break;
			case 3: s = coco_string_create(L"\"-\" expected"); break;
			case 4: s = coco_string_create(L"\",\" expected"); break;
			case 5: s = coco_string_create(L"\"000\" expected"); break;
			case 6: s = coco_string_create(L"\"/\" expected"); break;
			case 7: s = coco_string_create(L"\"+\" expected"); break;
			case 8: s = coco_string_create(L"\"++\" expected"); break;
			case 9: s = coco_string_create(L"\"*\" expected"); break;
			case 10: s = coco_string_create(L"\"01\" expected"); break;
			case 11: s = coco_string_create(L"\"014\" expected"); break;
			case 12: s = coco_string_create(L"\"015\" expected"); break;
			case 13: s = coco_string_create(L"\"012\" expected"); break;
			case 14: s = coco_string_create(L"\"013\" expected"); break;
			case 15: s = coco_string_create(L"\"09\" expected"); break;
			case 16: s = coco_string_create(L"\"011\" expected"); break;
			case 17: s = coco_string_create(L"\"018\" expected"); break;
			case 18: s = coco_string_create(L"\"06\" expected"); break;
			case 19: s = coco_string_create(L"\"07\" expected"); break;
			case 20: s = coco_string_create(L"\"016\" expected"); break;
			case 21: s = coco_string_create(L"\"017\" expected"); break;
			case 22: s = coco_string_create(L"\"02\" expected"); break;
			case 23: s = coco_string_create(L"\"03\" expected"); break;
			case 24: s = coco_string_create(L"\"04\" expected"); break;
			case 25: s = coco_string_create(L"\"05\" expected"); break;
			case 26: s = coco_string_create(L"??? expected"); break;
			case 27: s = coco_string_create(L"this symbol not expected in Definition"); break;
			case 28: s = coco_string_create(L"this symbol not expected in Statement"); break;
			case 29: s = coco_string_create(L"invalid Statement"); break;
			case 30: s = coco_string_create(L"invalid LoopStatement"); break;
			case 31: s = coco_string_create(L"invalid Print"); break;
			case 32: s = coco_string_create(L"invalid Primary"); break;

		default:
		{
			wchar_t format[20];
			coco_swprintf(format, 20, L"error %d", n);
			s = coco_string_create(format);
		}
		break;
	}
	wprintf(L"-- line %d col %d: %ls\n", line, col, s);
	coco_string_delete(s);
	count++;
}

void Errors::Error(int line, int col, const wchar_t *s) {
	wprintf(L"-- line %d col %d: %ls\n", line, col, s);
	count++;
}

void Errors::Warning(int line, int col, const wchar_t *s) {
	wprintf(L"-- line %d col %d: %ls\n", line, col, s);
}

void Errors::Warning(const wchar_t *s) {
	wprintf(L"%ls\n", s);
}

void Errors::Exception(const wchar_t* s) {
	wprintf(L"%ls", s); 
	exit(1);
}


