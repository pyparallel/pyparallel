import sys

try:
    import parallel
except:
    class parallel:
        @classmethod
        def t4(cls, fn, data):
            results = [ fn(d) for d in data ]
            return results

def simple_test():
    data = [ i for i in range(1, 5) ]
    expected = [ i*i for i in data ]

    def f(x):
        z = x * x
        l = list(z)
        d = dict(x=z, y=z)
        s = str(d)
        return z

    result = parallel.map(f, data)
    print(result)

def t1():
    def f(x):
        return x * x

    result = parallel.t1(f, 8)
    print(result)

def t2():
    def f(x):
        return x * x

    result = parallel.t2(f, 8)
    print(result)

def t3():
    def f(x):
        return x * x

    result = parallel.t3(f, 8)
    print(result)

def t4():
    def f(x):
        return x * x

    result = parallel.t4(f, [2, 3])
    print(result)

def t4_2():
    def f(x):
        return x * x

    data = [ i for i in range(1, 150) ]
    result = parallel.t4(f, data)
    print(result)

if __name__ == '__main__':
    if len(sys.argv) == 2:
        fn = "%s()" % sys.argv[1]
        if not fn.startswith('t'):
            fn = 't' + fn
        print("fn: %s" % fn)
        eval(fn)
    else:
        t2()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
