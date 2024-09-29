# tjv creates some Tcl_Objs during the first use and store them in threaded
# storage for later use. This means that some objects may be created during
# test execution and not released after test execution is complete.
# They will be released during the completion of the process thread.
#
# These objects will be detected by the memory leak hunter and reported
# as a memory leak.
#
# These false positives should be avoided. To do this, we will run
# the following code to create such objects in threaded storage.
#
# We need to pre-create objects for error messages and regexp objects for
# custom validation types.

# Create objects for error messages:
catch { ::tjv::validate -type integer a }

# Create regexp objects for custom types:
foreach type [list \
    email duration uri uri-template url hostname ipv4 ipv6 uuid \
    json-pointer json-pointer-uri-fragment relative-json-pointer \
] {
    catch [list ::tjv::validate -type $type foo]
}
unset type

# helper proc
proc test_custom_format { frm data } {
    set i 0
    set previous_runs [list]
    foreach line [split $data \n] {
        set line [string trimleft $line]
        if { $line eq "" || [string index $line 0] eq "#" } continue
        set case [string index $line 0]
        # if MEMDEBUG is specified, run 1 success case and 1 failure case
        if { [info exists ::env(MEMDEBUG)] } {
            if { [lsearch -exact $previous_runs $case] != -1 } continue
            lappend previous_runs $case
        }
        set string [string range $line 2 end]
        set cmd [list \
            ::test tjvValidateFormat-${frm}-[incr i] "Test $frm format: $string" \
            -body [list ::tjv::validate -type $frm $string] \
        ]
        if { $case eq "+" } {
            lappend cmd -result {}
        } else {
            lappend cmd -returnCodes error -result "Error while validating data: should be $frm"
        }
        uplevel #0 $cmd
    }
}