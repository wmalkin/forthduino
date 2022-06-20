/*********
 main.cpp
 rgb-forth

 Created by Wayne Malkin on 2021-11-23.

rgb-forth is a dialect of the 'forth' language, originally created in the
1960's and 1970's as a very efficient stack-based language. It was created
by Charles Moore to help him write and manage control software for radio
telescopes, I think. It still has a following, especially in circles
where a high level language is desired and computing resources are
at a premium.

This dialect diverges significantly from the original design, mostly
in the trade-offs between convenience and efficiency. My goals with this
language are as follows:

1. It will easily run on the target hardware platform, which is a
   teensyduino 3.x or 4.x.
2. The lexer/parser/compiler is compact and easy to code, even if that
   creates a few compromises in the language syntax.
3. It conveniently includes richer types (int, float, string, etc.),
   and is able to support dynamic type conversion. Later inclusion is
   array of int, so whole LED string calculations can be comprehended.
4. It has multiple evaluation stacks to support the target use case,
   which is rendering color data for strings of RGB LEDs (WS2812).

I wanted a simple text based language so I could send strings of color
data, or complete animations, over a LAN to a target device with attached
LED strings. Thus, the teensy attached to the network and to the LED
strings with Octo would become a low maintenance applicance.

Frames of color data can thus be sent individually as RGB pixel values,
HSV data, etc., and shown on the string.

A frame can be sent as a rendering algorithm, for example a color gradient.

A set of frames can be sent as an animation algorithm.



*************************
A word about sequences...
*************************

A sequence is a string of Forth words enclosed in brackets like this:

    [ dup * swap dup * + sqrt ]

A sequence is not evaluated word-by-word but is pushed onto the evaluation
stack. Sequences can be the body of a defined word (function), or can be
used in flow-of-control words like if, repeat, and map.

Functions can be defined as a sequence bound to a symbol like this:

    [ dup * swap dup * + sqrt ] 'pyth def

or using an alternative syntax:

    :pyth dup * swap dup * + sqrt ;

In the second syntax, the ':' sigil is used to designate the beginning of
a function definition, and the semicolon terminates it. The words in
between are the body of the function and are enclosed in a sequence
exactly as if the sequence was explicitly declared as in the first syntax.
The second syntax is Forth conventional and more readable.

The words in a sequence can span multiple lines.

Sequences are often used in loops and as the body of functions that
are called repeatedly. Conventional memory management would demand either
that sequences are duplicated when pushed onto the evaluation stack, or
that a reference counting or garbage collection scheme be used. I didn't
want to introduce the complexity of a memory management scheme and the
cloning/deleting of sequences seemed like excessive malloc overhead, so
instead a radical approach of ignoring memory has been used.

When a sequence is executed immediately from the top level (either from
serial or UDP immediate commands), then the sequence is deleted after
it is run. Otherwise, any sequence that is bound to a variable (a function
definition) is assumed to be long-lived. Sequences can be pushed to the
execution stack, operated on, and then the reference is simply dropped.
Thus the bound sequence (function body) does not have to be cloned or
deleted. The original sequence is simply used over and over again.

If a bound sequence is dropped from the dictionary, the memory will not be
cleaned up and a leak will occur. In practice, this never occurs.

*********/



//#include <string>
#if __APPLE__
#else
#include <arduino.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "forth.h"

#include "FastLED.h"


// memstat tracking Data
struct MEMSTATS {
  int vmalloc;
  int vallocated;
  int vfreed;
  int vcurrentallocated;
  int vcurrentfreed;
  int amalloc;
  int afreed;
//  int vamalloc;
//  int vafreed;
};

MEMSTATS mem = { 0, 0, 0, 0, 0, 0, 0 };


// debugging "step" behaviour
bool step_on = false;


char* strclone(const char* s)
{
    char* cstr = new char[strlen(s) + 1];
    strcpy(cstr, s);
    return cstr;
}


char* strdelete(char* s)
{
    delete[] s;
    return NULL;
}


int* iaclone(const int* ia, int len)
{
  mem.amalloc++;
  int* cpy = new int[len];
  memcpy(cpy, ia, len*sizeof(int));
  return cpy;
}


int* iadelete(int* ia)
{
  mem.afreed++;
  delete[] ia;
  return NULL;
}


//Value* vaclone(const Value* va, int len)
//{
//  mem.vamalloc++;
//  int* cpy = new Value*[len];
//  memcpy(cpy, ia, len*sizeof(int));
//  return cpy;
//}
//
//
//Value* vadelete(Value* va, int len)
//{
//  mem.afreed++;
//  delete[] ia;
//  return NULL;
//}


Value* vfreelist = NULL;

Value* valloc()
{
    if (vfreelist) {
        Value* rs = vfreelist;
        vfreelist = rs->next;
        // minimally zero out ptrs and unions
        memset(rs, 0, sizeof(Value));
        mem.vallocated++;
        mem.vcurrentallocated++;
        mem.vcurrentfreed--;
        return rs;
    }
    mem.vmalloc++;
    mem.vallocated++;
    mem.vcurrentallocated++;
    return new Value();
}


void vfree(Value* v)
{
  mem.vfreed++;
  mem.vcurrentallocated--;
  mem.vcurrentfreed++;
  
  switch(v->vtype) {
    case SEQ:
      // delete v->seq;
      break;
    case STR:
      strdelete(v->str);
      break;
    case ARRAY:
      iadelete(v->ia);
      break;
    default:
      break;
  }

  v->next = vfreelist;
  vfreelist = v;
  v->vtype = FREE;
}


Value::Value()
{
    next = NULL;
    vtype = FREE;
}

Value::~Value()
{
    // alert! not supposed to be calling the destructor
    // vfree(this);
}


int Value::asint()
{
    int rs;
    switch(this->vtype) {
        case INT:
            rs = this->inum;
            break;
        case FLOAT:
            rs = (int)this->fnum;
            break;
        case STR:
            rs = atoi(this->str);
            break;
        case SYM:
            rs = this->sym->value->asint();
            break;
        default:
            rs = 0;
            break;
    }
    return rs;
}


double Value::asfloat()
{
    double rs;
    switch(this->vtype) {
        case INT:
            rs = this->inum;
            break;
        case FLOAT:
            rs = this->fnum;
            break;
        case STR:
            rs = atof(this->str);
            break;
        case SYM:
            rs = this->sym->value->asfloat();
            break;
        default:
            rs = 0.0;
            break;
    }
    return rs;
}


char* Value::asstring()
{
    char temp[100];
    char* rs = NULL;
    switch(this->vtype) {
        case INT:
            sprintf(temp, "%d", this->inum);
            rs = strclone(temp);
            break;
        case FLOAT:
            sprintf(temp, "%f", this->fnum);
            rs = strclone(temp);
            break;
        case STR:
            rs = strclone(this->str);
            break;
        case SYM:
            rs = this->sym->value->asstring();
            break;
        default:
            rs = strclone("");
            break;
    }
    return rs;
}


ValueStack* Value::asseq()
{
    ValueStack* rs;
    switch(this->vtype) {
        case SEQ:
            rs = this->seq;
            break;
        case SYM:
            rs = this->sym->value->asseq();
            break;
        default:
            rs = NULL;
            break;
    }
    return rs;
}




Value* valloc(int n)
{
    Value* rs = valloc();
    rs->vtype = INT;
    rs->inum = n;
    return rs;
}

Value* valloc(double n)
{
    Value* rs = valloc();
    rs->vtype = FLOAT;
    rs->fnum = n;
    return rs;
}

Value* valloc(const char* s)
{
    Value* rs = valloc();
    rs->vtype = STR;
    rs->str = strclone(s);
    return rs;
}

Value* valloc(void(*f)())
{
    Value* rs = valloc();
    rs->vtype = FUNC;
    rs->func = f;
    return rs;
}

Value* valloc(int* i, int ilen)
{
    Value* rs = valloc();
    rs->vtype = ARRAY;
    rs->ia = i;
    rs->len = ilen;
    return rs;
}

Value* valloc(Sym* s)
{
    Value* rs = valloc();
    rs->vtype = SYM;
    rs->sym = s;
    return rs;
}


Value* valloc(ValueStack* s)
{
    Value* rs = valloc();
    // takes a reference to the ValueStack, and does not delete it later
    rs->vtype = SEQ;
    rs->seq = s;
    return rs;
}


Value* valloc(Value* obj)
{
    Value* rs = valloc();
    rs->vtype = obj->vtype;
    switch(obj->vtype) {
        case INT:
            rs->inum = obj->inum;
            break;
        case FLOAT:
            rs->fnum = obj->fnum;
            break;
        case STR:
            rs->str = strclone(obj->str);
            break;
        case FUNC:
            rs->func = obj->func;
            rs->seq = obj->seq;
            break;
        case SEQ:
//            rs->seq = new ValueStack(obj->seq);
            rs->seq = obj->seq;
            break;
        case SYM:
            rs->sym = obj->sym;
            break;
        case ARRAY:
            rs->ia = iaclone(obj->ia, obj->len);
            rs->len = obj->len;
            break;
        case FREE:
        default:
            break;
    }
    return rs;
}


ValueStack::ValueStack()
{
    head = NULL;
    tail = NULL;
    mOuter = NULL;
}


ValueStack::ValueStack(ValueStack* src)
{
    head = NULL;
    tail = NULL;
    mOuter = NULL;
    Value* itm = src->head;
    while (itm) {
        Value* newv = valloc(itm);
        if (newv->vtype == SEQ)
            newv->seq = new ValueStack(newv->seq);
        pushTail(newv);
        itm = itm->next;
    }
}


ValueStack::~ValueStack()
{
    Value* it = head;
    while (it) {
        Value* nxt = it->next;
        vfree(it);
        it = nxt;
    }
}


void ValueStack::deleteSequences()
{
    Value* it = head;
    while(it) {
        if (it->vtype == SEQ) {
            it->seq->deleteSequences();
            delete it->seq;
            it->seq = NULL;
            it->vtype = FREE;
        }
        it = it->next;
    }
}


ValueStack* ValueStack::closeSequence()
{
    if (mOuter) {
        ValueStack* outer = mOuter;
        mOuter = NULL;
        outer->pushTail(this);
        return outer;
    }
    return this;
}


void ValueStack::push(Value* v)
{
    v->next = head;
    head = v;
    if (!tail)
        tail = v;
}


void ValueStack::pushTail(Value* v)
{
    v->next = NULL;
    if (tail)
        tail->next = v;
    tail = v;
    if (!head)
        head = v;
}


void ValueStack::push(int vv)
{
    Value* v = valloc(vv);
    push(v);
}


void ValueStack::push(double vv)
{
    Value* v = valloc(vv);
    push(v);
}


void ValueStack::push(const char* vv)
{
    Value* v = valloc(vv);
    push(v);
}


void ValueStack::push(void(*f)())
{
    Value* v = valloc(f);
    push(v);
}


void ValueStack::push(Sym* s)
{
    Value* v = valloc(s);
    push(v);
}


void ValueStack::push(ValueStack* s)
{
    Value* v = valloc(s);
    push(v);
}


void ValueStack::push(int* ia, int ilen)
{
    Value* v = valloc(ia, ilen);
    push(v);
}


void ValueStack::pushTail(int vv)
{
    Value* v = valloc(vv);
    pushTail(v);
}


void ValueStack::pushTail(double vv)
{
    Value* v = valloc(vv);
    pushTail(v);
}


void ValueStack::pushTail(const char* vv)
{
    Value* v = valloc(vv);
    pushTail(v);
}


void ValueStack::pushTail(void(*f)())
{
    Value* v = valloc(f);
    pushTail(v);
}


void ValueStack::pushTail(Sym* s)
{
    Value* v = valloc(s);
    pushTail(v);
}


void ValueStack::pushTail(ValueStack* s)
{
    Value* v = valloc(s);
    pushTail(v);
}


void ValueStack::pushTail(int* ia, int ilen)
{
    Value* v = valloc(ia, ilen);
    pushTail(v);
}


Value* ValueStack::pop()
{
    Value* rs = head;
    if (head)
        head = rs->next;
    if (!head)
        tail = NULL;
    return rs;
}


Value* ValueStack::top()
{
    return head;
}


Value* ValueStack::back()
{
    return tail;
}


int ValueStack::size()
{
    int sz = 0;
    Value* it = head;
    while (it) {
        sz++;
        it = it->next;
    }
    return sz;
}


Value* ValueStack::at(int n)
{
    Value* it = head;
    while (n > 0 && it) {
        n--;
        it = it->next;
    }
    return it;
}


void ValueStack::clear()
{
    Value* it = head;
    while (it) {
        Value* nxt = it->next;
        vfree(it);
        it = nxt;
    }
    head = NULL;
    tail = NULL;
}


void ValueStack::reverse()
{
    Value* it = head;
    Value* nhead = NULL;
    Value* ntail = it;
    while (it) {
        Value* nxt = it->next;
        it->next = nhead;
        nhead = it;
        it = nxt;
    }
    head = nhead;
    tail = ntail;
}


int ValueStack::popint()
{
    Value* rv = pop();
    if (!rv)
        return 0;
    int rs = rv->asint();
    vfree(rv);
    return rs;
}


double ValueStack::popfloat()
{
    Value* rv = pop();
    if (!rv)
        return 0.0;
    double rs = rv->asfloat();
    vfree(rv);
    return rs;
}


char* ValueStack::popstring()
{
    Value* rv = pop();
    if (!rv)
        return strclone("");
    char* rs = rv->asstring();
    vfree(rv);
    return rs;
}


ValueStack* ValueStack::popseq()
{
    Value* rv = pop();
    if (!rv)
        return NULL;
    ValueStack* rs = rv->asseq();
    vfree(rv);
    return rs;
}



//
// Main computation stack is 'vstk'
//
ValueStack* vstk;
ValueStack* vstash;


ValueStack* forth_stack()
{
    return vstk;
}


//
// Sym and FDict implement the symbol or word dictionary. There is only
// one global dictionary; this is not a reusable key-value data structure.
//
Sym::Sym(const char* w, Value* v)
{
    next = NULL;
    word = strclone(w);
    value = v;
}


Sym::~Sym()
{
    strdelete((char*)word);
    if (value->vtype == SEQ) {
        value->seq->deleteSequences();
        delete value->seq;
    }
    vfree(value);
}


FDict::FDict()
{
    head = NULL;
}


void FDict::def(const char* word, Value* value)
{
    if (value->vtype == SEQ)
        value->seq = new ValueStack(value->seq);
    Sym* sym = new Sym(word, value);
    sym->next = head;
    head = sym;
}


void FDict::def(const char* word, void(*func)())
{
    Value* v = valloc(func);
    def(word, v);
}


void FDict::forget(const char* word)
{
    Sym* last = NULL;
    Sym* curr = head;
    while (curr && strcmp(word, curr->word) != 0) {
        last = curr;
        curr = curr->next;
    }
    if (curr) {
        if (last)
            last->next = curr->next;
        else
            head = curr->next;
        delete curr;
    }
}


Sym* FDict::findsym(const char* word)
{
    Sym* it = head;
    while (it) {
        if (strcmp(word, it->word) == 0)
            return it;
        it = it->next;
    }
    return NULL;
}


Value* FDict::find(const char* word)
{
    Sym* sym = findsym(word);
    if (sym)
        return sym->value;
    return NULL;
}



//
// Main forth dictionary is 'dict'
//
FDict* dict;


FDict* forth_dict()
{
    return dict;
}


//
// forward declaration of runSequence, which is called recursively by
// conditional and loop statements.
//
void runSequence(ValueStack* seq);



//
// This is the current word being executed, in case there are parameters inside.
//
Value* gfuncparams;


//
// Unary and Binary iterator that handles arrays correctly
//
void unary(int(*oper)(int), double(*dbl_oper)(double))
{
  Value *a = vstk->pop();
  if (a->vtype == ARRAY) {
    int len = a->len;
    int* rs = new int[len];
    mem.amalloc++;
    for (int i = 0; i < len; i++) {
      rs[i] = (*oper)(a->ia[i]);
    }
    vstk->push(rs, len);
  } else if (a->vtype == INT) {
    vstk->push((*oper)(a->asint()));
  } else {
    vstk->push((*dbl_oper)(a->asfloat()));
  }
  vfree(a);
}


void binary(int(*oper)(int,int), double(*dbl_oper)(double,double))
{
  Value *b = vstk->pop();
  Value *a = vstk->pop();
  
  // test if we should use int oper or double dbl_oper
  // always use double version unless both input operands are INT or ARRAY.
  bool usefloat = (dbl_oper != NULL) && !((a->vtype == ARRAY || a->vtype == INT) && (b->vtype == ARRAY || b->vtype == INT));

  // test if we need to iterate and return an array (at least one input operand is an ARRAY)
  if (a->vtype == ARRAY || b->vtype == ARRAY) {
    // iterate to length of longest input operand
    int la = a->vtype == ARRAY ? a->len : 1;
    int lb = b->vtype == ARRAY ? b->len : 1;
    int len = la > lb ? la : lb;

    // create result array
    int* rs = new int[len];
    mem.amalloc++;

    // iterate over inputs
    for (int i = 0; i < len; i++) {
      if (usefloat) {
        double va = a->vtype == ARRAY ? (i < a->len ? (double)a->ia[i] : 0.0) : a->asfloat();
        double vb = b->vtype == ARRAY ? (i < b->len ? (double)b->ia[i] : 0.0) : b->asfloat();
        rs[i] = (int)((*dbl_oper)(va, vb));
      } else {
        int va = a->vtype == ARRAY ? (i < a->len ? a->ia[i] : 0) : a->asint();
        int vb = b->vtype == ARRAY ? (i < b->len ? b->ia[i] : 0) : b->asint();
        rs[i] = (*oper)(va, vb);
      }
    }
    vstk->push(rs, len);
  } else {
    // no arrays, just call the operator once, either double or int version
    if (usefloat)
      vstk->push((*dbl_oper)(a->asfloat(), b->asfloat()));
    else
      vstk->push((*oper)(a->asint(), b->asint()));
  }
  vfree(a);
  vfree(b);
}


void trinary(int(*oper)(int,int,int), double(*dbl_oper)(double,double,double))
{
  Value *c = vstk->pop();
  Value *b = vstk->pop();
  Value *a = vstk->pop();
  
  // test if we should use int oper or double dbl_oper
  // always use double version unless both input operands are INT or ARRAY.
  bool usefloat = (dbl_oper != NULL) && !((a->vtype == ARRAY || a->vtype == INT) && (b->vtype == ARRAY || b->vtype == INT) && (c->vtype == ARRAY || c->vtype == INT));

  // test if we need to iterate and return an array (at least one input operand is an ARRAY)
  if (a->vtype == ARRAY || b->vtype == ARRAY || c->vtype == ARRAY) {
    // iterate to length of longest input operand
    int la = a->vtype == ARRAY ? a->len : 1;
    int lb = b->vtype == ARRAY ? b->len : 1;
    int lc = c->vtype == ARRAY ? c->len : 1;
    int len = la > lb ? la : lb;
    len = lc > len ? lc : len;

    // create result array
    int* rs = new int[len];
    mem.amalloc++;

    // iterate over inputs
    for (int i = 0; i < len; i++) {
      if (usefloat) {
        double va = a->vtype == ARRAY ? (i < a->len ? (double)a->ia[i] : 0.0) : a->asfloat();
        double vb = b->vtype == ARRAY ? (i < b->len ? (double)b->ia[i] : 0.0) : b->asfloat();
        double vc = c->vtype == ARRAY ? (i < c->len ? (double)c->ia[i] : 0.0) : c->asfloat();
        rs[i] = (int)((*dbl_oper)(va, vb, vc));
      } else {
        int va = a->vtype == ARRAY ? (i < a->len ? a->ia[i] : 0) : a->asint();
        int vb = b->vtype == ARRAY ? (i < b->len ? b->ia[i] : 0) : b->asint();
        int vc = c->vtype == ARRAY ? (i < c->len ? c->ia[i] : 0) : c->asint();
        rs[i] = (*oper)(va, vb, vc);
      }
    }
    vstk->push(rs, len);
  } else {
    // no arrays, just call the operator once, either double or int version
    if (usefloat)
      vstk->push((*dbl_oper)(a->asfloat(), b->asfloat(), c->asfloat()));
    else
      vstk->push((*oper)(a->asint(), b->asint(), c->asint()));
  }
  vfree(a);
  vfree(b);
  vfree(c);
}


//
// Built-in words
//
void op_clst()
{
    vstk->clear();
}


int oper_add(int a, int b)
{
  return a + b;
}

double oper_dbl_add(double a, double b)
{
  return a + b;
}

void op_add()
{
    binary(&oper_add, &oper_dbl_add);
}


void op_sum()
{
    Value* v = vstk->pop();
    if (v && v->vtype == ARRAY) {
        int ttl = 0;
        for (int i = 0; i < v->len; i++)
            ttl += v->ia[i];
        vstk->push(ttl);
    } else {
      vstk->push(0);
    }
    vfree(v);
}


int oper_sub(int a, int b)
{
  return a - b;
}

double oper_dbl_sub(double a, double b)
{
  return a - b;
}

void op_sub()
{
    binary(&oper_sub, &oper_dbl_sub);
}


int oper_mul(int a, int b)
{
  return a * b;
}

double oper_dbl_mul(double a, double b)
{
  return a * b;
}

void op_mul()
{
    binary(&oper_mul, &oper_dbl_mul);
}


int oper_div(int a, int b)
{
  if (b == 0)
    return 0;
  return a / b;
}

double oper_dbl_div(double a, double b)
{
  if (b == 0.0)
    return 0.0;
  return a / b;
}

void op_div()
{
    binary(&oper_div, &oper_dbl_div);
}


int oper_mod(int a, int b)
{
  if (b == 0)
    return 0;
  return a % b;
}

double oper_dbl_mod(double a, double b)
{
  
  if (b == 0.0)
    return 0.0;
  return a - trunc(a / b) * b;
}

void op_mod()
{
    binary(&oper_mod, &oper_dbl_mod);
}


double oper_dbl_sqrt(double a)
{
  return sqrt(a);
}

int oper_sqrt(int a)
{
  return (int)(sqrt(a));
}

void op_sqrt()
{
  unary(&oper_sqrt, &oper_dbl_sqrt);
}


double oper_dbl_sq(double a)
{
  return sq(a);
}

int oper_sq(int a)
{
  return sq(a);
}

void op_sq()
{
  unary(&oper_sq, &oper_dbl_sq);
}


double oper_dbl_constrain(double a, double b, double c)
{
  return constrain(a, b, c);
}

int oper_constrain(int a, int b, int c)
{
  return constrain(a, b, c);
}

void op_constrain()
{
  trinary(&oper_constrain, &oper_dbl_constrain);
}


double oper_dbl_abs(double a)
{
  return abs(a);
}

int oper_abs(int a)
{
  return abs(a);
}

void op_abs()
{
  unary(&oper_abs, &oper_dbl_abs);
}


double oper_dbl_deg(double a)
{
#if defined(__APPLE__)
    return a * 180.0 / M_PI;
#else
    return a * 180.0 / PI;
#endif
}

int oper_deg(int a)
{
  return (int)(oper_dbl_deg(a));
}

void op_deg()
{
  unary(&oper_deg, &oper_dbl_deg);
}


double oper_dbl_rad(double a)
{
#if defined(__APPLE__)
    return a * M_PI / 180.0;
#else
    return a * PI / 180.0;
#endif
}

int oper_rad(int a)
{
  return (int)(oper_dbl_rad(a));
}

void op_rad()
{
  unary(&oper_rad, &oper_dbl_rad);
}


int oper_min(int a, int b)
{
  return a < b ? a : b;
}

double oper_dbl_min(double a, double b)
{
  return a < b ? a : b;
}

void op_min()
{
    binary(&oper_min, &oper_dbl_min);
}


int oper_max(int a, int b)
{
  return a > b ? a : b;
}

double oper_dbl_max(double a, double b)
{
  return a > b ? a : b;
}

void op_max()
{
    binary(&oper_max, &oper_dbl_max);
}


double oper_dbl_sin(double a)
{
  return sin(a);
}

int oper_sin(int a)
{
  return (int)(oper_dbl_sin(a));
}

void op_sin()
{
  unary(&oper_sin, &oper_dbl_sin);
}


double oper_dbl_cos(double a)
{
  return cos(a);
}

int oper_cos(int a)
{
  return (int)(oper_dbl_cos(a));
}

void op_cos()
{
  unary(&oper_cos, &oper_dbl_cos);
}


double oper_dbl_tan(double a)
{
  return tan(a);
}

int oper_tan(int a)
{
  return (int)(oper_dbl_tan(a));
}

void op_tan()
{
  unary(&oper_tan, &oper_dbl_tan);
}


double oper_dbl_pow(double a, double b)
{
  return pow(a, b);
}

int oper_pow(int a, int b)
{
  return (int)(oper_dbl_pow(a, b));
}

void op_pow()
{
  binary(&oper_pow, &oper_dbl_pow);
}


double oper_dbl_round(double a)
{
  return round(a);
}

int oper_round(int a)
{
  return a;
}

void op_round()
{
  unary(&oper_round, &oper_dbl_round);
}

double oper_dbl_ceil(double a)
{
  return ceil(a);
}

int oper_ceil(int a)
{
  return a;
}

void op_ceil()
{
  unary(&oper_ceil, &oper_dbl_ceil);
}

double oper_dbl_floor(double a)
{
  return floor(a);
}

int oper_floor(int a)
{
  return a;
}

void op_floor()
{
  unary(&oper_floor, &oper_dbl_floor);
}



//
// String operations
//

void op_num_dec()
{
    int dps = vstk->popint();
    int width = vstk->popint();
    double num = vstk->popfloat();
    char fmt[20], out[50];
    sprintf(fmt, "%%%d.%df", width, dps);
    sprintf(out, fmt, num);
    vstk->push(out);
}


void op_num_sci()
{
    int dps = vstk->popint();
    int width = vstk->popint();
    double num = vstk->popfloat();
    char fmt[20], out[50];
    sprintf(fmt, "%%%d.%dE", width, dps);
    sprintf(out, fmt, num);
    vstk->push(out);
}


void op_str_mid()
{
    int len = vstk->popint();
    int start = vstk->popint();
    char *inp = vstk->popstring();
    if (start < (int)strlen(inp)) {
        if ((start+len) < (int)strlen(inp))
            inp[start+len] = 0;
        vstk->push(inp+start);
    }
    strdelete(inp);
}


//
// Stack manipulations
//

void op_dup()
{
    vstk->push(valloc(vstk->top()));
}


void op_over()
{
    vstk->push(valloc(vstk->at(1)));
}


void op_stack_size()
{
    vstk->push(vstk->size());
}


void op_aty()
{
    vstk->push(valloc(vstk->at(1)));
}


void op_atz()
{
    vstk->push(valloc(vstk->at(2)));
}


void op_atu()
{
    vstk->push(valloc(vstk->at(3)));
}


void op_atv()
{
    vstk->push(valloc(vstk->at(4)));
}


void op_atw()
{
    vstk->push(valloc(vstk->at(5)));
}


void op_at()
{
    vstk->push(valloc(vstk->at(vstk->popint())));
}


void op_swap()
{
    Value* temp = vstk->head;
    vstk->head = temp->next;
    temp->next = vstk->head->next;
    vstk->head->next = temp;
}


void op_rot()
{
    Value* v1 = vstk->pop();
    Value* v2 = vstk->pop();
    Value* v3 = vstk->pop();
    vstk->push(v1);
    vstk->push(v3);
    vstk->push(v2);
}


void op_rup()
{
    Value* v1 = vstk->pop();
    Value* v2 = vstk->pop();
    Value* v3 = vstk->pop();
    vstk->push(v2);
    vstk->push(v1);
    vstk->push(v3);
}


void op_rot4()
{
    Value* v1 = vstk->pop();
    Value* v2 = vstk->pop();
    Value* v3 = vstk->pop();
    Value* v4 = vstk->pop();
    vstk->push(v1);
    vstk->push(v4);
    vstk->push(v3);
    vstk->push(v2);
}


void op_rup4()
{
    Value* v1 = vstk->pop();
    Value* v2 = vstk->pop();
    Value* v3 = vstk->pop();
    Value* v4 = vstk->pop();
    vstk->push(v3);
    vstk->push(v2);
    vstk->push(v1);
    vstk->push(v4);
}


void op_rotn()
{
    int n = vstk->popint();
    Value* h = vstk->head;
    vstk->head = h->next;
    Value* after = vstk->head;
    while (n > 2) {
        after = after->next;
        n--;
    }
    h->next = after->next;
    after->next = h;
}


void op_rupn()
{
    int n = vstk->popint();
    Value* after = vstk->head;
    while (n > 2) {
        after = after->next;
        n--;
    }
    Value* h = after->next;
    after->next = h->next;
    h->next = vstk->head;
    vstk->head = h;
}


void op_drop()
{
    vfree(vstk->pop());
}


void op_dup2()
{
    op_over();
    op_over();
}


void op_drop2()
{
    op_drop();
    op_drop();
}


void op_stash()
{
    Value* v = vstk->pop();
    vstash->push(v);
}


void op_unstash()
{
    Value* v = vstash->pop();
    vstk->push(v);
}


void op_swapstash()
{
    ValueStack* temp = vstk;
    vstk = vstash;
    vstash = temp;
}


void op_array()
{
    int sz = vstk->popint();
    int* ia = new int[sz];
    mem.amalloc++;
    memset(ia, 0, sz*sizeof(int));
    vstk->push(ia, sz);
}


void op_identity()
{
    Value* v = vstk->top();
    if (v->vtype == ARRAY)
        for (int i = 0; i < v->len; i++)
            v->ia[i] = i;
}


void op_index()
{
    Value* idxarray = vstk->pop();
    Value* indices = vstk->pop();
    if (indices->vtype == ARRAY) {
        //for (int i = 0; i < indices->len; i++)
            
    }
    vstk->push(indices);
    vfree(idxarray);
}


void op_map()
{
    ValueStack* block = vstk->popseq();
    Value* va = vstk->pop();
    if (va->vtype == ARRAY) {
        for (int i = 0; i < va->len; i++) {
            vstk->push(va->ia[i]);
            runSequence(block);
            va->ia[i] = vstk->popint();
        }
        vstk->push(va);
    }
}


void op_geta()
{
    int idx = vstk->popint();
    Value* v = vstk->top();
    if (v->vtype == ARRAY) {
        if (idx >= 0 && idx < v->len)
            vstk->push(v->ia[idx]);
        else
            vstk->push(0);
    } else {
        vstk->push(0);
    }
}


void op_puta()
{
    int ival = vstk->popint();
    int idx = vstk->popint();
    Value* v = vstk->top();
    if (v->vtype == ARRAY) {
        if (idx >= 0 && idx < v->len)
            v->ia[idx] = ival;
    }
}


void op_dgeta()
{
    int idx = vstk->popint();
    char* w = vstk->popstring();
    Value* v = dict->find(w);
    if (v->vtype == ARRAY) {
        if (idx >= 0 && idx < v->len)
            vstk->push(v->ia[idx]);
        else
            vstk->push(0);
    } else {
        vstk->push(0);
    }
    strdelete(w);
}


void op_dputa()
{
    int ival = vstk->popint();
    int idx = vstk->popint();
    char* w = vstk->popstring();
    Value* v = dict->find(w);
    if (v->vtype == ARRAY) {
        if (idx >= 0 && idx < v->len)
            v->ia[idx] = ival;
    }
    strdelete(w);
}


void op_size()
{
    Value* v = vstk->top();
    if (v->vtype == ARRAY)
        vstk->push(v->len);
    else
        vstk->push(0);
}


int comp(int a, int b)
{
    int c = a > b ? 1 : a < b ? -1 : 0;
    return c;
}


int oper_eq(int a, int b)
{
  return comp(a,b) == 0;
}

void op_eq()
{
  binary(&oper_eq, NULL);
}


int oper_ne(int a, int b)
{
  return comp(a,b) != 0;
}

void op_ne()
{
  binary(&oper_ne, NULL);
}


int oper_gt(int a, int b)
{
  return comp(a,b) == 1;
}

void op_gt()
{
  binary(&oper_gt, NULL);
}


int oper_lt(int a, int b)
{
  return comp(a,b) == -1;
}

void op_lt()
{
  binary(&oper_lt, NULL);
}


int oper_ge(int a, int b)
{
  return comp(a,b) != -1;
}

void op_ge()
{
  binary(&oper_ge, NULL);
}


int oper_le(int a, int b)
{
  return comp(a,b) != 1;
}

void op_le()
{
  binary(&oper_le, NULL);
}


int oper_and(int a, int b)
{
  return b != 0 ? a : 0;
}

void op_and()
{
    binary(&oper_and, NULL);
}


int oper_or(int a, int b)
{
  return a != 0 ? a : b;
}

void op_or()
{
    binary(&oper_or, NULL);
}


int oper_not(int a)
{
  return a != 0 ? 0 : 1;
}

void op_not()
{
  unary(&oper_not, NULL);
}


void op_if()
{
    int test = vstk->popint();
    ValueStack* ifblock = vstk->popseq();
    if (test != 0)
        runSequence(ifblock);
}


void op_ife()
{
    int test = vstk->popint();
    ValueStack* elseblock = vstk->popseq();
    ValueStack* ifblock = vstk->popseq();
    if (test != 0)
        runSequence(ifblock);
    else
        runSequence(elseblock);
}


void op_loop()
{
    int end = vstk->popint();
    int begin = vstk->popint();
    ValueStack* block = vstk->popseq();
    if (begin < end) {
        for (int i = begin; i < end; i++) {
            vstk->push(i);
            runSequence(block);
        }
    } else {
        for (int i = begin; i > end; i--) {
            vstk->push(i);
            runSequence(block);
        }
    }
}


void op_repeat()
{
    int times = vstk->popint();
    ValueStack* block = vstk->popseq();
    for (int i = 0; i < times; i++) {
        runSequence(block);
    }
}


void op_call()
{
    if (gfuncparams->seq) {
        runSequence(gfuncparams->seq);
    } else {
        char* word = vstk->popstring();
        Value* dv = dict->find(word);
        if (dv && dv->vtype == SEQ)
            runSequence(dv->seq);
        strdelete(word);
    }
}


int max (int a, int b)
{
  return a < b ? b : a;
}


int min (int a, int b)
{
  return a > b ? b : a;
}


struct LRGB {
    int r;
    int g;
    int b;
};

struct LHSV {
    int h;
    int s;
    int v;
};


int RGBFORMAT = 0;


void op_rgbformat() {
  RGBFORMAT = vstk->popint();
}


int rgbpack(LRGB* rgb)
{
  switch(RGBFORMAT) {
    case 0: //rgb
    default:
      return (rgb->r & 0xff) << 16 | (rgb->g & 0xff) << 8 | (rgb->b & 0xff);
    case 1: //grb
      return (rgb->g & 0xff) << 16 | (rgb->r & 0xff) << 8 | (rgb->b & 0xff);
    case 2: //bgr
      return (rgb->b & 0xff) << 16 | (rgb->g & 0xff) << 8 | (rgb->r & 0xff);
    case 3: //gbr
      return (rgb->g & 0xff) << 16 | (rgb->b & 0xff) << 8 | (rgb->r & 0xff);
    case 4: //rbg
      return (rgb->r & 0xff) << 16 | (rgb->b & 0xff) << 8 | (rgb->g & 0xff);
    case 5: //brg
      return (rgb->b & 0xff) << 16 | (rgb->r & 0xff) << 8 | (rgb->g & 0xff);
  }
}


void rgbunpack(int c, LRGB* rgb)
{
  switch(RGBFORMAT) {
    case 0:
      rgb->r = (c & 0xff0000) >> 16;
      rgb->g = (c & 0x00ff00) >> 8;
      rgb->b = c & 0x0000ff;
      break;
    case 1:
      rgb->g = (c & 0xff0000) >> 16;
      rgb->r = (c & 0x00ff00) >> 8;
      rgb->b = c & 0x0000ff;
      break;
    case 2:
      rgb->b = (c & 0xff0000) >> 16;
      rgb->g = (c & 0x00ff00) >> 8;
      rgb->r = c & 0x0000ff;
      break;
    case 3:
      rgb->g = (c & 0xff0000) >> 16;
      rgb->b = (c & 0x00ff00) >> 8;
      rgb->r = c & 0x0000ff;
      break;
    case 4:
      rgb->r = (c & 0xff0000) >> 16;
      rgb->b = (c & 0x00ff00) >> 8;
      rgb->g = c & 0x0000ff;
      break;
    case 5:
      rgb->b = (c & 0xff0000) >> 16;
      rgb->r = (c & 0x00ff00) >> 8;
      rgb->g = c & 0x0000ff;
      break;
  }
}


unsigned int h2rgb(unsigned int v1, unsigned int v2, unsigned int hue)
{
    if (hue < 60)
        return v1 * 60 + (v2 - v1) * hue;
    if (hue < 180)
        return v2 * 60;
    if (hue < 240)
        return v1 * 60 + (v2 - v1) * (240 - hue);
    return v1 * 60;
}


int makeColor(unsigned int hue, unsigned int saturation, unsigned int lightness)
{
    unsigned int red, green, blue;
    unsigned int var1, var2;

    if (hue > 359) hue = hue % 360;
    if (saturation > 100) saturation = 100;
    if (lightness > 100) lightness = 100;

    // algorithm from: http://www.easyrgb.com/index.php?X=MATH&H=19#text19
    if (saturation == 0) {
        red = green = blue = lightness * 255 / 100;
    } else {
        if (lightness < 50) {
            var2 = lightness * (100 + saturation);
        } else {
            var2 = ((lightness + saturation) * 100) - (saturation * lightness);
        }
        var1 = lightness * 200 - var2;
        red = h2rgb(var1, var2, (hue < 240) ? hue + 120 : hue - 240) * 255 / 600000;
        green = h2rgb(var1, var2, hue) * 255 / 600000;
        blue = h2rgb(var1, var2, (hue >= 120) ? hue - 120 : hue + 240) * 255 / 600000;
    }
    LRGB rgb;
    rgb.r = red;
    rgb.g = green;
    rgb.b = blue;
    return rgbpack(&rgb);
    // return (red << 16) | (green << 8) | blue;
}


int oper_hsvr(int h, int s, int v)
{
  CHSV hsv;
  CRGB rgb;
  LRGB srgb;
  hsv.val = max(min(((v * 255) / 100) % 256, 255), 0);
  hsv.sat = max(min(((s * 255) / 100) % 256, 255), 0);
  hsv.hue = max(min(((h * 255) / 360) % 256, 255), 0);
  hsv2rgb_rainbow(hsv, rgb);
  srgb.r = rgb.red;
  srgb.g = rgb.green;
  srgb.b = rgb.blue;
  return rgbpack(&srgb);
}

int __elementAt(Value* v, int i)
{
    switch(v->vtype) {
      case ARRAY:
          if (i < v->len)
              return v->ia[i];
          else
              return 0;
      case INT:
          return v->inum;
      case FLOAT:
          return (int)v->fnum;
      default:
          return 0;
    }
}

void op_hsvr()
{
  trinary(&oper_hsvr, NULL);
}


int oper_hsv(int h, int s, int v)
{
  return makeColor(h, s, v);
}

void op_hsv()
{
  trinary(&oper_hsv, NULL);
}


void rgbblend(LRGB* a, LRGB* b, int ratio, LRGB* rs)
{
    rs->r = ((b->r * ratio) + (a->r * (100-ratio))) / 100;
    rs->g = ((b->g * ratio) + (a->g * (100-ratio))) / 100;
    rs->b = ((b->b * ratio) + (a->b * (100-ratio))) / 100;
}


int cblend(int a, int b, int ratio)
{
    LRGB rgba;
    LRGB rgbb;
    rgbunpack(a, &rgba);
    rgbunpack(b, &rgbb);
    rgbblend(&rgba, &rgbb, ratio, &rgba);
    return rgbpack(&rgba);
}

void op_rgbToColor ()
{
    LRGB rgb;
    rgb.b = vstk->popint();
    rgb.g = vstk->popint();
    rgb.r = vstk->popint();
    vstk->push(rgbpack(&rgb));
}


void op_colorToRgb ()
{
    LRGB rgb;
    int c = vstk->popint();
    rgbunpack(c, &rgb);
    vstk->push(rgb.r);
    vstk->push(rgb.g);
    vstk->push(rgb.b);
}


void op_rgb_blend()
{
    int ratio = vstk->popint();
    int b = vstk->popint();
    int a = vstk->popint();
    vstk->push(cblend(a, b, ratio));
}


void op_argb_blend()
{
    int ratio = vstk->popint();
    Value* vb = vstk->pop();
    Value* va = vstk->pop();
    if (va->vtype == ARRAY && vb->vtype == ARRAY && va->len == vb->len) {
        for (int i = 0; i < va->len; i++)
            va->ia[i] = cblend(va->ia[i], vb->ia[i], ratio);
        vstk->push(va);
        vfree(vb);
    } else {
        vfree(va);
        vfree(vb);
    }
}


void op_redef()
{
    char* w = vstk->popstring();
    Value* v = vstk->pop();
    dict->def(w, v);
    strdelete(w);
}


void op_forget()
{
    char* w = vstk->popstring();
    dict->forget(w);
    strdelete(w);
}


void op_def()
{
    char* w = vstk->popstring();
    Value* v = vstk->pop();
    dict->forget(w);
    dict->def(w, v);
    strdelete(w);
}


void op_defp()
{
    char* word = vstk->popstring();
    Sym* sym = dict->findsym(word);
    if (sym)
        vstk->push(1);
    else
        vstk->push(0);
    strdelete(word);
}


void op_varget()
{
    char* word = vstk->popstring();
    Sym* sym = dict->findsym(word);
    if (sym)
        vstk->push(valloc(sym->value));
    else
        vstk->push(0);
    strdelete(word);
}


void op_step()
{
    step_on = true;
}


void op_rb()
{
    void(* freset) (void) = 0;
    freset();
}


void op_mem_malloc()
{
  vstk->push(mem.vmalloc);
}

void op_mem_alloc()
{
  vstk->push(mem.vallocated);
}

void op_mem_free()
{
  vstk->push(mem.vfreed);
}

void op_mem_calloc()
{
  vstk->push(mem.vcurrentallocated);
}

void op_mem_cfree()
{
  vstk->push(mem.vcurrentfreed);
}

void op_mem_amalloc()
{
  vstk->push(mem.amalloc);
}

void op_mem_afreed()
{
  vstk->push(mem.afreed);
}

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}

void op_free_sram()
{
  vstk->push(freeMemory());
}


//
// register built-in words
//
void defineBuiltins ()
{
  dict->def("+", &op_add);
  dict->def("-", &op_sub);
  dict->def("*", &op_mul);
  dict->def("/", &op_div);
  dict->def("mod", &op_mod);
  
  dict->def("sq", &op_sq);
  dict->def("sqrt", &op_sqrt);
  dict->def("constrain", &op_constrain);
  dict->def("sin", &op_sin);
  dict->def("cos", &op_cos);
  dict->def("tan", &op_tan);
  dict->def("deg", &op_deg);
  dict->def("rad", &op_rad);
  dict->def("pow", &op_pow);
  dict->def("abs", &op_abs);

  dict->def("min", &op_min);
  dict->def("max", &op_max);
  dict->def("round", &op_round);
  dict->def("ceil", &op_ceil);
  dict->def("floor", &op_floor);

  dict->def("stack:size", &op_stack_size);
  dict->def("num:dec", &op_num_dec);
  dict->def("num:sci", &op_num_sci);
  dict->def("str:mid", &op_str_mid);
  
  dict->def("dup", &op_dup);
  dict->def("over", &op_over);
  dict->def("aty", &op_aty);
  dict->def("atz", &op_atz);
  dict->def("atu", &op_atu);
  dict->def("atv", &op_atv);
  dict->def("atw", &op_atw);
  dict->def("at", &op_at);
  dict->def("swap", &op_swap);
  dict->def("rot", &op_rot);
  dict->def("rup", &op_rup);
  dict->def("rot4", &op_rot4);
  dict->def("rup4", &op_rup4);
  dict->def("rotn", &op_rotn);
  dict->def("rupn", &op_rupn);
  dict->def("drop", &op_drop);
  dict->def("dup2", &op_dup2);
  dict->def("drop2", &op_drop2);
  dict->def("clst", &op_clst);
  
  dict->def(">>>", &op_stash);
  dict->def("<<<", &op_unstash);
  dict->def("<swap>", &op_swapstash);

  // int array operands
  dict->def("sum", &op_sum);
  dict->def("array", &op_array);
  dict->def("identity", &op_identity);
  dict->def("index", &op_index);
  dict->def("geta", &op_geta);
  dict->def("puta", &op_puta);
  dict->def("dgeta", &op_dgeta);
  dict->def("dputa", &op_dputa);
  dict->def("size", &op_size);
  dict->def("map", &op_map);
  
  dict->def("eq", &op_eq);
  dict->def("ne", &op_ne);
  dict->def("gt", &op_gt);
  dict->def("lt", &op_lt);
  dict->def("ge", &op_ge);
  dict->def("le", &op_le);
  dict->def("and", &op_and);
  dict->def("or", &op_or);
  dict->def("not", &op_not);

  dict->def("if", &op_if);
  dict->def("ife", &op_ife);
  dict->def("loop", &op_loop);
  dict->def("repeat", &op_repeat);
  dict->def("call", &op_call);

  dict->def("rgbformat", &op_rgbformat);
  dict->def("rgb>", &op_rgbToColor);
  dict->def(">rgb", &op_colorToRgb);
  dict->def("hsv>", &op_hsv);
  dict->def("hsvr>", &op_hsvr);
  dict->def("blend", &op_rgb_blend);
  dict->def("ablend", &op_argb_blend);

  dict->def("def", &op_def);
  dict->def("redef", &op_redef);
  dict->def("forget", &op_forget);
  dict->def("def?", &op_defp);
  dict->def("vget", &op_varget);
  dict->def("step", &op_step);
  
  dict->def("rb", &op_rb);

  dict->def("mem:malloc", &op_mem_malloc);
  dict->def("mem:alloc", &op_mem_alloc);
  dict->def("mem:free", &op_mem_free);
  dict->def("mem:calloc", &op_mem_calloc);
  dict->def("mem:cfree", &op_mem_cfree);
  dict->def("mem:amalloc", &op_mem_amalloc);
  dict->def("mem:afree", &op_mem_afreed);
  dict->def("mem:sram", &op_free_sram);
}


void(*step_function)(Value*);

void forth_stepfunction(void(*sf)(Value*))
{
    step_function = sf;
}


void runValue(Value* it)
{
    if (it->vtype == FUNC) {
        gfuncparams = it;
        it->func();
    } else if (it->vtype == SYM && it->sym->value->vtype == FUNC) {
      gfuncparams = it->sym->value;
      it->sym->value->func();
    } else {
        vstk->push(valloc(it));
    }
    if (step_on && step_function)
        (*step_function)(it);
}


void runSequence(ValueStack* seq)
{
    Value* it = seq->head;
    while(it) {
        runValue(it);
        it = it->next;
    }
}


// convenience globals for specific words
Value* word_call;
Value* word_vget;
Value* word_def;


void sigil_seq(char* w, ValueStack** seq)
{
    ValueStack* newstack = new ValueStack();
    newstack->mOuter = *seq;
    *seq = newstack;
}


void sigil_endseq(char* w, ValueStack** seq)
{
    *seq = (*seq)->closeSequence();
}


void sigil_get(char* w, ValueStack** seq)
{
    (*seq)->pushTail(w);
    (*seq)->pushTail(valloc(word_vget));
}


void sigil_put(char* w, ValueStack** seq)
{
    (*seq)->pushTail(w);
    (*seq)->pushTail(valloc(word_def));
}


void sigil_hexn(char* w, ValueStack** seq)
{
    (*seq)->pushTail((int) strtol(w, NULL, 16));
}


void sigil_str(char* w, ValueStack** seq)
{
    (*seq)->pushTail(w);
}


char* pending_definition = NULL;

void sigil_define(char* w, ValueStack** seq)
{
    pending_definition = strclone(w);
    sigil_seq(w, seq);
}


void sigil_defend(char* w, ValueStack** seq)
{
    if (pending_definition) {
        sigil_endseq(w, seq);
        (*seq)->pushTail(pending_definition);
        (*seq)->pushTail(valloc(word_def));
        strdelete(pending_definition);
        pending_definition = NULL;
    }
}


void sigil_stack_comment(char* w, ValueStack** seq)
{
    // nop: stack comments are for source code readability only
}


//
// Words are delimited by spaces, and spaces cannot occur in words.
//
// A word is one of the following:
//
//   A sigil, meaning a word prefixed by a special character []@!#':;(
//   A word that is already defined in the dictionary
//   A number (int or float)
//
void parseSequenceWord(char* w, ValueStack** seq)
{
    // check for sigils (single character prefixes)
    switch(w[0]) {
        case '[':
            sigil_seq(&w[1], seq);
            break;
        case ']':
            sigil_endseq(&w[1], seq);
            break;
        case '@':
            sigil_get(&w[1], seq);
            break;
        case '!':
            sigil_put(&w[1], seq);
            break;
        case '#':
            sigil_hexn(&w[1], seq);
            break;
        case '\'':
            sigil_str(&w[1], seq);
            break;
        case ':':
            sigil_define(&w[1], seq);
            break;
        case ';':
            sigil_defend(&w[1], seq);
            break;
        case '(':
            sigil_stack_comment(&w[1], seq);
            break;
         default: {
            // check for a defined word
            Sym* sym = dict->findsym(w);
            if (sym) {
                // TODO: this could be simpler and could better account for words that are not yet defined.
                if (sym->value->vtype == SEQ) {
                    // set up a call() function
                    Value* call = valloc(word_call);
                    call->seq = sym->value->seq;
                    (*seq)->pushTail(call);
                } else {
                    (*seq)->pushTail(valloc(sym));
                }
                break;
            }
        
            // everything else is a number
            if (strstr(w, ".") != NULL)
                (*seq)->pushTail(atof(w));
            else
                (*seq)->pushTail(atoi(w));
         }
    }
}


bool unu_comment = false;
ValueStack* current_seq = NULL;

void forth_unu(bool state)
{
  unu_comment = state;
}

void forth_run(const char* line)
{
    if (line[0] == '/' && line[1] == '/') {
        // comment line, please ignore
    } else if (line[0] == '~' && line[1] == '~' && line[2] == '~') {
        unu_comment = !unu_comment;
    } else if (!unu_comment) {
        if (current_seq == NULL) {
            current_seq = new ValueStack();
        }
        char* cstr = strclone(line);
        char* rest = cstr;
        char* p;
        while ((p = strtok_r(rest, " ", &rest)) != 0)
        {
            parseSequenceWord(p, &current_seq);
        }
        strdelete(cstr);
        if (current_seq->mOuter == NULL) {
            ValueStack* thisseq = current_seq;
            current_seq = NULL;
            runSequence(thisseq);
            thisseq->deleteSequences();
            delete thisseq;
        }
    }
}


void forth_run(Value* v)
{
  if (v->vtype == SEQ)
    runSequence(v->seq);
  else if (v->vtype == STR)
    forth_run(v->str);
}


void forth_init()
{
    vstk = new ValueStack();
    vstash = new ValueStack();
    dict = new FDict();
    defineBuiltins();
    word_call = dict->find("call");
    word_vget = dict->find("vget");
    word_def = dict->find("def");
}
