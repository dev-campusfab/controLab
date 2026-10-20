#include "state_machine.h"

namespace core {

Context ctx;

static void (*onChange)() = nullptr;

void onStateChangeCallback(void (*cb)()) {
    onChange = cb;
}

const char* stateToString(State s) {
    switch (s) {
        case State::BOOT: return "BOOT";
        case State::SETUP: return "SETUP";
        case State::CONNECTING: return "CONNECTING";
        case State::BINDING: return "BINDING";
        case State::RUNNING: return "RUNNING";
        case State::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

const char* errorToString(ErrorCode e) {
    switch (e) {
        case ErrorCode::NONE: return "null";
        case ErrorCode::WIFI_AUTH_FAILED: return "WIFI_AUTH_FAILED";
        case ErrorCode::WIFI_TIMEOUT: return "WIFI_TIMEOUT";
        case ErrorCode::BIND_TOKEN_INVALID: return "BIND_TOKEN_INVALID";
        case ErrorCode::BIND_TOKEN_EXPIRED: return "BIND_TOKEN_EXPIRED";
        case ErrorCode::SERVER_UNREACHABLE: return "SERVER_UNREACHABLE";
    }
    return "null";
}

void setState(State s) {
    if (ctx.state == s) return;
    ctx.state = s;
    if (s != State::ERROR) ctx.error = ErrorCode::NONE;
    if (onChange) onChange();
}

void setError(ErrorCode e) {
    ctx.error = e;
    ctx.state = State::ERROR;
    if (onChange) onChange();
}

} // namespace core
