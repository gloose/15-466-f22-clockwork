#include "Compiler.hpp"
#include <fstream>
#include "data_path.hpp"
#include <iostream>
#include <cassert>
#include <iterator>
#include <memory>

// Constructor doesn't do anything at the moment
Compiler::Compiler() {
}

// Reads code from filename and compiles it into an Executable struct.
// If compilation fails, prints some error message and returns nullptr.
Compiler::Executable* Compiler::compile(std::string filename) {
    // Open file at dist/filename
    std::ifstream ifile(data_path(filename), std::ios::binary);
    
    // Vector of vectors of strings, each vector represents a line of text words
    Program program;

    { // Read all lines of text from file into program
        Line line;
        std::string word;

        // Appends a word to the line
        auto addWord = [&]() {
            if (word.size() > 0) {
                line.push_back(word);
                word.clear();
            }
        };

        // Appends a line to the program
        auto addLine = [&]() {
            addWord();
            if (line.size() > 0) {
                program.push_back(line);
                line.clear();
            }
        };

        // Parse text into words and lines
        char c;
        while (ifile.get(c)) {
            if (c == ' ' || c == '\t') {
                addWord();
            } else if (c == '.' || c == '(' || c == ')') {
                addWord();
                word = c;
                addWord();
            } else if (c == '\n' || c == '\r') {
                if (c == '\r') {
                    assert(ifile.get() == '\n' && "Expected \\r\\n");
                }
                addLine();
            } else {
                word = word + c;
            }
        }
        addLine();
    }

    // Compile the executable
    Executable* exe = new Executable();
    auto it = program.begin();
    while (it != program.end()) {
        size_t line_num = std::distance(program.begin(), it) + 1;
        exe->statements.push_back(parseStatement(program, it));
        if (exe->statements.back() == nullptr) {
            std::cout << "Failed to parse statement on line " << line_num << std::endl;
            delete exe;
            return nullptr;
        }
    }
    return exe;
}

// Parses an if statement or an action statement at the current line.
// Advances the line iterator if successful
Compiler::Statement* Compiler::parseStatement(Program& program, Program::iterator& line_it) {
    Statement* out;
    
    // Attempt to parse the line as an if statement
    out = parseIfStatement(program, line_it);
    if (out != nullptr) {
        return out;
    }
    
    // Attempt to parse the line as an action statement
    out = parseActionStatement(program, line_it);
    if (out != nullptr) {
        return out;
    }

    // If all else failed, return nullptr
    return nullptr;
}

// Parses a line of the form "object1.action(object2)".
// Advances the line iterator if successful.
Compiler::ActionStatement* Compiler::parseActionStatement(Program& program, Program::iterator& line_it) {
    Line::iterator word_it = line_it->begin();
    ActionStatement* out = new ActionStatement();

    if (parseObject(word_it, &out->object)
     && parseWord(word_it, ".")
     && parseAction(word_it, out->object, &out->func, &out->duration)
     && parseWord(word_it, "(")
     && parseObject(word_it, &out->target)
     && parseWord(word_it, ")")) {
        std::advance(line_it, 1);
        return out;
    }

    delete out;
    return nullptr;
}

// Parses a line of the form "if (value1 comparator value2)", 
// as well as all statements up to the next "end" line.
// Advances the line iterator if successful.
Compiler::IfStatement* Compiler::parseIfStatement(Program& program, Program::iterator& line_it) {
    Program::iterator old_it = line_it;

    Line::iterator word_it = line_it->begin();
    IfStatement* out = new IfStatement();

    // Check if the first line matches the if statement format
    if (parseWord(word_it, "if")
     && parseWord(word_it, "(")
     && parseCondition(word_it, &out->condition)
     && parseWord(word_it, ")")) {
        // If so, parse all subsequent lines into out->statements until end is reached
        std::advance(line_it, 1);
        while (line_it != program.end()) {
            word_it = line_it->begin();
            size_t line_num = std::distance(program.begin(), line_it) + 1;

            // On end, return successfully
            if (parseWord(word_it, "end")) {
                std::advance(line_it, 1);
                return out;
            }

            // Otherwise, parse next statement
            out->statements.push_back(parseStatement(program, line_it));

            // If the parse failed, print error message and return failure
            if (out->statements.back() == nullptr) {
                std::cout << "Failed to parse statement on line " << line_num << std::endl;
                delete out;
                line_it = old_it;
                return nullptr;
            }
        }

        std::cout << "If statement must be closed with and end" << std::endl;
    }

    line_it = old_it;
    delete out;
    return nullptr;
}

// Attempts to parse a word as the name of an object.
// Advances the word iterator if successful.
bool Compiler::parseObject(Line::iterator& word_it, Object** out) {
    auto obj = objects.find(*word_it);
    if (obj != objects.end()) {
        *out = obj->second;
        std::advance(word_it, 1);
        return true;
    }
    return false;
}

// Attempts to parse a word as the name of a legal action for the given object.
// Advances the word iterator if successful.
bool Compiler::parseAction(Line::iterator& word_it, Object* obj, ActionFunction* out_func, float* out_dur) {
    auto act = obj->actions.find(*word_it);
    if (act != obj->actions.end()) {
        *out_func = act->second.func;
        if (out_dur != nullptr) {
            *out_dur = act->second.duration;
        }
        std::advance(word_it, 1);
        return true;
    }
    return false;
}

// Attempts to parse a sequence of words as a condition of the form:
// value1 comparator value2
// Advances the word iterator if successful.
bool Compiler::parseCondition(Line::iterator& word_it, Condition* out) {
    Line::iterator old_it = word_it;

    if (parseValue(word_it, &out->left)
     && parseComparator(word_it, &out->comparator)
     && parseValue(word_it, &out->right)) {
        return true;
    }

    word_it = old_it;
    return false;
}

// Attempts to parse a word as a comparator.
// Currently, only =, <, and > are supported.
// Advances the word iterator if successful.
bool Compiler::parseComparator(Line::iterator& word_it, std::string* out) {
    std::string comp = *word_it;
    if (parseWord(word_it, "=")
     || parseWord(word_it, "<")
     || parseWord(word_it, ">")) {
        *out = comp;
        return true;
    }
    return false;
}

// Attempts to parse a word or sequence of words as either
// an integer value or an object property.
// Advances the word iterator if successful.
bool Compiler::parseValue(Line::iterator& word_it, int** out) {
    if (parsePropertyValue(word_it, out)
     || parseIntValue(word_it, out)) {
        return true;
    }

    return false;
}

// Attempts to parse a word as an integer value.
// Advances the word iterator if successful.
bool Compiler::parseIntValue(Line::iterator& word_it, int** out) {
    int val;
    try {
        val = std::stoi(*word_it);
    } catch (...) {
        return false;
    }
    *out = new int();
    **out = val;
    std::advance(word_it, 1);
    return true;
}

// Attempts to parse a sequence of words as an object property value,
// of the form: object.property
// Advances the word iterator if successful.
bool Compiler::parsePropertyValue(Line::iterator& word_it, int** out) {
    Line::iterator old_it = word_it;
    Object* obj;
    
    if (parseObject(word_it, &obj)
     && parseWord(word_it, ".")
     && parseProperty(word_it, obj, out)) {
        return true;
    }

    word_it = old_it;
    return false;
}

// Attempts to parse a word as the name of a property of the given object.
// Advances the word iterator if successful.
bool Compiler::parseProperty(Line::iterator& word_it, Object* obj, int** out) {
    auto prop = obj->properties.find(*word_it);
    if (prop != obj->properties.end()) {
        *out = prop->second;
        std::advance(word_it, 1);
        return true;
    }
    return false;
}

// Attempts to parse a word that is equal to the given word.
// Advances the word iterator if successful.
bool Compiler::parseWord(Line::iterator& word_it, std::string word) {
    if (*word_it == word) {
        std::advance(word_it, 1);
        return true;
    }
    return false;
}

// Virtual destructor to prevent compiler warnings
Compiler::Statement::~Statement() {
}

// If statement constructor just sets statement type
Compiler::IfStatement::IfStatement() {
    type = IF_STATEMENT;
    duration = 0.25f;
}

// Action statement constructor just sets statement type
Compiler::ActionStatement::ActionStatement() {
    type = ACTION_STATEMENT;
}

// Statement superclass doesn't implement next
Compiler::Statement* Compiler::Statement::next() {
    assert(false && "Statement::next must be overridden by child class");
    return nullptr;
}

// Statement superclass doesn't implement execute
void Compiler::Statement::execute() {
    assert(false && "Statement::execute must be overridden by child class");
}

// Action statement only returns itself
Compiler::Statement* Compiler::ActionStatement::next() {
    if (current_line == 0) {
        current_line++;
        return this;
    }
    return nullptr;
}

// Execute action statement by calling the action function with arguments object, target
void Compiler::ActionStatement::execute() {
    func(object, target);
}

// Exeecute an if statement by checking if its condition is true,
// then executing all statements contained within it if so.
Compiler::Statement* Compiler::IfStatement::next() {
    if (current_line == 0) {
        current_line++;
        return this;
    } else if (truth && current_line - 1 < statements.size()) {
        return statements[current_line++ - 1]->next();
    }

    return nullptr;
}

void Compiler::IfStatement::execute() {
    if (condition.comparator == "=") {
        truth = *condition.left == *condition.right;
    } else if (condition.comparator == "<") {
        truth = *condition.left < *condition.right;
    } else if (condition.comparator == ">") {
        truth = *condition.left > *condition.right;
    } else {
        std::cout << "Invalid comparator " << condition.comparator << " in if statement" << std::endl;
    }
}

// Execute an executable by executing all of its statements
void Compiler::Executable::execute() {
    Statement* statement;
    while (true) {
        statement = next();
        if (statement) {
            statement->execute();
        } else {
            break;
        }
    }
}

// Get the next statement to be executed
Compiler::Statement* Compiler::Executable::next() {
    Statement* statement = nullptr;
    while (current_line < statements.size()) {
        statement = statements[current_line]->next();
        if (statement == nullptr) {
            current_line++;
        } else {
            break;
        }
    }

    return statement;
}

// Add object to compiler's object map
void Compiler::addObject(Object* obj) {
    objects.emplace(obj->name, obj);
}

// Construct object with name
Compiler::Object::Object(std::string name) : name(name) {
}

// Construct action with function and duration
Compiler::Action::Action(ActionFunction func, float duration) : func(func), duration(duration) {
}

// Add action to object's map of valid actions
void Compiler::Object::addAction(std::string action_name, ActionFunction func, float duration) {
    actions.emplace(action_name, Action(func, duration));
}

// Add property to object's map of properties
void Compiler::Object::addProperty(std::string property_name, int default_value) {
    properties.emplace(property_name, new int(default_value));
}

// Returns a reference to the property named property_name
// If the object has no such property, create one with default value 0
int& Compiler::Object::property(std::string property_name) {
    auto prop = properties.find(property_name);
    if (prop == properties.end()) {
        addProperty(property_name, 0);
        prop = properties.find(property_name);
    }
    return *prop->second;
}

// Placeholder function to be called on an attack action
void attackFunction(Compiler::Object* obj, Compiler::Object* target) {
    if (obj->property("health") > 9) {
        std::cout << "Object " << obj->name << " attacks target " << target->name << std::endl;
        target->property("health")--;
    } else {
        std::cout << "Object " << obj->name << " is too weak to attack target " << target->name << std::endl;
    }
}

// Placeholder function to be called on a defend action
void defendFunction(Compiler::Object* obj, Compiler::Object* target) {
    std::cout << "Object " << obj->name << " defends target " << target->name << std::endl;
}

/*
// Compile the code in code.txt, then execute it.
int main(int argc, char** argv) {
    Compiler compiler = Compiler();

    { // Create the warrior object
        Compiler::Object* obj = new Compiler::Object("warrior");
        obj->addAction("attack", &attackFunction, 1);
        obj->addAction("defend", &defendFunction, 0.5);
        obj->addProperty("health", 10);
        compiler.addObject(obj);
    }

    // Compile the code in code.txt
    Compiler::Executable* exe = compiler.compile("code.txt");

    // If compilation succeeded, execute it
    if (exe != nullptr) {
        exe->execute();
        delete exe;
    }
}
*/