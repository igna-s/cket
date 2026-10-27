# Checked calls to quantum function templates

Quantum functions are templates expanded at checked call sites in the same translation unit. They do
not have a general C calling convention. A compatible C prototype alone cannot turn ordinary integer
bits into quantum owners.

Previously, an ordinary call or function pointer recorded before a later `quant` definition could
keep a classical entry point. That entry returned without performing the quantum operation. Global
function-pointer initializers could also bypass the local address check. These were false successful
executions, not quantum implementations of an indirect-call ABI.

The compiler records ordinary references by assembler symbol and checks them when a later definition
becomes quantum. It checks global pointer initializers as well as local references. Known quantum
function addresses remain rejected. Constructors and destructors cannot run a quantum template as an
automatic classical callback.

The remaining native entry now reports an explicit runtime error if reached through foreign or
dynamic lookup. It never prepares a quantum state from those arguments or returns a fake result.
Valid checked calls expand the saved template and keep the existing ownership, phase and inverse
contracts. Ordinary C callbacks, including callbacks used for CPU work inside a quantum procedure,
keep their classical implementation.

The validator covers 90 cases at five GCC levels: 30 valid native compositions, 55 compile-time ABI
diagnostics and five explicit runtime rejections through a POSIX dynamic lookup. It exercises
forward declarations, global/local pointers, arrays and structs of pointers, callback arguments,
nonvoid results and automatic callbacks. The dynamic case is a Linux/glibc boundary check, not
support for quantum function pointers.

[Structured CPU early returns](c11-procedure-returns.md) are supported in void procedures, including
CPU loops. They finish lexical cleanup before continuing the caller; owned early results and general
control flow keep their separate restrictions.

Recursion, general indirect quantum calls and cross-unit quantum linking remain pending. The
complete integration gate also remains open; these checks do not qualify all C programs.
