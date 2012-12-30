
import _async

def run():
    i = 0
    while _async.active():
        i += 1
        print("run count: %d" % i)
        errors = _async.run_once()
        if (errors):
            for error in errors:
                raise error

# vim:set ts=8 sw=4 sts=4 tw=78 et:
