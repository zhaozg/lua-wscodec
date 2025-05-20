#include <errno.h>
#include <lua.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <assert.h>
#include <string.h>

#include <lauxlib.h>
#include <lualib.h>

#include "ws_parser.h"

#if LUA_VERSION_NUM == 501
#define lua_rawlen lua_objlen
#endif

typedef struct {
  lua_State *L;
  int idx;
} ws_arg;

static int on_data_begin(void *user_data, ws_frame_type_t frame_type) {
  ws_arg *arg = user_data;
  lua_State *L = arg->L;

  arg->idx--;
  if (frame_type == WS_FRAME_TEXT)
    lua_pushliteral(L, "text");
  else if (frame_type == WS_FRAME_BINARY)
    lua_pushliteral(L, "binary");
  else
    lua_pushinteger(L, frame_type);

  return WS_OK;
}

static int on_data_payload(void *user_data, const char *buff, size_t len) {
  ws_arg *arg = user_data;
  lua_State *L = arg->L;

  arg->idx--;
  lua_pushlstring(L, buff, len);
  return WS_OK;
}

static int on_control_begin(void *user_data, ws_frame_type_t frame_type) {
  ws_arg *arg = user_data;
  lua_State *L = arg->L;

  arg->idx--;

  switch (frame_type) {
  case WS_FRAME_PING:
    lua_pushliteral(L, "ping");
    break;
  case WS_FRAME_PONG:
    lua_pushliteral(L, "pong");
    break;
  case WS_FRAME_CLOSE:
    lua_pushliteral(L, "close");
    break;
  default:
    lua_pushinteger(L, frame_type);
  }
  lua_pushliteral(L, "control");
  lua_pushvalue(L, -2);
  lua_rawset(L, -4);

  return WS_OK;
}

static int on_control_payload(void *user_data, const char *buff, size_t len) {
  ws_arg *arg = user_data;
  lua_State *L = arg->L;

  arg->idx--;
  lua_pushlstring(L, buff, len);
  return WS_OK;
}

static int on_end(void *user_data) {
  ws_arg *arg = user_data;
  lua_State *L = arg->L;
  if (arg->idx == -2) {
    lua_pushliteral(L, "");
    arg->idx--;
  }
  if (arg->idx == -3) {
    lua_rawset(L, arg->idx);
  }
  arg->idx = -1;
  return WS_OK;
}

static ws_parser_callbacks_t callbacks = {
    .on_data_begin = on_data_begin,
    .on_data_payload = on_data_payload,
    .on_data_end = on_end,
    .on_control_begin = on_control_begin,
    .on_control_payload = on_control_payload,
    .on_control_end = on_end,
};

static int ws_websocket_decode(lua_State *L) {
  int ret;
  ws_arg arg;
  ws_parser_t parser;

  size_t sz;
  const char *input = luaL_checklstring(L, 1, &sz);

  ws_parser_init(&parser);
  arg.L = L;
  arg.idx = -1;

  lua_newtable(L);
  ret = ws_parser_execute(&parser, &callbacks, &arg, (void *)input, sz);
  if (ret != WS_OK ) {
    lua_pushnil(L);
    lua_pushstring(L, ws_parser_error(ret));
    lua_pushinteger(L, ret);
    return 3;
  }
  on_end(&arg);
  assert(arg.idx == -1);

  if (parser.bytes_remaining > 0) {
    lua_pushliteral(L, "remaining");
    lua_pushinteger(L, parser.bytes_remaining);
    lua_rawset(L, -3);
  }

  return 1;
}

static size_t ws_calc_frame_size(ws_frame_type_t flags, size_t data_len) {
  size_t size = data_len + 2; // body + 2 bytes of head
  if (data_len >= 126) {
    if (data_len > 0xFFFF) {
      size += 8;
    } else {
      size += 2;
    }
  }
  if (flags & WS_HAS_MASK) {
    size += 4;
  }

  return size;
}

static uint8_t websocket_decode(char *dst, const char *src, size_t len,
                                const char mask[4], uint8_t mask_offset) {
  size_t i = 0;
  for (; i < len; i++) {
    dst[i] = src[i] ^ mask[(i + mask_offset) % 4];
  }

  return (uint8_t)((i + mask_offset) % 4);
}

static size_t websocket_build_frame(char *frame, ws_frame_type_t flags,
                                    const char mask[4], const char *data,
                                    size_t data_len) {
  size_t body_offset = 0;
  frame[0] = 0;
  frame[1] = 0;
  if (flags & WS_FIN) {
    frame[0] = (char)(1 << 7);
  }
  frame[0] |= flags & WS_OP_MASK;
  if (flags & WS_HAS_MASK) {
    frame[1] = (char)(1 << 7);
  }
  if (data_len < 126) {
    frame[1] |= data_len;
    body_offset = 2;
  } else if (data_len <= 0xFFFF) {
    frame[1] |= 126;
    frame[2] = (char)(data_len >> 8);
    frame[3] = (char)(data_len & 0xFF);
    body_offset = 4;
  } else {
    frame[1] |= 127;
    frame[2] = (char)(((uint64_t)data_len >> 56) & 0xFF);
    frame[3] = (char)(((uint64_t)data_len >> 48) & 0xFF);
    frame[4] = (char)(((uint64_t)data_len >> 40) & 0xFF);
    frame[5] = (char)(((uint64_t)data_len >> 32) & 0xFF);
    frame[6] = (char)((data_len >> 24) & 0xFF);
    frame[7] = (char)((data_len >> 16) & 0xFF);
    frame[8] = (char)((data_len >> 8) & 0xFF);
    frame[9] = (char)((data_len)&0xFF);
    body_offset = 10;
  }
  if (flags & WS_HAS_MASK) {
    if (mask != NULL) {
      memcpy(&frame[body_offset], mask, 4);
    }
    websocket_decode(&frame[body_offset + 4], data, data_len,
                     &frame[body_offset], 0);
    body_offset += 4;
  } else {
    memcpy(&frame[body_offset], data, data_len);
  }

  return body_offset + data_len;
}

static int ws_websocket_encode(lua_State *L) {
  size_t sz, len;
  char *frame;
  const char *str;
  char mask[4] = {0};

  const char *const stype[] = {"text", "binary", "close", "ping", "pong", NULL};
  ws_frame_type_t ntype[] = {WS_FRAME_TEXT, WS_FRAME_BINARY, WS_FRAME_CLOSE,
                             WS_FRAME_PING, WS_FRAME_PONG,   0};

  const char *input = luaL_optlstring(L, 1, NULL, &sz);
  ws_frame_type_t itype = ntype[luaL_checkoption(L, 2, "binary", stype)];

  if (!lua_isnone(L, 3)) {
    luaL_argcheck(L, (lua_isstring(L, 3) && lua_rawlen(L, 3)) || lua_isnumber(L, 3),
                  3, "number or 4 bytes string expected");
    if (lua_isnumber(L, 3)) {
      uint32_t n = lua_tointeger(L, 3);
      memcpy(mask, &n, 4);
    } else {
      str = lua_tolstring(L, 3, &len);
      memcpy(mask, str, len);
    }
    itype |= WS_HAS_MASK;
  }

  itype |= (lua_isnone(L, 3) ? WS_FINAL_FRAME : (lua_toboolean(L, 3) ? WS_FINAL_FRAME : 0));

  len = ws_calc_frame_size(itype, sz);
  frame = malloc(sizeof(char) * len);

  len = websocket_build_frame(frame, itype, mask, input, sz);
  lua_pushlstring(L, frame, len);
  free(frame);
  return 1;
}

static const luaL_Reg ws_funcs[] = {{"decode", ws_websocket_decode},
                                    {"encode", ws_websocket_encode},

                                    {NULL, NULL}};

#define WEBSOCKET_UUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WEBSOCKET_VERSION 13

LUALIB_API int luaopen_ws_codec(lua_State *L) {
  luaL_newlib(L, ws_funcs);

  lua_pushliteral(L, "UUID");
  lua_pushliteral(L, WEBSOCKET_UUID);
  lua_rawset(L, -3);

  lua_pushliteral(L, "VERSION");
  lua_pushinteger(L, WEBSOCKET_VERSION);
  lua_rawset(L, -3);

  lua_pushliteral(L, "text");
  lua_pushinteger(L, WS_FRAME_TEXT);
  lua_rawset(L, -3);

  lua_pushliteral(L, "binary");
  lua_pushinteger(L, WS_FRAME_BINARY);
  lua_rawset(L, -3);

  lua_pushliteral(L, "close");
  lua_pushinteger(L, WS_FRAME_CLOSE);
  lua_rawset(L, -3);

  lua_pushliteral(L, "ping");
  lua_pushinteger(L, WS_FRAME_PING);
  lua_rawset(L, -3);

  lua_pushliteral(L, "pong");
  lua_pushinteger(L, WS_FRAME_PONG);
  lua_rawset(L, -3);

  return 1;
}
