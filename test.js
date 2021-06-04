var tie = require('./buffer-tie');

console.log('creating memfd-backed array buffer, 64kB');
var src = tie.openMemFDBuffer('test', 1 << 16)

console.log('creating wasm memory, 64 kB');
var m = new WebAssembly.Memory({initial:1, maximum:1});
var b = m.buffer;

console.log('tying bytes [4096,8192) from memfd to wasm memory [8192,12288)');
var untie = src.tie(m.buffer,// dest
                    8192,  // dest offset
                    4096,  // size
                    4096); // source offset
src.close();

function assertArrayBufferUint8RangeIs(buf, start, end, value) {
    let array = new Uint8Array(buf);
    for (let i = start; i < end; i++) {
        if (array[i] !== value) {
            throw new Error(`expected array[${i}] == ${value}, got ${array[i]}`)
        }
    }
}

console.log('bytes [4096+100,4096+180) of memfd-backed array',
            Array.from(new Uint8Array(src, 4096+100, 80)).join(','));
console.log('bytes [8192+100,8192+180) of wasm memory',
            Array.from(new Uint8Array(m.buffer, 8192+100, 80)).join(','));

assertArrayBufferUint8RangeIs(src, 0, 1<<16, 0);
assertArrayBufferUint8RangeIs(m.buffer, 0, 1<<16, 0);

console.log('populating [0,100) in memfd-backed array with 42')
new Uint8Array(src, 0,100).fill(42);
console.log('populating [4096+100,4096+140) in memfd-backed array with 1')
new Uint8Array(src, 4096+100, 40).fill(1);
console.log('populating [2*16-100,100) in wasm memory with 42')
new Uint8Array(m.buffer, (1<<16)-100, 100).fill(42);

console.log('bytes [4096+100,4096+180) of memfd-backed array',
            Array.from(new Uint8Array(src, 4096+100, 80)).join(','));
console.log('bytes [8192+100,8192+180) of wasm memory',
            Array.from(new Uint8Array(m.buffer, 8192+100, 80)).join(','));

assertArrayBufferUint8RangeIs(src, 0, 100, 42);
assertArrayBufferUint8RangeIs(src, 100, 4096+100, 0);
assertArrayBufferUint8RangeIs(src, 4096+100, 4096+140, 1);
assertArrayBufferUint8RangeIs(src, 4096+140, 1<<16, 0);

assertArrayBufferUint8RangeIs(m.buffer, 0, 8192+100, 0);
assertArrayBufferUint8RangeIs(m.buffer, 8192+100, 8192+140, 1);
assertArrayBufferUint8RangeIs(m.buffer, 8192+140, (1<<16)-100, 0);
assertArrayBufferUint8RangeIs(m.buffer, (1<<16)-100, 100, 42);

console.log('populating [8192+140,8192+180) in wasm memory with 2')
new Uint8Array(m.buffer, 8192+140, 40).fill(2);

console.log('bytes [4096+100,4096+180) of memfd-backed array',
            Array.from(new Uint8Array(src, 4096+100, 80)).join(','));
console.log('bytes [8192+100,8192+180) of wasm memory',
            Array.from(new Uint8Array(m.buffer, 8192+100, 80)).join(','));

assertArrayBufferUint8RangeIs(src, 0, 100, 42);
assertArrayBufferUint8RangeIs(src, 100, 4096+100, 0);
assertArrayBufferUint8RangeIs(src, 4096+100, 4096+140, 1);
assertArrayBufferUint8RangeIs(src, 4096+140, 4096+180, 2);
assertArrayBufferUint8RangeIs(src, 4096+180, 1<<16, 0);

assertArrayBufferUint8RangeIs(m.buffer, 0, 8192+100, 0);
assertArrayBufferUint8RangeIs(m.buffer, 8192+100, 8192+140, 1);
assertArrayBufferUint8RangeIs(m.buffer, 8192+140, 8192+180, 2);
assertArrayBufferUint8RangeIs(m.buffer, 8192+180, (1<<16)-100, 0);
assertArrayBufferUint8RangeIs(m.buffer, (1<<16)-100, 100, 42);

console.log('untying buffer')
untie();

assertArrayBufferUint8RangeIs(src, 0, 100, 42);
assertArrayBufferUint8RangeIs(src, 100, 4096+100, 0);
assertArrayBufferUint8RangeIs(src, 4096+100, 4096+140, 1);
assertArrayBufferUint8RangeIs(src, 4096+140, 4096+180, 2);
assertArrayBufferUint8RangeIs(src, 4096+180, 1<<16, 0);

assertArrayBufferUint8RangeIs(m.buffer, 0, (1<<16)-100, 0);
assertArrayBufferUint8RangeIs(m.buffer, (1<<16)-100, 100, 42);

console.log('success')
