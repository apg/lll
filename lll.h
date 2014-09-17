#ifndef LLL_H_
#define LLL_H_

#define SYMTAB_INIT_SIZE 8
#define OPSTACK_INIT_SIZE 1024

typedef struct atom atom_t;
typedef struct cons cons_t;
typedef struct prim prim_t;
typedef struct obj obj_t;

#define ISNIL(a) (a.car == NULL && a.cdr == NULL)

typedef enum flag {
  ATOM_T,
  CONS_T,
  CLOS_T,
  PRIM_T
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
  obj_t * (*func)(obj_t *a);
};

struct obj {
  flag_t flag;
  union {
    atom_t atom;
    cons_t cons;
    prim_t prim;
  };
};

typedef struct sn {
} sn_t;

void print_object(FILE *out, obj_t *o);
obj_t *read_object(FILE *in);

obj_t *mk_fixnum(long d);
obj_t *mk_flonum(double d);
obj_t *mk_str(char *str, size_t len);
obj_t *mk_sym(char *str, size_t len, int keywordp);
obj_t *intern(char *str, size_t len);
obj_t *mk_clos(obj_t *code, obj_t *env);

obj_t *cons(obj_t *a, obj_t *d);
obj_t *car(obj_t *a);
obj_t *cdr(obj_t *a);

obj_t *eval(obj_t *a, obj_t *env);

#endif
