#ifndef COMMON_INSTR_CALLBACKS_HPP
#define COMMON_INSTR_CALLBACKS_HPP
// clang-format off

// Some macros to make it "easier" and more obscure
// to create the probes replacements

// Proto definitions
//
// How to use it:
//  PROTO proto = START_PROTO(void)       /* the return type is passed here */
//                  PIN_PARG(PyObject *), /* first formal param */
//                  ...
//                END_PROTO;
//  CLEAR_PROTO(proto);
//
#define START_PROTO(ret_type) PROTO_Allocate(PIN_PARG(ret_type), \
                                             CALLINGSTD_DEFAULT, \
                                             rtnName.c_str(),
#define END_PROTO             PIN_PARG_END())
#define CLEAR_PROTO(proto)    PROTO_Free(proto)


// Signature replacement definitions
//
// How to use it:
//
//  START_REPLACE_JITTED(wrapper_to_interesting_function, proto)
//    REPLACE_INSERT_ARG(0), /* formal param number to pass to the wrapper */
//    REPLACE_INSERT_ARG(1),
//    ...
//  END_REPLACE;
//
#define REPLACE_INSERT_ARG(num)  IARG_FUNCARG_ENTRYPOINT_VALUE, num


#define START_REPLACE_JITTED(fptr, proto)     \
  RTN_ReplaceSignature(routine,               \
                       AFUNPTR(fptr),         \
                       IARG_PROTOTYPE, proto, \
                       IARG_THREAD_ID,        \
                       IARG_CONTEXT,          \
                       IARG_ORIG_FUNCPTR,


#define START_REPLACE_PROBED(fptr, proto)     \
  RTN_ReplaceSignatureProbed(routine,               \
                             AFUNPTR(fptr),         \
                             IARG_PROTOTYPE, proto, \
                             IARG_THREAD_ID,        \
                             IARG_CONTEXT,          \
                             IARG_ORIG_FUNCPTR,

#define END_REPLACE              IARG_END)

// clang-format on

#endif
