//
//  forth.h
//  rgb-forth
//
//  Created by Wayne Malkin on 2021-11-24.
//

#ifndef forth_h
#define forth_h

enum VALUETYPE { FREE, INT, FLOAT, STR, FUNC, SEQ, ARRAY, SYM };

class ValueStack;
class Sym;

//
// class Value:
// A variant class that can hold any of the following value types:
//
//    integer
//    double
//    string
//    sequence (a Forth sequence, used for loops, conditionals, etc.)
//    symbol
//    array of int (used to compute arrays of LED color values)
//    cons (car/cdr pointers) -- not used yet
// 
// There are no array, list, or dictionary types. These types are designed
// to efficiently manage LED appliances.
//

class Value {
public:
    VALUETYPE vtype;
    Value* next;
    union {
        int inum;
        double fnum;
        char* str;
        Sym* sym;
        struct {
            void(*func)();
            ValueStack* seq;
        };
        struct {
            int len;
            int* ia;
        };
        struct {
            Value* car;
            Value* cdr;
        };
//        Value* va;
    };
    
    Value();
    ~Value();

    int asint();
    double asfloat();
    char* asstring();
    ValueStack* asseq();
};


// valloc and vfree must be used instead of constructed Value objects directly.
// This allows for the internal management of a free list.
Value* valloc();
Value* valloc(int n);
Value* valloc(double n);
Value* valloc(const char* s);
Value* valloc(void(*f)());
Value* valloc(int* i, int ilen);
//Value* valloc(Value* v, int vlen);
Value* valloc(Sym* s);
Value* valloc(ValueStack* s);
Value* valloc(Value* obj);
void vfree(Value* v);


class ValueStack {
public:
    Value* head;
    Value* tail;
    ValueStack* mOuter;
    
    ValueStack();
    ValueStack(ValueStack* src);
    ~ValueStack();
    void deleteSequences();
    
    ValueStack* closeSequence();
    
    void push(Value* v);
    void push(int vv);
    void push(double vv);
    void push(const char* vv);
    void push(void(*f)());
    void push(Sym* s);
    void push(ValueStack* s);
    void push(int* ia, int ilen);
//    void push(Value* va, int vlen);

    void pushTail(Value* v);
    void pushTail(int vv);
    void pushTail(double vv);
    void pushTail(const char* vv);
    void pushTail(void(*f)());
    void pushTail(Sym* s);
    void pushTail(ValueStack* s);
    void pushTail(int* ia, int ilen);
//    void pushTail(Value* va, int vlen);

    Value* pop();
    int popint();
    double popfloat();
    char* popstring();
    ValueStack* popseq();

    Value* top();
    Value* back();
    int size();
    Value* at(int n);
    void clear();
    void reverse();
};


ValueStack* forth_stack();

void forth_stepfunction(void(*sf)(Value*));



class Sym {
public:
    const char* word;
    Value* value;
    Sym* next;

    Sym(const char* w, Value* v);
    ~Sym();
};


class FDict {
public:
    Sym* head;

    FDict();
    
    void def(const char* word, Value* value);
    void def(const char* word, void(*func)());
    void forget(const char* word);
    Sym* findsym(const char* word);
    Value* find(const char* word);
};


FDict* forth_dict();

char* strclone(const char* s);
char* strdelete(char* s);
int* iaclone(const int* ia, int len);
int* iadelete(int* ia);
//Value* vaclone(const Value* va, int len);
//Value* vadelete(Value* va, int len);

// process implicit array arguments
void unary(int(*oper)(int), double(*dbl_oper)(double));
void binary(int(*oper)(int,int), double(*dbl_oper)(double,double));
void ternary(int(*oper)(int,int,int), double(*dbl_oper)(double,double,double));


void forth_init();
void forth_run(const char* line);
void forth_run(Value* v);
void forth_unu(bool state);
bool forth_getecho();
void forth_setecho(bool echo);



#endif /* forth_h */
