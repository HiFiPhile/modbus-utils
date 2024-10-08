/*
*  MIT License

*  Copyright (c) 2013  Krzysztow

*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:

*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#ifndef MBU_COMMON_H
#define MBU_COMMON_H

#include <string.h>
#include <stdio.h>
#include <ctype.h>

int getInt(const char str[], int *ok) {
    int value;
    int ret = sscanf(str, "0x%x", &value);
    if (0 >= ret) {//couldn't convert from hex, try dec
        ret = sscanf(str, "%d", &value);
    }

    if (0 != ok) {
        *ok = (0 < ret);
    }

    return value;
}

#endif //MBU_COMMON_H
