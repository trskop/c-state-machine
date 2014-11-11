Description
===========

Simple low-level state machine in C.

Some interesting features:

* Allows either using transition table or transition function.
* It is designed so that it does not do any memory allocation of its own. This
  may come handy when application uses its own memory management.
* Another interesting one is that it either doesn't do any locking or it uses
  locking primitives supplied by application.
