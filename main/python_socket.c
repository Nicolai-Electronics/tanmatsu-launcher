#include "python_socket.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pocketpy/pocketpy.h"
#include "sys/socket.h"

static bool example_func(int argc, py_Ref argv) {
    PY_CHECK_ARGC(1);
    py_f64 secs;
    if (!py_castfloat(argv, &secs)) return false;

    printf("EXAMPLE FUNC\r\n");

    py_newnone(py_retval());

    return true;
}

static bool getaddrinfo(int argc, py_Ref argv) {
    PY_CHECK_ARGC(2);
    PY_CHECK_ARG_TYPE(0, tp_str);  // Hostname
    PY_CHECK_ARG_TYPE(1, tp_int);  // Port

    const char* hostname = py_tostr(py_arg(0));
    int         port     = py_toint(py_arg(1));

    // struct addrinfo  hints        = {.ai_socktype = SOCK_STREAM};
    struct addrinfo* address_info = NULL;

    /*int res = getaddrinfo(hostname, port, &hints, &address_info);
    if (res != 0 || address_info == NULL) {
        return OSError("Getaddrinfo failed %d", res);
    }*/

    py_newnone(py_retval());
    return true;
}

void pk__add_module_socket(void) {
    py_Ref mod = py_newmodule("socket");
    py_bindfunc(mod, "socket", example_func);
    py_bindfunc(mod, "getaddrinfo", getaddrinfo);
}

/*
    py_bindfunc(mod, "bind", example_func);
    py_bindfunc(mod, "listen", example_func);
    py_bindfunc(mod, "accept", example_func);
    py_bindfunc(mod, "connect", example_func);
    py_bindfunc(mod, "send", example_func);
    py_bindfunc(mod, "sendall", example_func);
    py_bindfunc(mod, "recv", example_func);
    py_bindfunc(mod, "sendto", example_func);
    py_bindfunc(mod, "recvfrom", example_func);
    py_bindfunc(mod, "setsockopt", example_func);
    py_bindfunc(mod, "makefile", example_func);
    py_bindfunc(mod, "settimeout", example_func);
    py_bindfunc(mod, "setblocking", example_func);
*/