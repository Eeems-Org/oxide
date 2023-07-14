#pragma once
static ssize_t(*func_write)(int, const void*, size_t);
static int(*func_open)(const char*, int, mode_t);
static int(*func_ioctl)(int, unsigned long, ...);
