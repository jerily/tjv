# tjv

A TCL/C extension that provides validation of Tcl dict and JSON objects to ensure that the data conforms to specified schema.

This extension can be useful for validating data coming from users, including JSON format. For example, a request from a web server where the request parameters are specified as Tcl dict and one of the keys is the body of the request in JSON format.

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

There are 2 commands defined for data validation:

* **::tjv::compile validation_schema ?variable_name?** - compiles a validation schema for later use and returns its handle
* **::tjv::validate validation_schema value ?outcome_variable?** - performs a validation, `validation_schema` can be either a descriptor returned by **::tjv::compile** or a schema description

### Define validation schema

The most important part is defining the validation scheme.

A validation schema definition is a list of parameters. The general format is as follows:

**-type type_name ?-required? ?-nullable? ?-outkey keys? ?type_specific_options?**

* **-type type_name** - this mandatory parameter specifies desired value type. The following type names are known:
  * **json** - specifies a JSON value to be parsed and optionally validated
  * **object** - specifies Tcl dict or JSON object
  * **array** - specifies Tcl list or JSON array
  * **list** - alias of **array**
  * **integer** - specified an integer value
  * **float** - specifies a floating point number
  * **double** - alias of **float**
  * **boolean** - specifies a boolean value
  * **string** - specifies a plain string
  * **email** - specifies a string representing a valid e-mail address
  * **duration** - specifies a string representing a duration ([RFC3339](https://tools.ietf.org/html/rfc3339#appendix-A))
  * **uri** - specifies a string representing a valid URI ([RFC3986](https://datatracker.ietf.org/doc/html/rfc3986))
  * **uri-template** - specifies a string representing a valid URI template ([RFC6570][https://datatracker.ietf.org/doc/html/rfc6570])
  * **url** - specifies a string representing a valid URL ([RFC1738](https://datatracker.ietf.org/doc/html/rfc1738))
  * **hostname** - specifies a string representing a valid hostname
  * **ipv4** - specifies a string representing a valid IPv4 address
  * **ipv6** - specifies a string representing a valid IPv6 address
  * **uuid** - specifies a string representing a valid UUID ([RFC9562](https://datatracker.ietf.org/doc/html/rfc9562))
  * **json-pointer** - specifies a string representing a valid absolute JSON pointer ([RFC6901](https://datatracker.ietf.org/doc/html/rfc6901))
  * **json-pointer-uri-fragment** - specifies a string representing a valid absolute JSON pointer in a URI fragment identifier ([RFC6901#section-6](https://datatracker.ietf.org/doc/html/rfc6901#section-6))
  * **relative-json-pointer** specifies a string representing a valid relative JSON pointer ([draft-handrews-relative-json-pointer-01](https://datatracker.ietf.org/doc/html/draft-handrews-relative-json-pointer-01))
* **-required** - (optional flag) if it is specified, validation will fail if this key is missing.
* **-nullable** - (optional flag) if it is specified, then a null value is allowed for this key. Only works for JSON.
* **-outkey keys** - (optional flag) if it is specified, then the value for this key will be stored in output dictionary.

These parameters are allowed only for the `string` type:

* **-pattern pattern** - (optional) specifies a pattern to which the string value to be validated should match
* **-match regexp|glob|list** - (optional) specifies a method to match the string and the specified pattern
  * **regexp** - the specified pattern is a regexp
  * **glob** - the specified pattern is a glob
  * **list** - the specified pattern is a list of plain strings. The string must match at least one string from the list specified by option `-pattern`

These parameters are allowed only for the `integer` and `float` (`double`) types:

* **-minimum value** - (optional) minimum value
* **-maximum value** - (optional) maximum value

This parameter is allowed only for the `json` and `object` types:

* **-properties list** - (optional) specifies a list of keys and their format in Tcl dict or JSON object

This parameter is allowed only for the `json` and `array` (`list`) types:

* **-items validation_schema** - (optional) specifies a format for array (list) elements

For example:

```tcl
package require tjv

::tjv::compile -type object -properties {
    { foo -type integer }
    { bar -type integer -minimum 0 -maximum 255 }
    { users -type array -required -items {
        -type object -properties {
            { name -type string }
            { email -type email }
        }
    }}
    { data -type json -required -properties {
        { state -type string -match list -pattern {enabled disabled} }
        { id -type uuid }
    }}
}
```

This example is a scheme where a Tcl dict with 4 keys is expected:

1. `foo` is expected to be an integer.
2. `bar` is expected to be an integer in the range from 0 to 255.
3. `users` is a mandatory key and is expected to be a list of Tcl dicts. Each dict is expected to be a Tcl dict with 2 keys: `name` and `email`.
4. `data` is a mandatory key and is expected to be JSON. This JSON should represent an object with 2 keys: `state` and `id`.

### Compile validation schema

For maximum performance, it is recommended to compile the validation scheme into an internal format and then use the resulting handle for validation.

This can be done using the `::tjv::compile` command, which has the following format:

* **::tjv::compile validation_schema ?variable_name?**

The result of this procedure will be a handle that can be used for data validation.

If a `variable_name` variable is specified, then the handle will also be assigned to the specified variable in the local scope. If the variable is deleted, the handle will be destroyed. This can be used within a procedure and the local variable specified. On exiting the procedure, the local variable will be deleted, and the handle will be deleted along with it.

The returned handle has commands in the following format:

* **handle validate value ?output_variable?**

Validates the value of `value`.

If the `output_variable` variable is specified, then the result of executing the command will be `1` if the validation succeeds and `0` if it fails. The result of the validation will be written to the variable specified in `output_variable`.

If the `output_variable` is not specified, then the command will finish successfully or with an error, and a test result or error message will be returned.

* **handle destroy**

Destroys the compiled validation scheme handle and frees memory.

For example:

```tcl
package require tjv

set data [dict create "key1" "foo" "key2" "bar"]

set handle [::tjv::compile -type object -properties {
    { key1 -type integer }
    { keyX -type string -required }
}]

if { [$handle validate $data outcome] } {
    puts "Verification was successful"
} else {
    puts "Validation error: [dict get $outcome error message]"
}

$handle destroy
```

The result is:
```
Validation error: Error while validating data: .key1 should be integer, should have required property 'keyX'
```

### Run validation

The recommended workflow for validation, is to compile the schema using **::tjv::compile** and validate the data using the returned descriptor as described above.

But for simplicity and when there are no performance requirements, it is possible to use the command **::tjv::validate** which has the following format:

**::tjv::validate validation_schema value ?outcome_variable?**

Validates the value of `value` using `validation_schema`.

`validation_schema` should be specified in the same format as for **::tjv::compile**. It can be also a handle returned by the command **::tjv::compile**.

If the `output_variable` variable is specified, then the result of executing the command will be `1` if the validation succeeds and `0` if it fails. The result of the validation will be written to the variable specified in `output_variable`.

If the `output_variable` is not specified, then the command will finish successfully or with an error, and a test result or error message will be returned.

These 3 examples lead to the same result:

```tcl
package require tjv

set data [dict create "key1" "foo" "key2" "bar"]

if { [::tjv::validate -type object -properties {
    { key1 -type integer }
    { keyX -type string -required }
} $data outcome] } {
    puts "Verification was successful"
} else {
    puts "Validation error: [dict get $outcome error message]"
}
```

or

```tcl
package require tjv

set data [dict create "key1" "foo" "key2" "bar"]

set schema {-type object -properties {
    { key1 -type integer }
    { keyX -type string -required }
}}

if { [::tjv::validate {*}$schema $data outcome] } {
    puts "Verification was successful"
} else {
    puts "Validation error: [dict get $outcome error message]"
}
```

or

```tcl
package require tjv

set data [dict create "key1" "foo" "key2" "bar"]

set handle [::tjv::compile -type object -properties {
    { key1 -type integer }
    { keyX -type string -required }
}]

if { [::tjv::validate $handle $data outcome] } {
    puts "Verification was successful"
} else {
    puts "Validation error: [dict get $outcome error message]"
}

$handle destroy
```

The result is:
```
Validation error: Error while validating data: .key1 should be integer, should have required property 'keyX'
```

### Validation results

TBD

## Copyrights

Copyright (c) 2024 Neofytos Dimitriou (neo@jerily.cy)

## License

`tjv` sources are available under the MIT license.

