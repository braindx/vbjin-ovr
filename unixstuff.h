#ifndef _UNIXSTUFF_H_
#define _UNIXSTUFF_H_

#include <io.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <math.h>
#define dup _dup
#define lseek _lseek
#define stat _stat
#define strtoull _strtoui64
#define strtoll _strtoi64
#define S_ISREG(x) 0

inline double round(double x)
{
        double t;

		//alas, this isnt supported
        //if (!isfinite(x))
          //      return (x);

        if (x >= 0.0) {
                t = floor(x);
                if (t - x <= -0.5)
                        t += 1.0;
                return (t);
        } else {
                t = floor(-x);
                if (t + x <= -0.5)
                        t += 1.0;
                return (-t);
        }
}

#endif
