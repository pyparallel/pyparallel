import sys
import unittest
from test import support

parallel = support.import_module('parallel')

class TestBasic(unittest.TestCase):
    def test_1arg_4data(self):
        data = [ i for i in range(1, 5) ]
        expected = [ i*i for i in data ]

        def f(x):
            z = x * x
            l = list(z)
            d = dict(x=z, y=z)
            s = str(d)
            return z

        result = parallel.map(f, data)
        self.assertEqual(result, expected)

    def _disabled_test_two_args(self):
        numbers = [ i for i in range(1, 5) ]
        data = [ (x, y) for (x, y) in zip(numbers, reversed(numbers)) ]
        def f(x, y):
            z = x * y
            l = list(z)
            d = dict(x=z, y=z)
            s = str(d)
            return z

        result = parallel.map(f, data)
        expected = [ x*y for (x, y) in data ]
        self.assertEqual(result, expected)

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

def test_main():
    mod = sys.modules[__name__]
    support.run_unittest(
        *[getattr(mod, name) for name in dir(mod) if name.startswith('Test')]
    )

if __name__ == '__main__':
    simple_test()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
