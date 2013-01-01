
import _async

def run_old():
    i = 0
    count = 0
    ctx_count = 0
    is_active = 0
    while True:
        count = _async.active_count()
        ctx_count = _async.active_contexts()
        print("active count: %d" % count)
        print("active contexts: %d" % ctx_count)
        print("is_active: %s" % str(bool(_async.is_active())))
        print("is_active_ex: %s" % str(bool(_async.is_active_ex())))
        if not count:
            break
        i += 1
        print("run count: %d" % i)
        errors = _async.run_once()
        if (errors):
            for error in errors:
                print(error)

def run_slim():
    i = 0
    while _async.is_active():
        i += 1
        print("run count: %d" % i)
        errors = _async.run_once()
        if (errors):
            for error in errors:
                print(error)

run = run_old

# vim:set ts=8 sw=4 sts=4 tw=78 et:
