local openssl = require("openssl")
local codec = require("ws.codec")

local data = "hello world"
local mask = string.char(0xde, 0xad, 0xbe, 0xef)

local encoded = codec.encode(data, "text", mask)
local decoded, off = codec.decode(encoded)
assert(type(decoded) == "table")
assert(decoded.text == data)
assert(off == #encoded + 1)

encoded = codec.encode(data, "binary", 0xdeadbeaf)
decoded, off = codec.decode(encoded)
assert(type(decoded) == "table")
assert(decoded.binary == data)
assert(off == #encoded + 1)

data = openssl.random(65536)
encoded = codec.encode(data, "binary")
decoded, off = codec.decode(encoded)
assert(type(decoded) == "table")
assert(decoded.binary == data)
decoded.binary = nil
assert(off == #encoded + 1)

local part = encoded:sub(1, 32768)
decoded = codec.decode(part)
assert(type(decoded) == "table")
assert(decoded.remaining == #encoded - #part)
assert(off == #encoded + 1)
