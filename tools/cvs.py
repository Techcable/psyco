import sys, os


def file2unix(name):
    "Turn a file's end-of-line into Unix flavor without changing the timestamp."
    result = 0
    data = []
    s = os.stat(name)
    f = open(name, 'r')
    for line in f:
        if line[-2:] == '\r\n':
            line = line[:-2] + '\n'
            result = 1
        data.append(line)
    f.close()
    if result:
        f = open(name, 'w')
        f.writelines(data)
        f.close()
    os.utime(name, (s.st_atime, s.st_mtime))
    return result

class Directory:
    def __init__(self, srcpath):
        self.path = srcpath
        entryname = os.path.join(srcpath, 'CVS', 'Entries')
        f = open(entryname, 'r')
        lines = f.readlines()
        f.close()
        entryname2 = os.path.join(srcpath, 'CVS', 'Entries.Log')
        try:
            f = open(entryname2, 'r')
        except IOError:
            pass
        else:
            for line in f.readlines():
                if line[:2] == 'A ':
                    lines.append(line[2:])
                elif line[:2] == 'R ':
                    lines.remove(line[2:])
            f.close()
        self.subdirs = []
        self.fileinfo = {}
        for line in lines:
            line = line.split('/')
            if len(line) >= 6:
                fname1 = line[1]
                if 'D' in line[0]:
                    self.subdirs.append(fname1)
                else:
                    self.fileinfo[fname1] = line
    def subdir(self, name):
        return Directory(os.path.join(self.path, name))
    def alldirs(self):
        result = [self]
        for name in self.subdirs:
            result += self.subdir(name).alldirs()
        return result


if __name__ == '__main__':
    # Example: print the name of recursively all the files in the
    # given path list
    for path in sys.argv[1:]:
        root = Directory(path)
        for dir in root.alldirs():
            for name in dir.fileinfo.keys():
                print os.path.join(dir.path, name)
