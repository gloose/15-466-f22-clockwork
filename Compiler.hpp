#include <unordered_map>
#include <vector>
#include <list>
#include <string>
#include "Object.hpp"

#ifndef _COMPILER_H_
#define _COMPILER_H_

struct Compiler {
    std::unordered_map<std::string, Object*> objects;

    typedef std::list<std::string> Line;
    typedef std::list<Line> Program;

    static const size_t MAX_LINE_SIZE = 1024;

    enum ConditionType {
        CONJUNCTION,
        DISJUNCTION
    };

    struct Condition {
        ConditionType type;
        int* left;
        std::string comparator;
        int* right;
        std::vector<Condition> conditions;

        bool isTrue();
    };

    enum StatementType {
        ACTION_STATEMENT,
        IF_STATEMENT,
        WHILE_STATEMENT
    };

    struct Statement {
        StatementType type;
        size_t current_line = 0;
        float base_duration = 1.f;
        float duration = 1.f;
        size_t line_num = 0;

        virtual ~Statement();
        virtual Statement* next();
        virtual void execute();
        virtual void reset();
    };

    struct ActionStatement : Statement {
        Object* object;
        ActionFunction func;
        Object* target;

        ActionStatement();
        Statement* next();
        void execute();
    };

    struct IfStatement : Statement {
        Condition condition;
        std::vector<Statement*> statements;
        bool truth = false;

        IfStatement();
        Statement* next();
        void execute();
        void reset();
    };

    struct WhileStatement : Statement {
        Condition condition;
        std::vector<Statement*> statements;
        bool truth = false;

        WhileStatement();
        Statement* next();
        void execute();
        void reset();
    };

    struct Executable {
        std::vector<Statement*> statements;
        size_t current_line = 0;
        void execute();
        Statement* next();
    };

    std::string error_message = "";

    Compiler();
    Statement* parseStatement(Program& program, Program::iterator& line_it);
    ActionStatement* parseActionStatement(Program& program, Program::iterator& line_it);
    IfStatement* parseIfStatement(Program& program, Program::iterator& line_it);
    Compiler::WhileStatement* parseWhileStatement(Program& program, Program::iterator& line_it);
    bool parseObject(Program::iterator& line_it, Line::iterator& word_it, Object** out);
    bool parseAction(Program::iterator& line_it, Line::iterator& word_it, Object* obj, ActionFunction* out_func, float* out_dur);
    bool parseStatementBlock(Program& program, Program::iterator& line_it, std::vector<Statement*>* out, std::string end = "END");
    bool parseWord(Program::iterator& line_it, Line::iterator& word_it, std::string word);
    bool parseCondition(Program::iterator& line_it, Line::iterator& word_it, Condition* out, std::string* problem = nullptr);
    bool parseValue(Program::iterator& line_it, Line::iterator& word_it, int** out);
    bool parseIntValue(Program::iterator& line_it, Line::iterator& word_it, int** out);
    bool parseBooleanValue(Program::iterator& line_it, Line::iterator& word_it, int** out);
    bool parsePropertyValue(Program::iterator& line_it, Line::iterator& word_it, int** out);
    bool parseProperty(Program::iterator& line_it, Line::iterator& word_it, Object* obj, int** out);
    bool parseComparator(Program::iterator& line_it, Line::iterator& word_it, std::string* out);
    Executable* compile(Program program);
    Executable* compile(std::string filename);
    Executable* compile(std::vector<std::string> lines);
    void addObject(Object* obj);
    Program readProgram(std::string filename);
    Program readProgram(std::vector<std::string> lines);
    void set_error(size_t line_num, std::string message);
};

#endif