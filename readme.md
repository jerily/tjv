# tjv

TCL/C extension that provides validation for Tcl dict and JSON objects.

## Requirements

- [cJSON](https://github.com/DaveGamble/cJSON)

Supported systems:

- Linux
- MacOS

## Installation

### Install Dependencies

```bash
git clone https://github.com/DaveGamble/cJSON.git
cd cJSON
mkdir build
cd build
cmake ..
make
make install
```

### Install tjv

```bash
git clone https://github.com/jerily/tjv.git
cd tjv
mkdir build
cd build
cmake ..
# or if TCL is not in the default path (/usr/local/lib):
# change "TCL_LIBRARY_DIR" and "TCL_INCLUDE_DIR" to the correct paths
# cmake .. -DTCL_LIBRARY_DIR=/usr/local/lib -DTCL_INCLUDE_DIR=/usr/local/include
make
make install
```

## Usage

