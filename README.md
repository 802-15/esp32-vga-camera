ESP-IDF template app
====================

This is a template application to be used with [Espressif IoT Development Framework](https://github.com/espressif/esp-idf).

Please check [ESP-IDF docs](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for getting started instructions.

Build using esp-idf's own tool:
After cloning esp-idf repository source the 'export.sh' inside it using:

$. ./export.sh

After that navigate to project directory and try executing:

$idf.py

You should now be able to use the following commands for building and flashing:

$idf.py build
$idf.py flash

To monitor ESP 32 UART output you can use '$idf.py monitor' or minicom/putty

*Code in this repository is in the Public Domain (or CC0 licensed, at your option.)
Unless required by applicable law or agreed to in writing, this
software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.*


