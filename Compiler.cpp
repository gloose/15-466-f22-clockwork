#include "Compiler.hpp"
#include <fstream>
#include "data_path.hpp"
#include <iostream>
#include <cassert>
#include <iterator>
#include <memory>
#include <algorithm>

// Constructor doesn't do anything at the moment
Compiler::Compiler() {
}

// Read a vector of strings into the Program format that can be compiled
Compiler::Program Compiler::readProgram(std::vector<std::string> lines) {
    // Vector of vectors of strings, each vector represents a line of text words
    Program program;

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
        program.push_back(line);
        line.clear();
    };

    // Parse text into words and lines
    for (size_t i = 0; i < lines.size(); i++) {
        for (size_t j = 0; j < lines[i].size(); j++) {
            char c = (char)toupper(lines[i][j]);

            if (c == ' ' || c == '\t') {
                addWord();
            } else if (c == '.' || c == '(' || c == ')') {
                addWord();
                word = c;
                addWord();
            } else {
                word = word + c;
            }
        }
        addLine();
    }
    addLine();

    return program;
}

// Read a file into the Program format that can be compiled
Compiler::Program Compiler::readProgram(std::string filename) {
    // Open file at dist/filename
    std::ifstream ifile(data_path(filename), std::ios::binary);

    // Read the file into a line vector
    std::vector<std::string> lines;
    char line[MAX_LINE_SIZE];
    while (ifile.getline(line, MAX_LINE_SIZE)) {
        std::string str(line);
        if (str[str.size() - 1] == '\r') {
            str = str.substr(0, str.size() - 1);
        }
        lines.push_back(str);
    }

    // Pass the lines to the usual readProgram function
    return readProgram(lines);
}

// Compiles a program into an Executable struct.
// If compilation fails, prints some error message and returns nullptr.
Compiler::Executable* Compiler::compile(Program program) {
    // Compile the executable
    Executable* exe = new Executable();
    auto line_it = program.begin();
    error_message = "";
    bool success = parseStatementBlock(program, line_it, &exe->statements, "");

    if (success && exe->statements.empty()) {
        success = false;
        set_error(0, "Cannot submit an empty program.");
    }

    if (!success) {
        delete exe;
        return nullptr;
    }

    return exe;
}

// Overload to compile from a text file
Compiler::Executable* Compiler::compile(std::string filename) {
    return compile(readProgram(filename));
}

// Overload to compile from a vector of strings
Compiler::Executable* Compiler::compile(std::vector<std::string> lines) {
    return compile(readProgram(lines));
}

// Parses an if statement or an action statement at the current line.
// Advances the line iterator if successful
Compiler::Statement* Compiler::parseStatement(Program& program, Program::iterator& line_it) {
    Statement* out;

    size_t line_num = std::distance(program.begin(), line_it);

    // Attempt to parse the line as an if statement
    out = parseIfStatement(program, line_it);
    if (out != nullptr) {
        out->line_num = line_num;
        return out;
    }

    // Attempt to parse the line as a while statement
    out = parseWhileStatement(program, line_it);
    if (out != nullptr) {
        out->line_num = line_num;
        return out;
    }
    
    // Attempt to parse the line as an action statement
    out = parseActionStatement(program, line_it);
    if (out != nullptr) {
        out->line_num = line_num;
        return out;
    }

    // If all else failed, return nullptr
    return nullptr;
}

// Parses a line of the form "object1.action(object2)".
// Advances the line iterator if successful.
Compiler::ActionStatement* Compiler::parseActionStatement(Program& program, Program::iterator& line_it) {
    size_t line_num = std::distance(program.begin(), line_it);
    Line::iterator word_it = line_it->begin();
    ActionStatement* out = new ActionStatement();

    std::string obj = *word_it;
    if (parseObject(line_it, word_it, &out->object)) {
        if (parseWord(line_it, word_it, ".")) {
            if (parseAction(line_it, word_it, out->object, &out->func, &out->base_duration)) {
                if (parseWord(line_it, word_it, "(")) {
                    if (parseObject(line_it, word_it, &out->target)) {
                        if (parseWord(line_it, word_it, ")")) {
                            line_it++;
                            out->duration = out->base_duration;
                            return out;
                        } else {
                            set_error(line_num, "Action statement missing a ')'");
                        }
                    } else if (word_it != line_it->end()) {
                        set_error(line_num, "Action target '" + *word_it + "' is not a valid object");
                    } else {
                        set_error(line_num, "Action statement missing a target after '('.");
                    }
                } else {
                    set_error(line_num, "Action statement missing a '('");
                }
            } else if (word_it != line_it->end()) {
                set_error(line_num, "Invalid action '" + *word_it + "' for object '" + obj + "'.");
            } else {
                set_error(line_num, "Action statement missing an action after '.'");
            }
        } else {
            set_error(line_num, "Action statement missing a '.' after object name.");
        }
    }

    delete out;
    return nullptr;
}

// Parses a line of the form "if (value1 comparator value2)", 
// as well as all statements up to the next "end" line.
// Advances the line iterator if successful.
Compiler::IfStatement* Compiler::parseIfStatement(Program& program, Program::iterator& line_it) {
    Program::iterator old_it = line_it;

    size_t line_num = std::distance(program.begin(), line_it);
    Line::iterator word_it = line_it->begin();
    IfStatement* out = new IfStatement();

    // Check if the first line matches the if statement format
    if (parseWord(line_it, word_it, "IF")) {
        std::string problem;
        if (parseCondition(line_it, word_it, &out->condition, &problem)) {
            // If so, parse all subsequent lines into out->statements until end is reached
            line_it++;
            if (!parseStatementBlock(program, line_it, &out->statements)) {
                delete out;
                line_it = old_it;
                return nullptr;
            }
            return out;
        } else {
            set_error(line_num, problem);
        }
    }

    line_it = old_it;
    delete out;
    return nullptr;
}

Compiler::WhileStatement* Compiler::parseWhileStatement(Program& program, Program::iterator& line_it) {
    Program::iterator old_it = line_it;

    size_t line_num = std::distance(program.begin(), line_it);
    Line::iterator word_it = line_it->begin();
    WhileStatement* out = new WhileStatement();

    // Check if the first line matches the if statement format
    if (parseWord(line_it, word_it, "WHILE")) {
        std::string problem;
        if (parseCondition(line_it, word_it, &out->condition, &problem)) {
            // If so, parse all subsequent lines into out->statements until end is reached
            line_it++;
            if (!parseStatementBlock(program, line_it, &out->statements)) {
                delete out;
                line_it = old_it;
                return nullptr;
            }
            return out;
        } else {
            set_error(line_num, problem);
        }
    }

    line_it = old_it;
    delete out;
    return nullptr;
}

bool Compiler::parseStatementBlock(Program& program, Program::iterator& line_it, std::vector<Statement*>* out, std::string end) {
    Program::iterator old_it = line_it;
    size_t start_line = std::distance(program.begin(), line_it);

    while (line_it != program.end()) {
        if (line_it->size() > 0) {
            // On end, return successfully
            Line::iterator word_it = line_it->begin();
            if (parseWord(line_it, word_it, end)) {
                line_it++;
                return true;
            }

            // Otherwise, parse next statement
            out->push_back(parseStatement(program, line_it));

            // If the parse failed, print error message and return failure
            if (out->back() == nullptr) {
                if (error_message.empty()) {
                    size_t line_num = std::distance(program.begin(), line_it);
                    set_error(line_num, "Could not parse '" + *line_it->begin() + "' as an IF, WHILE, or object name.");
                }
                line_it = old_it;
                out->clear();
                return false;
            }
        } else {
            line_it++;
        }
    }

    if (end == "") {
        return true;
    }

    set_error(start_line - 1, "Code block starting here must be closed with '" + end + "'.");
    line_it = old_it;
    out->clear();
    return false;
}

// Attempts to parse a word as the name of an object.
// Advances the word iterator if successful.
bool Compiler::parseObject(Program::iterator& line_it, Line::iterator& word_it, Object** out) {
    if (word_it == line_it->end()) {
        return false;
    }

    auto obj = objects.find(*word_it);
    if (obj != objects.end()) {
        *out = obj->second;
        word_it++;
        return true;
    }
    return false;
}

// Attempts to parse a word as the name of a legal action for the given object.
// Advances the word iterator if successful.
bool Compiler::parseAction(Program::iterator& line_it, Line::iterator& word_it, Object* obj, ActionFunction* out_func, float* out_dur) {
    if (word_it == line_it->end()) {
        return false;
    }

    auto act = obj->actions.find(*word_it);
    if (act != obj->actions.end()) {
        *out_func = act->second.func;
        if (out_dur != nullptr) {
            *out_dur = act->second.duration;
        }
        word_it++;
        return true;
    }
    return false;
}

// Attempts to parse a sequence of words as a condition of the form:
// value1 comparator value2
// Advances the word iterator if successful.
bool Compiler::parseCondition(Program::iterator& line_it, Line::iterator& word_it, Condition* out, std::string* problem) {
    if (word_it == line_it->end()) {
        if (problem != nullptr) {
            *problem = "Missing condition.";
        }
        return false;
    }
    
    Line::iterator old_it = word_it;
    std::string prob = "";

    if (parseWord(line_it, word_it, "(")) {
        if (parseValue(line_it, word_it, &out->left)) {
            if (parseComparator(line_it, word_it, &out->comparator)) {
                if (parseValue(line_it, word_it, &out->right)) {
                    if (parseWord(line_it, word_it, ")")) {
                        return true;
                    } else {
                        prob = "Condition is missing a ')'";
                    }
                } else {
                    prob = "Failed to parse right value in condition.";
                }
            } else if (word_it != line_it->end()) {
                prob = "Failed to parse comparator '" + *word_it + "' in condition.";
            } else {
                prob = "Condition is missing a ')'";
            }
        } else {
            prob = "Failed to parse left value in condition.";
        }
    } else {
        prob = "Condition is missing a '('";
    }

    word_it = old_it;

    if (parseWord(line_it, word_it, "(")
     && parseValue(line_it, word_it, &out->left)
     && parseWord(line_it, word_it, ")")) {
        out->comparator = "!=";
        out->right = new int(0);
        return true;
    }

    word_it = old_it;

    if (problem != nullptr) {
        *problem = prob;
    }

    return false;
}

// Attempts to parse a word as a comparator.
// Currently, only =, <, and > are supported.
// Advances the word iterator if successful.
bool Compiler::parseComparator(Program::iterator& line_it, Line::iterator& word_it, std::string* out) {
    if (word_it == line_it->end()) {
        return false;
    }
    
    std::string comp = *word_it;
    if (parseWord(line_it, word_it, "==")
     || parseWord(line_it, word_it, "<")
     || parseWord(line_it, word_it, ">")
     || parseWord(line_it, word_it, "!=")
     || parseWord(line_it, word_it, "<=")
     || parseWord(line_it, word_it, ">=")) {
        *out = comp;
        return true;
    }
    return false;
}

// Attempts to parse a word or sequence of words as either
// an integer value or an object property.
// Advances the word iterator if successful.
bool Compiler::parseValue(Program::iterator& line_it, Line::iterator& word_it, int** out) {
    if (word_it == line_it->end()) {
        return false;
    }
    
    if (parsePropertyValue(line_it, word_it, out)
     || parseIntValue(line_it, word_it, out)
     || parseBooleanValue(line_it, word_it, out)) {
        return true;
    }

    return false;
}

// Attempts to parse a word as an integer value.
// Advances the word iterator if successful.
bool Compiler::parseIntValue(Program::iterator& line_it, Line::iterator& word_it, int** out) {
    if (word_it == line_it->end()) {
        return false;
    }
    
    int val;
    try {
        val = std::stoi(*word_it);
    } catch (...) {
        return false;
    }
    *out = new int();
    **out = val;
    word_it++;
    return true;
}

// Attempts to parse a word as true/false
// Advances the word iterator if successful.
bool Compiler::parseBooleanValue(Program::iterator& line_it, Line::iterator& word_it, int** out) {
    if (word_it == line_it->end()) {
        return false;
    }
    
    if (parseWord(line_it, word_it, "TRUE")) {
        *out = new int();
        **out = 1;
        return true;
    }

    if (parseWord(line_it, word_it, "FALSE")) {
        *out = new int();
        **out = 0;
        return true;
    }

    return false;
}

// Attempts to parse a sequence of words as an object property value,
// of the form: object.property
// Advances the word iterator if successful.
bool Compiler::parsePropertyValue(Program::iterator& line_it, Line::iterator& word_it, int** out) {
    if (word_it == line_it->end()) {
        return false;
    }
    
    Line::iterator old_it = word_it;
    Object* obj;
    
    if (parseObject(line_it, word_it, &obj)
     && parseWord(line_it, word_it, ".")
     && parseProperty(line_it, word_it, obj, out)) {
        return true;
    }

    word_it = old_it;
    return false;
}

// Attempts to parse a word as the name of a property of the given object.
// Advances the word iterator if successful.
bool Compiler::parseProperty(Program::iterator& line_it, Line::iterator& word_it, Object* obj, int** out) {
    if (word_it == line_it->end()) {
        return false;
    }
    
    auto prop = obj->properties.find(*word_it);
    if (prop != obj->properties.end()) {
        *out = prop->second;
        word_it++;
        return true;
    }
    return false;
}

// Attempts to parse a word that is equal to the given word.
// Advances the word iterator if successful.
bool Compiler::parseWord(Program::iterator& line_it, Line::iterator& word_it, std::string word) {
    if (word_it == line_it->end() || word == "") {
        return false;
    }
    
    if (*word_it == word) {
        word_it++;
        return true;
    }
    return false;
}

// Virtual destructor to prevent compiler warnings
Compiler::Statement::~Statement() {
}

// Action statement constructor
Compiler::ActionStatement::ActionStatement() {
    type = ACTION_STATEMENT;
}

// If statement constructor
Compiler::IfStatement::IfStatement() {
    type = IF_STATEMENT;
    base_duration = 0.25f;
    duration = 0.25f;
}

// While statement constructor
Compiler::WhileStatement::WhileStatement() {
    type = WHILE_STATEMENT;
    base_duration = 0.25f;
    duration = 0.25f;
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

// Reset a statement so it can be used again
void Compiler::Statement::reset() {
    duration = base_duration;
    current_line = 0;
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

// First call returns the if line itself.
// If true, subsequent calls return statements in order
Compiler::Statement* Compiler::IfStatement::next() {
    if (current_line == 0) {
        current_line++;
        return this;
    } else if (truth && current_line - 1 < statements.size()) {
        Statement* statement = nullptr;

        // Move past statement ends until you get a next statement
        while (current_line < 1 + statements.size()) {
            statement = statements[current_line - 1]->next();
            if (statement == nullptr) {
                current_line++;
            } else {
                break;
            }
        }

        return statement;
    }

    return nullptr;
}

// Execute just the conditional line and set the truth value
void Compiler::IfStatement::execute() {
    truth = condition.isTrue();
    if (truth) {
        reset();
        current_line = 1;
        truth = true;
    }
}

// Reset if statement and all lines inside of it
void Compiler::IfStatement::reset() {
    current_line = 0;
    duration = base_duration;
    truth = false;
    for (size_t i = 0; i < statements.size(); i++) {
        statements[i]->reset();
    }
}

// First call returns the if line itself.
// If true, subsequent calls return statements in order
Compiler::Statement* Compiler::WhileStatement::next() {
    if (current_line == 0) {
        current_line++;
        return this;
    } else if (truth && current_line < 1 + statements.size()) {
        Statement* statement = nullptr;

        // Move past statement ends until you get a next statement
        while (current_line < 1 + statements.size()) {
            statement = statements[current_line - 1]->next();
            if (statement == nullptr) {
                current_line++;
            } else {
                break;
            }
        }

        // If we reached the end of the loop, start over
        if (current_line == 1 + statements.size()) {
            current_line = 1;
            return this;
        }
        
        return statement;
    } else if (truth && current_line == 1 + statements.size()) {
        current_line = 1;
        return this;
    }

    return nullptr;
}

// Execute just the conditional line and set the truth value
void Compiler::WhileStatement::execute() {
    truth = condition.isTrue();
    if (truth) {
        reset();
        current_line = 1;
        truth = true;
    }
}

// Reset while statement and all lines inside of it
void Compiler::WhileStatement::reset() {
    current_line = 0;
    duration = base_duration;
    truth = false;
    for (size_t i = 0; i < statements.size(); i++) {
        statements[i]->reset();
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

// Evaluate a conditional
bool Compiler::Condition::isTrue() {
    if (comparator == "==") {
        return *left == *right;
    } else if (comparator == "<") {
        return *left < *right;
    } else if (comparator == ">") {
        return *left > *right;
    } else if (comparator == "!=") {
        return *left != *right;
    } else if (comparator == "<=") {
        return *left <= *right;
    } else if (comparator == ">=") {
        return *left >= *right;
    } else {
        std::cout << "Invalid comparator " << comparator << " in condition" << std::endl;
        return false;
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

// Sets the error message
void Compiler::set_error(size_t line_num, std::string message) {
    error_message = "ERROR (line " + std::to_string(line_num + 1) + "): " + message;
}