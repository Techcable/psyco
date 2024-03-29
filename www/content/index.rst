Home Page
*********

High-level languages need not be slower than low-level ones.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Psyco** is a Python extension module which can greatly speed up the execution of any Python code.


News
====

4 March 2010

    On Mac OS/X "Snow Leopart" you don't actually need to recompile
    Python yourself, but just use the following command line
    (thanks Elvis Pfützenreuter)::

        defaults write com.apple.versioner.python Prefer-32-Bit -bool yes

8 December 2009

    Just for reference, Psyco does not work on any 64-bit systems at all.
    This fact is worth being noted again, now that the latest Mac OS/X
    10.6 "Snow Leopart" comes with a default Python that is 64-bit on
    64-bit machines.  The only way to use Psyco on OS/X 10.6 is by
    recompiling a custom Python in 32-bit mode (on Linux, see
    `here@http://bruynooghe.blogspot.com/2009/02/compiling-32-bit-python-on-amd64.html`
    or `here@http://pypy.org/download.html#here-are-hints` for example).

20 January 2009

    Michael Foord contributed a Windows binary for Python 2.6 (as well as
    Psyco 1.5.1 binaries for Python 2.2 and 2.3).  Get them
    `directly from voidspace.org.uk.@http://www.voidspace.org.uk/python/modules.shtml#psyco`

7 January 2008

    Thanks Chaiwat Suttipongsakul for contributing a Win32 binary for
    Python 2.5!  Get it from the
    `downloads@http://sourceforge.net/project/showfiles.php?group_id=41036`
    page.

16 December 2007

    Released `Psyco 1.6@http://sourceforge.net/project/showfiles.php?group_id=41036` with Intel MacOS/X support, thanks to Gary Grossman.  Also includes a couple of general bug fixes (Windows binaries: contribution welcome).  From this version onwards we no longer support Pythons older than 2.2.2.

28 September 2007
    Sorry, the file at http://codespeak.net/~arigo/psyco-mactel.diff was
    missing for a while.  It's back now.

26 September 2007

    Psyco was ported to Intel Mac OS/X by Gary Grossman.  I will review and
    check in his code; in the meantime the patch is available there:
    http://codespeak.net/~arigo/psyco-mactel.diff

2 May 2007

    The *snake* machine is down - died of too much PyPy - so the
    snapshot of the latest sources is now at
    http://wyvern.cs.uni-duesseldorf.de/psyco/psyco-snapshot.tar.gz .
    See `installing from sources@psycoguide/sources.html`.

26 January 2007

    Fixed a bug triggered by Python 2.5's bytecode optimizer.  Users of 2.5
    should either avoid ``psyco.profile()`` or upgrade to the
    `subversion head@http://codespeak.net/svn/psyco/dist/`.

8 December 2006

    Got a Win32 binary for Python 2.4.  See
    `downloads@http://sourceforge.net/project/showfiles.php?group_id=41036`.
    Contributed by ffao.

29 November 2006

    Fixed the corrupted Win32 binary for Python 2.5.  See
    `downloads@http://sourceforge.net/project/showfiles.php?group_id=41036`.
    Thanks again to Jon Foster.  The corruption of binary files attached to 
    SourceForge's patch tracker seems to be a known problem...
    (Note that as far as I can tell, mirrors make take a little while
    before they have got the updated version.  The correct version has an MD5
    sum of 14194043a1488c7a33ad7f4ba7c2171c.)

27 November 2006

    The Win32 binary installer seems to be broken :-(
    Updates to follow...

22 November 2006

    Got a Win32 binary for Python 2.5.  See
    `downloads@http://sourceforge.net/project/showfiles.php?group_id=41036`.
    Thanks Jon Foster!

21 November 2006

    Released `Psyco 1.5.2@http://sourceforge.net/project/showfiles.php?group_id=41036` with Python 2.5 support.  (Windows binaries: contribution welcome.)


About
=====

+-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------+
| `<!-- --><A href="http://sourceforge.net"> <IMG src="http://sourceforge.net/sflogo.php?group_id=41036&amp;type=5" width="210" height="62" border="0" alt="SourceForge Logo"></A>` | `This site@http://psyco.sourceforge.net` has been last updated `$LASTUPDATED`.|
|                                                                                                                                                                                   +-------------------------------------------------------------------------------+
|                                                                                                                                                                                   | Download the whole site (pages and documents, not                             |
|                                                                                                                                                                                   | Psyco itself) in one click: `psyco-site.tar.gz` or                            |
|                                                                                                                                                                                   | `psyco-site.zip`.                                                             |
|                                                                                                                                                                                   +-------------------------------------------------------------------------------+
|                                                                                                                                                                                   | The site is built with customized                                             |
|                                                                                                                                                                                   | `reStructuredText@http://docutils.sourceforge.net/rst.html`                   |
|                                                                                                                                                                                   | mark-up and hosted on `SourceForge@http://sourceforge.net`.                   |
+-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------+

Contact me: *arigo* @ *users.sourceforge.net*
