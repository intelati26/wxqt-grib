#!/usr/bin/env python3
"""Writes src/gfs/ServersBuiltIn.cpp from servers/servers.json (the program's built-in server table is the same text as the file the project publishes): python3 servers/make_builtin.py"""
import os
here = os.path.dirname(os.path.abspath(__file__))
text = open(os.path.join(here, 'servers.json')).read()
if not text.endswith('\n'):
    text += '\n'
cpp = '''// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The built-in server table: the text of servers/servers.json of the repository (a test compares the two). Edit the file and run python3 servers/make_builtin.py.
#include "gfs/Servers.h"

const std::string& Servers::builtInJson() {
    static const std::string text = R"JSON(''' + text + ''')JSON";
    return text;
}
'''
open(os.path.join(here, '..', 'src', 'gfs', 'ServersBuiltIn.cpp'), 'w').write(cpp)
print('written')
