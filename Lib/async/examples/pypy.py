import async
def work():
    for j in range(10):
        for k in range(10**6):
            pass

for i in range(10):
    async.submit_work(work)

async.run()
