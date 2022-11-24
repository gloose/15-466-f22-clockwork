#pragma once

#include <unordered_map>
#include <vector>
#include <list>
#include <string>
#include "Object.hpp"

#ifndef _COMPILER_H_
#define _COMPILER_H_

struct Compiler {
    std::unordered_map<std::string, Object*> objects;

    std::vector<Object*> players;
    std::vector<Object*> enemies;

    typedef std::vector<std::string> Line;
    typedef std::vector<Line> Program;

    static const size_t MAX_LINE_SIZE = 1024;

    enum CompoundType {
        INVALID_COMPOUND,
        CONJUNCTION,
        DISJUNCTION
    };

    struct Condition {
        int* left;
        std::string comparator;
        int* right;

        bool isTrue();
    };

    enum StatementType {
        ACTION_STATEMENT,
        IF_STATEMENT,
        WHILE_STATEMENT,
        COMPOUND_STATEMENT
    };

    struct Statement {
        StatementType type;
        size_t current_line = 0;
        float base_duration = 1.f;
        float duration = 1.f;
        size_t line_num = 0;
        Compiler* compiler;

        Statement(Compiler* compiler);
        virtual ~Statement();
        virtual Statement* next();
        virtual void execute();
        virtual void reset();
    };

    struct ActionStatement : Statement {
        Object* object;
        ActionFunction func;
        Object* target;
        bool has_target;

        ActionStatement(Compiler* compiler);
        Statement* next();
        void execute();
        Object* getRealTarget();
    };

    struct CompoundStatement : Statement {
        Condition condition;
        CompoundType compound_type;

        CompoundStatement(Compiler* compiler);
    };

    struct IfStatement : Statement {
        Condition condition;
        std::vector<Statement*> statements;
        std::vector<CompoundStatement*> compounds;
        bool truth = false;

        IfStatement(Compiler* compiler);
        Statement* next();
        void execute();
        void reset();
    };

    struct WhileStatement : Statement {
        Condition condition;
        std::vector<Statement*> statements;
        std::vector<CompoundStatement*> compounds;
        bool truth = false;

        WhileStatement(Compiler* compiler);
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

    Object* random_player;
    Object* random_enemy;

    Compiler();
    Statement* parseStatement(Program& program, Program::iterator& line_it);
    ActionStatement* parseActionStatement(Program& program, Program::iterator& line_it);
    IfStatement* parseIfStatement(Program& program, Program::iterator& line_it);
    WhileStatement* parseWhileStatement(Program& program, Program::iterator& line_it);
    CompoundStatement* parseCompoundStatement(Program& program, Program::iterator& line_it);
    bool parseObject(Program::iterator& line_it, Line::iterator& word_it, Object** out);
    bool parseAction(Program::iterator& line_it, Line::iterator& word_it, Object* obj, ActionFunction* out_func, float* out_dur, bool* out_has_target);
    bool parseStatementBlock(Program& program, Program::iterator& line_it, std::vector<Statement*>* out, std::string end = "END");
    bool parseCompoundBlock(Program& program, Program::iterator& line_it, std::vector<CompoundStatement*>* out);
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
    static Line readLine(std::string text, std::vector<int>* offsets = nullptr);
    void set_error(size_t line_num, std::string message);
    void initSpecialObjects();
    void clearObjects();
    static std::vector<std::string> readFile(std::string filename);
};

#endif