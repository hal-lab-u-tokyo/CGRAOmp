#!/usr/bin/env python3
# -*- coding:utf-8 -*-

###
#   MIT License
#   
#   Copyright (c) 2022 Amano laboratory, Keio University & Processor Research Team, RIKEN Center for Computational Science
#   
#   Permission is hereby granted, free of charge, to any person obtaining a copy of
#   this software and associated documentation files (the "Software"), to deal in
#   the Software without restriction, including without limitation the rights to
#   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
#   of the Software, and to permit persons to whom the Software is furnished to do
#   so, subject to the following conditions:
#   
#   The above copyright notice and this permission notice shall be included in all
#   copies or substantial portions of the Software.
#   
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#   SOFTWARE.
#   
#   File:          /scripts/backend/decorder.py
#   Project:       CGRAOmp
#   Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
#   Created Date:  30-06-2022 11:39:16
#   Last Modified: 30-06-2022 13:35:51
###

from enum import IntEnum, auto
from typing import List

LARGE_NUM = 1000000000

# ascii special char
BEL = 0x07
BS = 0x08
HT = 0x09
LF = 0x0A
VT = 0x0B
FF = 0x0C
CR = 0x0D
ESC = 0x1B
DEL = 0x7F
SQBRA = 0x5B # [
NULL = 0

SPECIAL_CHARS = [BEL, BS, HT, LF, VT, FF, CR, ESC, DEL]
DECIMAL_CHARS = [i for i in range(48, 58)]

class DecodeStat(IntEnum):
    NORMAL = auto()
    ESC_BEGIN = auto()
    CTRL_START = auto()
    END = auto()

def set_char(chr : int, buf : List[List[int]], col, row):
    # print("set ({0}, {1}) {2}".format(col, row, bytes([chr]).decode()))
    if row >= len(buf):
        for _ in range(row + 1 - len(buf)):
            buf.append([NULL])
    if col >= len(buf[row]):
        for _ in range(col + 1 - len(buf[row])):
            buf[row].append(NULL)
    buf[row][col] = chr

def decode(data : bytes) -> List[str]:
    """decord bytes for virtual terminal output"""
    terminal = [[NULL]]

    if (len(data) == 0):
        return []

    stat = DecodeStat.NORMAL
    row, col = 0, 0
    i = 0
    num = 0
    while stat != DecodeStat.END:
        chr = data[i]
        if stat == DecodeStat.NORMAL:
            if chr in SPECIAL_CHARS:
                if chr == CR:
                    col = 0
                elif chr == LF:
                    row += 1
                    col = 0
                elif chr == ESC:
                    stat = DecodeStat.ESC_BEGIN
            else:
                set_char(chr, terminal, col, row)
                col += 1
        elif stat == DecodeStat.ESC_BEGIN:
            if chr == SQBRA:
                stat = DecodeStat.CTRL_START
                num = 0
            else:
                stat = DecodeStat.NORMAL
        elif stat == DecodeStat.CTRL_START:
            cursor_dir = (0, 0)

            if chr in DECIMAL_CHARS:
                num = num * 10 + chr - DECIMAL_CHARS[0]
            elif chr == ord('A'):
                # cursor up
                cursor_dir = (-1, 0)
            elif chr == ord('B'):
                # cursor down
                cursor_dir = (1, 0)
            elif chr == ord('C'):
                # cursor rifht
                cursor_dir = (0, 1)
            elif chr == ord('D'):
                # cursor left
                cursor_dir = (0, -1)
            elif chr == ord('E'): 
                # beginning of next line
                cursor_dir = (1, -LARGE_NUM)
            elif chr == ord('F'):
                # beginning of prev line
                cursor_dir = (-1, -LARGE_NUM)
            elif chr == ord('G'):
                # change column
                col = num
            elif chr == ord('n') and num == 6:
                # request
                pass         
            else:
                # not supported sequence so discarded
                stat = DecodeStat.NORMAL
            
            if cursor_dir != (0, 0):
                if num == 0:
                    num = 1
                row += num * cursor_dir[0]
                row = max(row, 0)
                col += num * cursor_dir[1]
                col = max(col, 0)
                stat = DecodeStat.NORMAL

        i += 1
        if i == len(data):
            stat = DecodeStat.END

    return [ bytes(l).decode() for l in terminal]

if __name__ == "__main__":
    with open("/home/members/tkojima/CGRAOmp_build/dump", "rb") as f:
        data = f.read()
    
    for l in decode(data):
        print(l)