Home Page
*********

High-level languages need not be slower than low-level ones.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


**Psyco** is a Python extension module which can massively speed up the execution of any Python code.


News
====

25 September 2006
    Python 2.5 has been released.  The `subversion source@http://codespeak.net/svn/psyco/dist/` works fine with it.  Don't use the Psyco 1.5.1 release with Python 2.5: there has been a change in the way Python handles the new special method ``__index__()`` between the alpha/beta and the final release, which will probably crash any program using it.

    Tagged the current Subversion head as `Psyco 1.5.2@http://sourceforge.net/project/showfiles.php?group_id=41036`.

24 March 2006
    Bugfix release `Psyco 1.5.1@http://sourceforge.net/project/showfiles.php?group_id=41036`.  It fixes a memory leak with functions that contain local 'def' statements or lambdas.  The performance of creating instances of new-style classes has been improved.  It is also compatible with Python 2.5 (which at the moment is only a pair of alpha releases); please report further 2.5 problems as they show up.  Finally, as usual they are some fixes for rare bugs that nevertheless showed up in practice, so yes, of course I recommend upgrading Psyco :-)

23 November 2005
    Added a Windows binary for `Psyco 1.5@http://sourceforge.net/project/showfiles.php?group_id=41036` on Python 2.4 (thanks Alexander!).

30 October 2005
    Tagged the current Subversion head as `Psyco 1.5@http://sourceforge.net/project/showfiles.php?group_id=41036`.  This is probably the last release of Psyco (unless incompatibilities with the upcoming CPython 2.5 show up later, but it works with the CVS CPython 2.5 at the moment).  This release contains nothing new if you already got the Subversion version.

24 September 2005
    Documented how to use the Subversion repository, which is now considered as the most official and stable (and probably ultimate) version of Psyco.

    A common question I get is whether Psyco will work on 64-bit x86 architectures.  The answer is no, unless you have a Python compiled in 32-bit compatibility mode.  If you haven't heard about PyPy, see the following news item.

6 August 2005
    For the last few months, Psyco has been hosted on http://codespeak.net in a `Subversion repository@http://codespeak.net/svn/psyco/dist`.  However, Psyco has not been in very active development for quite a while now.  I consider that the project is as complete as it can reasonably be.  Developing it further would be possible and interesting, but require much more efforts that I want to invest.  The future of Psyco now lies in the `PyPy@http://codespeak.net/pypy` project, which according to plan will provide a good base for a Python interpreter with better and well-integrated Psyco-like techniques as soon as 2006.  (Additionally, it is not impossible that we could even derive a C extension module for CPython very similar to today's Psyco.)  So stay tuned to PyPy!

14 January 2005
    Windows installers for `Psyco 1.4@http://sourceforge.net/project/showfiles.php?group_id=41036`. Note that they don't include the documentation nor the test and example files.

6 January 2005
    Source release `Psyco 1.4@http://sourceforge.net/project/showfiles.php?group_id=41036`. I will not release precompiled binary for all versions because I want to be able to release more quickly after a small change like a segfault fix. This release still gets a new version number because it contains an interesting new optimization: instances of user-defined classes are now really supported, i.e. their attributes keep type information and are stored quite compactly in memory. However, it only works so far with instances of a new type ``psyco.compact`` (which is subclassable). The line ``from psyco.classes import *`` has the effect of turning your classes into psyco.compact subclasses, too. For more information, see the new paragraph in the user guide about `psyco.compact@http://psyco.sourceforge.net/psycoguide/psycocompact.html`.

3 December 2004
    Release `Psyco 1.3@http://sourceforge.net/project/showfiles.php?group_id=41036`.  Includes support for Python 2.4 (and of course still supports Python 2.1 to 2.3).  As always it comes with a few bugfixes, including a memory leak when using the profiler.  Another good news is that the built-in functions that read the local variables -- locals(), eval(), execfile(), vars(), dir(), input() -- now work correctly!

30 July 2004
    Psyco will be presented at the `PEPM'04@http://profs.sci.univr.it/~pepm04/`
    conference, part of ACM SIGPLAN 2004.
    The paper is available (compressed Postscript `[A4]@psyco-pepm-a.ps.gz`
    or `[Letter]@psyco-pepm-l.ps.gz`).

29 April 2004
    Following the Python UK conference at
    `ACCU 2004@http://www.accu.org/conference/` here are some
    `animated slides@accu2004-psyco.tgz` that are, as far as I can tell, my
    best attempt so far at trying to explain how Psyco works.
    (`Pygame@http://www.pygame.org` required)

4 March 2004
    Bugfix release `Psyco 1.2@http://sourceforge.net/project/showfiles.php?group_id=41036`. Includes support for Fedora, plus a number of smaller bug fixes. This version does not yet work correctly on platforms other than PCs. I will need to spend some time again on the 'ivm' portable back-end before that dream comes true :-)

21 Aug 2003
    The Linux binaries have been compiled for the recent 'glibc-2.3', although a lot of systems still have 'glibc-2.2'. See the `note about Linux binaries@psycoguide/binaries.html`.

19 Aug 2003
    Fixbug release `Psyco 1.1.1@http://sourceforge.net/project/showfiles.php?group_id=41036&release_id=178943`. Fixes `loading problems@http://sourceforge.net/project/shownotes.php?release_id=178943` both on Windows and Red Hat Linux.

15 Aug 2003
    Released `Psyco 1.1@http://sourceforge.net/project/showfiles.php?group_id=41036&release_id=178161`. Contains the enhancements described below, the usual subtle bug fixes, and complete Python 2.3 support.

16 Jun 2003
    Enough new things that I would like to make a release 1.1 soon. Top points: Psyco will now inline calls to short functions, almost cancelling the cost of creating small helpers like 'def f(x): return (x+1) & MASK'. And I have rewritten the string concatenation implementation, as the previous one was unexpectedly inefficient: now using 's=s+t' repeatedly to build a large string is at least as efficient as filling a cStringIO object (and more memory-conservative than using a large list of small strings and calling '"".join()' at the end).

5 May 2003
    `Release 1.0@http://sourceforge.net/project/showfiles.php?group_id=41036&release_id=157214` is out.  Note that Psyco is distributed under the MIT License, and no longer under the GPL as it used to be.

    The plan for the next release is to include a fast low-level interpreter that can be used on non-Intel processors. It will finally make Psyco portable -- althought of course not as fast as it could possibly be if it could emit real machine code.

    IRC users, try irc.freenode.net channel #psyco.

1 May 2003
    Psyco is now compatible with the new `Python 2.3b1@http://www.python.org/2.3/`. This and other bug fixes, plus positive feedback, allow me to officially announce the release of Psyco 1.0 (which should take place in a few hour's time, please come back soon!).

17 Mar 2003
    Major new `beta release 1.0.0b1@http://sourceforge.net/project/showfiles.php?group_id=41036&release_id=147038` containing the accumulated enhancements from the CVS tree!  Also comes with a `complete guide@psycoguide/index.html`!  The web site has been updated; outdated information was removed. I will soon tell more about how I currently see Psyco's future.

12 Sep 2002

    Various bug fixes have been committed in CVS. Next release soon. See also the new `links` page.

11 Aug 2002

    `Release 0.4.1@http://sourceforge.net/project/showfiles.php?group_id=41036` is out. A major new feature I recently added is the reduced memory consumption. On some examples, Psyco uses several times less memory than it used to!

7 Aug 2002

    The new site is up and running. I will take the current CVS source and release it as a stable version within the next few days.

24 Jul 2002

    Psyco talk at the Open Source Convention 2002, San Diego. This talk will eventually be turned into a written document; in the meantime, you can see the `slides@slides/header.html` (or `download them@psyco-slides.zip`).

26 Jun 2002

    Psyco talk at the EuroPython, Charleroi. Same `slides@slides/header.html` as above.

About
=====

+--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------+
| `<!--><A href="http://sourceforge.net"> <IMG src="http://sourceforge.net/sflogo.php?group_id=41036&amp;type=5" width="210" height="62" border="0" alt="SourceForge Logo"></A>` | `This site@http://psyco.sourceforge.net` has been last updated `$LASTUPDATED`.|
|                                                                                                                                                                                +-------------------------------------------------------------------------------+
|                                                                                                                                                                                | Download the whole site (pages and documents, not                             |
|                                                                                                                                                                                | Psyco itself) in one click: `psyco-site.tar.gz` or                            |
|                                                                                                                                                                                | `psyco-site.zip`.                                                             |
|                                                                                                                                                                                +-------------------------------------------------------------------------------+
|                                                                                                                                                                                | The site is built with customized                                             |
|                                                                                                                                                                                | `reStructuredText@http://docutils.sourceforge.net/rst.html`                   |
|                                                                                                                                                                                | mark-up and hosted on `SourceForge@http://sourceforge.net`.                   |
+--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------+

Write me: *arigo* @ *users.sourceforge.net*
