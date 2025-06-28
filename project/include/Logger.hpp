#ifndef __LOGGER_HPP__
#define __LOGGER_HPP__
#include <stdio.h>

#define LOG_INFO(args...) { fprintf(stdout, args); fprintf(stdout, "\n"); }
#define LOG_ERR(args...) { fprintf(stderr, args); fprintf(stderr, "\n"); }
#ifdef DBG
#define LOG_DBG(args...) { fprintf(stderr, args); fprintf(stderr, "\n"); }
#else
#define LOG_DBG(args...) { }
#endif

#endif
