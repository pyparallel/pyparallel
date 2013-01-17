import unittest
import async

class TestProtect(unittest.TestCase):

    def test_protect_basic(self):
        o = object()
        async.protect(o)
        self.assertEqual(async.protected(o), True)
        async.unprotect(o)
        self.assertEqual(async.protected(o), False)

if __name__ == '__main__':
    unittest.main()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
