local openssl = require("openssl")
local codec = require("ws.codec")

local data = "hello world"
local mask = string.char(0xde, 0xad, 0xbe, 0xef)

local encoded = codec.encode(data, "text", mask)
local decoded = codec.decode(encoded)
assert(type(decoded) == "table")
assert(decoded.text == data)

encoded = codec.encode(data, "binary", 0xdeadbeaf)
decoded = codec.decode(encoded)
assert(type(decoded) == "table")
assert(decoded.binary == data)

data = openssl.random(65536)
encoded = codec.encode(data, "binary")
decoded = codec.decode(encoded)
assert(type(decoded) == "table")
assert(decoded.binary == data)
decoded.binary = nil
