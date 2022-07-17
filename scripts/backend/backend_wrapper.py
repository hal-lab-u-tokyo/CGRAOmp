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
#   File:          /scripts/backend/backend_wrapper.py
#   Project:       CGRAOmp
#   Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
#   Created Date:  18-06-2022 21:03:38
#   Last Modified: 30-06-2022 13:38:38
###

try:
    from rich.console import Console
    from rich.layout import Layout
    from rich.panel import Panel
    from rich.table import Table
    from rich.live import Live
    from rich.status import Status
    rich_available = True
except ImportError:
    rich_available = False

from cProfile import run
from glob import escape
from time import sleep
from typing import List, Tuple, Dict, Set
from datetime import datetime
import subprocess
import os
import fcntl
import re

escape_finder = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
cursor_up_finder = re.compile(r'\x1B\[A')

from .decorder import decode

def backend_wrapper(files, cmd, panel_num, proc_num, nowait):
    jobs = []
    for f in files:
        jobs.append(BackendJob(f, cmd))
    if rich_available:
        runner = RichRunner(panel_num, jobs, proc_num, nowait)
        runner.run()
    else:
        pass


class Header:
    """Display header with clock."""

    def __rich__(self) -> Panel:
        grid = Table.grid(expand=True)
        grid.add_column(justify="center", ratio=1)
        grid.add_column(justify="right")
        grid.add_row(
            "Backend mapping processes are running...",
            datetime.now().ctime().replace(":", "[blink]:[/]"),
        )
        return Panel(grid, style="white on blue")

class BackendJob():

    def __init__(self, dotfile : str, cmd : List[str]):
        self.dotfile = dotfile
        self.buf = bytes()
        self.proc : subprocess.Popen = None
        self.finished = False
        self.started = False
        c = Console()
        self.col_size = c.width
        self.row_size = c.height
        self.returncode = 0
        self.cmd = cmd

    def jobName(self) -> str:
        name = self.dotfile
        return name

    def getPID(self) -> int:
        if self.is_started() and not self.is_finished():
            return self.proc.pid
        return None

    def getReturnCode(self) -> int:
        return self.returncode

    def run(self):
        """run command"""
        self.started = True
        env = dict(os.environ)
        env["COLUMNS"] = str(self.col_size - 4)
        env["LINES"] = str(self.row_size - 4)
        env["DOTFILE_NAME"] = self.dotfile
        # launch a process
        self.proc = subprocess.Popen(self.cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env = env)
        # making the stdout non-blocking mode
        flag = fcntl.fcntl(self.proc.stdout.fileno(), fcntl.F_GETFL)
        fcntl.fcntl(self.proc.stdout.fileno(), fcntl.F_SETFL, flag | os.O_NONBLOCK)

    def is_finished(self) -> bool:
        return self.finished
    
    def is_started(self) -> bool:
        return self.started

    def is_running(self) -> bool:
        return self.started and not self.finished

    def update(self):
        """Update stdout buffer and check the process status"""
        if self.proc is not None:
            if not self.proc.poll(): # only when running
                ret = self.__readAllSoFar()
                if not ret is None:
                    if len(ret) > 0:
                        self.buf += ret
                    else:
                        try:
                            # check if it is finished
                            (ret_stdout, _) = self.proc.communicate(timeout=0.1)
                            if ret_stdout is not None:
                                self.buf += ret_stdout
                            self.returncode = self.proc.returncode
                            self.finished = True
                            self.proc = None
                        except subprocess.TimeoutExpired:
                            pass
            else:
                # already finished
                (ret_stdout, _) = self.proc.communicate()
                if ret_stdout is not None:
                                self.buf += ret_stdout
                self.returncode = self.proc.returncode
                self.proc = None
                self.finished = True

    def kill(self):
        if self.proc is not None:
            self.proc.kill()

    def readlines(self, max_lines : int = None, width : int = None) -> List[str]:
        """read lines from subprocess stdout"""
        if width is not None:
            lines = []
            for l in decode(self.buf):
                if len(l) > width:
                    lines.append(l[:width])
                    lines.append(l[width:])
                else:
                    lines.append(l)
        else:
            lines = decode(self.buf)

        if max_lines is None:
            max_lines = len(lines)
    
        return lines[-max_lines:]



    def __readAllSoFar(self, rdata=bytes()) -> bytes:
        """read stdout of the subprocess as much as possible"""
        try:
            rdata = self.proc.stdout.read()
        except IOError as e:
            pass
        return rdata

            
    # def __decode(self) -> List[str]:
    #     """decode buffered bytes to string"""
    #     lines = []
    #     prev = 0
    #     for i in range(len(self.buf)):
    #         c = self.buf[i]

    #         if c == b'\n'[0]:
    #             lines.append(self.buf[prev:i].decode())
    #             prev = i + 1
    #         elif c == b'\r'[0]:
    #             prev = i + 1

    #     if prev < len(self.buf):
    #         lines.append(self.buf[prev:].decode())

    #     escape_lines = [escape_finder.sub('', l) for l in lines]

    #     return escape_lines


class StatusPanel():
    def __init__(self, jobs : List[BackendJob]):
        self.jobs = jobs
        self.job_num = len(jobs)
        self.spinner = [Status("", spinner="line") for _ in jobs]
        self.count = 0

    def __make_table(self):
        grid = Table.grid(expand=True)
        grid.add_column() # icon
        grid.add_column(justify="left") # pid
        grid.add_column(justify="left") # name
        grid.add_column(justify="center") # status
        grid.add_row("", "PID", "Job", "Status") # header

        for i in range(self.job_num):
            pid = ""
            j = self.jobs[i]
            if j.is_started():
                if j.is_finished():
                    if j.getReturnCode() == 0:
                        result = "[bold green]Success" 
                        icon = ":heavy_check_mark:"
                    else:
                        result = "[bold red blink]Fail"
                        icon = ":warning:"
                else:
                    icon = self.spinner[i] 
                    pid = str(j.getPID())
                    result = "running"
            else:
                icon = ""
                result = "not started"
            grid.add_row(icon, pid, j.jobName(), result)

        return grid

    def update(self):
        return Panel(self.__make_table(), title = "Job status", border_style="cyan")

class JobScreen:
    def __init__(self, root : Layout, layout : Layout):
        self.root = root
        self.layout : Layout = layout

    def __make_panel(self, title : str, lines : List[str]) -> Panel:
        return Panel("\n".join(lines), title=title, border_style="cyan")

    def __rich__(self) -> Panel:
        return self.__make_panel()

    def get_screen_size(self):
        try:
            (region, _) = self.root.map[self.layout]
        except KeyError:
            c = Console()
            return (c.width, c.height)
        return (region.width - 4, region.height - 2)

    def update(self, attach_job : BackendJob):
        """redraw the screen"""
        w, h = self.get_screen_size()
        lines = attach_job.readlines(h, w)
        return self.__make_panel(attach_job.jobName(), lines)

class RichRunner():
    def __init__(self, screen_num, jobs : List[BackendJob], num_proc, nowait = False):
        self.num_proc = min(num_proc, len(jobs))
        self.screen_num = min(screen_num, self.num_proc)
        self.jobs = jobs
        self.job_queue = [j for j in self.jobs]
        self.status = StatusPanel(jobs)
        self.layout = self.make_layout()
        self.job_screens = [JobScreen(self.layout, self.layout[i]) \
                                for i in range(screen_num)]
        self.nowait = nowait

    def make_layout(self) -> Layout:
        """Make sreen layout"""
        layout = Layout(name = "root")

        layout.split(
            Layout(name = "header", size = 3),
            Layout(name = "main", ratio = 1),
            Layout(name="status", size = len(self.jobs) + 3),
        )
        
        layout["main"].split_column(
            *[Layout(name=i) for i in range(self.screen_num)]
        )

        layout["header"].update(Header())
        
        layout["status"].update(self.status.update())

        return layout 
        
    def run(self):
        start = 0
        running : List[BackendJob] = []
        with Live(self.layout, refresh_per_second=10, screen=True):
            count = 100000
            while count > 0:
                # job queue control
                dispatch_count = min(self.num_proc - len(running),  len(self.job_queue))
                for i in range(dispatch_count):
                    self.job_queue[i].run()
                    running.append(self.job_queue[i])
                self.job_queue = self.job_queue[dispatch_count:]

                # status update
                for job in running:
                    job.update()


                if self.num_proc > self.screen_num:
                    view_jobs = running[start:start+self.screen_num]
                    if (len(view_jobs) < self.screen_num):
                        view_jobs += running[:start]
                    if (count % 10) == 0:
                        start = (start + self.screen_num) % self.num_proc
                else:
                    view_jobs = running


                for i in range(len(view_jobs)):
                    self.layout[i].update(self.job_screens[i].update(
                        view_jobs[i]
                    ))
                for i in range(len(view_jobs), self.screen_num):
                    self.layout[i].update(Panel("", border_style="cyan"))


                self.layout["status"].update(self.status.update())
                
                if sum([j.is_finished() for j in self.jobs]) == len(self.jobs):
                    break

                running = [job for job in running if job.is_running()]
                
                count -= 1
                sleep(0.1)

            if not self.nowait:
                input()

class PlainRunner():
    pass

