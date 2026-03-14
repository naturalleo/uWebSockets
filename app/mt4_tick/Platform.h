
// MT4ManagerAPI.h 内部使用 WSADATA/WSAStartup，需先包含 winsock2.h
// winsock2.h 必须在 windows.h 之前，否则会有重定义冲突
#include <winsock2.h>
#include <windows.h>