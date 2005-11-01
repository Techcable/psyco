Introduction
************

Psyco is a *specializing compiler*. In a few words let us first see:


What you can do with it
+++++++++++++++++++++++

In short: run your existing Python software much faster, with no change in your source.

Think of Psyco as a kind of just-in-time (JIT) compiler, a little bit like what exists for other languages, that emit machine code on the fly instead of interpreting your Python program step by step. The difference with the traditional approach to JIT compilers is that Psyco writes several version of the same blocks (a block is a bit of a function), which are optimized by being specialized to some kinds of variables (a "kind" can mean a type, but it is more general). The result is that your *unmodified* Python programs run faster.

Benefits

    2x to 100x speed-ups, typically 4x, with an unmodified Python interpreter and unmodified source code, just a dynamically loadable C extension module.

Drawbacks

    Psyco currently uses a lot of memory. It only runs on Intel 386-compatible processors (under any OS) right now. There are some subtle semantic differences (i.e. bugs) with the way Python works; they should not be apparent in most programs.


Expected results
++++++++++++++++

The actual performance gains can be very large. For common code, expect at least a 2x speed-up, more typically 4x. But where Psyco shines is when running algorithmical code --- these are the first pieces of code that you would consider rewriting in C for performance. If you are in this situation, consider using Psyco instead! You might get 10x to 100x speed-ups. It is theoretically possible to actually speed up this kind of code up to the performance of C itself.

Because of the nature of Psyco, it is difficult to forecast the actual performance gains for a given program. Just try and see.

The memory overhead of Psyco is currently large. I has been reduced a bit over time, but it is still an overhead. This overhead is proportional to the amount of Python code that Psyco rewrites; thus if your application has a few algorithmic "core" functions, these are the ones you will want Psyco to accelerate --- not the whole program.

Psyco can transparently use a Python profiler to automatically select which functions it is interesting to accelerate.


Differences with traditional JIT compilers
++++++++++++++++++++++++++++++++++++++++++

The basic idea behind "traditional" JIT compilers is to write one machine-code version of each of your function, delivering a constant speed-up (typically around 2x).  This is for example how Java JITs work -- although modern JITs use many more advanced techniques on top of that.  But the basic idea behind Psyco is slightly different. Psyco uses the actual run-time data that your program manipulates to write potentially several versions of the machine code, each differently *specialized* for different kinds of data. Depending on how well it can do it, you can get smaller or higher speed-ups. In extreme cases, when all computations can be done in advance, nothing remains to be done at run-time. In this sense, Psyco is a Just-In-Time Specializer, not really a compiler at all. See `How does it work?@doc.html#how-does-it-work` for more details.

There is no static analysis of your program, no separated compilation phase, no type inference. It is all done at run-time: Psyco infers from the values your program manipulates some restrictions about the variables, like always containing an integer, or a list of strings of length 1, or a tuple whose first item is zero. Using these restrictions, efficient machine code can be emitted. Only this second phase -- code generation -- is similar to what a C or a JIT compiler does. In statically typed languages (C, Java...) the restrictions are static: they come from the programmer's type declarations. The purpose of Psyco is to try and dynamically build the restrictions that will produce the best code for the currently-manipulated data. The flexibility comes from the fact that if any data that does not fit is later found, new machine code can be emitted.

It means that your program works in all cases, but the common case gets the fastest code without the overhead of having to care about the exceptional cases. In this perspective we can theoretically expect *faster* results than what you get with low-level languages: programs optimized for the data that it *currently* handles.


My goals
++++++++

My goal in programming Psyco is to contribute to reduce the following wide gap between academic computer science and industrial programming tools.

While the former develops a number of programming languages with very cool semantics and features, the latter stick with low-level languages principally for performance reasons, on the ground that the higher the level of a language, the slower it is. Althought clearly justified in practice, this belief is theoretically false, and even completely inverted --- for large, evolving systems like a whole operating system and its applications, high-level programming can deliver much higher performances.

The new class of languages called "dynamic scripting languages", of which Python is an example, is semantically close to long-studied languages like Lisp. The constrains behind their designs are however different: some high-level languages can be relatively well statically compiled, we can do some type inference, and so on, whereas with Python it is much harder --- the design goal was different. We now have powerful machines to stick with interpretation for a number of applications. This, of course, contributes to the common belief that high-level languages are terribly slow.

Psyco is both an academic and an industrial project. It is an academic experiment testing some new techniques in the field of on-line specialization. It develops an industrially useful performance benefit for Python. And first of all it is a modest step towards:

    *High-level languages are faster than low-level ones!*


Future plans
++++++++++++

Psyco is a reasonably complete project.  I will not continue to develop it beyond making sure it works with future versions of Python.  My plans for 2006 are to port the techniques implemented in Psyco to `PyPy@http://codespeak.net/pypy/`.  PyPy will allow us to build a more flexible JIT specializer, easier to experiment with, and without the overhead of having to keep in sync with the evolutions of the Python language.
