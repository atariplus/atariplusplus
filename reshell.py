#!/usr/bin/env python3
# ReShell: shell/console wrapper to internally implement command recall
# reshell.py /bin/bash
# reshell.py atari++ -FrontEnd None -InstallEDevice True

import sys
import tty
import termios
import os
import re

import select
import subprocess

import signal
import atexit

import fcntl

class raw_nonblocking(object):
    def __init__(self, stream):
        self.stream = stream
        self.fd = self.stream.fileno()
    def __enter__(self):
        self.original_stty = termios.tcgetattr(self.stream)
        tty.setcbreak(self.stream)
        self.orig_fl = fcntl.fcntl(self.fd, fcntl.F_GETFL)
        fcntl.fcntl(self.fd, fcntl.F_SETFL, self.orig_fl | os.O_NONBLOCK)
    def __exit__(self, type, value, traceback):
        fcntl.fcntl(self.fd, fcntl.F_SETFL, self.orig_fl)
        termios.tcsetattr(self.stream, termios.TCSANOW, self.original_stty)

def get_screen_code(n):
	f = sys.stdin

	buf = ''
	with raw_nonblocking(f):
		sys.stdout.write(f"\033]{n};?\007")
		sys.stdout.flush()
		poller = select.poll()
		poller.register(sys.stdin.fileno(), select.POLLIN)
		poller.poll(500)
		while(None is not (c := f.read(64)) and c != ''):
			buf += c
	return buf

#old_settings = termios.tcgetattr(sys.stdin)
save_screen = get_screen_code(10) + get_screen_code(11) + get_screen_code(12)

def handle_exit(*args):
	sys.stdout.write(save_screen)
	sys.stdout.write('\033[0m')
#	sys.stdout.write('\033[H\033[2J')
	sys.stdout.write('\033[?1049l\033[?1l')
	sys.stdout.flush()

atexit.register(handle_exit)

from enum import IntEnum
class keys(IntEnum):
	ESC = 0o33
	ENTER = 0o12
	BACKSPACE = 0o177

	DOWN = 0o402
	UP = 0o403
	LEFT = 0o404
	RIGHT = 0o405
	HOME = 0o406
	DC = 0o512
	IC = 0o513
	END = 0o550

lines = []
line = ''
linei = 0
linen = 0
insert = True
prompt = ''

alt_screen = False

process = subprocess.Popen(sys.argv[1:], stdout=subprocess.PIPE, stdin=subprocess.PIPE)

os.set_blocking(process.stdout.fileno(), False)

#sys.stdout.write(u"\u001b[11;#1a4bb6\a")
#sys.stdout.write(u"\u001b[48;5;32m ")

# atari
#sys.stdout.write(u"\u001b]11;#1c71c6\007") # atari
#sys.stdout.write(u"\u001b]12;#70c7ff\007")

sys.stdout.write("\033]11;#1b61b6\007")
sys.stdout.write("\033]10;#80d7ff\007")
sys.stdout.write("\033]12;#90e7ff\007")
sys.stdout.write("\033]12;#80d7ff\007")

#sys.stdout.write('\033[48;5;25m')
#sys.stdout.write('\033[38;5;117m')
#sys.stdout.write('\033[H\033[2J')

sys.stdout.write('\033[?1049h\033[?1h')

sys.stdout.flush()
#sys.stdout.write(u"\u001b]708;#000000\007")

def print_line():
	sys.stdout.write(u"\u001b[1000D")
	sys.stdout.write(u"\u001b[0K")
	sys.stdout.write(prompt)
	sys.stdout.write(line)
	sys.stdout.write(u"\u001b[1000D")
	if(linei > 0 or prompt != ""):
		sys.stdout.write(u"\u001b[" + str(linei + len(prompt)) + "C")
	sys.stdout.flush()

inputs = [sys.stdin, process.stdout]
outputs = []

def process_stdin():
	global lines, line, linei, linen, prompt, alt_screen

	keyb = sys.stdin.buffer.read(1)
	key = keyb[0]

	if key == keys.ESC:
		next1, next2 = ord(sys.stdin.read(1)), ord(sys.stdin.read(1))
		if next1 == 91 or next1 == 79:
			if next2 == 68: # Left
				key = keys.LEFT
			elif next2 == 67: # Right
				key = keys.RIGHT
			elif next2 == 66:
				key = keys.DOWN
			elif next2 == 65:
				key = keys.UP

	if key == keys.UP:
		if(linen):
			linen = linen - 1;
			line = lines[linen];
			linei = len(line)
			print_line();
	elif key == keys.DOWN:
		if(linen + 1  == len(lines)):
			line = ''
			linen = len(lines)
			linei = 0
			print_line();
		elif(linen + 1 < len(lines)):
			linen = linen + 1
			line = lines[linen]
			linei = len(line)
			print_line();
	elif key == keys.LEFT:
		if(linei):
			linei = linei - 1
			sys.stdout.write(u"\u001b[D")
			sys.stdout.flush()
	elif key == keys.RIGHT:
		if(linei < len(line)):
			linei = linei + 1
			sys.stdout.write(u"\u001b[C")
			sys.stdout.flush()
	elif key == keys.BACKSPACE:
		if(not insert):
			sys.stdout('\b')
			sys.stdout.flush()
		elif(linei):
			linei = linei - 1
			line = line[:linei] + line[linei+1:]
			print_line()
	elif key == keys.ENTER:
		process.stdin.write(bytes(line.swapcase(), 'utf8'));
		process.stdin.write(b"\n");
		process.stdin.flush()
		print()
		if(linen == len(lines)):
			lines.append(line)
			line = ''
		else:
			lines[linen] = line
		linen = linen + 1

		if linen < len(lines):
			line = lines[linen]

		linei = 0
#		print_line()
	else:
		if(len(line) == linei or not insert):
			line = line[:linei] + chr(key) + line[linei+1:]
			sys.stdout.write(chr(key))
			sys.stdout.flush()
			linei = linei + 1
		else:
			line = line[:linei] + chr(key) + line[linei:]
			linei = linei + 1
			print_line()

with raw_nonblocking(sys.stdin):
	while(inputs):
		readable, writable, exceptional = select.select(
			inputs, outputs, inputs)
		for s in readable:
			if(s == sys.stdin):
				if alt_screen:
					process.stdin.write(sys.stdin.buffer.read())
					process.stdin.flush()
					continue
				process_stdin()
			if(s == process.stdout):
				if False and alt_screen:
					sys.stdout.buffer.write(process.stdout.read())
					sys.stdout.buffer.flush()
					continue
				input = process.stdout.read().decode('utf8')
				n = input.rfind("\f")
				if(n >= 0):
					lines = []
				n = input.rfind("\n")
				prompt = input[n+1:] if n >=0 else prompt + input
				if 0 <= (n := input.find('\x1b[?1049')) and n < (len(input) - 7):
					alt_screen = True if input[7] == 'h' else False

				print(input, end="")
				sys.stdout.flush()
				line = '';
				linen = len(lines)

