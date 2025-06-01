# ws_parser

ws_parser is a streaming parser for the WebSocket protocol [RFC 6455](https://tools.ietf.org/html/rfc6455).

This library is loosely inspired by [joyent/http_parser](https://github.com/joyent/http-parser)
and shares many of the same attributes: it has no dependencies, makes no
allocations or syscalls, and only requires 16 bytes of memory to maintain
its parse state.

## lua-wscedec

wscedec is a Lua module that provides a WebSocket decoder based on ws_parser,
a WebSocket encoder based on the [RFC 6455](https://tools.ietf.org/html/rfc6455),
but without WebSocket handshake support.

### document

see [manual](doc/manual.html)

### Usage

```lua

local codec = require("ws.codec")
local msg = "Hello, world!"

-- returns a WebSocket frame with the payload msg
local data = codec.encode(msg, 'text', mask)

-- returns a parsed WebSocket frame or failure
local parsed, off, code = codec.decode(data)
```
