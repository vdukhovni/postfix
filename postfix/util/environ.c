 /*
  * The environ.c module from TCP Wrappers is not bundled with IBM's public
  * release. It will be made available as contributed software from
  * http://www.postfix.org/
  */
#include "sys_defs.h"

#ifdef MISSING_SETENV_PUTENV
#error "This requires contributed software from http://www.postfix.org/"
#endif
