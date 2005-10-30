Download
********

Subversion access to the sources
================================

The source code of Psyco is hosted in a `Subversion@http://subversion.tigris.org` repository at http://codespeak.net/svn/psyco/dist/ .

It is recommended to install from Subversion instead of using the latest official release: the Subversion repository evolves very slowly and is completely stable.  Minor bug fixes don't go into a new official release.  See `installing from sources@psycoguide/sources.html`.

Current version is 1.5
======================

You can `download Release 1.5@http://sourceforge.net/project/showfiles.php?group_id=41036`.  But if you can, it is recommended to install from source: see the documentation for how to install from `precompiled binaries@psycoguide/binaries.html` or `from sources@psycoguide/sources.html`.

Be sure to check the `requirements@psycoguide/req.html`.

Other tools
===========

Windows users who want to explore the generated machine code need `objdump.exe@http://cvs.sourceforge.net/viewcvs.py/psyco/psyco/py-utils/win32/objdump.exe?rev=1.1&view=auto` which itself needs `cygwin1.dll@http://cvs.sourceforge.net/viewcvs.py/psyco/psyco/py-utils/win32/cygwin1.dll?rev=1.1&view=auto`. Both are GPL software distributed (with source) in `Red Hat's Cygwin@http://www.redhat.com/software/cygwin/`. They should be put in the Psyco subdirectory ``py-utils\win32``.

The required tool, objdump, is generally preinstalled on Linux distributions.
