#! /usr/bin/env python

from docutils.core import publish
from docutils import utils
import wikireader, wikiwriter


def convert(source, target, translator, kw=None):
    reporter = utils.Reporter(2, 4)
    writer = wikiwriter.Writer()
    writer.translator = translator
    if kw is not None:
        writer.translator_kw = kw
    if source == '-':
        source = None
    publish(source=source,
            destination=target,
            writer=writer,
            reader=wikireader.Reader(reporter, 'en'),
            reporter=reporter)


def main():
    import sys, getopt
    opts, args = getopt.getopt(sys.argv[1:], 'o:t:k:',
                               ['output=', 'translator=', 'kw='])
    if len(args) != 1:
        raise ValueError, "wrong number of arguments (expected 1)"
    source = args[0]
    output = None
    translator = None
    kw = None
    for o, a in opts:
        if o in ('-o', '--output'):
            output = a
        elif o in ('-t', '--translator'):
            translator = a
        elif o in ('-k', '--kw'):
            kw = a
    if translator is not None:
        d = {}
        execfile(translator, d)
    else:
        d = wikiwriter.__dict__
    if kw is not None:
        kw = eval('{' + kw + '}')
    convert(source, output, d['HTMLTranslator'], kw)

if __name__ == '__main__':
    main()
