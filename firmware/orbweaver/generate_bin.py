# Orbweaver - generate_bin.py
# Copyright (C) 2026 Vespid
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import os
import subprocess
import shutil
import sys

build_config = sys.argv[1]

script_dir = os.path.dirname(os.path.abspath(__file__))

version_path = os.path.join(script_dir, "source\\version.h")

hex2bin_path = os.path.join(script_dir, ".\\hex2bin.exe")

hex_path = os.path.join(script_dir, build_config + "\\orbweaver.hex")

bin_path_default = os.path.join(script_dir, build_config + "\\orbweaver.bin")


with open(version_path, "r") as file:
    
    content = file.read()
    
    name = content.split("FIRMWARE_VERSION \"")[1].split("\"")[0]

bin_path_out = os.path.join(script_dir, build_config + "\\orbweaver_" + name + ".bin")
 
 
result = subprocess.run(
    [hex2bin_path, hex_path],  
    capture_output=True,       
    text=True,                 
    check=True                 
)

shutil.move(bin_path_default, bin_path_out)

print("Generated binary file: " + bin_path_out)