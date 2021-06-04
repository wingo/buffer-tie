// buffer-tie.cc
#include <cstdint>
#include <optional>

#include <linux/memfd.h>
#include <sys/mman.h>
#include <unistd.h>

#include <node.h>
#include <node_buffer.h>

namespace buffer_tie {

using v8::ArrayBuffer;
using v8::BigInt;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

static void
ThrowTypeError(Isolate *isolate, const char *msg)
{
  MaybeLocal<String> str(String::NewFromUtf8(isolate, msg));
  isolate->ThrowException(Exception::TypeError(str.ToLocalChecked()));
}

static std::optional<int32_t>
ParseInt32(Isolate *isolate, Local<Value> v) 
{
  MaybeLocal<Int32> addrv = v->ToInt32(isolate->GetCurrentContext());
  if (addrv.IsEmpty()) {
    ThrowTypeError(isolate, "Expected an int32");
    return std::nullopt;
  }
  return addrv.ToLocalChecked()->Value();
}

static std::optional<uintptr_t>
ParseAddress(Isolate *isolate, Local<Value> v) 
{
  if (!v->IsNumber()) {
    MaybeLocal<BigInt> addrv = v->ToBigInt(isolate->GetCurrentContext());
    if (!addrv.IsEmpty()) {
      bool lossless = false;
      uint64_t addr = addrv.ToLocalChecked()->Uint64Value(&lossless);
      if (lossless) {
        uintptr_t ptr = static_cast<uintptr_t>(addr);
        if (addr == static_cast<uint64_t>(ptr))
          return ptr;
      }
      ThrowTypeError(isolate, "Out of range");
      return std::nullopt;
    }
  }

  {
    MaybeLocal<Uint32> addrv = v->ToUint32(isolate->GetCurrentContext());
    if (!addrv.IsEmpty())
      return static_cast<uintptr_t>(addrv.ToLocalChecked()->Value());
  }

  ThrowTypeError(isolate, "Expected an address");
  return std::nullopt;
}

static std::optional<void*>
ParsePointer(Isolate *isolate, Local<Value> v) 
{
  std::optional<uintptr_t> res = ParseAddress(isolate, v);
  if (res)
    return reinterpret_cast<void*>(*res);
  return std::nullopt;
}

static std::optional<size_t>
ParseSize(Isolate *isolate, Local<Value> v) 
{
  std::optional<uintptr_t> res = ParseAddress(isolate, v);
  if (res)
    return static_cast<size_t>(*res);
  return std::nullopt;
}

static Local<Value>
AddressToValue(Isolate *isolate, uintptr_t addr)
{
  return BigInt::NewFromUnsigned(isolate, addr);
}
  
static void OpenMemFD(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  
  if (args.Length() != 2)
    return ThrowTypeError(isolate, "Wrong number of arguments");

  String::Utf8Value name(isolate, args[0]);
  auto flags = ParseInt32(isolate, args[1]);
  if (!flags) return;

  int res = memfd_create(*name, *flags);
  if (res == -1) {
    isolate->ThrowException(node::ErrnoException(isolate, errno,
                                                 "memfd_create"));
    return;
  }

  args.GetReturnValue().Set(res);
}

static void Alias(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() != 2)
    return ThrowTypeError(isolate, "Wrong number of arguments");

  auto addr = ParsePointer(isolate, args[0]);
  if (!addr) return;
  auto length = ParseSize(isolate, args[1]);
  if (!length) return;

  Local<ArrayBuffer> res = ArrayBuffer::New(isolate, *addr, *length);

  args.GetReturnValue().Set(res);
}
  
static void Map(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  
  if (args.Length() != 6)
    return ThrowTypeError(isolate, "Wrong number of arguments");

  auto addr = ParsePointer(isolate, args[0]);
  if (!addr) return;
  auto length = ParseSize(isolate, args[1]);
  if (!length) return;
  auto prot = ParseInt32(isolate, args[2]);
  if (!prot) return;
  auto flags = ParseInt32(isolate, args[3]);
  if (!flags) return;
  auto fd = ParseInt32(isolate, args[4]);
  if (!fd) return;
  auto offset = ParseSize(isolate, args[5]);
  if (!offset) return;

  void* res = mmap(*addr, *length, *prot, *flags, *fd, *offset);
  if (res == MAP_FAILED) {
    isolate->ThrowException(node::ErrnoException(isolate, errno, "mmap"));
    return;
  }
  uintptr_t res_addr = reinterpret_cast<uintptr_t>(res);

  args.GetReturnValue().Set(AddressToValue(isolate, res_addr));
}

static void Unmap(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  
  if (args.Length() != 2)
    return ThrowTypeError(isolate, "Wrong number of arguments");

  auto addr = ParsePointer(isolate, args[0]);
  if (!addr) return;
  auto length = ParseSize(isolate, args[1]);
  if (!length) return;

  int res = munmap(*addr, *length);
  if (res != 0) {
    isolate->ThrowException(node::ErrnoException(isolate, errno, "munmap"));
    return;
  }

  args.GetReturnValue().SetUndefined();
}

static void BufferData(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  
  if (args.Length() != 1)
    return ThrowTypeError(isolate, "Wrong number of arguments");

  if (!args[0]->IsArrayBuffer())
    return ThrowTypeError(isolate, "Expected an ArrayBuffer");

  void *addr = args[0].As<ArrayBuffer>()->GetContents().Data();
  args.GetReturnValue().Set(
      AddressToValue(isolate, reinterpret_cast<uintptr_t>(addr)));
}

static void Initialize(Local<Object> exports, Local<Value> module, void* priv) {
  NODE_DEFINE_CONSTANT(exports, MFD_CLOEXEC);
  NODE_DEFINE_CONSTANT(exports, MFD_ALLOW_SEALING);
  NODE_DEFINE_CONSTANT(exports, MFD_HUGETLB);
  NODE_DEFINE_CONSTANT(exports, MFD_HUGE_2MB);
  NODE_DEFINE_CONSTANT(exports, MFD_HUGE_1GB);

  NODE_DEFINE_CONSTANT(exports, PROT_READ);
  NODE_DEFINE_CONSTANT(exports, PROT_WRITE);
  NODE_DEFINE_CONSTANT(exports, PROT_EXEC);
  NODE_DEFINE_CONSTANT(exports, PROT_NONE);

  NODE_DEFINE_CONSTANT(exports, MAP_SHARED);
  NODE_DEFINE_CONSTANT(exports, MAP_PRIVATE);
  NODE_DEFINE_CONSTANT(exports, MAP_FIXED);
  NODE_DEFINE_CONSTANT(exports, MAP_ANONYMOUS);

  NODE_SET_METHOD(exports, "openMemFD", OpenMemFD);
  NODE_SET_METHOD(exports, "alias", Alias);
  NODE_SET_METHOD(exports, "map", Map);
  NODE_SET_METHOD(exports, "unmap", Unmap);
  NODE_SET_METHOD(exports, "bufferData", BufferData);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

} // namespace buffer_tie
