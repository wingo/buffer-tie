# Zero-copy marshalling of data into WebAssembly

This module uses dirty tricks to remap external data into WebAssembly
modules without copying.

```js
let tie = require('buffer-tie')
let src = tie.openMemFDBuffer('my data', 1<<20);
// A Memory with just one 64kB page.
let dst = new WebAssembly.Memory({initial:1,maximum:1})

// Zero-copy alias of the first 32 kB of src into the last 32 kB of dst.
src.tie(dst, 32*1024, 32*1024);
```

## Motivation

Sometimes when you embed WebAssembly in a C++ environment, there can be
a large amount of external data that you don't want to copy into the
WebAssembly module.

With simple JavaScript we don't have this a problem currently because
web engines can allow their ArrayBuffer instances to use memory
allocated external to the JS engine.  But for an already-instantiated
WebAssembly module, all the data has to be in the WebAssembly module's
linear memory.

## Horrible solution

On Linux, one page in physical memory can have many addresses in virtual
memory.  This is a feature of virtual memory in hardware.  Unfortunately
the full capabilities of the virtual memory system aren't exported to
userspace by Linux; you need a file descriptor for the source memory
before you can alias it.  So in this module we create array buffers that
are backed by memfd, and actually we never unmap the corresponding
memory.  In some production system you would do this differently :)

Then to actually alias the memory into the target, we stomple on V8's
memory by remapping our data into the WebAssembly memory.  Because we
know that WebAssembly memories are allocated on page-aligned boundaries,
a page-aligned offset inside the array buffer will be page-aligned when
treated as an absolute address too.  You can choose the offset from the
"outside", or the WebAssembly program could allocate page-aligned memory
using `posix_memalign`, and then call out to an import to stomple it.

## `MREMAP_DONTUNMAP`?

There is another possible implementation, to use
[`MREMAP_DONTUNMAP`](https://github.com/torvalds/linux/commit/e346b3813067d4b17383f975f197a9aa28a3b077)
for anonymous memory, but in that case it requires you to know that the
memory is anonymous, and apparently mremap is quite slow.
