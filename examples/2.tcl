package require tjv

::tjv::compile -type object -properties {
    { foo -type integer }
    { bar -type integer -minimum 0 -maximum 255 }
    { users -type array -required -outkey users -items {
        -type object -properties {
            { name -type string -outkey name }
            { email -type email -outkey email }
        }
    }}
    { data -type json -required -properties {
        { state -type string -match list -pattern {enabled disabled} -outkey state}
        { id -type uuid -outkey id }
    }}
} vvv

set mydict {
   foo 10
   bar 100
   users {
       {{ name "John Doe"} { email "john@example.com"}}
       {{ name "Jane Doe"} { email "jane@example.com"}}
   }
   data {{
       "state": "enabled",
       "id": "123e4567-e89b-12d3-a456-426614174000"
   }}
}

set valid [$vvv validate $mydict outcome]
puts "valid=$valid outcome: $outcome"