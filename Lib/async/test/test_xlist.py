import unittest
import _async
import async
import time

#class TestXlistBasic(unittest.TestCase):
#
#    def test_basic_len(self):
#        src = 1
#        xl = async.xlist()
#        xl.push(src)
#        self.assertEqual(len(xl), 1)
#
#    def test_basic_pop(self):
#        src = 1
#        xl = async.xlist()
#        xl.push(src)
#        dst = xl.pop()
#        self.assertEqual(src, dst)
#        self.assertEqual(len(xl), 0)

class TestXlistParallel(unittest.TestCase):

    def test_basic_len(self):
        before = async.rdtsc()
        xl = async.xlist()
        def w():
            xl.push(async.rdtsc())
        async.submit_work(w)
        async.run()
        after = async.rdtsc()
        during = xl.pop()
        self.assertLess(before, during)
        self.assertGreater(after, during)

def main():
    unittest.main()

if __name__ == '__main__':
    main()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
