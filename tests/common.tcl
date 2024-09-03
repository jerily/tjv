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
foreach type [list email duration uri] {
    catch [list ::tjv::validate -type $type foo]
}
unset type

# helper proc
proc test_custom_format { frm data } {
    set i 0
    foreach line [split $data \n] {
        set line [string trimleft $line]
        if { $line eq "" || [string index $line 0] eq "#" } continue
        set string [string range $line 2 end]
        set cmd [list \
            ::test tjvValidateFormat-${frm}-[incr i] "Test $frm format: $string" \
            -body [list ::tjv::validate -type $frm $string] \
        ]
        if { [string index $line 0] eq "+" } {
            lappend cmd -result {}
        } else {
            lappend cmd -returnCodes error -result "Error while validating data: should be $frm"
        }
        uplevel #0 $cmd
    }
}