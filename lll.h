#ifndef LLL_H_
#define LLL_H_

#define SYMTAB_INIT_SIZE 8
#define OPSTACK_INIT_SIZE 1024

typedef struct sn sn_t;
typedef struct atom atom_t;
typedef struct cons cons_t;
typedef struct prim prim_t;
typedef struct module module_t;
typedef struct module_entry module_entry_t;
typedef struct obj obj_t;

#define ISNIL(a) (a.car == NULL && a.cdr == NULL)

typedef enum flag {
  ATOM_T,
  CONS_T,
  CLOS_T,
  PRIM_T,
  MODULE_T
} flag_t;

typedef enum atom_flag {
  FIXNUM_T,
  FLONUM_T,
  STRING_T,
  SYMBOL_T,
  KEYWORD_T
} atom_flag_t;

typedef enum opcode {
  OP_DISPATCH,
  OP_DONE,
  OP_POPJ_RET,
  OP_IF_DECIDE,
  OP_APPLY_NO_ARGS,
  OP_ARGS,
  OP_ARGS_1,
  OP_ARGS_2,
  OP_LAST_ARG,
  OP_APPLY
} opcode_t;

struct atom {
  atom_flag_t flag;
  union {
    long fixnum;
    double flonum;
    struct {
      char *data;
      size_t length;
    } string;
  };
};

struct cons {
  obj_t *car;
  obj_t *cdr;
};

struct prim {
  obj_t *(*func)(sn_t *S, obj_t *a);
  int arity; /* minimum number of args for application. */
  int max_arity; /* maximum number of args for this function
                    -1 is unlimited */
};

struct module_entry {
  char *name;
  obj_t *(*func)(sn_t *, obj_t *);  
  int arity;
  int max_arity;
};

struct obj {
  flag_t flag;
  union {
    atom_t atom;
    cons_t cons;
    prim_t prim;
  };
};

struct sn {
  obj_t *NIL;
  obj_t *Toplevel_Env;
  obj_t *Env;
  obj_t *Exp;
  obj_t *Clink;
  obj_t *Val;
  obj_t *Args;
  obj_t *FN;
  obj_t *IF;
  obj_t *QUOTE;
  obj_t **Symtab;
  size_t Symtab_alloc;
  int Symtab_index;
  opcode_t *Opstack;
  size_t Opstack_alloc;
  int Opstack_index;
};

void print_object(sn_t *S, FILE *out, obj_t *o);
obj_t *read_object(sn_t *S, FILE *in);

obj_t *mk_fixnum(sn_t *S, long d);
obj_t *mk_flonum(sn_t *S, double d);
obj_t *mk_str(sn_t *S, char *str, size_t len);
obj_t *mk_sym(sn_t *S, char *str, size_t len, int keywordp);
obj_t *intern(sn_t *S, char *str, size_t len);
obj_t *mk_clos(sn_t *S, obj_t *code, obj_t *env);
obj_t *mk_prim(sn_t *S, obj_t *(*func)(sn_t *, obj_t *),
               int minarity, int maxarity);
obj_t *mk_module(sn_t *S, module_entry_t *entries);

obj_t *cons(sn_t *S, obj_t *a, obj_t *d);
obj_t *car(sn_t *S, obj_t *a);
obj_t *cdr(sn_t *S, obj_t *a);
int length(sn_t *S, obj_t *a);

obj_t *eval(sn_t *S, obj_t *a, obj_t *env);

obj_t *module_install(sn_t *S, char *name, module_entry_t *);

void install_builtins(sn_t *S);

#endif
