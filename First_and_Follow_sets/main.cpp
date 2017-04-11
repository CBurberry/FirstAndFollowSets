#ifdef _MSC_VER && !__INTEL_COMPILER
#include "windows.h"
#endif

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <ctype.h>
#include <unordered_set>
#include <signal.h>

#define MAX_LINE_LENGTH 512

using namespace std;

//Program to be able to output the FIRST and FOLLOW sets of a given grammar
//#Acceptted format: "A ::= B | C D | E !" where ! denotes end of productions for the LHS symbol
//Sequential elements in a sentential form are represented as "<rule> ::= <form1> <form2> " - trailing space followed by '|' or '!'


/*
	FOLLOW SET ALGORITHM

	Loop through every existing production
	If there is a NT A followed by a symbol b, then everything in FIRST(b) except epsilon,
	is placed in FOLLOW(A).

	If there is a NT A followed by a symbol b, and FIRST(b) contains epsilon,
	then everything in the LHS of the production in which A is contained (we'll name that NT X),
	is in FOLLOW(A). aka X -> Ab && b -> epsilon, then everything in FOLLOW(X) is in FOLLOW(A).

	Same as above if the NT A is the last element in the production.
	aka X -> bA, then everything in FOLLOW(X) is in FOLLOW(A).

	NOTE: If X -> aAbc where there is an NT A followed by element b (FIRST(b) has epsilon)
	& element c (FIRST(c) does not have epsilon) then FIRST(b) is placed in FOLLOW(A)
	& FIRST(c) is placed in FOLLOW(A)

	NOTE 2: in the above example, in the case FIRST(c) also contains epsilon, everything in
	FOLLOW(X) is in FOLLOW(A).
*/

//Debug values
int exec_state = 0;
int method_state = 0;
int loop_state = 0;
bool still_updating = false;

class statement;							//Forward declaration
class grammar_element;
class firstSet;
class followSet;
class followDataContainer;

/*
extern "C" void my_function_to_handle_aborts(int signal_number) 
{
	cerr << "\n\nProgram execution state: " << exec_state << 
		" \n| add_symbol method state: "<< method_state << " \n| loop_state: " << loop_state << endl;

	//keep console open
	cerr << "\n\nAny character key to continue.";
	char c;
	cin >> c;

}*/

class grammar_element
{
public:
	grammar_element() {}
	grammar_element(int i, int t, string v) 
	{
		id = i;
		type = t;
		value = v;
	}
	int id;									//Unique ID
	int type;								//0 - T, 1 - NT, 2- Unknown
	string value = "";						//For both, value for T, representation for NT
	vector<statement> productionList;			//Only for Non-Terminals

	bool operator==(const grammar_element &other) const
	{
		return (value == other.value);
	}
};

namespace std {
	template<>
	struct hash<grammar_element>
	{
		size_t operator()(grammar_element const& k) const
		{
			return hash<string>()(k.value);
		}
	};
}

class statement
{
public:
	statement() {}
	statement(grammar_element& s, vector<grammar_element>& v) 
	{
		source = s;
		rhs = v;
	}
	grammar_element source;					//The LHS of the production
	vector<grammar_element> rhs;			//The RHS of the production
};

class firstSet 
{
	public:
	firstSet() {}
	firstSet(grammar_element& s, unordered_set<grammar_element>& ss) 
	{
		source = s;
		set_elements = ss;
	}
	grammar_element source;
	unordered_set<grammar_element> set_elements = {};
	bool operator==(const firstSet &other) const
	{
		return (source == other.source);
	}
};

namespace std {
	template<>
	struct hash<firstSet>
	{
		size_t operator()(firstSet const& k) const
		{
			return hash<grammar_element>()(k.source);
		}
	};
}
class followSet
{
public:
	followSet() {}
	followSet(grammar_element& s,  unordered_set<grammar_element>& ss)
	{
		source = s;
		defined_elements = ss;
	}
	grammar_element source;
	unordered_set<grammar_element> defined_elements = {};
	unordered_set<grammar_element> undefined_elements = {};
	bool isDefined() 
	{
		return (undefined_elements.size() == 0 && defined_elements.size() > 0);
	}
	bool operator==(const followSet &other) const
	{
		return (source == other.source);
	}
};

namespace std {
	template<>
	struct hash<followSet>
	{
		size_t operator()(followSet const& k) const
		{
			return hash<grammar_element>()(k.source);
		}
	};
}

//class content is identical to firstSet, but defining a new class for code readibility.
class first_plus 
{
public:
	first_plus() {}
	first_plus(grammar_element l, unordered_set<grammar_element>& r)
	{
		lhs = l;
		rhs = r;
	}
	grammar_element lhs;
	unordered_set<grammar_element> rhs;
	bool operator==(const first_plus &other) const
	{
		return (lhs == other.lhs);
	}
};

namespace std {
	template<>
	struct hash<first_plus>
	{
		size_t operator()(first_plus const& k) const
		{
			return hash<grammar_element>()(k.lhs);
		}
	};
}

class followDataContainer
{
public:
	unordered_set<followSet> undefinedSymbols;
	unordered_set<followSet> definedSymbols;

	//0 means undef, 1 means def - the return only indicates a set with the same ID(!) is present.
	bool setContainsElem(int flag, followSet& set);

	//If elems are already in the set, hey are deleted first to update content.
	void add_symbol_to_symbolsets(followSet& elemRef);

	//Returns true if all symbols are defined.
	bool updateDefinitions();
};

bool followDataContainer::setContainsElem(int flag, followSet& set)
{
	if (flag == 0)
	{
		for (auto& elem : undefinedSymbols)
		{
			if (set == elem)
			{
				return true;
			}
		}
	}

	if (flag == 1)
	{
		for (auto& elem : definedSymbols)
		{
			if (set == elem)
			{
				return true;
			}
		}
	}

	return false;
}

void followDataContainer::add_symbol_to_symbolsets(followSet& elemRef)
{
	cout << "\n\t\tSymbol to add to followSetData: " << elemRef.source.value << endl;
	//remove all instances of this element already present.
	if (setContainsElem(0, elemRef))
	{
		undefinedSymbols.erase(undefinedSymbols.find(elemRef));
		cout << "\t\t" << elemRef.source.value << " erased from undefined." << endl;
	}

	if (setContainsElem(1, elemRef))
	{
		definedSymbols.erase(undefinedSymbols.find(elemRef));
		cout << "\t\t" << elemRef.source.value << " erased from undefined." << endl;
	}

	//IF elem is defined
	if (elemRef.isDefined())
	{
		definedSymbols.insert(elemRef);
		cout << "\t\t" << elemRef.source.value << " inserted in defined." << endl;
	}
	//If elem is not defined
	else
	{
		undefinedSymbols.insert(elemRef);
		cout << "\t\t" << elemRef.source.value << " inserted in undefined." << endl;
	}

	cout << "\t\t" << "Set status: defined = { ";
	for (auto& def_elem : elemRef.defined_elements)
	{
		cout << def_elem.value << " ";
	}
	cout << "} | Set status: undefined = { ";
	for (auto& undef_elem : elemRef.undefined_elements)
	{
		cout << undef_elem.value << " ";
	}
	cout << "}\n";
}

bool followDataContainer::updateDefinitions()
{
	followSet currentSet;
	bool stillUndefined = true;
	bool update = false;
	bool debug_out = false;
	if (exec_state == 4)
	{
		debug_out = true;
	}

	//For each undefined followSet,
	for (auto& symbol : undefinedSymbols)
	{
		update = false;
		currentSet = symbol;

		if (debug_out)
		{
			cout << "\nFOLLOW(" << symbol.source.value << "):" << endl;
			cout << "\tUndefined elements: ";
			loop_state = 1;
			for (auto& elem : symbol.undefined_elements)
			{
				cout << elem.value << " ";
			}
		}

		//Check each of the undefined elements of that followSet
		for (auto& elem : symbol.undefined_elements)
		{

			//check all defined followSets
			for (auto& defined : definedSymbols)
			{
				//If there is a defined followSet for that undefined element,
				if (elem == defined.source || elem.value == symbol.source.value)
				{
					if (debug_out)
					{
						cout << "\n\t" << elem.value << " - DEFINED!\n";
						loop_state = 3;
					}

					currentSet.undefined_elements.erase(currentSet.undefined_elements.find(elem));
					if (elem.value != symbol.source.value)
					{
						cout << "\n\tUpdating " << symbol.source.value << "'s defined elements. Adding: \n";
						//Copy FOLLOW set contents
						for (auto& def_elem : defined.defined_elements)
						{
							cout << "\n\t\t" << def_elem.value << "\n";
							currentSet.defined_elements.insert(def_elem);
						}
					}
					stillUndefined = currentSet.isDefined();
					update = true;
					break;
				}
			}


			if (update)
			{
				break;
			}
		}

		if (update)
		{
			break;
		}

		//For each undefined element, undef in elem, if undef contains elem in its undefined elements (Confirm circular def)
		//elem has to change its definition of FOLLOW(undef) in its undefined elements to 
		//	all the defined elements in FOLLOW(undef) to elem's defined & all its undefined elements to elem's undefined
		followSet undef_fs = followSet();
		for (auto& undef_elem : symbol.undefined_elements)
		{
			for (auto& undef_sym : undefinedSymbols)
			{
				if (undef_elem.value == undef_sym.source.value)
				{
					undef_fs = undef_sym;
					break;
				}
			}

			//There was no transitive followset for that undefined element. (aka default constructor value)
			if (undef_fs.undefined_elements.size() == 0)
			{
				//Ignore. Move on.
			}
			else
			{
				for (auto& nested : undef_fs.undefined_elements)
				{
					if (nested.value == symbol.source.value)
					{
						cout << "\n\tUpdating " << symbol.source.value
							<< "'s elements. Resolving Cyclic defintion on " << undef_elem.value << endl;
						currentSet.undefined_elements.erase(currentSet.undefined_elements.find(undef_elem));
						for (auto& undef_fs_def : undef_fs.defined_elements)
						{
							currentSet.defined_elements.insert(undef_fs_def);
						}
						for (auto& undef_fs_undef : undef_fs.undefined_elements)
						{
							//exclude undef_elem if it is here
							if (undef_fs_undef.value == undef_fs.source.value)
							{
								//Exclude
							}
							else
							{
								currentSet.undefined_elements.insert(undef_fs_undef);
							}
						}
						update = true;
						break;
					}
				}
				if (update)
				{
					break;
				}
			}
		}
		if (update)
		{
			break;
		}
	}


	if (update)
	{
		if (debug_out)
		{
			loop_state = 4;
		}
		//Old symbol removed, new symbol inserted.
		add_symbol_to_symbolsets(currentSet);
		cout << "\n" << currentSet.source.value << " Updated!" << endl;
		if (debug_out)
		{
			loop_state = 5;
		}
	}
	else
	{
		cout << "\n\tNo defined element found." << endl;
	}
	still_updating = update;
	return (undefinedSymbols.size() > 0);
}


/*
	Global Variables - I know its bad practice, but I don't want to add complexity with GCC and creating a makefile
	===============================================================================================================
*/
vector<grammar_element> symbolList;
unordered_set<firstSet> firstSetData;
unordered_set<followSet> followSetData;
//The firstset of the next element in the production, see ruling method.
firstSet next_element_fsData;
//The updating set of followData
followDataContainer dataContainer;




void print_all_symbol_data(vector<grammar_element>& symbolList)
{
	for (auto& elem : symbolList)
	{
		string type = "";
		elem.type == 1 ? type = "NT" : type = "T";
		cout << "Token ID: " << elem.id << " | type: " << type << " | value: " << elem.value << endl;
	}
}

void print_all_productions(vector<grammar_element>& symbolList)
{
	int itr = 0;
	int production_itr = 0;
	int list_length = 0;
	for (auto& elem : symbolList)
	{
		if (elem.id >= 100) 
		{
			continue;
		}

		list_length = elem.productionList.size();
		cout << elem.value << " ::= ";
		for (auto& production : elem.productionList) 
		{
			for (auto& symbol : production.rhs) 
			{
				cout << symbol.value << " ";
			}
			if (production_itr < list_length - 1)
			{
				cout << "\n\t| ";
			}
			else
			{
				cout << "\n";
			}
			production_itr++;
		}
		production_itr = 0;
		list_length = 0;
		itr++;
	}
}

//Print followset data to console (intermediate step)
void print_all_followset_data(unordered_set<followSet>& setData) 
{
	for (auto& fset : setData) 
	{
		cout << "\nFOLLOW(" << fset.source.value << "):" << endl
			<< "\n\tUndefined: { ";
		for (auto& g_elem : fset.undefined_elements) 
		{
			cout << g_elem.value << " ";
		}
		cout << "}\n\tDefined: { ";
		for (auto& g_elem : fset.defined_elements) 
		{
			cout << g_elem.value << " ";
		}
		cout << "}\n";
	}
}

//Print firstset data to console & output.
void print_firstSets(unordered_set<firstSet>& setList, ofstream& out)
{
	int itr;
	for (auto& elem : setList)
	{
		itr = 0;
		cout << "Token value: " << elem.source.value << " | FIRST =  { ";
		out << "Token value: " << elem.source.value << " | FIRST =  { ";
		for (auto& g_elem : elem.set_elements)
		{
			cout << g_elem.value;
			out << g_elem.value;
			if (itr != elem.set_elements.size() - 1)
			{
				cout << ", ";
				out << ", ";
			}
			itr++;
		}
		cout << " }\n";
		out << " }\n";
	}
}

//Print followset data to console & output.
void print_followSets(unordered_set<followSet>& fData, followDataContainer& dataContainer, ofstream& out)
{
	for (auto& fset : fData)
	{
		cout << "Token value: " << fset.source.value << " | FOLLOW = { ";
		out << "Token value: " << fset.source.value << " | FOLLOW = { ";
		for (auto& elem : fset.defined_elements)
		{
			cout << elem.value << " ";
			out << elem.value << " ";
		}
		cout << "}" << endl;
		out << "}" << endl;
	}

	if (dataContainer.undefinedSymbols.size() != 0)
	{
		cout << "\n\n======================= UNRESOLVED ELEMENTS (looping) ========================\n\n";
		out << "\n\n======================= UNRESOLVED ELEMENTS (looping) ========================\n\n";
		for (auto& undef_elem : dataContainer.undefinedSymbols)
		{
			cout << "Undefined symbol: " << undef_elem.source.value << endl;
			out << "Undefined symbol: " << undef_elem.source.value << endl;
			for (auto& unresolved : undef_elem.undefined_elements)
			{
				cout << "\tUnresolved element: " << unresolved.value << "\n";
				out << "\tUnresolved element: " << unresolved.value << "\n";
			}
			cout << endl;
			out << endl;
		}
		cout << "\n\n==============================================================================\n\n";
		out << "\n\n==============================================================================\n\n";
	}
}

grammar_element get_elem_by_value(string value, vector<grammar_element>& symbolList)
{
	for (grammar_element& elem : symbolList)
	{
		if (elem.value == value)
		{
			return elem;
		}
	}
	return grammar_element(0, 2, "");
}

grammar_element get_elem_by_ID(int id)
{
	for (grammar_element& elem : symbolList)
	{
		if (elem.id == id)
		{
			return elem;
		}
	}
	return grammar_element(0, 2, "");
}

void update_all_grammar(vector<grammar_element>& symbolList) 
{
	//For each element in the list of symbol Data
	for (auto& elem : symbolList)
	{
		for (auto& production : elem.productionList)
		{
			for (auto& symbol : production.rhs)
			{
				symbol = get_elem_by_value(symbol.value, symbolList);
			}
		}
	}
}


vector<grammar_element> add_all_terminals(ifstream& in) 
{
	vector<grammar_element> elementList = {};
	char line[30];		//Smaller line limit for terminal representations.
	string str;
	int id = 100;

	while (in.getline(line, 30)) 
	{
		str = string(line);
		elementList.push_back(grammar_element(id, 0, str));
	}

	return elementList;
}

//Compute all first sets for list of symbols, uses recursive call.
firstSet compute_first_sets(grammar_element& param)
{
	firstSet fst = firstSet(param, unordered_set<grammar_element>());
	bool epsilonEncountered = false;
	grammar_element updated_elem;
	firstSet elementFirstSet;

	if (param.type == 0) 
	{
		fst.set_elements.insert(param);
	}
	else 
	{
		//For each production in the selected element. - note elements need to be updated before use.
		for (auto& production : param.productionList) 
		{
			epsilonEncountered = false;
			//For each element in the production
			for (auto& elem : production.rhs) 
			{
				unordered_set<firstSet>::iterator ptr;
				updated_elem = get_elem_by_value(elem.value, symbolList);
				//Get the FIRST of the current element.
				//check if its present in firstSetData first - 'first'... haha

				ptr = firstSetData.find(firstSet(updated_elem,unordered_set<grammar_element>()));
				//If the element was not found/defined already.
				if (ptr == firstSetData.end()) 
				{
					elementFirstSet = compute_first_sets(updated_elem);
				}
				else 
				{
					elementFirstSet = *ptr;
				}
				//Determine if this FIRST contains epsilon,
				if (elementFirstSet.set_elements.count(get_elem_by_value("epsilon", symbolList)))
				{
					epsilonEncountered = true;
				}
				else 
				{
					epsilonEncountered = false;
				}
				//Add FIRST(elem) to FIRST(param) excluding epsilon.
				for (auto& fst_elem : elementFirstSet.set_elements) 
				{
					if (fst_elem.value != "epsilon") 
					{
						fst.set_elements.insert(fst_elem);
					}
				}
				//If epsilon was encountered, proceed to the next loop iteration
				if (epsilonEncountered == false) 
				{
					break;
				}
			}
			//If epsilon was encountered until the end, add epsilon to firstSet(param).
			if (epsilonEncountered) 
			{
				fst.set_elements.insert(get_elem_by_value("epsilon", symbolList));
			}
		}	
	}
	firstSetData.insert(fst);
	cout << "\n\tfirstSetData.insert(" << fst.source.value << ")";
	return fst;
}


//pos represents what position currentElem is within the RHS of the production, zero-based
/*
	Rules:
	 - 0 current is a T
	 - 1 current is a NT and is the last element.
	 - 2 current is a NT and is followed by a element with epsilon in its FIRST.
	 - 3 current is a NT and is followed by a element without epsilon in its FIRST

*/
int get_algorithm_ruling(statement& production, grammar_element& currentElem, int pos) 
{
	int result = 0;
	bool lastElement = (production.rhs.size() - 1 == pos);
	grammar_element next;
	bool hasEpsilon = false;
	cout << "\n\t\tRHS size = " << production.rhs.size() << " | pos = " << pos << endl;

	if (currentElem.type == 0) 
	{
		return 0;
	}

	cout << "\n\t\tlastElement: " << (bool)lastElement;

	if (lastElement) 
	{
		return 1;
	}
	else 
	{
		next = production.rhs[pos + 1];
	}

	//Check if next's FIRST contains epsilon
	for (auto& fset : firstSetData) {
		if (fset.source == next)
		{
			next_element_fsData = fset;
			cout << " | next = " << next.value;
			break;
		}
	}

	hasEpsilon = next_element_fsData.set_elements.count(get_elem_by_value("epsilon", symbolList));
	if (hasEpsilon)
	{
		result = 2;
	}
	else
	{
		result = 3;
	}
	cout << " | next contains epsilon? : " << hasEpsilon << endl;
	
	return result;
}


//Compute all follow sets for list of symbols, uses recursive call.
unordered_set<followSet> compute_follow_sets()
{
	exec_state = 1;
	//Data is ordered in symbol id-1
	vector<followSet> data = {};
	unordered_set<grammar_element> temp_set;
	
	//Initialise all symbol followSets
	for (auto& symbol : symbolList) 
	{
		if (symbol.type == 1) 
		{
			if (symbol.value == "goal") 
			{
				temp_set.insert(grammar_element(get_elem_by_value("$", symbolList)));
				data.push_back(followSet(symbol, temp_set));
			}
			else 
			{
				data.push_back(followSet(symbol, unordered_set<grammar_element>()));
			}
		}
	}


	int itr = 0;
	int offset = 0;
	int rule = 0;
	method_state = 0;

	for (auto& symbol : symbolList)
	{
		for (auto& production : symbol.productionList) 
		{
			cout << "\n\n" << symbol.value << " ::= ";
			//Printing Loop
			for (auto& elem : production.rhs)
			{
				cout << elem.value << " ";
			}

			itr = 0;
			for (auto& g_elem : production.rhs) 
			{
				cout << "\n\tChecking: " << g_elem.value << endl;

				offset = 0;
				//Get ruling for current iterator value of production.
				rule = get_algorithm_ruling(production, g_elem, itr);
				cout << "\n\t\tRule: " << rule << " applied." << endl;
				switch (rule)
				{
				case 0: //T
					break;
				case 1: //NT & last
					cout << "\n\t\t\tEverything in FOLLOW(" << production.source.value
						<< "), is placed in FOLLOW(" << data[g_elem.id - 1].source.value << ")" << endl;
					data[g_elem.id - 1].undefined_elements.insert(production.source);
					break;
				case 2: //NT followed by epsilon
					//Done the first time at least
					while (rule == 2) 
					{
						cout << "\n\t\t\tAll in FIRST(" << next_element_fsData.source.value
							<< "), except epsilon, placed in FOLLOW(" << data[g_elem.id - 1].source.value << ")" << endl;
						for (auto& next_elements : next_element_fsData.set_elements)
						{
							if (next_elements.value == "epsilon")
							{
								//Ignore.
							}
							else
							{
								data[g_elem.id - 1].defined_elements.insert(next_elements);
							}
						}
						offset++;
						//Get the ruling of current iterator + offset elements ahead, current position in production is iterator + offset.
						//We set the next firstSet data of the offset element to be added to the current iterator element.
						rule = get_algorithm_ruling(production, production.rhs[itr + offset], itr + offset);
					}
					//Check other rule values resulting and apply appropriate behaviour
					if (rule == 1) 
					{
						cout << "\n\t\t\tEverything in FOLLOW(" << production.source.value
							<< "), is placed in FOLLOW(" << data[g_elem.id - 1].source.value << ")" << endl;
						data[g_elem.id - 1].undefined_elements.insert(production.source);
					}
					else if (rule == 0 ||rule == 3) 
					{
						cout << "\n\t\t\tAll in FIRST(" << next_element_fsData.source.value
							<< ") placed in FOLLOW(" << data[g_elem.id - 1].source.value << ")" << endl;
						for (auto& next_elements : next_element_fsData.set_elements)
						{
							data[g_elem.id - 1].defined_elements.insert(next_elements);
						}
					}
					else 
					{
						//ignore terminal as current element.
					}
					break;
				case 3: //NT not followed by epsilon.
					cout << "\n\t\t\tAll in FIRST(" << next_element_fsData.source.value
						<< ") placed in FOLLOW(" << data[g_elem.id - 1].source.value << ")" << endl;
					for (auto& next_elements : next_element_fsData.set_elements)
					{
						data[g_elem.id - 1].defined_elements.insert(next_elements);
					}
					break;
				}

				//Only print contents on productions.
				if (g_elem.type == 1)
				{
					//print the set contents after each loop
					cout << "\n\t\t\tElement: " << g_elem.value << "= defSet: { ";
					for (auto& dset_elem : data[g_elem.id - 1].defined_elements)
					{
						cout << dset_elem.value << " ";
					}
					cout << "} | undefSet: { ";
					for (auto& udset_elem : data[g_elem.id - 1].undefined_elements)
					{
						cout << udset_elem.value << " ";
					}
					cout << "}" << endl;
				}
				itr++;
			}
		}
	}


	exec_state = 2;
	cout << "\n\nAll symbols processed, adding to data container." << endl;
	//We add all the symbols follow data to dataContainer for refinement / definition resolution.
	for (auto& elem : data) 
	{
		dataContainer.add_symbol_to_symbolsets(elem);
		exec_state = 3;
	}

	exec_state = 4;
	cout << "\n\n================== UNDEFINED FOLLOWSETS ===================\n\n";
	print_all_followset_data(dataContainer.undefinedSymbols);
	cout << "\n\n=================== DEFINED FOLLOWSETS ====================\n\n";
	print_all_followset_data(dataContainer.definedSymbols);

	/*
	//keep console open
	cout << "\n\nAny character key to continue.";
	char c;
	cin >> c;
	*/

	while (dataContainer.updateDefinitions()) 
	{
		//Break if there is no update in data (a cycle occurs)
		if (still_updating == false)
		{
			break;
		}
	}

	/*
	//keep console open
	cout << "\n\nComplete! Any character key to continue.";
	cin >> c;
	*/

	return dataContainer.definedSymbols;
}

//We assume all LHS's FOLLOW and FIRSTS are defined. -  we dont check for disjointness (we know its not LL(1))
unordered_set<first_plus> compute_firstPlusSets(unordered_set<firstSet>& first_data, unordered_set<followSet>& follow_data) 
{
	first_plus fp_elem;
	unordered_set<grammar_element> productionFirstPlusSet;
	unordered_set<first_plus> result;

	for (auto& symbol : symbolList) 
	{
		//If terminal skip
		if (symbol.type == 0) 
		{
			continue;
		}

		bool productionIsNullable = true;
		firstSet temp = firstSet();
		followSet tempFollow = followSet();
		fp_elem = first_plus();
		fp_elem.lhs = symbol;
		productionFirstPlusSet = unordered_set<grammar_element>();

		//Check all productions of this NT
		for (auto& production : symbol.productionList) 
		{
			productionIsNullable = true;

			//For each symbol in this production
			for (auto& elem : production.rhs) 
			{
				//We can search using a firstSet with only the source defined as only source is matched in Hash
				temp = *first_data.find(firstSet(elem, unordered_set<grammar_element>()));

				//Hash for grammar_element also matches by string value, count returns 0 or 1 (not present/present)
				//If Nullable and no epsilon is in temp's FIRST, set nullable to False.
				if (productionIsNullable && temp.set_elements.count(grammar_element(0, 0, "epsilon")) == 0)
				{
					productionIsNullable = false;
				}
				
				//Add all elements that are not epsilon from this elem's (temp) FIRST into our FirstPlus (fp_elem)
				for (auto& f_elem : temp.set_elements) 
				{
					if (f_elem.value != "epsilon") 
					{
						productionFirstPlusSet.insert(f_elem);
					}
				}

				//If the production is no longer Nullable, we don't keep reading in FIRST sets for this production.
				if (productionIsNullable == false) 
				{	
					break;
				}
			}

			//At the end of adding all the FIRSTs, if the production is nullable then we add the LHS's FOLLOW
			if (productionIsNullable)
			{
				//We can search using a followSet with only the source defined as only source is matched in Hash
				tempFollow = *follow_data.find(followSet(symbol, unordered_set<grammar_element>()));
				//We assume all symbols are in defined.
				for (auto& f_elem : tempFollow.defined_elements)
				{
					productionFirstPlusSet.insert(f_elem);
				}
			}
		}
		//At the end of the all productions for a symbol, we save the set of elements in First+(LHS) to a set of First+ to return.
		fp_elem.rhs = productionFirstPlusSet;
		result.insert(fp_elem);
	}
	return result;
}

void print_all_firstPlus(unordered_set<first_plus> data, ofstream& out)
{
	for (auto& fp_elem : data) 
	{
		cout << "\nFIRST_PLUS(" << fp_elem.lhs.value << ") = { ";
		out << "\nFIRST_PLUS(" << fp_elem.lhs.value << ") = { ";
		for (auto& elem : fp_elem.rhs) 
		{
			cout << elem.value << " ";
			out << elem.value << " ";
		}
		cout << "}";
		out << "}";
	}
	cout << endl;
}


int main() 
{
	//Debugging message for Abort()
	//signal(SIGABRT, &my_function_to_handle_aborts);

	int id_itr = 1;		//Value of 0 will be an identifier for unset.

	ofstream ofile;
	ofile.open("FnF_Sets_Output.txt", ios::trunc);

	ifstream ifile;
	ifile.open("language_input.txt");
	char line[MAX_LINE_LENGTH];
	string str;

	ifstream terminalsIn;
	terminalsIn.open("terminals_input.txt");

	//SymbolList becomes populated with filled symboldata and productions whos rhs' are grammar_element(0,2, buffer).
	symbolList = add_all_terminals(terminalsIn);

	bool lhs_symbol_found = false;
	bool production_arrow_found = false;
	string buffer = "";
	string value = "";
	grammar_element current_element;
	int p_itr = 0;

	/*
		After all grammar_symbols & productions are made, must loop over all
		grammar_symbols' productions and set the correct grammar_symbol ID's & type
		in the productions.
	*/
	while (ifile.getline(line, MAX_LINE_LENGTH))
	{
		str = string(line);

		for (char c : str)
		{
			//If character is ws, discard. (should incl. \n, & \t)
			if (iswspace(c) != 0 && !lhs_symbol_found && !production_arrow_found)
			{
				//Consume
				continue;
			}
			else if (c == '!')
			{
				symbolList.push_back(current_element);
				lhs_symbol_found = false;
				buffer.clear();
				value.clear();
				current_element = grammar_element();
				p_itr = 0;
				continue;
			}
			else
			{
				//If we already have the LHS of the production...
				if (lhs_symbol_found)
				{
					if (c == '|')
					{
						current_element.productionList.push_back(statement(current_element, vector<grammar_element>()));
						buffer.clear();
						p_itr++;
					}
					else if (iswspace(c) != 0 && buffer.length() > 0) 
					{
						current_element.productionList[p_itr].rhs.push_back(grammar_element(0,2, buffer));
						buffer.clear();
					}
					else if (iswspace(c) != 0 && buffer.length() == 0) 
					{
						//Consume ws
					}
					else
					{
						buffer.push_back(c);
					}
				}
				else
				{
					if (isalpha(c) || c == '_')
					{
						value.push_back(c);
					}
					else if (c == '=' && value.length() != 0)
					{
						lhs_symbol_found = true;
						current_element = grammar_element(id_itr, 1, value);
						current_element.productionList.push_back(statement(current_element, vector<grammar_element>()));
						id_itr++;
					}
					else
					{
						//Do nothing.
					}
				}
			}
		}
	}
	cout << "Parsing complete!\n";

	update_all_grammar(symbolList);
	cout << "\nUpdating complete!\n";
	for (auto& symbol : symbolList) 
	{
		compute_first_sets(symbol);
	}
	cout << "\nComputing FIRST data complete!";

	
	followSetData = compute_follow_sets();
	cout << "\nComputing FOLLOW data complete!";
	
	//print_all_productions(symbolList);

	cout << "\n\n ============= FIRST SETS ==============\n\n";
	ofile << "\n\n ============= FIRST SETS ==============\n\n";
	print_firstSets(firstSetData, ofile);
	cout << "\n\n =======================================\n\n";
	ofile << "\n\n =======================================\n\n";

	
	cout << "\n\n ============= FOLLOW SETS ==============\n\n";
	ofile << "\n\n ============= FOLLOW SETS ==============\n\n";
	print_followSets(followSetData, dataContainer, ofile);

	cout << "\n\n ============= FIRSTPLUS SETS ==============\n\n";
	ofile << "\n\n ============= FIRSTPLUS SETS ==============\n\n";
	print_all_firstPlus(compute_firstPlusSets(firstSetData, followSetData), ofile);

	//keep console open
	cout << "\n\nEnd Of Program! Any character key to continue.";
	char c;
	cin >> c;

	ifile.close();
	ofile.close();
	return 0;
}