#include <stdlib.h>
#include <stdio.h>
#include "lll.h"

static obj_t *
builtin_cons(sn_t *S, obj_t *args)
{
  obj_t *a, *d;
  int l = length(S, args);
  switch (l) {
  case 1:
    fprintf(stderr, "PROMISEs aren't implemented yet\n");
    return NULL;
  case 2:
    /* essentially, POPARG */
    d = car(S, args);
    args = cdr(S, args);
    a = car(S, args);
    return cons(S, a, d);
  case 0:
  default:
    fprintf(stderr, "ARITY_ERROR: cons requires 2 arguments\n");
    exit(EXIT_FAILURE);
  }
  return NULL;
}

static obj_t *
builtin_list(sn_t *S, obj_t *args)
{
  obj_t *accum = S->NIL, *tmp;
  for (; args != S->NIL && args != NULL; args = cdr(S, args)) {
    tmp = car(S, args);
    accum = cons(S, tmp, accum);
  }
  return accum;
}

static obj_t *
builtin_nil_p(sn_t *S, obj_t *args)
{
  obj_t *arg;
  int l = length(S, args);
  if (l == 1) {
    arg = car(S, args);
    return (arg == NULL || arg == S->NIL) ? intern(S, ":true", 5) : S->NIL;
  }
  fprintf(stderr, "ARITY_ERROR: nil? requires 1 argument\n");
  exit(EXIT_FAILURE);

  return NULL;
}

static obj_t *
builtin_list_head(sn_t *S, obj_t *args)
{
  obj_t *arg;
  int l = length(S, args);
  if (l != 1) {
    fprintf(stderr, "ARITY_ERROR: head requires a single argument\n");
    exit(EXIT_FAILURE);
  }

  arg = car(S, args);

  if (arg == NULL || arg == S->NIL) {
    return S->NIL;
  }

  if (arg->flag == CONS_T) {
    return car(S, arg);
  }  
  
  fprintf(stderr, "TYPE_ERROR: Can't take head of non-list\n");
  exit(EXIT_FAILURE);
  return NULL;
}

static obj_t *
builtin_list_rest(sn_t *S, obj_t *args)
{
  obj_t *arg;
  int l = length(S, args);
  if (l != 1) {
    fprintf(stderr, "ARITY_ERROR: rest requires a single argument\n");
    exit(EXIT_FAILURE);
  }

  arg = car(S, args);

  if (arg == NULL || arg == S->NIL) {
    return S->NIL;
  }

  if (arg->flag == CONS_T) {
    return cdr(S, arg);
  }  
  
  fprintf(stderr, "TYPE_ERROR: Can't take rest of non-list\n");
  exit(EXIT_FAILURE);
  return NULL;
}

static obj_t *
builtin_list_empty_p(sn_t *S, obj_t *args)
{
  obj_t *arg;
  int l = length(S, args);
  if (l != 1) {
    fprintf(stderr, "ARITY_ERROR: empty? requires a single argument\n");
    exit(EXIT_FAILURE);
  }

  arg = car(S, args);

  if (arg == NULL || arg == S->NIL) {
    return intern(S, ":true", 5);
  }
  else if (arg->flag == CONS_T) {
    return S->NIL;
  }
  
  fprintf(stderr, "TYPE_ERROR: Can't call empty? on non list\n");
  exit(EXIT_FAILURE);

  return NULL;
}

static obj_t *
builtin_module_set_b(sn_t *S, obj_t *args)
{
  obj_t *name, *value, *frame, *tmp;
  int l = length(S, args);
  if (l != 2) {
    fprintf(stderr, "ARITY_ERROR: module-set! requires 2 arguments\n");
    exit(EXIT_FAILURE);
  }

  name = car(S, args);
  value = car(S, cdr(S, args));

  frame = car(S, S->Toplevel_Env);
  frame->cons.car = cons(S, value, frame->cons.car);
  frame->cons.cdr = cons(S, name, frame->cons.cdr);

  return S->NIL;
}


static module_entry_t builtins[] = {
  { "cons", builtin_cons, 1, 2 },
  { "list", builtin_list, 0, -1 },
  { "nil?", builtin_nil_p, 1, 1 },

  /* These should actually be part of the SEQ protocol */
  { "head", builtin_list_head, 1, 1 },
  { "rest", builtin_list_rest, 1, 1 },
  { "empty?", builtin_list_empty_p, 1, 1 },

  { "module-set!", builtin_module_set_b, 2, 2 },

  /* { "length", builtin_length, 1, 1 }, */

  /* { "+", builtin_plus, 1, -1 }, */
  /* { "-", builtin_minus, 1, -1 }, */
  /* { "*", builtin_multiply, 1, -1 }, */
  /* { "/", builtin_divide, 1, -1 }, */
  /* { "%", builtin_mod, 1, -1 }, */
  /* { NULL, NULL, 0, 0 } */
};

void
install_builtins(sn_t *S)
{
  module_install(S, "builtins", builtins);
}
