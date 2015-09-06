# Introduction
--

[PyParallel][] is an experimental, proof-of-concept fork of Python 3.3.5 designed to optimally exploit contemporary hardware: multiple CPU cores, fast SSDs, NUMA architectures, and fast I/O channels (10GbE, Thunderbolt, etc).  It presents a solution for removing the *limitation* of the Python Global Interpreter Lock (GIL) without needing to actually remove it at all.

The code changes  required to the interpreter are relatively unobtrusive, all existing semantics such as reference counting and garbage collection remain unchanged, the new mental model required in order to write *PyParallel-safe* code is very simple (*don't persist parallel objects*), the single-thread overhead is negligible, and, most desirably, [performance scales linearly with cores](http://pyparallel.org/#performance).

[PyParallel]: http://pyparallel.org

# Disclaimer
--
PyParallel is, first and foremost, an experiment.  It is not currently suitable for production.  (*If you're interested in commercial or production support, [contact us](mailto:sales@continuum.io)*.)  It is a product of trial-and-error, intended to shape the discussions surrounding the next generation of Python.  We attempt to juggle the difficult task of setting the stage for Python to flourish over the next 25 years, without discarding all the progress we made in the last 25.

PyParallel was created by an existing Python committer with the intention of eventually merging it back into the mainline.  It is not a hostile fork.  There are many details that still need to be ironed out.  It will need to prove itself as an independent project first before it could be considered for inclusion back in the main source tree.  We anticipate this being at least 5 years out, and think Python 4.x would be a more realistic target than Python 3.x.

5 years sounds like a long time, however, it will come and go, just like any other.  We may as well start the ball rolling now.  There's nothing wrong with slow and steady as long as you're heading in the right direction.  And it's not like we're getting any less cores per year.

Expectations need to be set reasonably, and we encourage the Python community toward biasing yes versus biasing no, with a view toward the long term benefits of such a project.  Early adopters and technical evaluators will need to have thick skin, a pioneering spirit and a hearty sense of adventure.  You will definitely hit a `__debugbreak()` or two if you're doing things right.  But hey, you'll be able to melt all your cores during the process, and that's kinda' fun.

We encourage existing committers to play around and experiment, to fork and to send pull requests.  One of the benefits of PyParallel at the moment is the freedom to experiment without the constraints that come with normal mainline development, where much more discipline is required per commit.  It provides a nice change of pace and helps get the creative juices flowing.  There is also a lot of low-hanging fruit, ripe for picking by Computer Science and Software Engineering students that want to get their feet wet with open source.

# Catalyst
--

PyParallel and `asyncio` share the same origin.  They were both products of an innocuous e-mail to `python-ideas` in September 2012 titled [asyncore: included batteries don't fit](https://mail.python.org/pipermail/python-ideas/2012-October/016311.html).  The general discussion centered around providing better asynchronous I/O primitives in Python 3.4.  PyParallel took the wildly ambitious (and at the time, somewhat ridiculous) path of trying to solve both asynchronous I/O and the parallel problem at the same time.  The efforts paid off, as we consider the whole experiment to be a success (at least in terms of its original goals), but it is a much longer term project, alluded to above.  There is some inevitable overlap between the `asyncio` interface, and the `async` interface we exposed for PyParallel.  Additionally, `async` became a keyword in Python 3.5, so we will need to change the module name when we rebase against 3.5.  The most likely name will be `parallel`.  At the time of writing (August 2015), all examples and public documentation to date, including examples in this document, use the `async` module name.

> Note: the parallel facilities provided by PyParallel are actually complementary to the single-threaded event loop facilities provided by `asyncio`.  In fact, we envision hybrid solutions emerging that use `asyncio` to drive the parallel facilities behind the scenes, where the main thread dispatches requests to parallel servers behind the scenes, acting as the coordinator for parallel computation.

# Overview
--

We expose a new `async` module to Python user code which must be used in order to leverage the new parallel execution facilities.  Specifically, users implement completion-oriented protocol classes, then *register* them with PyParallel TCP/IP *client* or *server* objects.

```python
import async
class Hello:
    def connection_made(self, transport, data):
        return b'Hello, World!\r\n'
    def data_received(self, transport, data):
        return b'You said: ' + data + '\r\n'

server = async.server('0.0.0.0', 8080)
async.register(transport=server, protocol=Hello)
async.run()
```

The protocol callbacks are automatically executed in parallel.  This is achieved by creating parallel contexts for each client socket that connects to a server.  The parallel context owns the underlying socket object, and all memory allocations required during callback execution are served from the context's heap, which is a simple block allocator.  If callbacks need to send data back to the client, they must return a sendable object: `bytes`, `bytearray` or `string`.  (That is, they do not explicitly call `read()` and `write()` methods against the transport directly.)  If the contents of a file needs to be returned, `transport.sendfile()` can be used.  Byte ranges can also be efficiently returned via `transport.ranged_sendfile()`.  Both of these methods serve file content directly from the kernel's cache via `TransmitFile`.

### GIL Semantics Unchanged

The semantic behavior of the "main thread" (the current thread holding the GIL) is unchanged.  Instead, we introduce the notion of a parallel thread (or parallel context), and parallel objects, which are `PyObjects` allocated from parallel contexts.  We provide an efficient way to detect if we're in a parallel thread via the `Py_PXCTX()` macro, as well as a way to detect if a `PyObject` was allocated from a parallel thread via `Py_ISPX(ob)`.  Using only these two facilities, we are able to intercept all thread-sensitive parts of the interpreter and redirect to our new parallel alternative if necessary.  (The GIL entry and exit macros, `Py_BEGIN_ALLOW_THREADS` and `Py_END_ALLOW_THREADS` respectively, also get ignored in parallel contexts.)

### The One New Restriction: Don't Persist Parallel Objects

We introduce one new restriction that will affect existing Python code (and C extensions): *don't persist parallel objects*.  More explicitly, don't cache objects that were created during parallel callback processing.

For the CPython interpreter internals (in C), this means avoiding the following: freelist manipulation, first-use static PyObject initialization, and unicode interning.  On the Python side, this means avoiding mutation of global state (or, more specifically, avoiding mutation of Python objects that were allocated from the main thread; don't append to a main thread list or assign to a main thread dict from a parallel thread).

### Reference Counting and Garbage Collection in Parallel Contexts

We approached the problem of referencing counting and garbage collection within parallel contexts using a rigorous engineering methodology that can be summed up as follows: *let's not do it, and see what happens*.  Nothing happened, so we don't do it.

Instead, we manage object lifetime and memory allocation in parallel contexts by exploiting the temporal and predictable nature of the protocol callbacks, which map closely to TCP/IP states (`connection_made()`, `data_received()`, `send_complete()`, etc).

A snapshot is taken prior to invoking the callback and then rolled back upon completion.  Object lifetime is therefore governed by the duration of the callback; all objects allocated during the processing of a HTTP request, for example, including the final bytes object we send as a response, will immediately cease to exist the moment our TCP/IP stack informs us the send completed.  (Specifically, we perform the rollback activity upon receipt of the completion notification.)

This is effective for the same reason generational garbage collectors are effective: *most objects are short lived*.  For stateless, idempotent protocols, like HTTP, *all objects are short lived*.  For stateful protocols, scalar objects (ints, floats, strings, bytes) can be assigned to `self` (a special per-connection instance of the protocol class), which will trigger a copy of the object from an alternate heap (still associated with the parallel context).  ([This is described in more detail here.](https://mail.python.org/pipermail/python-ideas/2015-June/034342.html))  The lifetime of these objects will last as long as the TCP/IP connection persists, or until a previous value is overwritten by a new value, which ever comes first.

Thus, PyParallel requires no changes to existing reference counting and garbage collection semantics or APIs.  `Py_INCREF(op)` and `Py_DECREF(op)` get ignored in parallel contexts, and GC-specific calls like `PyObject_GC_New()` simply get re-routed to our custom parallel object allocator in the same fashion as `PyObject_New()`.  This obviates the need for fine-grain, per-object locking, as well as the need for a thread-safe, concurrent garbage collector.

This is significant when you factor in how Python's scoping works at a language level: Python code executing in a parallel thread can freely access any non-local variables created by the "main thread".  That is, it has the exact same scoping and variable name resolution rules as any other Python code.  This facilitates loading large data structures from the main thread and then freely accessing them from parallel callbacks.

We demonstrate this with our simple [Wikipedia "instant search" server](https://github.com/pyparallel/pyparallel/blob/branches/3.3-px/examples/wiki/wiki.py#L294), which loads a trie with 27 million entries, each one mapping a title to a 64-bit byte offset within a 60GB XML file.  We then load a sorted NumPy array of all 64-bit offsets, which allows us to extract the exact byte range a given title's content appears within the XML file, allowing a client to issue a ranged request for those bytes to get the exact content via a single call to `TransmitFile`.  This call returns immediately, but sets up the necessary structures for the kernel to send that byte range directly to the client without further interaction from us.

The working set size of the `python.exe` process is about 11GB when the trie and NumPy array are loaded.  Thus, `multiprocessing` would not be feasible, as you'd have 8 separate processes of 11GB if you had 8 cores and started 8 workers, requiring 88GB just for the processes.  The number of allocated objects is around 27.1 million; the `datrie` library can efficiently store values if they're a 32-bit integer, however, our offsets are 64-bit, so an 80-something byte `PyObject` needs to be allocated to represent each one.

This is significant because it demonstrates the competitive advantage PyParallel has against other languages when dealing with large heap sizes and object counts, whilst simultaneously avoiding the need for continual GC-motivated heap traversal, a product of memory allocation pressure (which is an inevitable side-effect of high-end network load, where incoming links are saturated at line rate).  We ellaborate on this shortly.

For a more contained example, we present the following.  We load an array with a billion elements, consuming about 9GB of memory, then expose a HTTP server that services client requests in parallel, randomly picking a slice of 10 elements within the array and summing it.  The principles are the same as the Wikipedia instant search server (random access to huge data structures in parallel, where request latency and throughput scale linearly with cores and concurrency up to ncpu). 

```python
import numpy as np
import async

one_billion = 1000000000
large_array = np.random.randints(low=0, high=100, size=one_billion)
fmt = 'The sum of the items in slice [%d:%d] is %d'

class Random:
    http11 = True
    # ^^^^ This class attribute tells PyParallel to use an SSE-accelerated
    # HTTP 1.1 parser for this protocol, automatically translating a GET
    # request for the URL '/random' to a direct call to `random()` (meaning
    # we don't have to implement `data_received()` and do HTTP parsing ourself).
    # (HTTP headers are accessible at this point via `transport.http_headers`,
    # which is a dict.)
    def random(self, transport, data):
        # This code executes within the new parallel execution environment.
        # We can access anything we'd normally be able to access according
        # to Python's scope rules.  Any new objects we create herein are
        # "parallel objects" and will only exist for the duration of the
        # callback.  (Although we could assign a scalar to self here,
        # provided we define it as a class variable above.  This would
        # allow persistence of state between callbacks, and could either
        # be used to track a higher level protocol state (like POP3), or
        # to track the results of intermediate computation.  Keep in mind
        # persisting to a scalar isn't so bad from an overhead perspective
        # when you've got other cores servicing other requests simultaneously;
        # Pickling and unpickling a complex structure (list, dict) to a string
        # each request is fast and simple, and allows us to avoid a whole
        # host of memory/object/pointer ownership issues we'd have to deal with
        # if we were persisting container objects (like lists and dicts).)
        start = np.random.randint(low=0, high=one_billion-11)
        end = start + 10
        return fmt % (start, end, large_array[start:end].sum())

server = async.server('0.0.0.0', 8080)
async.register(transport=server, protocol=Random)
print("Random server now accessible at http://localhost:8080/random")
print("Press Ctrl-C to quit.")
async.run()
```

This split-brain *main-thread versus parallel thread* approach to object allocation and ownership is a unique breakthrough.  By separating the two concepts, we get the best of both worlds: reference counting and garbage collection at the global, "main thread" level, where object lifetime cannot be implicitly known any other way, *and* very fast GC-less allocation at the parallel level, where we can rely on the temporal nature of our protocol semantics to manage object lifetime.  _The incumbent "main thread" behavior doesn't need to know anything about the latter parallel behavior, and the parallel environment knows how to avoid disturbing the former._

This gives PyParallel a unique advantage over other garbage collected languages like C#, Java and Go, despite those languages being much faster in general due to being compiled versus interpreted.  The absense of GC within parallel contexts means there are no GC pauses, which results in PyParallel having [request latency cumulative frequency distributions](http://pyparallel.org/#performance) on par with GC-less languages like C, C++, and Rust (in terms of the distribution, not the actual latency numbers; C/C++ will always be faster than interpreted Python, of course).

Furthermore, our experimentation shows that the new solution plays nicely with tools such as Cython (provided the Cython code uses normal Python memory allocation facilities and doesn't try to persist objects generated from parallel contexts).  In fact, our Wikipedia instant search trie is powered by a Cython project named `datrie`, which is a wrapper around the C library `libdatrie`.

This is significant as it allows us to continue to leverage the stellar work done by the Python community for accelerating Python (or Python-esque) code, whilst simultaneously leveraging the benefits afforded by PyParallel.  Entire request callbacks could be implemented in Cython, providing C-like performance to those domains that rely upon it.  We anticipate similar compatibility with other optimizers like Numba.  Converting Python code into performant, potentially-JIT-accelerated machine code will be a constantly evolving domain.  PyParallel is complimentary to these techniques, providing the glue behind the scenes to minimize the latency between I/O and subsequent computation across multiple cores.  We defer to the other projects to provide means for accelerating the computation from within the callback.  It is a symbiotic relationship, not a competitive one.

Additionally, because code executed within parallel contexts is normal Python code, we can run it in a single-threaded fashion from the main thread during development, which allows the normal debugging and unit testing facilities needed at such a stage.  (We also implement experimental support for a parallel thread to acquire the GIL and *become* the main thread in order to further assist debugging efforts.)


We attribute the success of the PyParallel experiment to five key things:

 - *Being relatively unfamiliar with the intracacies of Python's C implementation.*  This was advantageous, despite how counter-intuitive it seems.  Not knowing what you don't know is a great way to lower the cost of experimentation.
 - *Attacking parallelization __and__ asynchronous I/O at the same time.*  The two are intrinsically linked.  The single-threaded, non-blocking event loop with a system multiplex call (kqueue/epoll) is simply not suitable for exploiting today's multicore 10GbE+ environments.  If your I/O strategy isn't intrinsically linked to your parallel computation strategy, you're not going to be able to optimally use your underlying hardware.  Minimizing the latency between completion of I/O and subsequent scheduling of computation is critical.
 - *The willingness of the CPython interpreter internals to be completely subverted by our parallel shenanigans without requiring any breaking API changes or complex locking semantics.*  This was a delightful surprise, and all credit goes to Guido for developing the primitives the way he did 25-odd years ago.
 - *The situational awareness provided by Visual Studio's debugger for facilitating such subversive changes on a code base we were ultimately unfamiliar with.*  Writing code was a miniscule portion of PyParallel's development time -- the vast majority of time was spent within the debugger, stepping through unfamiliar code, figuring out ways to subvert the interpreter (or determing why our attempts at subversion weren't working).  All *Aha!* moments were had either in a) the shower, or b) whilst Visual Studio was attached to `python_d.exe` and we were staring at call stacks, locals, autos, memory values and source code all within the same place, stepping through individual instructions, constantly building up our situational awareness.
 - *Designing around mechanical sympathy at the hardware, kernel, OS userspace, and protocol level.*  The reason Windows was used to develop PyParallel is because it simply has the best kernel and userspace primitives for not only solving the problem, but solving it as optimally as the underlying hardware will allow.  Windows itself exhibits excellent mechanical sympathy between all levels of the stack, which we leverage extensively.  The reception of a TCP/IP packet off the wire, to the I/O request completion by the NIC's device driver, to the threadpool I/O completion, to the dispatching of the most optimal thread within our process, to calling back into the C function we requested, to us immediately invoking the relevant Python object's `data_received()` call; mechanical sympathy is maintained at every layer of the stack, facilitating the lowest possible latency.

# Interpreter Changes
--
> *Note: in order to ease the task of reviewing the changes made to the interpreter, we provide [diffs against the v3.3.5 tag PyParallel was created from](https://github.com/pyparallel/pyparallel/tree/branches/3.3-px/diffs).  We recommend reviewing these diffs after the key concepts in this section are understood in order to get a better sense of the overall interpreter changes.*

Thread-sensitive calls are ubiquitous within the Python interpreter.  Past experiments have shown that even minor changes to the overhead incurred by `Py_INCREF(op)` and `Py_DECREF(op)` impact the interpreter's performance in a non-negligible way.

### Parallel Context Detection
Thus, the first problem we had to solve was coming up with a quick way to detect whether or not we were in a parallel context.  The quicker it can be detected, the lower the overhead, the more viable the approach is likely to be.  The key to being fast is to do less, so we wondered: *what do we reliably have access to at all times that we can use to distinguish our parallel threads from normal, main threads?*  The convergence was quick: thread IDs.

We add the following public members to `pyparallel.h`:

```c
PyAPI_DATA(volatile long) Py_MainThreadId;
PyAPI_FUNC(void) _PyParallel_JustAcquiredGIL();
```

And we alter `take_gil()` in `ceval_gil.h` to call into us as soon as a main thread has acquired the GIL:

```diff
@@ -236,6 +245,9 @@ static void take_gil(PyThreadState *tstate)
     MUTEX_LOCK(switch_mutex);
 #endif
     /* We now hold the GIL */
+#ifdef WITH_PARALLEL
+    _PyParallel_JustAcquiredGIL();
+#endif
     _Py_atomic_store_relaxed(&gil_locked, 1);
     _Py_ANNOTATE_RWLOCK_ACQUIRED(&gil_locked, /*is_write=*/1);
```

The implementation of `_PyParallel_JustAcquiredGIL()` in `pyparallel.c` simply updates `Py_MainThreadId`:

```c
void
_PyParallel_JustAcquiredGIL(void)
{
    Py_MainThreadId = _Py_get_current_thread_id();
}
```

We now have all we need to detect if we're in a parallel context by simply comparing the value of `Py_MainThreadId` to `_Py_get_current_thread_id()`, which becomes the basis for our `Py_PXCTX()` macro:

```c
#define Py_PXCTX() (Py_MainThreadId == _Py_get_current_thread_id())
```

However, we can improve things further.  `_Py_get_current_thread_id()` has a direct analogue on both Windows and POSIX: `GetCurrentThreadId()` and `pthread_self()`, respectively.  But our `Py_PXCTX()` call will be ubiquitous; is there any way we can avoid a function call?  On Windows, we can interrogate the thread environment block (TEB) directly to derive the current thread ID:

```c
#define _Py_get_current_thread_id()  (__readgsdword(0x48))
```

(On POSIX/amd64, we'd use `__read[fg]sbase()` instead.  This guarantees to return a "unique" address for a thread, and we'd simply use this for the identity test instead of the actual thread ID assigned by the operating system.)

Thus, the `Py_PXCTX()` macro essentially expands to a simple equality test between two 32-bit variables.  For example:

```c
if (Py_PXCTX())
    return;
```

Expands to:

```c
if (Py_MainThreadId == __readgsdword(0x48))
    return;
```

This approach has remained unchanged since inception, and has proven to be a reliable and optimal solution for parallel context detection.  It has one limitation, though: it can't be used in parts of the interpreter that operate without holding the GIL.  Luckily, this is mostly limited to code that deals with GIL acquisition, which is not frequently executed relative to the rest of the code base.  In this situation, code can use the more reliable but slightly slower alternative `_PyParallel_GetActiveContext()`, which returns the TLS `ctx` variable we set within `pyparallel.c` as soon as we're invoked by the threadpool upon I/O completion.  This method doesn't suffer the false-positive issue `Py_PXCTX()` is subject to when the GIL isn't held.  The increased overhead comes from the two-level lookup required to resolve the TLS variable:

```
5982:     if (ctx) {
 8B 0D FF 7C 28 00          mov         ecx,dword ptr [_tls_index (1E42FD90h)]  
 BA 80 29 00 00             mov         edx,2980h  
 48 89 83 90 02 00 00       mov         qword ptr [rbx+290h],rax  
 65 48 8B 04 25 58 00 00 00 mov         rax,qword ptr gs:[58h]  
 48 8B 04 C8                mov         rax,qword ptr [rax+rcx*8]  
 48 8B 0C 02                mov         rcx,qword ptr [rdx+rax]  
 48 85 C9                   test        rcx,rcx  
 74 70                      je          new_context+253h (1E1A8123h)
```
This overhead is an acceptable trade-off when we need to resolve our `ctx` variable once within a C function body, but it makes sense to avoid it if we can for hot-path code, like reference counting interception.

### Playing Defense

When you don't know what you don't know: play defense.  Specifically, code defensively.  We implement `Py_GUARD()` and `Px_GUARD()` macros and liberally sprinkle them over the code.

The former says: *crash if I hit this from a parallel thread*, the latter says: *crash if I hit this from the main-thread*.

(*Crash*, in this context, refers to raising a `Py_FatalError()` with file and line information, or, if the environment variable `PYPARALLEL_NO_MINIDUMP` is set to 1, we `__debugbreak()`, allowing us to attach the Visual Studio debugger.)

The primary recipient of `Py_GUARD()` is [`gcmodule.c`](https://github.com/pyparallel/pyparallel/blob/branches/3.3-px/diffs/Modules/gcmodule.c.patch).  If a parallel thread ever manages to make its way into any function in that file, something is seriously awry.  We reward the effort with a crash.

We add additional guards for protecting not only contexts but object and memory allocations as well.  It is valuable during development to be able to test (and assert, if necessary) that a given `PyObject *op` is a main thread object or a parallel thread object:

 - `Py_GUARD_OBJ(o)`: if we're main thread and `o` is a parallel object: *crash*.
 - `Px_GUARD_OBJ(o)`: if we're a parallel thread and `o` is a main thread object: *crash*.
 - `PyPx_GUARD_OBJ(o)`: *context sensitive crash*: if we're a main thread and `o` is a parallel object: *crash*.  If we're a parallel thread and `o` is a main thread object: *also crash*.

Likewise, for a given `void *ptr` memory allocation:

 - `Py_GUARD_MEM(m)`: if we're a main thread and `m` is memory allocated from a parallel context: *crash*.
 - `Px_GUARD_MEM(m)`: if we're a parallel thread and `m` is memory allocated from a main thread: *crash*.
 - `PyPx_GUARD_MEM(m)`: *context sensitive crash*: if we're a main thread and `m` is memory allocated from a parallel context: crash; if we're a parallel thread and `m` is memory is allocated from a main thread: *also crash*.

In general, our rule of thumb is the venerable: *crash early, crash often*.

[The vast majority of interpreter changes we made](https://github.com/pyparallel/pyparallel/tree/branches/3.3-px/diffs) were simply adding in the necessary guards to ensure we'd crash if any of our assumptions about invariants were incorrect.

