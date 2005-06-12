Links
*****


SourceForge
===========

Psyco is hosted on SourceForge. Go to the `SourceForge project page.@http://sourceforge.net/projects/psyco`


Related projects
================

Among the related projects, Psyco's unique features are that it works on standard unmodified Python code, and never uses a C compiler.

- `Pyrex@http://www.cosc.canterbury.ac.nz/~greg/python/Pyrex/` lets you write code that mixes Python and C data types any way you want, and compiles it into a C extension for Python. This is a *compile-time* tool that uses programmer annotations in the Python source to produce a more efficient C version of your code, which you then compile with a regular C compiler.

- `Weave@http://www.scipy.org/site_content/weave/` is a tool for inline C/C++ in Python. The basic functionality lets you write C or C++ code in the middle of your Python program. It calls (and thus requires) a C/C++ compiler at run-time. On top of this you will also find functions that directly accept Python Numeric expression, or that build a regular C extension module for static compilation.

Dynamic compiler back-ends
==========================

Here are a couple of non-Pythonic dynamic compilation projects. I am often pointed to them as potentials back-ends for Psyco, to help make Psyco portable beyond the current x86 machine code. However, I consider them as not really suited for Psyco. The problem is that they all assume things about the way the code will be emitted, e.g. that we emit a whole function at a time. This model does not suit Psyco at all, where I need all the time to write a small portion of a function, run it, then compile the sequel, and so on. This said, I believe that e.g. a Dynamo-based project could indeed give very good results if applied to Python. This would just be a different project: Psyco does not really work like that. It is indirectly based on the general notion of `partial evaluation@http://www.google.be/search?q=partial+evaluation`.

- `vcode@http://www.pdos.lcs.mit.edu/~engler` is a very fast dynamic code generation system.

- `Dynamo@http://www.cs.indiana.edu/proglang/dynamo/` is a dynamic compiler and optimizer.

- `GNU lightning@http://www.gnu.org/software/lightning/` is a simple library for dynamic code generation.

- `Self@http://research.sun.com/self/` is a dynamic language for which quite some research has been done on optimization.
