#! /usr/bin/python

import sys

while True:
	line = sys.stdin.readline()
	if not line:
		break

	line = line.rstrip('\n');

	# ME/THEM server channel nick :text
	dest = line.partition(' ')

	parts = line.split(':')

	if dest[0] == 'ME':
		print 'REPLACE %s' % (parts[1].replace('f-irc', 'f-irc (http://vanheusden.com/f-irc/ )'))

	else:
		print 'REPLACE %s' % (parts[1].replace('f-irc', '\002f-irc\002'))
