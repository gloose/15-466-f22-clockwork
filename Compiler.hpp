#include <unordered_map>
#include <vector>
#include <list>

struct Compiler {
    enum Action {
        ATTACK,
        DEFEND
    };

    struct Object;

    typedef void (*ActionFunction)(Object*, Object*);

    struct Object {
        std::string name = "";
        std::unordered_map<std::string, ActionFunction> actions;
        std::unordered_map<std::string, int*> properties;

        Object(std::string name);
        void addAction(std::string action_name, ActionFunction func);
        void addProperty(std::string property_name, int default_value);
        int& property(std::string property_name);
    };

    std::unordered_map<std::string, Object*> objects;

    typedef std::list<std::string> Line;
    typedef std::list<Line> Program;

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
    };

    enum StatementType {
        ACTION_STATEMENT,
        IF_STATEMENT
    };

    struct Statement {
        virtual void execute();
        StatementType type;
    };

    struct ActionStatement : Statement {
        Object* object;
        ActionFunction action;
        Object* target;

        ActionStatement();
        void execute();
    };

    struct IfStatement : Statement {
        Condition condition;
        std::vector<Statement*> statements;

        IfStatement();
        void execute();
    };

    struct Executable {
        std::vector<Statement*> statements;
        void execute();
    };

    Compiler();
    Statement* parseStatement(Program& program, Program::iterator& line_it);
    bool parseObject(Line::iterator& word_it, Object** out);
    bool parseAction(Line::iterator& word_it, Object* obj, ActionFunction* out);
    ActionStatement* parseActionStatement(Program& program, Program::iterator& line_it);
    IfStatement* parseIfStatement(Program& program, Program::iterator& line_it);
    bool parseWord(Line::iterator& word_it, std::string word);
    bool parseCondition(Line::iterator& word_it, Condition* out);
    bool parseValue(Line::iterator& word_it, int** out);
    bool parseIntValue(Line::iterator& word_it, int** out);
    bool parsePropertyValue(Line::iterator& word_it, int** out);
    bool parseProperty(Line::iterator& word_it, Object* obj, int** out);
    bool parseComparator(Line::iterator& word_it, std::string* out);
    Executable* compile(std::string filename);
    void addObject(Object* obj);
};