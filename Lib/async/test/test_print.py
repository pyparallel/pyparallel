import async

def cb():
    async.stdout("timecounter: %d\n" % async.rdtsc())

async.submit_work(cb)
async.run()
