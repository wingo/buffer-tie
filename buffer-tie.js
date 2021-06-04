var mmap = require('bindings')('mmap.node')
var fs = require('fs')

function untie(dst, size) {
    let fd = -1;
    let prot = mmap.PROT_READ|mmap.PROT_WRITE;
    let flags = mmap.MAP_FIXED|mmap.MAP_PRIVATE|mmap.MAP_ANONYMOUS;
    let offset = 0;
    mmap.map(dst, size, prot, flags, fd, offset);
}

function tie(dst, size, fd, offset) {
    let prot = mmap.PROT_READ|mmap.PROT_WRITE;
    let flags = mmap.MAP_FIXED|mmap.MAP_SHARED;
    mmap.map(dst, size, prot, flags, fd, offset);
    return () => untie(dst, size);
}

function openMemFDBuffer(name, size, flags = mmap.MFD_CLOEXEC) {
    let fd = mmap.openMemFD(name, flags);
    fs.ftruncateSync(fd, size);
    let addr = 0;
    let prot = mmap.PROT_READ|mmap.PROT_WRITE;
    let mflags = mmap.MAP_SHARED;
    let offset = 0;
    addr = mmap.map(addr, size, prot, mflags, fd, offset);
    let buf = mmap.alias(addr, size);
    let open = true;
    buf.tie = (dst, doffset=0, dsize=Math.min(size, dst.byteLength), soffset=0) => {
        if (!open) throw new Error('fd already closed');
        let daddr = mmap.bufferData(dst)
        if (BigInt(doffset) + BigInt(dsize) > BigInt(dst.byteLength))
            throw new Error("destination size out of bounds");
        if (BigInt(soffset) + BigInt(dsize) > BigInt(size))
            throw new Error("source size out of bounds");
        return tie(BigInt(daddr) + BigInt(doffset), dsize, fd, BigInt(soffset));
    }
    buf.close = () => {
        if (open) {
            open = false;
            fs.closeSync(fd);
        }
    }
    return buf;
}

exports = module.exports = {openMemFDBuffer}
